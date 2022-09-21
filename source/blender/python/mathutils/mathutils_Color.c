/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#include <Python.h>

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#ifndef MATH_STANDALONE
#  include "IMB_colormanagement.h"
#endif

#ifndef MATH_STANDALONE
#  include "BLI_dynstr.h"
#endif

#define COLOR_SIZE 3

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * \note #BaseMath_ReadCallback must be called beforehand.
 */
static PyObject *Color_to_tuple_ex(ColorObject *self, int ndigits)
{
  PyObject *ret;
  int i;

  ret = PyTuple_New(COLOR_SIZE);

  if (ndigits >= 0) {
    for (i = 0; i < COLOR_SIZE; i++) {
      PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round((double)self->col[i], ndigits)));
    }
  }
  else {
    for (i = 0; i < COLOR_SIZE; i++) {
      PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->col[i]));
    }
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: `__new__` / `mathutils.Color()`
 * \{ */

static PyObject *Color_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  float col[3] = {0.0f, 0.0f, 0.0f};

  if (kwds && PyDict_Size(kwds)) {
    PyErr_SetString(PyExc_TypeError,
                    "mathutils.Color(): "
                    "takes no keyword args");
    return NULL;
  }

  switch (PyTuple_GET_SIZE(args)) {
    case 0:
      break;
    case 1:
      if ((mathutils_array_parse(
              col, COLOR_SIZE, COLOR_SIZE, PyTuple_GET_ITEM(args, 0), "mathutils.Color()")) ==
          -1) {
        return NULL;
      }
      break;
    default:
      PyErr_SetString(PyExc_TypeError,
                      "mathutils.Color(): "
                      "more than a single arg given");
      return NULL;
  }
  return Color_CreatePyObject(col, type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Methods: Color Space Conversion
 * \{ */

#ifndef MATH_STANDALONE

PyDoc_STRVAR(Color_from_scene_linear_to_srgb_doc,
             ".. function:: from_scene_linear_to_srgb()\n"
             "\n"
             "   Convert from scene linear to sRGB color space.\n"
             "\n"
             "   :return: A color in sRGB color space.\n"
             "   :rtype: :class:`Color`\n");
static PyObject *Color_from_scene_linear_to_srgb(ColorObject *self)
{
  float col[3];
  IMB_colormanagement_scene_linear_to_srgb_v3(col, self->col);
  return Color_CreatePyObject(col, Py_TYPE(self));
}

PyDoc_STRVAR(Color_from_srgb_to_scene_linear_doc,
             ".. function:: from_srgb_to_scene_linear()\n"
             "\n"
             "   Convert from sRGB to scene linear color space.\n"
             "\n"
             "   :return: A color in scene linear color space.\n"
             "   :rtype: :class:`Color`\n");
static PyObject *Color_from_srgb_to_scene_linear(ColorObject *self)
{
  float col[3];
  IMB_colormanagement_srgb_to_scene_linear_v3(col, self->col);
  return Color_CreatePyObject(col, Py_TYPE(self));
}

PyDoc_STRVAR(Color_from_scene_linear_to_xyz_d65_doc,
             ".. function:: from_scene_linear_to_xyz_d65()\n"
             "\n"
             "   Convert from scene linear to CIE XYZ (Illuminant D65) color space.\n"
             "\n"
             "   :return: A color in XYZ color space.\n"
             "   :rtype: :class:`Color`\n");
static PyObject *Color_from_scene_linear_to_xyz_d65(ColorObject *self)
{
  float col[3];
  IMB_colormanagement_scene_linear_to_xyz(col, self->col);
  return Color_CreatePyObject(col, Py_TYPE(self));
}

PyDoc_STRVAR(Color_from_xyz_d65_to_scene_linear_doc,
             ".. function:: from_xyz_d65_to_scene_linear()\n"
             "\n"
             "   Convert from CIE XYZ (Illuminant D65) to scene linear color space.\n"
             "\n"
             "   :return: A color in scene linear color space.\n"
             "   :rtype: :class:`Color`\n");
static PyObject *Color_from_xyz_d65_to_scene_linear(ColorObject *self)
{
  float col[3];
  IMB_colormanagement_xyz_to_scene_linear(col, self->col);
  return Color_CreatePyObject(col, Py_TYPE(self));
}

PyDoc_STRVAR(Color_from_scene_linear_to_aces_doc,
             ".. function:: from_scene_linear_to_aces()\n"
             "\n"
             "   Convert from scene linear to ACES2065-1 linear color space.\n"
             "\n"
             "   :return: A color in ACES2065-1 linear color space.\n"
             "   :rtype: :class:`Color`\n");
static PyObject *Color_from_scene_linear_to_aces(ColorObject *self)
{
  float col[3];
  IMB_colormanagement_scene_linear_to_aces(col, self->col);
  return Color_CreatePyObject(col, Py_TYPE(self));
}

PyDoc_STRVAR(Color_from_aces_to_scene_linear_doc,
             ".. function:: from_aces_to_scene_linear()\n"
             "\n"
             "   Convert from ACES2065-1 linear to scene linear color space.\n"
             "\n"
             "   :return: A color in scene linear color space.\n"
             "   :rtype: :class:`Color`\n");
static PyObject *Color_from_aces_to_scene_linear(ColorObject *self)
{
  float col[3];
  IMB_colormanagement_aces_to_scene_linear(col, self->col);
  return Color_CreatePyObject(col, Py_TYPE(self));
}

PyDoc_STRVAR(Color_from_scene_linear_to_rec709_linear_doc,
             ".. function:: from_scene_linear_to_rec709_linear()\n"
             "\n"
             "   Convert from scene linear to Rec.709 linear color space.\n"
             "\n"
             "   :return: A color in Rec.709 linear color space.\n"
             "   :rtype: :class:`Color`\n");
static PyObject *Color_from_scene_linear_to_rec709_linear(ColorObject *self)
{
  float col[3];
  IMB_colormanagement_scene_linear_to_rec709(col, self->col);
  return Color_CreatePyObject(col, Py_TYPE(self));
}

PyDoc_STRVAR(Color_from_rec709_linear_to_scene_linear_doc,
             ".. function:: from_rec709_linear_to_scene_linear()\n"
             "\n"
             "   Convert from Rec.709 linear color space to scene linear color space.\n"
             "\n"
             "   :return: A color in scene linear color space.\n"
             "   :rtype: :class:`Color`\n");
static PyObject *Color_from_rec709_linear_to_scene_linear(ColorObject *self)
{
  float col[3];
  IMB_colormanagement_rec709_to_scene_linear(col, self->col);
  return Color_CreatePyObject(col, Py_TYPE(self));
}

#endif /* MATH_STANDALONE */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Methods: Color Copy/Deep-Copy
 * \{ */

PyDoc_STRVAR(Color_copy_doc,
             ".. function:: copy()\n"
             "\n"
             "   Returns a copy of this color.\n"
             "\n"
             "   :return: A copy of the color.\n"
             "   :rtype: :class:`Color`\n"
             "\n"
             "   .. note:: use this to get a copy of a wrapped color with\n"
             "      no reference to the original data.\n");
static PyObject *Color_copy(ColorObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  return Color_CreatePyObject(self->col, Py_TYPE(self));
}
static PyObject *Color_deepcopy(ColorObject *self, PyObject *args)
{
  if (!PyC_CheckArgs_DeepCopy(args)) {
    return NULL;
  }
  return Color_copy(self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: `__repr__` & `__str__`
 * \{ */

static PyObject *Color_repr(ColorObject *self)
{
  PyObject *ret, *tuple;

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  tuple = Color_to_tuple_ex(self, -1);

  ret = PyUnicode_FromFormat("Color(%R)", tuple);

  Py_DECREF(tuple);
  return ret;
}

#ifndef MATH_STANDALONE
static PyObject *Color_str(ColorObject *self)
{
  DynStr *ds;

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  ds = BLI_dynstr_new();

  BLI_dynstr_appendf(
      ds, "<Color (r=%.4f, g=%.4f, b=%.4f)>", self->col[0], self->col[1], self->col[2]);

  return mathutils_dynstr_to_py(ds); /* frees ds */
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Rich Compare
 * \{ */

static PyObject *Color_richcmpr(PyObject *a, PyObject *b, int op)
{
  PyObject *res;
  int ok = -1; /* zero is true */

  if (ColorObject_Check(a) && ColorObject_Check(b)) {
    ColorObject *colA = (ColorObject *)a;
    ColorObject *colB = (ColorObject *)b;

    if (BaseMath_ReadCallback(colA) == -1 || BaseMath_ReadCallback(colB) == -1) {
      return NULL;
    }

    ok = EXPP_VectorsAreEqual(colA->col, colB->col, COLOR_SIZE, 1) ? 0 : -1;
  }

  switch (op) {
    case Py_NE:
      ok = !ok;
      ATTR_FALLTHROUGH;
    case Py_EQ:
      res = ok ? Py_False : Py_True;
      break;

    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE:
      res = Py_NotImplemented;
      break;
    default:
      PyErr_BadArgument();
      return NULL;
  }

  return Py_INCREF_RET(res);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Hash (`__hash__`)
 * \{ */

static Py_hash_t Color_hash(ColorObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }

  if (BaseMathObject_Prepare_ForHash(self) == -1) {
    return -1;
  }

  return mathutils_array_hash(self->col, COLOR_SIZE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Sequence & Mapping Protocols Implementation
 * \{ */

/** Sequence length: `len(object)`. */
static int Color_len(ColorObject *UNUSED(self))
{
  return COLOR_SIZE;
}

/** Sequence accessor (get): `x = object[i]`. */
static PyObject *Color_item(ColorObject *self, int i)
{
  if (i < 0) {
    i = COLOR_SIZE - i;
  }

  if (i < 0 || i >= COLOR_SIZE) {
    PyErr_SetString(PyExc_IndexError,
                    "color[item]: "
                    "array index out of range");
    return NULL;
  }

  if (BaseMath_ReadIndexCallback(self, i) == -1) {
    return NULL;
  }

  return PyFloat_FromDouble(self->col[i]);
}

/** Sequence accessor (set): `object[i] = x`. */
static int Color_ass_item(ColorObject *self, int i, PyObject *value)
{
  float f;

  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return -1;
  }

  f = PyFloat_AsDouble(value);
  if (f == -1 && PyErr_Occurred()) { /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError,
                    "color[item] = x: "
                    "assigned value not a number");
    return -1;
  }

  if (i < 0) {
    i = COLOR_SIZE - i;
  }

  if (i < 0 || i >= COLOR_SIZE) {
    PyErr_SetString(PyExc_IndexError,
                    "color[item] = x: "
                    "array assignment index out of range");
    return -1;
  }

  self->col[i] = f;

  if (BaseMath_WriteIndexCallback(self, i) == -1) {
    return -1;
  }

  return 0;
}

/** Sequence slice accessor (get): `x = object[i:j]`. */
static PyObject *Color_slice(ColorObject *self, int begin, int end)
{
  PyObject *tuple;
  int count;

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  CLAMP(begin, 0, COLOR_SIZE);
  if (end < 0) {
    end = (COLOR_SIZE + 1) + end;
  }
  CLAMP(end, 0, COLOR_SIZE);
  begin = MIN2(begin, end);

  tuple = PyTuple_New(end - begin);
  for (count = begin; count < end; count++) {
    PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->col[count]));
  }

  return tuple;
}

/** Sequence slice accessor (set): `object[i:j] = x`. */
static int Color_ass_slice(ColorObject *self, int begin, int end, PyObject *seq)
{
  int i, size;
  float col[COLOR_SIZE];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  CLAMP(begin, 0, COLOR_SIZE);
  if (end < 0) {
    end = (COLOR_SIZE + 1) + end;
  }
  CLAMP(end, 0, COLOR_SIZE);
  begin = MIN2(begin, end);

  if ((size = mathutils_array_parse(col, 0, COLOR_SIZE, seq, "mathutils.Color[begin:end] = []")) ==
      -1) {
    return -1;
  }

  if (size != (end - begin)) {
    PyErr_SetString(PyExc_ValueError,
                    "color[begin:end] = []: "
                    "size mismatch in slice assignment");
    return -1;
  }

  for (i = 0; i < COLOR_SIZE; i++) {
    self->col[begin + i] = col[i];
  }

  (void)BaseMath_WriteCallback(self);
  return 0;
}

/** Sequence generic subscript (get): `x = object[...]`. */
static PyObject *Color_subscript(ColorObject *self, PyObject *item)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i;
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return NULL;
    }
    if (i < 0) {
      i += COLOR_SIZE;
    }
    return Color_item(self, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, COLOR_SIZE, &start, &stop, &step, &slicelength) < 0) {
      return NULL;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return Color_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with color");
    return NULL;
  }

  PyErr_Format(
      PyExc_TypeError, "color indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return NULL;
}

/** Sequence generic subscript (set): `object[...] = x`. */
static int Color_ass_subscript(ColorObject *self, PyObject *item, PyObject *value)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }
    if (i < 0) {
      i += COLOR_SIZE;
    }
    return Color_ass_item(self, i, value);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, COLOR_SIZE, &start, &stop, &step, &slicelength) < 0) {
      return -1;
    }

    if (step == 1) {
      return Color_ass_slice(self, start, stop, value);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with color");
    return -1;
  }

  PyErr_Format(
      PyExc_TypeError, "color indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Numeric Protocol Implementation
 * \{ */

/** Addition: `object + object`. */
static PyObject *Color_add(PyObject *v1, PyObject *v2)
{
  ColorObject *color1 = NULL, *color2 = NULL;
  float col[COLOR_SIZE];

  if (!ColorObject_Check(v1) || !ColorObject_Check(v2)) {
    PyErr_Format(PyExc_TypeError,
                 "Color addition: (%s + %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return NULL;
  }
  color1 = (ColorObject *)v1;
  color2 = (ColorObject *)v2;

  if (BaseMath_ReadCallback(color1) == -1 || BaseMath_ReadCallback(color2) == -1) {
    return NULL;
  }

  add_vn_vnvn(col, color1->col, color2->col, COLOR_SIZE);

  return Color_CreatePyObject(col, Py_TYPE(v1));
}

/** Addition in-place: `object += object`. */
static PyObject *Color_iadd(PyObject *v1, PyObject *v2)
{
  ColorObject *color1 = NULL, *color2 = NULL;

  if (!ColorObject_Check(v1) || !ColorObject_Check(v2)) {
    PyErr_Format(PyExc_TypeError,
                 "Color addition: (%s += %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return NULL;
  }
  color1 = (ColorObject *)v1;
  color2 = (ColorObject *)v2;

  if (BaseMath_ReadCallback_ForWrite(color1) == -1 || BaseMath_ReadCallback(color2) == -1) {
    return NULL;
  }

  add_vn_vn(color1->col, color2->col, COLOR_SIZE);

  (void)BaseMath_WriteCallback(color1);
  Py_INCREF(v1);
  return v1;
}

/** Subtraction: `object - object`. */
static PyObject *Color_sub(PyObject *v1, PyObject *v2)
{
  ColorObject *color1 = NULL, *color2 = NULL;
  float col[COLOR_SIZE];

  if (!ColorObject_Check(v1) || !ColorObject_Check(v2)) {
    PyErr_Format(PyExc_TypeError,
                 "Color subtraction: (%s - %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return NULL;
  }
  color1 = (ColorObject *)v1;
  color2 = (ColorObject *)v2;

  if (BaseMath_ReadCallback(color1) == -1 || BaseMath_ReadCallback(color2) == -1) {
    return NULL;
  }

  sub_vn_vnvn(col, color1->col, color2->col, COLOR_SIZE);

  return Color_CreatePyObject(col, Py_TYPE(v1));
}

/** Subtraction in-place: `object -= object`. */
static PyObject *Color_isub(PyObject *v1, PyObject *v2)
{
  ColorObject *color1 = NULL, *color2 = NULL;

  if (!ColorObject_Check(v1) || !ColorObject_Check(v2)) {
    PyErr_Format(PyExc_TypeError,
                 "Color subtraction: (%s -= %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return NULL;
  }
  color1 = (ColorObject *)v1;
  color2 = (ColorObject *)v2;

  if (BaseMath_ReadCallback_ForWrite(color1) == -1 || BaseMath_ReadCallback(color2) == -1) {
    return NULL;
  }

  sub_vn_vn(color1->col, color2->col, COLOR_SIZE);

  (void)BaseMath_WriteCallback(color1);
  Py_INCREF(v1);
  return v1;
}

static PyObject *color_mul_float(ColorObject *color, const float scalar)
{
  float tcol[COLOR_SIZE];
  mul_vn_vn_fl(tcol, color->col, COLOR_SIZE, scalar);
  return Color_CreatePyObject(tcol, Py_TYPE(color));
}

/** Multiplication: `object * object`. */
static PyObject *Color_mul(PyObject *v1, PyObject *v2)
{
  ColorObject *color1 = NULL, *color2 = NULL;
  float scalar;

  if (ColorObject_Check(v1)) {
    color1 = (ColorObject *)v1;
    if (BaseMath_ReadCallback(color1) == -1) {
      return NULL;
    }
  }
  if (ColorObject_Check(v2)) {
    color2 = (ColorObject *)v2;
    if (BaseMath_ReadCallback(color2) == -1) {
      return NULL;
    }
  }

  /* make sure v1 is always the vector */
  if (color1 && color2) {
    /* col * col, don't support yet! */
  }
  else if (color1) {
    if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* COLOR * FLOAT */
      return color_mul_float(color1, scalar);
    }
  }
  else if (color2) {
    if (((scalar = PyFloat_AsDouble(v1)) == -1.0f && PyErr_Occurred()) == 0) { /* FLOAT * COLOR */
      return color_mul_float(color2, scalar);
    }
  }
  else {
    BLI_assert_msg(0, "internal error");
  }

  PyErr_Format(PyExc_TypeError,
               "Color multiplication: not supported between "
               "'%.200s' and '%.200s' types",
               Py_TYPE(v1)->tp_name,
               Py_TYPE(v2)->tp_name);
  return NULL;
}

/** Division: `object / object`. */
static PyObject *Color_div(PyObject *v1, PyObject *v2)
{
  ColorObject *color1 = NULL;
  float scalar;

  if (ColorObject_Check(v1)) {
    color1 = (ColorObject *)v1;
    if (BaseMath_ReadCallback(color1) == -1) {
      return NULL;
    }
  }
  else {
    PyErr_SetString(PyExc_TypeError, "Color division not supported in this order");
    return NULL;
  }

  /* make sure v1 is always the vector */
  if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* COLOR * FLOAT */
    if (scalar == 0.0f) {
      PyErr_SetString(PyExc_ZeroDivisionError, "Color division: divide by zero error");
      return NULL;
    }
    return color_mul_float(color1, 1.0f / scalar);
  }

  PyErr_Format(PyExc_TypeError,
               "Color multiplication: not supported between "
               "'%.200s' and '%.200s' types",
               Py_TYPE(v1)->tp_name,
               Py_TYPE(v2)->tp_name);
  return NULL;
}

/** Multiplication in-place: `object *= object`. */
static PyObject *Color_imul(PyObject *v1, PyObject *v2)
{
  ColorObject *color = (ColorObject *)v1;
  float scalar;

  if (BaseMath_ReadCallback_ForWrite(color) == -1) {
    return NULL;
  }

  /* only support color *= float */
  if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* COLOR *= FLOAT */
    mul_vn_fl(color->col, COLOR_SIZE, scalar);
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "Color multiplication: (%s *= %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return NULL;
  }

  (void)BaseMath_WriteCallback(color);
  Py_INCREF(v1);
  return v1;
}

/** Division in-place: `object *= object`. */
static PyObject *Color_idiv(PyObject *v1, PyObject *v2)
{
  ColorObject *color = (ColorObject *)v1;
  float scalar;

  if (BaseMath_ReadCallback_ForWrite(color) == -1) {
    return NULL;
  }

  /* only support color /= float */
  if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* COLOR /= FLOAT */
    if (scalar == 0.0f) {
      PyErr_SetString(PyExc_ZeroDivisionError, "Color division: divide by zero error");
      return NULL;
    }

    mul_vn_fl(color->col, COLOR_SIZE, 1.0f / scalar);
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "Color division: (%s /= %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return NULL;
  }

  (void)BaseMath_WriteCallback(color);
  Py_INCREF(v1);
  return v1;
}

/** Negative (returns the negative of this object): `-object`. */
static PyObject *Color_neg(ColorObject *self)
{
  float tcol[COLOR_SIZE];

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  negate_vn_vn(tcol, self->col, COLOR_SIZE);
  return Color_CreatePyObject(tcol, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Protocol Declarations
 * \{ */

static PySequenceMethods Color_SeqMethods = {
    (lenfunc)Color_len,              /*sq_length*/
    (binaryfunc)NULL,                /*sq_concat*/
    (ssizeargfunc)NULL,              /*sq_repeat*/
    (ssizeargfunc)Color_item,        /*sq_item*/
    NULL,                            /*sq_slice(DEPRECATED)*/
    (ssizeobjargproc)Color_ass_item, /*sq_ass_item*/
    NULL,                            /*sq_ass_slice(DEPRECATED)*/
    (objobjproc)NULL,                /*sq_contains*/
    (binaryfunc)NULL,                /*sq_inplace_concat*/
    (ssizeargfunc)NULL,              /*sq_inplace_repeat*/
};

static PyMappingMethods Color_AsMapping = {
    (lenfunc)Color_len,
    (binaryfunc)Color_subscript,
    (objobjargproc)Color_ass_subscript,
};

static PyNumberMethods Color_NumMethods = {
    (binaryfunc)Color_add, /*nb_add*/
    (binaryfunc)Color_sub, /*nb_subtract*/
    (binaryfunc)Color_mul, /*nb_multiply*/
    NULL,                  /*nb_remainder*/
    NULL,                  /*nb_divmod*/
    NULL,                  /*nb_power*/
    (unaryfunc)Color_neg,  /*nb_negative*/
    (unaryfunc)Color_copy, /*tp_positive*/
    (unaryfunc)NULL,       /*tp_absolute*/
    (inquiry)NULL,         /*tp_bool*/
    (unaryfunc)NULL,       /*nb_invert*/
    NULL,                  /*nb_lshift*/
    (binaryfunc)NULL,      /*nb_rshift*/
    NULL,                  /*nb_and*/
    NULL,                  /*nb_xor*/
    NULL,                  /*nb_or*/
    NULL,                  /*nb_int*/
    NULL,                  /*nb_reserved*/
    NULL,                  /*nb_float*/
    Color_iadd,            /*nb_inplace_add*/
    Color_isub,            /*nb_inplace_subtract*/
    Color_imul,            /*nb_inplace_multiply*/
    NULL,                  /*nb_inplace_remainder*/
    NULL,                  /*nb_inplace_power*/
    NULL,                  /*nb_inplace_lshift*/
    NULL,                  /*nb_inplace_rshift*/
    NULL,                  /*nb_inplace_and*/
    NULL,                  /*nb_inplace_xor*/
    NULL,                  /*nb_inplace_or*/
    NULL,                  /*nb_floor_divide*/
    Color_div,             /*nb_true_divide*/
    NULL,                  /*nb_inplace_floor_divide*/
    Color_idiv,            /*nb_inplace_true_divide*/
    NULL,                  /*nb_index*/
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Get/Set Item Implementation
 * \{ */

/* Color channel (RGB): `color.r/g/b`. */

PyDoc_STRVAR(Color_channel_r_doc, "Red color channel.\n\n:type: float");
PyDoc_STRVAR(Color_channel_g_doc, "Green color channel.\n\n:type: float");
PyDoc_STRVAR(Color_channel_b_doc, "Blue color channel.\n\n:type: float");

static PyObject *Color_channel_get(ColorObject *self, void *type)
{
  return Color_item(self, POINTER_AS_INT(type));
}

static int Color_channel_set(ColorObject *self, PyObject *value, void *type)
{
  return Color_ass_item(self, POINTER_AS_INT(type), value);
}

/* Color channel (HSV): `color.h/s/v`. */

PyDoc_STRVAR(Color_channel_hsv_h_doc, "HSV Hue component in [0, 1].\n\n:type: float");
PyDoc_STRVAR(Color_channel_hsv_s_doc, "HSV Saturation component in [0, 1].\n\n:type: float");
PyDoc_STRVAR(Color_channel_hsv_v_doc, "HSV Value component in [0, 1].\n\n:type: float");

static PyObject *Color_channel_hsv_get(ColorObject *self, void *type)
{
  float hsv[3];
  const int i = POINTER_AS_INT(type);

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  rgb_to_hsv(self->col[0], self->col[1], self->col[2], &(hsv[0]), &(hsv[1]), &(hsv[2]));

  return PyFloat_FromDouble(hsv[i]);
}

static int Color_channel_hsv_set(ColorObject *self, PyObject *value, void *type)
{
  float hsv[3];
  const int i = POINTER_AS_INT(type);
  float f = PyFloat_AsDouble(value);

  if (f == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError,
                    "color.h/s/v = value: "
                    "assigned value not a number");
    return -1;
  }

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  rgb_to_hsv_v(self->col, hsv);
  CLAMP(f, 0.0f, 1.0f);
  hsv[i] = f;
  hsv_to_rgb_v(hsv, self->col);

  if (BaseMath_WriteCallback(self) == -1) {
    return -1;
  }

  return 0;
}

PyDoc_STRVAR(Color_hsv_doc, "HSV Values in [0, 1].\n\n:type: float triplet");
/** Color channel HSV (get): `x = color.hsv`. */
static PyObject *Color_hsv_get(ColorObject *self, void *UNUSED(closure))
{
  float hsv[3];
  PyObject *ret;

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  rgb_to_hsv(self->col[0], self->col[1], self->col[2], &(hsv[0]), &(hsv[1]), &(hsv[2]));

  ret = PyTuple_New(3);
  PyTuple_SET_ITEMS(
      ret, PyFloat_FromDouble(hsv[0]), PyFloat_FromDouble(hsv[1]), PyFloat_FromDouble(hsv[2]));
  return ret;
}

/** Color channel HSV (set): `color.hsv = x`. */
static int Color_hsv_set(ColorObject *self, PyObject *value, void *UNUSED(closure))
{
  float hsv[3];

  if (mathutils_array_parse(hsv, 3, 3, value, "mathutils.Color.hsv = value") == -1) {
    return -1;
  }

  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return -1;
  }

  clamp_v3(hsv, 0.0f, 1.0f);
  hsv_to_rgb_v(hsv, self->col);

  if (BaseMath_WriteCallback(self) == -1) {
    return -1;
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Get/Set Item Definitions
 * \{ */

static PyGetSetDef Color_getseters[] = {
    {"r", (getter)Color_channel_get, (setter)Color_channel_set, Color_channel_r_doc, (void *)0},
    {"g", (getter)Color_channel_get, (setter)Color_channel_set, Color_channel_g_doc, (void *)1},
    {"b", (getter)Color_channel_get, (setter)Color_channel_set, Color_channel_b_doc, (void *)2},

    {"h",
     (getter)Color_channel_hsv_get,
     (setter)Color_channel_hsv_set,
     Color_channel_hsv_h_doc,
     (void *)0},
    {"s",
     (getter)Color_channel_hsv_get,
     (setter)Color_channel_hsv_set,
     Color_channel_hsv_s_doc,
     (void *)1},
    {"v",
     (getter)Color_channel_hsv_get,
     (setter)Color_channel_hsv_set,
     Color_channel_hsv_v_doc,
     (void *)2},

    {"hsv", (getter)Color_hsv_get, (setter)Color_hsv_set, Color_hsv_doc, (void *)0},

    {"is_wrapped",
     (getter)BaseMathObject_is_wrapped_get,
     (setter)NULL,
     BaseMathObject_is_wrapped_doc,
     NULL},
    {"is_frozen",
     (getter)BaseMathObject_is_frozen_get,
     (setter)NULL,
     BaseMathObject_is_frozen_doc,
     NULL},
    {"is_valid",
     (getter)BaseMathObject_is_valid_get,
     (setter)NULL,
     BaseMathObject_is_valid_doc,
     NULL},
    {"owner", (getter)BaseMathObject_owner_get, (setter)NULL, BaseMathObject_owner_doc, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Method Definitions
 * \{ */

static struct PyMethodDef Color_methods[] = {
    {"copy", (PyCFunction)Color_copy, METH_NOARGS, Color_copy_doc},
    {"__copy__", (PyCFunction)Color_copy, METH_NOARGS, Color_copy_doc},
    {"__deepcopy__", (PyCFunction)Color_deepcopy, METH_VARARGS, Color_copy_doc},

    /* base-math methods */
    {"freeze", (PyCFunction)BaseMathObject_freeze, METH_NOARGS, BaseMathObject_freeze_doc},

/* Color-space methods. */
#ifndef MATH_STANDALONE
    {"from_scene_linear_to_srgb",
     (PyCFunction)Color_from_scene_linear_to_srgb,
     METH_NOARGS,
     Color_from_scene_linear_to_srgb_doc},
    {"from_srgb_to_scene_linear",
     (PyCFunction)Color_from_srgb_to_scene_linear,
     METH_NOARGS,
     Color_from_srgb_to_scene_linear_doc},
    {"from_scene_linear_to_xyz_d65",
     (PyCFunction)Color_from_scene_linear_to_xyz_d65,
     METH_NOARGS,
     Color_from_scene_linear_to_xyz_d65_doc},
    {"from_xyz_d65_to_scene_linear",
     (PyCFunction)Color_from_xyz_d65_to_scene_linear,
     METH_NOARGS,
     Color_from_xyz_d65_to_scene_linear_doc},
    {"from_scene_linear_to_aces",
     (PyCFunction)Color_from_scene_linear_to_aces,
     METH_NOARGS,
     Color_from_scene_linear_to_aces_doc},
    {"from_aces_to_scene_linear",
     (PyCFunction)Color_from_aces_to_scene_linear,
     METH_NOARGS,
     Color_from_aces_to_scene_linear_doc},
    {"from_scene_linear_to_rec709_linear",
     (PyCFunction)Color_from_scene_linear_to_rec709_linear,
     METH_NOARGS,
     Color_from_scene_linear_to_rec709_linear_doc},
    {"from_rec709_linear_to_scene_linear",
     (PyCFunction)Color_from_rec709_linear_to_scene_linear,
     METH_NOARGS,
     Color_from_rec709_linear_to_scene_linear_doc},
#endif /* MATH_STANDALONE */

    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: Python Object Definition
 * \{ */

PyDoc_STRVAR(
    color_doc,
    ".. class:: Color(rgb)\n"
    "\n"
    "   This object gives access to Colors in Blender.\n"
    "\n"
    "   Most colors returned by Blender APIs are in scene linear color space, as defined by "
    "   the OpenColorIO configuration. The notable exception is user interface theming colors, "
    "   which are in sRGB color space.\n"
    "\n"
    "   :arg rgb: (r, g, b) color values\n"
    "   :type rgb: 3d vector\n");
PyTypeObject color_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "Color", /* tp_name */
    sizeof(ColorObject),                    /* tp_basicsize */
    0,                                      /* tp_itemsize */
    (destructor)BaseMathObject_dealloc,     /* tp_dealloc */
    (printfunc)NULL,                        /* tp_print */
    NULL,                                   /* tp_getattr */
    NULL,                                   /* tp_setattr */
    NULL,                                   /* tp_compare */
    (reprfunc)Color_repr,                   /* tp_repr */
    &Color_NumMethods,                      /* tp_as_number */
    &Color_SeqMethods,                      /* tp_as_sequence */
    &Color_AsMapping,                       /* tp_as_mapping */
    (hashfunc)Color_hash,                   /* tp_hash */
    NULL,                                   /* tp_call */
#ifndef MATH_STANDALONE
    (reprfunc)Color_str, /* tp_str */
#else
    NULL, /* tp_str */
#endif
    NULL,                                                          /* tp_getattro */
    NULL,                                                          /* tp_setattro */
    NULL,                                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /* tp_flags */
    color_doc,                                                     /* tp_doc */
    (traverseproc)BaseMathObject_traverse,                         /* tp_traverse */
    (inquiry)BaseMathObject_clear,                                 /* tp_clear */
    (richcmpfunc)Color_richcmpr,                                   /* tp_richcompare */
    0,                                                             /* tp_weaklistoffset */
    NULL,                                                          /* tp_iter */
    NULL,                                                          /* tp_iternext */
    Color_methods,                                                 /* tp_methods */
    NULL,                                                          /* tp_members */
    Color_getseters,                                               /* tp_getset */
    NULL,                                                          /* tp_base */
    NULL,                                                          /* tp_dict */
    NULL,                                                          /* tp_descr_get */
    NULL,                                                          /* tp_descr_set */
    0,                                                             /* tp_dictoffset */
    NULL,                                                          /* tp_init */
    NULL,                                                          /* tp_alloc */
    Color_new,                                                     /* tp_new */
    NULL,                                                          /* tp_free */
    NULL,                                                          /* tp_is_gc */
    NULL,                                                          /* tp_bases */
    NULL,                                                          /* tp_mro */
    NULL,                                                          /* tp_cache */
    NULL,                                                          /* tp_subclasses */
    NULL,                                                          /* tp_weaklist */
    NULL,                                                          /* tp_del */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Type: C/API Constructors
 * \{ */

PyObject *Color_CreatePyObject(const float col[3], PyTypeObject *base_type)
{
  ColorObject *self;
  float *col_alloc;

  col_alloc = PyMem_Malloc(COLOR_SIZE * sizeof(float));
  if (UNLIKELY(col_alloc == NULL)) {
    PyErr_SetString(PyExc_MemoryError,
                    "Color(): "
                    "problem allocating data");
    return NULL;
  }

  self = BASE_MATH_NEW(ColorObject, color_Type, base_type);
  if (self) {
    self->col = col_alloc;

    /* init callbacks as NULL */
    self->cb_user = NULL;
    self->cb_type = self->cb_subtype = 0;

    /* NEW */
    if (col) {
      copy_v3_v3(self->col, col);
    }
    else {
      zero_v3(self->col);
    }

    self->flag = BASE_MATH_FLAG_DEFAULT;
  }
  else {
    PyMem_Free(col_alloc);
  }

  return (PyObject *)self;
}

PyObject *Color_CreatePyObject_wrap(float col[3], PyTypeObject *base_type)
{
  ColorObject *self;

  self = BASE_MATH_NEW(ColorObject, color_Type, base_type);
  if (self) {
    /* init callbacks as NULL */
    self->cb_user = NULL;
    self->cb_type = self->cb_subtype = 0;

    /* WRAP */
    self->col = col;
    self->flag = BASE_MATH_FLAG_DEFAULT | BASE_MATH_FLAG_IS_WRAP;
  }

  return (PyObject *)self;
}

PyObject *Color_CreatePyObject_cb(PyObject *cb_user, uchar cb_type, uchar cb_subtype)
{
  ColorObject *self = (ColorObject *)Color_CreatePyObject(NULL, NULL);
  if (self) {
    Py_INCREF(cb_user);
    self->cb_user = cb_user;
    self->cb_type = cb_type;
    self->cb_subtype = cb_subtype;
    PyObject_GC_Track(self);
  }

  return (PyObject *)self;
}

/** \} */
