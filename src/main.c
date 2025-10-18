#include <stdio.h>

#include "ecs.h"



typedef struct { float x,y; } pos_comp;

int main(int argc, char** argv) {
    ecs_instance* world = ecs_init();
    entity_id e0 = create_entity(world);

    COMPONENT_REGISTER(world, pos_comp);
    component_id c0 = comp_id(world, pos_comp);
    printf("%lu\n", c0);

    int ret = add_component(world, e0, c0);
    if(ret == 0) {
        fprintf(stderr, "Unable to add component\n");
        return 0;
    }
    printf("Success!! %i\n", ret);

    {
        pos_comp* comp = (pos_comp*) get_component(world, e0, c0);
        *comp = (pos_comp) { 1.f, 112415.f };
    }

    pos_comp* comp = (pos_comp*) get_component(world, e0, c0);
    printf("%f, %f\n", comp->x, comp->y);



    return 0;
}
