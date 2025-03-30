#include <complex.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../khashl/khashl.h"
#include "../klib/kvec.h"

#include "../include/ecs.h"



/// Get a value from the given key
/// Performs no bounds checks
#define kh_val_unsafe(name, h, key) kh_val(h, name##_get(h, key))

/// Remove an element within a kvec
/// Preserves the numerical order (deletes, then shifts)
/// Performs no bounds checks on `index`
#define kv_rm_at(vec, index) do {                                       \
        if((index) < kv_size(vec)) {                                    \
            memmove(&kv_A(vec, index),                                  \
                    &kv_A(vec, (index) + 1),                            \
                    (kv_size(vec) - (index) - 1) * sizeof(*(vec).a));   \
            kv_size(vec)--;                                             \
        }                                                               \
    } while (0)



struct ArchetypeEdge {
    Archetype* add;
    Archetype* remove;
};

// Maps ComponentId to an ArchetypeEdge
KHASHL_MAP_INIT(KH_LOCAL, edge_map_t, edge_map, uint64_t, ArchetypeEdge, kh_hash_uint64, kh_eq_generic)
typedef edge_map_t EdgeMap;

typedef kvec_t(Column) VecColumn;
typedef kvec_t(uint64_t) vec_uint64_t;
typedef vec_uint64_t VecComponentId;

struct Archetype {
    ArchetypeId id;         // The hash of `type`
    VecComponentId type;    // Vector of component ids contained in this archetype
    VecColumn components;   // Vector of components for each component
    EdgeMap* edges;         // Stores add/remove archetypes for every ComponentId in this archetype
};

struct Column {
    void* elements;      // buffer with component data
    size_t element_size; // size of a single element
    size_t count;        // number of elements
};

struct Record {
    Archetype* archetype;
    size_t index;   // ie. row
};

khint_t uint64_vec_hash(VecComponentId vec) { return kh_hash_bytes(vec.n, (uint8_t*)vec.a); }
khint_t uint64_vec_compare(VecComponentId a, VecComponentId b) { return (a.n == b.n) ? (memcmp(a.a, b.a, a.n) == 0) : 0; }
static inline int uint64_compare(const void *a, const void *b) {
    uint64_t ua = *(const uint64_t *)a;
    uint64_t ub = *(const uint64_t *)b;
    return (ua > ub) - (ua < ub); // Returns -1, 0, or 1
}

///
/// ECS instance and related type definitions
///
// Maps EntityId to a Record array
// Used to find the archetypes that the entity is a part of
KHASHL_MAP_INIT(KH_LOCAL, entity_map_t, entity_map, uint64_t, Record, kh_hash_uint64, kh_eq_generic)
// Maps a vector of ComponentIds to an Archetype
// Used to find an archetype by its list of components
KHASHL_MAP_INIT(KH_LOCAL, archetype_map_t, archetype_map, VecComponentId, Archetype, uint64_vec_hash, uint64_vec_compare)
// Maps ArchetypeId to a column (size_t)
// Used to find a component's column in a given archetype that it is a part of
// Used in component_index
KHASHL_MAP_INIT(KH_LOCAL, component_column_map_t, component_column_map, uint64_t, size_t, kh_hash_uint64, kh_eq_generic)
// Maps ComponentId to kh_component_columns_t
// Used to get the various archetypes that the given component is a part of
KHASHL_MAP_INIT(KH_LOCAL, component_map_t, component_map, uint64_t, component_column_map_t, kh_hash_uint64, kh_eq_generic)

typedef entity_map_t EntityIndex;
typedef archetype_map_t ArchetypeIndex;
typedef component_map_t ComponentIndex;
typedef component_column_map_t ArchetypeMap;

struct ECSInstance {
    // Global entity tracker, key is EntityId
    EntityIndex* entity_index;

    // Global archetype index, key is a vector of ComponentId
    ArchetypeIndex* archetype_index;

    // Global component index, key is ComponentId
    ComponentIndex* component_index;
};



///
/// Internal Function Implementations
///
/// Moves an entity at row `index` from `src` to `dest`
void move_entity(ECSInstance* instance, Archetype* src, Archetype* dest, size_t index) {

}
/// Create a new Archetype for `type` components
Archetype* archetype_create(ECSInstance instance[static 1], VecComponentId type[static 1], EdgeMap* existing_edges) {
    Archetype* archetype = malloc(sizeof(Archetype));
    if (archetype == NULL)
        return NULL;

    // Initialize type vector
    kv_init(archetype->type);
    kv_copy(uint64_t, archetype->type, *type);
    qsort(archetype->type.a, archetype->type.n, sizeof(uint64_t), uint64_compare);
    kv_init(archetype->components);

    // Initialize component storage
    for (size_t i = 0; i < archetype->type.n; i++) {
        Column column;
        column.elements = malloc(0);
        column.element_size = sizeof(uint64_t);  // TODO: Get actual component size
        column.count = 0;
        kv_push(Column, archetype->components, column);
    }

    // Initialize edge map
    archetype->edges = edge_map_init();

    // If an existing edge map is provided, copy its contents
    if (existing_edges) {
        khint_t iter;   // The index in the hash table
        kh_foreach(existing_edges, iter) {
            // Copy the old [key,val] into the new hashmap
            int ret;
            khint_t iter2 = edge_map_put(archetype->edges, kh_key(existing_edges, iter), &ret);
            kh_val(archetype->edges, iter2) = kh_val(existing_edges, iter);
        }
    }

    // Add new archetype to the global archetype index
    int ret;
    khint_t iter = archetype_map_put(instance->archetype_index, archetype->type, &ret);
    if (ret == 0) {
        free(archetype);
        return NULL;
    }
    kh_val(instance->archetype_index, iter) = *archetype;

    return archetype;
}



///
/// External Function Implementations
///
ECSInstance* ecs_init() {
    ECSInstance* instance = malloc(sizeof(ECSInstance));
    if(instance == NULL)
        return NULL;

    instance->entity_index = entity_map_init();
    instance->archetype_index = archetype_map_init();
    instance->component_index = component_map_init();

    return (instance->entity_index && instance->component_index) ? instance : NULL;
}
/// Moves an entity based on the archetype specified in the add edge for `component`
int add_component(ECSInstance* instance, EntityId entity, ComponentId component) {
    khint_t iter = entity_map_get(instance->entity_index, entity);
    if(iter == kh_end(instance->entity_index)) {
        return 0;
    }
    Record record = kh_val(instance->entity_index, iter);

    Archetype* archetype = record.archetype;
    Archetype* next_archetype;

    iter = edge_map_get(archetype->edges, component);
    if(iter == kh_end(archetype->edges)) {
        // Copy existing component list and add new component
        VecComponentId new_type;
        kv_init(new_type);
        kv_copy(uint64_t, new_type, archetype->type);
        kv_push(uint64_t, new_type, component);
        qsort(new_type.a, new_type.n, sizeof(uint64_t), uint64_compare);

        // Create new archetype
        next_archetype = archetype_create(instance, &new_type, NULL);

        // Store new archetype in edge map
        int ret;
        iter = edge_map_put(archetype->edges, component, &ret);
        ArchetypeEdge* edge = &kh_val(archetype->edges, iter);

        edge->add = next_archetype; // If another component (of this type) is added, go to `next_archetype`
        edge->remove = archetype;   // If this component is removed later, go back here

        kv_destroy(new_type);
    } else {
        next_archetype = kh_val(archetype->edges, iter).add;
    }

    move_entity(instance, archetype, next_archetype, record.index);
    return 1;
}
/// The same as add, but uses the remove edge
int remove_component(ECSInstance* instance, EntityId entity, ComponentId component) {
    khint_t iter = entity_map_get(instance->entity_index, entity);
    if(iter == kh_end(instance->entity_index)) {
        return 0;
    }
    Record record = kh_val(instance->entity_index, iter);

    Archetype* archetype = record.archetype;
    Archetype* next_archetype;

    iter = edge_map_get(archetype->edges, component);
    if(iter == kh_end(archetype->edges)) {
        // Copy existing component list and remove the old component
        VecComponentId new_type;
        kv_init(new_type);
        kv_copy(uint64_t, new_type, archetype->type);
        kv_push(uint64_t, new_type, component);
        khint_t i;
        for(i = 0; i < kv_size(new_type); i++) {
            if(kv_A(new_type, i) == component)
                break;
        }
        kv_rm_at(new_type, i);

        // Create new archetype
        next_archetype = archetype_create(instance, &new_type, NULL);

        // Store new archetype in edge map
        int ret;
        iter = edge_map_put(archetype->edges, component, &ret);
        ArchetypeEdge* edge = &kh_val(archetype->edges, iter);

        edge->add = archetype;          // If another component (of this type) is added, go back to `archetype`
        edge->remove = next_archetype;  // If another component (of this type) is removed, go to `next_archetype`

        kv_destroy(new_type);
    } else {
        next_archetype = kh_val(archetype->edges, iter).remove;
    }

    move_entity(instance, archetype, next_archetype, record.index);
    return 1;
}

void* get_component(ECSInstance* instance, EntityId entity, ComponentId component) {
    khint_t iter = entity_map_get(instance->entity_index, entity);
    if(iter == kh_end(instance->entity_index)) {
        return NULL;
    }
    Record record = kh_val(instance->entity_index, iter);

    Archetype* archetype = record.archetype;
    ArchetypeMap archetypes = kh_val_unsafe(component_map, instance->component_index, component);

    size_t a_record = kh_val_unsafe(component_column_map, &archetypes, archetype->id);
    //return archetype->components[a_record].elements[record->index];
    exit(1);
}
