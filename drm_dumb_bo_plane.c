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
#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "bitmap_utils.h"
#include "drm_utils.h"

/* */

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

int main(int argc, char *argv[])
{
	uint64_t has_dumb;
	int ret, fd, opt, i;
	void *map;

	struct drm_mode_destroy_dumb dreq;
	struct drm_mode_create_dumb creq;
	struct drm_mode_map_dumb mreq;

	drmModePlaneRes *resources;
	drmModePlane *plane = NULL;

	uint32_t handle, stride, size;
	uint32_t plane_id, crtc_id;
	uint32_t width, height;
	uint32_t posx, posy;
	uint32_t fb;

	/* parse command line */

	while ((opt = getopt(argc, argv, "x:y:w:v:c:p:h")) != -1) {
		switch (opt) {
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
				printf("\t-c <crtc>			crtc id, default is 0\n");
				printf("\t-p <plane>		plane id, default is 0\n");
				printf("\t-x <posx>			plane top left corner xpos, default is 0'\n");
				printf("\t-y <posy>			plane top left corner ypos, default is 0'\n");
				printf("\t-w <width>		plane width, default is 0'\n");
				printf("\t-v <height>		plane height, default is 0'\n");
				exit(0);
		}
	}

	/* open drm device */

	fd = open(device_name, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("cannot open drm device");
		exit(-1);
	}

	drmSetMaster(fd);

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

	/* get plane */

	resources = drmModeGetPlaneResources(fd);
	if (!resources || resources->count_planes == 0) {
		fprintf(stderr, "drmModeGetPlaneResources failed\n");
		ret = -ENODEV;
		goto err_close;
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
		goto err_close;
	}

	/* create dumb buffer object */

	memset(&creq, 0, sizeof(creq));

	creq.height = height;
	creq.width = width;
	creq.bpp = 32;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
	if (ret) {
		fprintf(stderr, "failed drmIoctl(DRM_IOCTL_MODE_CREATE_DUMB)\n");
		goto err_close;
	}

	handle = creq.handle;
	stride = creq.pitch;
	size = creq.size;

	/* create framebuffer for dumb buffer object */

	ret = drmModeAddFB(fd, width, height, 24, 32, stride, handle, &fb);
	if (ret) {
		fprintf(stderr, "cannot add drm framebuffer for dumb buffer object\n");
		goto err_destroy_dumb;
	}

	/* map dumb buffer object */

	memset(&mreq, 0, sizeof(mreq));

	mreq.handle = handle;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	if (ret) {
		fprintf(stderr, "failed drmIoctl(DRM_IOCTL_MODE_MAP_DUMB)\n");
		goto err_destroy_fb;
	}

	map = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
	if (map == MAP_FAILED) {
		fprintf(stderr, "cannot mmap dumb buffer\n");
		goto err_destroy_fb;
	}

	/* setup new plane */

	ret = drmModeSetPlane(fd, plane_id, crtc_id, fb, 0, posx, posy,
		width, height, 0, 0, width << 16, height << 16);
	if (ret) {
		fprintf(stderr, "cannot set plane\n");
		goto err_unmap;
	}

	/* draw on the screen */
	draw_test_image((uint32_t *) map, width, height);
	getchar();

	draw_fancy_image((uint32_t *) map, width, height);
	getchar();

	drmModeSetPlane(fd, plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

err_unmap:
	if (map)
		munmap(map, size);

err_destroy_fb:
	drmModeRmFB(fd, fb);

err_destroy_dumb:
	memset(&dreq, 0, sizeof(dreq));

	dreq.handle = handle;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	if (ret) {
		fprintf(stderr, "cannot destroy dumb buffer\n");
	}

err_close:
	close(fd);

	return ret;
}
