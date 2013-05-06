/*
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Benjamin Franzke
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <gbm.h>
#include <drm.h>
#include <xf86drmMode.h>

#include "drm_utils.h"
#include "gl_utils.h"

static const char device_name[] = "/dev/dri/card0";

int main(int argc, char *argv[])
{
    EGLDisplay dpy;
    EGLContext ctx;
    EGLint major, minor;
    EGLConfig eglConfig;
    EGLSurface eglSurface;

    EGLint n = 0;

    const char *ver, *extensions;

    drmModeCrtcPtr saved_crtc;
    struct kms_display kms;

    struct gbm_surface* surface;
    struct gbm_device *gbm;
    struct gbm_bo* bo = NULL;

    uint32_t handle, stride;
    uint32_t fb_id;

    float angle = 0.0;
    int ret, fd, i;

	fd = open(device_name, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "couldn't open %s, skipping\n", device_name);
		return -1;
	}

	/* */

    if (argc > 1)
        ret = drm_get_conf_cmdline(fd, &kms, argc, argv);
    else
        ret = drm_autoconf(fd, &kms);

    if (ret == false) {
        fprintf(stderr, "failed to setup KMS\n");
        ret = -EFAULT;
        goto close_fd;
    }

    dump_drm_configuration(&kms);

	/* */

    gbm = gbm_create_device(fd);
    if (gbm == NULL) {
        fprintf(stderr, "couldn't create gbm device\n");
        ret = -1;
        goto close_fd;
    }

    dpy = eglGetDisplay((EGLNativeDisplayType) gbm);
    if (dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetDisplay() failed\n");
        ret = -1;
        goto destroy_gbm_device;
    }

    if (!eglInitialize(dpy, &major, &minor)) {
        printf("eglInitialize() failed\n");
        ret = -1;
        goto egl_terminate;
    }

    ver = eglQueryString(dpy, EGL_VERSION);
    printf("EGL_VERSION = %s\n", ver);

    extensions = eglQueryString(dpy, EGL_EXTENSIONS);
    printf("EGL_EXTENSIONS: %s\n", extensions);

    if (!strstr(extensions, "EGL_KHR_surfaceless_context")) {
        printf("No support for EGL_KHR_surfaceless_context\n");
        ret = -1;
        goto egl_terminate;
    }

    eglBindAPI(EGL_OPENGL_API);

    static const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE,   1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE,  1,
        EGL_ALPHA_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    if (!eglChooseConfig(dpy, configAttribs, &eglConfig, 1, &n) || n != 1)
    {
        fprintf(stderr, "failed to choose config\n");
        ret = -1;
        goto egl_terminate;
    }

    EGLint contextAttrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    ctx = eglCreateContext(dpy, eglConfig, EGL_NO_CONTEXT, contextAttrs);
    if (!ctx)
    {
        fprintf(stderr, "failed to create context\n");
        ret = -1;
        goto egl_terminate;
    }

    if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        fprintf(stderr, "failed to make context current\n");
        ret = -1;
        goto destroy_context;
    }

    /* */

    surface = gbm_surface_create(gbm, kms.mode->hdisplay, kms.mode->vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    if (!surface) {
        fprintf(stderr, "failed to create gbm surface\n");
        ret = -1;
        goto unmake_current;
    }

    eglSurface = eglCreateWindowSurface(dpy, eglConfig, (EGLNativeWindowType) surface, NULL);

    if (eglSurface == EGL_NO_SURFACE) {
        fprintf(stderr, "failed to create egl surface\n");
        ret = -1;
        goto unmake_current;
    }

    if (!eglMakeCurrent(dpy, eglSurface, eglSurface, ctx))
    {
        fprintf(stderr, "failed to make eglSurface[%d] current\n", i);
        ret = -1;
        goto unmake_current;
    }

    eglSwapBuffers(dpy, eglSurface);

    bo = gbm_surface_lock_front_buffer(surface);
    if (!bo)
    {
        fprintf(stderr, "failed to lock front buffer\n", i);
        ret = -1;
        goto unmake_current;
    }

    handle = gbm_bo_get_handle(bo).u32;
    stride = gbm_bo_get_stride(bo);

    ret = drmModeAddFB(fd, kms.mode->hdisplay, kms.mode->vdisplay, 24, 32, stride, handle, &fb_id);

    if (ret) {
        fprintf(stderr, "failed to create fb\n");
        goto unmake_current;
    }

    gbm_surface_release_buffer(surface, bo);

    saved_crtc = drmModeGetCrtc(fd, kms.crtc->crtc_id);
    if (saved_crtc == NULL) {
        fprintf(stderr, "failed to get current mode\n");
        goto rm_fb;
    }

    dump_crtc_configuration("saved_crtc", saved_crtc);

	/* set new crtc: display DRM framebuffer */

    ret = drmModeSetCrtc(fd, kms.crtc->crtc_id, fb_id, 0, 0,
            &kms.connector->connector_id, 1, kms.mode);

    if (ret) {
        fprintf(stderr, "failed to set mode: %m\n");
        goto free_saved_crtc;
    }

    for(i = 0; i < 10; i++) {

        render_stuff(kms.mode->hdisplay, kms.mode->vdisplay, angle);

        drmModeDirtyFB(fd, fb_id, NULL, 0);

        angle += 1.0;
        puts("press enter...");
        getchar();
    }

    ret = drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
            saved_crtc->x, saved_crtc->y,
            &kms.connector->connector_id, 1, &saved_crtc->mode);

    if (ret) {
        fprintf(stderr, "failed to restore crtc: %m\n");
    }

free_saved_crtc:
    drmModeFreeCrtc(saved_crtc);
rm_fb:
    drmModeRmFB(fd, fb_id);
unmake_current:
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
destroy_context:
    eglDestroyContext(dpy, ctx);
egl_terminate:
    eglTerminate(dpy);
destroy_gbm_device:
    gbm_device_destroy(gbm);
close_fd:
    close(fd);

    return ret;
}
