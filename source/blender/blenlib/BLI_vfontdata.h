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

#ifndef __BLI_VFONTDATA_H__
#define __BLI_VFONTDATA_H__

/** \file BLI_vfontdata.h
 *  \ingroup bli
 *  \brief A structure to represent vector fonts, 
 *   and to load them from PostScript fonts.
 */

#include "DNA_listBase.h"

struct PackedFile;
struct VFont;

typedef struct VFontData {
	struct GHash *characters;
	char name[128];
	float scale;
} VFontData;

typedef struct VChar {
	ListBase nurbsbase;
	unsigned int index;
	float width;
} VChar;

VFontData *BLI_vfontdata_from_freetypefont(struct PackedFile *pf);

VChar *BLI_vfontchar_from_freetypefont(struct VFont *vfont, unsigned long character);

#endif

