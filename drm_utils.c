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

bool drm_autoconf(int fd, struct kms_display *kms)
{
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeCrtc *crtc;
	drmModeCrtc *c;

    drmModeRes *resources;
	drmModeModeInfo *mode;

	int i;

	resources = drmModeGetResources(fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return false;
	}

    /* find connected connector */

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(fd, resources->connectors[i]);
		if (!connector)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0)
			break;

		drmModeFreeConnector(connector);
	}

	if (i == resources->count_connectors) {
		fprintf(stderr, "No currently active connector found\n");
		return false;
	}

    /* find appropriate encoder */

	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(fd, resources->encoders[i]);

		if (encoder)
			continue;

		if (encoder->encoder_id == connector->encoder_id)
			break;

		drmModeFreeEncoder(encoder);
	}

	if (i == resources->count_encoders) {
        fprintf(stderr, "No matching encoder for connector, use the first supported encoder\n");
        encoder = drmModeGetEncoder(fd, connector->encoders[0]);

        if (!encoder) {
            fprintf(stderr, "Can't get preferred encoders\n");
            goto drm_free_connector;
        }
    }

    /* select preferred mode */

    for (i = 0, mode = connector->modes; i < connector->count_modes; i++, mode++) {
        if (0 == strcmp(mode->name, "preferred"))
            break;
    }

	if (i == connector->count_modes) {
        fprintf(stderr, "No preferred mode, choose the first available mode\n");
        mode = connector->modes;
    }

    /* find crtc */

    c = NULL;

    for (i = 0; i < resources->count_crtcs; i++) {
        crtc = drmModeGetCrtc(fd, resources->crtcs[i]);

        if (!crtc)
            continue;

        if (crtc->crtc_id == encoder->crtc_id)
            break;

        if (!c && (encoder->possible_crtcs & (1 << i)))
            c = crtc;
    }

	if (i == resources->count_crtcs) {
        fprintf(stderr, "No crtc for encoder, choose the first possible crtc\n");
        crtc = c;
    }

    /* */

	kms->connector = connector;
	kms->encoder = encoder;
    kms->crtc = crtc;
	kms->mode = mode;

	return true;

drm_free_connector:
	drmModeFreeConnector(connector);

	return false;
}

void dump_drm_configuration(struct kms_display *kms)
{
    printf("setting mode \"%s\" on connector %d, encoder %d, crtc %d\n",
            kms->mode->name, kms->connector->connector_id, kms->encoder->encoder_id, kms->crtc->crtc_id);
}
