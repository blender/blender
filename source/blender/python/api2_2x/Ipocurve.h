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

#ifndef EXPP_IPOCURVE_H
#define EXPP_IPOCURVE_H

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
/* Python API function prototypes for the IpoCurve module.                   */
/*****************************************************************************/
PyObject *M_IpoCurve_New (PyObject *self, PyObject *args);
PyObject *M_IpoCurve_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.IpoCurve.__doc__                                                  */
/*****************************************************************************/
static char M_IpoCurve_doc[] = "";
static char M_IpoCurve_New_doc[] ="";
static char M_IpoCurve_Get_doc[] ="";


/*****************************************************************************/
/* Python method structure definition for Blender.IpoCurve module:           */
/*****************************************************************************/

static struct PyMethodDef M_IpoCurve_methods[] = {
  {"New",(PyCFunction)M_IpoCurve_New, METH_VARARGS|METH_KEYWORDS,M_IpoCurve_New_doc},
  {"Get",         M_IpoCurve_Get,         METH_VARARGS, M_IpoCurve_Get_doc},
  {"get",         M_IpoCurve_Get,         METH_VARARGS, M_IpoCurve_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_IpoCurve structure definition:                                   */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  IpoCurve *ipocurve;
} C_IpoCurve;

/*****************************************************************************/
/* Python C_IpoCurve methods declarations:                                   */
/*****************************************************************************/
PyObject *IpoCurve_getName(C_IpoCurve *self);
PyObject *IpoCurve_Recalc(C_IpoCurve *self);
PyObject *IpoCurve_setName(C_IpoCurve *self, PyObject *args);

/*****************************************************************************/
/* Python C_IpoCurve methods table:                                          */
/*****************************************************************************/
static PyMethodDef C_IpoCurve_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)IpoCurve_getName, METH_NOARGS,
      "() - Return IpoCurve Data name"},  
  {"Recalc", (PyCFunction)IpoCurve_Recalc, METH_NOARGS,
      "() - Return IpoCurve Data name"},  
{"setName", (PyCFunction)IpoCurve_setName, METH_VARARGS,
      "(str) - Change IpoCurve Data name"},
  {0}
};

/*****************************************************************************/
/* Python IpoCurve_Type callback function prototypes:                        */
/*****************************************************************************/
void IpoCurveDeAlloc (C_IpoCurve *self);
//int IpoCurvePrint (C_IpoCurve *self, FILE *fp, int flags);
int IpoCurveSetAttr (C_IpoCurve *self, char *name, PyObject *v);
PyObject *IpoCurveGetAttr (C_IpoCurve *self, char *name);
PyObject *IpoCurveRepr (C_IpoCurve *self);

/*****************************************************************************/
/* Python IpoCurve_Type structure definition:                                */
/*****************************************************************************/
static PyTypeObject IpoCurve_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                      /* ob_size */
  "IpoCurve",                               /* tp_name */
  sizeof (C_IpoCurve),                      /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)IpoCurveDeAlloc,              /* tp_dealloc */
  0,                 /* tp_print */
  (getattrfunc)IpoCurveGetAttr,             /* tp_getattr */
  (setattrfunc)IpoCurveSetAttr,             /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)IpoCurveRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_IpoCurve_methods,                       /* tp_methods */
  0,                                      /* tp_members */
};

#endif /* EXPP_IPOCURVE_H */
