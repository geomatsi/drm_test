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
    drm_magic_t magic;
    int ret, fd;

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
        magic = atoi(buf);
        fprintf(stdout, "accepted request for magic = %d\n", magic);

        ret = drmAuthMagic(fd, magic);
        if (ret) {
            perror("failed drmAuthMagic");
        }

        bzero(buf, sizeof(buf));
        snprintf(buf, sizeof(buf), ret ? DRM_AUTH_FAIL : DRM_AUTH_OK);
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
