/**
 * blenlib/DNA_camera_types.h (mar-2001 nzc)
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
#ifndef DNA_CAMERA_TYPES_H
#define DNA_CAMERA_TYPES_H

#include "DNA_ID.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct AnimData;
struct Ipo;

typedef struct Camera {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 
	
	short type, flag;
	float passepartalpha, angle;
	float clipsta, clipend;
	float lens, ortho_scale, drawsize;
	float shiftx, shifty;
	
	/* yafray: dof params */
	/* qdn: yafray var 'YF_dofdist' now enabled for defocus composit node as well.
	        The name was not changed so that no other files need to be modified */
	float YF_dofdist, YF_aperture;
	short YF_bkhtype, YF_bkhbias;
	float YF_bkhrot;

	struct Ipo *ipo;			// XXX depreceated... old animation system
	
	struct Object *dof_ob;
} Camera;

/* **************** CAMERA ********************* */

/* type */
#define CAM_PERSP		0
#define CAM_ORTHO		1

/* flag */
#define CAM_SHOWLIMITS	1
#define CAM_SHOWMIST	2
#define CAM_SHOWPASSEPARTOUT	4
#define CAM_SHOWTITLESAFE	8
#define CAM_SHOWNAME		16
#define CAM_ANGLETOGGLE		32
#define CAM_DS_EXPAND		64
#define CAM_PANORAMA		128

/* yafray: dof sampling switch */
#define CAM_YF_NO_QMC	512


#ifdef __cplusplus
}
#endif

#endif
