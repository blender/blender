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

#ifndef EXPP_SCENE_H
#define EXPP_SCENE_H

#include <Python.h>
#include <DNA_scene_types.h>

#include "constant.h"
#include "gen_utils.h"
#include "sceneRender.h"

/* The Scene PyType Object defined in Scene.c */
PyTypeObject Scene_Type;

#define BPy_Scene_Check(v) \
    ((v)->ob_type == &Scene_Type) /* for type checking */

/*****************************************************************************/
/* Python BPy_Scene structure definition:                                   */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Scene *scene;

} BPy_Scene;

/*****************************************************************************/
/* Python Scene_Type helper functions needed by Blender (the Init function) */
/* and Object modules.                                                       */
/*****************************************************************************/
PyObject *Scene_Init (void);
PyObject *Scene_CreatePyObject (Scene *cam);
Scene    *Scene_FromPyObject (PyObject *pyobj);
int       Scene_CheckPyObject (PyObject *pyobj);


#endif /* EXPP_SCENE_H */
