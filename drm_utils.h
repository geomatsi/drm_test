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

struct kms_display {
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeModeInfo *mode;
	drmModeCrtc *crtc;
};

/* */

bool find_drm_configuration(int fd, struct kms_display *kms);
void dump_drm_configuration(struct kms_display *kms);
