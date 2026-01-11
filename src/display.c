#include <owl/owl.h>
#include <stdlib.h>
#include <wayland-server-core.h>

struct Owl_Display {
    struct wl_display* wayland_display;
    struct wl_event_loop* event_loop;
    bool running;
};

Owl_Display* owl_display_create(void) {
    Owl_Display* display = calloc(1, sizeof(Owl_Display));
    if (!display) {
        return NULL;
    }

    display->wayland_display = wl_display_create();
    if (!display->wayland_display) {
        free(display);
        return NULL;
    }

    display->event_loop = wl_display_get_event_loop(display->wayland_display);
    display->running = false;

    return display;
}

void owl_display_destroy(Owl_Display* display) {
    if (!display) {
        return;
    }

    if (display->wayland_display) {
        wl_display_destroy(display->wayland_display);
    }

    free(display);
}

void owl_display_run(Owl_Display* display) {
    if (!display) {
        return;
    }

    display->running = true;

    while (display->running) {
        wl_event_loop_dispatch(display->event_loop, -1);
    }
}

void owl_display_terminate(Owl_Display* display) {
    if (!display) {
        return;
    }

    display->running = false;
}
