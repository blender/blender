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
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_OBJECT_H
#define EXPP_OBJECT_H

#include <Python.h>
#include <stdio.h>

#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_main.h>
#include <BKE_mesh.h>
#include <BKE_object.h>
#include <BLI_arithb.h>
#include <BLI_blenlib.h>
#include <DNA_ID.h>
#include <DNA_ika_types.h>
#include <DNA_listBase.h>
#include <DNA_object_types.h>
#include <DNA_scene_types.h>
#include <DNA_userdef_types.h>
#include <DNA_view3d_types.h>

#include "gen_utils.h"

/*****************************************************************************/
/* Python API function prototypes for the Blender module.                    */
/*****************************************************************************/
PyObject *M_Object_New(PyObject *self, PyObject *args);
PyObject *M_Object_Get(PyObject *self, PyObject *args);
PyObject *M_Object_GetSelected (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Object.__doc__                                                    */
/*****************************************************************************/
char M_Object_doc[] =
"The Blender Object module\n\n\
This module provides access to **Object Data** in Blender.\n";

char M_Object_New_doc[] =
"(type) - Add a new object of type 'type' in the current scene";

char M_Object_Get_doc[] =
"(name) - return the object with the name 'name', returns None if not\
    found.\n\
    If 'name' is not specified, it returns a list of all objects in the\n\
    current scene.";

char M_Object_GetSelected_doc[] =
"() - Returns a list of selected Objects in the active layer(s)\n\
The active object is the first in the list, if visible";

/*****************************************************************************/
/* Python BlenderObject structure definition.                                */
/*****************************************************************************/
typedef struct {
    PyObject_HEAD
    PyObject         *dict;
    struct Object    *object;
} C_BlenObject;

/*****************************************************************************/
/* PythonTypeObject callback function prototypes                             */
/*****************************************************************************/
void ObjectDeAlloc (C_BlenObject *obj);
PyObject* ObjectGetAttr (C_BlenObject *obj, char *name);
int ObjectSetAttr (C_BlenObject *obj, char *name, PyObject *v);

/*****************************************************************************/
/* Python method structure definition.                                       */
/*****************************************************************************/
struct PyMethodDef M_Object_methods[] = {
    {"New",         (PyCFunction)M_Object_New,         METH_VARARGS,
                    M_Object_New_doc},
    {"Get",         (PyCFunction)M_Object_Get,         METH_VARARGS,
                    M_Object_Get_doc},
    {"get",         (PyCFunction)M_Object_Get,         METH_VARARGS,
                    M_Object_Get_doc},
    {"getSelected", (PyCFunction)M_Object_GetSelected, METH_VARARGS,
                    M_Object_GetSelected_doc},
    {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python TypeObject structure definition.                                   */
/*****************************************************************************/
static PyTypeObject object_type =
{
    PyObject_HEAD_INIT(&PyType_Type)
    0,                                /* ob_size */
    "Object",                        /* tp_name */
    sizeof (C_BlenObject),            /* tp_basicsize */
    0,                                /* tp_itemsize */
    /* methods */
    (destructor)ObjectDeAlloc,        /* tp_dealloc */
    0,                                /* tp_print */
    (getattrfunc)ObjectGetAttr,        /* tp_getattr */
    (setattrfunc)ObjectSetAttr,        /* tp_setattr */
    0,                                /* tp_compare */
    0,                                /* tp_repr */
    0,                                /* tp_as_number */
    0,                                /* tp_as_sequence */
    0,                                /* tp_as_mapping */
    0,                                /* tp_as_hash */
};

#endif /* EXPP_OBJECT_H */
