#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/un.h>

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

#define DRM_SERVER_NAME	"/tmp/drm_srv"

#define DRM_SRV_MSGLEN  1024

/* drm proto description */

/* request:

	CMD_AUTH
		| magic:uint32_t | command:uint32_t |

	CMD_CRTC
		| magic:uint32_t | command:uint32_t | crtc_id:uint32_t | connector_id:uint32_t | fb:uint32_t | mode |

*/

/* response:

	| magic:uint32_t | value:uint32_t |

*/

/* client requests */

enum {
	CMD_AUTH,
	CMD_CRTC,
	CMD_QUIT,
};

/* server responses */

enum {
	DRM_OK,
	DRM_ERROR,
};

/* */

struct drm_client_info {
	drm_magic_t magic;
    drmModeCrtcPtr saved_crtc;
    drmModeCrtcPtr current_crtc;
	uint32_t crtc_id;
	uint32_t conn_id;
	uint32_t fb;
	char *mode_name;
	drmModeModeInfo *mode;
};

/* */

int drm_to_server(char *tx_buf);
