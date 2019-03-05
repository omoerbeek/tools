/*
 * Copyright (c) 2019 Otto Moerbeek <otto@drijf.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

char *port = "54";
char *host;
int fourflag = 1;
int sixflag = 1;


void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "%s: usage [-46] [-p port] host\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[]) 
{
	int ch, ret, s, save_errno;
	struct addrinfo h;
	struct addrinfo *res, *res0;
	char *cause, buf[1];
	struct timeval sec = {3, 0};

	while ((ch = getopt(argc, argv, "46p:")) != -1) {
		switch (ch) {
		case '4':
			sixflag = 0; fourflag = 1;
			break;
		case '6':
			sixflag = 1; fourflag = 0;
			break;
		case 'p':
			port = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();
	host = *argv;

	memset(&h, 0, sizeof(h));
	if (fourflag && sixflag)
		h.ai_family = AF_UNSPEC;
	else if (fourflag)
		h.ai_family = AF_INET;
	else
		h.ai_family = AF_INET6;
		
	h.ai_flags = AI_ADDRCONFIG;
	h.ai_socktype = SOCK_DGRAM;
	h.ai_protocol = IPPROTO_UDP;
	
	ret = getaddrinfo(host, port, &h, &res0);
	if (ret) 
		errx(1, "%s: %s", host, gai_strerror(ret));

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		char hostname[NI_MAXHOST];
		char portnum[NI_MAXSERV];
		getnameinfo(res->ai_addr, res->ai_addrlen, hostname, sizeof(hostname), portnum, sizeof(portnum),
		    NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM);
		printf("Connecting to %s:%s...\n", hostname, portnum);
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			cause = "socket";
			continue;
		}

		if (connect(s, res->ai_addr, res->ai_addrlen) == -1) {
			cause = "connect";
			save_errno = errno;
			close(s);
			errno = save_errno;
			s = -1;
			continue;
		}

		printf("Connected!\n");
		break;  /* okay we got one */
	}
	if (s == -1)
		err(1, "%s", cause);

	freeaddrinfo(res);
	ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &sec, sizeof(sec));
	if (ret != 0)
		err(1, "setsockopt SO_RCVTIMEO");


	buf[0] = 0xba;
	ret = send(s, buf, 1, 0);
	if (ret != 1)
		err(1, "send");

	ret = recvfrom(s, buf, 1, 0, NULL, NULL);
	if (ret != 1) {
		switch (errno) {
		case EAGAIN:
			fprintf(stderr, "Timeout happened\n");
			exit(1);
		case ECONNREFUSED:
			fprintf(stderr, "Connection actively refused!\n");
			exit(1);
		default:
			err(1, "recv");
		}
	}
	exit(0);
}



