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
#include <BKE_armature.h>
#include <BKE_curve.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_main.h>
#include <BKE_mesh.h>
#include <BKE_object.h>
#include <BKE_scene.h>
#include <BLI_arithb.h>
#include <BLI_blenlib.h>
#include <DNA_armature_types.h>
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
static PyObject *M_Object_New(PyObject *self, PyObject *args);
static PyObject *M_Object_Get(PyObject *self, PyObject *args);
static PyObject *M_Object_GetSelected (PyObject *self, PyObject *args);

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
struct C_Object;

typedef struct {
    PyObject_HEAD
    struct Object   * object;

    /* points to the data. This only is set when there's a valid PyObject */
    /* that points to the linked data. */
    PyObject        * data;

    /* points to the parent object. This is only set when there's a valid */
    /* PyObject (already created at some point). */
    struct C_Object * parent;

    /* points to the object that is tracking this object. This is only set */
    /* when there's a valid PyObject (already created at some point). */
    struct C_Object * track;
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
        "Clears parent object. Optionally specify:\n\
mode\n\t2: Keep object transform\nfast\n\t>0: Don't update scene \
hierarchy (faster)"},
    {"getData",          (PyCFunction)Object_getData,          METH_NOARGS,
        "Returns the datablock object containing the object's data, \
e.g. Mesh"},
    {"getDeformData",    (PyCFunction)Object_getDeformData,    METH_NOARGS,
        "Returns the datablock object containing the object's deformed \
data.\nCurrently, this is only supported for a Mesh"},
    {"getDeltaLocation", (PyCFunction)Object_getDeltaLocation, METH_NOARGS,
        "Returns the object's delta location (x, y, z)"},
    {"getDrawMode",      (PyCFunction)Object_getDrawMode,      METH_NOARGS,
        "Returns the object draw modes"},
    {"getDrawType",      (PyCFunction)Object_getDrawType,      METH_NOARGS,
        "Returns the object draw type"},
    {"getEuler",         (PyCFunction)Object_getEuler,         METH_NOARGS,
        "Returns the object's rotation as Euler rotation vector\n\
(rotX, rotY, rotZ)"},
    {"getInverseMatrix", (PyCFunction)Object_getInverseMatrix, METH_NOARGS,
        "Returns the object's inverse matrix"},
    {"getLocation",      (PyCFunction)Object_getLocation,      METH_VARARGS,
        "Returns the object's location (x, y, z)"},
    {"getMaterials",     (PyCFunction)Object_getMaterials,     METH_NOARGS,
        "Returns list of materials assigned to the object"},
    {"getMatrix",        (PyCFunction)Object_getMatrix,        METH_NOARGS,
        "Returns the object matrix"},
    {"getParent",        (PyCFunction)Object_getParent,        METH_NOARGS,
        "Returns the object's parent object"},
    {"getTracked",       (PyCFunction)Object_getTracked,       METH_NOARGS,
        "Returns the object's tracked object"},
    {"getType",          (PyCFunction)Object_getType,          METH_NOARGS,
        "Returns type of string of Object"},
    {"link",             (PyCFunction)Object_link,             METH_VARARGS,
        "Links Object with data provided in the argument. The data must \n\
match the Object's type, so you cannot link a Lamp to a Mesh type object."},
    {"makeParent",       (PyCFunction)Object_makeParent,       METH_VARARGS,
        "Makes the object the parent of the objects provided in the \n\
argument which must be a list of valid Objects. Optional extra arguments:\n\
mode:\n\t0: make parent with inverse\n\t1: without inverse\n\
fase:\n\t0: update scene hierarchy automatically\n\t\
don't update scene hierarchy (faster). In this case, you must\n\t\
explicitely update the Scene hierarchy."},
    {"materialUsage",    (PyCFunction)Object_materialUsage,    METH_VARARGS,
        "Determines the way the material is used and returs status.\n\
Possible arguments (provide as strings):\n\
\tData:   Materials assigned to the object's data are shown. (default)\n\
\tObject: Materials assigned to the object are shown."},
    {"setDeltaLocation", (PyCFunction)Object_setDeltaLocation, METH_VARARGS,
        "Sets the object's delta location which must be a vector triple."},
    {"setDrawMode",      (PyCFunction)Object_setDrawMode,      METH_VARARGS,
        "Sets the object's drawing mode. The argument can be a sum of:\n\
2:  axis\n4:  texspace\n8:  drawname\n16: drawimage\n32: drawwire"},
    {"setDrawType",      (PyCFunction)Object_setDrawType,      METH_VARARGS,
        "Sets the object's drawing type. The argument must be one of:\n\
1: Bounding box\n2: Wire\n3: Solid\n4: Shaded\n5: Textured"},
    {"setEuler",         (PyCFunction)Object_setEuler,         METH_VARARGS,
        "Set the object's rotation according to the specified Euler\n\
angles. The argument must be a vector triple"},
    {"setLocation",      (PyCFunction)Object_setLocation,      METH_VARARGS,
        "Set the object's location. The first argument must be a vector\n\
triple."},
    {"setMaterials",     (PyCFunction)Object_setMaterials,     METH_VARARGS,
        "Sets materials. The argument must be a list of valid material\n\
objects."},
    {"shareFrom",        (PyCFunction)Object_shareFrom,        METH_VARARGS,
        "Link data of self with object specified in the argument. This\n\
works only if self and the object specified are of the same type."},
    {0}
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
PyTypeObject Object_Type =
{
    PyObject_HEAD_INIT(NULL)
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
