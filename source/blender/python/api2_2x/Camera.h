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
#include <DNA_camera_types.h>
#include "constant.h"
#include "gen_utils.h"
#include "modules.h"
#include "bpy_types.h" /* where the BPy_Camera struct is declared */

/*****************************************************************************/
/* Python BPy_Camera defaults:                                               */
/*****************************************************************************/

/* Camera types */

#define EXPP_CAM_TYPE_PERSP 0
#define EXPP_CAM_TYPE_ORTHO 1

/* Camera mode flags */

#define EXPP_CAM_MODE_SHOWLIMITS 1
#define EXPP_CAM_MODE_SHOWMIST   2

/* Camera MIN, MAX values */

#define EXPP_CAM_LENS_MIN         1.0
#define EXPP_CAM_LENS_MAX       250.0
#define EXPP_CAM_CLIPSTART_MIN    0.0
#define EXPP_CAM_CLIPSTART_MAX  100.0
#define EXPP_CAM_CLIPEND_MIN      1.0
#define EXPP_CAM_CLIPEND_MAX   5000.0
#define EXPP_CAM_DRAWSIZE_MIN     0.1
#define EXPP_CAM_DRAWSIZE_MAX    10.0

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
static char M_Camera_doc[] =
"The Blender Camera module\n\
\n\
This module provides access to **Camera Data** objects in Blender\n\
\n\
Example::\n\
\n\
  from Blender import Camera, Object, Scene\n\
  c = Camera.New('ortho')      # create new ortho camera data\n\
  c.lens = 35.0                # set lens value\n\
  cur = Scene.getCurrent()     # get current Scene\n\
  ob = Object.New('Camera')    # make camera object\n\
  ob.link(c)                   # link camera data with this object\n\
  cur.link(ob)                 # link object into scene\n\
  cur.setCurrentCamera(ob)     # make this camera the active";

static char M_Camera_New_doc[] =
"Camera.New (type = 'persp', name = 'CamData'):\n\
        Return a new Camera Data object with the given type and name.";

static char M_Camera_Get_doc[] =
"Camera.Get (name = None):\n\
        Return the camera data with the given 'name', None if not found, or\n\
        Return a list with all Camera Data objects in the current scene,\n\
        if no argument was given.";

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
/* Python BPy_Camera methods declarations:                                   */
/*****************************************************************************/
static PyObject *Camera_getIpo(BPy_Camera *self);
static PyObject *Camera_getName(BPy_Camera *self);
static PyObject *Camera_getType(BPy_Camera *self);
static PyObject *Camera_getMode(BPy_Camera *self);
static PyObject *Camera_getLens(BPy_Camera *self);
static PyObject *Camera_getClipStart(BPy_Camera *self);
static PyObject *Camera_getClipEnd(BPy_Camera *self);
static PyObject *Camera_getDrawSize(BPy_Camera *self);
static PyObject *Camera_setIpo(BPy_Camera *self, PyObject *args);
static PyObject *Camera_clearIpo(BPy_Camera *self);
static PyObject *Camera_setName(BPy_Camera *self, PyObject *args);
static PyObject *Camera_setType(BPy_Camera *self, PyObject *args);
static PyObject *Camera_setIntType(BPy_Camera *self, PyObject *args);
static PyObject *Camera_setMode(BPy_Camera *self, PyObject *args);
static PyObject *Camera_setIntMode(BPy_Camera *self, PyObject *args);
static PyObject *Camera_setLens(BPy_Camera *self, PyObject *args);
static PyObject *Camera_setClipStart(BPy_Camera *self, PyObject *args);
static PyObject *Camera_setClipEnd(BPy_Camera *self, PyObject *args);
static PyObject *Camera_setDrawSize(BPy_Camera *self, PyObject *args);

/*****************************************************************************/
/* Python BPy_Camera methods table:                                          */
/*****************************************************************************/
static PyMethodDef BPy_Camera_methods[] = {
 /* name, method, flags, doc */
  {"getIpo", (PyCFunction)Camera_getIpo, METH_NOARGS,
      "() - Return Camera Data Ipo's"},
  {"getName", (PyCFunction)Camera_getName, METH_NOARGS,
      "() - Return Camera Data name"},
  {"getType", (PyCFunction)Camera_getType, METH_NOARGS,
      "() - Return Camera type - 'persp':0, 'ortho':1"},
  {"getMode", (PyCFunction)Camera_getMode, METH_NOARGS,
      "() - Return Camera mode flags (or'ed value) -\n"
      "     'showLimits':1, 'showMist':2"},
  {"getLens", (PyCFunction)Camera_getLens, METH_NOARGS,
      "() - Return Camera lens value"},
  {"getClipStart", (PyCFunction)Camera_getClipStart, METH_NOARGS,
      "() - Return Camera clip start value"},
  {"getClipEnd", (PyCFunction)Camera_getClipEnd, METH_NOARGS,
      "() - Return Camera clip end value"},
  {"getDrawSize", (PyCFunction)Camera_getDrawSize, METH_NOARGS,
      "() - Return Camera draw size value"},
  {"setIpo", (PyCFunction)Camera_setIpo, METH_VARARGS,
      "(Blender Ipo) - Set Camera Ipo"},
  {"clearIpo", (PyCFunction)Camera_clearIpo, METH_NOARGS,
      "() - Unlink Ipo from this Camera."},
  {"setName", (PyCFunction)Camera_setName, METH_VARARGS,
      "(s) - Set Camera Data name"},
  {"setType", (PyCFunction)Camera_setType, METH_VARARGS,
      "(s) - Set Camera type, which can be 'persp' or 'ortho'"},
  {"setMode", (PyCFunction)Camera_setMode, METH_VARARGS,
      "(<s<,s>>) - Set Camera mode flag(s): 'showLimits' and 'showMist'"},
  {"setLens", (PyCFunction)Camera_setLens, METH_VARARGS,
      "(f) - Set Camera lens value"},
  {"setClipStart", (PyCFunction)Camera_setClipStart, METH_VARARGS,
      "(f) - Set Camera clip start value"},
  {"setClipEnd", (PyCFunction)Camera_setClipEnd, METH_VARARGS,
      "(f) - Set Camera clip end value"},
  {"setDrawSize", (PyCFunction)Camera_setDrawSize, METH_VARARGS,
      "(f) - Set Camera draw size value"},
  {0}
};

/*****************************************************************************/
/* Python Camera_Type callback function prototypes:                          */
/*****************************************************************************/
static void Camera_dealloc (BPy_Camera *self);
static int Camera_setAttr (BPy_Camera *self, char *name, PyObject *v);
static int Camera_compare (BPy_Camera *a, BPy_Camera *b);
static PyObject *Camera_getAttr (BPy_Camera *self, char *name);
static PyObject *Camera_repr (BPy_Camera *self);


#endif /* EXPP_CAMERA_H */
