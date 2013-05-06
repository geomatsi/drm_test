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

#ifdef GL_OES_EGL_image
static PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES_func;
#endif

static const char device_name[] = "/dev/dri/card0";

int main(int argc, char *argv[])
{
    EGLDisplay dpy;
    EGLContext ctx;
    EGLImageKHR image[2];
    EGLint major, minor;
    const char *ver, *extensions;

    drmModeCrtcPtr saved_crtc;
    struct kms_display kms;

    struct gbm_device *gbm;
    struct gbm_bo *bo[2];

    uint32_t fb[2], color_rb, depth_rb;
    uint32_t handle, stride;
    uint32_t fb_id[2];
    uint32_t fbo;

    float angle = 0.0;
    uint32_t current;
    int ret, fd, i;

	fd = open(device_name, O_RDWR);
	if (fd < 0) {
		/* Probably permissions error */
		fprintf(stderr, "couldn't open %s, skipping\n", device_name);
		return -1;
	}

	/* Find the first available KMS configuration */

	if (!drm_autoconf(fd, &kms)) {
		fprintf(stderr, "failed to setup KMS\n");
		ret = -EFAULT;
		goto close_fd;
	}

    dump_drm_configuration(&kms);

	/* Init EGL and create EGL context */

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

    ctx = eglCreateContext(dpy, NULL, EGL_NO_CONTEXT, NULL);
    if (ctx == NULL) {
        fprintf(stderr, "failed to create context\n");
        ret = -1;
        goto egl_terminate;
    }

    if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        fprintf(stderr, "failed to make context current\n");
        ret = -1;
        goto destroy_context;
    }

#ifdef GL_OES_EGL_image
    glEGLImageTargetRenderbufferStorageOES_func =
        (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC)
        eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
#else
    fprintf(stderr, "GL_OES_EGL_image not supported at compile time\n");
#endif

	/*	Bind together DRM (gbm), EGL (image) and OpenGL (rfb, fb) enitities
	 *		gbm buffer object <-> EGL image <-> GL renderbuffer
	 */

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenRenderbuffers(2, fb);

    for(i = 0; i < 2; ++i) {

        glBindRenderbuffer(GL_RENDERBUFFER, fb[i]);

        bo[i] = gbm_bo_create(gbm, kms.mode->hdisplay, kms.mode->vdisplay,
                    GBM_BO_FORMAT_XRGB8888,
                    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

        if (bo[i] == NULL) {
            fprintf(stderr, "failed to create gbm bo\n");
            ret = -1;
            goto unmake_current;
        }

        image[i] = eglCreateImageKHR(dpy, NULL, EGL_NATIVE_PIXMAP_KHR, bo[i], NULL);

        if (image == EGL_NO_IMAGE_KHR) {
            fprintf(stderr, "failed to create egl image\n");
            ret = -1;
            goto destroy_gbm_bo;
        }

        glEGLImageTargetRenderbufferStorageOES_func(GL_RENDERBUFFER, image[i]);

        handle = gbm_bo_get_handle(bo[i]).u32;
        stride = gbm_bo_get_stride(bo[i]);

        ret = drmModeAddFB(fd, kms.mode->hdisplay, kms.mode->vdisplay,
			24, 32, stride, handle, &fb_id[i]);

        if (ret) {
            fprintf(stderr, "failed to create fb\n");
            goto rm_rb;
        }

    }

    current  = 0;

    /*
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
							  GL_COLOR_ATTACHMENT0,
							  GL_RENDERBUFFER,
							  fb[current]);
    */

    saved_crtc = drmModeGetCrtc(fd, kms.crtc->crtc_id);
    if (saved_crtc == NULL) {
        fprintf(stderr, "failed to get current mode\n");
        goto rm_fb;
    }

    dump_crtc_configuration("saved_crtc", saved_crtc);

	/* set new crtc: display DRM framebuffer */

    ret = drmModeSetCrtc(fd, kms.crtc->crtc_id, fb_id[current], 0, 0,
            &kms.connector->connector_id, 1, kms.mode);

    if (ret) {
        fprintf(stderr, "failed to set mode: %m\n");
        goto free_saved_crtc;
    }

    for(i = 0; i < 50; i++) {

        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
							  GL_COLOR_ATTACHMENT0,
							  GL_RENDERBUFFER,
							  fb[current]);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    		fprintf(stderr, "glCheckFramebufferStatus() failed\n");
	    }

        /* FIXME: for some reason so far only vmware needed it */
        drmModeDirtyFB(fd, fb_id[current], NULL, 0);

        render_stuff(kms.mode->hdisplay, kms.mode->vdisplay, angle);

        if (drmModePageFlip(fd, kms.crtc->crtc_id, fb_id[current], 0, NULL) < 0) {
            fprintf(stderr, "queueing pageflip failed\n");
    	} else {
            fprintf(stderr, "queueing ok\n");
        }

        /*
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
							  GL_COLOR_ATTACHMENT0,
							  GL_RENDERBUFFER,
							  0);
        */

        current ^= 1;
        angle += 1.0;
        usleep(50000);
    }

    ret = drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
            saved_crtc->x, saved_crtc->y,
            &kms.connector->connector_id, 1, &saved_crtc->mode);

    if (ret) {
        fprintf(stderr, "failed to restore crtc: %m\n");
    }

free_saved_crtc:
    drmModeFreeCrtc(saved_crtc);
rm_rb:
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_RENDERBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glDeleteRenderbuffers(1, &fb[0]);
    glDeleteRenderbuffers(1, &fb[1]);
rm_fb:
    drmModeRmFB(fd, fb_id[0]);
    drmModeRmFB(fd, fb_id[1]);
    eglDestroyImageKHR(dpy, image[0]);
    eglDestroyImageKHR(dpy, image[1]);
destroy_gbm_bo:
    gbm_bo_destroy(bo[0]);
    gbm_bo_destroy(bo[1]);
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
