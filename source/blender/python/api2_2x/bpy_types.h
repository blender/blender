/* 
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

#ifndef EXPP_bpy_types_h
#define EXPP_bpy_types_h

#include <Python.h>

#include <DNA_camera_types.h>
#include <DNA_lamp_types.h>
#include <DNA_ipo_types.h>
#include <DNA_meta_types.h>
#include <DNA_effect_types.h>
#include <DNA_curve_types.h>
#include <DNA_world_types.h>

#include "rgbTuple.h"		/* for BPy_rgbTuple */

/*****************************************************************************/
/* Camera Data                                                               */
/*****************************************************************************/
extern PyTypeObject Camera_Type;

#define BPy_Camera_Check(v) \
    ((v)->ob_type == &Camera_Type)	/* for type checking */

/* Python BPy_Camera structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  Camera * camera;

}
BPy_Camera;

/*****************************************************************************/
/* Lamp Data                                                                 */
/*****************************************************************************/
extern PyTypeObject Lamp_Type;

#define BPy_Lamp_Check(v) \
    ((v)->ob_type == &Lamp_Type)	/* for type checking */

/* Python BPy_Lamp structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  Lamp * lamp;
  BPy_rgbTuple *color;
}
BPy_Lamp;

/*****************************************************************************/
/* Ipo Data                                                                 */
/*****************************************************************************/
extern PyTypeObject Ipo_Type;

#define BPy_Ipo_Check(v)  ((v)->ob_type == &Ipo_Type)	/* for type checking */

/* Python BPy_Ipo structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  Ipo * ipo;
}
BPy_Ipo;

/*****************************************************************************/
/* Metaball Data                                                             */
/*****************************************************************************/
extern PyTypeObject Metaball_Type;

#define BPy_Metaball_Check(v) ((v)->ob_type==&Metaball_Type)

/* Python BPy_Metaball structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  MetaBall * metaball;
}
BPy_Metaball;

/*****************************************************************************/
/* Effect Data                                                             */
/*****************************************************************************/
extern PyTypeObject Effect_Type;

#define BPy_Effect_Check(v) ((v)->ob_type==&Effect_Type)

/* Python BPy_Effect structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  Effect * effect;
}
BPy_Effect;

/*****************************************************************************/
/* Wave Data                                                             */
/*****************************************************************************/
extern PyTypeObject Wave_Type;

#define BPy_Wave_Check(v) ((v)->ob_type==&Wave_Type)

/* Python BPy_Wave structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  Effect * wave;
}
BPy_Wave;

/*****************************************************************************/
/* Build Data                                                             */
/*****************************************************************************/
extern PyTypeObject Build_Type;

#define BPy_Build_Check(v) ((v)->ob_type==&Build_Type)

/* Python BPy_Build structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  Effect * build;
}
BPy_Build;

/*****************************************************************************/
/* Particle Data                                                             */
/*****************************************************************************/
extern PyTypeObject Particle_Type;

#define BPy_Particle_Check(v) ((v)->ob_type==&Particle_Type)

/* Python BPy_Particle structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  Effect * particle;
}
BPy_Particle;

/*****************************************************************************/
/* Curve Data                                                             */
/*****************************************************************************/
extern PyTypeObject Curve_Type;

#define BPy_Curve_Check(v) ((v)->ob_type==&Curve_Type)

/* Python BPy_Curve structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  Curve * curve;
}
BPy_Curve;

/*****************************************************************************/
/* World Data                                                             */
/*****************************************************************************/
extern PyTypeObject World_Type;

#define BPy_World_Check(v) ((v)->ob_type==&World_Type)

/* Python BPy_World structure definition */
typedef struct
{
  PyObject_HEAD			/* required py macro */
  World * world;
}
BPy_World;

#endif /* EXPP_bpy_types_h */
