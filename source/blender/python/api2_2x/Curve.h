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

#ifndef EXPP_CURVE_H
#define EXPP_CURVE_H

#include <Python.h>
#include <stdio.h>

#include <BLI_arithb.h>
#include <BLI_blenlib.h>
#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BKE_curve.h>
#include <DNA_curve_types.h>

#include "gen_utils.h"

/*****************************************************************************/
/* Python API function prototypes for the Curve module.                      */
/*****************************************************************************/
static PyObject *M_Curve_New (PyObject *self, PyObject *args);
static PyObject *M_Curve_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Curve.__doc__                                                     */
/*****************************************************************************/
char M_Curve_doc[] = "";
char M_Curve_New_doc[] ="";
char M_Curve_Get_doc[] ="xxx";
/*****************************************************************************/
/* Python method structure definition for Blender.Curve module:              */
/*****************************************************************************/
struct PyMethodDef M_Curve_methods[] = {
  {"New",(PyCFunction)M_Curve_New, METH_VARARGS,M_Curve_New_doc},
  {"Get",         M_Curve_Get,         METH_VARARGS, M_Curve_Get_doc},
  {"get",         M_Curve_Get,         METH_VARARGS, M_Curve_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Curve structure definition:                                     */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Curve   *curve;
} C_Curve;

/*****************************************************************************/
/* Python C_Curve methods declarations:                                      */
/*****************************************************************************/
static PyObject *Curve_getName(C_Curve *self);
static PyObject *Curve_setName(C_Curve *self, PyObject *args);
static PyObject *Curve_getPathLen(C_Curve *self);
static PyObject *Curve_setPathLen(C_Curve *self, PyObject *args);
static PyObject *Curve_getTotcol(C_Curve *self);
static PyObject *Curve_setTotcol(C_Curve *self, PyObject *args);
static PyObject *Curve_getMode(C_Curve *self);
static PyObject *Curve_setMode(C_Curve *self, PyObject *args);
static PyObject *Curve_getBevresol(C_Curve *self);
static PyObject *Curve_setBevresol(C_Curve *self, PyObject *args);
static PyObject *Curve_getResolu(C_Curve *self);
static PyObject *Curve_setResolu(C_Curve *self, PyObject *args);
static PyObject *Curve_getResolv(C_Curve *self);
static PyObject *Curve_setResolv(C_Curve *self, PyObject *args);
static PyObject *Curve_getWidth(C_Curve *self);
static PyObject *Curve_setWidth(C_Curve *self, PyObject *args);
static PyObject *Curve_getExt1(C_Curve *self);
static PyObject *Curve_setExt1(C_Curve *self, PyObject *args);
static PyObject *Curve_getExt2(C_Curve *self);
static PyObject *Curve_setExt2(C_Curve *self, PyObject *args);
static PyObject *Curve_getControlPoint(C_Curve *self, PyObject *args);
static PyObject *Curve_setControlPoint(C_Curve *self, PyObject *args);
static PyObject *Curve_getLoc(C_Curve *self);
static PyObject *Curve_setLoc(C_Curve *self, PyObject *args);
static PyObject *Curve_getRot(C_Curve *self);
static PyObject *Curve_setRot(C_Curve *self, PyObject *args);
static PyObject *Curve_getSize(C_Curve *self);
static PyObject *Curve_setSize(C_Curve *self, PyObject *args);

/*****************************************************************************/
/* Python C_Curve methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_Curve_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Curve_getName, METH_NOARGS,
          "() - Return Curve Data name"},
  {"setName", (PyCFunction)Curve_setName, METH_NOARGS,
          "() - Sets Curve Data name"},
  {"getPathLen", (PyCFunction)Curve_getPathLen, METH_NOARGS,
          "() - Return Curve path length"},
  {"setPathLen", (PyCFunction)Curve_setPathLen, METH_VARARGS,
          "(int) - Sets Curve path length"},
  {"getTotcol", (PyCFunction)Curve_getTotcol, METH_NOARGS,"() - Return totcol"},
  {"setTotcol", (PyCFunction)Curve_setTotcol, METH_VARARGS,
          "(int) - Sets totcol"},
  {"getFlag", (PyCFunction)Curve_getMode, METH_NOARGS,"() - Return flag"},
  {"setFlag", (PyCFunction)Curve_setMode, METH_VARARGS,"(int) - Sets flag"},
  {"getBevresol", (PyCFunction)Curve_getBevresol, METH_NOARGS,
          "() - Return bevresol"},
  {"setBevresol", (PyCFunction)Curve_setBevresol, METH_VARARGS,
          "(int) - Sets bevresol"},
  {"getResolu", (PyCFunction)Curve_getResolu, METH_NOARGS,"() - Return resolu"},
  {"setResolu", (PyCFunction)Curve_setResolu, METH_VARARGS,
          "(int) - Sets resolu"},
  {"getResolv", (PyCFunction)Curve_getResolv, METH_NOARGS,"() - Return resolv"},
  {"setResolv", (PyCFunction)Curve_setResolv, METH_VARARGS,
          "(int) - Sets resolv"},
  {"getWidth", (PyCFunction)Curve_getWidth, METH_NOARGS,"() - Return width"},
  {"setWidth", (PyCFunction)Curve_setWidth, METH_VARARGS,"(int) - Sets width"},
  {"getExt1", (PyCFunction)Curve_getExt1, METH_NOARGS,"() - Return ext1"},
  {"setExt1", (PyCFunction)Curve_setExt1, METH_VARARGS,"(int) - Sets ext1"},
  {"getExt2", (PyCFunction)Curve_getExt2, METH_NOARGS,"() - Return ext2"},
  {"setExt2", (PyCFunction)Curve_setExt2, METH_VARARGS,"(int) - Sets ext2"},
  {"getControlPoint", (PyCFunction)Curve_getControlPoint, METH_VARARGS,
          "(int numcurve,int numpoint) - Gets a control point.Depending upon"
          " the curve type, returne a list of 4 or 9 floats"},
  {"setControlPoint", (PyCFunction)Curve_setControlPoint, METH_VARARGS,
          "(int numcurve,int numpoint,float x,float y,float z, float w)(nurbs)"
          " or  (int numcurve,int numpoint,float x1,...,x9(bezier) "
					"Sets a control point "},
  {"getLoc", (PyCFunction)Curve_getLoc, METH_NOARGS,"() - Gets Location"},
  {"setLoc", (PyCFunction)Curve_setLoc, METH_VARARGS,
          "(float x,float y,float z) - Sets Location"},
  {"getRot", (PyCFunction)Curve_getRot, METH_NOARGS,"() - Gets Rotation"},
  {"setRot", (PyCFunction)Curve_setRot, METH_VARARGS,
          "(float x,float y,float z) - Sets Rotation"},
  {"getSize", (PyCFunction)Curve_getSize, METH_NOARGS,"() - Gets Size"},
  {"setSize", (PyCFunction)Curve_setSize, METH_VARARGS,
          "(float x,float y,float z) - Sets Size"},
  {0}
};

/*****************************************************************************/
/* Python Curve_Type callback function prototypes:                          */
/*****************************************************************************/
static void CurveDeAlloc (C_Curve *msh);
static int CurvePrint (C_Curve *msh, FILE *fp, int flags);
static int CurveSetAttr (C_Curve *msh, char *name, PyObject *v);
static PyObject *CurveGetAttr (C_Curve *msh, char *name);
static PyObject *CurveRepr (C_Curve *msh);
PyObject* CurveCreatePyObject (struct Curve *curve);
int CurveCheckPyObject (PyObject *py_obj);
struct Curve* CurveFromPyObject (PyObject *py_obj);

/*****************************************************************************/
/* Python Curve_Type structure definition:                                  */
/*****************************************************************************/
static PyTypeObject Curve_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                                      /* ob_size */
  "Curve",                               /* tp_name */
  sizeof (C_Curve),                      /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)CurveDeAlloc,              /* tp_dealloc */
  (printfunc)CurvePrint,                 /* tp_print */
  (getattrfunc)CurveGetAttr,             /* tp_getattr */
  (setattrfunc)CurveSetAttr,             /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)CurveRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_Curve_methods,                       /* tp_methods */
  0,                                      /* tp_members */
};

#endif /* EXPP_CURVE_H */
