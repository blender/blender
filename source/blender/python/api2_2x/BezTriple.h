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

#ifndef EXPP_BEZTRIPLE_H
#define EXPP_BEZTRIPLE_H

#include <Python.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <DNA_ipo_types.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"


/*****************************************************************************/
/* Python API function prototypes for the BezTriple module.                        */
/*****************************************************************************/
PyObject *M_BezTriple_New (PyObject *self, PyObject *args);
PyObject *M_BezTriple_Get (PyObject *self, PyObject *args);



/*****************************************************************************/
/* Python method structure definition for Blender.BezTriple module:             */
/*****************************************************************************/

static struct PyMethodDef M_BezTriple_methods[] = {
  {"New",(PyCFunction)M_BezTriple_New, METH_VARARGS|METH_KEYWORDS,0},
  {"Get",         M_BezTriple_Get,         METH_VARARGS, 0},
  {"get",         M_BezTriple_Get,         METH_VARARGS, 0},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_BezTriple structure definition:                                     */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  BezTriple *beztriple;
} C_BezTriple;

/*****************************************************************************/
/* Python C_BezTriple methods declarations:                                     */
/*****************************************************************************/
PyObject *BezTriple_getName(C_BezTriple *self);
PyObject *BezTriple_setName(C_BezTriple *self, PyObject *args);

/*****************************************************************************/
/* Python C_BezTriple methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_BezTriple_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)BezTriple_getName, METH_NOARGS,
      "() - Return BezTriple Data name"},  
{"setName", (PyCFunction)BezTriple_setName, METH_VARARGS,
      "(str) - Change BezTriple Data name"},
  {0}
};

/*****************************************************************************/
/* Python BezTriple_Type callback function prototypes:                          */
/*****************************************************************************/
void BezTripleDeAlloc (C_BezTriple *self);
//int BezTriplePrint (C_BezTriple *self, FILE *fp, int flags);
int BezTripleSetAttr (C_BezTriple *self, char *name, PyObject *v);
PyObject *BezTripleGetAttr (C_BezTriple *self, char *name);
PyObject *BezTripleRepr (C_BezTriple *self);

/*****************************************************************************/
/* Python BezTriple_Type structure definition:                                  */
/*****************************************************************************/
static PyTypeObject BezTriple_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                      /* ob_size */
  "BezTriple",                               /* tp_name */
  sizeof (C_BezTriple),                      /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)BezTripleDeAlloc,              /* tp_dealloc */
  0,                 /* tp_print */
  (getattrfunc)BezTripleGetAttr,             /* tp_getattr */
  (setattrfunc)BezTripleSetAttr,             /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)BezTripleRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_BezTriple_methods,                       /* tp_methods */
  0,                                      /* tp_members */
};

#endif /* EXPP_BEZTRIPLE_H */
