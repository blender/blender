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

#include <BDR_editobject.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_main.h>
#include <BKE_mesh.h>
#include <BKE_object.h>
#include <BKE_scene.h>
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
#include "modules.h"

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
/* Python C_Object structure definition.                                     */
/*****************************************************************************/
typedef struct {
    PyObject_HEAD
    struct Object    *object;
} C_Object;

/*****************************************************************************/
/* Python method structure definition for Blender.Object module:             */
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
/* Python C_Object methods declarations:                                     */
/*****************************************************************************/
static PyObject *Object_clrParent (C_Object *self, PyObject *args);
static PyObject *Object_getData (C_Object *self);
static PyObject *Object_getDeformData (C_Object *self);
static PyObject *Object_getDeltaLocation (C_Object *self);
static PyObject *Object_getDrawMode (C_Object *self);
static PyObject *Object_getDrawType (C_Object *self);
static PyObject *Object_getEuler (C_Object *self);
static PyObject *Object_getInverseMatrix (C_Object *self);
static PyObject *Object_getLocation (C_Object *self, PyObject *args);
static PyObject *Object_getMaterials (C_Object *self);
static PyObject *Object_getMatrix (C_Object *self);
static PyObject *Object_getParent (C_Object *self);
static PyObject *Object_getTracked (C_Object *self);
static PyObject *Object_getType (C_Object *self);
static PyObject *Object_link (C_Object *self, PyObject *args);
static PyObject *Object_makeParent (C_Object *self, PyObject *args);
static PyObject *Object_materialUsage (C_Object *self, PyObject *args);
static PyObject *Object_setDeltaLocation (C_Object *self, PyObject *args);
static PyObject *Object_setDrawMode (C_Object *self, PyObject *args);
static PyObject *Object_setDrawType (C_Object *self, PyObject *args);
static PyObject *Object_setEuler (C_Object *self, PyObject *args);
static PyObject *Object_setLocation (C_Object *self, PyObject *args);
static PyObject *Object_setMaterials (C_Object *self, PyObject *args);
static PyObject *Object_shareFrom (C_Object *self, PyObject *args);

/*****************************************************************************/
/* Python C_Object methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_Object_methods[] = {
    /* name, method, flags, doc */
    {"clrParent",        (PyCFunction)Object_clrParent,        METH_VARARGS,
        "(x) - "},
    {"getData",          (PyCFunction)Object_getData,          METH_NOARGS,
        "(x) - "},
    {"getDeformData",    (PyCFunction)Object_getDeformData,    METH_NOARGS,
        "(x) - "},
    {"getDeltaLocation", (PyCFunction)Object_getDeltaLocation, METH_NOARGS,
        "(x) - "},
    {"getDrawMode",      (PyCFunction)Object_getDrawMode,      METH_NOARGS,
        "(x) - "},
    {"getDrawType",      (PyCFunction)Object_getDrawType,      METH_NOARGS,
        "(x) - "},
    {"getEuler",         (PyCFunction)Object_getEuler,         METH_NOARGS,
        "(x) - "},
    {"getInverseMatrix", (PyCFunction)Object_getInverseMatrix, METH_NOARGS,
        "(x) - "},
    {"getLocation",      (PyCFunction)Object_getLocation,      METH_VARARGS,
        "(x) - "},
    {"getMaterials",     (PyCFunction)Object_getMaterials,     METH_NOARGS,
        "(x) - "},
    {"getMatrix",        (PyCFunction)Object_getMatrix,        METH_NOARGS,
        "(x) - "},
    {"getParent",        (PyCFunction)Object_getParent,        METH_NOARGS,
        "(x) - "},
    {"getTracked",       (PyCFunction)Object_getTracked,       METH_NOARGS,
        "(x) - "},
    {"getType",          (PyCFunction)Object_getType,          METH_NOARGS,
        "(x) - "},
    {"link",             (PyCFunction)Object_link,             METH_VARARGS,
        "(x) - "},
    {"makeParent",       (PyCFunction)Object_makeParent,       METH_VARARGS,
        "(x) - "},
    {"materialUsage",    (PyCFunction)Object_materialUsage,    METH_VARARGS,
        "(x) - "},
    {"setDeltaLocation", (PyCFunction)Object_setDeltaLocation, METH_VARARGS,
        "(x) - "},
    {"setDrawMode",      (PyCFunction)Object_setDrawMode,      METH_VARARGS,
        "(x) - "},
    {"setDrawType",      (PyCFunction)Object_setDrawType,      METH_VARARGS,
        "(x) - "},
    {"setEuler",         (PyCFunction)Object_setEuler,         METH_VARARGS,
        "(x) - "},
    {"setLocation",      (PyCFunction)Object_setLocation,      METH_VARARGS,
        "(x) - "},
    {"setMaterials",     (PyCFunction)Object_setMaterials,     METH_VARARGS,
        "(x) - "},
    {"shareFrom",        (PyCFunction)Object_shareFrom,        METH_VARARGS,
        "(x) - "},
};

/*****************************************************************************/
/* PythonTypeObject callback function prototypes                             */
/*****************************************************************************/
static void      ObjectDeAlloc (C_Object *obj);
static int       ObjectPrint   (C_Object *obj, FILE *fp, int flags);
static PyObject* ObjectGetAttr (C_Object *obj, char *name);
static int       ObjectSetAttr (C_Object *obj, char *name, PyObject *v);
static PyObject* ObjectRepr    (C_Object *obj);

/*****************************************************************************/
/* Python TypeObject structure definition.                                   */
/*****************************************************************************/
static PyTypeObject object_type =
{
    PyObject_HEAD_INIT(&PyType_Type)
    0,                                /* ob_size */
    "Object",                         /* tp_name */
    sizeof (C_Object),                /* tp_basicsize */
    0,                                /* tp_itemsize */
    /* methods */
    (destructor)ObjectDeAlloc,        /* tp_dealloc */
    (printfunc)ObjectPrint,           /* tp_print */
    (getattrfunc)ObjectGetAttr,       /* tp_getattr */
    (setattrfunc)ObjectSetAttr,       /* tp_setattr */
    0,                                /* tp_compare */
    (reprfunc)ObjectRepr,             /* tp_repr */
    0,                                /* tp_as_number */
    0,                                /* tp_as_sequence */
    0,                                /* tp_as_mapping */
    0,                                /* tp_as_hash */
    0,0,0,0,0,0,
    0,                                /* tp_doc */ 
    0,0,0,0,0,0,
    C_Object_methods,                 /* tp_methods */
    0,                                /* tp_members */
};

#endif /* EXPP_OBJECT_H */
