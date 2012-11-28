#define _FILE_OFFSET_BITS 64

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

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "drm_utils.h"

/* */

struct dumb_rb {
	uint32_t fb;
	uint32_t handle;
	uint32_t stride;
	uint64_t size;
	void *map;

	uint32_t plane_id;
	uint32_t h;
	uint32_t w;
};

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

static void do_draw_1(struct dumb_rb *dbo)
{
	uint32_t color32[] = {
		0x000000FF, 0x0000FF00, 0x00FF0000,
		0x00FF00FF, 0x00FFFF00, 0x0000FFFF,
	};

	uint32_t *dst = dbo->map;
	uint32_t h = dbo->h;
	uint32_t w = dbo->w;
	uint32_t color;
	int i, j;

	printf("1: %dx%d\n", w, h);
	for(i = 0; i < h; i++ ){

		color = color32[6*i/h];

		for(j = 0; j < w; j++) {
			*(dst + i*w + j) = color;
		}
	}
}

static void do_draw_2(struct dumb_rb *dbo)
{
	uint32_t color32[] = {
		0x00000000, 0x000000FF, 0x0000FF00, 0x0000FFFF,
		0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00FFFFFF,
	};

	uint32_t *dst = dbo->map;
	uint32_t h = dbo->h;
	uint32_t w = dbo->w;
	uint32_t s = dbo->stride / 4;
	uint32_t color;
	int i, j;

	printf("2: %dx%d\n", w, h);

	for(j = 0; j < w; j++) {
		*(dst + j) = 0x00ffffff;
	}
	for(i = 1; i < h-1; i++ ){

		color = color32[8*i/h];

		*(dst + i * s) = 0x00ffffff;
		for(j = 1; j < w-1; j++) {
			*(dst + i*s + j) = color;
		}
		*(dst + i * s + j) = 0x00ffffff;
	}
	for(j = 0; j < w; j++) {
		*(dst + (h - 1) * s + j) = 0x00ffffff;
	}
}

static void do_draw_3(struct dumb_rb *dbo)
{
	uint32_t color32[] = {
		0x000000FF, 0x0000FF00, 0x00FF0000,
		0x00FF00FF, 0x00FFFF00, 0x0000FFFF,
	};

	uint32_t *dst = dbo->map;
	uint32_t h = dbo->h;
	uint32_t w = dbo->w;
	uint32_t color;
	int i, j;

	printf("3: %dx%d\n", w, h);
	for(i = 0; i < h; i++ ){

		color = color32[6*i/h];

		for(j = 0; j < w; j++) {
			*(dst + i*w + j) = color;
		}
		*(dst + i * w + (i % w)) = (~color) & 0xffffff;
	}
}

/* */

int main(int argc, char *argv[])
{
	struct kms_display kms_data;
	struct dumb_rb dbo;
	uint64_t has_dumb;
	int ret, fd;

	struct drm_mode_destroy_dumb dreq;
	struct drm_mode_create_dumb creq;
	struct drm_mode_map_dumb mreq;

	drmModePlaneRes *resources;
	drmModePlane *plane;

	/* */

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
			fprintf(stderr, "failed to autoconfigure KMS\n");
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

    /* check dumb buffer support */

	if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0) {
		perror("DRM_CAP_DUMB_BUFFER ioctl");
		ret = -EFAULT;
		goto err_close;
	}

	if (!has_dumb) {
		fprintf(stderr, "driver does not support dumb buffers\n");
		ret = -EFAULT;
		goto err_close;
	}

	memset(&dbo, 0, sizeof(dbo));
	memset(&mreq, 0, sizeof(mreq));
	memset(&creq, 0, sizeof(creq));
	memset(&dreq, 0, sizeof(dreq));

	/* get plane */

	resources = drmModeGetPlaneResources(fd);
	if (!resources || resources->count_planes == 0) {
		fprintf(stderr, "drmModeGetPlaneResources failed\n");
		ret = -ENODEV;
		goto err_close;
	}

	plane = drmModeGetPlane(fd, resources->planes[0]);

	if (!plane) {
		fprintf(stderr, "drmModeGetPlane failed\n");
		ret = -ENODEV;
		goto err_close;
	}

	dbo.plane_id = plane->plane_id;

	/* create dumb buffer object */

	dbo.w = 800;
	dbo.h = 480;

	creq.width = dbo.w;
	creq.height = dbo.h;
	creq.bpp = 32;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
	if (ret) {
		fprintf(stderr, "failed drmIoctl(DRM_IOCTL_MODE_CREATE_DUMB)\n");
		return ret;
	}

	dbo.handle = creq.handle;
	dbo.stride = creq.pitch;
	dbo.size = creq.size;

	/* create framebuffer for dumb buffer object */

	ret = drmModeAddFB(fd, dbo.w, dbo.h, 24, 32, dbo.stride, dbo.handle, &dbo.fb);
	if (ret) {
		fprintf(stderr, "cannot add drm framebuffer for dumb buffer object\n");
		return ret;
	}

	/* map dumb buffer object */

	mreq.handle = dbo.handle;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	if (ret) {
		fprintf(stderr, "failed drmIoctl(DRM_IOCTL_MODE_MAP_DUMB)\n");
		return ret;
	}

	dbo.map = mmap(0, dbo.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
	if (dbo.map == MAP_FAILED) {
		fprintf(stderr, "cannot mmap dumb buffer\n");
		return -EFAULT;
	}

	/* setup new plane */

	ret = drmModeSetPlane(fd, plane->plane_id, kms_data.crtc->crtc_id,
		dbo.fb, 0, 32, 32, dbo.w, dbo.h, 0, 0, dbo.w << 16, dbo.h << 16);
	if (ret) {
		fprintf(stderr, "cannot set plane\n");
		return ret;
	}

	/* draw on the screen */
	do_draw_1(&dbo);
	getchar();

	do_draw_2(&dbo);
	getchar();

	do_draw_3(&dbo);
	getchar();

err_unmap:
	drmModeSetPlane(fd, dbo.plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (dbo.map)
		munmap(dbo.map, dbo.size);

	drmModeRmFB(fd, dbo.fb);

	dreq.handle = dbo.handle;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	if (ret) {
		fprintf(stderr, "cannot destroy dumb buffer\n");
	}

err_close:
	close(fd);

	return ret;
}
