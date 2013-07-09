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

#ifndef __AUD_JACKLIBRARY__
#define __AUD_JACKLIBRARY__

#if defined(__APPLE__) // always first include for jack weaklinking !
#include <weakjack.h>
#endif

#include <jack.h>
#include <ringbuffer.h>

#ifdef AUD_JACK_LIBRARY_IMPL
# define JACK_SYM
#else
# define JACK_SYM extern
#endif

/* All loadable Jack sumbols, prototypes from original jack.h */

JACK_SYM jack_transport_state_t (*AUD_jack_transport_query) (
        const jack_client_t *client,
        jack_position_t *pos);

JACK_SYM int (*AUD_jack_transport_locate) (jack_client_t *client,
                                           jack_nframes_t frame);

JACK_SYM void (*AUD_jack_transport_start) (jack_client_t *client);
JACK_SYM void (*AUD_jack_transport_stop) (jack_client_t *client);

JACK_SYM void (*AUD_jack_ringbuffer_reset) (jack_ringbuffer_t *rb);
JACK_SYM size_t (*AUD_jack_ringbuffer_write) (jack_ringbuffer_t *rb,
                                              const char *src, size_t cnt);
JACK_SYM size_t (*AUD_jack_ringbuffer_write_space) (const jack_ringbuffer_t *rb);
JACK_SYM void (*AUD_jack_ringbuffer_write_advance) (jack_ringbuffer_t *rb,
                                                    size_t cnt);
JACK_SYM size_t (*AUD_jack_ringbuffer_read) (jack_ringbuffer_t *rb, char *dest,
                                             size_t cnt);
JACK_SYM jack_ringbuffer_t *(*AUD_jack_ringbuffer_create) (size_t sz);
JACK_SYM void (*AUD_jack_ringbuffer_free) (jack_ringbuffer_t *rb);
JACK_SYM size_t (*AUD_jack_ringbuffer_read_space) (const jack_ringbuffer_t *rb);
JACK_SYM int  (*AUD_jack_set_sync_callback) (jack_client_t *client,
     JackSyncCallback sync_callback,
     void *arg);

JACK_SYM void *(*AUD_jack_port_get_buffer) (jack_port_t *, jack_nframes_t);

JACK_SYM jack_client_t *(*AUD_jack_client_open) (const char *client_name,
                                                 jack_options_t options,
                                                 jack_status_t *status, ...);
JACK_SYM int (*AUD_jack_set_process_callback) (jack_client_t *client,
       JackProcessCallback process_callback, void *arg);
JACK_SYM void (*AUD_jack_on_shutdown) (jack_client_t *client,
       JackShutdownCallback function, void *arg);
JACK_SYM jack_port_t *(*AUD_jack_port_register) (jack_client_t *client,
                                                 const char *port_name,
                                                 const char *port_type,
                                                 unsigned long flags,
                                                 unsigned long buffer_size);
JACK_SYM int (*AUD_jack_client_close) (jack_client_t *client);
JACK_SYM jack_nframes_t (*AUD_jack_get_sample_rate) (jack_client_t *);
JACK_SYM int (*AUD_jack_activate) (jack_client_t *client);
JACK_SYM const char **(*AUD_jack_get_ports) (jack_client_t *, 
                                             const char *port_name_pattern,
                                             const char *type_name_pattern, 
                                             unsigned long flags);
JACK_SYM const char *(*AUD_jack_port_name) (const jack_port_t *port);
JACK_SYM int (*AUD_jack_connect) (jack_client_t *,
                                  const char *source_port,
                                  const char *destination_port);

/* Public API */

void AUD_jack_init(void);
void AUD_jack_exit(void);
bool AUD_jack_supported(void);

#endif  // __AUD_JACKLIBRARY__
