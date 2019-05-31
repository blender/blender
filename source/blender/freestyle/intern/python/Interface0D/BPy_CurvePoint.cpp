/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup freestyle
 */

#include "BPy_CurvePoint.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------CurvePoint methods----------------------------*/

PyDoc_STRVAR(CurvePoint_doc,
             "Class hierarchy: :class:`Interface0D` > :class:`CurvePoint`\n"
             "\n"
             "Class to represent a point of a curve.  A CurvePoint can be any point\n"
             "of a 1D curve (it doesn't have to be a vertex of the curve).  Any\n"
             ":class:`Interface1D` is built upon ViewEdges, themselves built upon\n"
             "FEdges.  Therefore, a curve is basically a polyline made of a list of\n"
             ":class:`SVertex` objects.  Thus, a CurvePoint is built by linearly\n"
             "interpolating two :class:`SVertex` instances.  CurvePoint can be used\n"
             "as virtual points while querying 0D information along a curve at a\n"
             "given resolution.\n"
             "\n"
             ".. method:: __init__()\n"
             "\n"
             "   Defult constructor.\n"
             "\n"
             ".. method:: __init__(brother)\n"
             "\n"
             "   Copy constructor.\n"
             "\n"
             "   :arg brother: A CurvePoint object.\n"
             "   :type brother: :class:`CurvePoint`\n"
             "\n"
             ".. method:: __init__(first_vertex, second_vertex, t2d)\n"
             "\n"
             "   Builds a CurvePoint from two SVertex objects and an interpolation parameter.\n"
             "\n"
             "   :arg first_vertex: The first SVertex.\n"
             "   :type first_vertex: :class:`SVertex`\n"
             "   :arg second_vertex: The second SVertex.\n"
             "   :type second_vertex: :class:`SVertex`\n"
             "   :arg t2d: A 2D interpolation parameter used to linearly interpolate\n"
             "             first_vertex and second_vertex.\n"
             "   :type t2d: float\n"
             "\n"
             ".. method:: __init__(first_point, second_point, t2d)\n"
             "\n"
             "   Builds a CurvePoint from two CurvePoint objects and an interpolation\n"
             "   parameter.\n"
             "\n"
             "   :arg first_point: The first CurvePoint.\n"
             "   :type first_point: :class:`CurvePoint`\n"
             "   :arg second_point: The second CurvePoint.\n"
             "   :type second_point: :class:`CurvePoint`\n"
             "   :arg t2d: The 2D interpolation parameter used to linearly interpolate\n"
             "             first_point and second_point.\n"
             "   :type t2d: float");

static int CurvePoint_init(BPy_CurvePoint *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", NULL};
  static const char *kwlist_2[] = {"first_vertex", "second_vertex", "t2d", NULL};
  static const char *kwlist_3[] = {"first_point", "second_point", "t2d", NULL};
  PyObject *obj1 = 0, *obj2 = 0;
  float t2d;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &CurvePoint_Type, &obj1)) {
    if (!obj1) {
      self->cp = new CurvePoint();
    }
    else {
      self->cp = new CurvePoint(*(((BPy_CurvePoint *)obj1)->cp));
    }
  }
  else if (PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!O!f",
                                       (char **)kwlist_2,
                                       &SVertex_Type,
                                       &obj1,
                                       &SVertex_Type,
                                       &obj2,
                                       &t2d)) {
    self->cp = new CurvePoint(((BPy_SVertex *)obj1)->sv, ((BPy_SVertex *)obj2)->sv, t2d);
  }
  else if (PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!O!f",
                                       (char **)kwlist_3,
                                       &CurvePoint_Type,
                                       &obj1,
                                       &CurvePoint_Type,
                                       &obj2,
                                       &t2d)) {
    CurvePoint *cp1 = ((BPy_CurvePoint *)obj1)->cp;
    CurvePoint *cp2 = ((BPy_CurvePoint *)obj2)->cp;
    if (!cp1 || cp1->A() == 0 || cp1->B() == 0) {
      PyErr_SetString(PyExc_TypeError, "argument 1 is an invalid CurvePoint object");
      return -1;
    }
    if (!cp2 || cp2->A() == 0 || cp2->B() == 0) {
      PyErr_SetString(PyExc_TypeError, "argument 2 is an invalid CurvePoint object");
      return -1;
    }
    self->cp = new CurvePoint(cp1, cp2, t2d);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_if0D.if0D = self->cp;
  self->py_if0D.borrowed = false;
  return 0;
}

/// bool     operator== (const CurvePoint &b)

/*----------------------CurvePoint get/setters ----------------------------*/

PyDoc_STRVAR(CurvePoint_first_svertex_doc,
             "The first SVertex upon which the CurvePoint is built.\n"
             "\n"
             ":type: :class:`SVertex`");

static PyObject *CurvePoint_first_svertex_get(BPy_CurvePoint *self, void *UNUSED(closure))
{
  SVertex *A = self->cp->A();
  if (A) {
    return BPy_SVertex_from_SVertex(*A);
  }
  Py_RETURN_NONE;
}

static int CurvePoint_first_svertex_set(BPy_CurvePoint *self,
                                        PyObject *value,
                                        void *UNUSED(closure))
{
  if (!BPy_SVertex_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an SVertex");
    return -1;
  }
  self->cp->setA(((BPy_SVertex *)value)->sv);
  return 0;
}

PyDoc_STRVAR(CurvePoint_second_svertex_doc,
             "The second SVertex upon which the CurvePoint is built.\n"
             "\n"
             ":type: :class:`SVertex`");

static PyObject *CurvePoint_second_svertex_get(BPy_CurvePoint *self, void *UNUSED(closure))
{
  SVertex *B = self->cp->B();
  if (B) {
    return BPy_SVertex_from_SVertex(*B);
  }
  Py_RETURN_NONE;
}

static int CurvePoint_second_svertex_set(BPy_CurvePoint *self,
                                         PyObject *value,
                                         void *UNUSED(closure))
{
  if (!BPy_SVertex_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an SVertex");
    return -1;
  }
  self->cp->setB(((BPy_SVertex *)value)->sv);
  return 0;
}

PyDoc_STRVAR(CurvePoint_fedge_doc,
             "Gets the FEdge for the two SVertices that given CurvePoints consists out of.\n"
             "A shortcut for CurvePoint.first_svertex.get_fedge(CurvePoint.second_svertex).\n"
             "\n"
             ":type: :class:`FEdge`");

static PyObject *CurvePoint_fedge_get(BPy_CurvePoint *self, void *UNUSED(closure))
{
  SVertex *A = self->cp->A();
  Interface0D *B = (Interface0D *)self->cp->B();
  // B can be NULL under certain circumstances
  if (B) {
    return Any_BPy_Interface1D_from_Interface1D(*(A->getFEdge(*B)));
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(CurvePoint_t2d_doc,
             "The 2D interpolation parameter.\n"
             "\n"
             ":type: float");

static PyObject *CurvePoint_t2d_get(BPy_CurvePoint *self, void *UNUSED(closure))
{
  return PyFloat_FromDouble(self->cp->t2d());
}

static int CurvePoint_t2d_set(BPy_CurvePoint *self, PyObject *value, void *UNUSED(closure))
{
  float scalar;
  if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "value must be a number");
    return -1;
  }
  self->cp->setT2d(scalar);
  return 0;
}

static PyGetSetDef BPy_CurvePoint_getseters[] = {
    {(char *)"first_svertex",
     (getter)CurvePoint_first_svertex_get,
     (setter)CurvePoint_first_svertex_set,
     (char *)CurvePoint_first_svertex_doc,
     NULL},
    {(char *)"second_svertex",
     (getter)CurvePoint_second_svertex_get,
     (setter)CurvePoint_second_svertex_set,
     (char *)CurvePoint_second_svertex_doc,
     NULL},
    {(char *)"fedge", (getter)CurvePoint_fedge_get, NULL, CurvePoint_fedge_doc, NULL},
    {(char *)"t2d",
     (getter)CurvePoint_t2d_get,
     (setter)CurvePoint_t2d_set,
     (char *)CurvePoint_t2d_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_CurvePoint type definition ------------------------------*/
PyTypeObject CurvePoint_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "CurvePoint", /* tp_name */
    sizeof(BPy_CurvePoint),                      /* tp_basicsize */
    0,                                           /* tp_itemsize */
    0,                                           /* tp_dealloc */
    0,                                           /* tp_print */
    0,                                           /* tp_getattr */
    0,                                           /* tp_setattr */
    0,                                           /* tp_reserved */
    0,                                           /* tp_repr */
    0,                                           /* tp_as_number */
    0,                                           /* tp_as_sequence */
    0,                                           /* tp_as_mapping */
    0,                                           /* tp_hash  */
    0,                                           /* tp_call */
    0,                                           /* tp_str */
    0,                                           /* tp_getattro */
    0,                                           /* tp_setattro */
    0,                                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
    CurvePoint_doc,                              /* tp_doc */
    0,                                           /* tp_traverse */
    0,                                           /* tp_clear */
    0,                                           /* tp_richcompare */
    0,                                           /* tp_weaklistoffset */
    0,                                           /* tp_iter */
    0,                                           /* tp_iternext */
    0,                                           /* tp_methods */
    0,                                           /* tp_members */
    BPy_CurvePoint_getseters,                    /* tp_getset */
    &Interface0D_Type,                           /* tp_base */
    0,                                           /* tp_dict */
    0,                                           /* tp_descr_get */
    0,                                           /* tp_descr_set */
    0,                                           /* tp_dictoffset */
    (initproc)CurvePoint_init,                   /* tp_init */
    0,                                           /* tp_alloc */
    0,                                           /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
