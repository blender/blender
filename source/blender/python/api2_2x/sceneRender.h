/* 
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
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_SCENERENDER_H
#define EXPP_SCENERENDER_H

#include <Python.h>
#include "DNA_scene_types.h"

PyTypeObject RenderLayer_Type;

#define BPy_RenderData_Check(v)       ((v)->ob_type == &RenderData_Type)
#define BPy_RenderLayer_Check(v)       ((v)->ob_type == &RenderLayer_Type)

//------------------------------------Struct definitions-------
typedef struct {
	PyObject_HEAD 
	struct RenderData *renderContext;
	struct Scene *scene;
} BPy_RenderData;

typedef struct {
	PyObject_HEAD 
	struct SceneRenderLayer *renderLayer; /* this is totally weak, pointer can become invalid */
	struct Scene *scene;
} BPy_RenderLayer;

//------------------------------------Visible prototypes-------
PyObject *Render_Init( void );

PyObject *RenderData_CreatePyObject( struct Scene *scene );
PyObject *RenderLayer_CreatePyObject( struct Scene *scene, struct SceneRenderLayer * renderLayer );
#endif				/* EXPP_SCENERENDER_H */
