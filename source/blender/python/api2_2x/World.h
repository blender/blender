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
 * Contributor(s): Jacques Guignot
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_WORLD_H
#define EXPP_WORLD_H

#include <Python.h>


#include "constant.h"
#include "gen_utils.h"
#include "bpy_types.h"
#include "modules.h"


/*****************************************************************************/
/* Python API function prototypes for the World module.                     */
/*****************************************************************************/
static PyObject *M_World_New (PyObject *self, PyObject *args,
                               PyObject *keywords);
static PyObject *M_World_Get (PyObject *self, PyObject *args);


/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.World.__doc__                                                     */
/*****************************************************************************/
static char M_World_doc[] = 
"The Blender World module\n\n\
This module provides access to **World Data** objects in Blender\n\n";

static char M_World_New_doc[] ="() - return a new World object";

static char M_World_Get_doc[] ="(name) - return the world with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all worlds in the\ncurrent scene.";



/*****************************************************************************/
/* Python method structure definition for Blender.World module:              */
/*****************************************************************************/
struct PyMethodDef M_World_methods[] = {
  {"New",(PyCFunction)M_World_New, METH_VARARGS|METH_KEYWORDS,M_World_New_doc},
  {"Get",         M_World_Get,         METH_VARARGS, M_World_Get_doc},
  {"get",         M_World_Get,         METH_VARARGS, M_World_Get_doc},
  {NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Python BPy_World methods declarations:                                   */
/*****************************************************************************/
static PyObject *World_getIpo(BPy_World *self);
static PyObject *World_setIpo(BPy_World *self, PyObject *args);
static PyObject *World_clearIpo(BPy_World *self);
static PyObject *World_getName(BPy_World *self);
static PyObject *World_setName(BPy_World *self, PyObject *args);
static PyObject *World_getSkytype(BPy_World *self);
static PyObject *World_setSkytype(BPy_World *self, PyObject *args );
static PyObject *World_getMistype(BPy_World *self);
static PyObject *World_setMistype(BPy_World *self, PyObject *args );
static PyObject *World_getHor(BPy_World *self);
static PyObject *World_setHor(BPy_World *self, PyObject *args );
static PyObject *World_getZen(BPy_World *self);
static PyObject *World_setZen(BPy_World *self, PyObject *args );
static PyObject *World_getAmb(BPy_World *self);
static PyObject *World_setAmb(BPy_World *self, PyObject *args );
static PyObject *World_getStar(BPy_World *self);
static PyObject *World_setStar(BPy_World *self, PyObject *args );
static PyObject *World_getMist(BPy_World *self);
static PyObject *World_setMist(BPy_World *self, PyObject *args );

/*****************************************************************************/
/* Python BPy_World methods table:                                          */
/*****************************************************************************/
static PyMethodDef BPy_World_methods[] = {
  {"getIpo", (PyCFunction)World_getIpo, METH_NOARGS,
      "() - Return World Ipo"},
  {"setIpo", (PyCFunction)World_setIpo, METH_VARARGS,
      "() - Change this World's ipo"},
  {"clearIpo", (PyCFunction)World_clearIpo, METH_VARARGS,
      "() - Unlink Ipo from this World"},
  {"getName", (PyCFunction)World_getName, METH_NOARGS,
      "() - Return World Data name"},
  {"setName", (PyCFunction)World_setName, METH_VARARGS,
      "() - Return World Data name"},
  {"getSkytype", (PyCFunction)World_getSkytype, METH_NOARGS,
      "() - Return World Data skytype"},
  {"setSkytype", (PyCFunction)World_setSkytype, METH_VARARGS,
      "() - Return World Data skytype"},
  {"getMistype", (PyCFunction)World_getMistype, METH_NOARGS,
      "() - Return World Data mistype"},
  {"setMistype", (PyCFunction)World_setMistype, METH_VARARGS,
      "() - Return World Data mistype"},
  {"getHor", (PyCFunction)World_getHor, METH_NOARGS,
      "() - Return World Data hor"},
  {"setHor", (PyCFunction)World_setHor, METH_VARARGS,
      "() - Return World Data hor"},
  {"getZen", (PyCFunction)World_getZen, METH_NOARGS,
      "() - Return World Data zen"},
  {"setZen", (PyCFunction)World_setZen, METH_VARARGS,
      "() - Return World Data zen"},
  {"getAmb", (PyCFunction)World_getAmb, METH_NOARGS,
      "() - Return World Data amb"},
  {"setAmb", (PyCFunction)World_setAmb, METH_VARARGS,
      "() - Return World Data amb"},
  {"getStar", (PyCFunction)World_getStar, METH_NOARGS,
      "() - Return World Data star"},
  {"setStar", (PyCFunction)World_setStar, METH_VARARGS,
      "() - Return World Data star"},
  {"getMist", (PyCFunction)World_getMist, METH_NOARGS,
      "() - Return World Data mist"},
  {"setMist", (PyCFunction)World_setMist, METH_VARARGS,
      "() - Return World Data mist"},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python World_Type helper functions needed by Blender (the Init function) */
/* and Object modules.                                                       */
/*****************************************************************************/
PyObject *World_Init (void);
PyObject *World_CreatePyObject (World *cam);
World   *World_FromPyObject (PyObject *pyobj);
int       World_CheckPyObject (PyObject *pyobj);


#endif /* EXPP_WORLD_H */
