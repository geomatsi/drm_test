
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

#define PREFERRED_MODE	"preferred"

/* */

#define DRM_SERVER_NAME	"/tmp/drm_srv"

#define DRM_SRV_MSGLEN  20
#define DRM_AUTH_OK     "AUTH OK"
#define DRM_AUTH_FAIL   "AUTH FAIL"

/* */

struct kms_display {
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeModeInfo *mode;
	drmModeCrtc *crtc;
};

/* */

bool drm_get_conf_cmdline(int fd, struct kms_display *kms, int argc, char *argv[]);
bool drm_autoconf(int fd, struct kms_display *kms);
void dump_drm_configuration(struct kms_display *kms);
void dump_crtc_configuration(char *msg, drmModeCrtc *crtc);
