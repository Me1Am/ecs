#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <limits.h>



#define INVALID_ID ULONG_MAX



///
/// Types
///

/// Entity identifier, split into the following format
/// |  Low  |      High     |
/// | 00-31 | 32-47 | 48-63 |
/// |  eID  |  Gen  | Flags |
typedef uint64_t entity_id;
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
/// API
///

ecs_instance* ecs_init();
void ecs_destroy(ecs_instance* instance);

// TODO Possibly this to work on other id's (will require creating a "descriptor" struct)
component_id ecs_component_id(ecs_instance* instance, const char* component_name);

entity_id ecs_entity_create(ecs_instance* instance);
entity_id ecs_entity_copy(ecs_instance* instance, entity_id src);
void ecs_entity_destroy(ecs_instance* instance, entity_id entity);

void ecs_component_register(ecs_instance* instance, const char* component_name, size_t size);
bool ecs_component_add(ecs_instance* instance, entity_id entity, component_id component);
bool ecs_component_remove(ecs_instance* instance, entity_id entity, component_id component);
void ecs_component_set(ecs_instance* instance, entity_id entity, component_id component, size_t size, const void* data);
void* ecs_component_get(ecs_instance* instance, entity_id entity, component_id component);



///
/// Convenience Macro API
///

/// @brief Register a component to the `ecs_instance`
/// @param type Component type
#define COMPONENT_REGISTER(ecs_instance, type) ecs_component_register(ecs_instance, #type, sizeof(type))
/// @brief Get an entity's actual ID (uint32)
#define ecs_id_uid(id) ((uint32_t) ((id) >> 32))
/// @brief Get an entity's generation number (uint16)
#define ecs_id_gen(id) ((uint16_t) (((uint32_t) (id)) >> 16))
/// @brief Get an entity's flags (uint16)
#define ecs_id_flags(id) ((uint16_t) (id))
/// @brief Set an entity's actual ID (uint32)
#define ecs_id_uid_set(id, uid)                             \
    (id) = (((entity_id) (uid)) << 32) | ((uint32_t) (id)); \
/// @brief Set an entity's generation number (uint16)
#define ecs_id_gen_set(id, gen)                                                         \
    (id) = ((id) & 0xFFFFFFFF0000FFFF) | (((uint32_t) (gen)) << 16) | ecs_id_flags(id); \
/// @brief Set an entity's flags (binary 16b)
#define ecs_id_flags_set(id, flags) (id) = ((id) & 0xFFFFFFFFFFFF0000) | flags;
/// @brief Get an ID from a name
/// @note Only used for components right now
/// TODO Expand functionality to other types
#define ecs_id_str(ecs_instance, component) ecs_component_id(ecs_instance, #component)
/// @brief Get a new entity
#define ecs_new(ecs_instance) ecs_entity_create(ecs_instance)
/// @brief Get a copy of an existing entity
#define ecs_copy(ecs_instance, src) ecs_entity_copy(ecs_instance, src)
/// @brief Destroy an entity
#define ecs_dest(ecs_instance, entity) ecs_entity_destroy(ecs_instance, entity)
/// @brief Add a component to an entity
/// @param component Component type
/// @note Zeroes the component data
#define ecs_add(ecs_instance, entity, component) ecs_component_add(ecs_instance, entity, ecs_id_str(ecs_instance, #component))
/// @brief Remove a component from an entity
/// @param component Component type
#define ecs_remove(ecs_instance, entity, component)                                  \
    ecs_component_remove(ecs_instance, entity, ecs_id_str(ecs_instance, #component))
/// @brief Set an entity's component
/// @param component Component type
/// @param __VA_ARGS__ (named)list to write to the component
/// @note Will overwrite unset data, just like *comp_ptr = (component) { .y = 1, .z = 8 }
#define ecs_set(ecs_instance, entity, component, ...)                                                                          \
    ecs_component_set(ecs_instance, entity, ecs_id_str(ecs_instance, #component), sizeof(component), &(component) __VA_ARGS__)
/// @brief Get a pointer to an entity's component
/// @param component Component type
#define ecs_get(ecs_instance, entity, component) ecs_component_get(ecs_instance, entity, ecs_id_str(ecs_instance, #component))

