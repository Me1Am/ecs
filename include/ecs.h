#include <stdint.h>



///
/// Types
///

typedef uint64_t ArchetypeId;
typedef uint64_t ComponentId;
typedef uint64_t EntityId;
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
/// Functions
///
/// Initializes an ECS instance
ECSInstance* ecs_init();
/// Adds a component to an entity and moves the entity to the respective archetype
int add_component(ECSInstance* instance, EntityId entity, ComponentId component);
/// Similar to add_component, but uses the remove edge
int remove_component(ECSInstance* instance, EntityId entity, ComponentId component);
void* get_component(ECSInstance* instance, EntityId entity, ComponentId component);
