/*
 * vanillaRenderPipe_types.h
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

#ifndef VANILLARENDERPIPE_TYPES_H
#define VANILLARENDERPIPE_TYPES_H

/* Render defines */
#define  RE_MAX_OSA_COUNT 16 /* The max. number of possible oversamples     */
#define  RE_MAX_FACES_PER_PIXEL 500 /* max. nr of faces rendered behind one */
                             /* pixel                                       */

enum RE_SkyAlphaBlendingType {
	RE_ALPHA_NODEF = 0,
	RE_ALPHA_PREMUL,
	RE_ALPHA_KEY,
	RE_ALPHA_SKY,
	RE_ALPHA_MAX
};


/* Render typedefs */
typedef float RE_COLBUFTYPE; /* datatype for the colour buffer              */


/**
 * Threshold for add-blending for faces
 */
#define RE_FACE_ADD_THRESHOLD 0.001

/**
   For oversampling 
   
   New stack: the old stack limits our freedom to do all kinds of
   manipulation, so we rewrite it.
   
   A stacked face needs:
   - a face type
   - a colour
   - a conflict count
   - a data pointer (void*)
  - a mask
  
  The stack starts at index 0, with the closest face, and stacks up.
  
*/

struct RE_faceField {
	int faceType;
	float colour[4];
	int conflictCount;
	void *data;
	int mask;
};

#endif /* VANILLARENDERPIPE_TYPES_H */

