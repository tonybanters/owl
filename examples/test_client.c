#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct wl_seat *seat;
static struct wl_keyboard *keyboard;
static struct wl_pointer *pointer;
static struct wl_data_device_manager *data_device_manager;
static struct wl_data_device *data_device;
static struct wl_surface *surface;
static struct wl_surface *cursor_surface;
static struct wl_buffer *buffer;
static struct wl_buffer *cursor_buffer;
static uint32_t pointer_enter_serial = 0;
static struct xdg_wm_base *xdg_wm_base;
static struct xdg_surface *xdg_surface;
static struct xdg_toplevel *xdg_toplevel;
static int done = 0;

struct xdg_wm_base_listener xdg_wm_base_listener;
struct xdg_surface_listener xdg_surface_listener;
struct xdg_toplevel_listener xdg_toplevel_listener;

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(base, serial);
}

static void xdg_surface_configure(void *data, struct xdg_surface *surf, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(surf, serial);
    printf("test_client: got configure, acking serial %d\n", serial);
}

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *top,
                                   int32_t width, int32_t height, struct wl_array *states) {
    (void)data;
    (void)top;
    (void)states;
    printf("test_client: toplevel configure %dx%d\n", width, height);
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *top) {
    (void)data;
    (void)top;
    printf("test_client: close requested\n");
    done = 1;
}

static void keyboard_keymap(void *data, struct wl_keyboard *kb, uint32_t format,
                            int32_t fd, uint32_t size) {
    (void)data; (void)kb; (void)format; (void)size;
    close(fd);
    printf("test_client: got keymap\n");
}

static void keyboard_enter(void *data, struct wl_keyboard *kb, uint32_t serial,
                           struct wl_surface *surf, struct wl_array *keys) {
    (void)data; (void)kb; (void)serial; (void)surf; (void)keys;
    printf("test_client: keyboard enter\n");
}

static void keyboard_leave(void *data, struct wl_keyboard *kb, uint32_t serial,
                           struct wl_surface *surf) {
    (void)data; (void)kb; (void)serial; (void)surf;
    printf("test_client: keyboard leave\n");
}

static void keyboard_key(void *data, struct wl_keyboard *kb, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t state) {
    (void)data; (void)kb; (void)serial; (void)time;
    printf("test_client: key %d %s\n", key, state ? "press" : "release");
    if (key == 1) done = 1;
}

static void keyboard_modifiers(void *data, struct wl_keyboard *kb, uint32_t serial,
                               uint32_t dep, uint32_t lat, uint32_t lock, uint32_t group) {
    (void)data; (void)kb; (void)serial; (void)dep; (void)lat; (void)lock; (void)group;
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *kb,
                                 int32_t rate, int32_t delay) {
    (void)data; (void)kb; (void)rate; (void)delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

static void pointer_enter(void *data, struct wl_pointer *ptr, uint32_t serial,
                          struct wl_surface *surf, wl_fixed_t x, wl_fixed_t y) {
    (void)data; (void)surf; (void)x; (void)y;
    printf("test_client: pointer enter\n");
    pointer_enter_serial = serial;
    if (cursor_surface && cursor_buffer) {
        wl_pointer_set_cursor(ptr, serial, cursor_surface, 0, 0);
        printf("test_client: set cursor\n");
    }
}

static void pointer_leave(void *data, struct wl_pointer *ptr, uint32_t serial,
                          struct wl_surface *surf) {
    (void)data; (void)ptr; (void)serial; (void)surf;
    printf("test_client: pointer leave\n");
}

static void pointer_motion(void *data, struct wl_pointer *ptr, uint32_t time,
                           wl_fixed_t x, wl_fixed_t y) {
    (void)data; (void)ptr; (void)time; (void)x; (void)y;
}

static void pointer_button(void *data, struct wl_pointer *ptr, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state) {
    (void)data; (void)ptr; (void)serial; (void)time;
    printf("test_client: button %d %s\n", button, state ? "press" : "release");
}

static void pointer_axis(void *data, struct wl_pointer *ptr, uint32_t time,
                         uint32_t axis, wl_fixed_t value) {
    (void)data; (void)ptr; (void)time; (void)axis; (void)value;
}

static void pointer_frame(void *data, struct wl_pointer *ptr) {
    (void)data; (void)ptr;
}

static void pointer_axis_source(void *data, struct wl_pointer *ptr, uint32_t src) {
    (void)data; (void)ptr; (void)src;
}

static void pointer_axis_stop(void *data, struct wl_pointer *ptr, uint32_t time, uint32_t axis) {
    (void)data; (void)ptr; (void)time; (void)axis;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *ptr, uint32_t axis, int32_t disc) {
    (void)data; (void)ptr; (void)axis; (void)disc;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
};

static void seat_capabilities(void *data, struct wl_seat *st, uint32_t caps) {
    (void)data;
    printf("test_client: seat caps %d\n", caps);
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
        keyboard = wl_seat_get_keyboard(st);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
        printf("test_client: got keyboard\n");
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
        pointer = wl_seat_get_pointer(st);
        wl_pointer_add_listener(pointer, &pointer_listener, NULL);
        printf("test_client: got pointer\n");
    }
}

static void seat_name(void *data, struct wl_seat *st, const char *name) {
    (void)data; (void)st;
    printf("test_client: seat name: %s\n", name);
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void registry_global(void *data, struct wl_registry *reg,
                           uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    printf("test_client: global %s v%d\n", interface, version);

    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        seat = wl_registry_bind(reg, name, &wl_seat_interface, 5);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    } else if (strcmp(interface, "wl_data_device_manager") == 0) {
        data_device_manager = wl_registry_bind(reg, name, &wl_data_device_manager_interface, 3);
        printf("test_client: got data_device_manager\n");
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        xdg_wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    }
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name) {
    (void)data;
    (void)reg;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static struct wl_buffer *create_buffer(int width, int height) {
    int stride = width * 4;
    int size = stride * height;

    char name[] = "/tmp/test-shm-XXXXXX";
    int fd = mkstemp(name);
    unlink(name);
    ftruncate(fd, size);

    uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            data[y * width + x] = 0xFF00FF00;
        }
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    munmap(data, size);

    return buf;
}

int main(void) {
    xdg_wm_base_listener.ping = xdg_wm_base_ping;
    xdg_surface_listener.configure = xdg_surface_configure;
    xdg_toplevel_listener.configure = xdg_toplevel_configure;
    xdg_toplevel_listener.close = xdg_toplevel_close;

    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "test_client: failed to connect\n");
        return 1;
    }
    printf("test_client: connected\n");

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    printf("test_client: roundtrip 1\n");
    wl_display_roundtrip(display);

    if (!compositor || !shm || !xdg_wm_base) {
        fprintf(stderr, "test_client: missing globals\n");
        return 1;
    }

    if (data_device_manager && seat) {
        data_device = wl_data_device_manager_get_data_device(data_device_manager, seat);
        printf("test_client: created data_device\n");
    }

    surface = wl_compositor_create_surface(compositor);
    printf("test_client: surface created\n");

    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    printf("test_client: xdg_surface created\n");

    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(xdg_toplevel, "Test Client");
    printf("test_client: xdg_toplevel created\n");

    wl_surface_commit(surface);
    printf("test_client: initial commit\n");

    printf("test_client: roundtrip 2\n");
    wl_display_roundtrip(display);

    buffer = create_buffer(200, 200);
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, 200, 200);
    wl_surface_commit(surface);
    printf("test_client: buffer committed\n");

    cursor_surface = wl_compositor_create_surface(compositor);
    cursor_buffer = create_buffer(16, 16);
    wl_surface_attach(cursor_surface, cursor_buffer, 0, 0);
    wl_surface_damage(cursor_surface, 0, 0, 16, 16);
    wl_surface_commit(cursor_surface);
    printf("test_client: cursor surface created\n");

    printf("test_client: entering event loop\n");
    while (!done && wl_display_dispatch(display) != -1) {
    }

    printf("test_client: cleanup\n");
    wl_display_disconnect(display);
    return 0;
}
