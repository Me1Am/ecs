#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../khashl/khashl.h"
#include "../klib/kvec.h"

#include "../include/ecs.h"



/// Get a value from the given key
/// Performs no bounds checks
#define kh_val_unsafe(name, h, key) kh_val(h, name##_get(h, key))

struct ArchetypeEdge {
    Archetype* add;
    Archetype* remove;
};

// Maps ComponentId to an ArchetypeEdge
KHASHL_MAP_INIT(KH_LOCAL, edge_map_t, edge_map, uint64_t, ArchetypeEdge, kh_hash_uint64, kh_eq_generic)
typedef edge_map_t EdgeMap;

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
khint_t uint64_vec_hash(VecComponentId vec) { return kh_hash_bytes(vec.n, (uint8_t*)vec.a); }
khint_t uint64_vec_compare(VecComponentId a, VecComponentId b) { return (a.n == b.n) ? (memcmp(a.a, b.a, a.n) == 0) : 0; }

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
    Archetype* next_archetype = kh_val_unsafe(edge_map, &archetype->edges, component).add;

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
    Archetype* prev_archetype = kh_val_unsafe(edge_map, &archetype->edges, component).remove;

    move_entity(instance, archetype, prev_archetype, record.index);
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
