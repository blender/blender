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

#ifndef EXPP_BUILD_H
#define EXPP_BUILD_H

#include <Python.h>
#include <stdio.h>

#include <BLI_arithb.h>
#include <BLI_blenlib.h>
#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <DNA_effect_types.h>

#include "gen_utils.h"
#include "bpy_types.h"
#include "Effect.h"

/*****************************************************************************/
/* Python API function prototypes for the Build module.                      */
/*****************************************************************************/
PyObject *M_Build_New (PyObject *self, PyObject *args);
PyObject *M_Build_Get (PyObject *self, PyObject *args);
/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Camera.__doc__                                                    */
/*****************************************************************************/

static char M_Build_doc[] =
"The Blender Build module\n\
This module provides access to **Build Data** objects in Blender\n";

static char M_Build_New_doc[] =
"Build.New ():\n\
        Return a new Build Data object with the given type and name.";

static char M_Build_Get_doc[] =
"Build.Get (name = None):\n\
        Return the build data with the given 'name', None if not found, or\n\
        Return a list with all Build Data objects in the current scene,\n\
        if no argument was given.";

/*****************************************************************************/
/* Python method structure definition for Blender.Build module:              */
/*****************************************************************************/
/*
struct PyMethodDef M_Build_methods[] = {
  {"New",(PyCFunction)M_Build_New, METH_VARARGS, 0},
  {"Get",         M_Build_Get,         METH_VARARGS, 0},
  {"get",         M_Build_Get,         METH_VARARGS,  0},
  {NULL, NULL, 0, NULL}
};
*/
/*****************************************************************************/
/* Python BPy_Build methods declarations:                                      */
/*****************************************************************************/
PyObject *Build_getLen(BPy_Build *self);
PyObject *Build_setLen(BPy_Build *self,PyObject*a);
PyObject *Build_getSfra(BPy_Build *self);
PyObject *Build_setSfra(BPy_Build *self,PyObject*a);

/*****************************************************************************/
/* Python BPy_Build methods table:                                             */
/*****************************************************************************/
static PyMethodDef BPy_Build_methods[] = {
  {"getLen",(PyCFunction)Build_getLen,
	 METH_NOARGS,"()-Return Build len"},
  {"setLen",(PyCFunction)Build_setLen, METH_VARARGS,
	 "()- Sets Build len"},
  {"getSfra",(PyCFunction)Build_getSfra,
	 METH_NOARGS,"()-Return Build sfra"},
  {"setSfra",(PyCFunction)Build_setSfra, METH_VARARGS,
	 "()- Sets Build sfra"},
	{0}
};


/*****************************************************************************/
/* Python Build_Type callback function prototypes:                           */
/*****************************************************************************/
void BuildDeAlloc (BPy_Build *msh);
//int BuildPrint (BPy_Build *msh, FILE *fp, int flags);
int BuildSetAttr (BPy_Build *msh, char *name, PyObject *v);
PyObject *BuildGetAttr (BPy_Build *msh, char *name);
PyObject *BuildRepr (BPy_Build *msh);
PyObject* BuildCreatePyObject (struct Effect *build);
int BuildCheckPyObject (PyObject *py_obj);
struct Build* BuildFromPyObject (PyObject *py_obj);



#endif /* EXPP_BUILD_H */
