#include <stdint.h>
#include <limits.h>



#define INVALID_ID ULONG_MAX



///
/// Types
///

typedef uint64_t archetype_id;
typedef uint64_t component_id;
typedef archetype_id* archatype_set; // unordered_set<ArchetypeId>

// Type used to store an array of an unknown component
typedef struct column_t column;
// Edges to other archetypes
typedef struct archetype_edge_t archetype_edge;
// Type used to store each unique component list only once
typedef struct archetype_t archetype;
// Stores the index to the entity's data in each column in the archetype
typedef struct record_t record;
// Instance data
typedef struct ecs_instance_t ecs_instance;



///
/// EntityId Definitions
///

/// Entity identifier, split into the following format
/// |  Low  |      High     |
/// | 00-31 | 32-47 | 48-63 |
/// |  eID  |  Gen  | Flags |
typedef uint64_t entity_id;
#define ENTITY_ID(id) ((uint32_t) (id))
#define ENTITY_GENERATION(id) ((uint16_t) ((id) >> 32))
#define ENTITY_FLAGS(id) ((uint16_t) ((id) >> 48))



#define COMPONENT_REGISTER(ecs_instance, type) register_component(ecs_instance, #type)
#define comp_id(ecs_instance, component) get_component_id(ecs_instance, #component)

void register_component(ecs_instance* instance, const char* component_name);
component_id get_component_id(ecs_instance* instance, const char* component_name);

///
/// Functions
///

/// Initializes an ECS instance
ecs_instance* ecs_init();

entity_id create_entity(ecs_instance* instance);

/// Adds a component to an entity and moves the entity to the respective archetype
int add_component(ecs_instance* instance, entity_id entity, component_id component);
/// Removes a component from an entity and moves the entity to the respective archetype
int remove_component(ecs_instance* instance, entity_id entity, component_id component);
void* get_component(ecs_instance* instance, entity_id entity, component_id component);
