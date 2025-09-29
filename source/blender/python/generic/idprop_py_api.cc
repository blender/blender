/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

#include <algorithm>

#include <Python.h>

#include "python_compat.hh" /* IWYU pragma: keep. */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "idprop_py_api.hh"
#include "idprop_py_ui_api.hh"

#include "BKE_idprop.hh"

#include "DNA_ID.h" /* ID property definitions. */

#define USE_STRING_COERCE

#ifdef USE_STRING_COERCE
#  include "py_capi_utils.hh"
#endif

#include "python_utildefines.hh"

extern bool pyrna_id_FromPyObject(PyObject *obj, ID **id);
extern PyObject *pyrna_id_CreatePyObject(ID *id);
extern bool pyrna_id_CheckPyObject(PyObject *obj);

/* Currently there is no need to expose this publicly. */
static PyObject *BPy_IDGroup_IterKeys_CreatePyObject(BPy_IDProperty *group, const bool reversed);
static PyObject *BPy_IDGroup_IterValues_CreatePyObject(BPy_IDProperty *group, const bool reversed);
static PyObject *BPy_IDGroup_IterItems_CreatePyObject(BPy_IDProperty *group, const bool reversed);

static PyObject *BPy_IDGroup_ViewKeys_CreatePyObject(BPy_IDProperty *group);
static PyObject *BPy_IDGroup_ViewValues_CreatePyObject(BPy_IDProperty *group);
static PyObject *BPy_IDGroup_ViewItems_CreatePyObject(BPy_IDProperty *group);

static BPy_IDGroup_View *IDGroup_View_New_WithType(BPy_IDProperty *group, PyTypeObject *type);
static int BPy_IDGroup_Contains(BPy_IDProperty *self, PyObject *value);

/* -------------------------------------------------------------------- */
/** \name Python from ID-Property (Internal Conversions)
 *
 * Low level conversion to avoid duplicate code, no type checking.
 * \{ */

static PyObject *idprop_py_from_idp_string(const IDProperty *prop)
{
  if (prop->subtype == IDP_STRING_SUB_BYTE) {
    return PyBytes_FromStringAndSize(IDP_string_get(prop), prop->len);
  }

#ifdef USE_STRING_COERCE
  return PyC_UnicodeFromBytesAndSize(IDP_string_get(prop), prop->len - 1);
#else
  return PyUnicode_FromStringAndSize(IDP_string_get(prop), prop->len - 1);
#endif
}

static PyObject *idprop_py_from_idp_int(const IDProperty *prop)
{
  return PyLong_FromLong(long(IDP_int_get(prop)));
}

static PyObject *idprop_py_from_idp_float(const IDProperty *prop)
{
  return PyFloat_FromDouble(double(IDP_float_get(prop)));
}

static PyObject *idprop_py_from_idp_double(const IDProperty *prop)
{
  return PyFloat_FromDouble(IDP_double_get(prop));
}

static PyObject *idprop_py_from_idp_bool(const IDProperty *prop)
{
  return PyBool_FromLong(IDP_bool_get(prop));
}

static PyObject *idprop_py_from_idp_group(ID *id, IDProperty *prop, IDProperty *parent)
{
  BPy_IDProperty *group = PyObject_New(BPy_IDProperty, &BPy_IDGroup_Type);
  group->owner_id = id;
  group->prop = prop;
  group->parent = parent; /* can be nullptr */
  return (PyObject *)group;
}

static PyObject *idprop_py_from_idp_id(IDProperty *prop)
{
  return pyrna_id_CreatePyObject(static_cast<ID *>(prop->data.pointer));
}

static PyObject *idprop_py_from_idp_array(ID *id, IDProperty *prop)
{
  BPy_IDProperty *array = PyObject_New(BPy_IDProperty, &BPy_IDArray_Type);
  array->owner_id = id;
  array->prop = prop;
  return (PyObject *)array;
}

static PyObject *idprop_py_from_idp_idparray(ID *id, IDProperty *prop)
{
  PyObject *seq = PyList_New(prop->len);
  IDProperty *array = IDP_property_array_get(prop);
  int i;

  if (!seq) {
    PyErr_Format(
        PyExc_RuntimeError, "%s: IDP_IDPARRAY: PyList_New(%d) failed", __func__, prop->len);
    return nullptr;
  }

  for (i = 0; i < prop->len; i++) {
    PyObject *wrap = BPy_IDGroup_WrapData(id, array++, prop);

    /* BPy_IDGroup_MapDataToPy sets the error */
    if (UNLIKELY(wrap == nullptr)) {
      Py_DECREF(seq);
      return nullptr;
    }

    PyList_SET_ITEM(seq, i, wrap);
  }

  return seq;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name IDProp Group Access
 * \{ */

/* use for both array and group */
static Py_hash_t BPy_IDGroup_hash(BPy_IDProperty *self)
{
  return Py_HashPointer(self->prop);
}

static PyObject *BPy_IDGroup_repr(BPy_IDProperty *self)
{
  return PyUnicode_FromFormat("<bpy id prop: owner=\"%s\", name=\"%s\", address=%p>",
                              self->owner_id ? self->owner_id->name : "<NONE>",
                              self->prop->name,
                              self->prop);
}

PyObject *BPy_IDGroup_WrapData(ID *id, IDProperty *prop, IDProperty *parent)
{
  switch (prop->type) {
    case IDP_STRING:
      return idprop_py_from_idp_string(prop);
    case IDP_INT:
      return idprop_py_from_idp_int(prop);
    case IDP_FLOAT:
      return idprop_py_from_idp_float(prop);
    case IDP_DOUBLE:
      return idprop_py_from_idp_double(prop);
    case IDP_BOOLEAN:
      return idprop_py_from_idp_bool(prop);
    case IDP_GROUP:
      return idprop_py_from_idp_group(id, prop, parent);
    case IDP_ARRAY:
      return idprop_py_from_idp_array(id, prop);
    case IDP_IDPARRAY:
      return idprop_py_from_idp_idparray(id, prop); /* this could be better a internal type */
    case IDP_ID:
      return idprop_py_from_idp_id(prop);
    default:
      Py_RETURN_NONE;
  }
}

/* UNUSED, currently assignment overwrites into new properties, rather than setting in-place. */
#if 0
static int BPy_IDGroup_SetData(BPy_IDProperty *self, IDProperty *prop, PyObject *value)
{
  switch (prop->type) {
    case IDP_STRING: {
      char *st;
      if (!PyUnicode_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "expected a string!");
        return -1;
      }
/* NOTE: if this code is enabled, bytes support needs to be added */
#  ifdef USE_STRING_COERCE
      {
        int alloc_len;
        PyObject *value_coerce = nullptr;

        st = (char *)PyC_UnicodeAsBytes(value, &value_coerce);
        alloc_len = strlen(st) + 1;

        st = PyUnicode_AsUTF8(value);
        IDP_ResizeArray(prop, alloc_len);
        memcpy(IDP_string_get(prop), st, alloc_len);
        Py_XDECREF(value_coerce);
      }
#  else
      length_ssize_t st_len;
      st = PyUnicode_AsUTF8AndSize(value, &st_len);
      IDP_ResizeArray(prop, st_len + 1);
      memcpy(IDP_string_get(prop), st, st_len + 1);
#  endif

      return 0;
    }

    case IDP_INT: {
      int ivalue = PyLong_AsSsize_t(value);
      if (ivalue == -1 && PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "expected an int type");
        return -1;
      }
      IDP_int_set(prop, ivalue);
      break;
    }
    case IDP_FLOAT: {
      float fvalue = float(PyFloat_AsDouble(value));
      if (fvalue == -1 && PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "expected a float");
        return -1;
      }
      IDP_float_set(self->prop, fvalue);
      break;
    }
    case IDP_DOUBLE: {
      double dvalue = PyFloat_AsDouble(value);
      if (dvalue == -1 && PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "expected a float");
        return -1;
      }
      IDP_double_set(self->prop, value);
      break;
    }
    default:
      PyErr_SetString(PyExc_AttributeError, "attempt to set read-only attribute!");
      return -1;
  }
  return 0;
}
#endif

static PyObject *BPy_IDGroup_GetName(BPy_IDProperty *self, void * /*closure*/)
{
  return PyUnicode_FromString(self->prop->name);
}

static int BPy_IDGroup_SetName(BPy_IDProperty *self, PyObject *value, void * /*closure*/)
{
  const char *name;
  Py_ssize_t name_len;

  if (!PyUnicode_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "expected a string!");
    return -1;
  }

  name = PyUnicode_AsUTF8AndSize(value, &name_len);

  if (!(name_len < MAX_IDPROP_NAME)) {
    PyErr_SetString(PyExc_TypeError, "string length cannot exceed 63 characters!");
    return -1;
  }
  if (STREQ(name, self->prop->name)) {
    return 0;
  }
  if (IDProperty *parent = self->parent) {
    if (IDP_GetPropertyFromGroup(parent, name)) {
      PyErr_SetString(PyExc_NameError, "property name already exists in parent group");
      return -1;
    }
  }

  memcpy(self->prop->name, name, name_len + 1);
  return 0;
}

#if 0
static PyObject *BPy_IDGroup_GetType(BPy_IDProperty *self)
{
  return PyLong_FromLong(self->prop->type);
}
#endif

static PyGetSetDef BPy_IDGroup_getseters[] = {
    {"name",
     (getter)BPy_IDGroup_GetName,
     (setter)BPy_IDGroup_SetName,
     "The name of this Group.",
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr},
};

static Py_ssize_t BPy_IDGroup_Map_Len(BPy_IDProperty *self)
{
  if (self->prop->type != IDP_GROUP) {
    PyErr_SetString(PyExc_TypeError, "len() of unsized object");
    return -1;
  }

  return self->prop->len;
}

static PyObject *BPy_IDGroup_Map_GetItem(BPy_IDProperty *self, PyObject *item)
{
  IDProperty *idprop;
  const char *name;

  if (self->prop->type != IDP_GROUP) {
    PyErr_SetString(PyExc_TypeError, "unsubscriptable object");
    return nullptr;
  }

  name = PyUnicode_AsUTF8(item);

  if (name == nullptr) {
    PyErr_SetString(PyExc_TypeError, "only strings are allowed as keys of ID properties");
    return nullptr;
  }

  idprop = IDP_GetPropertyFromGroup(self->prop, name);

  if (idprop == nullptr) {
    PyErr_SetString(PyExc_KeyError, "key not in subgroup dict");
    return nullptr;
  }

  return BPy_IDGroup_WrapData(self->owner_id, idprop, self->prop);
}

/* Return identified matching IDProperty type, or -1 if error (e.g. mixed and/or incompatible
 * types, etc.). */
static char idp_sequence_type(PyObject *seq_fast)
{
  PyObject **seq_fast_items = PySequence_Fast_ITEMS(seq_fast);
  PyObject *item;
  char type = IDP_INT;

  Py_ssize_t i, len = PySequence_Fast_GET_SIZE(seq_fast);

  for (i = 0; i < len; i++) {
    item = seq_fast_items[i];
    if (PyFloat_Check(item)) {
      /* Mixed float and any other type but int.
       * NOTE: Mixed float/int is allowed, and considered as float values. */
      if (!ELEM(type, IDP_INT, IDP_DOUBLE)) {
        return -1;
      }
      type = IDP_DOUBLE;
    }
    else if (PyBool_Check(item)) {
      /* Mixed boolean and any other type. */
      if (i != 0 && (type != IDP_BOOLEAN)) {
        return -1;
      }
      type = IDP_BOOLEAN;
    }
    else if (PyLong_Check(item)) {
      /* Mixed int and any other type but float.
       * NOTE: Mixed float/int is allowed, and considered as float values. */
      if (!ELEM(type, IDP_INT, IDP_DOUBLE)) {
        return -1;
      }
    }
    else if (PyMapping_Check(item)) {
      /* Mixed dict and any other type. */
      if (i != 0 && (type != IDP_IDPARRAY)) {
        return -1;
      }
      type = IDP_IDPARRAY;
    }
    else {
      return -1;
    }
  }

  return type;
}

static const char *idp_try_read_name(PyObject *name_obj)
{
  const char *name = nullptr;
  if (name_obj) {
    Py_ssize_t name_len;
    name = PyUnicode_AsUTF8AndSize(name_obj, &name_len);

    if (name == nullptr) {
      PyErr_Format(PyExc_KeyError,
                   "invalid id-property key, expected a string, not a %.200s",
                   Py_TYPE(name_obj)->tp_name);
      return nullptr;
    }

    if (!(name_len < MAX_IDPROP_NAME)) {
      PyErr_SetString(PyExc_KeyError,
                      "the length of IDProperty names is limited to 63 characters");
      return nullptr;
    }
  }
  else {
    name = "";
  }
  return name;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID-Property from Python (Internal Conversions)
 * \{ */

/**
 * The `idp_from_Py*` functions expect that the input type has been checked before
 * and return nullptr if the IDProperty can't be created.
 *
 * \param prop_exist: If not null, attempt to assign given `ob` value to this property first, and
 * only create a new one if not possible.
 * If no assignment (or conversion and assignment) is possible, the current
 * value remains unchanged.
 *
 * \param do_conversion: If `true`, allow some 'reasonable' conversion of input value to match the
 * `prop_exist` property type. E.g. can convert an `int` to a `float`, but not
 * the other way around.
 *
 * \param can_create: Whether creating a new IDProperty is allowed.
 *
 * \return `prop_exist` if given and it could be assigned given `ob` value, a new IDProperty
 *         otherwise.
 */

static IDProperty *idp_from_PyFloat(IDProperty *prop_exist,
                                    const char *name,
                                    PyObject *ob,
                                    const bool do_conversion,
                                    const bool can_create)
{
  IDProperty *prop = nullptr;
  const double value = PyFloat_AsDouble(ob);
  if (prop_exist) {
    if (prop_exist->type == IDP_DOUBLE) {
      IDP_double_set(prop_exist, value);
      prop = prop_exist;
    }
    else if (do_conversion) {
      switch (prop_exist->type) {
        case IDP_FLOAT:
          IDP_float_set(prop_exist, float(value));
          prop = prop_exist;
          break;
        case IDP_STRING:
        case IDP_INT:
        case IDP_ARRAY:
        case IDP_GROUP:
        case IDP_ID:
        case IDP_DOUBLE:
        case IDP_IDPARRAY:
        case IDP_BOOLEAN:
          break;
      }
    }
  }
  if (!prop && can_create) {
    prop = blender::bke::idprop::create(name, value).release();
  }
  return prop;
}

static IDProperty *idp_from_PyBool(IDProperty *prop_exist,
                                   const char *name,
                                   PyObject *ob,
                                   const bool do_conversion,
                                   const bool can_create)
{
  IDProperty *prop = nullptr;
  const bool value = PyC_Long_AsBool(ob);
  if (prop_exist) {
    if (prop_exist->type == IDP_BOOLEAN) {
      IDP_bool_set(prop_exist, value);
      prop = prop_exist;
    }
    else if (do_conversion) {
      switch (prop_exist->type) {
        case IDP_INT:
          IDP_int_set(prop_exist, int(value));
          prop = prop_exist;
          break;
        case IDP_STRING:
        case IDP_FLOAT:
        case IDP_ARRAY:
        case IDP_GROUP:
        case IDP_ID:
        case IDP_DOUBLE:
        case IDP_IDPARRAY:
        case IDP_BOOLEAN:
          break;
      }
    }
  }
  if (!prop && can_create) {
    prop = blender::bke::idprop::create_bool(name, value).release();
  }
  return prop;
}

static IDProperty *idp_from_PyLong(IDProperty *prop_exist,
                                   const char *name,
                                   PyObject *ob,
                                   const bool do_conversion,
                                   const bool can_create)
{
  IDProperty *prop = nullptr;
  if (prop_exist) {
    if (prop_exist->type == IDP_INT) {
      const int value = PyC_Long_AsI32(ob);
      if (value == -1 && PyErr_Occurred()) {
        return prop;
      }
      IDP_int_set(prop_exist, value);
      prop = prop_exist;
    }
    else if (do_conversion) {
      const int64_t value = PyC_Long_AsI64(ob);
      if (value == -1 && PyErr_Occurred()) {
        return prop;
      }
      switch (prop_exist->type) {
        case IDP_FLOAT:
          IDP_float_set(prop_exist, float(value));
          prop = prop_exist;
          break;
        case IDP_DOUBLE:
          IDP_double_set(prop_exist, double(value));
          prop = prop_exist;
          break;
        case IDP_STRING:
        case IDP_INT:
        case IDP_ARRAY:
        case IDP_GROUP:
        case IDP_ID:
        case IDP_IDPARRAY:
        case IDP_BOOLEAN:
          break;
      }
    }
  }
  if (!prop && can_create) {
    const int value = PyC_Long_AsI32(ob);
    if (value == -1 && PyErr_Occurred()) {
      return prop;
    }
    prop = blender::bke::idprop::create(name, value).release();
  }
  return prop;
}

static IDProperty *idp_from_PyUnicode(IDProperty *prop_exist,
                                      const char *name,
                                      PyObject *ob,
                                      const bool /*do_conversion*/,
                                      const bool can_create)
{
  IDProperty *prop = nullptr;
  Py_ssize_t value_len = 0;
  const char *value = nullptr;

#ifdef USE_STRING_COERCE
  PyObject *value_coerce = nullptr;
  value = PyC_UnicodeAsBytesAndSize(ob, &value_len, &value_coerce);
#else
  value = PyUnicode_AsUTF8AndSize(ob, &value_len);
#endif

  if (prop_exist) {
    if (prop_exist->type == IDP_STRING && prop_exist->subtype == IDP_STRING_SUB_UTF8) {
      IDP_AssignStringMaxSize(prop_exist, value, int(value_len) + 1);
      prop = prop_exist;
    }
    /* No conversion. */
  }

  if (!prop && can_create) {
    IDPropertyTemplate val = {0};
    val.string.str = value;
    val.string.len = int(value_len) + 1;
    val.string.subtype = IDP_STRING_SUB_UTF8;
    prop = IDP_New(IDP_STRING, &val, name);
  }

#ifdef USE_STRING_COERCE
  Py_XDECREF(value_coerce);
#endif

  return prop;
}

static IDProperty *idp_from_PyBytes(IDProperty *prop_exist,
                                    const char *name,
                                    PyObject *ob,
                                    const bool /*do_conversion*/,
                                    const bool can_create)
{
  IDProperty *prop = nullptr;
  Py_ssize_t value_len = PyBytes_GET_SIZE(ob);
  const char *value = PyBytes_AS_STRING(ob);

  if (prop_exist) {
    if (prop_exist->type == IDP_STRING && prop_exist->subtype == IDP_STRING_SUB_BYTE) {
      IDP_AssignStringMaxSize(prop_exist, value, int(value_len) + 1);
      prop = prop_exist;
    }
    /* No conversion. */
  }

  if (!prop && can_create) {
    IDPropertyTemplate val = {0};
    val.string.str = value;
    val.string.len = int(value_len);
    val.string.subtype = IDP_STRING_SUB_BYTE;
    prop = IDP_New(IDP_STRING, &val, name);
  }

  return prop;
}

static int idp_array_type_from_formatstr_and_size(const char *typestr, Py_ssize_t itemsize)
{
  const char format = PyC_StructFmt_type_from_str(typestr);

  if (PyC_StructFmt_type_is_float_any(format)) {
    if (itemsize == 4) {
      return IDP_FLOAT;
    }
    if (itemsize == 8) {
      return IDP_DOUBLE;
    }
  }
  if (PyC_StructFmt_type_is_int_any(format)) {
    if (itemsize == 4) {
      return IDP_INT;
    }
    /* TODO: Support Booleans? */
  }

  return -1;
}

static const char *idp_format_from_array_type(int type)
{
  if (type == IDP_INT) {
    return "i";
  }
  if (type == IDP_FLOAT) {
    return "f";
  }
  if (type == IDP_DOUBLE) {
    return "d";
  }
  if (type == IDP_BOOLEAN) {
    return "b";
  }
  return nullptr;
}

static IDProperty *idp_from_PySequence_Buffer(IDProperty *prop_exist,
                                              const char *name,
                                              const Py_buffer &buffer,
                                              const int idp_type,
                                              const bool /*do_conversion*/,
                                              const bool can_create)
{
  BLI_assert(idp_type != -1);
  IDProperty *prop = nullptr;

  if (prop_exist) {
    if (prop_exist->type == IDP_ARRAY && prop_exist->subtype == idp_type) {
      BLI_assert(buffer.len == prop_exist->len);
      memcpy(IDP_array_voidp_get(prop_exist), buffer.buf, buffer.len);
      prop = prop_exist;
    }
    /* No conversion. */
  }
  if (!prop && can_create) {
    IDPropertyTemplate val = {0};
    val.array.type = idp_type;
    val.array.len = buffer.len / buffer.itemsize;
    prop = IDP_New(IDP_ARRAY, &val, name);
    memcpy(IDP_array_voidp_get(prop), buffer.buf, buffer.len);
  }
  return prop;
}

static IDProperty *idp_from_PySequence_Fast(IDProperty *prop_exist,
                                            const char *name,
                                            PyObject *ob,
                                            const bool do_conversion,
                                            const bool can_create)
{
  IDProperty *prop = nullptr;
  IDPropertyTemplate val = {0};

  PyObject **ob_seq_fast_items;
  PyObject *item;
  int i;

  ob_seq_fast_items = PySequence_Fast_ITEMS(ob);

  /* IDProperties do not support mixed type of data in an array. Try to extract a single type from
   * the whole sequence, or error. */
  val.array.type = idp_sequence_type(ob);
  if (val.array.type == char(-1)) {
    PyErr_SetString(PyExc_TypeError,
                    "only floats, ints, booleans and dicts are allowed in ID property arrays");
    return nullptr;
  }
  if (!ELEM(val.array.type, IDP_DOUBLE, IDP_INT, IDP_BOOLEAN, IDP_IDPARRAY)) {
    /* Should never happen. */
    PyErr_SetString(PyExc_RuntimeError, "internal error with idp array.type");
    BLI_assert_unreachable();
    return nullptr;
  }

  val.array.len = PySequence_Fast_GET_SIZE(ob);

  /* NOTE: For now do not consider resizing existing array property. Also do not handle IDPARRAY.
   * - 'static type' also means 'fixed length' (e.g. vectors or matrices cases).
   * - For 'dynamic type' case, it's not really a problem if array properties get replaced
   * currently.
   */
  if (prop_exist && prop_exist->len == val.array.len) {
    switch (val.array.type) {
      case IDP_DOUBLE: {
        const bool to_float = (prop_exist->subtype == IDP_FLOAT);
        if (!(prop_exist->type == IDP_ARRAY &&
              (prop_exist->subtype == IDP_DOUBLE || (do_conversion && to_float))))
        {
          break;
        }
        prop = prop_exist;
        void *prop_data = IDP_array_voidp_get(prop);
        for (i = 0; i < val.array.len; i++) {
          item = ob_seq_fast_items[i];
          const double value = PyFloat_AsDouble(item);
          if ((value == -1.0) && PyErr_Occurred()) {
            continue;
          }
          if (to_float) {
            static_cast<float *>(prop_data)[i] = float(value);
          }
          else {
            static_cast<double *>(prop_data)[i] = value;
          }
        }
        break;
      }
      case IDP_INT: {
        const bool to_float = (prop_exist->subtype == IDP_FLOAT);
        const bool to_double = (prop_exist->subtype == IDP_DOUBLE);
        if (!(prop_exist->type == IDP_ARRAY &&
              (prop_exist->subtype == IDP_INT || (do_conversion && (to_float || to_double)))))
        {
          break;
        }
        prop = prop_exist;
        void *prop_data = IDP_array_voidp_get(prop);
        for (i = 0; i < val.array.len; i++) {
          item = ob_seq_fast_items[i];
          if (to_float || to_double) {
            const int64_t value = PyC_Long_AsI64(item);
            if ((value == -1) && PyErr_Occurred()) {
              continue;
            }
            if (to_float) {
              static_cast<float *>(prop_data)[i] = float(value);
            }
            else { /* if (to_double) */
              static_cast<double *>(prop_data)[i] = double(value);
            }
          }
          else {
            const int value = PyC_Long_AsI32(item);
            if ((value == -1) && PyErr_Occurred()) {
              continue;
            }
            static_cast<int *>(prop_data)[i] = value;
          }
        }
        break;
      }
      case IDP_BOOLEAN: {
        const bool to_int = (prop_exist->subtype == IDP_INT);
        if (!(prop_exist->type == IDP_ARRAY &&
              (prop_exist->subtype == IDP_BOOLEAN || (do_conversion && to_int))))
        {
          break;
        }
        prop = prop_exist;
        void *prop_data = IDP_array_voidp_get(prop);
        for (i = 0; i < val.array.len; i++) {
          item = ob_seq_fast_items[i];
          const int value = PyC_Long_AsBool(item);
          if ((value == -1) && PyErr_Occurred()) {
            continue;
          }
          if (to_int) {
            static_cast<int *>(prop_data)[i] = value;
          }
          else {
            static_cast<bool *>(prop_data)[i] = bool(value);
          }
        }
        break;
      }
      case IDP_IDPARRAY: {
        /* TODO? */
        break;
      }
    }
  }

  if (prop || !can_create) {
    return prop;
  }

  switch (val.array.type) {
    case IDP_DOUBLE: {
      double *prop_data;
      prop = IDP_New(IDP_ARRAY, &val, name);
      prop_data = IDP_array_double_get(prop);
      for (i = 0; i < val.array.len; i++) {
        item = ob_seq_fast_items[i];
        if (((prop_data[i] = PyFloat_AsDouble(item)) == -1.0) && PyErr_Occurred()) {
          IDP_FreeProperty(prop);
          return nullptr;
        }
      }
      break;
    }
    case IDP_INT: {
      int *prop_data;
      prop = IDP_New(IDP_ARRAY, &val, name);
      prop_data = IDP_array_int_get(prop);
      for (i = 0; i < val.array.len; i++) {
        item = ob_seq_fast_items[i];
        if (((prop_data[i] = PyC_Long_AsI32(item)) == -1) && PyErr_Occurred()) {
          IDP_FreeProperty(prop);
          return nullptr;
        }
      }
      break;
    }
    case IDP_IDPARRAY: {
      prop = IDP_NewIDPArray(name);
      for (i = 0; i < val.array.len; i++) {
        item = ob_seq_fast_items[i];
        if (BPy_IDProperty_Map_ValidateAndCreate(nullptr, prop, item) == false) {
          IDP_FreeProperty(prop);
          return nullptr;
        }
      }
      break;
    }
    case IDP_BOOLEAN: {
      prop = IDP_New(IDP_ARRAY, &val, name);
      int8_t *prop_data = IDP_array_bool_get(prop);
      for (i = 0; i < val.array.len; i++) {
        item = ob_seq_fast_items[i];
        const int value = PyC_Long_AsBool(item);
        if ((value == -1) && PyErr_Occurred()) {
          IDP_FreeProperty(prop);
          return nullptr;
        }
        prop_data[i] = (value != 0);
      }
      break;
    }
  }
  return prop;
}

static IDProperty *idp_from_PySequence(IDProperty *prop_exist,
                                       const char *name,
                                       PyObject *ob,
                                       const bool do_conversion,
                                       const bool can_create)
{
  Py_buffer buffer;
  bool use_buffer = false;
  int idp_buffer_type = -1;

  if (PyObject_CheckBuffer(ob)) {
    if (PyObject_GetBuffer(ob, &buffer, PyBUF_ND | PyBUF_FORMAT) == -1) {
      /* Request failed. A `PyExc_BufferError` will have been raised,
       * so clear it to silently fall back to accessing as a sequence. */
      PyErr_Clear();
    }
    else {
      idp_buffer_type = idp_array_type_from_formatstr_and_size(buffer.format, buffer.itemsize);
      if (idp_buffer_type != -1) {
        /* If creating a new IDProp is not allowed, and the existing one is not usable (same size
         * and type), then the 'buffer assignment' process cannot be used. */
        if (!can_create && (!prop_exist || (prop_exist->type != idp_buffer_type) ||
                            (prop_exist->len != buffer.len)))
        {
          PyBuffer_Release(&buffer);
        }
        else {
          use_buffer = true;
        }
      }
      else {
        PyBuffer_Release(&buffer);
      }
    }
  }

  if (use_buffer) {
    IDProperty *prop = idp_from_PySequence_Buffer(
        prop_exist, name, buffer, idp_buffer_type, do_conversion, can_create);
    PyBuffer_Release(&buffer);
    return prop;
  }

  PyObject *ob_seq_fast = PySequence_Fast(ob, "py -> idprop");
  if (ob_seq_fast != nullptr) {
    IDProperty *prop = idp_from_PySequence_Fast(
        prop_exist, name, ob_seq_fast, do_conversion, can_create);
    Py_DECREF(ob_seq_fast);
    return prop;
  }

  return nullptr;
}

static IDProperty *idp_from_PyMapping(IDProperty * /*prop_exist*/,
                                      const char *name,
                                      PyObject *ob,
                                      const bool /*do_conversion*/,
                                      const bool /*can_create*/)
{
  IDProperty *prop;

  /* TODO: Handle editing in-place of existing property (#IDP_FLAG_STATIC_TYPE flag). */

  PyObject *keys, *vals, *key, *pval;
  int i, len;
  /* yay! we get into recursive stuff now! */
  keys = PyMapping_Keys(ob);
  vals = PyMapping_Values(ob);

  /* We allocate the group first; if we hit any invalid data,
   * we can delete it easily enough. */
  prop = blender::bke::idprop::create_group(name).release();
  len = PyMapping_Length(ob);
  for (i = 0; i < len; i++) {
    key = PySequence_GetItem(keys, i);
    pval = PySequence_GetItem(vals, i);
    if (BPy_IDProperty_Map_ValidateAndCreate(key, prop, pval) == false) {
      IDP_FreeProperty(prop);
      Py_XDECREF(keys);
      Py_XDECREF(vals);
      Py_XDECREF(key);
      Py_XDECREF(pval);
      /* error is already set */
      return nullptr;
    }
    Py_XDECREF(key);
    Py_XDECREF(pval);
  }
  Py_XDECREF(keys);
  Py_XDECREF(vals);
  return prop;
}

static IDProperty *idp_from_DatablockPointer(IDProperty *prop_exist,
                                             const char *name,
                                             PyObject *ob,
                                             const bool /*do_conversion*/,
                                             const bool can_create)
{
  IDProperty *prop = nullptr;
  ID *value = nullptr;
  pyrna_id_FromPyObject(ob, &value);

  if (value && (value->flag & ID_FLAG_EMBEDDED_DATA) != 0) {
    PyErr_SetString(PyExc_ValueError, "Cannot assign an embedded ID pointer to an id-property");
    return nullptr;
  }

  if (prop_exist) {
    if (prop_exist->type == IDP_ID) {
      IDP_AssignID(prop_exist, value, 0);
      prop = prop_exist;
    }
    /* No conversion. */
  }
  if (!prop && can_create) {
    prop = blender::bke::idprop::create(name, value).release();
  }
  return prop;
}

static IDProperty *idp_from_PyObject(IDProperty *prop_exist,
                                     const char *name,
                                     PyObject *ob,
                                     const bool do_conversion,
                                     const bool can_create)
{
  if (name == nullptr) {
    return nullptr;
  }

  if (PyFloat_Check(ob)) {
    return idp_from_PyFloat(prop_exist, name, ob, do_conversion, can_create);
  }
  if (PyBool_Check(ob)) {
    return idp_from_PyBool(prop_exist, name, ob, do_conversion, can_create);
  }
  if (PyLong_Check(ob)) {
    return idp_from_PyLong(prop_exist, name, ob, do_conversion, can_create);
  }
  if (PyUnicode_Check(ob)) {
    return idp_from_PyUnicode(prop_exist, name, ob, do_conversion, can_create);
  }
  if (PyBytes_Check(ob)) {
    return idp_from_PyBytes(prop_exist, name, ob, do_conversion, can_create);
  }
  if (PySequence_Check(ob)) {
    return idp_from_PySequence(prop_exist, name, ob, do_conversion, can_create);
  }
  if (ob == Py_None || pyrna_id_CheckPyObject(ob)) {
    return idp_from_DatablockPointer(prop_exist, name, ob, do_conversion, can_create);
  }
  if (PyMapping_Check(ob)) {
    return idp_from_PyMapping(prop_exist, name, ob, do_conversion, can_create);
  }

  PyErr_Format(
      PyExc_TypeError, "invalid id-property type %.200s not supported", Py_TYPE(ob)->tp_name);
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mapping Get/Set (Internal Access)
 * \{ */

bool BPy_IDProperty_Map_ValidateAndCreate(PyObject *key, IDProperty *group, PyObject *ob)
{
  const char *name = idp_try_read_name(key);
  if (!name) {
    return false;
  }

  /* If the container is an array of IDProperties, always add a new property to it. */
  if (group->type == IDP_IDPARRAY) {
    IDProperty *new_prop = idp_from_PyObject(nullptr, name, ob, false, true);
    if (new_prop == nullptr) {
      return false;
    }

    IDP_AppendArray(group, new_prop);
    /* IDP_AppendArray does a shallow copy (memcpy), only free memory */
    MEM_freeN(new_prop);

    return true;
  }

  IDProperty *prop_exist = IDP_GetPropertyFromGroup(group, name);

  /* If existing property is flagged to be statically typed, do not re-type it. Assign the value if
   * possible (potentially converting it), or fail. See #122743. */
  if (prop_exist && (prop_exist->flag & IDP_FLAG_STATIC_TYPE) != 0) {
    IDProperty *prop = idp_from_PyObject(prop_exist, name, ob, true, false);
    BLI_assert(ELEM(prop, prop_exist, nullptr));
    if (prop != prop_exist) {
      PyErr_Format(PyExc_TypeError,
                   "Cannot assign a '%.200s' value to the existing '%s' %s IDProperty",
                   Py_TYPE(ob)->tp_name,
                   name,
                   IDP_type_str(prop_exist));
      return false;
    }
    return true;
  }

  /* Attempt to assign new value in existing IDProperty, if types (and potentially subtypes) match
   * exactly. Otherwise, create a new IDProperty. */
  IDProperty *new_prop = idp_from_PyObject(prop_exist, name, ob, false, true);
  if (new_prop == nullptr) {
    return false;
  }
  if (new_prop == prop_exist) {
    return true;
  }

  /* Property was created with no existing counterpart, just insert it in the group container. */
  if (!prop_exist) {
    IDP_ReplaceInGroup_ex(group, new_prop, nullptr, 0);
    return true;
  }

  /* Try to preserve UI data from the existing, replaced property. See: #37073. */
  if (prop_exist->ui_data) {
    /* Take ownership of the existing property's UI data. */
    const eIDPropertyUIDataType src_type = IDP_ui_data_type(prop_exist);
    IDPropertyUIData *ui_data = prop_exist->ui_data;
    prop_exist->ui_data = nullptr;

    new_prop->ui_data = IDP_TryConvertUIData(ui_data, src_type, IDP_ui_data_type(new_prop));
  }
  /* Copy over the 'overridable' flag from existing property. */
  new_prop->flag |= (prop_exist->flag & IDP_FLAG_OVERRIDABLE_LIBRARY);

  IDP_ReplaceInGroup_ex(group, new_prop, prop_exist, 0);
  return true;
}

int BPy_Wrap_SetMapItem(IDProperty *prop, PyObject *key, PyObject *val)
{
  if (prop->type != IDP_GROUP) {
    PyErr_SetString(PyExc_TypeError, "unsubscriptable object");
    return -1;
  }

  if (val == nullptr) { /* del idprop[key] */
    IDProperty *pkey;
    const char *name = PyUnicode_AsUTF8(key);

    if (name == nullptr) {
      PyErr_Format(PyExc_KeyError, "expected a string, not %.200s", Py_TYPE(key)->tp_name);
      return -1;
    }

    pkey = IDP_GetPropertyFromGroup(prop, name);
    if (pkey) {
      IDP_FreeFromGroup(prop, pkey);
      return 0;
    }

    PyErr_SetString(PyExc_KeyError, "property not found in group");
    return -1;
  }

  bool ok;

  ok = BPy_IDProperty_Map_ValidateAndCreate(key, prop, val);
  if (ok == false) {
    return -1;
  }

  return 0;
}

static int BPy_IDGroup_Map_SetItem(BPy_IDProperty *self, PyObject *key, PyObject *val)
{
  return BPy_Wrap_SetMapItem(self->prop, key, val);
}

static PyObject *BPy_IDGroup_iter(BPy_IDProperty *self)
{
  PyObject *iterable = BPy_IDGroup_ViewKeys_CreatePyObject(self);
  PyObject *ret;
  if (iterable) {
    ret = PyObject_GetIter(iterable);
    Py_DECREF(iterable);
  }
  else {
    ret = nullptr;
  }
  return ret;
}

PyObject *BPy_IDGroup_MapDataToPy(IDProperty *prop)
{
  switch (prop->type) {
    case IDP_STRING:
      return idprop_py_from_idp_string(prop);
    case IDP_INT:
      return idprop_py_from_idp_int(prop);
    case IDP_FLOAT:
      return idprop_py_from_idp_float(prop);
    case IDP_DOUBLE:
      return idprop_py_from_idp_double(prop);
    case IDP_BOOLEAN:
      return idprop_py_from_idp_bool(prop);
    case IDP_ID:
      return idprop_py_from_idp_id(prop);
    case IDP_ARRAY: {
      PyObject *seq = PyList_New(prop->len);
      int i;

      if (!seq) {
        PyErr_Format(
            PyExc_RuntimeError, "%s: IDP_ARRAY: PyList_New(%d) failed", __func__, prop->len);
        return nullptr;
      }

      switch (prop->subtype) {
        case IDP_FLOAT: {
          const float *array = IDP_array_float_get(prop);
          for (i = 0; i < prop->len; i++) {
            PyList_SET_ITEM(seq, i, PyFloat_FromDouble(array[i]));
          }
          break;
        }
        case IDP_DOUBLE: {
          const double *array = IDP_array_double_get(prop);
          for (i = 0; i < prop->len; i++) {
            PyList_SET_ITEM(seq, i, PyFloat_FromDouble(array[i]));
          }
          break;
        }
        case IDP_INT: {
          const int *array = IDP_array_int_get(prop);
          for (i = 0; i < prop->len; i++) {
            PyList_SET_ITEM(seq, i, PyLong_FromLong(array[i]));
          }
          break;
        }
        case IDP_BOOLEAN: {
          const int8_t *array = IDP_array_bool_get(prop);
          for (i = 0; i < prop->len; i++) {
            PyList_SET_ITEM(seq, i, PyBool_FromLong(array[i]));
          }
          break;
        }
        default:
          PyErr_Format(
              PyExc_RuntimeError, "%s: invalid/corrupt array type '%d'!", __func__, prop->subtype);
          Py_DECREF(seq);
          return nullptr;
      }

      return seq;
    }
    case IDP_IDPARRAY: {
      PyObject *seq = PyList_New(prop->len);
      IDProperty *array = IDP_property_array_get(prop);
      int i;

      if (!seq) {
        PyErr_Format(
            PyExc_RuntimeError, "%s: IDP_IDPARRAY: PyList_New(%d) failed", __func__, prop->len);
        return nullptr;
      }

      for (i = 0; i < prop->len; i++) {
        PyObject *wrap = BPy_IDGroup_MapDataToPy(array++);

        /* BPy_IDGroup_MapDataToPy sets the error */
        if (UNLIKELY(wrap == nullptr)) {
          Py_DECREF(seq);
          return nullptr;
        }

        PyList_SET_ITEM(seq, i, wrap);
      }
      return seq;
    }
    case IDP_GROUP: {
      PyObject *dict = _PyDict_NewPresized(prop->len);
      IDProperty *loop;

      for (loop = static_cast<IDProperty *>(prop->data.group.first); loop; loop = loop->next) {
        PyObject *wrap = BPy_IDGroup_MapDataToPy(loop);

        /* BPy_IDGroup_MapDataToPy sets the error */
        if (UNLIKELY(wrap == nullptr)) {
          Py_DECREF(dict);
          return nullptr;
        }

        PyDict_SetItemString(dict, loop->name, wrap);
        Py_DECREF(wrap);
      }
      return dict;
    }
  }

  PyErr_Format(PyExc_RuntimeError,
               "%s ERROR: '%s' property exists with a bad type code '%d'!",
               __func__,
               prop->name,
               prop->type);
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID-Property Group Iterator Type
 * \{ */

static PyObject *BPy_IDGroup_Iter_repr(BPy_IDGroup_Iter *self)
{
  if (self->group == nullptr) {
    return PyUnicode_FromFormat("<%s>", Py_TYPE(self)->tp_name);
  }
  return PyUnicode_FromFormat("<%s \"%s\">", Py_TYPE(self)->tp_name, self->group->prop->name);
}

static void BPy_IDGroup_Iter_dealloc(BPy_IDGroup_Iter *self)
{
  if (self->group != nullptr) {
    PyObject_GC_UnTrack(self);
  }
  Py_CLEAR(self->group);
  PyObject_GC_Del(self);
}

static int BPy_IDGroup_Iter_traverse(BPy_IDGroup_Iter *self, visitproc visit, void *arg)
{
  Py_VISIT(self->group);
  return 0;
}

static int BPy_IDGroup_Iter_clear(BPy_IDGroup_Iter *self)
{
  Py_CLEAR(self->group);
  return 0;
}

static int BPy_IDGroup_Iter_is_gc(BPy_IDGroup_Iter *self)
{
  return (self->group != nullptr);
}

static bool BPy_Group_Iter_same_size_or_raise_error(BPy_IDGroup_Iter *self)
{
  if (self->len_init == self->group->prop->len) {
    return true;
  }
  PyErr_SetString(PyExc_RuntimeError, "IDPropertyGroup changed size during iteration");
  return false;
}

static PyObject *BPy_Group_IterKeys_next(BPy_IDGroup_Iter *self)
{
  if (self->cur != nullptr) {
    /* When `cur` is set, `group` cannot be nullptr. */
    if (!BPy_Group_Iter_same_size_or_raise_error(self)) {
      return nullptr;
    }
    IDProperty *cur = self->cur;
    self->cur = self->reversed ? self->cur->prev : self->cur->next;
    return PyUnicode_FromString(cur->name);
  }
  PyErr_SetNone(PyExc_StopIteration);
  return nullptr;
}

static PyObject *BPy_Group_IterValues_next(BPy_IDGroup_Iter *self)
{
  if (self->cur != nullptr) {
    /* When `cur` is set, `group` cannot be nullptr. */
    if (!BPy_Group_Iter_same_size_or_raise_error(self)) {
      return nullptr;
    }
    IDProperty *cur = self->cur;
    self->cur = self->reversed ? self->cur->prev : self->cur->next;
    return BPy_IDGroup_WrapData(self->group->owner_id, cur, self->group->prop);
  }
  PyErr_SetNone(PyExc_StopIteration);
  return nullptr;
}

static PyObject *BPy_Group_IterItems_next(BPy_IDGroup_Iter *self)
{
  if (self->cur != nullptr) {
    /* When `cur` is set, `group` cannot be nullptr. */
    if (!BPy_Group_Iter_same_size_or_raise_error(self)) {
      return nullptr;
    }
    IDProperty *cur = self->cur;
    self->cur = self->reversed ? self->cur->prev : self->cur->next;
    PyObject *ret = PyTuple_New(2);
    PyTuple_SET_ITEMS(ret,
                      PyUnicode_FromString(cur->name),
                      BPy_IDGroup_WrapData(self->group->owner_id, cur, self->group->prop));
    return ret;
  }
  PyErr_SetNone(PyExc_StopIteration);
  return nullptr;
}

PyTypeObject BPy_IDGroup_IterKeys_Type = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject BPy_IDGroup_IterValues_Type = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject BPy_IDGroup_IterItems_Type = {PyVarObject_HEAD_INIT(nullptr, 0)};

/* ID Property Group Iterator. */
static void IDGroup_Iter_init_type()
{
#define SHARED_MEMBER_SET(member, value) \
  { \
    k_ty->member = v_ty->member = i_ty->member = value; \
  } \
  ((void)0)

  PyTypeObject *k_ty = &BPy_IDGroup_IterKeys_Type;
  PyTypeObject *v_ty = &BPy_IDGroup_IterValues_Type;
  PyTypeObject *i_ty = &BPy_IDGroup_IterItems_Type;

  /* Unique members. */
  k_ty->tp_name = "IDPropertyGroupIterKeys";
  v_ty->tp_name = "IDPropertyGroupIterValues";
  i_ty->tp_name = "IDPropertyGroupIterItems";

  k_ty->tp_iternext = (iternextfunc)BPy_Group_IterKeys_next;
  v_ty->tp_iternext = (iternextfunc)BPy_Group_IterValues_next;
  i_ty->tp_iternext = (iternextfunc)BPy_Group_IterItems_next;

  /* Shared members. */
  SHARED_MEMBER_SET(tp_basicsize, sizeof(BPy_IDGroup_Iter));
  SHARED_MEMBER_SET(tp_dealloc, (destructor)BPy_IDGroup_Iter_dealloc);
  SHARED_MEMBER_SET(tp_repr, (reprfunc)BPy_IDGroup_Iter_repr);
  SHARED_MEMBER_SET(tp_flags, Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC);
  SHARED_MEMBER_SET(tp_traverse, (traverseproc)BPy_IDGroup_Iter_traverse);
  SHARED_MEMBER_SET(tp_clear, (inquiry)BPy_IDGroup_Iter_clear);
  SHARED_MEMBER_SET(tp_is_gc, (inquiry)BPy_IDGroup_Iter_is_gc);
  SHARED_MEMBER_SET(tp_iter, PyObject_SelfIter);

#undef SHARED_MEMBER_SET
}

static PyObject *IDGroup_Iter_New_WithType(BPy_IDProperty *group,
                                           const bool reversed,
                                           PyTypeObject *type)
{
  BLI_assert(group ? group->prop->type == IDP_GROUP : true);
  BPy_IDGroup_Iter *iter = PyObject_GC_New(BPy_IDGroup_Iter, type);
  iter->reversed = reversed;
  iter->group = group;
  if (group != nullptr) {
    Py_INCREF(group);
    BLI_assert(!PyObject_GC_IsTracked((PyObject *)iter));
    PyObject_GC_Track(iter);
    iter->cur = static_cast<IDProperty *>(
        (reversed ? group->prop->data.group.last : group->prop->data.group.first));
    iter->len_init = group->prop->len;
  }
  else {
    iter->cur = nullptr;
    iter->len_init = 0;
  }
  return (PyObject *)iter;
}

static PyObject *BPy_IDGroup_IterKeys_CreatePyObject(BPy_IDProperty *group, const bool reversed)
{
  return IDGroup_Iter_New_WithType(group, reversed, &BPy_IDGroup_IterKeys_Type);
}

static PyObject *BPy_IDGroup_IterValues_CreatePyObject(BPy_IDProperty *group, const bool reversed)
{
  return IDGroup_Iter_New_WithType(group, reversed, &BPy_IDGroup_IterValues_Type);
}

static PyObject *BPy_IDGroup_IterItems_CreatePyObject(BPy_IDProperty *group, const bool reversed)
{
  return IDGroup_Iter_New_WithType(group, reversed, &BPy_IDGroup_IterItems_Type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID-Property Group View Types (Keys/Values/Items)
 *
 * This view types is a thin wrapper on keys/values/items, this matches Python's `dict_view` type.
 * This is returned by `property.keys()` and is separate from the iterator that loops over keys.
 *
 * There are some less common features this type could support (matching Python's `dict_view`)
 *
 * TODO:
 * - Efficient contains checks for values and items which currently convert to a list first.
 * - Missing `dict_views.isdisjoint`.
 * - Missing `tp_as_number` (`nb_subtract`, `nb_and`, `nb_xor`, `nb_or`).
 * \{ */

static PyObject *BPy_IDGroup_View_repr(BPy_IDGroup_View *self)
{
  if (self->group == nullptr) {
    return PyUnicode_FromFormat("<%s>", Py_TYPE(self)->tp_name);
  }
  return PyUnicode_FromFormat("<%s \"%s\">", Py_TYPE(self)->tp_name, self->group->prop->name);
}

static void BPy_IDGroup_View_dealloc(BPy_IDGroup_View *self)
{
  if (self->group != nullptr) {
    PyObject_GC_UnTrack(self);
  }
  Py_CLEAR(self->group);
  PyObject_GC_Del(self);
}

static int BPy_IDGroup_View_traverse(BPy_IDGroup_View *self, visitproc visit, void *arg)
{
  Py_VISIT(self->group);
  return 0;
}

static int BPy_IDGroup_View_clear(BPy_IDGroup_View *self)
{
  Py_CLEAR(self->group);
  return 0;
}

static int BPy_IDGroup_View_is_gc(BPy_IDGroup_View *self)
{
  return (self->group != nullptr);
}

/* View Specific API's (Key/Value/Items). */

static PyObject *BPy_Group_ViewKeys_iter(BPy_IDGroup_View *self)
{
  return BPy_IDGroup_IterKeys_CreatePyObject(self->group, self->reversed);
}

static PyObject *BPy_Group_ViewValues_iter(BPy_IDGroup_View *self)
{
  return BPy_IDGroup_IterValues_CreatePyObject(self->group, self->reversed);
}

static PyObject *BPy_Group_ViewItems_iter(BPy_IDGroup_View *self)
{
  return BPy_IDGroup_IterItems_CreatePyObject(self->group, self->reversed);
}

static Py_ssize_t BPy_Group_View_len(BPy_IDGroup_View *self)
{
  if (self->group == nullptr) {
    return 0;
  }
  return self->group->prop->len;
}

static int BPy_Group_ViewKeys_Contains(BPy_IDGroup_View *self, PyObject *value)
{
  if (self->group == nullptr) {
    return 0;
  }
  return BPy_IDGroup_Contains(self->group, value);
}

static int BPy_Group_ViewValues_Contains(BPy_IDGroup_View *self, PyObject *value)
{
  if (self->group == nullptr) {
    return 0;
  }
  /* TODO: implement this without first converting to a list. */
  PyObject *list = PySequence_List((PyObject *)self);
  const int result = PySequence_Contains(list, value);
  Py_DECREF(list);
  return result;
}

static int BPy_Group_ViewItems_Contains(BPy_IDGroup_View *self, PyObject *value)
{
  if (self->group == nullptr) {
    return 0;
  }
  /* TODO: implement this without first converting to a list. */
  PyObject *list = PySequence_List((PyObject *)self);
  const int result = PySequence_Contains(list, value);
  Py_DECREF(list);
  return result;
}

static PySequenceMethods BPy_IDGroup_ViewKeys_as_sequence = {
    /*sq_length*/ (lenfunc)BPy_Group_View_len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ nullptr,
    /*was_sq_slice*/ nullptr,
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr,
    /*sq_contains*/ (objobjproc)BPy_Group_ViewKeys_Contains,
};

static PySequenceMethods BPy_IDGroup_ViewValues_as_sequence = {
    /*sq_length*/ (lenfunc)BPy_Group_View_len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ nullptr,
    /*was_sq_slice*/ nullptr,
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr,
    /*sq_contains*/ (objobjproc)BPy_Group_ViewValues_Contains,
};

static PySequenceMethods BPy_IDGroup_ViewItems_as_sequence = {
    /*sq_length*/ (lenfunc)BPy_Group_View_len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ nullptr,
    /*was_sq_slice*/ nullptr,
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr,
    /*sq_contains*/ (objobjproc)BPy_Group_ViewItems_Contains,
};

/* Methods. */

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_View_reversed_doc,
    "Return a reverse iterator over the ID Property keys values or items.");
static PyObject *BPy_IDGroup_View_reversed(BPy_IDGroup_View *self, PyObject * /*ignored*/)
{
  BPy_IDGroup_View *result = IDGroup_View_New_WithType(self->group, Py_TYPE(self));
  result->reversed = !self->reversed;
  return (PyObject *)result;
}

static PyMethodDef BPy_IDGroup_View_methods[] = {
    {"__reversed__",
     (PyCFunction)(void (*)())BPy_IDGroup_View_reversed,
     METH_NOARGS,
     BPy_IDGroup_View_reversed_doc},
    {nullptr, nullptr},
};

PyTypeObject BPy_IDGroup_ViewKeys_Type = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject BPy_IDGroup_ViewValues_Type = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject BPy_IDGroup_ViewItems_Type = {PyVarObject_HEAD_INIT(nullptr, 0)};

/* ID Property Group View. */
static void IDGroup_View_init_type()
{
  PyTypeObject *k_ty = &BPy_IDGroup_ViewKeys_Type;
  PyTypeObject *v_ty = &BPy_IDGroup_ViewValues_Type;
  PyTypeObject *i_ty = &BPy_IDGroup_ViewItems_Type;

  /* Unique members. */
  k_ty->tp_name = "IDPropertyGroupViewKeys";
  v_ty->tp_name = "IDPropertyGroupViewValues";
  i_ty->tp_name = "IDPropertyGroupViewItems";

  k_ty->tp_iter = (getiterfunc)BPy_Group_ViewKeys_iter;
  v_ty->tp_iter = (getiterfunc)BPy_Group_ViewValues_iter;
  i_ty->tp_iter = (getiterfunc)BPy_Group_ViewItems_iter;

  k_ty->tp_as_sequence = &BPy_IDGroup_ViewKeys_as_sequence;
  v_ty->tp_as_sequence = &BPy_IDGroup_ViewValues_as_sequence;
  i_ty->tp_as_sequence = &BPy_IDGroup_ViewItems_as_sequence;

/* Shared members. */
#define SHARED_MEMBER_SET(member, value) \
  { \
    k_ty->member = v_ty->member = i_ty->member = value; \
  } \
  ((void)0)

  SHARED_MEMBER_SET(tp_basicsize, sizeof(BPy_IDGroup_View));
  SHARED_MEMBER_SET(tp_dealloc, (destructor)BPy_IDGroup_View_dealloc);
  SHARED_MEMBER_SET(tp_repr, (reprfunc)BPy_IDGroup_View_repr);
  SHARED_MEMBER_SET(tp_flags, Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC);
  SHARED_MEMBER_SET(tp_traverse, (traverseproc)BPy_IDGroup_View_traverse);
  SHARED_MEMBER_SET(tp_clear, (inquiry)BPy_IDGroup_View_clear);
  SHARED_MEMBER_SET(tp_is_gc, (inquiry)BPy_IDGroup_View_is_gc);
  SHARED_MEMBER_SET(tp_methods, BPy_IDGroup_View_methods);

#undef SHARED_MEMBER_SET
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID-Property Group Methods
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_pop_doc,
    ".. method:: pop(key, default)\n"
    "\n"
    "   Remove an item from the group, returning a Python representation.\n"
    "\n"
    "   :raises KeyError: When the item doesn't exist.\n"
    "\n"
    "   :arg key: Name of item to remove.\n"
    "   :type key: str\n"
    "   :arg default: Value to return when key isn't found, otherwise raise an exception.\n"
    "   :type default: Any\n");
static PyObject *BPy_IDGroup_pop(BPy_IDProperty *self, PyObject *args)
{
  IDProperty *idprop;
  PyObject *pyform;

  char *key;
  PyObject *def = nullptr;

  if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
    return nullptr;
  }

  idprop = IDP_GetPropertyFromGroup(self->prop, key);
  if (idprop == nullptr) {
    if (def == nullptr) {
      PyErr_SetString(PyExc_KeyError, "item not in group");
      return nullptr;
    }
    return Py_NewRef(def);
  }

  pyform = BPy_IDGroup_MapDataToPy(idprop);
  if (pyform == nullptr) {
    /* Ok something bad happened with the #PyObject, so don't remove the prop from the group.
     * if `pyform` is nullptr, then it already should have raised an exception. */
    return nullptr;
  }

  IDP_FreeFromGroup(self->prop, idprop);
  return pyform;
}

/* utility function */
static void BPy_IDGroup_CorrectListLen(IDProperty *prop, PyObject *seq, int len, const char *func)
{
  int j;

  printf("%s: ID Property Error found and corrected!\n", func);

  /* fill rest of list with valid references to None */
  for (j = len; j < prop->len; j++) {
    PyList_SET_ITEM(seq, j, Py_NewRef(Py_None));
  }

  /* Set correct group length. */
  prop->len = len;
}

PyObject *BPy_Wrap_GetKeys(IDProperty *prop)
{
  PyObject *list = PyList_New(prop->len);
  IDProperty *loop;
  int i;

  for (i = 0, loop = static_cast<IDProperty *>(prop->data.group.first); loop && (i < prop->len);
       loop = loop->next, i++)
  {
    PyList_SET_ITEM(list, i, PyUnicode_FromString(loop->name));
  }

  /* if the id prop is corrupt, count the remaining */
  for (; loop; loop = loop->next, i++) {
    /* pass */
  }

  if (i != prop->len) { /* if the loop didn't finish, we know the length is wrong */
    BPy_IDGroup_CorrectListLen(prop, list, i, __func__);
    Py_DECREF(list); /* Free the list. */
    /* Call self again. */
    return BPy_Wrap_GetKeys(prop);
  }

  return list;
}

PyObject *BPy_Wrap_GetValues(ID *id, IDProperty *prop)
{
  PyObject *list = PyList_New(prop->len);
  IDProperty *loop;
  int i;

  for (i = 0, loop = static_cast<IDProperty *>(prop->data.group.first); loop;
       loop = loop->next, i++)
  {
    PyList_SET_ITEM(list, i, BPy_IDGroup_WrapData(id, loop, prop));
  }

  if (i != prop->len) {
    BPy_IDGroup_CorrectListLen(prop, list, i, __func__);
    Py_DECREF(list); /* Free the list. */
    /* Call self again. */
    return BPy_Wrap_GetValues(id, prop);
  }

  return list;
}

PyObject *BPy_Wrap_GetItems(ID *id, IDProperty *prop)
{
  PyObject *seq = PyList_New(prop->len);
  IDProperty *loop;
  int i;

  for (i = 0, loop = static_cast<IDProperty *>(prop->data.group.first); loop;
       loop = loop->next, i++)
  {
    PyObject *item = PyTuple_New(2);
    PyTuple_SET_ITEMS(
        item, PyUnicode_FromString(loop->name), BPy_IDGroup_WrapData(id, loop, prop));
    PyList_SET_ITEM(seq, i, item);
  }

  if (i != prop->len) {
    BPy_IDGroup_CorrectListLen(prop, seq, i, __func__);
    Py_DECREF(seq); /* Free the list. */
    /* Call self again. */
    return BPy_Wrap_GetItems(id, prop);
  }

  return seq;
}

PyObject *BPy_Wrap_GetKeys_View_WithID(ID *id, IDProperty *prop)
{
  PyObject *self = prop ? idprop_py_from_idp_group(id, prop, nullptr) : nullptr;
  PyObject *ret = BPy_IDGroup_ViewKeys_CreatePyObject((BPy_IDProperty *)self);
  Py_XDECREF(self); /* Owned by `ret`. */
  return ret;
}

PyObject *BPy_Wrap_GetValues_View_WithID(ID *id, IDProperty *prop)
{
  PyObject *self = prop ? idprop_py_from_idp_group(id, prop, nullptr) : nullptr;
  PyObject *ret = BPy_IDGroup_ViewValues_CreatePyObject((BPy_IDProperty *)self);
  Py_XDECREF(self); /* Owned by `ret`. */
  return ret;
}

PyObject *BPy_Wrap_GetItems_View_WithID(ID *id, IDProperty *prop)
{
  PyObject *self = prop ? idprop_py_from_idp_group(id, prop, nullptr) : nullptr;
  PyObject *ret = BPy_IDGroup_ViewItems_CreatePyObject((BPy_IDProperty *)self);
  Py_XDECREF(self); /* Owned by `ret`. */
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_keys_doc,
    ".. method:: keys()\n"
    "\n"
    "   Return the keys associated with this group as a list of strings.\n");
static PyObject *BPy_IDGroup_keys(BPy_IDProperty *self)
{
  return BPy_IDGroup_ViewKeys_CreatePyObject(self);
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_values_doc,
    ".. method:: values()\n"
    "\n"
    "   Return the values associated with this group.\n");
static PyObject *BPy_IDGroup_values(BPy_IDProperty *self)
{
  return BPy_IDGroup_ViewValues_CreatePyObject(self);
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_items_doc,
    ".. method:: items()\n"
    "\n"
    "   Iterate through the items in the dict; behaves like dictionary method items.\n");
static PyObject *BPy_IDGroup_items(BPy_IDProperty *self)
{
  return BPy_IDGroup_ViewItems_CreatePyObject(self);
}

static int BPy_IDGroup_Contains(BPy_IDProperty *self, PyObject *value)
{
  const char *name = PyUnicode_AsUTF8(value);

  if (!name) {
    PyErr_Format(PyExc_TypeError, "expected a string, not a %.200s", Py_TYPE(value)->tp_name);
    return -1;
  }

  return IDP_GetPropertyFromGroup(self->prop, name) ? 1 : 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_update_doc,
    ".. method:: update(other)\n"
    "\n"
    "   Update key, values.\n"
    "\n"
    "   :arg other: Updates the values in the group with this.\n"
    /* TODO: replace `Any` with an alias for all types an ID property can use. */
    "   :type other: :class:`IDPropertyGroup` | dict[str, Any]\n");
static PyObject *BPy_IDGroup_update(BPy_IDProperty *self, PyObject *value)
{
  PyObject *pkey, *pval;
  Py_ssize_t i = 0;

  if (BPy_IDGroup_Check(value)) {
    BPy_IDProperty *other = (BPy_IDProperty *)value;
    if (UNLIKELY(self->prop == other->prop)) {
      Py_RETURN_NONE;
    }

    /* XXX, possible one is inside the other */
    IDP_MergeGroup(self->prop, other->prop, true);
  }
  else if (PyDict_Check(value)) {
    while (PyDict_Next(value, &i, &pkey, &pval)) {
      BPy_IDGroup_Map_SetItem(self, pkey, pval);
      if (PyErr_Occurred()) {
        return nullptr;
      }
    }
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "expected a dict or an IDPropertyGroup type, not a %.200s",
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_to_dict_doc,
    ".. method:: to_dict()\n"
    "\n"
    "   Return a purely Python version of the group.\n");
static PyObject *BPy_IDGroup_to_dict(BPy_IDProperty *self)
{
  return BPy_IDGroup_MapDataToPy(self->prop);
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_clear_doc,
    ".. method:: clear()\n"
    "\n"
    "   Clear all members from this group.\n");
static PyObject *BPy_IDGroup_clear(BPy_IDProperty *self)
{
  IDP_ClearProperty(self->prop);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDGroup_get_doc,
    ".. method:: get(key, default=None)\n"
    "\n"
    "   Return the value for key, if it exists, else default.\n");
static PyObject *BPy_IDGroup_get(BPy_IDProperty *self, PyObject *args)
{
  IDProperty *idprop;
  const char *key;
  PyObject *def = Py_None;

  if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
    return nullptr;
  }

  idprop = IDP_GetPropertyFromGroup(self->prop, key);
  if (idprop) {
    PyObject *pyobj = BPy_IDGroup_WrapData(self->owner_id, idprop, self->prop);
    if (pyobj) {
      return pyobj;
    }
  }

  Py_INCREF(def);
  return def;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef BPy_IDGroup_methods[] = {
    {"pop", (PyCFunction)BPy_IDGroup_pop, METH_VARARGS, BPy_IDGroup_pop_doc},
    {"keys", (PyCFunction)BPy_IDGroup_keys, METH_NOARGS, BPy_IDGroup_keys_doc},
    {"values", (PyCFunction)BPy_IDGroup_values, METH_NOARGS, BPy_IDGroup_values_doc},
    {"items", (PyCFunction)BPy_IDGroup_items, METH_NOARGS, BPy_IDGroup_items_doc},
    {"update", (PyCFunction)BPy_IDGroup_update, METH_O, BPy_IDGroup_update_doc},
    {"get", (PyCFunction)BPy_IDGroup_get, METH_VARARGS, BPy_IDGroup_get_doc},
    {"to_dict", (PyCFunction)BPy_IDGroup_to_dict, METH_NOARGS, BPy_IDGroup_to_dict_doc},
    {"clear", (PyCFunction)BPy_IDGroup_clear, METH_NOARGS, BPy_IDGroup_clear_doc},
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
/** \name ID-Property Group Type
 * \{ */

static PySequenceMethods BPy_IDGroup_Seq = {
    /*sq_length*/ (lenfunc)BPy_IDGroup_Map_Len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /* TODO: setting this will allow `PySequence_Check()` to return True. */
    /*sq_item*/ nullptr,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ (objobjproc)BPy_IDGroup_Contains,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods BPy_IDGroup_Mapping = {
    /*mp_length*/ (lenfunc)BPy_IDGroup_Map_Len,
    /*mp_subscript*/ (binaryfunc)BPy_IDGroup_Map_GetItem,
    /*mp_ass_subscript*/ (objobjargproc)BPy_IDGroup_Map_SetItem,
};

PyTypeObject BPy_IDGroup_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /* For printing, in format `<module>.<name>`. */
    /*tp_name*/ "IDPropertyGroup",
    /*tp_basicsize*/ sizeof(BPy_IDProperty),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)BPy_IDGroup_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ &BPy_IDGroup_Seq,
    /*tp_as_mapping*/ &BPy_IDGroup_Mapping,
    /*tp_hash*/ (hashfunc)BPy_IDGroup_hash,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ (getiterfunc)BPy_IDGroup_iter,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_IDGroup_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_IDGroup_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Array Methods
 * \{ */

static PyTypeObject *idp_array_py_type(BPy_IDArray *self, size_t *elem_size)
{
  switch (self->prop->subtype) {
    case IDP_FLOAT:
      *elem_size = sizeof(float);
      return &PyFloat_Type;
    case IDP_DOUBLE:
      *elem_size = sizeof(double);
      return &PyFloat_Type;
    case IDP_BOOLEAN:
      *elem_size = sizeof(int8_t);
      return &PyBool_Type;
    case IDP_INT:
      *elem_size = sizeof(int);
      return &PyLong_Type;
    default:
      *elem_size = 0;
      return nullptr;
  }
}

static PyObject *BPy_IDArray_repr(BPy_IDArray *self)
{
  return PyUnicode_FromFormat("<bpy id property array [%d]>", self->prop->len);
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDArray_get_typecode_doc,
    "The type of the data in the array {'f': float, 'd': double, 'i': int, 'b': bool}.");
static PyObject *BPy_IDArray_get_typecode(BPy_IDArray *self, void * /*closure*/)
{
  const char *typecode;
  switch (self->prop->subtype) {
    case IDP_FLOAT:
      typecode = "f";
      break;
    case IDP_DOUBLE:
      typecode = "d";
      break;
    case IDP_INT:
      typecode = "i";
      break;
    case IDP_BOOLEAN:
      typecode = "b";
      break;
    default: {
      PyErr_Format(PyExc_RuntimeError,
                   "%s: invalid/corrupt array type '%d'!",
                   __func__,
                   self->prop->subtype);

      return nullptr;
    }
  }
  return PyUnicode_FromString(typecode);
}

static PyGetSetDef BPy_IDArray_getseters[] = {
    /* matches pythons array.typecode */
    {"typecode",
     (getter)BPy_IDArray_get_typecode,
     (setter) nullptr,
     BPy_IDArray_get_typecode_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr},
};

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDArray_to_list_doc,
    ".. method:: to_list()\n"
    "\n"
    "   Return the array as a list.\n");
static PyObject *BPy_IDArray_to_list(BPy_IDArray *self)
{
  return BPy_IDGroup_MapDataToPy(self->prop);
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef BPy_IDArray_methods[] = {
    {"to_list", (PyCFunction)BPy_IDArray_to_list, METH_NOARGS, BPy_IDArray_to_list_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static Py_ssize_t BPy_IDArray_Len(BPy_IDArray *self)
{
  return self->prop->len;
}

static PyObject *BPy_IDArray_GetItem(BPy_IDArray *self, Py_ssize_t index)
{
  if (index < 0 || index >= self->prop->len) {
    PyErr_SetString(PyExc_IndexError, "index out of range!");
    return nullptr;
  }

  switch (self->prop->subtype) {
    case IDP_FLOAT:
      return PyFloat_FromDouble(IDP_array_float_get(self->prop)[index]);
    case IDP_DOUBLE:
      return PyFloat_FromDouble(IDP_array_double_get(self->prop)[index]);
    case IDP_INT:
      return PyLong_FromLong(long(IDP_array_int_get(self->prop)[index]));
    case IDP_BOOLEAN:
      return PyBool_FromLong(long(IDP_array_bool_get(self->prop)[index]));
  }

  PyErr_Format(
      PyExc_RuntimeError, "%s: invalid/corrupt array type '%d'!", __func__, self->prop->subtype);

  return nullptr;
}

static int BPy_IDArray_SetItem(BPy_IDArray *self, Py_ssize_t index, PyObject *value)
{
  if (index < 0 || index >= self->prop->len) {
    PyErr_SetString(PyExc_RuntimeError, "index out of range!");
    return -1;
  }

  switch (self->prop->subtype) {
    case IDP_FLOAT: {
      const float f = float(PyFloat_AsDouble(value));
      if (f == -1 && PyErr_Occurred()) {
        return -1;
      }
      IDP_array_float_get(self->prop)[index] = f;
      break;
    }
    case IDP_DOUBLE: {
      const double d = PyFloat_AsDouble(value);
      if (d == -1 && PyErr_Occurred()) {
        return -1;
      }
      IDP_array_double_get(self->prop)[index] = d;
      break;
    }
    case IDP_INT: {
      const int i = PyC_Long_AsI32(value);
      if (i == -1 && PyErr_Occurred()) {
        return -1;
      }

      IDP_array_int_get(self->prop)[index] = i;
      break;
    }
    case IDP_BOOLEAN: {
      const int i = PyC_Long_AsBool(value);
      if (i == -1 && PyErr_Occurred()) {
        return -1;
      }

      IDP_array_bool_get(self->prop)[index] = i;
      break;
    }
  }
  return 0;
}

static PySequenceMethods BPy_IDArray_Seq = {
    /*sq_length*/ (lenfunc)BPy_IDArray_Len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ (ssizeargfunc)BPy_IDArray_GetItem,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ (ssizeobjargproc)BPy_IDArray_SetItem,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ nullptr,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

/* sequence slice (get): idparr[a:b] */
static PyObject *BPy_IDArray_slice(BPy_IDArray *self, int begin, int end)
{
  IDProperty *prop = self->prop;
  PyObject *tuple;
  int count;

  CLAMP(begin, 0, prop->len);
  if (end < 0) {
    end = prop->len + end + 1;
  }
  CLAMP(end, 0, prop->len);
  begin = std::min(begin, end);

  tuple = PyTuple_New(end - begin);

  switch (prop->subtype) {
    case IDP_FLOAT: {
      const float *array = IDP_array_float_get(prop);
      for (count = begin; count < end; count++) {
        PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(array[count]));
      }
      break;
    }
    case IDP_DOUBLE: {
      const double *array = IDP_array_double_get(prop);
      for (count = begin; count < end; count++) {
        PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(array[count]));
      }
      break;
    }
    case IDP_INT: {
      const int *array = IDP_array_int_get(prop);
      for (count = begin; count < end; count++) {
        PyTuple_SET_ITEM(tuple, count - begin, PyLong_FromLong(array[count]));
      }
      break;
    }
    case IDP_BOOLEAN: {
      const int8_t *array = IDP_array_bool_get(prop);
      for (count = begin; count < end; count++) {
        PyTuple_SET_ITEM(tuple, count - begin, PyBool_FromLong(long(array[count])));
      }
      break;
    }
  }

  return tuple;
}
/* sequence slice (set): idparr[a:b] = value */
static int BPy_IDArray_ass_slice(BPy_IDArray *self, int begin, int end, PyObject *seq)
{
  IDProperty *prop = self->prop;
  size_t elem_size;
  const PyTypeObject *py_type = idp_array_py_type(self, &elem_size);
  size_t alloc_len;
  size_t size;
  void *vec;

  CLAMP(begin, 0, prop->len);
  CLAMP(end, 0, prop->len);
  begin = std::min(begin, end);

  size = (end - begin);
  alloc_len = size * elem_size;

  /* NOTE: we count on int/float being the same size here */
  vec = MEM_mallocN(alloc_len, "array assignment");

  if (PyC_AsArray(vec, elem_size, seq, size, py_type, "slice assignment: ") == -1) {
    MEM_freeN(vec);
    return -1;
  }

  memcpy((void *)(((char *)IDP_array_voidp_get(prop)) + (begin * elem_size)), vec, alloc_len);

  MEM_freeN(vec);
  return 0;
}

static PyObject *BPy_IDArray_subscript(BPy_IDArray *self, PyObject *item)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i;
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }
    if (i < 0) {
      i += self->prop->len;
    }
    return BPy_IDArray_GetItem(self, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->prop->len, &start, &stop, &step, &slicelength) < 0) {
      return nullptr;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return BPy_IDArray_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_TypeError, "slice steps not supported with vectors");
    return nullptr;
  }

  PyErr_Format(PyExc_TypeError,
               "vector indices must be integers, not %.200s",
               __func__,
               Py_TYPE(item)->tp_name);
  return nullptr;
}

static int BPy_IDArray_ass_subscript(BPy_IDArray *self, PyObject *item, PyObject *value)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }
    if (i < 0) {
      i += self->prop->len;
    }
    return BPy_IDArray_SetItem(self, i, value);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->prop->len, &start, &stop, &step, &slicelength) < 0) {
      return -1;
    }

    if (step == 1) {
      return BPy_IDArray_ass_slice(self, start, stop, value);
    }

    PyErr_SetString(PyExc_TypeError, "slice steps not supported with vectors");
    return -1;
  }

  PyErr_Format(
      PyExc_TypeError, "vector indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return -1;
}

static PyMappingMethods BPy_IDArray_AsMapping = {
    /*mp_length*/ (lenfunc)BPy_IDArray_Len,
    /*mp_subscript*/ (binaryfunc)BPy_IDArray_subscript,
    /*mp_ass_subscript*/ (objobjargproc)BPy_IDArray_ass_subscript,
};

static int itemsize_by_idarray_type(int array_type)
{
  if (array_type == IDP_INT) {
    return sizeof(int);
  }
  if (array_type == IDP_FLOAT) {
    return sizeof(float);
  }
  if (array_type == IDP_DOUBLE) {
    return sizeof(double);
  }
  if (array_type == IDP_BOOLEAN) {
    return sizeof(bool);
  }
  return -1; /* should never happen */
}

static int BPy_IDArray_getbuffer(BPy_IDArray *self, Py_buffer *view, int flags)
{
  IDProperty *prop = self->prop;
  const int itemsize = itemsize_by_idarray_type(prop->subtype);
  const int length = itemsize * prop->len;

  if (PyBuffer_FillInfo(view, (PyObject *)self, IDP_array_voidp_get(prop), length, false, flags) ==
      -1)
  {
    return -1;
  }

  view->itemsize = itemsize;
  view->format = (char *)idp_format_from_array_type(prop->subtype);

  Py_ssize_t *shape = MEM_mallocN<Py_ssize_t>(__func__);
  shape[0] = prop->len;
  view->shape = shape;

  return 0;
}

static void BPy_IDArray_releasebuffer(BPy_IDArray * /*self*/, Py_buffer *view)
{
  MEM_freeN(view->shape);
}

static PyBufferProcs BPy_IDArray_Buffer = {
    /*bf_getbuffer*/ (getbufferproc)BPy_IDArray_getbuffer,
    /*bf_releasebuffer*/ (releasebufferproc)BPy_IDArray_releasebuffer,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Array Type
 * \{ */

PyTypeObject BPy_IDArray_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /* For printing, in format `<module>.<name>`. */
    /*tp_name*/ "IDPropertyArray",
    /*tp_basicsize*/ sizeof(BPy_IDArray),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)BPy_IDArray_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ &BPy_IDArray_Seq,
    /*tp_as_mapping*/ &BPy_IDArray_AsMapping,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ &BPy_IDArray_Buffer,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_IDArray_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_IDArray_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initialize Types
 * \{ */

void IDProp_Init_Types()
{
  IDGroup_Iter_init_type();
  IDGroup_View_init_type();

  PyType_Ready(&BPy_IDGroup_Type);
  PyType_Ready(&BPy_IDArray_Type);

  PyType_Ready(&BPy_IDGroup_IterKeys_Type);
  PyType_Ready(&BPy_IDGroup_IterValues_Type);
  PyType_Ready(&BPy_IDGroup_IterItems_Type);

  PyType_Ready(&BPy_IDGroup_ViewKeys_Type);
  PyType_Ready(&BPy_IDGroup_ViewValues_Type);
  PyType_Ready(&BPy_IDGroup_ViewItems_Type);
}

/**
 * \note `group` may be nullptr, unlike most other uses of this argument.
 * This is supported so RNA keys/values/items methods returns an iterator with the expected type:
 * - Without having ID-properties.
 * - Without supporting #BPy_IDProperty.prop being nullptr, which would incur many more checks.
 * Python's own dictionary-views also works this way too.
 */
static BPy_IDGroup_View *IDGroup_View_New_WithType(BPy_IDProperty *group, PyTypeObject *type)
{
  BLI_assert(group ? group->prop->type == IDP_GROUP : true);
  BPy_IDGroup_View *iter = PyObject_GC_New(BPy_IDGroup_View, type);
  iter->reversed = false;
  iter->group = group;
  if (group != nullptr) {
    Py_INCREF(group);
    BLI_assert(!PyObject_GC_IsTracked((PyObject *)iter));
    PyObject_GC_Track(iter);
  }
  return iter;
}

static PyObject *BPy_IDGroup_ViewKeys_CreatePyObject(BPy_IDProperty *group)
{
  return (PyObject *)IDGroup_View_New_WithType(group, &BPy_IDGroup_ViewKeys_Type);
}

static PyObject *BPy_IDGroup_ViewValues_CreatePyObject(BPy_IDProperty *group)
{
  return (PyObject *)IDGroup_View_New_WithType(group, &BPy_IDGroup_ViewValues_Type);
}

static PyObject *BPy_IDGroup_ViewItems_CreatePyObject(BPy_IDProperty *group)
{
  return (PyObject *)IDGroup_View_New_WithType(group, &BPy_IDGroup_ViewItems_Type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Module 'idprop.types'
 * \{ */

static PyModuleDef IDProp_types_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "idprop.types",
    /*m_doc*/ nullptr,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

static PyObject *BPyInit_idprop_types()
{
  PyObject *submodule;

  submodule = PyModule_Create(&IDProp_types_module_def);

  IDProp_Init_Types();
  IDPropertyUIData_Init_Types();

  /* `bmesh_py_types.cc` */
  PyModule_AddType(submodule, &BPy_IDGroup_Type);

  PyModule_AddType(submodule, &BPy_IDGroup_ViewKeys_Type);
  PyModule_AddType(submodule, &BPy_IDGroup_ViewValues_Type);
  PyModule_AddType(submodule, &BPy_IDGroup_ViewItems_Type);

  PyModule_AddType(submodule, &BPy_IDGroup_IterKeys_Type);
  PyModule_AddType(submodule, &BPy_IDGroup_IterValues_Type);
  PyModule_AddType(submodule, &BPy_IDGroup_IterItems_Type);

  PyModule_AddType(submodule, &BPy_IDArray_Type);

  return submodule;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Module 'idprop'
 * \{ */

static PyMethodDef IDProp_methods[] = {
    {nullptr, nullptr, 0, nullptr},
};

PyDoc_STRVAR(
    /* Wrap. */
    IDProp_module_doc,
    "This module provides access id property types (currently mainly for docs).");
static PyModuleDef IDProp_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "idprop",
    /*m_doc*/ IDProp_module_doc,
    /*m_size*/ 0,
    /*m_methods*/ IDProp_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_idprop()
{
  PyObject *mod;
  PyObject *submodule;
  PyObject *sys_modules = PyImport_GetModuleDict();

  mod = PyModule_Create(&IDProp_module_def);

  /* idprop.types */
  PyModule_AddObject(mod, "types", (submodule = BPyInit_idprop_types()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  return mod;
}

/** \} */
