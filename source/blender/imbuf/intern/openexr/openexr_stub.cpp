/*
 * ***** BEGIN GPLLICENSE BLOCK *****
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
 * Copyright by Gernot Ziegler <gz@lysator.liu.se>.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Austin Benesh, Ton Roosendaal (float, half, speedup, cleanup...).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/openexr/openexr_stub.cpp
 *  \ingroup openexr
 */

#include "openexr_api.h"
#include "openexr_multi.h"
#include "BLI_utildefines.h"  /* UNUSED_VARS */

void   *IMB_exr_get_handle          (void) {return NULL;}
void   *IMB_exr_get_handle_name     (const char *name) {(void)name; return NULL;}
void    IMB_exr_add_channel         (void *handle, const char *layname, const char *passname, const char *view, int xstride, int ystride, float *rect) {  (void)handle; (void)layname; (void)passname; (void)xstride; (void)ystride; (void)rect; }

int     IMB_exr_begin_read          (void *handle, const char *filename, int *width, int *height) { (void)handle; (void)filename; (void)width; (void)height; return 0;}
int     IMB_exr_begin_write         (void *handle, const char *filename, int width, int height, int compress) { (void)handle; (void)filename; (void)width; (void)height; (void)compress; return 0;}
void    IMB_exrtile_begin_write     (void *handle, const char *filename, int mipmap, int width, int height, int tilex, int tiley) { (void)handle; (void)filename; (void)mipmap; (void)width; (void)height; (void)tilex; (void)tiley; }

void    IMB_exr_set_channel         (void *handle, const char *layname, const char *passname, int xstride, int ystride, float *rect) { (void)handle; (void)layname; (void)passname; (void)xstride; (void)ystride; (void)rect; }
float  *IMB_exr_channel_rect        (void *handle, const char *layname, const char *passname, const char *view) { (void)handle; (void)layname; (void)passname; (void)view; return NULL; }

void    IMB_exr_read_channels       (void *handle) { (void)handle; }
void    IMB_exr_write_channels      (void *handle) { (void)handle; }
void    IMB_exrtile_write_channels  (void *handle, int partx, int party, int level, const char *viewname) { UNUSED_VARS(handle, partx, party, level, viewname); }
void    IMB_exrmultiview_write_channels(void *handle, const char *viewname) { UNUSED_VARS(handle, viewname); }
void    IMB_exr_clear_channels  (void *handle) { (void)handle; }

void    IMB_exr_multilayer_convert(
        void *handle, void *base,
        void * (*addview)(void *base, const char *str),
        void * (*addlayer)(void *base, const char *str),
        void (*addpass)(void *base, void *lay, const char *str, float *rect, int totchan,
                        const char *chan_id, const char *view))
{
	UNUSED_VARS(handle, base, addview, addlayer, addpass);
}

void    IMB_exr_multiview_convert(
        void *handle, void *base,
        void (*addview)(void *base, const char *str),
        void (*addbuffer)(void *base, const char *str, struct ImBuf *ibuf, const int frame), const int frame)
{
	UNUSED_VARS(handle, base, addview, addbuffer, frame);
}

bool    IMB_exr_multiview_save(
        struct ImBuf *ibuf, const char *name, const int flags, const size_t totviews,
        const char * (*getview)(void *base, size_t view_id),
        struct ImBuf * (*getbuffer)(void *base, const size_t view_id))
{
	UNUSED_VARS(ibuf, name, flags, totviews, getview, getbuffer);
	return false;
}

void    IMB_exr_close               (void *handle) { (void)handle; }

void    IMB_exr_add_view(void *handle, const char *name) { UNUSED_VARS(handle, name); }
int     IMB_exr_split_token(const char *str, const char *end, const char **token) { UNUSED_VARS(str, end, token); return 1; }
bool    IMB_exr_has_multilayer(void *handle) { UNUSED_VARS(handle); return false; }
bool    IMB_exr_has_singlelayer_multiview(void *handle) { UNUSED_VARS(handle); return false; }
