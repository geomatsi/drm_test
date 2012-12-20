#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>

#include <xf86drmMode.h>
#include <xf86drm.h>
#include <libkms.h>

#include "bitmap_utils.h"
#include "drm_utils.h"

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

int main(char argc, char *argv[])
{
	int ret, opt, i;
	int fd;

	struct kms_driver *drv;
	struct kms_bo *bo_crtc, *bo_plane;

	uint32_t plane_id = 0;
	uint32_t crtc_id = 0;
	uint32_t conn_id = 0;

	uint32_t width = 0, height = 0;
	uint32_t posx = 0, posy = 0;
	int fb_crtc, fb_plane;

	uint32_t *dst_crtc, *dst_plane;
	uint32_t stride, handle;
	char *mode_name;

	uint32_t attr[] = {
		KMS_WIDTH, 0,
		KMS_HEIGHT, 0,
		KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
		KMS_TERMINATE_PROP_LIST
	};

    drmModeCrtcPtr saved_crtc, current_crtc;
	drmModePlaneRes *resources;
	drmModePlane *plane = NULL;
	drmModeModeInfo *mode;

	/* parse command line */

	while ((opt = getopt(argc, argv, "n:x:y:w:v:c:p:m:h")) != -1) {
		switch (opt) {
			case 'm':
				mode_name = strdup(optarg);
				break;
			case 'n':
				conn_id = atoi(optarg);
				break;
			case 'x':
				posx = atoi(optarg);
				break;
			case 'y':
				posy = atoi(optarg);
				break;
			case 'w':
				width = atoi(optarg);
				break;
			case 'v':
				height = atoi(optarg);
				break;
			case 'p':
				plane_id = atoi(optarg);
				break;
			case 'c':
				crtc_id = atoi(optarg);
				break;
			case 'h':
			default:
				printf("usage: -h] -c <connector> -e <encoder> -m <mode>\n");
				printf("\t-h: this help message\n");
				printf("\t-n <connector>	connector id, default is 0\n");
				printf("\t-m <mode>			mode name, default is 'preferred'\n");
				printf("\t-c <crtc>			crtc id, default is 0\n");
				printf("\t-p <plane>		plane id, default is 0\n");
				printf("\t-x <posx>			plane top left corner xpos, default is 0'\n");
				printf("\t-y <posy>			plane top left corner ypos, default is 0'\n");
				printf("\t-w <width>		plane width, default is 0'\n");
				printf("\t-v <height>		plane height, default is 0'\n");
				exit(0);
		}
	}

	/* drm init */

	fd = open(device_name, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("cannot open drm device");
		goto err_exit;
	}

    drmSetMaster(fd);

	ret = kms_create(fd, &drv);
	if (ret) {
		perror("failed kms_create()");
		goto err_close_drm;
	}

	 /*	DRM: crtc configuration */

	mode = drm_get_mode_by_name(fd, conn_id, mode_name);

	if (!mode) {
		perror("failed drm_get_mode_by_name");
		goto err_driver_destroy;
	}

	attr[1] = mode->hdisplay;
	attr[3] = mode->vdisplay;

	ret = kms_bo_create(drv, attr, &bo_crtc);
	if (ret) {
		perror("failed kms_bo_create()");
		goto err_driver_destroy;
	}

	ret = kms_bo_get_prop(bo_crtc, KMS_PITCH, &stride);
	if (ret) {
		perror("failed kms_bo_get_prop(KMS_PITCH)");
		goto err_crtc_buffer_destroy;
	}

	ret = kms_bo_get_prop(bo_crtc, KMS_HANDLE, &handle);
	if (ret) {
		perror("failed kms_bo_get_prop(KMS_HANDLE)");
		goto err_crtc_buffer_destroy;
	}

	ret = kms_bo_map(bo_crtc, (void **) &dst_crtc);
	if (ret) {
		perror("failed kms_bo_map()");
		goto err_crtc_buffer_destroy;
	}

	ret = drmModeAddFB(fd, mode->hdisplay, mode->vdisplay, 24, 32, stride, handle, &fb_crtc);
	if (ret) {
		perror("failed drmModeAddFB()");
		goto err_crtc_buffer_unmap;
	}

    saved_crtc = drmModeGetCrtc(fd, crtc_id);
    if (saved_crtc == NULL) {
		perror("failed drmModeGetCrtc(current)");
        goto err_crtc_rm_fb;
    }

    dump_crtc_configuration("saved_crtc", saved_crtc);

    ret = drmModeSetCrtc(fd, crtc_id, fb_crtc, 0, 0, &conn_id, 1, mode);
	if (ret) {
		perror("failed drmModeSetCrtc(new)");
		goto err_crtc_rm_fb;
    }

    current_crtc = drmModeGetCrtc(fd, crtc_id);

    if (current_crtc != NULL) {
        dump_crtc_configuration("current_crtc", current_crtc);
    } else {
		perror("failed drmModeGetCrtc(new)");
    }

	/* background image: draw image on crtc */

    draw_test_image((uint32_t *) dst_crtc, mode->hdisplay, mode->vdisplay);

    /* FIXME: for some reason so far only vmware needed it */
    drmModeDirtyFB(fd, fb_crtc, NULL, 0);

	getchar();

	/* DRM: configure plane */

	resources = drmModeGetPlaneResources(fd);
	if (!resources || resources->count_planes == 0) {
		fprintf(stderr, "drmModeGetPlaneResources failed\n");
		ret = -ENODEV;
		goto err_crtc_exit;
	}

	for (i = 0; i < resources->count_planes; i++) {
		drmModePlane *p = drmModeGetPlane(fd, resources->planes[i]);
		if (!p)
			continue;

		if (p->plane_id == plane_id) {
			plane = p;
			break;
		}

		drmModeFreePlane(plane);
	}

	if (!plane) {
		fprintf(stderr, "couldn't find specified plane\n");
		ret = -ENODEV;
		goto err_crtc_exit;
	}

	attr[1] = width;
	attr[3] = height;

	ret = kms_bo_create(drv, attr, &bo_plane);
	if (ret) {
		perror("failed kms_bo_create()");
		goto err_crtc_exit;
	}

	ret = kms_bo_get_prop(bo_plane, KMS_PITCH, &stride);
	if (ret) {
		perror("failed kms_bo_get_prop(KMS_PITCH)");
		goto err_plane_buffer_destroy;
	}

	ret = kms_bo_get_prop(bo_plane, KMS_HANDLE, &handle);
	if (ret) {
		perror("failed kms_bo_get_prop(KMS_HANDLE)");
		goto err_plane_buffer_destroy;
	}

	ret = kms_bo_map(bo_plane, (void **) &dst_plane);
	if (ret) {
		perror("failed kms_bo_map()");
		goto err_plane_buffer_destroy;
	}

	ret = drmModeAddFB(fd, width, height, 24, 32, stride, handle, &fb_plane);
	if (ret) {
		perror("failed drmModeAddFB()");
		goto err_plane_buffer_unmap;
	}

	ret = drmModeSetPlane(fd, plane_id, crtc_id, fb_plane, 0,
		posx, posy, width, height, 0, 0, width << 16, height << 16);
	if (ret) {
		fprintf(stderr, "cannot set plane\n");
		goto err_plane_rm_fb;
	}

	/* foreground image: draw on the screen */

	clear_image((uint32_t *) dst_plane, width, height);
	getchar();

	draw_fancy_image((uint32_t *) dst_plane, width, height);
	getchar();

	clear_image((uint32_t *) dst_plane, width, height);
	getchar();

	drmModeSetPlane(fd, plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	getchar();

err_plane_rm_fb:
	drmModeRmFB(fd, fb_plane);

err_plane_buffer_unmap:
	ret = kms_bo_unmap(bo_plane);
	if (ret) {
		perror("failed kms_bo_unmap(new)");
	}

err_plane_buffer_destroy:
	ret = kms_bo_destroy(&bo_plane);
	if (ret) {
		perror("failed kms_bo_destroy(new)");
	}

err_crtc_exit:

	/* restore original crtc settings */

    if (saved_crtc->mode_valid) {
        ret = drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
                saved_crtc->x, saved_crtc->y, &conn_id, 1, &saved_crtc->mode);

        if (ret) {
            perror("failed drmModeSetCrtc(restore original)");
        }
    }

err_crtc_rm_fb:
	drmModeRmFB(fd, fb_crtc);

err_crtc_buffer_unmap:
	ret = kms_bo_unmap(bo_crtc);
	if (ret) {
		perror("failed kms_bo_unmap(new)");
	}

err_crtc_buffer_destroy:
	ret = kms_bo_destroy(&bo_crtc);
	if (ret) {
		perror("failed kms_bo_destroy(new)");
	}

err_driver_destroy:
	ret = kms_destroy(&drv);
	if (ret) {
		perror("failed kms_destroy(new)");
	}

err_close_drm:
	close(fd);

err_exit:
	return ret;
}
