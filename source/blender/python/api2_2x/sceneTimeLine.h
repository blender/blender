/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joilnen Leite
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#ifndef EXPP_TIMELINE_H
#define EXPP_TIMELINE_H

#include <Python.h>
#include <DNA_scene_types.h>

extern PyTypeObject TimeLine_Type;
#define BPy_TimeLine_Check (v) \
    ((v)->ob_type == &TimeLine_Type)	

typedef struct {
	PyObject_HEAD 
	ListBase *marker_list;
	int sfra, efra;
} BPy_TimeLine;

//---------------------------Python BPy_TimeLine visible prototypes-----------
// Python TimeLine_Type helper functions needed by Blender (the Init function) and Object modules. 


PyObject *TimeLine_Init (void);
PyObject *TimeLine_CreatePyObject (BPy_TimeLine *tl);
BPy_TimeLine *TimeLine_FromPyObject (PyObject * pyobj);

#endif				/* EXPP_TMARKER_H */

