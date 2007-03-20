/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
/*Include this file for access to vector, quat, matrix, euler, etc...*/

#ifndef EXPP_Geometry_H
#define EXPP_Geometry_H

#include <Python.h>
#include "vector.h"

PyObject *Geometry_Init( void );


/* Box Packer */
typedef struct boxVert {
	float x;
	float y;
	short free;
	
	struct boxPack *trb; /* top right box */
	struct boxPack *blb; /* bottom left box */
	struct boxPack *brb; /* bottom right box */
	struct boxPack *tlb; /* top left box */
	
	/* Store last intersecting boxes here
	 * speedup intersection testing */
	struct boxPack *isect_cache[4];
	
	int index;
} boxVert;


typedef struct boxPack {
	float x;
	float y;
	float w;
	float h;
	int index;
	
	/* Verts this box uses
	 * (BL,TR,TL,BR) / 0,1,2,3 */
	boxVert *v[4];
} boxPack;


#endif				/* EXPP_Geometry_H */
