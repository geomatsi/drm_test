
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
drmModeModeInfo * drm_get_mode_by_name(int fd, uint32_t connector_id, char *mode_name);
