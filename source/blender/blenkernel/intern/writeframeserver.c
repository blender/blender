/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2006 Peter Schlaile
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/writeframeserver.c
 *  \ingroup bke
 *
 * Frameserver
 * Makes Blender accessible from TMPGenc directly using VFAPI (you can
 * use firefox too ;-)
 */

#ifdef WITH_FRAMESERVER

#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <winbase.h>
#include <direct.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <fcntl.h>
#endif

#include <stdlib.h>

#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"

#include "BKE_writeframeserver.h"
#include "BKE_global.h"
#include "BKE_report.h"

#include "DNA_scene_types.h"

static int sock;
static int connsock;
static int write_ppm;
static int render_width;
static int render_height;


#if defined(_WIN32)
static int startup_socket_system(void)
{
	WSADATA wsa;
	return (WSAStartup(MAKEWORD(2, 0), &wsa) == 0);
}

static void shutdown_socket_system(void)
{
	WSACleanup();
}
static int select_was_interrupted_by_signal(void)
{
	return (WSAGetLastError() == WSAEINTR);
}
#else
static int startup_socket_system(void)
{
	return 1;
}

static void shutdown_socket_system(void)
{
}

static int select_was_interrupted_by_signal(void)
{
	return (errno == EINTR);
}

static int closesocket(int fd)
{
	return close(fd);
}
#endif

int start_frameserver(struct Scene *scene, RenderData *UNUSED(rd), int rectx, int recty, ReportList *reports)
{
	struct sockaddr_in addr;
	int arg = 1;
	
	(void)scene; /* unused */

	if (!startup_socket_system()) {
		BKE_report(reports, RPT_ERROR, "Can't startup socket system");
		return 0;
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		shutdown_socket_system();
		BKE_report(reports, RPT_ERROR, "Can't open socket");
		return 0;
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*) &arg, sizeof(arg));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(U.frameserverport);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		shutdown_socket_system();
		BKE_report(reports, RPT_ERROR, "Can't bind to socket");
		return 0;
	}

	if (listen(sock, SOMAXCONN) < 0) {
		shutdown_socket_system();
		BKE_report(reports, RPT_ERROR, "Can't establish listen backlog");
		return 0;
	}
	connsock = -1;

	render_width = rectx;
	render_height = recty;

	return 1;
}

static char index_page[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<html><head><title>Blender Frameserver</title></head>\n"
"<body><pre>\n"
"<H2>Blender Frameserver</H2>\n"
"<A HREF=info.txt>Render Info</A><br>\n"
"<A HREF=close.txt>Stop Rendering</A><br>\n"
"\n"
"Images can be found here\n"
"\n"
"images/ppm/%d.ppm\n"
"\n"
"</pre></body></html>\n";

static char good_bye[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<html><head><title>Blender Frameserver</title></head>\n"
"<body><pre>\n"
"Render stopped. Goodbye</pre></body></html>";

static int safe_write(char * s, int tosend)
{
	int total = tosend;
	do {
		int got = send(connsock, s, tosend, 0);
		if (got < 0) {
			return got;
		}
		tosend -= got;
		s += got;
	} while (tosend > 0);

	return total;
}

static int safe_puts(char * s)
{
	return safe_write(s, strlen(s));
}

static int handle_request(RenderData *rd, char * req)
{
	char * p;
	char * path;
	int pathlen;

	if (memcmp(req, "GET ", 4) != 0) {
		return -1;
	}
	   
	p = req + 4;
	path = p;

	while (*p != ' ' && *p) p++;

	*p = 0;

	if (strcmp(path, "/index.html") == 0 || strcmp(path, "/") == 0) {
		safe_puts(index_page);
		return -1;
	}

	write_ppm = 0;
	pathlen = strlen(path);

	if (pathlen > 12 && memcmp(path, "/images/ppm/", 12) == 0) {
		write_ppm = 1;
		return atoi(path + 12);
	}
	if (strcmp(path, "/info.txt") == 0) {
		char buf[4096];

		sprintf(buf,
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"\r\n"
			"start %d\n"
			"end %d\n"
			"width %d\n"
			"height %d\n"
			"rate %d\n"
			"ratescale %d\n",
			rd->sfra,
			rd->efra,
			render_width,
			render_height,
			rd->frs_sec,
			1
			);

		safe_puts(buf);
		return -1;
	}
	if (strcmp(path, "/close.txt") == 0) {
		safe_puts(good_bye);
		G.afbreek = 1; /* Abort render */
		return -1;
	}
	return -1;
}

int frameserver_loop(RenderData *rd, ReportList *UNUSED(reports))
{
	fd_set readfds;
	struct timeval tv;
	struct sockaddr_in      addr;
	int len, rval;
#ifdef FREE_WINDOWS
	int socklen;
#else
	unsigned int socklen;
#endif
	char buf[4096];

	if (connsock != -1) {
		closesocket(connsock);
		connsock = -1;
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);

	rval = select(sock + 1, &readfds, NULL, NULL, &tv);
	if (rval < 0) {
		return -1;
	}

	if (rval == 0) { /* nothing to be done */
		return -1;
	}

	socklen = sizeof(addr);

	if ((connsock = accept(sock, (struct sockaddr *)&addr, &socklen)) < 0) {
		return -1;
	}

	FD_ZERO(&readfds);
	FD_SET(connsock, &readfds);

	for (;;) {
		/* give 10 seconds for telnet testing... */
		tv.tv_sec = 10;
		tv.tv_usec = 0;

			rval = select(connsock + 1, &readfds, NULL, NULL, &tv);
		if (rval > 0) {
			break;
		}
		else if (rval == 0) {
			return -1;
		}
		else if (rval < 0) {
			if (!select_was_interrupted_by_signal()) {
				return -1;
			}
		}
	}

	len = recv(connsock, buf, sizeof(buf) - 1, 0);

	if (len < 0) {
		return -1;
	}

	buf[len] = 0;

	return handle_request(rd, buf);
}

static void serve_ppm(int *pixels, int rectx, int recty)
{
	unsigned char* rendered_frame;
	unsigned char* row = (unsigned char*) malloc(render_width * 3);
	int y;
	char header[1024];

	sprintf(header,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: image/ppm\r\n"
		"Connection: close\r\n"
		"\r\n"
		"P6\n"
		"# Creator: blender frameserver v0.0.1\n"
		"%d %d\n"
		"255\n",
		rectx, recty);

	safe_puts(header);

	rendered_frame = (unsigned char *)pixels;

	for (y = recty - 1; y >= 0; y--) {
		unsigned char* target = row;
		unsigned char* src = rendered_frame + rectx * 4 * y;
		unsigned char* end = src + rectx * 4;
		while (src != end) {
			target[2] = src[2];
			target[1] = src[1];
			target[0] = src[0];
			
			target += 3;
			src += 4;
		}
		safe_write((char*)row, 3 * rectx);
	}
	free(row);
	closesocket(connsock);
	connsock = -1;
}

int append_frameserver(RenderData *UNUSED(rd), int UNUSED(start_frame), int frame, int *pixels,
                       int rectx, int recty, ReportList *UNUSED(reports))
{
	fprintf(stderr, "Serving frame: %d\n", frame);
	if (write_ppm) {
		serve_ppm(pixels, rectx, recty);
	}
	if (connsock != -1) {
		closesocket(connsock);
		connsock = -1;
	}

	return 0;
}

void end_frameserver(void)
{
	if (connsock != -1) {
		closesocket(connsock);
		connsock = -1;
	}
	closesocket(sock);
	shutdown_socket_system();
}

#endif /* WITH_FRAMESERVER */
