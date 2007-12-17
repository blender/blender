/*
 * $Id: Camera.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_CAMERA_H
#define EXPP_CAMERA_H

#include <Python.h>
#include "DNA_camera_types.h"

extern PyTypeObject Camera_Type;

#define BPy_Camera_Check(v) \
    ((v)->ob_type == &Camera_Type)	/* for type checking */

/* Python BPy_Camera structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	Camera * camera;

} BPy_Camera;

/*****************************************************************************/
/* Python BPy_Camera defaults:                                               */
/*****************************************************************************/

/* Camera types */

#define EXPP_CAM_TYPE_PERSP 0
#define EXPP_CAM_TYPE_ORTHO 1

/* Camera mode flags */

#define EXPP_CAM_MODE_SHOWLIMITS 1
#define EXPP_CAM_MODE_SHOWMIST   2

/* Camera MIN, MAX values */

#define EXPP_CAM_LENS_MIN         1.0
#define EXPP_CAM_LENS_MAX       250.0
#define EXPP_CAM_SCALE_MIN        0.01f
#define EXPP_CAM_SCALE_MAX     1000.0
#define EXPP_CAM_CLIPSTART_MIN    0.0
#define EXPP_CAM_CLIPSTART_MAX  100.0
#define EXPP_CAM_CLIPEND_MIN      1.0
#define EXPP_CAM_CLIPEND_MAX   5000.0
#define EXPP_CAM_DRAWSIZE_MIN     0.1f
#define EXPP_CAM_DRAWSIZE_MAX    10.0

PyObject *Camera_Init( void );
PyObject *Camera_CreatePyObject( Camera * cam );
Camera *Camera_FromPyObject( PyObject * pyobj );

#endif				/* EXPP_CAMERA_H */
