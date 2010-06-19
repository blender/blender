/**
 * $Id$ 
 *
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
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Ton Roosendaal.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef _OPENEXR_MULTI_H
#define _OPENEXR_MULTI_H

/* experiment with more advanced exr api */

/* Note: as for now openexr only supports 32 chars in channel names.
   This api also supports max 8 channels per pass now. easy to fix! */
#define EXR_LAY_MAXNAME		19
#define EXR_PASS_MAXNAME	11
#define EXR_TOT_MAXNAME		32
#define EXR_PASS_MAXCHAN	8


#ifdef WITH_OPENEXR
void *	IMB_exr_get_handle			(void);
void	IMB_exr_add_channel			(void *handle, const char *layname, const char *passname, int xstride, int ystride, float *rect);

int		IMB_exr_begin_read			(void *handle, char *filename, int *width, int *height);
void	IMB_exr_begin_write			(void *handle, char *filename, int width, int height, int compress);
void	IMB_exrtile_begin_write		(void *handle, char *filename, int mipmap, int width, int height, int tilex, int tiley);

void	IMB_exr_set_channel			(void *handle, char *layname, char *passname, int xstride, int ystride, float *rect);

void	IMB_exr_read_channels		(void *handle);
void	IMB_exr_write_channels		(void *handle);
void	IMB_exrtile_write_channels	(void *handle, int partx, int party, int level);
void	IMB_exrtile_clear_channels	(void *handle);

void    IMB_exr_multilayer_convert	(void *handle, void *base,  
									 void * (*addlayer)(void *base, char *str), 
									 void (*addpass)(void *base, void *lay, char *str, float *rect, int totchan, char *chan_id));

void	IMB_exr_close				(void *handle);


#else

/* ugly... but we only use it on pipeline.c, render module, now */

void *	IMB_exr_get_handle			(void) {return NULL;}
void	IMB_exr_add_channel			(void *handle, const char *layname, const char *channame, int xstride, int ystride, float *rect) {}

int		IMB_exr_begin_read			(void *handle, char *filename, int *width, int *height) {return 0;}
void	IMB_exr_begin_write			(void *handle, char *filename, int width, int height, int compress) {}
void	IMB_exrtile_begin_write		(void *handle, char *filename, int mipmap, int width, int height, int tilex, int tiley) {}

void	IMB_exr_set_channel			(void *handle, char *layname, char *channame, int xstride, int ystride, float *rect) {}

void	IMB_exr_read_channels		(void *handle) {}
void	IMB_exr_write_channels		(void *handle) {}
void	IMB_exrtile_write_channels	(void *handle, int partx, int party, int level) {}
void	IMB_exrtile_clear_channels	(void *handle) {}

void    IMB_exr_multilayer_convert	(void *handle, void *base,  
									 void * (*addlayer)(void *base, char *str), 
									 void (*addpass)(void *base, void *lay, char *str, float *rect, int totchan, char *chan_id)) {}

void	IMB_exr_close				(void *handle) {}

#endif



#endif /* __OPENEXR_MULTI_H */
