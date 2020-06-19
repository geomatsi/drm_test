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
#include <drm_fourcc.h>
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
	{ DRM_MODE_ENCODER_VIRTUAL, "VIRTUAL" },
	{ DRM_MODE_ENCODER_DSI, "DSI" },
	{ DRM_MODE_ENCODER_DPMST, "DPMST" },
	{ DRM_MODE_ENCODER_DPI, "DPI" },
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
	{ DRM_MODE_CONNECTOR_VIRTUAL, "virtual" },
	{ DRM_MODE_CONNECTOR_DSI, "DSI" },
	{ DRM_MODE_CONNECTOR_DPI, "DPI" },
	{ DRM_MODE_CONNECTOR_WRITEBACK, "writeback" },
};

type_name_fn(connector_type)

struct type_name format_names[] = {
	{ DRM_FORMAT_C8, "C8" },
	{ DRM_FORMAT_R8, "R8" },
	{ DRM_FORMAT_R16, "R16" },
	{ DRM_FORMAT_RG88, "RG88" },
	{ DRM_FORMAT_GR88, "GR88" },
	{ DRM_FORMAT_RG1616, "RG1616" },
	{ DRM_FORMAT_GR1616, "GR1616" },
	{ DRM_FORMAT_RGB332, "RGB332" },
	{ DRM_FORMAT_BGR233, "BGR233" },
	{ DRM_FORMAT_XRGB4444, "XRGB4444" },
	{ DRM_FORMAT_XBGR4444, "XBGR4444" },
	{ DRM_FORMAT_RGBX4444, "RGBX4444" },
	{ DRM_FORMAT_BGRX4444, "BGRX4444" },
	{ DRM_FORMAT_ARGB4444, "ARGB4444" },
	{ DRM_FORMAT_ABGR4444, "ABGR4444" },
	{ DRM_FORMAT_RGBA4444, "RGBA4444" },
	{ DRM_FORMAT_BGRA4444, "BGRA4444" },
	{ DRM_FORMAT_XRGB1555, "XRGB1555" },
	{ DRM_FORMAT_XBGR1555, "XBGR1555" },
	{ DRM_FORMAT_RGBX5551, "RGBX5551" },
	{ DRM_FORMAT_BGRX5551, "BGRX5551" },
	{ DRM_FORMAT_ARGB1555, "ARGB1555" },
	{ DRM_FORMAT_ABGR1555, "ABGR1555" },
	{ DRM_FORMAT_RGBA5551, "RGBA5551" },
	{ DRM_FORMAT_BGRA5551, "BGRA5551" },
	{ DRM_FORMAT_RGB565, "RGB565" },
	{ DRM_FORMAT_BGR565, "BGR565" },
	{ DRM_FORMAT_RGB888, "RGB888" },
	{ DRM_FORMAT_BGR888, "BGR888" },
	{ DRM_FORMAT_XRGB8888, "XRGB8888" },
	{ DRM_FORMAT_XBGR8888, "XBGR8888" },
	{ DRM_FORMAT_RGBX8888, "RGBX8888" },
	{ DRM_FORMAT_BGRX8888, "BGRX8888" },
	{ DRM_FORMAT_ARGB8888, "ARGB8888" },
	{ DRM_FORMAT_ABGR8888, "ABGR8888" },
	{ DRM_FORMAT_RGBA8888, "RGBA8888" },
	{ DRM_FORMAT_BGRA8888, "BGRA8888" },
	{ DRM_FORMAT_XRGB2101010, "XRGB2101010" },
	{ DRM_FORMAT_XBGR2101010, "XBGR2101010" },
	{ DRM_FORMAT_RGBX1010102, "RGBX1010102" },
	{ DRM_FORMAT_BGRX1010102, "BGRX1010102" },
	{ DRM_FORMAT_ARGB2101010, "ARGB2101010" },
	{ DRM_FORMAT_ABGR2101010, "ABGR2101010" },
	{ DRM_FORMAT_RGBA1010102, "RGBA1010102" },
	{ DRM_FORMAT_BGRA1010102, "BGRA1010102" },
	{ DRM_FORMAT_XRGB16161616F, "XRGB16161616F" },
	{ DRM_FORMAT_XBGR16161616F, "XBGR16161616F" },
	{ DRM_FORMAT_ARGB16161616F, "ARGB16161616F" },
	{ DRM_FORMAT_ABGR16161616F, "ABGR16161616F" },
	{ DRM_FORMAT_YUYV, "YUYV" },
	{ DRM_FORMAT_YVYU, "YVYU" },
	{ DRM_FORMAT_UYVY, "UYVY" },
	{ DRM_FORMAT_VYUY, "VYUY" },
	{ DRM_FORMAT_AYUV, "AYUV" },
	{ DRM_FORMAT_XYUV8888, "XYUV8888" },
	{ DRM_FORMAT_VUY888, "VUY888" },
	{ DRM_FORMAT_VUY101010, "VUY101010" },
	{ DRM_FORMAT_Y210, "Y210" },
	{ DRM_FORMAT_Y212, "Y212" },
	{ DRM_FORMAT_Y216, "Y216" },
	{ DRM_FORMAT_Y410, "Y410" },
	{ DRM_FORMAT_Y412, "Y412" },
	{ DRM_FORMAT_Y416, "Y416" },
	{ DRM_FORMAT_XVYU2101010, "XVYU2101010" },
	{ DRM_FORMAT_XVYU12_16161616, "XVYU12_16161616" },
	{ DRM_FORMAT_XVYU16161616, "XVYU16161616" },
	{ DRM_FORMAT_Y0L0, "Y0L0" },
	{ DRM_FORMAT_X0L0, "X0L0" },
	{ DRM_FORMAT_Y0L2, "Y0L2" },
	{ DRM_FORMAT_X0L2, "X0L2" },
	{ DRM_FORMAT_YUV420_8BIT, "YUV420_8BIT" },
	{ DRM_FORMAT_YUV420_10BIT, "YUV420_10BIT" },
	{ DRM_FORMAT_XRGB8888_A8, "XRGB8888_A8" },
	{ DRM_FORMAT_XBGR8888_A8, "XBGR8888_A8" },
	{ DRM_FORMAT_RGBX8888_A8, "RGBX8888_A8" },
	{ DRM_FORMAT_BGRX8888_A8, "BGRX8888_A8" },
	{ DRM_FORMAT_RGB888_A8, "RGB888_A8" },
	{ DRM_FORMAT_BGR888_A8, "BGR888_A8" },
	{ DRM_FORMAT_RGB565_A8, "RGB565_A8" },
	{ DRM_FORMAT_BGR565_A8, "BGR565_A8" },
	{ DRM_FORMAT_NV12, "NV12" },
	{ DRM_FORMAT_NV21, "NV21" },
	{ DRM_FORMAT_NV16, "NV16" },
	{ DRM_FORMAT_NV61, "NV61" },
	{ DRM_FORMAT_NV24, "NV24" },
	{ DRM_FORMAT_NV42, "NV42" },
	{ DRM_FORMAT_P210, "P210" },
	{ DRM_FORMAT_P010, "P010" },
	{ DRM_FORMAT_P012, "P012" },
	{ DRM_FORMAT_P016, "P016" },
	{ DRM_FORMAT_YUV410, "YUV410" },
	{ DRM_FORMAT_YVU410, "YVU410" },
	{ DRM_FORMAT_YUV411, "YUV411" },
	{ DRM_FORMAT_YVU411, "YVU411" },
	{ DRM_FORMAT_YUV420, "YUV420" },
	{ DRM_FORMAT_YVU420, "YVU420" },
	{ DRM_FORMAT_YUV422, "YUV422" },
	{ DRM_FORMAT_YVU422, "YVU422" },
	{ DRM_FORMAT_YUV444, "YUV444" },
	{ DRM_FORMAT_YVU444, "YVU444" },
};

type_name_fn(format)

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

void crtc_info(drmModeRes *resources, drmModeCrtc *crtc)
{
	/* From xf86drmMode.h:

	typedef struct _drmModeCrtc {
		uint32_t crtc_id;
		uint32_t buffer_id; // FB id to connect to 0 = disconnect

		uint32_t x, y; // Position on the framebuffer
		uint32_t width, height;
		int mode_valid;
		drmModeModeInfo mode;

		int gamma_size; // Number of gamma stops

	} drmModeCrtc, *drmModeCrtcPtr;

	*/

	printf("\ncrtc [id = %u]\n", crtc->crtc_id);
	printf("\tbuffer [id = %u]\n", crtc->buffer_id);
	printf("\tposition: %xx%x @ %xx%x\n", crtc->width, crtc->height, crtc->x, crtc->y);
	if (crtc->mode_valid)
		printf("\tMode: valid [%s]\n", crtc->mode.name);
	else
		printf("\tMode: invalid\n");
}

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

void encoder_info(drmModeRes *resources, drmModeEncoder *encoder, drmModeCrtc **crtcs)
{
	int i;

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
	printf("\tCrtc [id = %u]\n", encoder->crtc_id);

	printf("\tSupported crtc:");
	for (i = 0; i < 31; i++)
		if (encoder->possible_crtcs & (1 << i)) {
			if (crtcs[i] != NULL)
				printf(" [id = %d]", crtcs[i]->crtc_id);
			else
				printf(" [#%d, ??]",  i);
		}
	printf("\n");

	return;
}

void plane_info(drmModePlane *plane, drmModeCrtc **crtcs)
{
	int i;

	/* From xf86drmMode.h:

	typedef struct _drmModePlane {
		uint32_t count_formats;
		uint32_t *formats;
		uint32_t plane_id;

		uint32_t crtc_id;
		uint32_t fb_id;

		uint32_t crtc_x, crtc_y;
		uint32_t x, y;

		uint32_t possible_crtcs;
		uint32_t gamma_size;
	} drmModePlane, *drmModePlanePtr;
	*/
	printf("\nPlane [id = %u]\n", plane->plane_id);
	printf("\tFB ID [id = %u]\n", plane->fb_id);
	printf("\tcrtc ID [id = %u]\n", plane->crtc_id);

	for (i = 0; i < plane->count_formats; i++)
		printf("%s '%s'", i == 0 ? "\tFormats:" : ",", format_str(plane->formats[i]));
	printf("\n");

	printf("\tCRTC XxY %ux%x\n", plane->crtc_x, plane->crtc_y);
	printf("\tXxY %ux%x\n", plane->x, plane->y);
	printf("\tSupported crtc:");
	for (i = 0; i < 31; i++)
		if (plane->possible_crtcs & (1 << i)) {
			if (crtcs[i] != NULL)
				printf(" [id = %d]", crtcs[i]->crtc_id);
			else
				printf(" [#%d, ??]",  i);
		}
	printf("\n");
}

void planes_info(int fd, drmModeCrtc **crtcs)
{
	drmModePlaneRes *plane_resources;
	int i;

	plane_resources = drmModeGetPlaneResources(fd);
	if (!plane_resources) {
		fprintf(stderr, "Error getting plane resources\n");
		return;
	}

	for (i = 0; i < plane_resources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(fd, plane_resources->planes[i]);
		if (!plane)
			continue;

		plane_info(plane, crtcs);

		drmModeFreePlane(plane);
	}
}

int main(int argc, char *argv[])
{
    drmModeCrtcPtr *crtcs;
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


    crtcs = calloc(32, sizeof(drmModeCrtcPtr));
    for (i = 0; i < resources->count_crtcs; i++) {
        crtcs[i] = drmModeGetCrtc(fd, resources->crtcs[i]);
        if (crtcs[i] == NULL)
            continue;

		crtc_info(resources, crtcs[i]);
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

		encoder_info(resources, encoder, crtcs);
        drmModeFreeEncoder(encoder);
    }

	planes_info(fd, crtcs);

	for (i = 0; i < resources->count_crtcs; i++) {
		if (crtcs[i] != NULL)
			drmModeFreeCrtc(crtcs[i]);
	}
	free(crtcs);

close_fd:
    close(fd);

exit:
    return ret;
}
