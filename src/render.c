#include "internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static uint32_t get_framebuffer_for_bo(Owl_Display* display, struct gbm_bo* bo) {
    uint32_t* fb_id_ptr = gbm_bo_get_user_data(bo);
    if (fb_id_ptr) {
        return *fb_id_ptr;
    }

    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t handle = gbm_bo_get_handle(bo).u32;

    uint32_t* fb_id = malloc(sizeof(uint32_t));
    if (!fb_id) {
        return 0;
    }

    int result = drmModeAddFB(display->drm_fd, width, height, 24, 32, stride, handle, fb_id);
    if (result) {
        fprintf(stderr, "owl: failed to add framebuffer: %d\n", result);
        free(fb_id);
        return 0;
    }

    gbm_bo_set_user_data(bo, fb_id, NULL);

    return *fb_id;
}

void owl_render_init(Owl_Display* display) {
    (void)display;
}

void owl_render_cleanup(Owl_Display* display) {
    (void)display;
}

void owl_render_frame(Owl_Display* display, Owl_Output* output) {
    if (!display || !output) {
        return;
    }

    if (output->page_flip_pending) {
        return;
    }

    if (!eglMakeCurrent(display->egl_display, output->egl_surface,
                        output->egl_surface, display->egl_context)) {
        fprintf(stderr, "owl: failed to make EGL context current\n");
        return;
    }

    glViewport(0, 0, output->width, output->height);
    glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!eglSwapBuffers(display->egl_display, output->egl_surface)) {
        fprintf(stderr, "owl: failed to swap buffers\n");
        return;
    }

    struct gbm_bo* bo = gbm_surface_lock_front_buffer(output->gbm_surface);
    if (!bo) {
        fprintf(stderr, "owl: failed to lock front buffer\n");
        return;
    }

    uint32_t fb_id = get_framebuffer_for_bo(display, bo);
    if (!fb_id) {
        gbm_surface_release_buffer(output->gbm_surface, bo);
        return;
    }

    if (!output->current_bo) {
        int result = drmModeSetCrtc(display->drm_fd, output->drm_crtc_id, fb_id,
                                    0, 0, &output->drm_connector_id, 1, &output->drm_mode);
        if (result) {
            fprintf(stderr, "owl: failed to set CRTC: %d\n", result);
            gbm_surface_release_buffer(output->gbm_surface, bo);
            return;
        }
        output->current_bo = bo;
        return;
    }

    int result = drmModePageFlip(display->drm_fd, output->drm_crtc_id, fb_id,
                                  DRM_MODE_PAGE_FLIP_EVENT, output);
    if (result) {
        fprintf(stderr, "owl: page flip failed: %d\n", result);
        gbm_surface_release_buffer(output->gbm_surface, bo);
        return;
    }

    output->next_bo = bo;
    output->page_flip_pending = true;
}
