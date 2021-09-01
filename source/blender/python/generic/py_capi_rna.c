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
 * \ingroup pygen
 *
 * Python/RNA utilities.
 *
 * RNA functions that aren't part of the `bpy_rna.c` API.
 */

/* Future-proof, See https://docs.python.org/3/c-api/arg.html#strings-and-buffers */
#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <stdbool.h>

#include "py_capi_rna.h"

#include "BLI_bitmap.h"
#include "BLI_dynstr.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

char *BPy_enum_as_string(const EnumPropertyItem *item)
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

/**
 * Same as #RNA_enum_value_from_id, but raises an exception.
 */
int pyrna_enum_value_from_id(const EnumPropertyItem *item,
                             const char *identifier,
                             int *r_value,
                             const char *error_prefix)
{
  if (RNA_enum_value_from_id(item, identifier, r_value) == 0) {
    const char *enum_str = BPy_enum_as_string(item);
    PyErr_Format(
        PyExc_ValueError, "%s: '%.200s' not found in (%s)", error_prefix, identifier, enum_str);
    MEM_freeN((void *)enum_str);
    return -1;
  }

  return 0;
}

/**
 * Use with #PyArg_ParseTuple's `O&` formatting.
 */
int pyrna_enum_value_parse_string(PyObject *o, void *p)
{
  const char *identifier = PyUnicode_AsUTF8(o);
  if (identifier == NULL) {
    PyErr_Format(PyExc_TypeError, "expected a string enum, not %.200s", Py_TYPE(o)->tp_name);
    return 0;
  }
  struct BPy_EnumProperty_Parse *parse_data = p;
  if (pyrna_enum_value_from_id(
          parse_data->items, identifier, &parse_data->value, "enum identifier") == -1) {
    return 0;
  }

  parse_data->value_orig = o;
  parse_data->is_set = true;
  return 1;
}

/**
 * Use with #PyArg_ParseTuple's `O&` formatting.
 */
int pyrna_enum_bitfield_parse_set(PyObject *o, void *p)
{
  if (!PySet_Check(o)) {
    PyErr_Format(PyExc_TypeError, "expected a set, not %.200s", Py_TYPE(o)->tp_name);
    return 0;
  }

  struct BPy_EnumProperty_Parse *parse_data = p;
  if (pyrna_set_to_enum_bitfield(
          parse_data->items, o, &parse_data->value, "enum identifier set") == -1) {
    return 0;
  }
  parse_data->value_orig = o;
  parse_data->is_set = true;
  return 1;
}

/**
 * Takes a set of strings and map it to and array of booleans.
 *
 * Useful when the values aren't flags.
 *
 * \param type_convert_sign: Maps signed to unsigned range,
 * needed when we want to use the full range of a signed short/char.
 */
BLI_bitmap *pyrna_set_to_enum_bitmap(const EnumPropertyItem *items,
                                     PyObject *value,
                                     int type_size,
                                     bool type_convert_sign,
                                     int bitmap_size,
                                     const char *error_prefix)
{
  /* Set looping. */
  Py_ssize_t pos = 0;
  Py_ssize_t hash = 0;
  PyObject *key;

  BLI_bitmap *bitmap = BLI_BITMAP_NEW(bitmap_size, __func__);

  while (_PySet_NextEntry(value, &pos, &key, &hash)) {
    const char *param = PyUnicode_AsUTF8(key);
    if (param == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s expected a string, not %.200s",
                   error_prefix,
                   Py_TYPE(key)->tp_name);
      goto error;
    }

    int ret;
    if (pyrna_enum_value_from_id(items, param, &ret, error_prefix) == -1) {
      goto error;
    }

    int index = ret;

    if (type_convert_sign) {
      if (type_size == 2) {
        union {
          signed short as_signed;
          ushort as_unsigned;
        } ret_convert;
        ret_convert.as_signed = (signed short)ret;
        index = (int)ret_convert.as_unsigned;
      }
      else if (type_size == 1) {
        union {
          signed char as_signed;
          uchar as_unsigned;
        } ret_convert;
        ret_convert.as_signed = (signed char)ret;
        index = (int)ret_convert.as_unsigned;
      }
      else {
        BLI_assert_unreachable();
      }
    }
    BLI_assert(index < bitmap_size);
    BLI_BITMAP_ENABLE(bitmap, index);
  }

  return bitmap;

error:
  MEM_freeN(bitmap);
  return NULL;
}

/**
 * 'value' _must_ be a set type, error check before calling.
 */
int pyrna_set_to_enum_bitfield(const EnumPropertyItem *items,
                               PyObject *value,
                               int *r_value,
                               const char *error_prefix)
{
  /* Set of enum items, concatenate all values with OR. */
  int ret, flag = 0;

  /* Set looping. */
  Py_ssize_t pos = 0;
  Py_ssize_t hash = 0;
  PyObject *key;

  *r_value = 0;

  while (_PySet_NextEntry(value, &pos, &key, &hash)) {
    const char *param = PyUnicode_AsUTF8(key);

    if (param == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s expected a string, not %.200s",
                   error_prefix,
                   Py_TYPE(key)->tp_name);
      return -1;
    }

    if (pyrna_enum_value_from_id(items, param, &ret, error_prefix) == -1) {
      return -1;
    }

    flag |= ret;
  }

  *r_value = flag;
  return 0;
}

PyObject *pyrna_enum_bitfield_to_py(const EnumPropertyItem *items, int value)
{
  PyObject *ret = PySet_New(NULL);
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
