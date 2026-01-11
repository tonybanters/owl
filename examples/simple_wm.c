#include <owl/owl.h>
#include <stdio.h>
#include <stdlib.h>

static void on_window_create(Owl_Display* display, Owl_Window* window, void* data) {
    (void)display;
    (void)data;

    printf("window created: %s\n", owl_window_get_title(window));
}

static void on_window_destroy(Owl_Display* display, Owl_Window* window, void* data) {
    (void)display;
    (void)data;

    printf("window destroyed: %s\n", owl_window_get_title(window));
}

static void on_key_press(Owl_Display* display, Owl_Input* input, void* data) {
    (void)data;

    uint32_t keysym = owl_input_get_keysym(input);
    uint32_t mods = owl_input_get_modifiers(input);

    if ((mods & OWL_MOD_SUPER) && keysym == 0xff1b) {
        owl_display_terminate(display);
    }
}

static void tile_windows(Owl_Display* display) {
    int window_count = 0;
    Owl_Window** windows = owl_get_windows(display, &window_count);

    if (window_count == 0) {
        return;
    }

    int output_count = 0;
    Owl_Output** outputs = owl_get_outputs(display, &output_count);

    if (output_count == 0) {
        return;
    }

    Owl_Output* output = outputs[0];
    int screen_width = owl_output_get_width(output);
    int screen_height = owl_output_get_height(output);

    if (window_count == 1) {
        owl_window_move(windows[0], 0, 0);
        owl_window_resize(windows[0], screen_width, screen_height);
        return;
    }

    int master_width = screen_width / 2;

    owl_window_move(windows[0], 0, 0);
    owl_window_resize(windows[0], master_width, screen_height);

    int stack_count = window_count - 1;
    int stack_height = screen_height / stack_count;

    for (int index = 1; index < window_count; index++) {
        owl_window_move(windows[index], master_width, (index - 1) * stack_height);
        owl_window_resize(windows[index], screen_width - master_width, stack_height);
    }
}

int main(void) {
    Owl_Display* display = owl_display_create();
    if (!display) {
        fprintf(stderr, "failed to create display\n");
        return 1;
    }

    owl_set_window_callback(display, OWL_WINDOW_EVENT_CREATE, on_window_create, NULL);
    owl_set_window_callback(display, OWL_WINDOW_EVENT_DESTROY, on_window_destroy, NULL);
    owl_set_input_callback(display, OWL_INPUT_KEY_PRESS, on_key_press, NULL);

    printf("simple_wm: starting\n");
    owl_display_run(display);

    owl_display_destroy(display);
    return 0;
}
