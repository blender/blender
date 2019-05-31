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

#include "BPy_StrokeVertex.h"

#include "../../BPy_Freestyle.h"
#include "../../BPy_Convert.h"
#include "../../BPy_StrokeAttribute.h"
#include "../../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    StrokeVertex_doc,
    "Class hierarchy: :class:`Interface0D` > :class:`CurvePoint` > :class:`StrokeVertex`\n"
    "\n"
    "Class to define a stroke vertex.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n"
    "\n"
    ".. method:: __init__(brother)\n"
    "\n"
    "   Copy constructor.\n"
    "\n"
    "   :arg brother: A StrokeVertex object.\n"
    "   :type brother: :class:`StrokeVertex`\n"
    "\n"
    ".. method:: __init__(first_vertex, second_vertex, t3d)\n"
    "\n"
    "   Build a stroke vertex from 2 stroke vertices and an interpolation\n"
    "   parameter.\n"
    "\n"
    "   :arg first_vertex: The first StrokeVertex.\n"
    "   :type first_vertex: :class:`StrokeVertex`\n"
    "   :arg second_vertex: The second StrokeVertex.\n"
    "   :type second_vertex: :class:`StrokeVertex`\n"
    "   :arg t3d: An interpolation parameter.\n"
    "   :type t3d: float\n"
    "\n"
    ".. method:: __init__(point)\n"
    "\n"
    "   Build a stroke vertex from a CurvePoint\n"
    "\n"
    "   :arg point: A CurvePoint object.\n"
    "   :type point: :class:`CurvePoint`\n"
    "\n"
    ".. method:: __init__(svertex)\n"
    "\n"
    "   Build a stroke vertex from a SVertex\n"
    "\n"
    "   :arg svertex: An SVertex object.\n"
    "   :type svertex: :class:`SVertex`\n"
    "\n"
    ".. method:: __init__(svertex, attribute)\n"
    "\n"
    "   Build a stroke vertex from an SVertex and a StrokeAttribute object.\n"
    "\n"
    "   :arg svertex: An SVertex object.\n"
    "   :type svertex: :class:`SVertex`\n"
    "   :arg attribute: A StrokeAttribute object.\n"
    "   :type attribute: :class:`StrokeAttribute`");

static int StrokeVertex_init(BPy_StrokeVertex *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", NULL};
  static const char *kwlist_2[] = {"first_vertex", "second_vertex", "t3d", NULL};
  static const char *kwlist_3[] = {"point", NULL};
  static const char *kwlist_4[] = {"svertex", "attribute", NULL};
  PyObject *obj1 = 0, *obj2 = 0;
  float t3d;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist_1, &StrokeVertex_Type, &obj1)) {
    if (!obj1) {
      self->sv = new StrokeVertex();
    }
    else {
      if (!((BPy_StrokeVertex *)obj1)->sv) {
        PyErr_SetString(PyExc_TypeError, "argument 1 is an invalid StrokeVertex object");
        return -1;
      }
      self->sv = new StrokeVertex(*(((BPy_StrokeVertex *)obj1)->sv));
    }
  }
  else if (PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!O!f",
                                       (char **)kwlist_2,
                                       &StrokeVertex_Type,
                                       &obj1,
                                       &StrokeVertex_Type,
                                       &obj2,
                                       &t3d)) {
    StrokeVertex *sv1 = ((BPy_StrokeVertex *)obj1)->sv;
    StrokeVertex *sv2 = ((BPy_StrokeVertex *)obj2)->sv;
    if (!sv1 || (sv1->A() == 0 && sv1->B() == 0)) {
      PyErr_SetString(PyExc_TypeError, "argument 1 is an invalid StrokeVertex object");
      return -1;
    }
    if (!sv2 || (sv2->A() == 0 && sv2->B() == 0)) {
      PyErr_SetString(PyExc_TypeError, "argument 2 is an invalid StrokeVertex object");
      return -1;
    }
    self->sv = new StrokeVertex(sv1, sv2, t3d);
  }
  else if (PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(
               args, kwds, "O!", (char **)kwlist_3, &CurvePoint_Type, &obj1)) {
    CurvePoint *cp = ((BPy_CurvePoint *)obj1)->cp;
    if (!cp || cp->A() == 0 || cp->B() == 0) {
      PyErr_SetString(PyExc_TypeError, "argument 1 is an invalid CurvePoint object");
      return -1;
    }
    self->sv = new StrokeVertex(cp);
  }
  else if (PyErr_Clear(),
           (obj2 = 0),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!|O!",
                                       (char **)kwlist_4,
                                       &SVertex_Type,
                                       &obj1,
                                       &StrokeAttribute_Type,
                                       &obj2)) {
    if (!obj2) {
      self->sv = new StrokeVertex(((BPy_SVertex *)obj1)->sv);
    }
    else {
      self->sv = new StrokeVertex(((BPy_SVertex *)obj1)->sv, *(((BPy_StrokeAttribute *)obj2)->sa));
    }
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_cp.cp = self->sv;
  self->py_cp.py_if0D.if0D = self->sv;
  self->py_cp.py_if0D.borrowed = false;
  return 0;
}

// real     operator[] (const int i) const
// real &   operator[] (const int i)

/*----------------------mathutils callbacks ----------------------------*/

static int StrokeVertex_mathutils_check(BaseMathObject *bmo)
{
  if (!BPy_StrokeVertex_Check(bmo->cb_user)) {
    return -1;
  }
  return 0;
}

static int StrokeVertex_mathutils_get(BaseMathObject *bmo, int /*subtype*/)
{
  BPy_StrokeVertex *self = (BPy_StrokeVertex *)bmo->cb_user;
  bmo->data[0] = (float)self->sv->x();
  bmo->data[1] = (float)self->sv->y();
  return 0;
}

static int StrokeVertex_mathutils_set(BaseMathObject *bmo, int /*subtype*/)
{
  BPy_StrokeVertex *self = (BPy_StrokeVertex *)bmo->cb_user;
  self->sv->setX((real)bmo->data[0]);
  self->sv->setY((real)bmo->data[1]);
  return 0;
}

static int StrokeVertex_mathutils_get_index(BaseMathObject *bmo, int /*subtype*/, int index)
{
  BPy_StrokeVertex *self = (BPy_StrokeVertex *)bmo->cb_user;
  switch (index) {
    case 0:
      bmo->data[0] = (float)self->sv->x();
      break;
    case 1:
      bmo->data[1] = (float)self->sv->y();
      break;
    default:
      return -1;
  }
  return 0;
}

static int StrokeVertex_mathutils_set_index(BaseMathObject *bmo, int /*subtype*/, int index)
{
  BPy_StrokeVertex *self = (BPy_StrokeVertex *)bmo->cb_user;
  switch (index) {
    case 0:
      self->sv->setX((real)bmo->data[0]);
      break;
    case 1:
      self->sv->setY((real)bmo->data[1]);
      break;
    default:
      return -1;
  }
  return 0;
}

static Mathutils_Callback StrokeVertex_mathutils_cb = {
    StrokeVertex_mathutils_check,
    StrokeVertex_mathutils_get,
    StrokeVertex_mathutils_set,
    StrokeVertex_mathutils_get_index,
    StrokeVertex_mathutils_set_index,
};

static unsigned char StrokeVertex_mathutils_cb_index = -1;

void StrokeVertex_mathutils_register_callback()
{
  StrokeVertex_mathutils_cb_index = Mathutils_RegisterCallback(&StrokeVertex_mathutils_cb);
}

/*----------------------StrokeVertex get/setters ----------------------------*/

PyDoc_STRVAR(StrokeVertex_attribute_doc,
             "StrokeAttribute for this StrokeVertex.\n"
             "\n"
             ":type: :class:`StrokeAttribute`");

static PyObject *StrokeVertex_attribute_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
  return BPy_StrokeAttribute_from_StrokeAttribute(self->sv->attribute());
}

static int StrokeVertex_attribute_set(BPy_StrokeVertex *self,
                                      PyObject *value,
                                      void *UNUSED(closure))
{
  if (!BPy_StrokeAttribute_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a StrokeAttribute object");
    return -1;
  }
  self->sv->setAttribute(*(((BPy_StrokeAttribute *)value)->sa));
  return 0;
}

PyDoc_STRVAR(StrokeVertex_curvilinear_abscissa_doc,
             "Curvilinear abscissa of this StrokeVertex in the Stroke.\n"
             "\n"
             ":type: float");

static PyObject *StrokeVertex_curvilinear_abscissa_get(BPy_StrokeVertex *self,
                                                       void *UNUSED(closure))
{
  return PyFloat_FromDouble(self->sv->curvilinearAbscissa());
}

static int StrokeVertex_curvilinear_abscissa_set(BPy_StrokeVertex *self,
                                                 PyObject *value,
                                                 void *UNUSED(closure))
{
  float scalar;
  if ((scalar = PyFloat_AsDouble(value)) == -1.0f &&
      PyErr_Occurred()) { /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError, "value must be a number");
    return -1;
  }
  self->sv->setCurvilinearAbscissa(scalar);
  return 0;
}

PyDoc_STRVAR(StrokeVertex_point_doc,
             "2D point coordinates.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *StrokeVertex_point_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
  return Vector_CreatePyObject_cb((PyObject *)self, 2, StrokeVertex_mathutils_cb_index, 0);
}

static int StrokeVertex_point_set(BPy_StrokeVertex *self, PyObject *value, void *UNUSED(closure))
{
  float v[2];
  if (mathutils_array_parse(v, 2, 2, value, "value must be a 2-dimensional vector") == -1) {
    return -1;
  }
  self->sv->setX(v[0]);
  self->sv->setY(v[1]);
  return 0;
}

PyDoc_STRVAR(StrokeVertex_stroke_length_doc,
             "Stroke length (it is only a value retained by the StrokeVertex,\n"
             "and it won't change the real stroke length).\n"
             "\n"
             ":type: float");

static PyObject *StrokeVertex_stroke_length_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
  return PyFloat_FromDouble(self->sv->strokeLength());
}

static int StrokeVertex_stroke_length_set(BPy_StrokeVertex *self,
                                          PyObject *value,
                                          void *UNUSED(closure))
{
  float scalar;
  if ((scalar = PyFloat_AsDouble(value)) == -1.0f &&
      PyErr_Occurred()) { /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError, "value must be a number");
    return -1;
  }
  self->sv->setStrokeLength(scalar);
  return 0;
}

PyDoc_STRVAR(StrokeVertex_u_doc,
             "Curvilinear abscissa of this StrokeVertex in the Stroke.\n"
             "\n"
             ":type: float");

static PyObject *StrokeVertex_u_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
  return PyFloat_FromDouble(self->sv->u());
}

static PyGetSetDef BPy_StrokeVertex_getseters[] = {
    {(char *)"attribute",
     (getter)StrokeVertex_attribute_get,
     (setter)StrokeVertex_attribute_set,
     (char *)StrokeVertex_attribute_doc,
     NULL},
    {(char *)"curvilinear_abscissa",
     (getter)StrokeVertex_curvilinear_abscissa_get,
     (setter)StrokeVertex_curvilinear_abscissa_set,
     (char *)StrokeVertex_curvilinear_abscissa_doc,
     NULL},
    {(char *)"point",
     (getter)StrokeVertex_point_get,
     (setter)StrokeVertex_point_set,
     (char *)StrokeVertex_point_doc,
     NULL},
    {(char *)"stroke_length",
     (getter)StrokeVertex_stroke_length_get,
     (setter)StrokeVertex_stroke_length_set,
     (char *)StrokeVertex_stroke_length_doc,
     NULL},
    {(char *)"u", (getter)StrokeVertex_u_get, (setter)NULL, (char *)StrokeVertex_u_doc, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_StrokeVertex type definition ------------------------------*/
PyTypeObject StrokeVertex_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "StrokeVertex", /* tp_name */
    sizeof(BPy_StrokeVertex),                      /* tp_basicsize */
    0,                                             /* tp_itemsize */
    0,                                             /* tp_dealloc */
    0,                                             /* tp_print */
    0,                                             /* tp_getattr */
    0,                                             /* tp_setattr */
    0,                                             /* tp_reserved */
    0,                                             /* tp_repr */
    0,                                             /* tp_as_number */
    0,                                             /* tp_as_sequence */
    0,                                             /* tp_as_mapping */
    0,                                             /* tp_hash  */
    0,                                             /* tp_call */
    0,                                             /* tp_str */
    0,                                             /* tp_getattro */
    0,                                             /* tp_setattro */
    0,                                             /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,      /* tp_flags */
    StrokeVertex_doc,                              /* tp_doc */
    0,                                             /* tp_traverse */
    0,                                             /* tp_clear */
    0,                                             /* tp_richcompare */
    0,                                             /* tp_weaklistoffset */
    0,                                             /* tp_iter */
    0,                                             /* tp_iternext */
    0,                                             /* tp_methods */
    0,                                             /* tp_members */
    BPy_StrokeVertex_getseters,                    /* tp_getset */
    &CurvePoint_Type,                              /* tp_base */
    0,                                             /* tp_dict */
    0,                                             /* tp_descr_get */
    0,                                             /* tp_descr_set */
    0,                                             /* tp_dictoffset */
    (initproc)StrokeVertex_init,                   /* tp_init */
    0,                                             /* tp_alloc */
    0,                                             /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
