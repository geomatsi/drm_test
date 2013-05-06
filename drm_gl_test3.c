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

/* */

struct DrmOutput;

struct DrmFb {
    struct DrmOutput *output;
    struct gbm_bo *bo;
    uint32_t fbid;
};

struct DrmOutput {
    struct gbm_surface *surface;
    EGLSurface  eglSurface;
    struct DrmFb *current, *next;
    int pageFlipPending;
};

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

static void pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data)
{
    struct DrmOutput *output = (struct DrmOutput *) data;

    fprintf(stdout, "-> %s\n", __func__);

    output->pageFlipPending = 0;

    if (output->current)
    {
        gbm_surface_release_buffer(output->surface, output->current->bo);
    }

    output->current = output->next;
    output->next = NULL;
}

static int
onDrmInput(int fd)
{
    drmEventContext evctx;
    memset(&evctx, 0, sizeof evctx);

    fprintf(stdout, "-> %s\n", __func__);

    evctx.version = DRM_EVENT_CONTEXT_VERSION;
    evctx.page_flip_handler = pageFlipHandler;
    evctx.vblank_handler = NULL;

    drmHandleEvent(fd, &evctx);

    return 1;
}

static void drmFbDestroyCallback(struct gbm_bo *bo, void *data)
{
    struct DrmFb *fb = (struct DrmFb *) data;
    struct gbm_device *gbm = gbm_bo_get_device(bo);

    fprintf(stdout, "-> %s\n", __func__);

    if (fb->fbid)
        drmModeRmFB(gbm_device_get_fd(gbm), fb->fbid);

    free(data);
}

struct DrmFb* drmFbGetFromBo(struct gbm_bo *bo, int fdDev, struct DrmOutput *output)
{
    struct DrmFb *fb = (struct DrmFb *) gbm_bo_get_user_data(bo);
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t handle;
    int ret;

    fprintf(stdout, "-> %s\n", __func__);

    if (fb)
        return fb;

    fprintf(stdout, "-> %s: create new fb\n", __func__);

    fb = (struct DrmFb*) calloc(sizeof(*fb), 1);
    fb->bo = bo;
    fb->output = output;

    width = gbm_bo_get_width(bo);
    height = gbm_bo_get_height(bo);
    stride = gbm_bo_get_stride(bo);
    handle = gbm_bo_get_handle(bo).u32;

    ret = drmModeAddFB(fdDev, width, height, 24, 32, stride, handle, &fb->fbid);
    if (ret)
    {
        return NULL;
    }

    gbm_bo_set_user_data(bo, fb, drmFbDestroyCallback);

    return fb;
}

/* */

int main(int argc, char *argv[])
{
    EGLDisplay dpy;
    EGLContext ctx;
    EGLint major, minor;
    EGLConfig eglConfig;
    EGLint n = 0;

    const char *ver, *extensions;

    drmModeCrtcPtr saved_crtc;
    struct kms_display kms;

    struct DrmOutput output = { 0 };

    struct gbm_device *gbm;

    float angle = 0.0;
    int ret, fd, i;

    struct timeval tv;
    fd_set rset;
    int len, max;

    /* */

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

    output.surface = gbm_surface_create(gbm,
            kms.mode->hdisplay, kms.mode->vdisplay,
            GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    if (!output.surface) {
        fprintf(stderr, "failed to create gbm surface\n");
        ret = -1;
        goto unmake_current;
    }

    output.eglSurface = eglCreateWindowSurface(dpy, eglConfig, (EGLNativeWindowType) output.surface, NULL);

    if (output.eglSurface == EGL_NO_SURFACE) {
        fprintf(stderr, "failed to create egl surface\n");
        ret = -1;
        goto destroy_gbm_surface;
    }

    if (!eglMakeCurrent(dpy, output.eglSurface, output.eglSurface, ctx)) {
        fprintf(stderr, "failed to make eglSurface[%d] current\n", i);
        ret = -1;
        goto destroy_egl_surface;
    }

    saved_crtc = drmModeGetCrtc(fd, kms.crtc->crtc_id);
    if (saved_crtc == NULL) {
        fprintf(stderr, "failed to get current mode\n");
        goto destroy_egl_surface;
    }

    dump_crtc_configuration("saved_crtc", saved_crtc);

    /* */

    for(i = 0; i < 10; i++){

        FD_ZERO(&rset);
        FD_SET(fd, &rset);

        tv.tv_sec = 0;
        tv.tv_usec = 0;

        ret = select(fd + 1, &rset, NULL, NULL,&tv);

        if(ret == -1) {
            if(errno == EINTR) {
                fprintf(stderr, "'select' was interrupted, try again");
                continue;
            } else {
                perror("exit on select error");
                break;
            }
        }

        if (FD_ISSET(fd, &rset)){
            fprintf(stdout, "process drm event\n");
            onDrmInput(fd);
        }


        fprintf(stdout, "render frame\n");
        render_stuff(kms.mode->hdisplay, kms.mode->vdisplay, angle);
        angle += 1.0;

        puts("press enter...");
        getchar();

        if (!output.next)
        {
            eglSwapBuffers(dpy, output.eglSurface);
            struct gbm_bo *bo = gbm_surface_lock_front_buffer(output.surface);
            if (!bo) {
                fprintf(stderr, "no gbm buffer object");
                break;
            }

            output.next = drmFbGetFromBo(bo, fd, &output);
            if (!output.next)
            {
                gbm_surface_release_buffer(output.surface, bo);
                break;
            }
        }

        if (!output.current)
        {
            if (drmModeSetCrtc(fd, kms.crtc->crtc_id, output.next->fbid, 0, 0,
                        &kms.connector->connector_id, 1, kms.mode))
            {
                fprintf(stderr, "failed to set mode in swapBuffers");
                break;
            }
        }

        if (drmModePageFlip(fd, kms.crtc->crtc_id, output.next->fbid, DRM_MODE_PAGE_FLIP_EVENT, &output) < 0)
        {
            fprintf(stderr, "queueing pageflip failed");
            break;
        }

        output.pageFlipPending = 1;
    }

    ret = drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
            saved_crtc->x, saved_crtc->y, &kms.connector->connector_id, 1, &saved_crtc->mode);

    if (ret) {
        fprintf(stderr, "failed to restore crtc: %m\n");
    }

    drmModeFreeCrtc(saved_crtc);

destroy_egl_surface:
    eglDestroySurface(dpy, output.eglSurface);

destroy_gbm_surface:
    gbm_surface_destroy(output.surface);

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
