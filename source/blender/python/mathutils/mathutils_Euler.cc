/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#include <algorithm>

#include <Python.h>

#include "mathutils.hh"

#include "../generic/py_capi_utils.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#ifndef MATH_STANDALONE
#  include "BLI_dynstr.h"
#endif

#define EULER_SIZE 3

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/** Internal use, assume read callback is done. */
static const char *euler_order_str(EulerObject *self)
{
  static const char order[][4] = {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"};
  return order[self->order - EULER_ORDER_XYZ];
}

short euler_order_from_string(const char *str, const char *error_prefix)
{
  if (str[0] && str[1] && str[2] && str[3] == '\0') {

/* NOTE: this is endianness-sensitive. */
#define MAKE_ID3(a, b, c) ((a) | ((b) << 8) | ((c) << 16))

    switch (*((const PY_INT32_T *)str)) {
      case MAKE_ID3('X', 'Y', 'Z'): {
        return EULER_ORDER_XYZ;
      }
      case MAKE_ID3('X', 'Z', 'Y'): {
        return EULER_ORDER_XZY;
      }
      case MAKE_ID3('Y', 'X', 'Z'): {
        return EULER_ORDER_YXZ;
      }
      case MAKE_ID3('Y', 'Z', 'X'): {
        return EULER_ORDER_YZX;
      }
      case MAKE_ID3('Z', 'X', 'Y'): {
        return EULER_ORDER_ZXY;
      }
      case MAKE_ID3('Z', 'Y', 'X'): {
        return EULER_ORDER_ZYX;
      }
    }

#undef MAKE_ID3
  }

  PyErr_Format(PyExc_ValueError, "%s: invalid euler order '%s'", error_prefix, str);
  return -1;
}

/**
 * \note #BaseMath_ReadCallback must be called beforehand.
 */
static PyObject *Euler_to_tuple_ex(EulerObject *self, int ndigits)
{
  PyObject *ret;
  int i;

  ret = PyTuple_New(EULER_SIZE);

  if (ndigits >= 0) {
    for (i = 0; i < EULER_SIZE; i++) {
      PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round(double(self->eul[i]), ndigits)));
    }
  }
  else {
    for (i = 0; i < EULER_SIZE; i++) {
      PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->eul[i]));
    }
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: `__new__` / `mathutils.Euler()`
 * \{ */

static PyObject *Euler_vectorcall(PyObject *type,
                                  PyObject *const *args,
                                  const size_t nargsf,
                                  PyObject *kwnames)
{
  if (UNLIKELY(kwnames && PyDict_Size(kwnames))) {
    PyErr_SetString(PyExc_TypeError,
                    "mathutils.Euler(): "
                    "takes no keyword args");
    return nullptr;
  }

  float eul[EULER_SIZE] = {0.0f, 0.0f, 0.0f};
  short order = EULER_ORDER_XYZ;

  const size_t nargs = PyVectorcall_NARGS(nargsf);
  switch (nargs) {
    case 0: {
      break;
    }
    case 2: {
      const char *order_str;

      if (((order_str = PyUnicode_AsUTF8(args[1])) == nullptr) ||
          ((order = euler_order_from_string(order_str, "mathutils.Euler()")) == -1))
      {
        return nullptr;
      }
      ATTR_FALLTHROUGH;
    }
    case 1: {
      if (mathutils_array_parse(eul, EULER_SIZE, EULER_SIZE, args[0], "mathutils.Euler()") == -1) {
        return nullptr;
      }
      break;
    }
    default: {
      PyErr_Format(PyExc_TypeError,
                   "mathutils.Euler(): "
                   "takes at most 2 arguments (%zd given)",
                   nargs);
      return nullptr;
    }
  }
  return Euler_CreatePyObject(eul, order, (PyTypeObject *)type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Methods
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Euler_to_quaternion_doc,
    ".. method:: to_quaternion()\n"
    "\n"
    "   Return a quaternion representation of the euler.\n"
    "\n"
    "   :return: Quaternion representation of the euler.\n"
    "   :rtype: :class:`Quaternion`\n");
static PyObject *Euler_to_quaternion(EulerObject *self)
{
  float quat[4];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  eulO_to_quat(quat, self->eul, self->order);

  return Quaternion_CreatePyObject(quat, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    Euler_to_matrix_doc,
    ".. method:: to_matrix()\n"
    "\n"
    "   Return a matrix representation of the euler.\n"
    "\n"
    "   :return: A 3x3 rotation matrix representation of the euler.\n"
    "   :rtype: :class:`Matrix`\n");
static PyObject *Euler_to_matrix(EulerObject *self)
{
  float mat[9];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  eulO_to_mat3((float (*)[3])mat, self->eul, self->order);

  return Matrix_CreatePyObject(mat, 3, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    Euler_zero_doc,
    ".. method:: zero()\n"
    "\n"
    "   Set all values to zero.\n");
static PyObject *Euler_zero(EulerObject *self)
{
  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return nullptr;
  }

  zero_v3(self->eul);

  if (BaseMath_WriteCallback(self) == -1) {
    return nullptr;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    Euler_rotate_axis_doc,
    ".. method:: rotate_axis(axis, angle, /)\n"
    "\n"
    "   Rotates the euler a certain amount and returning a unique euler rotation\n"
    "   (no 720 degree pitches).\n"
    "\n"
    "   :arg axis: An axis string.\n"
    "   :type axis: Literal['X', 'Y', 'Z']\n"
    "   :arg angle: angle in radians.\n"
    "   :type angle: float\n");
static PyObject *Euler_rotate_axis(EulerObject *self, PyObject *args)
{
  float angle = 0.0f;
  int axis; /* actually a character */

  if (!PyArg_ParseTuple(args, "Cf:rotate_axis", &axis, &angle)) {
    PyErr_SetString(PyExc_TypeError,
                    "Euler.rotate_axis(): "
                    "expected an axis 'X', 'Y', 'Z' and an angle (float)");
    return nullptr;
  }

  if (!ELEM(axis, 'X', 'Y', 'Z')) {
    PyErr_SetString(PyExc_ValueError,
                    "Euler.rotate_axis(): "
                    "expected axis to be 'X', 'Y' or 'Z'");
    return nullptr;
  }

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  rotate_eulO(self->eul, self->order, char(axis), angle);

  (void)BaseMath_WriteCallback(self);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    Euler_rotate_doc,
    ".. method:: rotate(other, /)\n"
    "\n"
    "   Rotates the euler by another mathutils value.\n"
    "\n"
    "   :arg other: rotation component of mathutils value\n"
    "   :type other: :class:`Euler` | :class:`Quaternion` | :class:`Matrix`\n");
static PyObject *Euler_rotate(EulerObject *self, PyObject *value)
{
  float self_rmat[3][3], other_rmat[3][3], rmat[3][3];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  if (mathutils_any_to_rotmat(other_rmat, value, "euler.rotate(value)") == -1) {
    return nullptr;
  }

  eulO_to_mat3(self_rmat, self->eul, self->order);
  mul_m3_m3m3(rmat, other_rmat, self_rmat);

  mat3_to_compatible_eulO(self->eul, self->eul, self->order, rmat);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    Euler_make_compatible_doc,
    ".. method:: make_compatible(other, /)\n"
    "\n"
    "   Make this euler compatible with another,\n"
    "   so interpolating between them works as intended.\n"
    "\n"
    "   :arg other: Other euler rotation.\n"
    "   :type other: :class:`Euler`\n"
    "\n"
    "   .. note:: the rotation order is not taken into account for this function.\n");
static PyObject *Euler_make_compatible(EulerObject *self, PyObject *value)
{
  float teul[EULER_SIZE];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse(teul,
                            EULER_SIZE,
                            EULER_SIZE,
                            value,
                            "euler.make_compatible(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  compatible_eul(self->eul, teul);

  (void)BaseMath_WriteCallback(self);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    Euler_copy_doc,
    ".. function:: copy()\n"
    "\n"
    "   Returns a copy of this euler.\n"
    "\n"
    "   :return: A copy of the euler.\n"
    "   :rtype: :class:`Euler`\n"
    "\n"
    "   .. note:: use this to get a copy of a wrapped euler with\n"
    "      no reference to the original data.\n");
static PyObject *Euler_copy(EulerObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  return Euler_CreatePyObject(self->eul, self->order, Py_TYPE(self));
}
static PyObject *Euler_deepcopy(EulerObject *self, PyObject *args)
{
  if (!PyC_CheckArgs_DeepCopy(args)) {
    return nullptr;
  }
  return Euler_copy(self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: `__repr__` & `__str__`
 * \{ */

static PyObject *Euler_repr(EulerObject *self)
{
  PyObject *ret, *tuple;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  tuple = Euler_to_tuple_ex(self, -1);

  ret = PyUnicode_FromFormat("Euler(%R, '%s')", tuple, euler_order_str(self));

  Py_DECREF(tuple);
  return ret;
}

#ifndef MATH_STANDALONE
static PyObject *Euler_str(EulerObject *self)
{
  DynStr *ds;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  ds = BLI_dynstr_new();

  BLI_dynstr_appendf(ds,
                     "<Euler (x=%.4f, y=%.4f, z=%.4f), order='%s'>",
                     self->eul[0],
                     self->eul[1],
                     self->eul[2],
                     euler_order_str(self));

  return mathutils_dynstr_to_py(ds); /* frees ds */
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Buffer Protocol
 * \{ */

static int Euler_getbuffer(PyObject *obj, Py_buffer *view, int flags)
{
  EulerObject *self = (EulerObject *)obj;
  if (UNLIKELY(BaseMath_Prepare_ForBufferAccess(self, view, flags) == -1)) {
    return -1;
  }
  if (UNLIKELY(BaseMath_ReadCallback(self) == -1)) {
    return -1;
  }

  memset(view, 0, sizeof(*view));

  view->obj = (PyObject *)self;
  view->buf = (void *)self->eul;
  view->len = Py_ssize_t(EULER_SIZE * sizeof(float));
  view->itemsize = sizeof(float);
  view->ndim = 1;
  if ((flags & PyBUF_WRITABLE) == 0) {
    view->readonly = 1;
  }
  if (flags & PyBUF_FORMAT) {
    view->format = (char *)"f";
  }

  self->flag |= BASE_MATH_FLAG_HAS_BUFFER_VIEW;

  Py_INCREF(self);
  return 0;
}

static void Euler_releasebuffer(PyObject * /*exporter*/, Py_buffer *view)
{
  EulerObject *self = (EulerObject *)view->obj;
  self->flag &= ~BASE_MATH_FLAG_HAS_BUFFER_VIEW;

  if (view->readonly == 0) {
    if (UNLIKELY(BaseMath_WriteCallback(self) == -1)) {
      PyErr_Print();
    }
  }
}

static PyBufferProcs Euler_as_buffer = {
    (getbufferproc)Euler_getbuffer,
    (releasebufferproc)Euler_releasebuffer,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Rich Compare
 * \{ */

static PyObject *Euler_richcmpr(PyObject *a, PyObject *b, int op)
{
  PyObject *res;
  int ok = -1; /* zero is true */

  if (EulerObject_Check(a) && EulerObject_Check(b)) {
    EulerObject *eulA = (EulerObject *)a;
    EulerObject *eulB = (EulerObject *)b;

    if (BaseMath_ReadCallback(eulA) == -1 || BaseMath_ReadCallback(eulB) == -1) {
      return nullptr;
    }

    ok = ((eulA->order == eulB->order) &&
          EXPP_VectorsAreEqual(eulA->eul, eulB->eul, EULER_SIZE, 1)) ?
             0 :
             -1;
  }

  switch (op) {
    case Py_NE: {
      ok = !ok;
      ATTR_FALLTHROUGH;
    }
    case Py_EQ: {
      res = ok ? Py_False : Py_True;
      break;
    }
    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE: {
      res = Py_NotImplemented;
      break;
    }
    default: {
      PyErr_BadArgument();
      return nullptr;
    }
  }

  return Py_NewRef(res);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Hash (`__hash__`)
 * \{ */

static Py_hash_t Euler_hash(EulerObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }

  if (BaseMathObject_Prepare_ForHash(self) == -1) {
    return -1;
  }

  return mathutils_array_hash(self->eul, EULER_SIZE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Sequence Protocol
 * \{ */

/** Sequence length: `len(object)`. */
static Py_ssize_t Euler_len(EulerObject * /*self*/)
{
  return EULER_SIZE;
}

/** Sequence accessor (get): `x = object[i]`. */
static PyObject *Euler_item(EulerObject *self, Py_ssize_t i)
{
  if (i < 0) {
    i = EULER_SIZE - i;
  }

  if (i < 0 || i >= EULER_SIZE) {
    PyErr_SetString(PyExc_IndexError,
                    "euler[attribute]: "
                    "array index out of range");
    return nullptr;
  }

  if (BaseMath_ReadIndexCallback(self, i) == -1) {
    return nullptr;
  }

  return PyFloat_FromDouble(self->eul[i]);
}

/** Sequence accessor (set): `object[i] = x`. */
static int Euler_ass_item(EulerObject *self, Py_ssize_t i, PyObject *value)
{
  float f;

  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return -1;
  }

  f = PyFloat_AsDouble(value);
  if (f == -1 && PyErr_Occurred()) { /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError,
                    "euler[attribute] = x: "
                    "assigned value not a number");
    return -1;
  }

  if (i < 0) {
    i = EULER_SIZE - i;
  }

  if (i < 0 || i >= EULER_SIZE) {
    PyErr_SetString(PyExc_IndexError,
                    "euler[attribute] = x: "
                    "array assignment index out of range");
    return -1;
  }

  self->eul[i] = f;

  if (BaseMath_WriteIndexCallback(self, i) == -1) {
    return -1;
  }

  return 0;
}

/** Sequence slice accessor (get): `x = object[i:j]`. */
static PyObject *Euler_slice(EulerObject *self, int begin, int end)
{
  PyObject *tuple;
  int count;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  CLAMP(begin, 0, EULER_SIZE);
  if (end < 0) {
    end = (EULER_SIZE + 1) + end;
  }
  CLAMP(end, 0, EULER_SIZE);
  begin = std::min(begin, end);

  tuple = PyTuple_New(end - begin);
  for (count = begin; count < end; count++) {
    PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->eul[count]));
  }

  return tuple;
}

/** Sequence slice accessor (set): `object[i:j] = x`. */
static int Euler_ass_slice(EulerObject *self, int begin, int end, PyObject *seq)
{
  int i, size;
  float eul[EULER_SIZE];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  CLAMP(begin, 0, EULER_SIZE);
  if (end < 0) {
    end = (EULER_SIZE + 1) + end;
  }
  CLAMP(end, 0, EULER_SIZE);
  begin = std::min(begin, end);

  if ((size = mathutils_array_parse(eul, 0, EULER_SIZE, seq, "mathutils.Euler[begin:end] = []")) ==
      -1)
  {
    return -1;
  }

  if (size != (end - begin)) {
    PyErr_SetString(PyExc_ValueError,
                    "euler[begin:end] = []: "
                    "size mismatch in slice assignment");
    return -1;
  }

  for (i = 0; i < EULER_SIZE; i++) {
    self->eul[begin + i] = eul[i];
  }

  (void)BaseMath_WriteCallback(self);
  return 0;
}

/** Sequence generic subscript (get): `x = object[...]`. */
static PyObject *Euler_subscript(EulerObject *self, PyObject *item)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i;
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }
    if (i < 0) {
      i += EULER_SIZE;
    }
    return Euler_item(self, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, EULER_SIZE, &start, &stop, &step, &slicelength) < 0) {
      return nullptr;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return Euler_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with eulers");
    return nullptr;
  }

  PyErr_Format(
      PyExc_TypeError, "euler indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return nullptr;
}

/** Sequence generic subscript (set): `object[...] = x`. */
static int Euler_ass_subscript(EulerObject *self, PyObject *item, PyObject *value)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }
    if (i < 0) {
      i += EULER_SIZE;
    }
    return Euler_ass_item(self, i, value);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, EULER_SIZE, &start, &stop, &step, &slicelength) < 0) {
      return -1;
    }

    if (step == 1) {
      return Euler_ass_slice(self, start, stop, value);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with euler");
    return -1;
  }

  PyErr_Format(
      PyExc_TypeError, "euler indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Sequence & Mapping Protocol Declarations
 * \{ */

static PySequenceMethods Euler_SeqMethods = {
    /*sq_length*/ (lenfunc)Euler_len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ (ssizeargfunc)Euler_item,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ (ssizeobjargproc)Euler_ass_item,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ nullptr,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods Euler_AsMapping = {
    /*mp_length*/ (lenfunc)Euler_len,
    /*mp_subscript*/ (binaryfunc)Euler_subscript,
    /*mp_ass_subscript*/ (objobjargproc)Euler_ass_subscript,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Get/Set Item Implementation
 * \{ */

/* Euler axis: `euler.x/y/z`. */

PyDoc_STRVAR(
    /* Wrap. */
    Euler_axis_doc,
    "Euler axis angle in radians.\n"
    "\n"
    ":type: float\n");
static PyObject *Euler_axis_get(EulerObject *self, void *type)
{
  return Euler_item(self, POINTER_AS_INT(type));
}

static int Euler_axis_set(EulerObject *self, PyObject *value, void *type)
{
  return Euler_ass_item(self, POINTER_AS_INT(type), value);
}

/* Euler rotation order: `euler.order`. */

PyDoc_STRVAR(
    /* Wrap. */
    Euler_order_doc,
    "Euler rotation order.\n"
    "\n"
    ":type: Literal['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX']\n");
static PyObject *Euler_order_get(EulerObject *self, void * /*closure*/)
{
  if (BaseMath_ReadCallback(self) == -1) {
    /* can read order too */
    return nullptr;
  }

  return PyUnicode_FromString(euler_order_str(self));
}

static int Euler_order_set(EulerObject *self, PyObject *value, void * /*closure*/)
{
  const char *order_str;
  short order;

  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return -1;
  }

  if (((order_str = PyUnicode_AsUTF8(value)) == nullptr) ||
      ((order = euler_order_from_string(order_str, "euler.order")) == -1))
  {
    return -1;
  }

  self->order = order;
  (void)BaseMath_WriteCallback(self); /* order can be written back */
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Get/Set Item Definitions
 * \{ */

static PyGetSetDef Euler_getseters[] = {
    {"x", (getter)Euler_axis_get, (setter)Euler_axis_set, Euler_axis_doc, POINTER_FROM_INT(0)},
    {"y", (getter)Euler_axis_get, (setter)Euler_axis_set, Euler_axis_doc, POINTER_FROM_INT(1)},
    {"z", (getter)Euler_axis_get, (setter)Euler_axis_set, Euler_axis_doc, POINTER_FROM_INT(2)},
    {"order", (getter)Euler_order_get, (setter)Euler_order_set, Euler_order_doc, nullptr},

    {"is_wrapped",
     (getter)BaseMathObject_is_wrapped_get,
     (setter) nullptr,
     BaseMathObject_is_wrapped_doc,
     nullptr},
    {"is_frozen",
     (getter)BaseMathObject_is_frozen_get,
     (setter) nullptr,
     BaseMathObject_is_frozen_doc,
     nullptr},
    {"is_valid",
     (getter)BaseMathObject_is_valid_get,
     (setter) nullptr,
     BaseMathObject_is_valid_doc,
     nullptr},
    {"owner",
     (getter)BaseMathObject_owner_get,
     (setter) nullptr,
     BaseMathObject_owner_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Method Definitions
 * \{ */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef Euler_methods[] = {
    {"zero", (PyCFunction)Euler_zero, METH_NOARGS, Euler_zero_doc},
    {"to_matrix", (PyCFunction)Euler_to_matrix, METH_NOARGS, Euler_to_matrix_doc},
    {"to_quaternion", (PyCFunction)Euler_to_quaternion, METH_NOARGS, Euler_to_quaternion_doc},
    {"rotate_axis", (PyCFunction)Euler_rotate_axis, METH_VARARGS, Euler_rotate_axis_doc},
    {"rotate", (PyCFunction)Euler_rotate, METH_O, Euler_rotate_doc},
    {"make_compatible", (PyCFunction)Euler_make_compatible, METH_O, Euler_make_compatible_doc},
    {"copy", (PyCFunction)Euler_copy, METH_NOARGS, Euler_copy_doc},
    {"__copy__", (PyCFunction)Euler_copy, METH_NOARGS, Euler_copy_doc},
    {"__deepcopy__", (PyCFunction)Euler_deepcopy, METH_VARARGS, Euler_copy_doc},

    /* base-math methods */
    {"freeze", (PyCFunction)BaseMathObject_freeze, METH_NOARGS, BaseMathObject_freeze_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: Python Object Definition
 * \{ */

#ifdef MATH_STANDALONE
#  define Euler_str nullptr
#endif

PyDoc_STRVAR(
    /* Wrap. */
    euler_doc,
    ".. class:: Euler(angles=(0.0, 0.0, 0.0), order='XYZ', /)\n"
    "\n"
    "   This object gives access to Eulers in Blender.\n"
    "\n"
    "   .. seealso:: `Euler angles <https://en.wikipedia.org/wiki/Euler_angles>`__ on "
    "Wikipedia.\n"
    "\n"
    "   :arg angles: (X, Y, Z) angles in radians.\n"
    "   :type angles: Sequence[float]\n"
    "   :arg order: Euler rotation order.\n"
    "   :type order: Literal['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX']\n");
PyTypeObject euler_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Euler",
    /*tp_basicsize*/ sizeof(EulerObject),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)BaseMathObject_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)Euler_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ &Euler_SeqMethods,
    /*tp_as_mapping*/ &Euler_AsMapping,
    /*tp_hash*/ (hashfunc)Euler_hash,
    /*tp_call*/ nullptr,
    /*tp_str*/ (reprfunc)Euler_str,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ &Euler_as_buffer,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ euler_doc,
    /*tp_traverse*/ (traverseproc)BaseMathObject_traverse,
    /*tp_clear*/ (inquiry)BaseMathObject_clear,
    /*tp_richcompare*/ (richcmpfunc)Euler_richcmpr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ Euler_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ Euler_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ (inquiry)BaseMathObject_is_gc,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ Euler_vectorcall,
};

#ifdef MATH_STANDALONE
#  undef Euler_str
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Euler Type: C/API Constructors
 * \{ */

PyObject *Euler_CreatePyObject(const float eul[3], const short order, PyTypeObject *base_type)
{
  EulerObject *self;
  float *eul_alloc;

  eul_alloc = static_cast<float *>(PyMem_Malloc(EULER_SIZE * sizeof(float)));
  if (UNLIKELY(eul_alloc == nullptr)) {
    PyErr_SetString(PyExc_MemoryError,
                    "Euler(): "
                    "problem allocating data");
    return nullptr;
  }

  self = BASE_MATH_NEW(EulerObject, euler_Type, base_type);
  if (self) {
    self->eul = eul_alloc;

    /* init callbacks as nullptr */
    self->cb_user = nullptr;
    self->cb_type = self->cb_subtype = 0;

    if (eul) {
      copy_v3_v3(self->eul, eul);
    }
    else {
      zero_v3(self->eul);
    }

    self->flag = BASE_MATH_FLAG_DEFAULT;
    self->order = order;
  }
  else {
    PyMem_Free(eul_alloc);
  }

  return (PyObject *)self;
}

PyObject *Euler_CreatePyObject_wrap(float eul[3], const short order, PyTypeObject *base_type)
{
  EulerObject *self;

  self = BASE_MATH_NEW(EulerObject, euler_Type, base_type);
  if (self) {
    /* init callbacks as nullptr */
    self->cb_user = nullptr;
    self->cb_type = self->cb_subtype = 0;

    self->eul = eul;
    self->flag = BASE_MATH_FLAG_DEFAULT | BASE_MATH_FLAG_IS_WRAP;

    self->order = order;
  }

  return (PyObject *)self;
}

PyObject *Euler_CreatePyObject_cb(PyObject *cb_user,
                                  const short order,
                                  uchar cb_type,
                                  uchar cb_subtype)
{
  EulerObject *self = (EulerObject *)Euler_CreatePyObject(nullptr, order, nullptr);
  if (self) {
    Py_INCREF(cb_user);
    self->cb_user = cb_user;
    self->cb_type = cb_type;
    self->cb_subtype = cb_subtype;
    BLI_assert(!PyObject_GC_IsTracked((PyObject *)self));
    PyObject_GC_Track(self);
  }

  return (PyObject *)self;
}

/** \} */
