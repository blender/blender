/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2013 Blender Foundation
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/jack/AUD_JackLibrary.cpp
 *  \ingroup audjack
 */

#define AUD_JACK_LIBRARY_IMPL

#include "AUD_JackLibrary.h"

#ifdef WITH_JACK_DYNLOAD
# include <dlfcn.h>
# include <stdio.h>
#endif

#ifdef WITH_JACK_DYNLOAD
static void *jack_handle = NULL;
#endif

static bool jack_supported = false;

void AUD_jack_init(void)
{
#ifdef WITH_JACK_DYNLOAD
	jack_handle = dlopen("libjack.so", RTLD_LAZY);

	if (!jack_handle) {
		return;
	}

# define JACK_SYMBOL(sym) \
	{ \
		char *error; \
		*(void **) (&(AUD_##sym)) = dlsym(jack_handle, #sym); \
		if ((error = dlerror()) != NULL)  { \
			fprintf(stderr, "%s\n", error); \
			return; \
		} \
	} (void)0

	dlerror();    /* Clear any existing error */
#else  // WITH_JACK_DYNLOAD
# define JACK_SYMBOL(sym) AUD_##sym = sym
#endif  // WITH_JACK_DYNLOAD

	JACK_SYMBOL(jack_transport_query);
	JACK_SYMBOL(jack_transport_locate);

	JACK_SYMBOL(jack_transport_start);
	JACK_SYMBOL(jack_transport_stop);

	JACK_SYMBOL(jack_ringbuffer_reset);
	JACK_SYMBOL(jack_ringbuffer_write);
	JACK_SYMBOL(jack_ringbuffer_write_space);
	JACK_SYMBOL(jack_ringbuffer_write_advance);
	JACK_SYMBOL(jack_ringbuffer_read);
	JACK_SYMBOL(jack_ringbuffer_create);
	JACK_SYMBOL(jack_ringbuffer_free);
	JACK_SYMBOL(jack_ringbuffer_read_space);
	JACK_SYMBOL(jack_set_sync_callback);

	JACK_SYMBOL(jack_port_get_buffer);

	JACK_SYMBOL(jack_client_open);
	JACK_SYMBOL(jack_set_process_callback);
	JACK_SYMBOL(jack_on_shutdown);
	JACK_SYMBOL(jack_port_register);
	JACK_SYMBOL(jack_client_close);
	JACK_SYMBOL(jack_get_sample_rate);
	JACK_SYMBOL(jack_activate);
	JACK_SYMBOL(jack_get_ports);
	JACK_SYMBOL(jack_port_name);
	JACK_SYMBOL(jack_connect);

	jack_supported = true;

#undef JACK_SYMBOL
}

void AUD_jack_exit(void)
{
#ifdef WITH_JACK_DYNLOAD
	if (jack_handle) {
		dlclose(jack_handle);
	}
#endif
	jack_supported = false;
}

bool AUD_jack_supported(void)
{
	return jack_supported;
}
