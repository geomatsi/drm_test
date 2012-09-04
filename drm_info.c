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
#include <fcntl.h>

#include <gbm.h>
#include <drm.h>
#include <xf86drmMode.h>

static const char device_name[] = "/dev/dri/card0";

int main(int argc, char *argv[])
{
    drmModeConnector *connector;
    drmModeEncoder *encoder;
	drmModeModeInfo *mode;
    drmModeRes *resources;

    struct gbm_device *gbm;
    int i, fd, ret;

    fd = open(device_name, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "couldn't open %s, skipping\n", device_name);
		ret = -1;
		goto exit;
    }

    gbm = gbm_create_device(fd);
    if (gbm == NULL) {
        fprintf(stderr, "couldn't create gbm device\n");
        ret = -1;
        goto close_fd;
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
		goto destroy_gbm_device;
    }

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


    for (i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector == NULL)
            continue;

        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0)
            break;

        drmModeFreeConnector(connector);
    }

    if (i == resources->count_connectors) {
        fprintf(stderr, "No currently active connector found.\n");
		ret = -1;
		goto destroy_gbm_device;
    }

	/* From xf86drmMode.h:

	typedef struct _drmModeEncoder {
		uint32_t encoder_id;
		uint32_t encoder_type;
		uint32_t crtc_id;
		uint32_t possible_crtcs;
		uint32_t possible_clones;
	} drmModeEncoder, *drmModeEncoderPtr;

	*/

    for (i = 0; i < resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, resources->encoders[i]);

        if (encoder == NULL)
            continue;

        if (encoder->encoder_id == connector->encoder_id)
            break;

        drmModeFreeEncoder(encoder);
    }

    if (i == resources->count_encoders) {
        fprintf(stderr, "No matching encoder for connector found.\n");
		ret = -1;
		goto drm_free_connector;
    }

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
		printf("mode: %s\n", mode->name);
    }

	drmModeFreeEncoder(encoder);

drm_free_connector:
	drmModeFreeConnector(connector);

destroy_gbm_device:
    gbm_device_destroy(gbm);

close_fd:
    close(fd);

exit:
    return ret;
}
