/**
 * $Id$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_MESHDATA_TYPES_H
#define DNA_MESHDATA_TYPES_H

struct Bone;

typedef struct MFace {
	unsigned int v1, v2, v3, v4;
	char puno, mat_nr;
	char edcode, flag;
} MFace;

typedef struct MDeformWeight {
	int				def_nr;
	float			weight;
	struct Bone		*data;		/* Runtime: Does not need to be valid in file */
} MDeformWeight;

typedef struct MDeformVert {
	struct MDeformWeight *dw;
	int totweight;
	int reserved1;
} MDeformVert;

typedef struct MVert {
	float	co[3];
	short	no[3];
	char flag, mat_nr;
} MVert;

typedef struct MCol {
	char a, b, g, r;
} MCol;

typedef struct MSticky {
	float co[2];
} MSticky;

#endif
