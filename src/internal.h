#ifndef OWL_INTERNAL_H
#define OWL_INTERNAL_H

#include <owl/owl.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>
#include <xf86drmMode.h>
#include <stdbool.h>
#include <stdint.h>

#define OWL_MAX_OUTPUTS 8
#define OWL_MAX_WINDOWS 256
#define OWL_MAX_CALLBACKS 16

struct Owl_Output {
    struct Owl_Display* display;
    int pos_x;
    int pos_y;
    int width;
    int height;
    char* name;
    uint32_t drm_connector_id;
    uint32_t drm_crtc_id;
    drmModeModeInfo drm_mode;
    struct gbm_surface* gbm_surface;
    void* egl_surface;
    uint32_t framebuffer_id;
    struct gbm_bo* current_bo;
    struct gbm_bo* next_bo;
    bool page_flip_pending;
};

struct Owl_Window {
    struct Owl_Display* display;
    struct wl_resource* surface_resource;
    struct wl_resource* xdg_surface_resource;
    struct wl_resource* xdg_toplevel_resource;
    int pos_x;
    int pos_y;
    int width;
    int height;
    char* title;
    char* app_id;
    bool fullscreen;
    bool focused;
    bool mapped;
    struct wl_list link;
};

struct Owl_Input {
    uint32_t keycode;
    uint32_t keysym;
    uint32_t modifiers;
    uint32_t button;
    int pointer_x;
    int pointer_y;
};

typedef struct {
    Owl_Window_Callback callback;
    void* data;
} Window_Callback_Entry;

typedef struct {
    Owl_Input_Callback callback;
    void* data;
} Input_Callback_Entry;

typedef struct {
    Owl_Output_Callback callback;
    void* data;
} Output_Callback_Entry;

struct Owl_Display {
    struct wl_display* wayland_display;
    struct wl_event_loop* event_loop;
    const char* socket_name;
    bool running;

    int drm_fd;
    struct gbm_device* gbm_device;
    void* egl_display;
    void* egl_context;
    void* egl_config;

    struct libinput* libinput;
    struct udev* udev;
    struct xkb_context* xkb_context;
    struct xkb_keymap* xkb_keymap;
    struct xkb_state* xkb_state;
    uint32_t modifier_state;

    struct Owl_Output* outputs[OWL_MAX_OUTPUTS];
    int output_count;

    struct wl_list windows;
    int window_count;

    Window_Callback_Entry window_callbacks[8][OWL_MAX_CALLBACKS];
    int window_callback_count[8];

    Input_Callback_Entry input_callbacks[5][OWL_MAX_CALLBACKS];
    int input_callback_count[5];

    Output_Callback_Entry output_callbacks[3][OWL_MAX_CALLBACKS];
    int output_callback_count[3];

    struct wl_event_source* drm_event_source;
    struct wl_event_source* libinput_event_source;
};

void owl_output_init(Owl_Display* display);
void owl_output_cleanup(Owl_Display* display);
void owl_output_render_frame(Owl_Output* output);

void owl_input_init(Owl_Display* display);
void owl_input_cleanup(Owl_Display* display);
void owl_input_process_events(Owl_Display* display);

void owl_render_init(Owl_Display* display);
void owl_render_cleanup(Owl_Display* display);
void owl_render_frame(Owl_Display* display, Owl_Output* output);

void owl_invoke_window_callback(Owl_Display* display, Owl_Window_Event type, Owl_Window* window);
void owl_invoke_input_callback(Owl_Display* display, Owl_Input_Event type, Owl_Input* input);
void owl_invoke_output_callback(Owl_Display* display, Owl_Output_Event type, Owl_Output* output);

#endif
