/* 
 * $Id: Object.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_OBJECT_H
#define EXPP_OBJECT_H

#include <Python.h>
#include "DNA_object_types.h"

/* The Object PyType Object defined in Object.c */
extern PyTypeObject Object_Type;

#define BPy_Object_Check(v) \
    ((v)->ob_type == &Object_Type)	/* for type checking */

/*****************************************************************************/
/* Python BPy_Object structure definition.                                  */
/*****************************************************************************/
typedef struct {
	PyObject_HEAD 
	struct Object *object; /* libdata must be second */
	short realtype;
} BPy_Object;

PyObject *Object_Init( void );
PyObject *Object_CreatePyObject( struct Object *obj );
Object *Object_FromPyObject( PyObject * py_obj );

void Object_updateDag( void *data );

int EXPP_add_obdata( struct Object *object );

#endif				/* EXPP_OBJECT_H */
