#include <stddef.h>

#include "../klib/khash.h"
#include "../klib/kvec.h"

#include "../include/ecs.h"



/// Get a value from the given key
/// Returns NULL if it's not found
#define kh_get_val(name, h, key) ({               \
    khiter_t iter = kh_get(name, h, key);         \
    (iter != kh_end(h)) ? kh_val(h, iter) : NULL; \
})

/// Get a value from the given key
/// Performs no checks
#define kh_get_val_unsafe(name, h, key) kh_val(h, kh_get(name, h, key))

struct ArchetypeEdge {
    Archetype* add;
    Archetype* remove;
};

// Maps ComponentId to an ArchetypeEdge
KHASH_MAP_INIT_INT64(edge, ArchetypeEdge*);
typedef kh_edge_t EdgeMap;

struct Archetype {
    ArchetypeId id;
    ComponentId* type;    // Vector of component ids contained in this archetype
    Column* components;   // Vector of components for each component
    EdgeMap edges;
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

typedef kvec_t(ComponentId) VecComponentId;
khint_t uint64_vec_hash(VecComponentId vec);
khint_t uint64_vec_compare(VecComponentId a, VecComponentId b);

///
/// ECS instance and related type definitions
///
// Maps EntityId to a Record array
// Used to find the archetypes that the entity is a part of
KHASH_MAP_INIT_INT64(entity_index, Record*);
// Maps a vector of ComponentIds to an Archetype
// Used to find an archetype by its list of components
KHASH_INIT(archetype_index, VecComponentId, Archetype, 1, uint64_vec_hash, uint64_vec_compare);
// Maps ArchetypeId to a column (size_t)
// Used to find a component's column in a given archetype that it is a part of
// Used in component_index
KHASH_MAP_INIT_INT64(component_columns, size_t);
// Maps ComponentId to kh_component_columns_t
// Used to get the various archetypes that the given component is a part of
KHASH_MAP_INIT_INT64(component_index, kh_component_columns_t);

typedef kh_entity_index_t EntityIndex;
typedef kh_archetype_index_t ArchetypeIndex;
typedef kh_component_index_t ComponentIndex;
typedef kh_component_columns_t ArchetypeMap;

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

///
/// External Function Implementations
///
ECSInstance* ecs_init() {
    ECSInstance* instance = malloc(sizeof(ECSInstance));
    if(instance == NULL)
        return NULL;

    instance->entity_index = kh_init(entity_index);
    instance->archetype_index = kh_init(archetype_index);
    instance->component_index = kh_init(component_index);

    return (instance->entity_index && instance->component_index) ? instance : NULL;
}
/// Moves an entity based on the archetype specified in the add edge for `component`
void add_component(ECSInstance* instance, EntityId entity, ComponentId component) {
    Record* record = kh_get_val(entity_index, instance->entity_index, entity);
    if(record == NULL)
        return;

    Archetype* archetype = record->archetype;
    Archetype* next_archetype = kh_get_val(edge, &archetype->edges, component)->add;

    move_entity(instance, archetype, next_archetype, record->index);
}
/// The same as add, but uses the remove edge
void remove_component(ECSInstance* instance, EntityId entity, ComponentId component) {
    Record* record = kh_get_val(entity_index, instance->entity_index, entity);
    if(record == NULL)
        return;

    Archetype* archetype = record->archetype;
    Archetype* prev_archetype = kh_get_val_unsafe(edge, &archetype->edges, component)->remove;

    move_entity(instance, archetype, prev_archetype, record->index);
}

void* get_component(ECSInstance* instance, EntityId entity, ComponentId component) {
    Record* record = kh_get_val(entity_index, instance->entity_index, entity);
    if(record == NULL)
        return NULL;

    Archetype* archetype = record->archetype;
    ArchetypeMap archetypes = kh_get_val_unsafe(component_index, instance->component_index, component);

    size_t a_record = kh_get_val_unsafe(component_columns, &archetypes, archetype->id);
    //return archetype->components[a_record].elements[record->index];
    exit(1);
    return NULL;
}
