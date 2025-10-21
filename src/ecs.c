#include <complex.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "khashl.h"
#include "kvec.h"

#include "ecs.h"



/// Always zero because it's the first component created
#define SIZE_COMPONENT_ID 0

/// Get a value from the given key
/// Performs no bounds checks
#define kh_val_unsafe(name, h, key) kh_val(h, name##_get(h, key))

/// Remove an element within a kvec
/// Preserves the numerical order (deletes, then shifts)
/// Performs no bounds checks on `index`
#define kv_rm_at(vec, index)                                                                                        \
    do {                                                                                                            \
        if((index) < kv_size(vec)) {                                                                                \
            memmove(&kv_A(vec, (index)), &kv_A(vec, (index) + 1), (kv_size(vec) - (index) - 1) * sizeof(*(vec).a)); \
            kv_size(vec)--;                                                                                         \
        }                                                                                                           \
    } while(0)



typedef struct {
    size_t size;
} __intern_comp_size;

struct archetype_edge {
    archetype* add;
    archetype* remove;
};

typedef kvec_t(column) vec_column;
typedef kvec_t(uint64_t) vec_uint64_t;
typedef vec_uint64_t vec_component_id;

struct archetype_t {
    archetype_id id;       // The hash of `type`
    vec_component_id type; // Vector of component ids contained in this archetype
    vec_uint64_t entities; // Vector of entity ids contained in this archetype, indicies correspond with row indicies
    vec_column components; // Vector of components for each component
};

struct column_t {
    void* elements;      // buffer with component data
    size_t element_size; // size of a single element
    size_t count;        // number of elements
    size_t allocated;    // number of allocated elements
};

struct record_t {
    archetype* archetype;
    size_t index; // ie. row
};

khint_t uint64_vec_hash(const vec_uint64_t vec) {
    return kh_hash_bytes(vec.n * sizeof(uint64_t), (uint8_t*) vec.a);
}
khint_t uint64_vec_compare(const vec_uint64_t a, const vec_uint64_t b) {
    return (a.n == b.n) ? (memcmp(a.a, b.a, a.n * sizeof(uint64_t)) == 0) : 0;
}
static inline int uint64_compare(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*) a;
    uint64_t ub = *(const uint64_t*) b;
    return (ua > ub) - (ua < ub); // Returns -1, 0, or 1
}

///
/// ECS instance and related type definitions
///
// Maps EntityId to a Record array
// Used to find the archetypes that the entity is a part of
KHASHL_MAP_INIT(KH_LOCAL, entity_map_t, entity_map, uint64_t, record, kh_hash_uint64, kh_eq_generic)
// Maps a vector of ComponentIds to an Archetype
// Used to find an archetype by its list of components
KHASHL_MAP_INIT(KH_LOCAL, archetype_map_t, archetype_map, vec_component_id, archetype, uint64_vec_hash, uint64_vec_compare)
// Maps ArchetypeId to a column (size_t)
// Used to find a component's column in a given archetype that it is a part of
// Used in component_index
KHASHL_MAP_INIT(KH_LOCAL, component_column_map_t, component_column_map, uint64_t, size_t, kh_hash_uint64, kh_eq_generic)
// Maps ComponentId to kh_component_columns_t
// Used to get the various archetypes that the given component is a part of
KHASHL_MAP_INIT(KH_LOCAL, component_map_t, component_map, uint64_t, component_column_map_t, kh_hash_uint64, kh_eq_generic)
// Maps component type names to ComponentIDs
// Used with user component actions
KHASHL_MAP_INIT(KH_LOCAL, component_name_map_t, component_name_map, const char*, uint64_t, kh_hash_str, kh_eq_str)

typedef component_column_map_t component_archetypes;

struct ecs_instance_t {
    // Global entity tracker, key is EntityId
    entity_map_t* entity_index;

    // Global archetype index, key is a vector of ComponentId
    archetype_map_t* archetype_index;

    // Global component index, key is ComponentId
    component_map_t* component_index;

    // Global component name map, key is component typenames (const char*)
    component_name_map_t* component_names;

    vec_uint64_t id_graveyard;

    uint32_t next_id;
};



///
/// Internal Function Implementations
///

#define archetype_destroy(archetype)                                \
    do {                                                            \
        kv_destroy((archetype).type);                               \
        kv_destroy((archetype).entities);                           \
        for(size_t i = 0; i < kv_size((archetype).components); i++) \
            free(kv_A((archetype).components, i).elements);         \
        kv_destroy((archetype).components);                         \
    } while(0)

/// Moves an entity at row `index` from `src` to `dest`
int move_entity(ecs_instance* instance, const entity_id entity, archetype* src, archetype* dest) {
    // `dest` is empty, so initialize the columns
    if(kv_size(dest->entities) == 0) {
        for(size_t i = 0; i < kv_size(dest->type); i++) {
            column* comp_col = &kv_A(dest->components, i);
            comp_col->count = 0;
            comp_col->allocated = 2;
            comp_col->elements = malloc(2 * comp_col->element_size);
        }
    }

    // TODO Remove this stupid if statement once the 'empty' archetype is implemented
    if(src) {
        const size_t min_size =
            (kv_size(dest->components) < kv_size(src->components)) ? kv_size(dest->components) : kv_size(src->components);
        for(size_t i = 0; i < min_size; i++) {
            // TODO Test this when types are in mismatched indicies (breaks i think)
            if(kv_A(src->type, i) == kv_A(dest->type, i)) {
                if(dest->components.a[i].count == dest->components.a[i].allocated) {
                    void* temp = realloc(
                        dest->components.a[i].elements, dest->components.a[i].allocated * 2 * dest->components.a[i].element_size
                    );
                    if(temp == NULL)
                        return 0;

                    dest->components.a[i].elements = temp;
                    dest->components.a[i].allocated *= 2;
                }

                memcpy(
                    (uint8_t*) dest->components.a[i].elements +
                        (dest->components.a[i].count++ * dest->components.a[i].element_size),
                    (uint8_t*) src->components.a[i].elements + (i * src->components.a[i].element_size),
                    src->components.a[i].element_size
                );
            }
        }
    } else {
        // 'src' doesn't exist, but we still need to ensure there're slots in `dest`
        for(size_t i = 0; i < kv_size(dest->components); i++) {
            if(kv_A(dest->components, i).count == kv_A(dest->components, i).allocated) {
                void* temp = realloc(
                    kv_A(dest->components, i).elements,
                    kv_A(dest->components, i).allocated * 2 * kv_A(dest->components, i).element_size
                );
                if(temp == NULL)
                    return 0;

                kv_A(dest->components, i).elements = temp;
                kv_A(dest->components, i).allocated *= 2;
            }
            kv_A(dest->components, i).count++;
        }
    }

    record* record = &kh_val_unsafe(entity_map, instance->entity_index, entity);
    if(src) { // TODO Remove this stupid if statement as well
        kv_rm_at(src->entities, record->index);

        if(kv_size(src->entities) == 0) {
            khint_t key = archetype_map_get(instance->archetype_index, src->type);
            archetype_destroy(kh_val(instance->archetype_index, key));
            archetype_map_del(instance->archetype_index, key);
        }
    }
    kv_push(entity_id, dest->entities, entity);
    record->archetype = dest;
    record->index = kv_size(dest->entities) - 1;

    return 1;
}
/// Create a new Archetype for `type` components
/// Assumes `type` is sorted
archetype* archetype_create(ecs_instance* instance, const vec_component_id* type) {
    vec_component_id type_cpy;
    kv_copy(component_id, type_cpy, *type);

    // Add new archetype to the global archetype index
    int ret;
    khint_t iter = archetype_map_put(instance->archetype_index, type_cpy, &ret);
    archetype* temp = &kh_val(instance->archetype_index, iter);

    temp->id = ecs_entity_create(instance);

    temp->type = type_cpy;
    kv_init(temp->entities);
    kv_init(temp->components);

    // Initialize component storage
    for(size_t i = 0; i < temp->type.n; i++) {
        // Bit of a hacky fix to allow registering the __intern_comp_size component (which doesn't have its size)
        size_t comp_size = (kv_A(type_cpy, i) == SIZE_COMPONENT_ID)
                               ? sizeof(__intern_comp_size)
                               : ((__intern_comp_size*) ecs_get(instance, kv_A(type_cpy, i), __intern_comp_size))->size;
        kv_push(column, temp->components, (column) { .element_size = comp_size });
        kv_A(temp->components, i).elements = NULL;

        // Add new archetype to the component's column map
        int absent;
        khint_t key = component_map_put(instance->component_index, kv_A(temp->type, i), &absent);
        component_column_map_t* column_map = &kh_val(instance->component_index, key);

        key = component_column_map_put(column_map, temp->id, &absent);
        kh_val(column_map, key) = i;
    }

    return temp;
}



///
/// External Function Implementations
///

ecs_instance* ecs_init() {
    ecs_instance* instance = malloc(sizeof(ecs_instance));
    if(instance == NULL)
        return NULL;

    instance->entity_index = entity_map_init();
    instance->archetype_index = archetype_map_init();
    instance->component_index = component_map_init();
    instance->component_names = component_name_map_init();
    kv_init(instance->id_graveyard);
    instance->next_id = 0;

    if(instance->entity_index && instance->archetype_index && instance->component_index && instance->component_names) {
        COMPONENT_REGISTER(instance, __intern_comp_size);

        return instance;
    }

    // Free memory
    if(instance->entity_index)
        entity_map_destroy(instance->entity_index);
    if(instance->archetype_index)
        archetype_map_destroy(instance->archetype_index);
    if(instance->component_index)
        component_map_destroy(instance->component_index);
    if(instance->component_names)
        component_name_map_destroy(instance->component_names);
    free(instance);

    return NULL;
}
void ecs_destroy(ecs_instance* instance) {
    if(instance == NULL)
        return;

    entity_map_destroy(instance->entity_index);

    khint_t key;
    kh_foreach(instance->archetype_index, key) archetype_destroy(kh_val(instance->archetype_index, key));
    archetype_map_destroy(instance->archetype_index);

    kh_foreach(instance->component_index, key) {
        component_column_map_t* column_map = &kh_val(instance->component_index, key);
        free(column_map->used);
        free(column_map->keys);
    }
    component_map_destroy(instance->component_index);

    component_name_map_destroy(instance->component_names);

    kv_destroy(instance->id_graveyard);

    free(instance);
}

component_id ecs_component_id(ecs_instance* instance, const char* component_name) {
    khint_t key = component_name_map_get(instance->component_names, component_name);

    return (key != kh_end(instance->component_names)) ? kh_val(instance->component_names, key) : INVALID_ID;
}

/// Add an entity to the world and returns its ID
entity_id ecs_entity_create(ecs_instance* instance) {
    entity_id eid;
    if(kv_size(instance->id_graveyard) > 0)
        eid = kv_pop(instance->id_graveyard);
    else
        eid = ((entity_id) instance->next_id++) << 32;
    printf("New Entt: %016lx\n", eid); // DEBUG PRINT

    // TODO Finish implementation
    int absent;
    khint_t key = entity_map_put(instance->entity_index, eid, &absent);

    // TODO Add to an 'empty' archetype
    kh_val(instance->entity_index, key) = (record) { NULL, 0 };

    return eid;
}
entity_id ecs_entity_copy(ecs_instance* instance, entity_id src) {
    fprintf(stderr, "ERROR: \"ecs_entity_copy\" NOT IMPLEMENTED");

    entity_id eid = ecs_entity_create(instance);

    // TODO Add component copying

    return INVALID_ID;
}
void ecs_entity_destroy(ecs_instance* instance, entity_id entity) {
    fprintf(stderr, "ERROR: \"ecs_entity_destroy\" NOT IMPLEMENTED");

    entity &= 0xFFFFFFFF00000000; // Reset flags
    ecs_id_gen_set(entity, ecs_id_gen(entity) + 1);
    kv_push(entity_id, instance->id_graveyard, entity);
}

void ecs_component_register(ecs_instance* instance, const char* component_name, size_t size) {
    entity_id comp_id = ecs_entity_create(instance);

    int absent;
    khint_t key = component_name_map_put(instance->component_names, component_name, &absent);
    kh_val(instance->component_names, key) = comp_id;

    // Create a column map for the component
    key = component_map_put(instance->component_index, comp_id, &absent);
    component_column_map_t* column_map = &kh_val(instance->component_index, key);
    *column_map = (component_column_map_t) { .km = NULL, .bits = 0, .count = 0, .used = NULL, .keys = NULL };

    ecs_add(instance, comp_id, __intern_comp_size);
    ecs_set(instance, comp_id, __intern_comp_size, { .size = size });

    /* TODO Add these to a delete component function and check if this is accurate
     key = component_map_get(instance->component_index, comp_id);
     if(key != kh_end(instance->component_index)) {
         free(column_map->used);
         free(column_map->keys);
         component_map_del(instance->component_index, key);
     }
    */
}
/// Moves an entity based on the archetype specified in the add edge for `component`
bool ecs_component_add(ecs_instance* instance, const entity_id entity, const component_id component) {
    khint_t iter = entity_map_get(instance->entity_index, entity);
    if(iter == kh_end(instance->entity_index))
        return false;

    record* record = &kh_val(instance->entity_index, iter);
    archetype* curr_archetype = record->archetype;

    // If record->archetype is NULL, then this is the first component to be added
    vec_component_id new_type;
    kv_init(new_type);
    if(curr_archetype) {
        // Get the new component list
        kv_copy(uint64_t, new_type, curr_archetype->type);
        kv_push(uint64_t, new_type, component);
        qsort(new_type.a, new_type.n, sizeof(uint64_t), uint64_compare);
    } else {
        kv_push(uint64_t, new_type, component);
    }

#ifdef DEBUG_COMPONENTS
    printf("new_type: ");
    for(size_t i = 0; i < new_type.n; i++)
        printf("%016lx, ", kv_A(new_type, i));
    printf("\n");
#endif

    // Get the new archetype, create one if necessary
    archetype* next_archetype;
    iter = archetype_map_get(instance->archetype_index, new_type);
    if(iter == kh_end(instance->archetype_index))
        next_archetype = archetype_create(instance, &new_type);
    else
        next_archetype = &kh_val(instance->archetype_index, iter);

    kv_destroy(new_type);

    return move_entity(instance, entity, curr_archetype, next_archetype);
}
/// The same as add, but uses the remove edge
bool ecs_component_remove(ecs_instance* instance, const entity_id entity, const component_id component) {
    khint_t iter = entity_map_get(instance->entity_index, entity);
    if(iter == kh_end(instance->entity_index)) {
        return 0;
    }
    record record = kh_val(instance->entity_index, iter);
    archetype* curr_archetype = record.archetype;

    // Get the new component list
    vec_component_id new_type;
    kv_init(new_type);
    kv_copy(uint64_t, new_type, curr_archetype->type);
    for(iter = 0; iter < kv_size(new_type); iter++) {
        if(kv_A(new_type, iter) == component)
            break;
    }
    kv_rm_at(new_type, iter);

    // Get the new archetype, create one if necessary
    archetype* next_archetype;
    iter = archetype_map_get(instance->archetype_index, new_type);
    if(iter == kh_end(instance->archetype_index))
        next_archetype = archetype_create(instance, &new_type);
    else
        next_archetype = &kh_val(instance->archetype_index, iter);
    kv_destroy(new_type);

    return move_entity(instance, entity, curr_archetype, next_archetype);
}

void ecs_component_set(ecs_instance* instance, entity_id entity, component_id component, size_t size, const void* data) {
    void* comp_ptr = ecs_component_get(instance, entity, component);

    memcpy(comp_ptr, data, size);
}

void* ecs_component_get(ecs_instance* instance, const entity_id entity, const component_id component) {
    khint_t iter = entity_map_get(instance->entity_index, entity);
    if(iter == kh_end(instance->entity_index)) {
        return NULL;
    }
    record record = kh_val(instance->entity_index, iter);

    archetype* archetype = record.archetype;
    component_archetypes* archetypes = &kh_val_unsafe(component_map, instance->component_index, component);
    size_t col = kh_val_unsafe(component_column_map, archetypes, archetype->id);

    column* comp_col = &kv_A(archetype->components, col);
    void* comp = (void*) ((uintptr_t) comp_col->elements + (record.index * comp_col->element_size));

#ifdef DEBUG_COMPONENTS
    printf(
        "The arithmatic: (uint64ptr_t*)%p + (%lu(index) * %lu(size))\nResulting in %p a %04lx difference\n",
        comp_col->elements,
        record.index,
        comp_col->element_size,
        comp,
        (uintptr_t) comp - (uintptr_t) comp_col->elements
    );
#endif

    return comp;
}
