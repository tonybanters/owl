#include "internal.h"
#include <stdlib.h>
#include <string.h>

Owl_Window** owl_get_windows(Owl_Display* display, int* count) {
    if (!display || !count) {
        if (count) *count = 0;
        return NULL;
    }

    *count = display->window_count;
    if (display->window_count == 0) {
        return NULL;
    }

    static Owl_Window* window_array[OWL_MAX_WINDOWS];
    int index = 0;

    Owl_Window* window;
    wl_list_for_each(window, &display->windows, link) {
        if (index < OWL_MAX_WINDOWS) {
            window_array[index++] = window;
        }
    }

    return window_array;
}

void owl_window_focus(Owl_Window* window) {
    if (!window) {
        return;
    }

    Owl_Window* other;
    wl_list_for_each(other, &window->display->windows, link) {
        if (other->focused && other != window) {
            other->focused = false;
            owl_invoke_window_callback(window->display, OWL_WINDOW_EVENT_UNFOCUS, other);
        }
    }

    window->focused = true;
    owl_invoke_window_callback(window->display, OWL_WINDOW_EVENT_FOCUS, window);
}

void owl_window_move(Owl_Window* window, int x, int y) {
    if (!window) {
        return;
    }
    window->pos_x = x;
    window->pos_y = y;
    owl_invoke_window_callback(window->display, OWL_WINDOW_EVENT_MOVE, window);
}

void owl_window_resize(Owl_Window* window, int width, int height) {
    if (!window) {
        return;
    }
    window->width = width;
    window->height = height;
    owl_invoke_window_callback(window->display, OWL_WINDOW_EVENT_RESIZE, window);
}

void owl_window_close(Owl_Window* window) {
    (void)window;
}

void owl_window_set_fullscreen(Owl_Window* window, bool fullscreen) {
    if (!window) {
        return;
    }
    window->fullscreen = fullscreen;
    owl_invoke_window_callback(window->display, OWL_WINDOW_EVENT_FULLSCREEN, window);
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
