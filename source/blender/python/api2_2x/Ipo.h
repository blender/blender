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

#ifndef EXPP_IPO_H
#define EXPP_IPO_H

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
/* Python API function prototypes for the Ipo module.                        */
/*****************************************************************************/
static PyObject *M_Ipo_New (PyObject *self, PyObject *args);
static PyObject *M_Ipo_Get (PyObject *self, PyObject *args);
static PyObject *M_Ipo_Recalc (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Ipo.__doc__                                                       */
/*****************************************************************************/
char M_Ipo_doc[] = "";
char M_Ipo_New_doc[] ="";
char M_Ipo_Get_doc[] ="";


/*****************************************************************************/
/* Python method structure definition for Blender.Ipo module:             */
/*****************************************************************************/

struct PyMethodDef M_Ipo_methods[] = {
  {"New",(PyCFunction)M_Ipo_New, METH_VARARGS|METH_KEYWORDS,M_Ipo_New_doc},
  {"Get",         M_Ipo_Get,         METH_VARARGS, M_Ipo_Get_doc},
  {"get",         M_Ipo_Get,         METH_VARARGS, M_Ipo_Get_doc},
  {"Recalc",         M_Ipo_Recalc,         METH_VARARGS, M_Ipo_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Ipo structure definition:                                     */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Ipo *ipo;
} C_Ipo;

/*****************************************************************************/
/* Python C_Ipo methods declarations:                                     */
/*****************************************************************************/
static PyObject *Ipo_getName(C_Ipo *self);
static PyObject *Ipo_setName(C_Ipo *self, PyObject *args);
static PyObject *Ipo_getBlocktype(C_Ipo *self);
static PyObject *Ipo_setBlocktype(C_Ipo *self, PyObject *args);
static PyObject *Ipo_getRctf(C_Ipo *self);
static PyObject *Ipo_setRctf(C_Ipo *self, PyObject *args);

static PyObject *Ipo_addCurve(C_Ipo *self, PyObject *args);
static PyObject *Ipo_getNcurves(C_Ipo *self);
static PyObject *Ipo_getNBezPoints(C_Ipo *self, PyObject *args);
static PyObject *Ipo_DeleteBezPoints(C_Ipo *self, PyObject *args);
static PyObject *Ipo_getCurveBP(C_Ipo *self, PyObject *args);
static PyObject *Ipo_getCurvecurval(C_Ipo *self, PyObject *args);
static PyObject *Ipo_EvaluateCurveOn(C_Ipo *self, PyObject *args);


static PyObject *Ipo_setCurveBeztriple(C_Ipo *self, PyObject *args);
static PyObject *Ipo_getCurveBeztriple(C_Ipo *self, PyObject *args);

/*****************************************************************************/
/* Python C_Ipo methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_Ipo_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Ipo_getName, METH_NOARGS,
      "() - Return Ipo Data name"},  
{"setName", (PyCFunction)Ipo_setName, METH_VARARGS,
      "(str) - Change Ipo Data name"},
  {"getBlocktype", (PyCFunction)Ipo_getBlocktype, METH_NOARGS,
      "() - Return Ipo blocktype -"},
  {"setBlocktype", (PyCFunction)Ipo_setBlocktype, METH_VARARGS,
      "(str) - Change Ipo blocktype"},
  {"getRctf", (PyCFunction)Ipo_getRctf, METH_NOARGS,
      "() - Return Ipo rctf - "},
  {"setRctf", (PyCFunction)Ipo_setRctf, METH_VARARGS,
      "(str) - Change Ipo rctf"},
  {"addCurve", (PyCFunction)Ipo_addCurve, METH_VARARGS,
      "() - Return Ipo ncurves"},
  {"getNcurves", (PyCFunction)Ipo_getNcurves, METH_NOARGS,
      "() - Return Ipo ncurves"},
  {"getNBezPoints", (PyCFunction)Ipo_getNBezPoints, METH_VARARGS,
      "() - Return curve number of Bez points"},
  {"delBezPoint", (PyCFunction)Ipo_DeleteBezPoints, METH_VARARGS,
      "() - Return curve number of Bez points"},
  {"getCurveBP", (PyCFunction)Ipo_getCurveBP, METH_VARARGS,
      "() - Return Ipo ncurves"},
  {"EvaluateCurveOn", (PyCFunction)Ipo_EvaluateCurveOn, METH_VARARGS,
      "() - Return curve value at given time"},
  {"getCurveCurval", (PyCFunction)Ipo_getCurvecurval, METH_VARARGS,
      "() - Return curval"},
  {"getCurveBeztriple", (PyCFunction)Ipo_getCurveBeztriple, METH_VARARGS,
      "() - Return Ipo ncurves"},
  {"setCurveBeztriple", (PyCFunction)Ipo_setCurveBeztriple, METH_VARARGS,
      "() - Return curval"},
  {0}
};

/*****************************************************************************/
/* Python Ipo_Type callback function prototypes:                          */
/*****************************************************************************/
static void IpoDeAlloc (C_Ipo *self);
//static int IpoPrint (C_Ipo *self, FILE *fp, int flags);
static int IpoSetAttr (C_Ipo *self, char *name, PyObject *v);
static PyObject *IpoGetAttr (C_Ipo *self, char *name);
static PyObject *IpoRepr (C_Ipo *self);

/*****************************************************************************/
/* Python Ipo_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject Ipo_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                      /* ob_size */
  "Ipo",                               /* tp_name */
  sizeof (C_Ipo),                      /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)IpoDeAlloc,              /* tp_dealloc */
  0,                 /* tp_print */
  (getattrfunc)IpoGetAttr,             /* tp_getattr */
  (setattrfunc)IpoSetAttr,             /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)IpoRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_Ipo_methods,                       /* tp_methods */
  0,                                      /* tp_members */
};

#endif /* EXPP_IPO_H */
