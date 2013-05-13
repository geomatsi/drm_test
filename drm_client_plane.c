#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>

#include <xf86drmMode.h>
#include <xf86drm.h>
#include <libkms.h>

#include "bitmap_utils.h"
#include "drm_utils.h"
#include "drm_proto.h"

/* */

static const char device_name[] = "/dev/dri/card0";

/* */

int main(int argc, char *argv[])
{
	/* */

	int ret, opt, i, imt = 0;

	/* drm vars */

	struct kms_driver *drv;
	struct kms_bo *bo;

	uint32_t plane_id = 0, crtc_id = 0;
	uint32_t width = 0, height = 0;
	uint32_t posx = 0, posy = 0;
	uint32_t fb;
	int fd;

	uint32_t stride, handle;
	uint32_t *dst;

	drm_magic_t magic;

	uint32_t attr[] = {
		KMS_WIDTH, 0,
		KMS_HEIGHT, 0,
		KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
		KMS_TERMINATE_PROP_LIST
	};

	drmModePlaneRes *resources;
	drmModePlane *plane = NULL;

	/* net vars */

	struct sockaddr_un  serv_addr;
	int sockfd, servlen;
	struct timeval tv;
	int val, mag;
	fd_set rset;

	char rx_buf[DRM_SRV_MSGLEN + 1];
	char tx_buf[DRM_SRV_MSGLEN + 1];

	uint32_t command;

	/* parse command line */

	while ((opt = getopt(argc, argv, "t:x:y:w:v:c:p:m:h")) != -1) {
		switch (opt) {
			case 't':
				imt = atoi(optarg);
				break;
			case 'x':
				posx = atoi(optarg);
				break;
			case 'y':
				posy = atoi(optarg);
				break;
			case 'w':
				width = atoi(optarg);
				break;
			case 'v':
				height = atoi(optarg);
				break;
			case 'p':
				plane_id = atoi(optarg);
				break;
			case 'c':
				crtc_id = atoi(optarg);
				break;
			case 'h':
			default:
				printf("usage: -h] -c <connector> -e <encoder> -m <mode>\n");
				printf("\t-h: this help message\n");
				printf("\t-c <crtc>			crtc id, default is 0\n");
				printf("\t-p <plane>		plane id, default is 0\n");
				printf("\t-x <posx>			plane top left corner xpos, default is 0'\n");
				printf("\t-y <posy>			plane top left corner ypos, default is 0'\n");
				printf("\t-w <width>		plane width, default is 0'\n");
				printf("\t-v <height>		plane height, default is 0'\n");
				printf("\t-t <image>		image type, default is 0\n");
				exit(0);
		}
	}

	/* init connection to server */

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;

	strcpy(serv_addr.sun_path, DRM_SERVER_NAME);
	servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("could not create socket");
		ret = -1;
		goto err_exit;
	}

	if (connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0) {
		perror("could not connect to server");
		ret = -1;
		goto err_close_net;
	}

	/* drm configuration */

	fd = open(device_name, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("cannot open drm device");
		goto err_close_net;
	}

    drmDropMaster(fd);

	ret = drmGetMagic(fd, &magic);
	if (ret) {
		perror("failed drmGetMagic");
		goto err_close_drm;
	}

	// get plane
	resources = drmModeGetPlaneResources(fd);
	if (!resources || resources->count_planes == 0) {
		fprintf(stderr, "drmModeGetPlaneResources failed\n");
		ret = -ENODEV;
		goto err_close_drm;
	}

	for (i = 0; i < resources->count_planes; i++) {
		drmModePlane *p = drmModeGetPlane(fd, resources->planes[i]);
		if (!p)
			continue;

		if (p->plane_id == plane_id) {
			plane = p;
			break;
		}

		drmModeFreePlane(plane);
	}

	if (!plane) {
		fprintf(stderr, "couldn't find specified plane\n");
		ret = -ENODEV;
		goto err_close_drm;
	}

	// set display dimensions for chosen configuration
	attr[1] = width;
	attr[3] = height;

	// create kms driver
	ret = kms_create(fd, &drv);
	if (ret) {
		perror("failed kms_create()");
		goto err_close_drm;
	}

	// create dumb buffer object
	ret = kms_bo_create(drv, attr, &bo);
	if (ret) {
		perror("failed kms_bo_create()");
		goto err_driver_destroy;
	}

	ret = kms_bo_get_prop(bo, KMS_PITCH, &stride);
	if (ret) {
		perror("failed kms_bo_get_prop(KMS_PITCH)");
		goto err_buffer_destroy;
	}

	ret = kms_bo_get_prop(bo, KMS_HANDLE, &handle);
	if (ret) {
		perror("failed kms_bo_get_prop(KMS_HANDLE)");
		goto err_buffer_destroy;
	}

	// map dumb buffer object
	ret = kms_bo_map(bo, (void **) &dst);
	if (ret) {
		perror("failed kms_bo_map()");
		goto err_buffer_destroy;
	}

	// create drm framebuffer
	ret = drmModeAddFB(fd, width, height, 24, 32, stride, handle, &fb);
	if (ret) {
		perror("failed drmModeAddFB()");
		goto err_buffer_unmap;
	}

	// everything is ready: send magic to server
	command = CMD_AUTH;
	bzero(tx_buf, sizeof(tx_buf));
	snprintf(tx_buf, sizeof(tx_buf), "%d:%d", magic, command);
	fprintf(stdout, "send message [%s] to server\n", tx_buf);

	ret = write(sockfd, tx_buf, sizeof(tx_buf));
	if (ret < 0) {
		perror("could not send magic to server");
		goto err_buffer_unmap;
	}

	/* server communication loop */

	while (1) {

		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);

		tv.tv_sec = 5;
		tv.tv_usec = 0;

		ret = select(sockfd + 1, &rset, NULL, NULL, &tv);

		if (ret == 0) {
			fprintf(stdout, "timeout expired: no response from server\n");
			ret = -1;
			break;
		}

		if (ret == -1) {
			if (errno == EINTR) {
				perror("'select' was interrupted");
			} else {
				perror("'select' problem");
			}
			break;
		}

		if (FD_ISSET(sockfd, &rset)) {

			bzero(rx_buf, sizeof(rx_buf));
			ret = read(sockfd, rx_buf, sizeof(rx_buf));
			if (ret <= 0) {
				perror("could not receive answer from server");
				ret = -1;
				break;
			}

			fprintf(stdout, "got server response: [%s]\n", rx_buf);
			ret = sscanf(rx_buf, "%d:%d", &mag, &val);

			if (ret < 2) {
				perror("incomplete response from server");
				ret = -1;
				break;
			}

			if (mag != magic) {
				fprintf(stderr, "invalid magic response: %d", mag);
				continue;
			}

			if (val != DRM_OK) {
				fprintf(stdout, "got error from server, can't continue\n");
				ret = -1;
				break;
			}

			switch (command) {
				case CMD_AUTH:
					command = CMD_PLANE;
					bzero(tx_buf, sizeof(tx_buf));
					snprintf(tx_buf, sizeof(tx_buf), "%d:%d:%d:%d:%d:%d:%d:%d:%d",
						magic, command, crtc_id, plane_id, fb, width, height, posx, posy);

					ret = write(sockfd, tx_buf, sizeof(tx_buf));
					if (ret < 0) {
						perror("could not send plane message to server");
						goto err_fb;
					}

					break;

				case CMD_PLANE:
					if (imt)
						draw_fancy_image((uint32_t *) dst, width, height);
					else
						draw_test_image((uint32_t *) dst, width, height);

					/* FIXME: for some reason so far only vmware needed it */
					drmModeDirtyFB(fd, fb, NULL, 0);

					getchar();

					command = CMD_PLANE_STOP;
					bzero(tx_buf, sizeof(tx_buf));
					snprintf(tx_buf, sizeof(tx_buf), "%d:%d", magic, command);

					ret = write(sockfd, tx_buf, sizeof(tx_buf));
					if (ret < 0) {
						perror("could not send quit message to server");
						goto err_fb;
					}

					break;

				case CMD_PLANE_STOP:
					fprintf(stdout, "server ok, quit...\n");
					goto err_fb;

				default:
					fprintf(stderr, "skip unexpected command = %d\n", command);
					break;
			}

		} else {
			fprintf(stdout, "hmm... should not happen... lets try one more cycle\n");
		}
	}

err_fb:
	drmModeRmFB(fd, fb);

err_buffer_unmap:
	ret = kms_bo_unmap(bo);
	if (ret) {
		perror("failed kms_bo_unmap(new)");
	}

err_buffer_destroy:
	ret = kms_bo_destroy(&bo);
	if (ret) {
		perror("failed kms_bo_destroy(new)");
	}

err_driver_destroy:
	ret = kms_destroy(&drv);
	if (ret) {
		perror("failed kms_destroy(new)");
	}

err_close_drm:
	close(fd);

err_close_net:
	close(sockfd);

err_exit:
	return ret;
}
