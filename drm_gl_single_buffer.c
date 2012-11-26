/*
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2012 matsi
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
	EGLImageKHR image;
	EGLint major, minor;

	GLuint rfb, fb;
    GLfloat angle = 0.0;

	drmModeCrtcPtr saved_crtc, current_crtc;

	struct gbm_device *gbm;
	struct gbm_bo *bo;

	const char *ver, *extensions;
	uint32_t handle, stride, fb_id;
	struct kms_display kms;
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

	if (!strstr(extensions, "EGL_KHR_surfaceless_opengl")) {
		printf("No support for EGL_KHR_surfaceless_opengl\n");
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

	glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);

	glGenRenderbuffers(1, &rfb);
	glBindRenderbuffer(GL_RENDERBUFFER, rfb);

	bo = gbm_bo_create(gbm, kms.mode->hdisplay, kms.mode->vdisplay,
			GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

	if (bo == NULL) {
		fprintf(stderr, "failed to create gbm bo\n");
		ret = -1;
		goto unmake_current;
	}

	image = eglCreateImageKHR(dpy, NULL, EGL_NATIVE_PIXMAP_KHR, bo, NULL);

	if (image == EGL_NO_IMAGE_KHR) {
		fprintf(stderr, "failed to create egl image\n");
		ret = -1;
		goto destroy_gbm_bo;
	}

	glEGLImageTargetRenderbufferStorageOES_func(GL_RENDERBUFFER, image);

	/* create DRM framebuffer bound to gbm buffer object via opaque handler 'handle' */

	handle = gbm_bo_get_handle(bo).u32;
	stride = gbm_bo_get_pitch(bo);

	ret = drmModeAddFB(fd, kms.mode->hdisplay, kms.mode->vdisplay,
			24, 32, stride, handle, &fb_id);

	if (ret) {
        perror("failed drmModeAddFB()");
		goto rm_rb;
	}

	/* store original crtc */

	saved_crtc = drmModeGetCrtc(fd, kms.crtc->crtc_id);
	if (saved_crtc == NULL) {
        perror("failed drmModeGetCrtc(current)");
		goto rm_fb;
	}

    dump_crtc_configuration("saved_crtc", saved_crtc);

	/* set new crtc: display DRM framebuffer */

	ret = drmModeSetCrtc(fd, kms.crtc->crtc_id, fb_id, 0, 0, &kms.connector->connector_id, 1, kms.mode);

	if (ret) {
        perror("failed drmModeSetCrtc(new)");
		goto free_saved_crtc;
	}

    current_crtc = drmModeGetCrtc(fd, kms.crtc->crtc_id);

    if (current_crtc != NULL) {
        dump_crtc_configuration("current_crtc", current_crtc);
    } else {
		perror("failed drmModeGetCrtc(new)");
    }

	/* attach OpenGL framebuffer to renderbuffer bound (via EGL image) to gbm buffer object */

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rfb);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "glCheckFramebufferStatus() failed\n");
	}

	/* render scene
	 * double buffering is not used in this simple examplenotice,
	 * so prepare to notice flickering
	 */

    for(i = 0; i < 10; i++) {
		render_stuff(kms.mode->hdisplay, kms.mode->vdisplay, angle);

        /* FIXME: for some reason so far only vmware needed it */
        drmModeDirtyFB(fd, fb_id, NULL, 0);

        angle += 1.0;
		(void) getchar();
        fprintf(stdout, "press enter to continue...\n");
	}

	/* restore original crtc */

    if (saved_crtc->mode_valid) {
        ret = drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
                saved_crtc->x, saved_crtc->y, &kms.connector->connector_id, 1, &saved_crtc->mode);

        if (ret) {
            perror("failed drmModeSetCrtc(restore original)");
        }
    }

	/* cleanup */

free_saved_crtc:
	drmModeFreeCrtc(saved_crtc);
rm_rb:
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glDeleteRenderbuffers(1, &rfb);
	glDeleteFramebuffers(1, &fb);
rm_fb:
	drmModeRmFB(fd, fb_id);
	eglDestroyImageKHR(dpy, image);
destroy_gbm_bo:
	gbm_bo_destroy(bo);
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
