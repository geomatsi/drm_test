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

bool find_drm_configuration(int fd, struct kms_display *kms)
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
