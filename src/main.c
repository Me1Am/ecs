#include <stdio.h>

#include "ecs.h"



typedef struct {
    float x, y;
} pos_comp;
typedef struct {
    float x, y;
} vel_comp;
typedef struct {
    const char* name;
} name_comp;

int main(int argc, char** argv) {
    ecs_instance* world = ecs_init();
    entity_id e0 = ecs_new(world);
    entity_id e1 = ecs_new(world);
    entity_id e2 = ecs_new(world);

    COMPONENT_REGISTER(world, pos_comp);
    COMPONENT_REGISTER(world, vel_comp);
    COMPONENT_REGISTER(world, name_comp);
    printf("pos id:  %016lx\n", ecs_id_str(world, pos_comp));
    printf("vel id:  %016lx\n", ecs_id_str(world, vel_comp));
    printf("name id: %016lx\n", ecs_id_str(world, name_comp));

    ecs_add(world, e0, pos_comp);
    ecs_add(world, e0, vel_comp);
    printf("Added e0 components\n");

    ecs_add(world, e1, name_comp);
    printf("Added e1 components\n");

    ecs_add(world, e2, pos_comp);
    ecs_add(world, e2, vel_comp);
    ecs_add(world, e2, name_comp);
    printf("Added e2 components\n");

    ecs_set(world, e0, pos_comp, { 1.f, 112415.f });
    ecs_set(world, e0, vel_comp, { 1.f, 0.f });

    ecs_set(world, e1, name_comp, { "What's great is setting tests getting too!\0" });

    ecs_set(world, e2, pos_comp, { 0.f, -12414.f });
    ecs_set(world, e2, vel_comp, { -12958.f, 1025125115.f });
    ecs_set(world, e2, name_comp, { "A real great one because now it works\0" });

    name_comp* name = (name_comp*) ecs_get(world, e1, name_comp);
    printf("e1 is: %s\n", name->name);
    name = (name_comp*) ecs_get(world, e2, name_comp);
    printf("e2 is: %s\n", name->name);

    ecs_destroy(world);
    return 0;
}
