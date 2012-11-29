#include "net_utils.h"
#include "drm_utils.h"

/* */

int drm_server_auth(drm_magic_t magic)
{
		struct sockaddr_un  serv_addr;
		int sockfd, servlen,n;
        struct timeval tv;
        fd_set rset;
		int ret;

		char buffer[DRM_SRV_MSGLEN];


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
			goto err_close;
		}

		bzero(buffer, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%d", magic);
		fprintf(stdout, "send magic [%d] to server\n", magic);

		ret = write(sockfd, buffer, sizeof(buffer));
		if (ret < 0) {
			perror("could not send magic to server");
			goto err_close;
		}

        /* wait for server response */

        while (1) {

            FD_ZERO(&rset);
            FD_SET(sockfd, &rset);

            tv.tv_sec = 5;
            tv.tv_usec = 0;

            ret = select(sockfd + 1, &rset, NULL, NULL, &tv);

            if (ret == 0) {
                /* timeout */
                fprintf(stdout, "timeout expired: no response from server\n");
				ret = -1;
                goto err_close;
            }

            if (ret == -1) {
                if (errno == EINTR) {
                    perror("'select' was interrupted, try again");
                    continue;
                } else {
                    perror("'select' problem");
                    goto err_close;
                }
            }

            if (FD_ISSET(sockfd, &rset)) {

                bzero(buffer, sizeof(buffer));
                ret = read(sockfd, buffer, sizeof(buffer));
                if (ret <= 0) {
                    perror("could not receive answer from server");
					ret = -1;
                    goto err_close;
                }

                fprintf(stdout, "got server response: [%s]\n", buffer);

                if (0 == strncmp(buffer, DRM_AUTH_OK, strlen(DRM_AUTH_OK))) {
                    fprintf(stdout, "ok, authentication successful\n");
					ret = 0;
                    break;
                } else {
                    fprintf(stdout, "authentication failed\n");
					ret = -1;
                    goto err_close;
                }

            } else {
                fprintf(stdout, "hmm... should not happen... lets try one more cycle\n");
            }
        }

err_close:
	close(sockfd);

err_exit:
	return ret;
}
