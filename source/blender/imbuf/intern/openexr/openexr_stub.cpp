/*
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
 * Copyright by Gernot Ziegler <gz@lysator.liu.se>.
 * All rights reserved.
 */

/** \file \ingroup openexr
 */

#include "openexr_api.h"
#include "openexr_multi.h"

void   *IMB_exr_get_handle          (void) {return NULL;}
void   *IMB_exr_get_handle_name     (const char * /*name*/) { return NULL;}
void    IMB_exr_add_channel         (void * /*handle*/, const char * /*layname*/, const char * /*passname*/, const char * /*view*/,
                                     int /*xstride*/, int /*ystride*/, float * /*rect*/,
                                     bool /*use_half_float*/) { }

int     IMB_exr_begin_read          (void * /*handle*/, const char * /*filename*/, int * /*width*/, int * /*height*/) { return 0;}
int     IMB_exr_begin_write         (void * /*handle*/, const char * /*filename*/, int /*width*/, int /*height*/, int /*compress*/, const struct StampData * /*stamp*/) { return 0;}
void    IMB_exrtile_begin_write     (void * /*handle*/, const char * /*filename*/, int /*mipmap*/, int /*width*/, int /*height*/, int /*tilex*/, int /*tiley*/) { }

void    IMB_exr_set_channel         (void * /*handle*/, const char * /*layname*/, const char * /*passname*/, int /*xstride*/, int /*ystride*/, float * /*rect*/) { }
float  *IMB_exr_channel_rect        (void * /*handle*/, const char * /*layname*/, const char * /*passname*/, const char * /*view*/) { return NULL; }

void    IMB_exr_read_channels       (void * /*handle*/) { }
void    IMB_exr_write_channels      (void * /*handle*/) { }
void    IMB_exrtile_write_channels  (void * /*handle*/, int /*partx*/, int /*party*/, int /*level*/, const char * /*viewname*/, bool /*empty*/) { }
void    IMB_exr_clear_channels  (void * /*handle*/) { }

void    IMB_exr_multilayer_convert(
        void * /*handle*/, void * /*base*/,
        void * (* /*addview*/)(void *base, const char *str),
        void * (* /*addlayer*/)(void *base, const char *str),
        void (* /*addpass*/)(void *base, void *lay, const char *str, float *rect, int totchan,
                             const char *chan_id, const char *view))
{
}

void    IMB_exr_close               (void * /*handle*/) { }

void    IMB_exr_add_view(void * /*handle*/, const char * /*name*/) { }
bool    IMB_exr_has_multilayer(void * /*handle*/) { return false; }
