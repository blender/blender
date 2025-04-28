/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 *
 * Python/RNA utilities.
 *
 * RNA functions that aren't part of the `bpy_rna.cc` API.
 */

/* Future-proof, See https://docs.python.org/3/c-api/arg.html#strings-and-buffers */
#define PY_SSIZE_T_CLEAN

#include <Python.h>

#include "py_capi_rna.hh"

#include "BLI_bitmap.h"
#include "BLI_dynstr.h"

#include "RNA_access.hh"

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name Enum Utilities
 * \{ */

char *pyrna_enum_repr(const EnumPropertyItem *item)
{
  DynStr *dynstr = BLI_dynstr_new();

  /* We can't compare with the first element in the array
   * since it may be a category (without an identifier). */
  for (bool is_first = true; item->identifier; item++) {
    if (item->identifier[0]) {
      BLI_dynstr_appendf(dynstr, is_first ? "'%s'" : ", '%s'", item->identifier);
      is_first = false;
    }
  }

  char *cstring = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);
  return cstring;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enum Conversion Utilities
 * \{ */

int pyrna_enum_value_from_id(const EnumPropertyItem *item,
                             const char *identifier,
                             int *r_value,
                             const char *error_prefix)
{
  if (RNA_enum_value_from_id(item, identifier, r_value) == 0) {
    const char *enum_str = pyrna_enum_repr(item);
    PyErr_Format(
        PyExc_ValueError, "%s: '%.200s' not found in (%s)", error_prefix, identifier, enum_str);
    MEM_freeN(enum_str);
    return -1;
  }

  return 0;
}

BLI_bitmap *pyrna_enum_bitmap_from_set(const EnumPropertyItem *items,
                                       PyObject *value,
                                       int type_size,
                                       bool type_convert_sign,
                                       int bitmap_size,
                                       const char *error_prefix)
{
  BLI_assert(PySet_Check(value));
  BLI_bitmap *bitmap = BLI_BITMAP_NEW(bitmap_size, __func__);

  if (PySet_GET_SIZE(value) > 0) {
    /* Set looping. */
    PyObject *it = PyObject_GetIter(value);
    PyObject *key;
    while ((key = PyIter_Next(it))) {
      /* Borrow from the set. */
      Py_DECREF(key);

      const char *param = PyUnicode_AsUTF8(key);
      if (param == nullptr) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s expected a string, not %.200s",
                     error_prefix,
                     Py_TYPE(key)->tp_name);
        break;
      }

      int ret;
      if (pyrna_enum_value_from_id(items, param, &ret, error_prefix) == -1) {
        break;
      }

      int index = ret;

      if (type_convert_sign) {
        if (type_size == 2) {
          union {
            signed short as_signed;
            ushort as_unsigned;
          } ret_convert;
          ret_convert.as_signed = (signed short)ret;
          index = int(ret_convert.as_unsigned);
        }
        else if (type_size == 1) {
          union {
            signed char as_signed;
            uchar as_unsigned;
          } ret_convert;
          ret_convert.as_signed = (signed char)ret;
          index = int(ret_convert.as_unsigned);
        }
        else {
          BLI_assert_unreachable();
        }
      }
      BLI_assert(index < bitmap_size);
      BLI_BITMAP_ENABLE(bitmap, index);
    }
    Py_DECREF(it);

    if (key) {
      MEM_freeN(bitmap);
      bitmap = nullptr;
    }
  }

  return bitmap;
}

int pyrna_enum_bitfield_from_set(const EnumPropertyItem *items,
                                 PyObject *value,
                                 int *r_value,
                                 const char *error_prefix)
{
  BLI_assert(PySet_Check(value));
  /* Set of enum items, concatenate all values with OR. */
  int flag = 0;

  *r_value = 0;

  PyObject *key = nullptr;
  if (PySet_GET_SIZE(value) > 0) {
    /* Set looping. */
    PyObject *it = PyObject_GetIter(value);
    while ((key = PyIter_Next(it))) {
      /* Borrow from the set. */
      Py_DECREF(key);

      const char *param = PyUnicode_AsUTF8(key);
      if (param == nullptr) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s expected a string, not %.200s",
                     error_prefix,
                     Py_TYPE(key)->tp_name);
        break;
      }

      int ret;
      if (pyrna_enum_value_from_id(items, param, &ret, error_prefix) == -1) {
        break;
      }

      flag |= ret;
    }
    Py_DECREF(it);
    if (key) {
      return -1;
    }
  }

  *r_value = flag;
  return 0;
}

PyObject *pyrna_enum_bitfield_as_set(const EnumPropertyItem *items, int value)
{
  PyObject *ret = PySet_New(nullptr);
  const char *identifier[RNA_ENUM_BITFLAG_SIZE + 1];

  if (RNA_enum_bitflag_identifiers(items, value, identifier)) {
    PyObject *item;
    int index;
    for (index = 0; identifier[index]; index++) {
      item = PyUnicode_FromString(identifier[index]);
      PySet_Add(ret, item);
      Py_DECREF(item);
    }
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Argument Parsing Helpers
 * \{ */

int pyrna_enum_value_parse_string(PyObject *o, void *p)
{
  const char *identifier = PyUnicode_AsUTF8(o);
  if (identifier == nullptr) {
    PyErr_Format(PyExc_TypeError, "expected a string enum, not %.200s", Py_TYPE(o)->tp_name);
    return 0;
  }
  BPy_EnumProperty_Parse *parse_data = static_cast<BPy_EnumProperty_Parse *>(p);
  if (pyrna_enum_value_from_id(
          parse_data->items, identifier, &parse_data->value, "enum identifier") == -1)
  {
    return 0;
  }

  parse_data->value_orig = o;
  parse_data->is_set = true;
  return 1;
}

int pyrna_enum_bitfield_parse_set(PyObject *o, void *p)
{
  if (!PySet_Check(o)) {
    PyErr_Format(PyExc_TypeError, "expected a set, not %.200s", Py_TYPE(o)->tp_name);
    return 0;
  }

  BPy_EnumProperty_Parse *parse_data = static_cast<BPy_EnumProperty_Parse *>(p);
  if (pyrna_enum_bitfield_from_set(
          parse_data->items, o, &parse_data->value, "enum identifier set") == -1)
  {
    return 0;
  }
  parse_data->value_orig = o;
  parse_data->is_set = true;
  return 1;
}

/** \} */
