/* pulltab: tunnel arbitrary streams through HTTP proxies.
 * Copyright (C) 2014 Cyphar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN DESTECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if !defined(bool)
#	define bool int
#	define true 1
#	define false 0
#endif

#define BUF_SIZE 4096
#define CARRIAGE_RETURN "\r\n\r\n"

#define DEFAULT_PROXY_PORT 8080
#define DEFAULT_DEST_PORT  22

#define PORT_UPPER_LIM 65535
#define PORT_LOWER_LIM 1

struct tab_opt {
	/* proxy server options */
	char *proxy_hostname;
	int proxy_port;

	/* proxy credentials (if applicable) */
	bool proxy_auth;
	char *proxy_username;
	char *proxy_password;

	/* destination */
	char *dest_hostname;
	int dest_port;
};

void tab_opt_init(struct tab_opt *opt) {
	opt->proxy_hostname = NULL;
	opt->proxy_port = DEFAULT_PROXY_PORT;
	opt->proxy_auth = false;
	opt->proxy_username = NULL;
	opt->proxy_password = NULL;
	opt->dest_hostname = NULL;
	opt->dest_port = DEFAULT_DEST_PORT;
}

void tab_opt_free(struct tab_opt *opt) {
	free(opt->proxy_hostname);
	free(opt->proxy_username);
	free(opt->proxy_password);
	free(opt->dest_hostname);
}

void usage() {
	printf("\n");
}

void bake_args(struct tab_opt *opt, int argc, char **argv) {
	int ch;
	while((ch = getopt(argc, argv, "a:x:d:h")) != -1) {
		switch(ch) {
			case 'a':
				{
					/* activate proxy auth */
					opt->proxy_auth = true;

					/* get a file descriptor for the auth file */
					int auth_fd = open(optarg, 0);
					if(auth_fd < 0) {
						perror("pulltab");
						exit(1);
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
						exit(1);
					}

					/* find null separator in auth spec */
					char *auth_sep = memchr(auth_str, '\0', auth_len);
					if(!auth_sep) {
						fprintf(stderr, "pulltab: invalid authentication specfication: no NULL separator\n");
						exit(1);
					}

					/* calculate length and offsets of username:password in string */
					int auth_ulen = auth_sep - auth_str;
					int auth_plen = auth_len - (auth_ulen + 1);

					/* copy over username */
					opt->proxy_username = malloc(auth_ulen + 1);
					memcpy(opt->proxy_username, auth_str, auth_ulen);
					opt->proxy_username[auth_ulen] = '\0';

					/* copy over username */
					opt->proxy_password = malloc(auth_plen + 1);
					memcpy(opt->proxy_password, auth_str + (auth_ulen + 1), auth_plen);
					opt->proxy_password[auth_plen] = '\0';

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
					memcpy(proxy_string, optarg, proxy_len);
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

					/* copy hostname over */
					opt->proxy_hostname = malloc(proxy_hlen + 1);
					memcpy(opt->proxy_hostname, proxy_string, proxy_hlen);
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
					memcpy(dest_string, optarg, dest_len);
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
						exit(1);
					}

					/* copy hostname over */
					opt->dest_hostname = malloc(dest_hlen + 1);
					memcpy(opt->dest_hostname, dest_string, dest_hlen);
					opt->dest_hostname[dest_hlen] = '\0';

					/* clean up */
					free(dest_string);
				}
				break;
			case 'h':
				usage();
				exit(0);
				break;
			case '?':
			default:
				usage();
				exit(1);
		}
	}

	/* make sure a proxy hostname has been given */
	if(!opt->proxy_hostname) {
		fprintf(stderr, "pulltab: missing proxy specification\n");
		exit(1);
	}

	/* make sure a dest hostname has been given */
	if(!opt->dest_hostname) {
		fprintf(stderr, "pulltab: missing dest specification\n");
		exit(1);
	}

	/* make sure proxy port number is valid */
	if(opt->proxy_port < PORT_LOWER_LIM || opt->proxy_port > PORT_UPPER_LIM) {
		fprintf(stderr, "pulltab: invalid proxy specification: proxy port is not in valid range\n");
		exit(1);
	}

	/* make sure dest port number is valid */
	if(opt->dest_port < PORT_LOWER_LIM || opt->dest_port > PORT_UPPER_LIM) {
		fprintf(stderr, "pulltab: invalid dest specification: dest port is not in valid range\n");
		exit(1);
	}
}

int main(int argc, char **argv) {
	struct tab_opt opt;
	tab_opt_init(&opt);

	bake_args(&opt, argc, argv);
	if(opt.proxy_auth) {
		printf("username: '%s' (%d)\n", opt.proxy_username, strlen(opt.proxy_username));
		printf("password: '%s' (%d)\n", opt.proxy_password, strlen(opt.proxy_password));
	}
	printf("   proxy: host(%d)='%s',port=%d\n", strlen(opt.proxy_hostname), opt.proxy_hostname, opt.proxy_port);
	printf("    dest: host(%d)='%s',port=%d\n", strlen(opt.dest_hostname), opt.dest_hostname, opt.dest_port);

	tab_opt_free(&opt);
	return 0;
}
