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
 * Contributor(s): Michel Selten, Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_modules_h
#define EXPP_modules_h

#include <Python.h>

#include <DNA_object_types.h>
#include <DNA_camera_types.h>
#include <DNA_lamp_types.h>
#include <DNA_curve_types.h>
#include <DNA_effect_types.h>
#include <DNA_armature_types.h>
#include <DNA_image_types.h>

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/
extern PyObject *g_blenderdict;


/*****************************************************************************/
/* Module Init functions and Data Object helper functions (used by the       */
/* Object module to work with its .data field for the various Data objs      */
/*****************************************************************************/
void            M_Blender_Init (void);

/* Object itself */
PyObject *      M_Object_Init (void);
PyObject *      M_ObjectCreatePyObject (struct Object *obj);
int             M_ObjectCheckPyObject (PyObject *py_obj);
struct Object * M_ObjectFromPyObject (PyObject *py_obj);

/* Types */
PyObject *      M_Types_Init (void);

/* NMesh Data */
PyObject *      M_NMesh_Init (void);

/* Material */
PyObject *      M_Material_Init (void);

/* Camera Data */
PyObject * M_Camera_Init (void);
PyObject * Camera_createPyObject (struct Camera *cam);
Camera   * Camera_fromPyObject   (PyObject *pyobj);
int        Camera_checkPyObject  (PyObject *pyobj);

/* Lamp Data */
PyObject * M_Lamp_Init (void);
PyObject * Lamp_createPyObject (struct Lamp *lamp);
Lamp     * Lamp_fromPyObject   (PyObject *pyobj);
int        Lamp_checkPyObject  (PyObject *pyobj);

/* Curve Data */
PyObject *      M_Curve_Init (void);
PyObject *      CurveCreatePyObject (struct Curve *curve);
struct Curve *  CurveFromPyObject   (PyObject *py_obj);
int             CurveCheckPyObject  (PyObject *py_obj);

/* Armature Data */
PyObject *         M_Armature_Init (void);
PyObject *         M_ArmatureCreatePyObject (bArmature *armature);
bArmature*         M_ArmatureFromPyObject   (PyObject *py_obj);
int                M_ArmatureCheckPyObject  (PyObject *py_obj);

/* Particle Effects Data */
/*PyObject *      M_Effect_Init (void);
PyObject *      EffectCreatePyObject (struct Effect *effect);
struct Effect * EffectFromPyObject (PyObject *py_obj);
int             EffectCheckPyObject (PyObject *py_obj);
*/

/* Image */
PyObject * M_Image_Init (void);
PyObject * Image_CreatePyObject (Image *image);
int        Image_CheckPyObject (PyObject *pyobj);

/* Init functions for other modules */
PyObject * M_Window_Init (void);
PyObject * M_Draw_Init (void);
PyObject * M_BGL_Init (void);
PyObject * M_Text_Init (void);

#endif /* EXPP_modules_h */
