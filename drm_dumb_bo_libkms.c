#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>

#include <xf86drmMode.h>
#include <xf86drm.h>
#include <libkms.h>

#include "draw_utils.h"
#include "drm_utils.h"

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

int main(char argc, char *argv[])
{
	struct kms_driver *drv;
	struct kms_bo *bo;

	struct kms_display kms_data;

	uint32_t fb, stride, handle;
	uint32_t *dst;
	int ret, fd;

    drmModeCrtcPtr saved_crtc, current_crtc;

	uint32_t attr[] = {
		KMS_WIDTH, 0,
		KMS_HEIGHT, 0,
		KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
		KMS_TERMINATE_PROP_LIST
	};

	fd = open(device_name, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("cannot open drm device");
		exit(-1);
	}

    drmSetMaster(fd);

	/* DRM configuration */

	if (argc <= 1) {

		/* DRM autoconfiguration */

		if (!drm_autoconf(fd, &kms_data)) {
			fprintf(stderr, "failed to setup KMS\n");
			ret = -EFAULT;
			goto err_close;
		}
	} else {

		/* parse command line to get DRM configuration */

		if (!drm_get_conf_cmdline(fd, &kms_data, argc, argv)) {
			fprintf(stderr, "failed to get KMS settings from command line\n");
			ret = -EINVAL;
			goto err_close;
		}
	}

    dump_drm_configuration(&kms_data);

	/* set display dimensions for chosen configuration */

	attr[1] = kms_data.mode->hdisplay;
	attr[3] = kms_data.mode->vdisplay;

	/* create kms driver */

	ret = kms_create(fd, &drv);
	if (ret) {
		perror("failed kms_create()");
		goto err_close;
	}

	/* create dumb buffer object */

	ret = kms_bo_create(drv, attr, &bo);
	if (ret) {
		perror("failed kms_bo_create()");
		goto err_driver_destroy;
	}

	ret = kms_bo_get_prop(bo, KMS_PITCH, &stride);
	if (ret) {
		perror("failed kms_bo_get_prop(KMS_PITCH)");
		goto err_buffer_destroy;
	}

	ret = kms_bo_get_prop(bo, KMS_HANDLE, &handle);
	if (ret) {
		perror("failed kms_bo_get_prop(KMS_HANDLE)");
		goto err_buffer_destroy;
	}

	/* map dumb buffer object */

	ret = kms_bo_map(bo, (void **) &dst);
	if (ret) {
		perror("failed kms_bo_map()");
		goto err_buffer_destroy;
	}

	/* create drm framebuffer */

	ret = drmModeAddFB(fd, kms_data.mode->hdisplay, kms_data.mode->vdisplay, 24, 32, stride, handle, &fb);
	if (ret) {
		perror("failed drmModeAddFB()");
		goto err_buffer_unmap;
	}

	/* store current crtc */

    saved_crtc = drmModeGetCrtc(fd, kms_data.crtc->crtc_id);
    if (saved_crtc == NULL) {
		perror("failed drmModeGetCrtc(current)");
        goto err_buffer_unmap;
    }

    dump_crtc_configuration("saved_crtc", saved_crtc);

	/* setup new crtc */

    ret = drmModeSetCrtc(fd, kms_data.crtc->crtc_id, fb, 0, 0,
            &kms_data.connector->connector_id, 1, kms_data.mode);

	if (ret) {
		perror("failed drmModeSetCrtc(new)");
		goto err_buffer_unmap;
    }

    current_crtc = drmModeGetCrtc(fd, kms_data.crtc->crtc_id);

    if (current_crtc != NULL) {
        dump_crtc_configuration("current_crtc", current_crtc);
    } else {
		perror("failed drmModeGetCrtc(new)");
    }

	/* draw on the screen */

    draw_test_image((uint32_t *) dst, kms_data.mode->hdisplay, kms_data.mode->vdisplay);

    /* FIXME: for some reason so far only vmware needed it */
    drmModeDirtyFB(fd, fb, NULL, 0);

	getchar();

    /* restore original crtc settings */

    if (saved_crtc->mode_valid) {
        ret = drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
                saved_crtc->x, saved_crtc->y, &kms_data.connector->connector_id, 1, &saved_crtc->mode);

        if (ret) {
            perror("failed drmModeSetCrtc(restore original)");
        }
    }

err_fb:
    drmModeRmFB(fd, fb);

err_buffer_unmap:
	ret = kms_bo_unmap(bo);
	if (ret) {
		perror("failed kms_bo_unmap(new)");
	}

err_buffer_destroy:
	ret = kms_bo_destroy(&bo);
	if (ret) {
		perror("failed kms_bo_destroy(new)");
	}

err_driver_destroy:
	ret = kms_destroy(&drv);
	if (ret) {
		perror("failed kms_destroy(new)");
	}

err_close:
	close(fd);

	return ret;
}
