#include "drm_proto.h"
#include "drm_utils.h"

/* */

int drm_to_server(char *tx_buf)
{
		struct sockaddr_un  serv_addr;
		int sockfd, servlen;
        struct timeval tv;
		int ret, val, mag;
        fd_set rset;

		char rx_buf[DRM_SRV_MSGLEN + 1];

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

		fprintf(stdout, "send message [%s] to server\n", tx_buf);

		ret = write(sockfd, tx_buf, sizeof(tx_buf));
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
                fprintf(stdout, "timeout expired: no response from server\n");
				ret = -1;
                goto err_close;
            }

            if (ret == -1) {
                if (errno == EINTR) {
                    perror("'select' was interrupted");
					goto err_close;
                } else {
                    perror("'select' problem");
                    goto err_close;
                }
            }

            if (FD_ISSET(sockfd, &rset)) {

                bzero(rx_buf, sizeof(rx_buf));
                ret = read(sockfd, rx_buf, sizeof(rx_buf));
                if (ret <= 0) {
                    perror("could not receive answer from server");
					ret = -1;
                    goto err_close;
                }

                fprintf(stdout, "got server response: [%s]\n", rx_buf);

				ret = sscanf(rx_buf, "%d:%d", &mag, &val);

				if (ret < 2) {
					perror("incomplete response from server");
					ret = -1;
					goto err_close;
				}
#if 0
				if (mag != magic) {
					perror("invalid destination in response");
					ret = -1;
					goto err_close;
				}
#endif
                fprintf(stdout, "ok, got response %d\n", val);
				ret = val;
				goto err_close;

            } else {
                fprintf(stdout, "hmm... should not happen... lets try one more cycle\n");
            }
        }

err_close:
	close(sockfd);

err_exit:
	return ret;
}
