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

/* */

struct kms {
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeModeInfo mode;
};

struct dumb_rb {
	uint32_t fb;
	uint32_t handle;
	uint32_t stride;
	uint64_t size;
	void *map;
};

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

bool find_drm_configuration(int fd, struct kms *kms)
{
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeRes *resources;

	int i;

	resources = drmModeGetResources(fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return false;
	}

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(fd, resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0)
			break;

		drmModeFreeConnector(connector);
	}

	if (i == resources->count_connectors) {
		fprintf(stderr, "No currently active connector found\n");
		return false;
	}

	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(fd, resources->encoders[i]);

		if (encoder == NULL)
			continue;

		if (encoder->encoder_id == connector->encoder_id)
			break;

		drmModeFreeEncoder(encoder);
	}

	if (i == resources->count_encoders) {
        fprintf(stderr, "No matching encoder for connector found\n");
		goto drm_free_connector;
    }


	kms->connector = connector;
	kms->encoder = encoder;
	kms->mode = connector->modes[0];

	return true;

drm_free_connector:
	drmModeFreeConnector(connector);

	return false;
}

int main(char argc, char *argv[])
{
	struct kms kms_data;
	struct dumb_rb dbo;
	uint64_t has_dumb;
	int ret, fd;

    struct drm_mode_destroy_dumb dreq;
	struct drm_mode_create_dumb creq;
	struct drm_mode_map_dumb mreq;
    drmModeCrtcPtr saved_crtc;


	memset(&dbo, 0, sizeof(dbo));
	memset(&mreq, 0, sizeof(mreq));
	memset(&creq, 0, sizeof(creq));
	memset(&dreq, 0, sizeof(dreq));

	fd = open(device_name, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("cannot open drm device");
		exit(-1);
	}

	drmDropMaster(fd);

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

	/* find current DRM configuration */

	if (!find_drm_configuration(fd, &kms_data)) {
		fprintf(stderr, "failed to setup KMS\n");
		ret = -EFAULT;
		goto err_close;
	}

	/* create dumb buffer object */

	creq.height = kms_data.mode.vdisplay;
	creq.width = kms_data.mode.hdisplay;
	creq.bpp = 32;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
	if (ret) {
		fprintf(stderr, "failed drmIoctl(DRM_IOCTL_MODE_CREATE_DUMB)\n");
		goto err_close;
	}

	dbo.handle = creq.handle;
	dbo.stride = creq.pitch;
	dbo.size = creq.size;

	/* create framebuffer for dumb buffer object */

	ret = drmModeAddFB(fd, kms_data.mode.hdisplay, kms_data.mode.vdisplay,
		24, 32, dbo.stride, dbo.handle, &dbo.fb);
	if (ret) {
		fprintf(stderr, "cannot add drm framebuffer for dumb buffer object\n");
		goto err_destroy;
	}

	/* map dumb buffer object */

	mreq.handle = dbo.handle;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	if (ret) {
		fprintf(stderr, "failed drmIoctl(DRM_IOCTL_MODE_MAP_DUMB)\n");
		goto err_fb;
	}

	dbo.map = mmap(0, dbo.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
	if (dbo.map == MAP_FAILED) {
		fprintf(stderr, "cannot mmap dumb buffer\n");
		ret = -EFAULT;
		goto err_fb;
	}

	/* store current crtc */

    saved_crtc = drmModeGetCrtc(fd, kms_data.encoder->crtc_id);
    if (saved_crtc == NULL) {
        fprintf(stderr, "failed to get current mode\n");
        goto err_unmap;
    }

	/* setup new crtc */

    ret = drmModeSetCrtc(fd, kms_data.encoder->crtc_id, dbo.fb, 0, 0,
            &kms_data.connector->connector_id, 1, &kms_data.mode);
	if (ret) {
		fprintf(stderr, "cannot set new drm crtc");
		goto err_unmap;
	}

	/* draw on the screen */

	do {

		uint32_t color32[] = {
			0x000000FF, 0x0000FF00, 0x00FF0000,
			0x00FF00FF, 0x00FFFF00, 0x0000FFFF,
		};

		uint32_t *dst = (uint32_t *) dbo.map;
		uint32_t h = kms_data.mode.vdisplay;
		uint32_t w = kms_data.mode.hdisplay;
		uint32_t color;
		int i, j;

		for(i = 0; i < h; i++ ){

			color = color32[6*i/h];

			for(j = 0; j < w; j++) {
				*(dst + i*w + j) = color;
			}
		}

		getchar();

	} while(0);

	/* restore old crtc */

    ret = drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
            saved_crtc->x, saved_crtc->y, &kms_data.connector->connector_id, 1, &saved_crtc->mode);


err_unmap:
    munmap(dbo.map, dbo.size);

err_fb:
    drmModeRmFB(fd, dbo.fb);

err_destroy:
	dreq.handle = dbo.handle;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	if (ret) {
		fprintf(stderr, "cannot destroy dumb buffer");
	}

err_close:
	close(fd);

	return ret;
}
