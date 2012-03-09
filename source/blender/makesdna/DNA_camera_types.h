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
	
	char type; /* CAM_PERSP or CAM_ORTHO */
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
#define CAM_PERSP		0
#define CAM_ORTHO		1

/* dtx */
#define CAM_DTX_CENTER			1
#define CAM_DTX_CENTER_DIAG		2
#define CAM_DTX_THIRDS			4
#define CAM_DTX_GOLDEN			8
#define CAM_DTX_GOLDEN_TRI_A	16
#define CAM_DTX_GOLDEN_TRI_B	32
#define CAM_DTX_HARMONY_TRI_A	64
#define CAM_DTX_HARMONY_TRI_B	128

/* flag */
#define CAM_SHOWLIMITS	1
#define CAM_SHOWMIST	2
#define CAM_SHOWPASSEPARTOUT	4
#define CAM_SHOWTITLESAFE	8
#define CAM_SHOWNAME		16
#define CAM_ANGLETOGGLE		32
#define CAM_DS_EXPAND		64
#define CAM_PANORAMA		128
#define CAM_SHOWSENSOR		256

/* yafray: dof sampling switch */
/* #define CAM_YF_NO_QMC	512 */ /* depreceated */

/* Sensor fit */
#define CAMERA_SENSOR_FIT_AUTO	0
#define CAMERA_SENSOR_FIT_HOR	1
#define CAMERA_SENSOR_FIT_VERT	2

#define DEFAULT_SENSOR_WIDTH	32.0f
#define DEFAULT_SENSOR_HEIGHT	18.0f

#ifdef __cplusplus
}
#endif

#endif
