#include <stdio.h>

#include "ecs.h"



typedef struct { float x,y; } pos_comp;

int main(int argc, char** argv) {
    ecs_instance* world = ecs_init();
    entity_id e0 = ecs_new(world);

    COMPONENT_REGISTER(world, pos_comp);
    component_id c0 = ecs_id_str(world, pos_comp);
    printf("%lu\n", c0);

    bool ret = ecs_add(world, e0, c0);
    if(!ret) {
        fprintf(stderr, "Unable to add component\n");
        return 0;
    }

    ecs_set(world, e0, pos_comp, { 1.f, 112415.f });

    {
        pos_comp* comp = ecs_get(world, e0, pos_comp);
        printf("%f, %f\n", comp->x, comp->y);
    }

    pos_comp* comp = ecs_get(world, e0, pos_comp);
    comp->x = 12441.f;
    printf("%f, %f\n", comp->x, comp->y);

    ecs_destroy(world);
    return 0;
}
