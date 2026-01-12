#define _GNU_SOURCE
#include "internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <libudev.h>
#include <libinput.h>

static int open_restricted(const char* path, int flags, void* user_data) {
    (void)user_data;
    int fd = open(path, flags);
    if (fd < 0) {
        fprintf(stderr, "owl: failed to open %s: %s\n", path, strerror(errno));
        return -errno;
    }
    fprintf(stderr, "owl: opened input device %s\n", path);
    return fd;
}

static void close_restricted(int fd, void* user_data) {
    (void)user_data;
    close(fd);
}

static const struct libinput_interface libinput_interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void update_modifier_state(Owl_Display* display) {
    if (!display->xkb_state) {
        return;
    }

    display->modifier_state = 0;

    if (xkb_state_mod_name_is_active(display->xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE)) {
        display->modifier_state |= OWL_MOD_SHIFT;
    }
    if (xkb_state_mod_name_is_active(display->xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE)) {
        display->modifier_state |= OWL_MOD_CTRL;
    }
    if (xkb_state_mod_name_is_active(display->xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE)) {
        display->modifier_state |= OWL_MOD_ALT;
    }
    if (xkb_state_mod_name_is_active(display->xkb_state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE)) {
        display->modifier_state |= OWL_MOD_SUPER;
    }
}

static void handle_keyboard_key(Owl_Display* display, struct libinput_event_keyboard* event) {
    uint32_t keycode = libinput_event_keyboard_get_key(event);
    enum libinput_key_state state = libinput_event_keyboard_get_key_state(event);

    if (display->xkb_state) {
        xkb_state_update_key(display->xkb_state, keycode + 8,
                            state == LIBINPUT_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP);
        update_modifier_state(display);
    }

    xkb_keysym_t keysym = XKB_KEY_NoSymbol;
    if (display->xkb_state) {
        keysym = xkb_state_key_get_one_sym(display->xkb_state, keycode + 8);
    }

    struct Owl_Input input = {
        .keycode = keycode,
        .keysym = keysym,
        .modifiers = display->modifier_state,
        .button = 0,
        .pointer_x = 0,
        .pointer_y = 0,
    };

    Owl_Input_Event event_type = state == LIBINPUT_KEY_STATE_PRESSED
        ? OWL_INPUT_KEY_PRESS : OWL_INPUT_KEY_RELEASE;

    owl_invoke_input_callback(display, event_type, &input);
}

static void handle_pointer_motion(Owl_Display* display, struct libinput_event_pointer* event) {
    (void)event;

    struct Owl_Input input = {
        .keycode = 0,
        .keysym = 0,
        .modifiers = display->modifier_state,
        .button = 0,
        .pointer_x = 0,
        .pointer_y = 0,
    };

    owl_invoke_input_callback(display, OWL_INPUT_POINTER_MOTION, &input);
}

static void handle_pointer_button(Owl_Display* display, struct libinput_event_pointer* event) {
    uint32_t button = libinput_event_pointer_get_button(event);
    enum libinput_button_state state = libinput_event_pointer_get_button_state(event);

    struct Owl_Input input = {
        .keycode = 0,
        .keysym = 0,
        .modifiers = display->modifier_state,
        .button = button,
        .pointer_x = 0,
        .pointer_y = 0,
    };

    Owl_Input_Event event_type = state == LIBINPUT_BUTTON_STATE_PRESSED
        ? OWL_INPUT_BUTTON_PRESS : OWL_INPUT_BUTTON_RELEASE;

    owl_invoke_input_callback(display, event_type, &input);
}

static int handle_libinput_event(int fd, uint32_t mask, void* data) {
    (void)fd;
    (void)mask;
    Owl_Display* display = data;

    if (libinput_dispatch(display->libinput) != 0) {
        fprintf(stderr, "owl: failed to dispatch libinput\n");
        return 0;
    }

    struct libinput_event* event;
    while ((event = libinput_get_event(display->libinput)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(event);

        switch (type) {
            case LIBINPUT_EVENT_KEYBOARD_KEY:
                handle_keyboard_key(display, libinput_event_get_keyboard_event(event));
                break;
            case LIBINPUT_EVENT_POINTER_MOTION:
            case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
                handle_pointer_motion(display, libinput_event_get_pointer_event(event));
                break;
            case LIBINPUT_EVENT_POINTER_BUTTON:
                handle_pointer_button(display, libinput_event_get_pointer_event(event));
                break;
            default:
                break;
        }

        libinput_event_destroy(event);
    }

    return 0;
}

void owl_input_init(Owl_Display* display) {
    display->udev = udev_new();
    if (!display->udev) {
        fprintf(stderr, "owl: failed to create udev context\n");
        return;
    }

    display->libinput = libinput_udev_create_context(
        &libinput_interface, display, display->udev);

    if (!display->libinput) {
        fprintf(stderr, "owl: failed to create libinput context\n");
        udev_unref(display->udev);
        display->udev = NULL;
        return;
    }

    if (libinput_udev_assign_seat(display->libinput, "seat0") != 0) {
        fprintf(stderr, "owl: failed to assign seat\n");
        libinput_unref(display->libinput);
        display->libinput = NULL;
        udev_unref(display->udev);
        display->udev = NULL;
        return;
    }

    display->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (display->xkb_context) {
        display->xkb_keymap = xkb_keymap_new_from_names(
            display->xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

        if (display->xkb_keymap) {
            display->xkb_state = xkb_state_new(display->xkb_keymap);
        }
    }

    int libinput_fd = libinput_get_fd(display->libinput);
    display->libinput_event_source = wl_event_loop_add_fd(
        display->event_loop, libinput_fd,
        WL_EVENT_READABLE, handle_libinput_event, display);

    libinput_dispatch(display->libinput);
    struct libinput_event* event;
    while ((event = libinput_get_event(display->libinput)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(event);
        if (type == LIBINPUT_EVENT_DEVICE_ADDED) {
            struct libinput_device* device = libinput_event_get_device(event);
            fprintf(stderr, "owl: input device added: %s\n", libinput_device_get_name(device));
        }
        libinput_event_destroy(event);
    }

    fprintf(stderr, "owl: input initialized\n");
}

void owl_input_cleanup(Owl_Display* display) {
    if (display->libinput_event_source) {
        wl_event_source_remove(display->libinput_event_source);
        display->libinput_event_source = NULL;
    }

    if (display->xkb_state) {
        xkb_state_unref(display->xkb_state);
        display->xkb_state = NULL;
    }

    if (display->xkb_keymap) {
        xkb_keymap_unref(display->xkb_keymap);
        display->xkb_keymap = NULL;
    }

    if (display->xkb_context) {
        xkb_context_unref(display->xkb_context);
        display->xkb_context = NULL;
    }

    if (display->libinput) {
        libinput_unref(display->libinput);
        display->libinput = NULL;
    }

    if (display->udev) {
        udev_unref(display->udev);
        display->udev = NULL;
    }
}

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
