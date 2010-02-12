/**
 * @file BLI_vfontdata.h
 * 
 * A structure to represent vector fonts, 
 * and to load them from PostScript fonts.
 * 
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLI_VFONTDATA_H
#define BLI_VFONTDATA_H

#include "DNA_listBase.h"

struct PackedFile;
struct VFont;

#define MAX_VF_CHARS 256

typedef struct VFontData {
	ListBase characters;
	// ListBase nurbsbase[MAX_VF_CHARS];
	// float	    resol[MAX_VF_CHARS];
	// float	    width[MAX_VF_CHARS];
	// float	    *points[MAX_VF_CHARS];
	 char		name[128];	
} VFontData;

typedef struct VChar {
	struct VChar    *next, *prev;
 	ListBase        nurbsbase;
	intptr_t   index;
	float           resol;
	float           width;
	float           *points;
} VChar;

struct TmpFont
{
	struct TmpFont *next, *prev;
	struct PackedFile *pf;
	struct VFont *vfont;
};

/**
 * Construct a new VFontData structure from 
 * Freetype font data in a PackedFile.
 * 
 * @param pf The font data.
 * @retval A new VFontData structure, or NULL
 * if unable to load.
 */
	VFontData*
BLI_vfontdata_from_freetypefont(
	struct PackedFile *pf);

	int
BLI_vfontchar_from_freetypefont(
	struct VFont *vfont, unsigned long character);

#endif

