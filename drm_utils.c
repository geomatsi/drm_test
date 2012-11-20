#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <getopt.h>
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

static void drm_cmdline_usage(char *name)
{
	printf("usage: %s [-h] -c <connector> -e <encoder> -m <mode>\n", name);
	printf("\t-h: this help message\n");
	printf("\t-c <connector>	connector id, default is 0\n");
	printf("\t-e <encoder>		encoder id, default is 0\n");
	printf("\t-m <mode>			mode name, default is 'preferred'\n");
}

bool drm_get_conf_cmdline(int fd, struct kms_display *kms, int argc, char *argv[])
{
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeCrtc *crtc;
	drmModeCrtc *c;

    drmModeRes *resources;
	drmModeModeInfo *mode;

	int opt, i;

	char *mode_name = PREFERRED_MODE;
	int cid = 0;
	int eid = 0;

	while ((opt = getopt(argc, argv, "c:e:m:h")) != -1) {
		switch (opt) {
			case 'm':
				mode_name = strdup(optarg);
				break;
			case 'c':
				cid = atoi(optarg);
				break;
			case 'e':
				eid = atoi(optarg);
				break;
			case 'h':
			default:
				drm_cmdline_usage(argv[0]);
				exit(0);
		}
	}

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

		if (cid == connector->connector_id)
			break;

		drmModeFreeConnector(connector);
	}

	if (i == resources->count_connectors) {
		fprintf(stderr, "Connector with id = %d does not exist\n", cid);
		return false;
	}

    /* find appropriate encoder */

	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(fd, resources->encoders[i]);
		if (!encoder)
			continue;

		if (eid == encoder->encoder_id)
			break;

		drmModeFreeEncoder(encoder);
	}

	if (i == resources->count_encoders) {
		fprintf(stderr, "Encoder with id = %d does not exist\n", eid);
		return false;
    }

    /* select preferred mode */

    for (i = 0, mode = connector->modes; i < connector->count_modes; i++, mode++) {
        if (0 == strcmp(mode->name, mode_name))
            break;
    }

	if (i == connector->count_modes) {
		fprintf(stderr, "Mode with name %s does not exist\n", mode_name);
		return false;
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
	drm_cmdline_usage(argv[0]);

	return false;
}


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

		if (!encoder)
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

void dump_crtc_configuration(char *msg, drmModeCrtc *crtc)
{
    printf("%s: id\tfb\tpos\tsize\n", msg);
    printf("%d\t%d\t(%d,%d)\t(%dx%d) mode[%s]\n", crtc->crtc_id, crtc->buffer_id,
            crtc->x, crtc->y, crtc->width, crtc->height, crtc->mode.name);
}
