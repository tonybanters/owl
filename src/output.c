#include <owl/owl.h>
#include <stdlib.h>

struct Owl_Output {
    int pos_x;
    int pos_y;
    int width;
    int height;
    char* name;
};

Owl_Output** owl_get_outputs(Owl_Display* display, int* count) {
    (void)display;
    *count = 0;
    return NULL;
}

int owl_output_get_x(Owl_Output* output) {
    return output ? output->pos_x : 0;
}

int owl_output_get_y(Owl_Output* output) {
    return output ? output->pos_y : 0;
}

int owl_output_get_width(Owl_Output* output) {
    return output ? output->width : 0;
}

int owl_output_get_height(Owl_Output* output) {
    return output ? output->height : 0;
}

const char* owl_output_get_name(Owl_Output* output) {
    return output ? output->name : NULL;
}
