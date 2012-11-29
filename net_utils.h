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

#include <sys/socket.h>
#include <sys/un.h>

#include <xf86drmMode.h>
#include <xf86drm.h>

/* */

int drm_server_auth(drm_magic_t magic);
