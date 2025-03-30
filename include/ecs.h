#include <stdint.h>
#include <limits.h>



#define INVALID_ID ULONG_MAX



///
/// Types
///

typedef uint64_t ArchetypeId;
typedef uint64_t ComponentId;
typedef ArchetypeId* ArchetypeSet;  // unordered_set<ArchetypeId>

// Type used to store an array of an unknown component
typedef struct Column Column;
// Edges to other archetypes
typedef struct ArchetypeEdge ArchetypeEdge;
// Type used to store each unique component list only once
typedef struct Archetype Archetype;
// Stores the index to the entity's data in each column in the archetype
typedef struct Record Record;
// Instance data
typedef struct ECSInstance ECSInstance;



///
/// EntityId Definitions
///

/// Entity identifier, split into the following format
/// |  Low  |      High     |
/// | 00-31 | 32-47 | 48-63 |
/// |  eID  |  Gen  | Flags |
typedef uint64_t EntityId;
#define ENTITY_ID(id) ((uint32_t)(id))
#define ENTITY_GENERATION(id) ((uint16_t)((id) >> 32))
#define ENTITY_FLAGS(id) ((uint16_t)((id) >> 48))

///
/// Functions
///

/// Initializes an ECS instance
ECSInstance* ecs_init();
/// Adds a component to an entity and moves the entity to the respective archetype
int add_component(ECSInstance* instance, EntityId entity, ComponentId component);
/// Similar to add_component, but uses the remove edge
int remove_component(ECSInstance* instance, EntityId entity, ComponentId component);
void* get_component(ECSInstance* instance, EntityId entity, ComponentId component);
