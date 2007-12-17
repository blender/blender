/* 
 * $Id: Scene.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
#include "DNA_scene_types.h"

/* The Scene PyType Object defined in Scene.c */
extern PyTypeObject Scene_Type;
extern PyTypeObject SceneObSeq_Type;

#define BPy_Scene_Check(v) \
    ((v)->ob_type == &Scene_Type)
#define BPy_SceneObSeq_Check(v) \
    ((v)->ob_type == &SceneObSeq_Type)

/*---------------------------Python BPy_Scene structure definition----------*/
typedef struct {
	PyObject_HEAD 
	Scene * scene; /* libdata must be second */
} BPy_Scene;
/*---------------------------Python BPy_Scene visible prototypes-----------*/
/* Python Scene_Type helper functions needed by Blender (the Init function) and Object modules. */


/* Scene object sequence, iterate on the scene object listbase*/
typedef struct {
	PyObject_VAR_HEAD /* required python macro   */
	BPy_Scene *bpyscene; /* link to the python scene so we can know if its been removed */
	Base *iter; /* so we can iterate over the objects */
	int mode; /*0:all objects, 1:selected objects, 2:user context*/
} BPy_SceneObSeq;


PyObject *Scene_Init( void );
PyObject *Scene_CreatePyObject( Scene * scene );
/*Scene *Scene_FromPyObject( PyObject * pyobj );*/  /* not used yet */

#endif				/* EXPP_SCENE_H */
