/**
 * blenlib/BKE_osa_types.h (mar-2001 nzc)
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
#ifndef BKE_OSA_TYPES_H
#define BKE_OSA_TYPES_H

/*  typedef struct Osa */
typedef struct RE_Osa
{
	float dxco[3], dyco[3];
	float dxlo[3], dylo[3], dxgl[3], dygl[3], dxuv[3], dyuv[3];
	float dxref[3], dyref[3], dxorn[3], dyorn[3];
	float dxno[3], dyno[3], dxview, dyview;
	float dxlv[3], dylv[3];
	float dxwin[3], dywin[3];
	float dxsticky[3], dysticky[3];
} Osa;

/*  extern Osa O;  */
/* This one used to be done in render/extern/include/render.h, because
    memory was allocated in that module. (nzc)*/

#endif
