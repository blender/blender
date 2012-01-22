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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_vfont_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
 */

#ifndef DNA_VFONT_TYPES_H
#define DNA_VFONT_TYPES_H

#include "DNA_ID.h"

struct PackedFile;
struct VFontData;

typedef struct VFont {
	ID id;
	
	char name[1024]; /* 1024 = FILE_MAX */
	
	struct VFontData *data;
	struct PackedFile * packedfile;
} VFont;

/* *************** FONT ****************** */
#define FO_EDIT			0
#define FO_CURS			1
#define FO_CURSUP		2
#define FO_CURSDOWN		3
#define FO_DUPLI		4
#define FO_PAGEUP		8
#define FO_PAGEDOWN		9
#define FO_SELCHANGE	10

#define FO_BUILTIN_NAME "<builtin>"
#endif

