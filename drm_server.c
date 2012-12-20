#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/poll.h>
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
	int sockfd, clientfd, servlen;
	socklen_t clilen;

	int ret, fd, max, rc, i;
	struct sigaction act;

	struct drm_client_info drm_clients[MAXCLIENTS];
	struct pollfd clients[MAXCLIENTS + 1];			// server socket will be in the last cell

	char tx_buf[DRM_SRV_MSGLEN + 1];
	char rx_buf[DRM_SRV_MSGLEN + 1];
	uint32_t command, magic;
	uint32_t *pmsg;

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

	listen(sockfd, 5);

	/* prepare data structures for clients */

	clients[MAXCLIENTS].fd = sockfd;
	clients[MAXCLIENTS].events = POLLIN;

	for(i = 0; i < MAXCLIENTS; i++) {
		clients[i].fd = -1;
	}

	/* go */

	do {

		rc = poll(clients, MAXCLIENTS + 2, 10000);

		if(rc == 0){
			fprintf(stdout, "poll timeout\n");
			continue;
		}

		if (rc == -1) {
			if (errno == EINTR) {
				perror("'poll' was interrupted");
				break;
			} else {
				perror("some problems with poll");
				break;
			}
		}

		/* new connection */

		if (clients[MAXCLIENTS].revents & POLLIN) {

			clientfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
			if (clientfd < 0) {
				perror("could not accept connection");
				break;
			}

			for (i = 0; i < MAXCLIENTS; i++) {

				if (clients[i].fd == -1) {
					clients[i].fd = clientfd;
					clients[i].events = POLLIN;
					break;
				}
			}

			if (i == MAXCLIENTS) {
				fprintf(stdout, "clients queue is full - drop client\n");
				close(clientfd);
			}

			if (--rc == 0) continue;
		}

		/* message from client */

		for (i = 0; i < MAXCLIENTS; i++) {

			if (clients[i].fd == -1)
				continue;

			if (!(clients[i].revents & (POLLIN | POLLERR)))
				continue;

			bzero(rx_buf, sizeof(rx_buf));
			ret = read(clients[i].fd, rx_buf, sizeof(rx_buf));

			if (ret < 0) {
				perror("problem with socket");
				close(clients[i].fd);
				clients[i].fd = -1;
				continue;
			}

			if (ret == 0) {
				fprintf(stdout, "client %d gone,closing connection\n", i);
				close(clients[i].fd);
				clients[i].fd = -1;
				continue;
			}

			/* handle drm client */

			pmsg = (uint32_t *) rx_buf;
			sscanf(rx_buf, "%d:%d", &magic, &command);
			fprintf(stdout, "accepted request [%s] -> (%d, %d)\n", rx_buf, magic, command);

			switch (command) {
				case CMD_AUTH:	/* authenticate client */
					do {
						ret = drmAuthMagic(fd, magic);

						if (ret) {
							perror("failed drmAuthMagic");
							ret = -1;
							break;
						} else {
							bzero(&drm_clients[i], sizeof(struct drm_client_info));
							drm_clients[i].magic = magic;
						}

						bzero(tx_buf, sizeof(tx_buf));
						snprintf(tx_buf, sizeof(tx_buf), "%d:%d", magic, DRM_OK);
					} while (0);

					break;

				case CMD_CRTC:	/* save old crtc and create new crtc */
					do {
						drmModeModeInfo *tm;
						uint32_t a, b;

						sscanf(rx_buf, "%d:%d:%d:%d:%d:%s", &a, &b,
							&drm_clients[i].crtc_id, &drm_clients[i].conn_id,
							&drm_clients[i].fb, drm_clients[i].mode_name);

						fprintf(stdout, "got req: %d:%d:%d:%d:%d:%s\n", a, b,
							drm_clients[i].crtc_id, drm_clients[i].conn_id,
							drm_clients[i].fb, drm_clients[i].mode_name);

						tm = drm_get_mode_by_name(fd, drm_clients[i].conn_id, drm_clients[i].mode_name);

						if (!tm) {
							perror("failed drm_get_mode_by_name");
							ret = -1;
							break;
						}

						drm_clients[i].mode = tm;

						/* store current crtc */

						drm_clients[i].saved_crtc = drmModeGetCrtc(fd, drm_clients[i].crtc_id);
						if (drm_clients[i].saved_crtc == NULL) {
							perror("failed drmModeGetCrtc(current)");
							ret = -1;
							break;
						}

						dump_crtc_configuration("saved_crtc", drm_clients[i].saved_crtc);

						/* setup new crtc */

						ret = drmModeSetCrtc(fd, drm_clients[i].crtc_id, drm_clients[i].fb, 0, 0,
								&drm_clients[i].conn_id, 1, drm_clients[i].mode);

						if (ret) {
							perror("failed drmModeSetCrtc(new)");
							ret = -1;
							break;
						}

						drm_clients[i].current_crtc = drmModeGetCrtc(fd, drm_clients[i].crtc_id);

						if (drm_clients[i].current_crtc != NULL) {
							dump_crtc_configuration("current_crtc", drm_clients[i].current_crtc);
						} else {
							perror("failed drmModeGetCrtc(new)");
							ret = -1;
							break;
						}

						bzero(tx_buf, sizeof(tx_buf));
						snprintf(tx_buf, sizeof(tx_buf), "%d:%d", magic, DRM_OK);
					} while (0);

					break;

				case CMD_PLANE:	/* setup plane */
					do {
						uint32_t a, b;

						sscanf(rx_buf, "%d:%d:%d:%d:%d:%d:%d:%d:%d", &a, &b,
							&drm_clients[i].crtc_id, &drm_clients[i].plane_id, &drm_clients[i].fb,
							&drm_clients[i].w, &drm_clients[i].h, &drm_clients[i].x, &drm_clients[i].y);

						fprintf(stdout, "got req: %d:%d:%d:%d:%d:%d:%d:%d:%d\n", a, b,
							drm_clients[i].crtc_id, drm_clients[i].plane_id, drm_clients[i].fb,
							drm_clients[i].w, drm_clients[i].h, drm_clients[i].x, drm_clients[i].y);

						ret = drmModeSetPlane(fd, drm_clients[i].plane_id, drm_clients[i].crtc_id,
								drm_clients[i].fb, 0, drm_clients[i].x, drm_clients[i].y,
								drm_clients[i].w, drm_clients[i].h, 0, 0,
								drm_clients[i].w << 16, drm_clients[i].h << 16);

						if (ret) {
							perror("cannot set plane");
							ret = -1;
							break;
						}

						bzero(tx_buf, sizeof(tx_buf));
						snprintf(tx_buf, sizeof(tx_buf), "%d:%d", magic, DRM_OK);
					} while (0);

					break;

				case CMD_CRTC_STOP:	/* client disconnects */
					do {

						if (drm_clients[i].saved_crtc->mode_valid) {
							ret = drmModeSetCrtc(fd, drm_clients[i].saved_crtc->crtc_id,
									drm_clients[i].saved_crtc->buffer_id,
									drm_clients[i].saved_crtc->x, drm_clients[i].saved_crtc->y,
									&(drm_clients[i].conn_id), 1, &(drm_clients[i].saved_crtc->mode));

							if (ret) {
								perror("failed drmModeSetCrtc(restore original)");
								ret = -1;
								break;
							}
						}

						bzero(tx_buf, sizeof(tx_buf));
						snprintf(tx_buf, sizeof(tx_buf), "%d:%d", magic, DRM_OK);
					} while (0);

					break;

				case CMD_PLANE_STOP:	/* client disconnects */
					do {


						ret = drmModeSetPlane(fd, drm_clients[i].plane_id,
								0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

						if (ret) {
							perror("failed drmModeSetCrtc(restore original)");
							ret = -1;
							break;
						}

						bzero(tx_buf, sizeof(tx_buf));
						snprintf(tx_buf, sizeof(tx_buf), "%d:%d", magic, DRM_OK);
					} while (0);

					break;

				default:
					ret = -1;
			}

			if (ret == -1) {
				bzero(tx_buf, sizeof(tx_buf));
				snprintf(tx_buf, sizeof(tx_buf), "%d:%d", magic, DRM_ERROR);
			}

			fprintf(stdout, "send to client %d response [%s]\n", i, tx_buf);
			write(clients[i].fd, tx_buf, sizeof(tx_buf));

			if (--rc == 0) break;
		}

	} while (running);

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
