/**
 * blenlib/DNA_mesh_types.h (mar-2001 nzc)
 *
 * Mesh stuff.
 *
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
#ifndef DNA_LATTICE_TYPES_H
#define DNA_LATTICE_TYPES_H

#include "DNA_ID.h"

struct BPoint;
struct Ipo;
struct Key;

typedef struct Lattice {
	ID id;
	
	short pntsu, pntsv, pntsw, flag;
	char typeu, typev, typew, type;
	int pad;
	
	struct BPoint *def;
	
	struct Ipo *ipo;
	struct Key *key;
	
} Lattice;

/* ***************** LATTICE ********************* */

/* flag */
#define LT_GRID		1
#define LT_OUTSIDE	2

#endif
