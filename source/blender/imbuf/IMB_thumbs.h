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
 * The Original Code is Copyright (C) 2007 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Andrea Weikert.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef _IMB_THUMBS_H
#define _IMB_THUMBS_H

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;

/** Thumbnail creation and retrieval according to the 'Thumbnail Management Standard'
 * supported by Gimp, Gnome (Nautilus), KDE etc.
 * Reference: http://jens.triq.net/thumbnail-spec/index.html
 */


typedef enum ThumbSize {
	THB_NORMAL,
	THB_LARGE,
	THB_FAIL
} ThumbSize;

typedef enum ThumbSource {
	THB_SOURCE_IMAGE,
	THB_SOURCE_MOVIE
} ThumbSource;

// IB_imginfo

/* create thumbnail for file and returns new imbuf for thumbnail */
ImBuf* IMB_thumb_create(const char* dir, const char* file, ThumbSize size, ThumbSource source);

/* read thumbnail for file and returns new imbuf for thumbnail */
ImBuf* IMB_thumb_read(const char* dir, const char* file, ThumbSize size);

/* delete all thumbs for the file */
void IMB_thumb_delete(const char* dir, const char* file, ThumbSize size);

/* return the state of the thumb, needed to determine how to manage the thumb */
ImBuf* IMB_thumb_manage(const char* dir, const char* file, ThumbSize size, ThumbSource source);

/* create the necessary dirs to store the thumbnails */
void IMB_thumb_makedirs();


#endif /* _IMB_THUMBS_H */

