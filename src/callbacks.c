#include <owl/owl.h>

void owl_set_window_callback(
        Owl_Display* display,
        Owl_Window_Event type,
        Owl_Window_Callback callback,
        void* data
    ) {
    (void)display;
    (void)type;
    (void)callback;
    (void)data;
}

void owl_set_input_callback(
        Owl_Display* display,
        Owl_Input_Event type,
        Owl_Input_Callback callback,
        void* data
    ) {
    (void)display;
    (void)type;
    (void)callback;
    (void)data;
}

void owl_set_output_callback(
        Owl_Display* display,
        Owl_Output_Event type,
        Owl_Output_Callback callback,
        void* data
    ) {
    (void)display;
    (void)type;
    (void)callback;
    (void)data;
}
