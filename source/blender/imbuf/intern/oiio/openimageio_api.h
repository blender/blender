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
 * The Original Code is Copyright (C) 2013 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/oiio/openimageio_api.h
 *  \ingroup openimageio
 */


#ifndef __OPENIMAGEIO_API_H__
#define __OPENIMAGEIO_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

struct ImBuf;

int		imb_is_a_photoshop			(const char *name);

int		imb_save_photoshop			(struct ImBuf *ibuf, const char *name, int flags);

struct ImBuf *imb_load_photoshop (const char *name, int flags, char *colorspace);

int OIIO_getVersionHex(void);

#ifdef __cplusplus
}

#endif

#endif  /* __OPENIMAGEIO_API_H__ */
