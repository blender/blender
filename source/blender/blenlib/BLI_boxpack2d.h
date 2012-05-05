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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_BOXPACK2D_H__
#define __BLI_BOXPACK2D_H__

/** \file BLI_boxpack2d.h
 *  \ingroup bli
 */

/* Box Packer */

typedef struct BoxPack {
	float x;
	float y;
	float w;
	float h;
	int index;
	
	/* Verts this box uses
	 * (BL,TR,TL,BR) / 0,1,2,3 */
	struct boxVert *v[4];
} BoxPack;

void BLI_box_pack_2D(BoxPack *boxarray, const int len, float *tot_width, float *tot_height);

#endif

