/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_Nature.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

static PyObject *BPy_Nature_and(PyObject *a, PyObject *b);
static PyObject *BPy_Nature_xor(PyObject *a, PyObject *b);
static PyObject *BPy_Nature_or(PyObject *a, PyObject *b);

/*-----------------------BPy_Nature number method definitions --------------------*/

static PyNumberMethods nature_as_number = {
    /*nb_add*/ nullptr,
    /*nb_subtract*/ nullptr,
    /*nb_multiply*/ nullptr,
    /*nb_remainder*/ nullptr,
    /*nb_divmod*/ nullptr,
    /*nb_power*/ nullptr,
    /*nb_negative*/ nullptr,
    /*nb_positive*/ nullptr,
    /*nb_absolute*/ nullptr,
    /*nb_bool*/ nullptr,
    /*nb_invert*/ nullptr,
    /*nb_lshift*/ nullptr,
    /*nb_rshift*/ nullptr,
    /*nb_and*/ (binaryfunc)BPy_Nature_and,
    /*nb_xor*/ (binaryfunc)BPy_Nature_xor,
    /*nb_or*/ (binaryfunc)BPy_Nature_or,
    /*nb_int*/ nullptr,
    /*nb_reserved*/ nullptr,
    /*nb_float*/ nullptr,
    /*nb_inplace_add*/ nullptr,
    /*nb_inplace_subtract*/ nullptr,
    /*nb_inplace_multiply*/ nullptr,
    /*nb_inplace_remainder*/ nullptr,
    /*nb_inplace_power*/ nullptr,
    /*nb_inplace_lshift*/ nullptr,
    /*nb_inplace_rshift*/ nullptr,
    /*nb_inplace_and*/ nullptr,
    /*nb_inplace_xor*/ nullptr,
    /*nb_inplace_or*/ nullptr,
    /*nb_floor_divide*/ nullptr,
    /*nb_true_divide*/ nullptr,
    /*nb_inplace_floor_divide*/ nullptr,
    /*nb_inplace_true_divide*/ nullptr,
    /*nb_index*/ nullptr,
    /*nb_matrix_multiply*/ nullptr,
    /*nb_inplace_matrix_multiply*/ nullptr,
};

/*-----------------------BPy_Nature doc-string -----------------------------------*/

PyDoc_STRVAR(
    /* Wrap. */
    Nature_doc,
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
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Nature",
    /*tp_basicsize*/ sizeof(PyLongObject),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ &nature_as_number,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ Nature_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &PyLong_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

/*-----------------------BPy_Nature instance definitions ----------------------------------*/

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

#define ADD_TYPE_CONST(id) \
  PyLong_subtype_add_to_dict(Nature_Type.tp_dict, &Nature_Type, STRINGIFY(id), Nature::id)

  // VertexNature
  ADD_TYPE_CONST(POINT);
  ADD_TYPE_CONST(S_VERTEX);
  ADD_TYPE_CONST(VIEW_VERTEX);
  ADD_TYPE_CONST(NON_T_VERTEX);
  ADD_TYPE_CONST(T_VERTEX);
  ADD_TYPE_CONST(CUSP);

  // EdgeNature
  ADD_TYPE_CONST(NO_FEATURE);
  ADD_TYPE_CONST(SILHOUETTE);
  ADD_TYPE_CONST(BORDER);
  ADD_TYPE_CONST(CREASE);
  ADD_TYPE_CONST(RIDGE);
  ADD_TYPE_CONST(VALLEY);
  ADD_TYPE_CONST(SUGGESTIVE_CONTOUR);
  ADD_TYPE_CONST(MATERIAL_BOUNDARY);
  ADD_TYPE_CONST(EDGE_MARK);

#undef ADD_TYPE_CONST

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
    result = (BPy_Nature *)PyLong_subtype_new(&Nature_Type, v);
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
