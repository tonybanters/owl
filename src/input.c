#include <owl/owl.h>
#include <stdlib.h>

struct Owl_Input {
    uint32_t keycode;
    uint32_t keysym;
    uint32_t modifiers;
    uint32_t button;
    int pointer_x;
    int pointer_y;
};

uint32_t owl_input_get_keycode(Owl_Input* input) {
    return input ? input->keycode : 0;
}

uint32_t owl_input_get_keysym(Owl_Input* input) {
    return input ? input->keysym : 0;
}

uint32_t owl_input_get_modifiers(Owl_Input* input) {
    return input ? input->modifiers : 0;
}

uint32_t owl_input_get_button(Owl_Input* input) {
    return input ? input->button : 0;
}

int owl_input_get_pointer_x(Owl_Input* input) {
    return input ? input->pointer_x : 0;
}

int owl_input_get_pointer_y(Owl_Input* input) {
    return input ? input->pointer_y : 0;
}
