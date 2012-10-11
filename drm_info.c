/*
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Benjamin Franzke
 *
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

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>

#include <drm.h>
#include <xf86drmMode.h>

/* */

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct type_name {
	int type;
	char *name;
};

#define type_name_fn(res) \
char * res##_str(int type) {			\
	unsigned int i;					\
	for (i = 0; i < ARRAY_SIZE(res##_names); i++) { \
		if (res##_names[i].type == type)	\
			return res##_names[i].name;	\
	}						\
	return "(invalid)";				\
}

struct type_name encoder_type_names[] = {
	{ DRM_MODE_ENCODER_NONE, "none" },
	{ DRM_MODE_ENCODER_DAC, "DAC" },
	{ DRM_MODE_ENCODER_TMDS, "TMDS" },
	{ DRM_MODE_ENCODER_LVDS, "LVDS" },
	{ DRM_MODE_ENCODER_TVDAC, "TVDAC" },
};

type_name_fn(encoder_type)

struct type_name connector_status_names[] = {
	{ DRM_MODE_CONNECTED, "connected" },
	{ DRM_MODE_DISCONNECTED, "disconnected" },
	{ DRM_MODE_UNKNOWNCONNECTION, "unknown" },
};

type_name_fn(connector_status)

struct type_name connector_type_names[] = {
	{ DRM_MODE_CONNECTOR_Unknown, "unknown" },
	{ DRM_MODE_CONNECTOR_VGA, "VGA" },
	{ DRM_MODE_CONNECTOR_DVII, "DVI-I" },
	{ DRM_MODE_CONNECTOR_DVID, "DVI-D" },
	{ DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
	{ DRM_MODE_CONNECTOR_Composite, "composite" },
	{ DRM_MODE_CONNECTOR_SVIDEO, "s-video" },
	{ DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ DRM_MODE_CONNECTOR_Component, "component" },
	{ DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN" },
	{ DRM_MODE_CONNECTOR_DisplayPort, "displayport" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "embedded displayport" },
};

type_name_fn(connector_type)

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

void connector_info(drmModeRes *resources, drmModeConnector *connector)
{
	drmModeModeInfo *mode;

	uint32_t *enc;
	int i;

	/* From xf86drmMode.h:

	typedef struct _drmModeConnector {
		uint32_t connector_id;
		uint32_t encoder_id;			// Encoder currently connected
		uint32_t connector_type;
		uint32_t connector_type_id;
		drmModeConnection connection;
		uint32_t mmWidth, mmHeight;		// HxW in millimeters
		drmModeSubPixel subpixel;

		int count_modes;
		drmModeModeInfoPtr modes;

		int count_props;
		uint32_t *props;				// List of property ids
		uint64_t *prop_values;			// List of property values

		int count_encoders;
		uint32_t *encoders;				// List of encoder ids
	} drmModeConnector, *drmModeConnectorPtr;

	*/

	printf("\nConnector [id = %u]\n", connector->connector_id);
	printf("\ttype [%s]\n", connector_type_str(connector->connector_type));
	printf("\tstatus [%s]\n", connector_status_str(connector->connection));

	for(i = 0, enc = connector->encoders; i < connector->count_encoders; i++, enc++) {
		printf("%s[%u]%s", i ? " " : "\tsupported encoders: ", *enc, i == connector->count_encoders - 1 ? "\n" : " ");
	}

	printf("\tcurrent encoder: %u\n", connector->encoder_id);

	/* From xf86drmMode.h:

	typedef struct _drmModeModeInfo {
		uint32_t clock;
		uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
		uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

		uint32_t vrefresh;

		uint32_t flags;
		uint32_t type;
		char name[DRM_DISPLAY_MODE_LEN];
	} drmModeModeInfo, *drmModeModeInfoPtr;

	*/

	for (i = 0, mode = connector->modes; i < connector->count_modes; i++, mode++) {
		printf("%s[%s]%s", i ? " " : "\tmodes: ", mode->name, i == connector->count_modes - 1 ? "\n" : " ");
    }

	return;
}

void encoder_info(drmModeRes *resources, drmModeEncoder *encoder)
{

	/* From xf86drmMode.h:

	typedef struct _drmModeEncoder {
		uint32_t encoder_id;
		uint32_t encoder_type;
		uint32_t crtc_id;
		uint32_t possible_crtcs;
		uint32_t possible_clones;
	} drmModeEncoder, *drmModeEncoderPtr;

	*/

	printf("\nEncoder [id = %u]\n", encoder->encoder_id);
	printf("\ttype [%s]\n", encoder_type_str(encoder->encoder_type));

	return;
}

int main(int argc, char *argv[])
{
    drmModeConnector *connector;
	drmModeEncoder *encoder;
    drmModeRes *resources;

    int i, fd, ret;

    fd = open(device_name, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "couldn't open %s, skipping\n", device_name);
		ret = -1;
		goto exit;
    }

	/* From xf86drmMode.h:

	typedef struct _drmModeRes {

		int count_fbs;
		uint32_t *fbs;

		int count_crtcs;
		uint32_t *crtcs;

		int count_connectors;
		uint32_t *connectors;

		int count_encoders;
		uint32_t *encoders;

		uint32_t min_width, max_width;
		uint32_t min_height, max_height;
	} drmModeRes, *drmModeResPtr;

	*/

    resources = drmModeGetResources(fd);
    if (!resources) {
        fprintf(stderr, "drmModeGetResources failed\n");
		ret = -1;
		goto close_fd;
    }


    for (i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector == NULL)
            continue;

		connector_info(resources, connector);
        drmModeFreeConnector(connector);
    }

	for (i = 0; i < resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, resources->encoders[i]);

        if (encoder == NULL)
            continue;

		encoder_info(resources, encoder);
        drmModeFreeEncoder(encoder);
    }


close_fd:
    close(fd);

exit:
    return ret;
}
