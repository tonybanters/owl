#define _GNU_SOURCE
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

static void shm_pool_destroy_handler(struct wl_resource* resource) {
    Owl_Shm_Pool* pool = wl_resource_get_user_data(resource);
    if (!pool) {
        return;
    }

    pool->ref_count--;
    if (pool->ref_count <= 0) {
        if (pool->data) {
            munmap(pool->data, pool->size);
        }
        if (pool->fd >= 0) {
            close(pool->fd);
        }
        free(pool);
    }
}

static void shm_buffer_destroy_handler(struct wl_resource* resource) {
    Owl_Shm_Buffer* buffer = wl_resource_get_user_data(resource);
    if (!buffer) {
        return;
    }

    if (buffer->pool) {
        buffer->pool->ref_count--;
        if (buffer->pool->ref_count <= 0 && buffer->pool->resource == NULL) {
            if (buffer->pool->data) {
                munmap(buffer->pool->data, buffer->pool->size);
            }
            if (buffer->pool->fd >= 0) {
                close(buffer->pool->fd);
            }
            free(buffer->pool);
        }
    }

    free(buffer);
}

static void shm_buffer_destroy(struct wl_client* client, struct wl_resource* resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_buffer_interface buffer_interface = {
    .destroy = shm_buffer_destroy,
};

static void shm_pool_create_buffer(struct wl_client* client, struct wl_resource* resource,
                                   uint32_t id, int32_t offset, int32_t width, int32_t height,
                                   int32_t stride, uint32_t format) {
    Owl_Shm_Pool* pool = wl_resource_get_user_data(resource);
    if (!pool) {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "invalid pool");
        return;
    }

    if (offset < 0 || width <= 0 || height <= 0 || stride < width * 4) {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE, "invalid buffer parameters");
        return;
    }

    if (offset + stride * height > pool->size) {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE, "buffer extends past pool");
        return;
    }

    if (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888) {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FORMAT, "unsupported format");
        return;
    }

    Owl_Shm_Buffer* buffer = calloc(1, sizeof(Owl_Shm_Buffer));
    if (!buffer) {
        wl_resource_post_no_memory(resource);
        return;
    }

    buffer->pool = pool;
    buffer->offset = offset;
    buffer->width = width;
    buffer->height = height;
    buffer->stride = stride;
    buffer->format = format;
    buffer->busy = false;

    pool->ref_count++;

    buffer->resource = wl_resource_create(client, &wl_buffer_interface, 1, id);
    if (!buffer->resource) {
        pool->ref_count--;
        free(buffer);
        wl_resource_post_no_memory(resource);
        return;
    }

    wl_resource_set_implementation(buffer->resource, &buffer_interface, buffer, shm_buffer_destroy_handler);
}

static void shm_pool_destroy(struct wl_client* client, struct wl_resource* resource) {
    (void)client;
    Owl_Shm_Pool* pool = wl_resource_get_user_data(resource);
    if (pool) {
        pool->resource = NULL;
    }
    wl_resource_destroy(resource);
}

static void shm_pool_resize(struct wl_client* client, struct wl_resource* resource, int32_t size) {
    (void)client;
    Owl_Shm_Pool* pool = wl_resource_get_user_data(resource);
    if (!pool) {
        return;
    }

    if (size < pool->size) {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "cannot shrink pool");
        return;
    }

    void* new_data = mremap(pool->data, pool->size, size, MREMAP_MAYMOVE);
    if (new_data == MAP_FAILED) {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "failed to resize pool");
        return;
    }

    pool->data = new_data;
    pool->size = size;
}

static const struct wl_shm_pool_interface shm_pool_interface = {
    .create_buffer = shm_pool_create_buffer,
    .destroy = shm_pool_destroy,
    .resize = shm_pool_resize,
};

static void shm_create_pool(struct wl_client* client, struct wl_resource* resource,
                            uint32_t id, int32_t fd, int32_t size) {
    Owl_Display* display = wl_resource_get_user_data(resource);

    if (size <= 0) {
        close(fd);
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "invalid pool size");
        return;
    }

    Owl_Shm_Pool* pool = calloc(1, sizeof(Owl_Shm_Pool));
    if (!pool) {
        close(fd);
        wl_resource_post_no_memory(resource);
        return;
    }

    pool->display = display;
    pool->fd = fd;
    pool->size = size;
    pool->ref_count = 1;

    pool->data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (pool->data == MAP_FAILED) {
        close(fd);
        free(pool);
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "failed to mmap pool");
        return;
    }

    pool->resource = wl_resource_create(client, &wl_shm_pool_interface, 1, id);
    if (!pool->resource) {
        munmap(pool->data, size);
        close(fd);
        free(pool);
        wl_resource_post_no_memory(resource);
        return;
    }

    wl_resource_set_implementation(pool->resource, &shm_pool_interface, pool, shm_pool_destroy_handler);
}

static const struct wl_shm_interface shm_interface = {
    .create_pool = shm_create_pool,
};

static void shm_bind(struct wl_client* client, void* data, uint32_t version, uint32_t id) {
    Owl_Display* display = data;
    (void)version;

    struct wl_resource* resource = wl_resource_create(client, &wl_shm_interface, 1, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &shm_interface, display, NULL);

    wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
    wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);
}

static void surface_state_init(Owl_Surface_State* state) {
    memset(state, 0, sizeof(Owl_Surface_State));
    wl_list_init(&state->frame_callbacks);
}

static void surface_state_cleanup(Owl_Surface_State* state) {
    Owl_Frame_Callback* callback;
    Owl_Frame_Callback* tmp;
    wl_list_for_each_safe(callback, tmp, &state->frame_callbacks, link) {
        wl_list_remove(&callback->link);
        free(callback);
    }
}

static void surface_destroy_handler(struct wl_resource* resource) {
    Owl_Surface* surface = wl_resource_get_user_data(resource);
    if (!surface) {
        return;
    }

    wl_list_remove(&surface->link);
    surface->display->surface_count--;

    surface_state_cleanup(&surface->pending);
    surface_state_cleanup(&surface->current);

    free(surface);
}

static void surface_destroy(struct wl_client* client, struct wl_resource* resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void surface_attach(struct wl_client* client, struct wl_resource* resource,
                           struct wl_resource* buffer_resource, int32_t x, int32_t y) {
    (void)client;
    Owl_Surface* surface = wl_resource_get_user_data(resource);
    if (!surface) {
        return;
    }

    surface->pending.buffer = buffer_resource ? wl_resource_get_user_data(buffer_resource) : NULL;
    surface->pending.buffer_x = x;
    surface->pending.buffer_y = y;
    surface->pending.buffer_attached = true;
}

static void surface_damage(struct wl_client* client, struct wl_resource* resource,
                           int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client;
    Owl_Surface* surface = wl_resource_get_user_data(resource);
    if (!surface) {
        return;
    }

    surface->pending.damage_x = x;
    surface->pending.damage_y = y;
    surface->pending.damage_width = width;
    surface->pending.damage_height = height;
    surface->pending.has_damage = true;
}

static void surface_frame(struct wl_client* client, struct wl_resource* resource, uint32_t callback_id) {
    Owl_Surface* surface = wl_resource_get_user_data(resource);
    if (!surface) {
        return;
    }

    Owl_Frame_Callback* callback = calloc(1, sizeof(Owl_Frame_Callback));
    if (!callback) {
        wl_resource_post_no_memory(resource);
        return;
    }

    callback->resource = wl_resource_create(client, &wl_callback_interface, 1, callback_id);
    if (!callback->resource) {
        free(callback);
        wl_resource_post_no_memory(resource);
        return;
    }

    wl_resource_set_implementation(callback->resource, NULL, callback, NULL);
    wl_list_insert(surface->pending.frame_callbacks.prev, &callback->link);
}

static void surface_set_opaque_region(struct wl_client* client, struct wl_resource* resource,
                                      struct wl_resource* region) {
    (void)client;
    (void)resource;
    (void)region;
}

static void surface_set_input_region(struct wl_client* client, struct wl_resource* resource,
                                     struct wl_resource* region) {
    (void)client;
    (void)resource;
    (void)region;
}

static Owl_Window* find_window_for_surface(Owl_Display* display, Owl_Surface* surface) {
    Owl_Window* window;
    wl_list_for_each(window, &display->windows, link) {
        if (window->surface == surface) {
            return window;
        }
    }
    return NULL;
}

static void surface_commit(struct wl_client* client, struct wl_resource* resource) {
    (void)client;
    Owl_Surface* surface = wl_resource_get_user_data(resource);
    if (!surface) {
        return;
    }

    if (surface->pending.buffer_attached) {
        surface->current.buffer = surface->pending.buffer;
        surface->current.buffer_x = surface->pending.buffer_x;
        surface->current.buffer_y = surface->pending.buffer_y;
        surface->pending.buffer_attached = false;
    }

    if (surface->pending.has_damage) {
        surface->current.damage_x = surface->pending.damage_x;
        surface->current.damage_y = surface->pending.damage_y;
        surface->current.damage_width = surface->pending.damage_width;
        surface->current.damage_height = surface->pending.damage_height;
        surface->current.has_damage = true;
        surface->pending.has_damage = false;
    }

    wl_list_insert_list(&surface->current.frame_callbacks, &surface->pending.frame_callbacks);
    wl_list_init(&surface->pending.frame_callbacks);

    if (surface->current.buffer) {
        owl_render_upload_texture(surface->display, surface);
        surface->has_content = true;

        Owl_Window* window = find_window_for_surface(surface->display, surface);
        if (window && window->xdg_toplevel_resource && !window->mapped) {
            if (window->width == 0 && window->height == 0) {
                window->width = surface->texture_width;
                window->height = surface->texture_height;
            }
            owl_window_map(window);
        }

        for (int index = 0; index < surface->display->output_count; index++) {
            owl_render_frame(surface->display, surface->display->outputs[index]);
        }
    }
}

static void surface_set_buffer_transform(struct wl_client* client, struct wl_resource* resource,
                                         int32_t transform) {
    (void)client;
    (void)resource;
    (void)transform;
}

static void surface_set_buffer_scale(struct wl_client* client, struct wl_resource* resource,
                                     int32_t scale) {
    (void)client;
    (void)resource;
    (void)scale;
}

static void surface_damage_buffer(struct wl_client* client, struct wl_resource* resource,
                                  int32_t x, int32_t y, int32_t width, int32_t height) {
    surface_damage(client, resource, x, y, width, height);
}

static void surface_offset(struct wl_client* client, struct wl_resource* resource,
                           int32_t x, int32_t y) {
    (void)client;
    (void)resource;
    (void)x;
    (void)y;
}

static const struct wl_surface_interface surface_interface = {
    .destroy = surface_destroy,
    .attach = surface_attach,
    .damage = surface_damage,
    .frame = surface_frame,
    .set_opaque_region = surface_set_opaque_region,
    .set_input_region = surface_set_input_region,
    .commit = surface_commit,
    .set_buffer_transform = surface_set_buffer_transform,
    .set_buffer_scale = surface_set_buffer_scale,
    .damage_buffer = surface_damage_buffer,
    .offset = surface_offset,
};

static void compositor_create_surface(struct wl_client* client, struct wl_resource* resource,
                                      uint32_t id) {
    Owl_Display* display = wl_resource_get_user_data(resource);

    Owl_Surface* surface = calloc(1, sizeof(Owl_Surface));
    if (!surface) {
        wl_resource_post_no_memory(resource);
        return;
    }

    surface->display = display;
    surface_state_init(&surface->pending);
    surface_state_init(&surface->current);

    uint32_t version = wl_resource_get_version(resource);
    surface->resource = wl_resource_create(client, &wl_surface_interface, version, id);
    if (!surface->resource) {
        free(surface);
        wl_resource_post_no_memory(resource);
        return;
    }

    wl_resource_set_implementation(surface->resource, &surface_interface, surface, surface_destroy_handler);

    wl_list_insert(&display->surfaces, &surface->link);
    display->surface_count++;

    fprintf(stderr, "owl: surface created (total: %d)\n", display->surface_count);
}

static void region_destroy_handler(struct wl_resource* resource) {
    (void)resource;
}

static void region_destroy(struct wl_client* client, struct wl_resource* resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void region_add(struct wl_client* client, struct wl_resource* resource,
                       int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client;
    (void)resource;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

static void region_subtract(struct wl_client* client, struct wl_resource* resource,
                            int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client;
    (void)resource;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

static const struct wl_region_interface region_interface = {
    .destroy = region_destroy,
    .add = region_add,
    .subtract = region_subtract,
};

static void compositor_create_region(struct wl_client* client, struct wl_resource* resource,
                                     uint32_t id) {
    struct wl_resource* region_resource = wl_resource_create(client, &wl_region_interface, 1, id);
    if (!region_resource) {
        wl_resource_post_no_memory(resource);
        return;
    }

    wl_resource_set_implementation(region_resource, &region_interface, NULL, region_destroy_handler);
}

static const struct wl_compositor_interface compositor_interface = {
    .create_surface = compositor_create_surface,
    .create_region = compositor_create_region,
};

static void compositor_bind(struct wl_client* client, void* data, uint32_t version, uint32_t id) {
    Owl_Display* display = data;

    uint32_t bound_version = version < 6 ? version : 6;
    struct wl_resource* resource = wl_resource_create(client, &wl_compositor_interface, bound_version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &compositor_interface, display, NULL);
}

void owl_surface_init(Owl_Display* display) {
    wl_list_init(&display->surfaces);

    display->compositor_global = wl_global_create(display->wayland_display,
        &wl_compositor_interface, 6, display, compositor_bind);

    if (!display->compositor_global) {
        fprintf(stderr, "owl: failed to create wl_compositor global\n");
        return;
    }

    display->shm_global = wl_global_create(display->wayland_display,
        &wl_shm_interface, 1, display, shm_bind);

    if (!display->shm_global) {
        fprintf(stderr, "owl: failed to create wl_shm global\n");
        return;
    }

    fprintf(stderr, "owl: surface protocol initialized\n");
}

void owl_surface_cleanup(Owl_Display* display) {
    Owl_Surface* surface;
    Owl_Surface* tmp;
    wl_list_for_each_safe(surface, tmp, &display->surfaces, link) {
        wl_resource_destroy(surface->resource);
    }

    if (display->shm_global) {
        wl_global_destroy(display->shm_global);
        display->shm_global = NULL;
    }

    if (display->compositor_global) {
        wl_global_destroy(display->compositor_global);
        display->compositor_global = NULL;
    }
}

Owl_Surface* owl_surface_from_resource(struct wl_resource* resource) {
    if (!resource) {
        return NULL;
    }
    return wl_resource_get_user_data(resource);
}

void owl_surface_send_frame_done(Owl_Display* display, uint32_t time) {
    Owl_Surface* surface;
    wl_list_for_each(surface, &display->surfaces, link) {
        Owl_Frame_Callback* callback;
        Owl_Frame_Callback* tmp;
        wl_list_for_each_safe(callback, tmp, &surface->current.frame_callbacks, link) {
            wl_callback_send_done(callback->resource, time);
            wl_resource_destroy(callback->resource);
            wl_list_remove(&callback->link);
            free(callback);
        }
    }
}

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
        if (index < OWL_MAX_WINDOWS && window->mapped) {
            window_array[index++] = window;
        }
    }

    *count = index;
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
    if (!window) {
        return;
    }
    owl_xdg_toplevel_send_close(window);
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
