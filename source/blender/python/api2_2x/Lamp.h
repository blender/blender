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

#ifndef EXPP_LAMP_H
#define EXPP_LAMP_H

#include <Python.h>
#include <stdio.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <DNA_lamp_types.h>

#include "constant.h"
#include "rgbTuple.h"
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python C_Lamp defaults:                                                   */
/*****************************************************************************/

/* Lamp types */

#define EXPP_LAMP_TYPE_LAMP 0
#define EXPP_LAMP_TYPE_SUN  1
#define EXPP_LAMP_TYPE_SPOT 2
#define EXPP_LAMP_TYPE_HEMI 3

/* Lamp mode flags */

#define EXPP_LAMP_MODE_SHADOWS       1
#define EXPP_LAMP_MODE_HALO          2
#define EXPP_LAMP_MODE_LAYER         4
#define EXPP_LAMP_MODE_QUAD          8
#define EXPP_LAMP_MODE_NEGATIVE     16
#define EXPP_LAMP_MODE_ONLYSHADOW   32
#define EXPP_LAMP_MODE_SPHERE       64
#define EXPP_LAMP_MODE_SQUARE      128
#define EXPP_LAMP_MODE_TEXTURE     256
#define EXPP_LAMP_MODE_OSATEX      512
#define EXPP_LAMP_MODE_DEEPSHADOW 1024

/* Lamp MIN, MAX values */

#define EXPP_LAMP_SAMPLES_MIN 1
#define EXPP_LAMP_SAMPLES_MAX 16
#define EXPP_LAMP_BUFFERSIZE_MIN 512
#define EXPP_LAMP_BUFFERSIZE_MAX 5120
#define EXPP_LAMP_ENERGY_MIN  0.0
#define EXPP_LAMP_ENERGY_MAX 10.0
#define EXPP_LAMP_DIST_MIN    0.1
#define EXPP_LAMP_DIST_MAX 5000.0
#define EXPP_LAMP_SPOTSIZE_MIN   1.0
#define EXPP_LAMP_SPOTSIZE_MAX 180.0
#define EXPP_LAMP_SPOTBLEND_MIN 0.00
#define EXPP_LAMP_SPOTBLEND_MAX 1.00
#define EXPP_LAMP_CLIPSTART_MIN    0.1
#define EXPP_LAMP_CLIPSTART_MAX 1000.0
#define EXPP_LAMP_CLIPEND_MIN    1.0
#define EXPP_LAMP_CLIPEND_MAX 5000.0
#define EXPP_LAMP_BIAS_MIN 0.01
#define EXPP_LAMP_BIAS_MAX 5.00
#define EXPP_LAMP_SOFTNESS_MIN   1.0
#define EXPP_LAMP_SOFTNESS_MAX 100.0
#define EXPP_LAMP_HALOINT_MIN 0.0
#define EXPP_LAMP_HALOINT_MAX 5.0
#define EXPP_LAMP_HALOSTEP_MIN  0
#define EXPP_LAMP_HALOSTEP_MAX 12
#define EXPP_LAMP_QUAD1_MIN 0.0
#define EXPP_LAMP_QUAD1_MAX 1.0
#define EXPP_LAMP_QUAD2_MIN 0.0
#define EXPP_LAMP_QUAD2_MAX 1.0
#define EXPP_LAMP_COL_MIN 0.0
#define EXPP_LAMP_COL_MAX 1.0

/*****************************************************************************/
/* Python API function prototypes for the Lamp module.                       */
/*****************************************************************************/
static PyObject *M_Lamp_New (PyObject *self, PyObject *args, PyObject *keywords);
static PyObject *M_Lamp_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Lamp.__doc__                                                      */
/*****************************************************************************/
char M_Lamp_doc[] =
"The Blender Lamp module\n\n\
This module provides control over **Lamp Data** objects in Blender.\n\n\
Example::\n\n\
  from Blender import Lamp\n\
  l = Lamp.New('Spot')\n\
  l.setMode('square', 'shadow')\n\
  ob = Object.New('Lamp')\n\
  ob.link(l)\n";

char M_Lamp_New_doc[] =
"(type, name) - return a new Lamp datablock of type 'type'\n\
                and optional name 'name'.";

char M_Lamp_Get_doc[] =
"(name) - return the lamp with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all lamps in the\ncurrent scene.";

/*****************************************************************************/
/* Python method structure definition for Blender.Lamp module:               */
/*****************************************************************************/
struct PyMethodDef M_Lamp_methods[] = {
  {"New",(PyCFunction)M_Lamp_New, METH_VARARGS|METH_KEYWORDS,
          M_Lamp_New_doc},
  {"Get",         M_Lamp_Get,         METH_VARARGS, M_Lamp_Get_doc},
  {"get",         M_Lamp_Get,         METH_VARARGS, M_Lamp_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Lamp structure definition:                                       */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Lamp *lamp;
	C_rgbTuple *color;

} C_Lamp;

/*****************************************************************************/
/* Python C_Lamp methods declarations:                                     */
/*****************************************************************************/
static PyObject *Lamp_getName(C_Lamp *self);
static PyObject *Lamp_getType(C_Lamp *self);
static PyObject *Lamp_getMode(C_Lamp *self);
static PyObject *Lamp_getSamples(C_Lamp *self);
static PyObject *Lamp_getBufferSize(C_Lamp *self);
static PyObject *Lamp_getHaloStep(C_Lamp *self);
static PyObject *Lamp_getEnergy(C_Lamp *self);
static PyObject *Lamp_getDist(C_Lamp *self);
static PyObject *Lamp_getSpotSize(C_Lamp *self);
static PyObject *Lamp_getSpotBlend(C_Lamp *self);
static PyObject *Lamp_getClipStart(C_Lamp *self);
static PyObject *Lamp_getClipEnd(C_Lamp *self);
static PyObject *Lamp_getBias(C_Lamp *self);
static PyObject *Lamp_getSoftness(C_Lamp *self);
static PyObject *Lamp_getHaloInt(C_Lamp *self);
static PyObject *Lamp_getQuad1(C_Lamp *self);
static PyObject *Lamp_getQuad2(C_Lamp *self);
static PyObject *Lamp_getCol(C_Lamp *self);
static PyObject *Lamp_setName(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setType(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setIntType(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setMode(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setIntMode(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setSamples(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setBufferSize(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setHaloStep(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setEnergy(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setDist(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setSpotSize(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setSpotBlend(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setClipStart(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setClipEnd(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setBias(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setSoftness(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setHaloInt(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setQuad1(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setQuad2(C_Lamp *self, PyObject *args);
static PyObject *Lamp_setCol(C_Lamp *self, PyObject *args);

static PyObject *Lamp_setColorComponent(C_Lamp *self, char *key,
                PyObject *args);

/*****************************************************************************/
/* Python C_Lamp methods table:                                              */
/*****************************************************************************/
static PyMethodDef C_Lamp_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Lamp_getName, METH_NOARGS,
          "() - return Lamp name"},
  {"getType", (PyCFunction)Lamp_getType, METH_NOARGS,
          "() - return Lamp type - 'Lamp':0, 'Sun':1, 'Spot':2, 'Hemi':3"},
  {"getMode", (PyCFunction)Lamp_getMode, METH_NOARGS,
          "() - return Lamp mode flags (or'ed value)"},
  {"getSamples", (PyCFunction)Lamp_getSamples, METH_NOARGS,
          "() - return Lamp samples value"},
  {"getBufferSize", (PyCFunction)Lamp_getBufferSize, METH_NOARGS,
          "() - return Lamp buffer size value"},
  {"getHaloStep", (PyCFunction)Lamp_getHaloStep, METH_NOARGS,
          "() - return Lamp halo step value"},
  {"getEnergy", (PyCFunction)Lamp_getEnergy, METH_NOARGS,
          "() - return Lamp energy value"},
  {"getDist", (PyCFunction)Lamp_getDist, METH_NOARGS,
          "() - return Lamp clipping distance value"},
  {"getSpotSize", (PyCFunction)Lamp_getSpotSize, METH_NOARGS,
          "() - return Lamp spot size value"},
  {"getSpotBlend", (PyCFunction)Lamp_getSpotBlend, METH_NOARGS,
          "() - return Lamp spot blend value"},
  {"getClipStart", (PyCFunction)Lamp_getClipStart, METH_NOARGS,
          "() - return Lamp clip start value"},
  {"getClipEnd", (PyCFunction)Lamp_getClipEnd, METH_NOARGS,
          "() - return Lamp clip end value"},
  {"getBias", (PyCFunction)Lamp_getBias, METH_NOARGS,
          "() - return Lamp bias value"},
  {"getSoftness", (PyCFunction)Lamp_getSoftness, METH_NOARGS,
          "() - return Lamp softness value"},
  {"getHaloInt", (PyCFunction)Lamp_getHaloInt, METH_NOARGS,
          "() - return Lamp halo intensity value"},
  {"getQuad1", (PyCFunction)Lamp_getQuad1, METH_NOARGS,
          "() - return light intensity value #1 for a Quad Lamp"},
  {"getQuad2", (PyCFunction)Lamp_getQuad2, METH_NOARGS,
          "() - return light intensity value #2 for a Quad Lamp"},
  {"getCol", (PyCFunction)Lamp_getCol, METH_NOARGS,
          "() - return light rgb color triplet"},
  {"setName", (PyCFunction)Lamp_setName, METH_VARARGS,
          "(str) - rename Lamp"},
  {"setType", (PyCFunction)Lamp_setType, METH_VARARGS,
          "(str) - change Lamp type, which can be 'persp' or 'ortho'"},
  {"setMode", (PyCFunction)Lamp_setMode, METH_VARARGS,
          "([up to eight str's]) - Set Lamp mode flag(s)"},
  {"setSamples", (PyCFunction)Lamp_setSamples, METH_VARARGS,
          "(int) - change Lamp samples value"},
  {"setBufferSize", (PyCFunction)Lamp_setBufferSize, METH_VARARGS,
          "(int) - change Lamp buffer size value"},
  {"setHaloStep", (PyCFunction)Lamp_setHaloStep, METH_VARARGS,
          "(int) - change Lamp halo step value"},
  {"setEnergy", (PyCFunction)Lamp_setEnergy, METH_VARARGS,
          "(float) - change Lamp energy value"},
  {"setSpotSize", (PyCFunction)Lamp_setSpotSize, METH_VARARGS,
          "(float) - change Lamp spot size value"},
  {"setSpotBlend", (PyCFunction)Lamp_setHaloStep, METH_VARARGS,
          "(float) - change Lamp spot blend value"},
  {"setClipStart", (PyCFunction)Lamp_setClipStart, METH_VARARGS,
          "(float) - change Lamp clip start value"},
  {"setClipEnd", (PyCFunction)Lamp_setClipEnd, METH_VARARGS,
          "(float) - change Lamp clip end value"},
  {"setBias", (PyCFunction)Lamp_setBias, METH_VARARGS,
          "(float) - change Lamp draw size value"},
  {"setSoftness", (PyCFunction)Lamp_setSoftness, METH_VARARGS,
          "(float) - change Lamp softness value"},
  {"setHaloInt", (PyCFunction)Lamp_setHaloInt, METH_VARARGS,
          "(float) - change Lamp halo intensity value"},
  {"setQuad1", (PyCFunction)Lamp_setQuad1, METH_VARARGS,
          "(float) - change light intensity value #1 for a Quad Lamp"},
  {"setQuad2", (PyCFunction)Lamp_setQuad2, METH_VARARGS,
          "(float) - change light intensity value #2 for a Quad Lamp"},
  {"setCol", (PyCFunction)Lamp_setCol, METH_VARARGS,
          "(f,f,f) or ([f,f,f]) - change light's rgb color triplet"},
  {0}
};

/*****************************************************************************/
/* Python TypeLamp callback function prototypes:                             */
/*****************************************************************************/
static void LampDeAlloc (C_Lamp *lamp);
static PyObject *LampGetAttr (C_Lamp *lamp, char *name);
static int LampSetAttr (C_Lamp *lamp, char *name, PyObject *v);
static int LampCompare (C_Lamp *a, C_Lamp *b);
static PyObject *LampRepr (C_Lamp *lamp);
static int LampPrint (C_Lamp *lamp, FILE *fp, int flags);

/*****************************************************************************/
/* Python Lamp_Type helper functions needed by Blender (the Init function)   */
/* and Object modules.                                                       */
/*****************************************************************************/
PyObject *M_Lamp_Init (void);
PyObject *LampCreatePyObject (Lamp *lamp);
Lamp *LampFromPyObject (PyObject *pyobj);
int LampCheckPyObject (PyObject *pyobj);

/*****************************************************************************/
/* Python TypeLamp structure definition:                                     */
/*****************************************************************************/
PyTypeObject Lamp_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                    /* ob_size */
  "Lamp",                               /* tp_name */
  sizeof (C_Lamp),                      /* tp_basicsize */
  0,                                    /* tp_itemsize */
  /* methods */
  (destructor)LampDeAlloc,              /* tp_dealloc */
  (printfunc)LampPrint,                 /* tp_print */
  (getattrfunc)LampGetAttr,             /* tp_getattr */
  (setattrfunc)LampSetAttr,             /* tp_setattr */
  (cmpfunc)LampCompare,                 /* tp_compare */
  (reprfunc)LampRepr,                   /* tp_repr */
  0,                                    /* tp_as_number */
  0,                                    /* tp_as_sequence */
  0,                                    /* tp_as_mapping */
  0,                                    /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                    /* tp_doc */ 
  0,0,0,0,0,0,
  C_Lamp_methods,                       /* tp_methods */
  0,                                    /* tp_members */
};

#endif /* EXPP_LAMP_H */
