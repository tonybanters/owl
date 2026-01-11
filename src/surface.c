#include <owl/owl.h>
#include <stdlib.h>

struct Owl_Window {
    int pos_x;
    int pos_y;
    int width;
    int height;
    char* title;
    char* app_id;
    bool fullscreen;
    bool focused;
};

Owl_Window** owl_get_windows(Owl_Display* display, int* count) {
    (void)display;
    *count = 0;
    return NULL;
}

void owl_window_focus(Owl_Window* window) {
    if (!window) {
        return;
    }
    window->focused = true;
}

void owl_window_move(Owl_Window* window, int x, int y) {
    if (!window) {
        return;
    }
    window->pos_x = x;
    window->pos_y = y;
}

void owl_window_resize(Owl_Window* window, int width, int height) {
    if (!window) {
        return;
    }
    window->width = width;
    window->height = height;
}

void owl_window_close(Owl_Window* window) {
    (void)window;
}

void owl_window_set_fullscreen(Owl_Window* window, bool fullscreen) {
    if (!window) {
        return;
    }
    window->fullscreen = fullscreen;
}

int owl_window_get_x(Owl_Window* window) {
    return window ? window->pos_x : 0;
}

int owl_window_get_y(Owl_Window* window) {
    return window ? window->pos_y : 0;
}

int owl_window_get_width(Owl_Window* window) {
    return window ? window->width : 0;
}

int owl_window_get_height(Owl_Window* window) {
    return window ? window->height : 0;
}

const char* owl_window_get_title(Owl_Window* window) {
    return window ? window->title : NULL;
}

const char* owl_window_get_app_id(Owl_Window* window) {
    return window ? window->app_id : NULL;
}

bool owl_window_is_fullscreen(Owl_Window* window) {
    return window ? window->fullscreen : false;
}

bool owl_window_is_focused(Owl_Window* window) {
    return window ? window->focused : false;
}
