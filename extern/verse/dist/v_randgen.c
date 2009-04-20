/*
 * Random number generator module. Defines a simple API to allocate, use and
 * destroy a generator of randomness. Relies on platform-specific APIs.
*/

#include <stdio.h>
#include <stdlib.h>

#include "v_randgen.h"

#if defined _WIN32

/* This is a fall-back to the old style of simply using rand(). It should
 * be replaced by something using the proper Win32 cryptography APIs.
 * The CryptAcquireContext() and CryptGenRandom() calls sound interesting.
 * 
 * FIXME: Replace ASAP.
*/

VRandGen * v_randgen_new(void)
{
	return (VRandGen *) 1;	/* Anything that isn't NULL. */
}

void v_randgen_get(VRandGen *gen, void *bytes, size_t num)
{
	if(gen != NULL && bytes != NULL)
	{
		unsigned char	*put = bytes, *get;
		size_t	i;
		int	x;

		while(num > 0)
		{
			x = rand();
			get = (unsigned char *) &x;
			for(i = 0; i < sizeof x && num > 0; i++, num--)
				*put++ = *get++;
		}
	}
}

void v_randgen_destroy(VRandGen *gen)
{
	/* Nothing to do here. */
}

#else

/* On non-Win32 platforms (which is Linux and Darwin, at the moment), we
 * read random data from a file, which is assumed to be one of the kernel's
 * virtual files.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct VRandGen {
	int	fd;
};

#define	SOURCE	"/dev/urandom"	/* Name of file to read random bits from. */

VRandGen * v_randgen_new(void)
{
	VRandGen	*gen;

	if((gen = malloc(sizeof *gen)) != NULL)
	{
		gen->fd = open(SOURCE, O_RDONLY);
		if(gen->fd < 0)
		{
			fprintf(stderr, __FILE__ ": Couldn't open " SOURCE " for reading\n");
			free(gen);
			gen = NULL;
		}
	}
	return gen;
}

void v_randgen_get(VRandGen *gen, void *bytes, size_t num)
{
	if(gen != NULL && bytes != NULL)
	{
		if(read(gen->fd, bytes, num) != (int) num)
			fprintf(stderr, __FILE__ ": Failed to read %u bytes of random data from " SOURCE "\n", (unsigned int) num);
	}
}

void v_randgen_destroy(VRandGen *gen)
{
	if(gen != NULL)
	{
		close(gen->fd);
		free(gen);
	}
}

#endif
