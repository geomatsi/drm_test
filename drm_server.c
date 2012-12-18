#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <xf86drmMode.h>
#include <xf86drm.h>
#include <libkms.h>

#include "drm_utils.h"
#include "drm_proto.h"

/* */

static const char device_name[] = "/dev/dri/card0";
static bool running  = true;

/* */

void int_handler(int);

/* */

int main(char argc, char *argv[])
{
    struct sockaddr_un  cli_addr, serv_addr;
    int sockfd, clientfd, servlen, msglen;
    socklen_t clilen;

    char buf[DRM_SRV_MSGLEN + 1];
    struct sigaction act;
	uint32_t command, magic;
	uint32_t *pmsg;
    int ret, fd;

	struct drm_client_info *pclient;

    /* setup signal handler */

    act.sa_handler = int_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT,&act,NULL);

    /* open drm device */

    fd = open(device_name, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("cannot open drm device");
        exit(-1);
    }

    drmSetMaster(fd);

    /* open unix socket and listen for clients */

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("could not create socket");
        goto err_close;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, DRM_SERVER_NAME);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
    clilen = sizeof(cli_addr);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, servlen) < 0) {
        perror("could not bind socket");
        goto err_close;
    }

    listen(sockfd,5);

    do {

        fprintf(stdout, "listen for incoming connections...\n");

        clientfd = accept(sockfd,(struct sockaddr *)&cli_addr, &clilen);
        if (clientfd < 0) {
            perror("could not accept connection");
            break;
        }

        bzero(buf, sizeof(buf));
        msglen = read(clientfd, buf, sizeof(buf));
		sscanf(buf, "%d:%d", &magic, &command);

        fprintf(stdout, "accepted request [%s]\n", buf);
		fprintf(stdout, "magic = %d, command = %d\n", magic, command);

		/* handle request */

		switch (command) {
			case CMD_AUTH:	/* authenticate client */
				do {

					ret = drmAuthMagic(fd, magic);

					if (ret) {
						perror("failed drmAuthMagic");
					} else {
						pclient = calloc(sizeof(*pclient), 1);
						pclient->magic = magic;
					}

					bzero(buf, sizeof(buf));
					snprintf(buf, sizeof(buf), "%d:%d", magic, ret ? DRM_ERROR: DRM_OK);
				} while (0);

				break;

			case CMD_CRTC:	/* save old crtc and create new crtc */
				do {

					pclient->crtc_id = pmsg[2];
					pclient->conn_id = pmsg[3];
					pclient->fb = pmsg[4];
					pclient->mode_name = strdup((char *) pmsg[5]);
					pclient->mode = drm_get_mode_by_name(fd, pclient->conn_id, pclient->mode_name);

					if (!pclient->mode) {
						perror("failed drm_get_mode_by_name");
						bzero(buf, sizeof(buf));
						snprintf(buf, sizeof(buf), "%d:%d", magic, DRM_ERROR);
						break;
					}

					/* store current crtc */

					pclient->saved_crtc = drmModeGetCrtc(fd, pclient->crtc_id);
					if (pclient->saved_crtc == NULL) {
						perror("failed drmModeGetCrtc(current)");
						bzero(buf, sizeof(buf));
						snprintf(buf, sizeof(buf), "%d:%d", magic, DRM_ERROR);
						break;
					}

					dump_crtc_configuration("saved_crtc", pclient->saved_crtc);

					/* setup new crtc */

					ret = drmModeSetCrtc(fd, pclient->crtc_id, pclient->fb, 0, 0,
							&pclient->conn_id, 1, pclient->mode);

					if (ret) {
						perror("failed drmModeSetCrtc(new)");
						bzero(buf, sizeof(buf));
						snprintf(buf, sizeof(buf), "%d:%d", magic, DRM_ERROR);
						break;
					}

					pclient->current_crtc = drmModeGetCrtc(fd, pclient->crtc_id);

					if (pclient->current_crtc != NULL) {
						dump_crtc_configuration("current_crtc", pclient->current_crtc);
					} else {
						perror("failed drmModeGetCrtc(new)");
						bzero(buf, sizeof(buf));
						snprintf(buf, sizeof(buf), "%d:%d", magic, DRM_ERROR);
						break;
					}

					bzero(buf, sizeof(buf));
					snprintf(buf, sizeof(buf), "%d:%d", magic, DRM_OK);
				} while (0);

				break;

			case CMD_QUIT:	/* client disconnects */
				do {

					if (pclient->saved_crtc->mode_valid) {
						ret = drmModeSetCrtc(fd, pclient->saved_crtc->crtc_id,
							pclient->saved_crtc->buffer_id, pclient->saved_crtc->x,
							pclient->saved_crtc->y, &pclient->conn_id,
							1, &(pclient->saved_crtc)->mode);

						if (ret) {
							perror("failed drmModeSetCrtc(restore original)");
							bzero(buf, sizeof(buf));
							snprintf(buf, sizeof(buf), "%d:%d", magic, DRM_ERROR);
							break;
						}
					}

					bzero(buf, sizeof(buf));
					snprintf(buf, sizeof(buf), "%d:%d", magic, DRM_OK);
				} while (0);

				break;

			default:
				bzero(buf, sizeof(buf));
				snprintf(buf, sizeof(buf), "%d:%d", magic, DRM_ERROR);
		}

        fprintf(stdout, "send response [%s]\n", buf);
        write(clientfd, buf, sizeof(buf));
        close(clientfd);

    } while(running);

    close(sockfd);
    unlink(DRM_SERVER_NAME);

err_close:
    close(fd);

    return ret;
}

void int_handler(int signo)
{
    running  = false;
}
