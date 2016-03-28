/* pulltab: tunnel arbitrary streams through HTTP proxies.
 * Copyright (C) 2014 Aleksa Sarai
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _XOPEN_SOURCE
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "b64/cencode.h"

#define BUF_SIZE 4096

#define DEFAULT_PROXY_PORT 8080
#define DEFAULT_DEST_PORT  22

#define CRLF "\r\n\r\n"
#define HTTP_VERSION "1.0"
#define PROXY_CONNECT_FORMAT "CONNECT %s:%d HTTP/" HTTP_VERSION
#define PROXY_BASIC_AUTH_FORMAT "\nProxy-Authorization: Basic %s"
#define PROXY_BASIC_AUTH_SEPARATOR ":"

#define PORT_UPPER_LIM 65535
#define PORT_LOWER_LIM 1

#define LENPRINTF(...) (snprintf(NULL, 0, __VA_ARGS__))

#if defined(DEBUG)
static void _debug(char *fmt, ...) {

	va_list ap;
	va_start(ap, fmt);

	fprintf(stderr, "[D:pulltab] ");
	vfprintf(stderr, fmt, ap);
	fflush(stderr);

	va_end(ap);
}
#else
#	define _debug(...)
#endif

enum {
	AUTH_NONE,
	AUTH_BASIC,
};

struct tab_opt {
	/* proxy server options */
	char *proxy_hostname;
	int proxy_port;

	/* proxy credentials (if applicable) */
	int proxy_auth;
	char *auth_username;
	char *auth_password;

	/* destination */
	char *dest_hostname;
	int dest_port;
};

static void tab_opt_init(struct tab_opt *opt) {
	opt->proxy_hostname = NULL;
	opt->proxy_port = DEFAULT_PROXY_PORT;
	opt->proxy_auth = AUTH_NONE;
	opt->auth_username = NULL;
	opt->auth_password = NULL;
	opt->dest_hostname = NULL;
	opt->dest_port = DEFAULT_DEST_PORT;
}

static void tab_opt_free(struct tab_opt *opt) {
	free(opt->proxy_hostname);
	free(opt->auth_username);
	free(opt->auth_password);
	free(opt->dest_hostname);
}

static void usage() {
	extern char *__progname;

	printf("%s [-a <auth-file>] -x proxy[:port] -d dest[:port] [-h]\n", __progname);
	printf("Tunnel arbitrary streams through HTTP proxies.\n");
	printf("\n");
	printf("Options:\n");
	printf("   -a <auth-file>  -- use HTTP Basic authentication, with the credentials in the given file (of the form 'user\\x00pass').\n");
	printf("   -x proxy[:port] -- tunnel through the given HTTP proxy (default port is %d).\n", DEFAULT_PROXY_PORT);
	printf("   -d dest[:port]  -- tunnel through to the given destination address (default port is %d).\n", DEFAULT_DEST_PORT);
	printf("   -h              -- print this help page and exit.\n");
}

static int sock_connect(char *hostname, int port) {
	struct sockaddr_in addr;

	/* create stream socket */
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
		return -1;

	/* try to resolve -- otherwise use as an ip address */
	struct hostent *hostent = gethostbyname(hostname);
	if(hostent)
		memcpy(&addr.sin_addr, hostent->h_addr, hostent->h_length);
	else
		addr.sin_addr.s_addr = inet_addr(hostname);

	_debug("resolving proxy '%s'\n", hostname);

	/* fill in other addr data */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	/* actually connect stream to host */
	if(connect(fd, (struct sockaddr *) &addr, sizeof(addr)))
		return -1;

	_debug("connected to proxy\n");
	return fd;
}

static char *generate_proxy_request(struct tab_opt *opt) {
	char *request_str = NULL;
	int request_len = 0;

	/* set up CONNECT request */
	int conn_len = LENPRINTF(PROXY_CONNECT_FORMAT, opt->dest_hostname, opt->dest_port);
	char *conn_str = malloc(conn_len + 1);
	snprintf(conn_str, conn_len + 1, PROXY_CONNECT_FORMAT, opt->dest_hostname, opt->dest_port);
	conn_str[conn_len] = '\0';

	/* append CONNECT to request */
	request_str = realloc(request_str, request_len + conn_len + 1);
	strncpy(request_str, conn_str, conn_len);
	request_len += conn_len;
	request_str[request_len] = '\0';

	/* set up Proxy-Authorization if needed */
	switch(opt->proxy_auth) {
		case AUTH_NONE:
			break;
		case AUTH_BASIC:
			{
				/* create basic "user:pass" spec */
				int auth_plain_len = LENPRINTF("%s:%s", opt->auth_username, opt->auth_password);
				char *auth_plain = malloc(auth_plain_len + 1);
				snprintf(auth_plain, auth_plain_len + 1, "%s:%s", opt->auth_username, opt->auth_password);
				auth_plain[auth_plain_len] = '\0';

				/* encode base64 digest for authentication */
				int auth_digest_len = LENTOBASE64(auth_plain_len), off = 0;
				char *auth_digest = malloc(auth_digest_len + 1);;
				base64_encodestate enc_state;
				base64_init_encodestate(&enc_state);
				off += base64_encode_block(auth_plain, auth_plain_len, auth_digest, &enc_state);
				off += base64_encode_blockend(auth_digest + off, &enc_state);
				auth_digest[off] = '\0';

				_debug("generated HTTP basic authentication digest '%s'\n", auth_digest);

				/* set up auth */
				int auth_len = LENPRINTF(PROXY_BASIC_AUTH_FORMAT, auth_digest);
				char *auth_str = malloc(auth_len + 1);
				snprintf(auth_str, auth_len + 1, PROXY_BASIC_AUTH_FORMAT, auth_digest);
				auth_str[auth_len] = '\0';

				/* append auth to request */
				request_str = realloc(request_str, request_len + auth_len + 1);
				strncat(request_str, auth_str, auth_len);
				request_len += auth_len;

				/* free memory */
				free(auth_plain);
				free(auth_digest);
				free(auth_str);
			}
			break;
	}

	/* append terminating CRLF */
	request_str = realloc(request_str, request_len + strlen(CRLF) + 1);
	strncat(request_str, CRLF, strlen(CRLF));
	request_len += strlen(CRLF);

	/* free memory */
	free(conn_str);

	/* request generated */
	_debug("generated proxy request\n");
	return request_str;
}

static void proxy_setup(struct tab_opt *opt, int sock_fd) {
	struct timeval tv;
	fd_set rfds, wfds;

	/* set timeout */
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	/* send request first */
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(sock_fd, &wfds);

	if(select(sock_fd+1, &rfds, &wfds, NULL, &tv) < 0) {
		perror("pulltab");
		goto error;
	}

	char *request = generate_proxy_request(opt);
	size_t len = write(sock_fd, request, strlen(request));
	if(len != strlen(request)) {
		fprintf(stderr, "pulltab: could not negotiate stream with proxy\n");
		free(request);
		goto error;
	}
	free(request);

	_debug("sent request to proxy\n");

	/* set timeout */
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	/* wait for response */
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(sock_fd, &rfds);

	if(select(sock_fd+1, &rfds, &wfds, NULL, &tv) < 0) {
		perror("pulltab");
		goto error;
	}

	/* read the response from the proxy */
	char buf[BUF_SIZE] = "";
	if(read(sock_fd, buf, BUF_SIZE) < 0) {
		perror("pulltab");
		goto error;
	}

	_debug("received response from proxy\n");

	char description[BUF_SIZE] = "";
	int maj = 0,
		min = 0,
		code = 0;

	/* parse the response. it should be of the form "HTTP/[x.y] [code] [description]". */
	if(sscanf(buf, "HTTP/%d.%d %d %[^\n]", &maj, &min, &code, description) < 4) {
		fprintf(stderr, "pulltab: error parsing proxy reponse\n");
		goto error;
	}

	_debug("parsed proxy response: %d (%s)\n", code, description);

	/* deal with error codes */
	if(code < 200 || code >= 300) {
		fprintf(stderr, "pulltab: error negotiating with proxy: %s\n", description);
		goto error;
	}

	/* deal with invalid HTTP version */
	if(maj < 1) {
		fprintf(stderr, "pulltab: invalid HTTP protocol version returned by proxy: %d.%d\n", maj, min);
		goto error;
	}
	return;

error:
	tab_opt_free(opt);
	exit(1);
}

static void bake_args(struct tab_opt *opt, int argc, char **argv) {
	int ch;
	while((ch = getopt(argc, argv, "a:x:d:h")) != -1) {
		switch(ch) {
			case 'a':
				{
					/* activate proxy auth */
					opt->proxy_auth = AUTH_BASIC;

					/* get a file descriptor for the auth file */
					int auth_fd = open(optarg, 0);
					if(auth_fd < 0) {
						perror("pulltab");
						close(auth_fd);
						goto error;
					}

					char *auth_str = NULL;
					char auth_buf[BUF_SIZE];
					int auth_len = 0, auth_dlen;

					/* read all file data -- in buffered chunks -- from the auth file (preserving null bytes) */
					while((auth_dlen = read(auth_fd, auth_buf, BUF_SIZE)) > 0) {
						auth_str = realloc(auth_str, auth_len + auth_dlen);
						memcpy(auth_str + auth_len, auth_buf, auth_dlen);
						auth_len += auth_dlen;
					}

					if(auth_dlen < 0) {
						perror("pulltab");
						free(auth_str);
						close(auth_fd);
						goto error;
					}

					/* find null separator in auth spec */
					char *auth_sep = memchr(auth_str, '\0', auth_len);
					if(!auth_sep) {
						fprintf(stderr, "pulltab: invalid authentication specfication: no NULL separator\n");
						free(auth_str);
						close(auth_fd);
						goto error;
					}

					/* calculate length and offsets of username:password in string */
					int auth_ulen = auth_sep - auth_str;
					int auth_plen = auth_len - (auth_ulen + 1);

					/* copy over username */
					opt->auth_username = malloc(auth_ulen + 1);
					strncpy(opt->auth_username, auth_str, auth_ulen);
					opt->auth_username[auth_ulen] = '\0';

					/* copy over username */
					opt->auth_password = malloc(auth_plen + 1);
					strncpy(opt->auth_password, auth_str + (auth_ulen + 1), auth_plen);
					opt->auth_password[auth_plen] = '\0';

					_debug("got HTTP basic authentication username '%s'\n", opt->auth_username);
					_debug("got HTTP basic authentication password '%s'\n", opt->auth_password);

					/* clean up */
					free(auth_str);
					close(auth_fd);
				}
				break;
			case 'x':
				{
					int proxy_len = strlen(optarg);

					/* copy proxy spec */
					char *proxy_string = malloc(proxy_len + 1);
					strncpy(proxy_string, optarg, proxy_len);
					proxy_string[proxy_len] = '\0';

					/* look for host:port separator */
					int proxy_hlen = proxy_len;
					char *proxy_sep = memchr(proxy_string, ':', proxy_len);

					/* deal with optional port number */
					if(proxy_sep) {
						proxy_hlen = proxy_sep - proxy_string;
						opt->proxy_port = atoi(proxy_sep + 1);
					} else {
						opt->proxy_port = DEFAULT_PROXY_PORT;
					}

					/* make sure port number is valid */
					if(opt->proxy_port < PORT_LOWER_LIM || opt->proxy_port > PORT_UPPER_LIM) {
						fprintf(stderr, "pulltab: invalid proxy specification: proxy port is not in valid range\n");
						free(proxy_string);
						goto error;
					}

					/* copy hostname over */
					opt->proxy_hostname = malloc(proxy_hlen + 1);
					strncpy(opt->proxy_hostname, proxy_string, proxy_hlen);
					opt->proxy_hostname[proxy_hlen] = '\0';

					/* clean up */
					free(proxy_string);
				}
				break;
			case 'd':
				{
					int dest_len = strlen(optarg);

					/* copy dest spec */
					char *dest_string = malloc(dest_len + 1);
					strncpy(dest_string, optarg, dest_len);
					dest_string[dest_len] = '\0';

					/* look for host:port separator */
					int dest_hlen = dest_len;
					char *dest_sep = memchr(dest_string, ':', dest_len);

					/* deal with optional port number */
					if(dest_sep) {
						dest_hlen = dest_sep - dest_string;
						opt->dest_port = atoi(dest_sep + 1);
					} else {
						opt->dest_port = DEFAULT_DEST_PORT;
					}

					/* make sure port number is valid */
					if(opt->dest_port < PORT_LOWER_LIM || opt->dest_port > PORT_UPPER_LIM) {
						fprintf(stderr, "pulltab: invalid dest specification: dest port is not in valid range\n");
						free(dest_string);
						goto error;
					}

					/* copy hostname over */
					opt->dest_hostname = malloc(dest_hlen + 1);
					strncpy(opt->dest_hostname, dest_string, dest_hlen);
					opt->dest_hostname[dest_hlen] = '\0';

					/* clean up */
					free(dest_string);
				}
				break;
			case 'h':
				usage();
				exit(0);
				goto success;
			case '?':
			default:
				usage();
				goto error;
		}
	}

	/* make sure a proxy hostname has been given */
	if(!opt->proxy_hostname) {
		fprintf(stderr, "pulltab: missing proxy specification\n");
		goto error;
	}

	/* make sure a dest hostname has been given */
	if(!opt->dest_hostname) {
		fprintf(stderr, "pulltab: missing dest specification\n");
		goto error;
	}

	return;

success:
	tab_opt_free(opt);
	exit(0);

error:
	tab_opt_free(opt);
	exit(1);
}

int main(int argc, char **argv) {
	/* initialise internal option struct */
	struct tab_opt opt;
	tab_opt_init(&opt);

	/* parse argument */
	bake_args(&opt, argc, argv);

	/* connect to the proxy */
	int sock_fd = sock_connect(opt.proxy_hostname, opt.proxy_port);
	if(sock_fd < 0) {
		perror("pulltab");
		goto error;
	}

	/* set up tunneling through the proxy */
	proxy_setup(&opt, sock_fd);

	/* set up relay structures */
	struct timeval tv;
	fd_set rfds;
	char buffer[BUF_SIZE];
	size_t len;

	/* main relay loop */
	_debug("starting main relay loop\n");
	while(1) {
		/* set timeout */
		tv.tv_sec = 5;
		tv.tv_usec = 0;

		FD_ZERO(&rfds);
		FD_SET(sock_fd, &rfds);
		FD_SET(STDIN_FILENO, &rfds);

		if(select(sock_fd+1, &rfds, NULL, NULL, &tv) < 0) {
			perror("pulltab");
			goto error;
		}

		/* is there any data ready to read from the socket? */
		if(FD_ISSET(sock_fd, &rfds)) {
			len = read(sock_fd, buffer, BUF_SIZE);
			if(len <= 0)
				break;

			len = write(STDOUT_FILENO, buffer, len);
			if(len <= 0)
				break;
		}

		/* is there any data ready to read from stdin?? */
		if(FD_ISSET(STDIN_FILENO, &rfds)) {
			len = read(STDIN_FILENO, buffer, BUF_SIZE);
			if(len <= 0)
				break;

			len = write(sock_fd, buffer, len);
			if(len <= 0)
				break;
		}
	}

	_debug("connection closed\n");

	/* clean up */
	if(sock_fd >= 0)
		close(sock_fd);

	tab_opt_free(&opt);
	return 0;

error:
	/* clean up */
	if(sock_fd >= 0)
		close(sock_fd);

	tab_opt_free(&opt);
	exit(1);
}
