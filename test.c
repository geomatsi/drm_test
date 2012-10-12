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
#define GL_GLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/glext.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <gbm.h>
#include <drm.h>
#include <xf86drmMode.h>

#ifdef GL_OES_EGL_image
static PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES_func;
#endif

struct kms {
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeModeInfo mode;
    uint32_t fb_id[2];
};

    static EGLBoolean
setup_kms(int fd, struct kms *kms)
{
    drmModeRes *resources;

    drmModeConnector *connector;
    drmModeEncoder *encoder;
    int i;

    resources = drmModeGetResources(fd);
    if (!resources) {
        fprintf(stderr, "drmModeGetResources failed\n");
        return EGL_FALSE;
    }

    for (i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector == NULL)
            continue;

        if (connector->connection == DRM_MODE_CONNECTED &&
                connector->count_modes > 0)
            break;

        drmModeFreeConnector(connector);
    }

    if (i == resources->count_connectors) {
        fprintf(stderr, "No currently active connector found.\n");
        return EGL_FALSE;
    }

    for (i = 0; i < resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, resources->encoders[i]);

        if (encoder == NULL)
            continue;

        if (encoder->encoder_id == connector->encoder_id)
            break;

        drmModeFreeEncoder(encoder);
    }

    kms->connector = connector;
    kms->encoder = encoder;
    kms->mode = connector->modes[0];

    return EGL_TRUE;
}

    static void
render_stuff(int width, int height, GLfloat rotz)
{
    GLfloat view_rotx = 0.0, view_roty = 0.0, view_rotz = rotz;
    static const GLfloat verts[3][2] = {
        { -1, -1 },
        {  1, -1 },
        {  0,  1 }
    };
    static const GLfloat colors[3][3] = {
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 }
    };
    GLfloat ar = (GLfloat) width / (GLfloat) height;

    glViewport(0, 0, (GLint) width, (GLint) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-ar, ar, -1, 1, 5.0, 60.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -10.0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glRotatef(view_rotx, 1, 0, 0);
    glRotatef(view_roty, 0, 1, 0);
    glRotatef(view_rotz, 0, 0, 1);

    glVertexPointer(2, GL_FLOAT, 0, verts);
    glColorPointer(3, GL_FLOAT, 0, colors);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glPopMatrix();

    glFinish();
}

static const char device_name[] = "/dev/dri/card0";

int main(int argc, char *argv[])
{
    EGLDisplay dpy;
    EGLContext ctx;
    EGLImageKHR image[2];
    EGLint major, minor;
    const char *ver, *extensions;
    GLuint fb[2], color_rb, depth_rb;
    uint32_t handle, stride;
    struct kms kms;
    int ret, fd, i;
    struct gbm_device *gbm;
    struct gbm_bo *bo[2];
    drmModeCrtcPtr saved_crtc;
    GLuint fbo;

    uint32_t current;
    GLfloat angle = 0.0;

    fd = open(device_name, O_RDWR);
    if (fd < 0) {
        /* Probably permissions error */
        fprintf(stderr, "couldn't open %s, skipping\n", device_name);
        return -1;
    }

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

    if (!setup_kms(fd, &kms)) {
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

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenRenderbuffers(2, fb);

    for(i = 0; i < 2; ++i) {

        glBindRenderbuffer(GL_RENDERBUFFER, fb[i]);

        bo[i] = gbm_bo_create(gbm, kms.mode.hdisplay, kms.mode.vdisplay,
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
        stride = gbm_bo_get_pitch(bo[i]);

        ret = drmModeAddFB(fd,
                kms.mode.hdisplay, kms.mode.vdisplay,
                24, 32, stride, handle, &kms.fb_id[i]);

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

    saved_crtc = drmModeGetCrtc(fd, kms.encoder->crtc_id);
    if (saved_crtc == NULL) {
        fprintf(stderr, "failed to get current mode\n");
        goto rm_fb;
    }

    ret = drmModeSetCrtc(fd, kms.encoder->crtc_id, kms.fb_id[current], 0, 0,
            &kms.connector->connector_id, 1, &kms.mode);

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

        render_stuff(kms.mode.hdisplay, kms.mode.vdisplay, angle);

        if (drmModePageFlip(fd, kms.encoder->crtc_id, kms.fb_id[current], 0, NULL) < 0) {
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
    drmModeRmFB(fd, kms.fb_id[0]);
    drmModeRmFB(fd, kms.fb_id[1]);
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
