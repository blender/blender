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

#ifndef EXPP_CAMERA_H
#define EXPP_CAMERA_H

#include <Python.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <DNA_camera_types.h>

#include "constant.h"
#include "gen_utils.h"

/*****************************************************************************/
/* Python C_Camera defaults:                                                 */
/*****************************************************************************/
/* Camera types */

#define EXPP_CAM_TYPE_PERSP 0
#define EXPP_CAM_TYPE_ORTHO 1

/* Camera mode flags */

#define EXPP_CAM_MODE_SHOWLIMITS 1
#define EXPP_CAM_MODE_SHOWMIST   2

/* Camera default and MIN, MAX values */

#define EXPP_CAM_TYPE           EXPP_CAM_TYPE_PERSP
#define EXPP_CAM_MODE           0
#define EXPP_CAM_LENS           35.0
#define EXPP_CAM_LENS_MIN       1.0
#define EXPP_CAM_LENS_MAX       250.0
#define EXPP_CAM_CLIPSTART      0.10
#define EXPP_CAM_CLIPSTART_MIN  0.00
#define EXPP_CAM_CLIPSTART_MAX  100.00
#define EXPP_CAM_CLIPEND        100.0
#define EXPP_CAM_CLIPEND_MIN    1.0
#define EXPP_CAM_CLIPEND_MAX    5000.0
#define EXPP_CAM_DRAWSIZE       0.5
#define EXPP_CAM_DRAWSIZE_MIN   0.1
#define EXPP_CAM_DRAWSIZE_MAX   10.0

/*****************************************************************************/
/* Python API function prototypes for the Camera module.                     */
/*****************************************************************************/
static PyObject *M_Camera_New (PyObject *self, PyObject *args,
                               PyObject *keywords);
static PyObject *M_Camera_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Camera.__doc__                                                    */
/*****************************************************************************/
char M_Camera_doc[] =
"The Blender Camera module\n\n\
This module provides access to **Camera Data** objects in Blender\n\n\
Example::\n\n\
  from Blender import Camera, Object, Scene\n\
  c = Camera.New('ortho')      # create new ortho camera data\n\
  c.lens = 35.0                # set lens value\n\
  cur = Scene.getCurrent()     # get current Scene\n\
  ob = Object.New('Camera')    # make camera object\n\
  ob.link(c)                   # link camera data with this object\n\
  cur.link(ob)                 # link object into scene\n\
  cur.setCurrentCamera(ob)     # make this camera the active\n";

char M_Camera_New_doc[] =
"(type) - return a new Camera object of type \"type\", \
which can be 'persp' or 'ortho'.\n\
() - return a new Camera object of type 'persp'.";

char M_Camera_Get_doc[] =
"(name) - return the camera with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all cameras in the\ncurrent scene.";

/*****************************************************************************/
/* Python method structure definition for Blender.Camera module:             */
/*****************************************************************************/
struct PyMethodDef M_Camera_methods[] = {
  {"New",(PyCFunction)M_Camera_New, METH_VARARGS|METH_KEYWORDS,
          M_Camera_New_doc},
  {"Get",         M_Camera_Get,         METH_VARARGS, M_Camera_Get_doc},
  {"get",         M_Camera_Get,         METH_VARARGS, M_Camera_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Camera structure definition:                                     */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Camera *camera;
} C_Camera;

/*****************************************************************************/
/* Python C_Camera methods declarations:                                     */
/*****************************************************************************/
static PyObject *Camera_getName(C_Camera *self);
static PyObject *Camera_getType(C_Camera *self);
static PyObject *Camera_getMode(C_Camera *self);
static PyObject *Camera_getLens(C_Camera *self);
static PyObject *Camera_getClipStart(C_Camera *self);
static PyObject *Camera_getClipEnd(C_Camera *self);
static PyObject *Camera_getDrawSize(C_Camera *self);
static PyObject *Camera_setName(C_Camera *self, PyObject *args);
static PyObject *Camera_setType(C_Camera *self, PyObject *args);
static PyObject *Camera_setIntType(C_Camera *self, PyObject *args);
static PyObject *Camera_setMode(C_Camera *self, PyObject *args);
static PyObject *Camera_setIntMode(C_Camera *self, PyObject *args);
static PyObject *Camera_setLens(C_Camera *self, PyObject *args);
static PyObject *Camera_setClipStart(C_Camera *self, PyObject *args);
static PyObject *Camera_setClipEnd(C_Camera *self, PyObject *args);
static PyObject *Camera_setDrawSize(C_Camera *self, PyObject *args);

/*****************************************************************************/
/* Python C_Camera methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_Camera_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Camera_getName, METH_NOARGS,
      "() - Return Camera Data name"},
  {"getType", (PyCFunction)Camera_getType, METH_NOARGS,
      "() - Return Camera type - 'persp':0, 'ortho':1"},
  {"getMode", (PyCFunction)Camera_getMode, METH_NOARGS,
      "() - Return Camera mode flags (or'ed value) -\n\t\
'showLimits':1, 'showMist':2"},
  {"getLens", (PyCFunction)Camera_getLens, METH_NOARGS,
      "() - Return Camera lens value"},
  {"getClipStart", (PyCFunction)Camera_getClipStart, METH_NOARGS,
      "() - Return Camera clip start value"},
  {"getClipEnd", (PyCFunction)Camera_getClipEnd, METH_NOARGS,
      "() - Return Camera clip end value"},
  {"getDrawSize", (PyCFunction)Camera_getDrawSize, METH_NOARGS,
      "() - Return Camera draw size value"},
  {"setName", (PyCFunction)Camera_setName, METH_VARARGS,
      "(str) - Change Camera Data name"},
  {"setType", (PyCFunction)Camera_setType, METH_VARARGS,
      "(str) - Change Camera type, which can be 'persp' or 'ortho'"},
  {"setMode", (PyCFunction)Camera_setMode, METH_VARARGS,
      "([str[,str]]) - Set Camera mode flag(s): 'showLimits' and 'showMist'"},
  {"setLens", (PyCFunction)Camera_setLens, METH_VARARGS,
      "(float) - Change Camera lens value"},
  {"setClipStart", (PyCFunction)Camera_setClipStart, METH_VARARGS,
      "(float) - Change Camera clip start value"},
  {"setClipEnd", (PyCFunction)Camera_setClipEnd, METH_VARARGS,
      "(float) - Change Camera clip end value"},
  {"setDrawSize", (PyCFunction)Camera_setDrawSize, METH_VARARGS,
      "(float) - Change Camera draw size value"},
  {0}
};

/*****************************************************************************/
/* Python Camera_Type callback function prototypes:                          */
/*****************************************************************************/
static void CameraDeAlloc (C_Camera *self);
static int CameraPrint (C_Camera *self, FILE *fp, int flags);
static int CameraSetAttr (C_Camera *self, char *name, PyObject *v);
static PyObject *CameraGetAttr (C_Camera *self, char *name);
static PyObject *CameraRepr (C_Camera *self);

/*****************************************************************************/
/* Python Camera_Type helper functions needed by Blender (the Init function) */
/* and Object modules.                                                       */
/*****************************************************************************/
PyObject *M_Camera_Init (void);
PyObject *CameraCreatePyObject (Camera *cam);
Camera *CameraFromPyObject (PyObject *pyobj);
int CameraCheckPyObject (PyObject *pyobj);

/*****************************************************************************/
/* Python Camera_Type structure definition:                                  */
/*****************************************************************************/
static PyTypeObject Camera_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                                      /* ob_size */
  "Camera",                               /* tp_name */
  sizeof (C_Camera),                      /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)CameraDeAlloc,              /* tp_dealloc */
  (printfunc)CameraPrint,                 /* tp_print */
  (getattrfunc)CameraGetAttr,             /* tp_getattr */
  (setattrfunc)CameraSetAttr,             /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)CameraRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_Camera_methods,                       /* tp_methods */
  0,                                      /* tp_members */
};

#endif /* EXPP_CAMERA_H */
