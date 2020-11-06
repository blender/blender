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

#include "BPy_Nature.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

static PyObject *BPy_Nature_and(PyObject *a, PyObject *b);
static PyObject *BPy_Nature_xor(PyObject *a, PyObject *b);
static PyObject *BPy_Nature_or(PyObject *a, PyObject *b);

/*-----------------------BPy_Nature number method definitions --------------------*/

static PyNumberMethods nature_as_number = {
    nullptr,                          /* binaryfunc nb_add */
    nullptr,                          /* binaryfunc nb_subtract */
    nullptr,                          /* binaryfunc nb_multiply */
    nullptr,                          /* binaryfunc nb_remainder */
    nullptr,                          /* binaryfunc nb_divmod */
    nullptr,                          /* ternaryfunc nb_power */
    nullptr,                          /* unaryfunc nb_negative */
    nullptr,                          /* unaryfunc nb_positive */
    nullptr,                          /* unaryfunc nb_absolute */
    nullptr,                          /* inquiry nb_bool */
    nullptr,                          /* unaryfunc nb_invert */
    nullptr,                          /* binaryfunc nb_lshift */
    nullptr,                          /* binaryfunc nb_rshift */
    (binaryfunc)BPy_Nature_and, /* binaryfunc nb_and */
    (binaryfunc)BPy_Nature_xor, /* binaryfunc nb_xor */
    (binaryfunc)BPy_Nature_or,  /* binaryfunc nb_or */
    nullptr,                          /* unaryfunc nb_int */
    nullptr,                          /* void *nb_reserved */
    nullptr,                          /* unaryfunc nb_float */
    nullptr,                          /* binaryfunc nb_inplace_add */
    nullptr,                          /* binaryfunc nb_inplace_subtract */
    nullptr,                          /* binaryfunc nb_inplace_multiply */
    nullptr,                          /* binaryfunc nb_inplace_remainder */
    nullptr,                          /* ternaryfunc nb_inplace_power */
    nullptr,                          /* binaryfunc nb_inplace_lshift */
    nullptr,                          /* binaryfunc nb_inplace_rshift */
    nullptr,                          /* binaryfunc nb_inplace_and */
    nullptr,                          /* binaryfunc nb_inplace_xor */
    nullptr,                          /* binaryfunc nb_inplace_or */
    nullptr,                          /* binaryfunc nb_floor_divide */
    nullptr,                          /* binaryfunc nb_true_divide */
    nullptr,                          /* binaryfunc nb_inplace_floor_divide */
    nullptr,                          /* binaryfunc nb_inplace_true_divide */
    nullptr,                          /* unaryfunc nb_index */
};

/*-----------------------BPy_Nature docstring ------------------------------------*/

PyDoc_STRVAR(Nature_doc,
             "Class hierarchy: int > :class:`Nature`\n"
             "\n"
             "Different possible natures of 0D and 1D elements of the ViewMap.\n"
             "\n"
             "Vertex natures:\n"
             "\n"
             "* Nature.POINT: True for any 0D element.\n"
             "* Nature.S_VERTEX: True for SVertex.\n"
             "* Nature.VIEW_VERTEX: True for ViewVertex.\n"
             "* Nature.NON_T_VERTEX: True for NonTVertex.\n"
             "* Nature.T_VERTEX: True for TVertex.\n"
             "* Nature.CUSP: True for CUSP.\n"
             "\n"
             "Edge natures:\n"
             "\n"
             "* Nature.NO_FEATURE: True for non feature edges (always false for 1D\n"
             "  elements of the ViewMap).\n"
             "* Nature.SILHOUETTE: True for silhouettes.\n"
             "* Nature.BORDER: True for borders.\n"
             "* Nature.CREASE: True for creases.\n"
             "* Nature.RIDGE: True for ridges.\n"
             "* Nature.VALLEY: True for valleys.\n"
             "* Nature.SUGGESTIVE_CONTOUR: True for suggestive contours.\n"
             "* Nature.MATERIAL_BOUNDARY: True for edges at material boundaries.\n"
             "* Nature.EDGE_MARK: True for edges having user-defined edge marks.");

/*-----------------------BPy_Nature type definition ------------------------------*/

PyTypeObject Nature_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "Nature", /* tp_name */
    sizeof(PyLongObject),                    /* tp_basicsize */
    0,                                       /* tp_itemsize */
    nullptr,                                       /* tp_dealloc */
    nullptr,                                       /* tp_print */
    nullptr,                                       /* tp_getattr */
    nullptr,                                       /* tp_setattr */
    nullptr,                                       /* tp_reserved */
    nullptr,                                       /* tp_repr */
    &nature_as_number,                       /* tp_as_number */
    nullptr,                                       /* tp_as_sequence */
    nullptr,                                       /* tp_as_mapping */
    nullptr,                                       /* tp_hash  */
    nullptr,                                       /* tp_call */
    nullptr,                                       /* tp_str */
    nullptr,                                       /* tp_getattro */
    nullptr,                                       /* tp_setattro */
    nullptr,                                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                      /* tp_flags */
    Nature_doc,                              /* tp_doc */
    nullptr,                                       /* tp_traverse */
    nullptr,                                       /* tp_clear */
    nullptr,                                       /* tp_richcompare */
    0,                                       /* tp_weaklistoffset */
    nullptr,                                       /* tp_iter */
    nullptr,                                       /* tp_iternext */
    nullptr,                                       /* tp_methods */
    nullptr,                                       /* tp_members */
    nullptr,                                       /* tp_getset */
    &PyLong_Type,                            /* tp_base */
    nullptr,                                       /* tp_dict */
    nullptr,                                       /* tp_descr_get */
    nullptr,                                       /* tp_descr_set */
    0,                                       /* tp_dictoffset */
    nullptr,                                       /* tp_init */
    nullptr,                                       /* tp_alloc */
    nullptr,                                       /* tp_new */
};

/*-----------------------BPy_Nature instance definitions ----------------------------------*/

static PyLongObject _Nature_POINT = {PyVarObject_HEAD_INIT(&Nature_Type, 0){Nature::POINT}};
static PyLongObject _Nature_S_VERTEX = {PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::S_VERTEX}};
static PyLongObject _Nature_VIEW_VERTEX = {
    PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::VIEW_VERTEX}};
static PyLongObject _Nature_NON_T_VERTEX = {
    PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::NON_T_VERTEX}};
static PyLongObject _Nature_T_VERTEX = {PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::T_VERTEX}};
static PyLongObject _Nature_CUSP = {PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::CUSP}};
static PyLongObject _Nature_NO_FEATURE = {
    PyVarObject_HEAD_INIT(&Nature_Type, 0){Nature::NO_FEATURE}};
static PyLongObject _Nature_SILHOUETTE = {
    PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::SILHOUETTE}};
static PyLongObject _Nature_BORDER = {PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::BORDER}};
static PyLongObject _Nature_CREASE = {PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::CREASE}};
static PyLongObject _Nature_RIDGE = {PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::RIDGE}};
static PyLongObject _Nature_VALLEY = {PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::VALLEY}};
static PyLongObject _Nature_SUGGESTIVE_CONTOUR = {
    PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::SUGGESTIVE_CONTOUR}};
static PyLongObject _Nature_MATERIAL_BOUNDARY = {
    PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::MATERIAL_BOUNDARY}};
static PyLongObject _Nature_EDGE_MARK = {
    PyVarObject_HEAD_INIT(&Nature_Type, 1){Nature::EDGE_MARK}};

#define BPy_Nature_POINT ((PyObject *)&_Nature_POINT)
#define BPy_Nature_S_VERTEX ((PyObject *)&_Nature_S_VERTEX)
#define BPy_Nature_VIEW_VERTEX ((PyObject *)&_Nature_VIEW_VERTEX)
#define BPy_Nature_NON_T_VERTEX ((PyObject *)&_Nature_NON_T_VERTEX)
#define BPy_Nature_T_VERTEX ((PyObject *)&_Nature_T_VERTEX)
#define BPy_Nature_CUSP ((PyObject *)&_Nature_CUSP)
#define BPy_Nature_NO_FEATURE ((PyObject *)&_Nature_NO_FEATURE)
#define BPy_Nature_SILHOUETTE ((PyObject *)&_Nature_SILHOUETTE)
#define BPy_Nature_BORDER ((PyObject *)&_Nature_BORDER)
#define BPy_Nature_CREASE ((PyObject *)&_Nature_CREASE)
#define BPy_Nature_RIDGE ((PyObject *)&_Nature_RIDGE)
#define BPy_Nature_VALLEY ((PyObject *)&_Nature_VALLEY)
#define BPy_Nature_SUGGESTIVE_CONTOUR ((PyObject *)&_Nature_SUGGESTIVE_CONTOUR)
#define BPy_Nature_MATERIAL_BOUNDARY ((PyObject *)&_Nature_MATERIAL_BOUNDARY)
#define BPy_Nature_EDGE_MARK ((PyObject *)&_Nature_EDGE_MARK)

//-------------------MODULE INITIALIZATION--------------------------------
int Nature_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&Nature_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Nature_Type);
  PyModule_AddObject(module, "Nature", (PyObject *)&Nature_Type);

  // VertexNature
  PyDict_SetItemString(Nature_Type.tp_dict, "POINT", BPy_Nature_POINT);
  PyDict_SetItemString(Nature_Type.tp_dict, "S_VERTEX", BPy_Nature_S_VERTEX);
  PyDict_SetItemString(Nature_Type.tp_dict, "VIEW_VERTEX", BPy_Nature_VIEW_VERTEX);
  PyDict_SetItemString(Nature_Type.tp_dict, "NON_T_VERTEX", BPy_Nature_NON_T_VERTEX);
  PyDict_SetItemString(Nature_Type.tp_dict, "T_VERTEX", BPy_Nature_T_VERTEX);
  PyDict_SetItemString(Nature_Type.tp_dict, "CUSP", BPy_Nature_CUSP);

  // EdgeNature
  PyDict_SetItemString(Nature_Type.tp_dict, "NO_FEATURE", BPy_Nature_NO_FEATURE);
  PyDict_SetItemString(Nature_Type.tp_dict, "SILHOUETTE", BPy_Nature_SILHOUETTE);
  PyDict_SetItemString(Nature_Type.tp_dict, "BORDER", BPy_Nature_BORDER);
  PyDict_SetItemString(Nature_Type.tp_dict, "CREASE", BPy_Nature_CREASE);
  PyDict_SetItemString(Nature_Type.tp_dict, "RIDGE", BPy_Nature_RIDGE);
  PyDict_SetItemString(Nature_Type.tp_dict, "VALLEY", BPy_Nature_VALLEY);
  PyDict_SetItemString(Nature_Type.tp_dict, "SUGGESTIVE_CONTOUR", BPy_Nature_SUGGESTIVE_CONTOUR);
  PyDict_SetItemString(Nature_Type.tp_dict, "MATERIAL_BOUNDARY", BPy_Nature_MATERIAL_BOUNDARY);
  PyDict_SetItemString(Nature_Type.tp_dict, "EDGE_MARK", BPy_Nature_EDGE_MARK);

  return 0;
}

static PyObject *BPy_Nature_bitwise(PyObject *a, int op, PyObject *b)
{
  BPy_Nature *result;
  long op1, op2, v;

  if (!BPy_Nature_Check(a) || !BPy_Nature_Check(b)) {
    PyErr_SetString(PyExc_TypeError, "operands must be a Nature object");
    return nullptr;
  }

  if ((op1 = PyLong_AsLong(a)) == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_ValueError, "operand 1: unexpected Nature value");
    return nullptr;
  }
  if ((op2 = PyLong_AsLong(b)) == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_ValueError, "operand 2: unexpected Nature value");
    return nullptr;
  }
  switch (op) {
    case '&':
      v = op1 & op2;
      break;
    case '^':
      v = op1 ^ op2;
      break;
    case '|':
      v = op1 | op2;
      break;
    default:
      PyErr_BadArgument();
      return nullptr;
  }
  if (v == 0) {
    result = PyObject_NewVar(BPy_Nature, &Nature_Type, 0);
  }
  else {
    result = PyObject_NewVar(BPy_Nature, &Nature_Type, 1);
    if (result) {
      result->i.ob_digit[0] = v;
    }
  }
  return (PyObject *)result;
}

static PyObject *BPy_Nature_and(PyObject *a, PyObject *b)
{
  return BPy_Nature_bitwise(a, '&', b);
}

static PyObject *BPy_Nature_xor(PyObject *a, PyObject *b)
{
  return BPy_Nature_bitwise(a, '^', b);
}

static PyObject *BPy_Nature_or(PyObject *a, PyObject *b)
{
  return BPy_Nature_bitwise(a, '|', b);
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
