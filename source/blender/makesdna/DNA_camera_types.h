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

/** \file DNA_camera_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_CAMERA_TYPES_H__
#define __DNA_CAMERA_TYPES_H__

#include "DNA_defs.h"

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
	
	char type; /* CAM_PERSP, CAM_ORTHO or CAM_PANO */
	char dtx; /* draw type extra */
	short flag;
	float passepartalpha;
	float clipsta, clipend;
	float lens, ortho_scale, drawsize;
	float sensor_x, sensor_y;
	float shiftx, shifty;

	/* yafray: dof params */
	/* qdn: yafray var 'YF_dofdist' now enabled for defocus composite node as well.
	 * The name was not changed so that no other files need to be modified */
	float YF_dofdist;

	struct Ipo *ipo  DNA_DEPRECATED; /* old animation system, deprecated for 2.5 */
	
	struct Object *dof_ob;

	char sensor_fit;
	char pad[7];
} Camera;

/* **************** CAMERA ********************* */

/* type */
enum {
	CAM_PERSP       = 0,
	CAM_ORTHO       = 1,
	CAM_PANO        = 2,
};

/* dtx */
enum {
	CAM_DTX_CENTER          = (1 << 0),
	CAM_DTX_CENTER_DIAG     = (1 << 1),
	CAM_DTX_THIRDS          = (1 << 2),
	CAM_DTX_GOLDEN          = (1 << 3),
	CAM_DTX_GOLDEN_TRI_A    = (1 << 4),
	CAM_DTX_GOLDEN_TRI_B    = (1 << 5),
	CAM_DTX_HARMONY_TRI_A   = (1 << 6),
	CAM_DTX_HARMONY_TRI_B   = (1 << 7),
};

/* flag */
enum {
	CAM_SHOWLIMITS          = (1 << 0),
	CAM_SHOWMIST            = (1 << 1),
	CAM_SHOWPASSEPARTOUT    = (1 << 2),
	CAM_SHOW_SAFE_MARGINS       = (1 << 3),
	CAM_SHOWNAME            = (1 << 4),
	CAM_ANGLETOGGLE         = (1 << 5),
	CAM_DS_EXPAND           = (1 << 6),
	CAM_PANORAMA            = (1 << 7), /* deprecated */
	CAM_SHOWSENSOR          = (1 << 8),
	CAM_SHOW_SAFE_CENTER    = (1 << 9),
};

#if (DNA_DEPRECATED_GCC_POISON == 1)
#pragma GCC poison CAM_PANORAMA
#endif

/* yafray: dof sampling switch */
/* #define CAM_YF_NO_QMC	512 */ /* deprecated */

/* Sensor fit */
enum {
	CAMERA_SENSOR_FIT_AUTO  = 0,
	CAMERA_SENSOR_FIT_HOR   = 1,
	CAMERA_SENSOR_FIT_VERT  = 2,
};

#define DEFAULT_SENSOR_WIDTH	32.0f
#define DEFAULT_SENSOR_HEIGHT	18.0f

#ifdef __cplusplus
}
#endif

#endif
