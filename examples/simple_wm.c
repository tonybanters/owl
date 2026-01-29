#define _GNU_SOURCE
#include <owl/owl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

static void on_window_create(Owl_Display* display, Owl_Window* window, void* data) {
    (void)data;

    printf("window created: %s\n", owl_window_get_title(window));

    int output_count = 0;
    Owl_Output** outputs = owl_get_outputs(display, &output_count);
    if (output_count > 0) {
        int width = owl_output_get_width(outputs[0]);
        int height = owl_output_get_height(outputs[0]);
        owl_window_move(window, 0, 0);
        owl_window_resize(window, width, height);
    }

    owl_window_focus(window);
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
    uint32_t keycode = owl_input_get_keycode(input);

    if ((mods & OWL_MOD_SUPER) && keysym == 0xff1b) {
        owl_display_terminate(display);
        return;
    }

    if ((mods & OWL_MOD_CTRL) && (mods & OWL_MOD_ALT) && keycode == 14) {
        owl_display_terminate(display);
        return;
    }

    if (keycode == 1) {
        owl_display_terminate(display);
        return;
    }
}

static void on_output_connect(Owl_Display* display, Owl_Output* output, void* data) {
    (void)display;
    (void)data;

    printf("output connected: %s (%dx%d)\n",
           owl_output_get_name(output),
           owl_output_get_width(output),
           owl_output_get_height(output));
}

static FILE* logfile = NULL;

static void log_msg(const char* fmt, ...) {
    if (!logfile) {
        logfile = fopen("/tmp/simple_wm.log", "w");
    }
    if (logfile) {
        va_list args;
        va_start(args, fmt);
        vfprintf(logfile, fmt, args);
        va_end(args);
        fflush(logfile);
    }
}

static void launch_client(const char* cmd, const char* socket_name) {
    log_msg("launch_client called: cmd=%s socket=%s\n", cmd, socket_name ? socket_name : "NULL");
    pid_t pid = fork();
    if (pid < 0) {
        log_msg("fork failed\n");
        return;
    }
    if (pid == 0) {
        FILE* child_log = fopen("/tmp/simple_wm_child.log", "w");
        if (child_log) {
            fprintf(child_log, "child: setting WAYLAND_DISPLAY=%s\n", socket_name);
            fflush(child_log);
        }
        setenv("WAYLAND_DISPLAY", socket_name, 1);

        int log_fd = open("/tmp/foot.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }

        if (child_log) {
            fprintf(child_log, "child: execing: /bin/sh -c %s\n", cmd);
            fflush(child_log);
            fclose(child_log);
        }
        execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
        _exit(1);
    }
    log_msg("fork returned pid=%d\n", pid);
}

int main(int argc, char* argv[]) {
    char* launch_cmd = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            launch_cmd = argv[++i];
        }
    }

    Owl_Display* display = owl_display_create();
    if (!display) {
        fprintf(stderr, "failed to create display\n");
        return 1;
    }

    owl_set_window_callback(display, OWL_WINDOW_EVENT_CREATE, on_window_create, NULL);
    owl_set_window_callback(display, OWL_WINDOW_EVENT_DESTROY, on_window_destroy, NULL);
    owl_set_input_callback(display, OWL_INPUT_KEY_PRESS, on_key_press, NULL);
    owl_set_output_callback(display, OWL_OUTPUT_EVENT_CONNECT, on_output_connect, NULL);

    log_msg("display created, socket=%s\n", owl_display_get_socket_name(display));

    if (launch_cmd) {
        const char* socket = owl_display_get_socket_name(display);
        log_msg("about to launch: %s (socket: %s)\n", launch_cmd, socket);
        launch_client(launch_cmd, socket);
        log_msg("launch_client returned\n");
    }

    printf("simple_wm: running (Super+Escape to quit)\n");
    owl_display_run(display);

    owl_display_destroy(display);
    return 0;
}
