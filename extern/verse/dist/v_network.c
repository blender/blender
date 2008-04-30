/*
**
*/

#if defined _WIN32
#include <winsock.h>
typedef unsigned int uint;
typedef SOCKET VSocket;
#else
typedef int VSocket;
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include <stdio.h>
#include <stdlib.h>

typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char uint8;
typedef char int8;
typedef unsigned char boolean;

#include "v_cmd_gen.h"
#include "v_network.h"

#if !defined socklen_t
#define socklen_t int
#endif

#define	TRUE	1
#define	FALSE	0

typedef struct{
	struct sockaddr_in address;
	struct hostent *he;
} VNetworkConnection;

#define VERSE_STD_CONNECT_TO_PORT 4950

static VSocket	my_socket = -1;
static uint16	my_port = 0;

void v_n_set_port(unsigned short port)
{
	my_port = port;
}

VSocket v_n_socket_create(void)
{
	static boolean initialized = FALSE;
	struct sockaddr_in address;
        int buffer_size = 1 << 20;

	if(my_socket != -1)
		return my_socket;
#if defined _WIN32
	if(!initialized)
	{
		WSADATA wsaData;

		if(WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
		{
			fprintf(stderr, "WSAStartup failed.\n");
			exit(1);
		}

	}
#endif
	initialized = TRUE;
	if((my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;
#if defined _WIN32
	{
		unsigned long	one = 1UL;
		if(ioctlsocket(my_socket, FIONBIO, &one) != 0)
			return -1;
	}
#else
	if(fcntl(my_socket, F_SETFL, O_NONBLOCK) != 0)
	{
		fprintf(stderr, "v_network: Couldn't make socket non-blocking\n");
		return -1;
	}
#endif
	address.sin_family = AF_INET;     /* host byte order */
	address.sin_port = htons(my_port); /* short, network byte order */
	address.sin_addr.s_addr = INADDR_ANY;
	if(bind(my_socket, (struct sockaddr *) &address, sizeof(struct sockaddr)) != 0)
	{
		fprintf(stderr, "v_network: Failed to bind(), code %d (%s)\n", errno, strerror(errno));
		exit(0); /* FIX ME */
	}
	if(setsockopt(my_socket, SOL_SOCKET, SO_SNDBUF, (const char *) &buffer_size, sizeof buffer_size) != 0)
		fprintf(stderr, "v_network: Couldn't set send buffer size of socket to %d\n", buffer_size);
	if(setsockopt(my_socket, SOL_SOCKET, SO_RCVBUF, (const char *) &buffer_size, sizeof buffer_size) != 0)
		fprintf(stderr, "v_network: Couldn't set receive buffer size of socket to %d\n", buffer_size);
	return my_socket;
}

void v_n_socket_destroy(void)
{
#if defined _WIN32
	closesocket(my_socket);
#else
	close(my_socket);
#endif
	my_socket = -1;
}

boolean v_n_set_network_address(VNetworkAddress *address, const char *host_name)
{
	struct hostent	*he;
	char		*colon = NULL, *buf = NULL;
	boolean		ok = FALSE;

	v_n_socket_create();
	address->port = VERSE_STD_CONNECT_TO_PORT;
	/* If a port number is included, as indicated by a colon, we need to work a bit more. */
	if((colon = strchr(host_name, ':')) != NULL)
	{
		size_t	hl = strlen(host_name);

		if((buf = malloc(hl + 1)) != NULL)
		{
			unsigned int	tp;

			strcpy(buf, host_name);
			colon = buf + (colon - host_name);
			*colon = '\0';
			host_name = buf;
			if(sscanf(colon + 1, "%u", &tp) == 1)
			{
				address->port = (unsigned short) tp;
				if(address->port != tp)	/* Protect against overflow. */
					host_name = NULL;
			}
			else
				host_name = NULL;	/* Protect against parse error. */
		}
		else
			return FALSE;
	}
	if(host_name != NULL && (he = gethostbyname(host_name)) != NULL)
	{
		memcpy(&address->ip, he->h_addr_list[0], he->h_length);
		address->ip = ntohl(address->ip);
		ok = TRUE;
	}
	if(buf != NULL)
		free(buf);

	return ok;
}

int v_n_send_data(VNetworkAddress *address, const char *data, size_t length)
{
	struct sockaddr_in	address_in;
	VSocket			sock;
	int			ret;

	if((sock = v_n_socket_create()) == -1 || length == 0)
		return 0;
	address_in.sin_family = AF_INET;     /* host byte order */
	address_in.sin_port = htons(address->port); /* short, network byte order */
	address_in.sin_addr.s_addr = htonl(address->ip);
	memset(&address_in.sin_zero, 0, sizeof address_in.sin_zero);
	ret = sendto(sock, data, length, 0, (struct sockaddr *) &address_in, sizeof(struct sockaddr_in));
	if(ret < 0)
		fprintf(stderr, "Socket sendto() of %u bytes failed, code %d (%s)\n", (unsigned int) length, errno, strerror(errno));
	return ret;
}

#if !defined V_GENERATE_FUNC_MODE

extern void *v_con_get_network_address_id(unsigned int id);
extern unsigned int v_con_get_network_address_count();

unsigned int v_n_wait_for_incoming(unsigned int microseconds) 
{
	struct timeval	tv;
	fd_set		fd_select;
	unsigned int	s1, f1, s2, f2;

	if(microseconds == 0)
		return 0;
	v_n_socket_create();
	tv.tv_sec  = microseconds / 1000000;
	tv.tv_usec = microseconds % 1000000;
	FD_ZERO(&fd_select);
	FD_SET(my_socket, &fd_select);
	v_n_get_current_time(&s1, &f1);
	select(1, &fd_select, NULL, NULL, &tv);
	v_n_get_current_time(&s2, &f2);
	return (unsigned int) (1000000 * (s2 - s1) + (1000000.0 / 0xffffffffu) * (long) (f2 - f1));	/* Must cast to (long) for f1 > f2 case! */
}

#endif

int v_n_receive_data(VNetworkAddress *address, char *data, size_t length)
{
	struct	sockaddr_in address_in;
	socklen_t from_length = sizeof address_in;
	size_t	len;

	if(v_n_socket_create() == -1)
		return 0;
	memset(&address_in, 0, sizeof address_in);
	address_in.sin_family = AF_INET;
	address_in.sin_port = htons(my_port); 
	address_in.sin_addr.s_addr = INADDR_ANY;
	len = recvfrom(v_n_socket_create(), data, length, 0, (struct sockaddr *) &address_in, &from_length);
	if(len > 0)
	{
		address->ip   = ntohl(address_in.sin_addr.s_addr);
		address->port = ntohs(address_in.sin_port);
	}
	return len;
}

#if defined _WIN32

void v_n_get_current_time(uint32 *seconds, uint32 *fractions)
{
	static LARGE_INTEGER frequency;
	static boolean init = FALSE;
	LARGE_INTEGER counter;

	if(!init)
	{
		init = TRUE;
		QueryPerformanceFrequency(&frequency);
	}

	QueryPerformanceCounter(&counter);
	if(seconds != NULL)
		*seconds = (uint32) (counter.QuadPart / frequency.QuadPart);
	if(fractions != NULL)
		*fractions = (uint32) ((0xffffffffUL * (counter.QuadPart % frequency.QuadPart)) / frequency.QuadPart);
}

#else

void v_n_get_current_time(uint32 *seconds, uint32 *fractions)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	if(seconds != NULL)
	    *seconds = tv.tv_sec;
	if(fractions != NULL)
		*fractions = tv.tv_usec * 1E-6 * (double) (uint32)~0;
}

#endif

void v_n_get_address_string(const VNetworkAddress *address, char *string)
{
	sprintf(string, "%u.%u.%u.%u:%u", address->ip >> 24, (address->ip >> 16) & 0xff,
		(address->ip >> 8) & 0xff, address->ip & 0xff, address->port);
}
