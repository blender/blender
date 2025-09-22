/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

#include <Python.h>

#include "python_compat.hh" /* IWYU pragma: keep. */

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "idprop_py_ui_api.hh"

#include "BKE_idprop.hh"

#include "DNA_ID.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#define USE_STRING_COERCE

#ifdef USE_STRING_COERCE
#  include "py_capi_utils.hh"
#endif
#include "py_capi_rna.hh"

/* -------------------------------------------------------------------- */
/** \name UI Data Update
 * \{ */

static bool args_contain_key(PyObject *kwargs, const char *name)
{
  /* When a function gets called without any kwargs, */
  /* Python just passes nullptr instead. #PyDict_GetItemString() is not null-safe, though. */
  return kwargs && PyDict_GetItemString(kwargs, name) != nullptr;
}

/**
 * \return False when parsing fails, in which case caller should return nullptr.
 */
static bool idprop_ui_data_update_base(IDPropertyUIData *ui_data,
                                       const char *rna_subtype,
                                       const char *description)
{
  if (rna_subtype != nullptr) {
    if (pyrna_enum_value_from_id(rna_enum_property_subtype_items,
                                 rna_subtype,
                                 &ui_data->rna_subtype,
                                 "IDPropertyUIManager.update") == -1)
    {
      return false;
    }
  }

  if (description != nullptr) {
    ui_data->description = BLI_strdup(description);
  }

  return true;
}

/* Utility function for parsing ints in an if statement. */
static bool py_long_as_int(PyObject *py_long, int *r_int)
{
  if (PyLong_CheckExact(py_long)) {
    *r_int = int(PyLong_AS_LONG(py_long));
    return true;
  }
  return false;
}

/**
 * Similar to #enum_items_from_py, which parses enum items for RNA properties.
 * This function is simpler, since it doesn't have to parse a default value or handle the case of
 * enum flags (PROP_ENUM_FLAG).
 */
static bool try_parse_enum_item(PyObject *py_item, const int index, IDPropertyUIDataEnumItem &item)
{
  if (!PyTuple_CheckExact(py_item)) {
    return false;
  }
  Py_ssize_t item_size = PyTuple_GET_SIZE(py_item);
  if (item_size < 3 || item_size > 5) {
    return false;
  }

  Py_ssize_t identifier_len;
  Py_ssize_t name_len;
  Py_ssize_t description_len;
  const char *identifier = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(py_item, 0), &identifier_len);
  const char *name = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(py_item, 1), &name_len);
  const char *description = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(py_item, 2),
                                                    &description_len);
  if (!identifier || !name || !description) {
    return false;
  }

  const char *icon_name = nullptr;
  if (item_size <= 3) {
    item.value = index;
  }
  else if (item_size == 4) {
    if (!py_long_as_int(PyTuple_GET_ITEM(py_item, 3), &item.value)) {
      return false;
    }
  }
  else if (item_size == 5) {
    /* Must have icon value or name. */
    if (!py_long_as_int(PyTuple_GET_ITEM(py_item, 3), &item.icon) &&
        !(icon_name = PyUnicode_AsUTF8(PyTuple_GET_ITEM(py_item, 3))))
    {
      return false;
    }
    if (!py_long_as_int(PyTuple_GET_ITEM(py_item, 4), &item.value)) {
      return false;
    }
  }

  item.identifier = BLI_strdup(identifier);
  item.name = BLI_strdup(name);
  item.description = BLI_strdup_null(description);
  if (icon_name) {
    RNA_enum_value_from_identifier(rna_enum_icon_items, icon_name, &item.icon);
  }
  return true;
}

static IDPropertyUIDataEnumItem *idprop_enum_items_from_py(PyObject *seq_fast, int &r_items_num)
{
  IDPropertyUIDataEnumItem *items;

  const Py_ssize_t seq_len = PySequence_Fast_GET_SIZE(seq_fast);
  PyObject **seq_fast_items = PySequence_Fast_ITEMS(seq_fast);
  int i;

  items = MEM_calloc_arrayN<IDPropertyUIDataEnumItem>(seq_len, __func__);
  r_items_num = seq_len;

  for (i = 0; i < seq_len; i++) {
    IDPropertyUIDataEnumItem item = {nullptr, nullptr, nullptr, 0, 0};
    PyObject *py_item = seq_fast_items[i];
    if (try_parse_enum_item(py_item, i, item)) {
      items[i] = item;
    }
    else if (py_item == Py_None) {
      items[i].identifier = nullptr;
    }
    else {
      MEM_freeN(items);
      PyErr_SetString(PyExc_TypeError,
                      "expected a tuple containing "
                      "(identifier, name, description) and optionally an "
                      "icon name and unique number");
      return nullptr;
    }
  }

  return items;
}

/**
 * \note The default value needs special handling because for array IDProperties it can
 * be a single value or an array, but for non-array properties it can only be a value.
 */
static bool idprop_ui_data_update_int_default(IDProperty *idprop,
                                              IDPropertyUIDataInt *ui_data,
                                              PyObject *default_value)
{
  if (PySequence_Check(default_value)) {
    if (idprop->type != IDP_ARRAY) {
      PyErr_SetString(PyExc_TypeError, "Only array properties can have array default values");
      return false;
    }

    Py_ssize_t len = PySequence_Size(default_value);
    int *new_default_array = MEM_malloc_arrayN<int>(size_t(len), __func__);
    if (PyC_AsArray(
            new_default_array, sizeof(int), default_value, len, &PyLong_Type, "ui_data_update") ==
        -1)
    {
      MEM_freeN(new_default_array);
      return false;
    }

    ui_data->default_array_len = len;
    ui_data->default_array = new_default_array;
  }
  else {
    const int value = PyC_Long_AsI32(default_value);
    if ((value == -1) && PyErr_Occurred()) {
      PyErr_SetString(PyExc_ValueError, "Cannot convert \"default\" argument to integer");
      return false;
    }

    /* Use the non-array default, even for arrays, also prevent dangling pointer, see #127952. */
    ui_data->default_array = nullptr;
    ui_data->default_array_len = 0;

    ui_data->default_value = value;
  }

  return true;
}

/**
 * \return False when parsing fails, in which case caller should return nullptr.
 */
static bool idprop_ui_data_update_int(IDProperty *idprop, PyObject *args, PyObject *kwargs)
{
  const char *rna_subtype = nullptr;
  const char *description = nullptr;
  int min, max, soft_min, soft_max, step;
  PyObject *default_value = nullptr;
  PyObject *items = nullptr;
  const char *kwlist[] = {
      "min",
      "max",
      "soft_min",
      "soft_max",
      "step",
      "default",
      "items",
      "subtype",
      "description",
      nullptr,
  };
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "|$iiiiiOOzz:update",
                                   (char **)kwlist,
                                   &min,
                                   &max,
                                   &soft_min,
                                   &soft_max,
                                   &step,
                                   &default_value,
                                   &items,
                                   &rna_subtype,
                                   &description))
  {
    return false;
  }

  /* Write to a temporary copy of the UI data in case some part of the parsing fails. */
  IDPropertyUIDataInt *ui_data_orig = (IDPropertyUIDataInt *)idprop->ui_data;
  IDPropertyUIDataInt ui_data = *ui_data_orig;

  if (!idprop_ui_data_update_base(&ui_data.base, rna_subtype, description)) {
    IDP_ui_data_free_unique_contents(&ui_data.base, IDP_ui_data_type(idprop), &ui_data_orig->base);
    return false;
  }

  if (args_contain_key(kwargs, "min")) {
    ui_data.min = min;
    ui_data.soft_min = std::max(ui_data.soft_min, ui_data.min);
    ui_data.max = std::max(ui_data.min, ui_data.max);
  }
  if (args_contain_key(kwargs, "max")) {
    ui_data.max = max;
    ui_data.soft_max = std::min(ui_data.soft_max, ui_data.max);
    ui_data.min = std::min(ui_data.min, ui_data.max);
  }
  if (args_contain_key(kwargs, "soft_min")) {
    ui_data.soft_min = soft_min;
    ui_data.soft_min = std::max(ui_data.soft_min, ui_data.min);
    ui_data.soft_max = std::max(ui_data.soft_min, ui_data.soft_max);
  }
  if (args_contain_key(kwargs, "soft_max")) {
    ui_data.soft_max = soft_max;
    ui_data.soft_max = std::min(ui_data.soft_max, ui_data.max);
    ui_data.soft_min = std::min(ui_data.soft_min, ui_data.soft_max);
  }
  if (args_contain_key(kwargs, "step")) {
    ui_data.step = step;
  }

  if (!ELEM(default_value, nullptr, Py_None)) {
    if (!idprop_ui_data_update_int_default(idprop, &ui_data, default_value)) {
      IDP_ui_data_free_unique_contents(
          &ui_data.base, IDP_ui_data_type(idprop), &ui_data_orig->base);
      return false;
    }
  }

  if (!ELEM(items, nullptr, Py_None)) {
    PyObject *items_fast;
    if (!(items_fast = PySequence_Fast(items, "expected a sequence of tuples for the enum items")))
    {
      return false;
    }

    int idprop_items_num = 0;
    IDPropertyUIDataEnumItem *idprop_items = idprop_enum_items_from_py(items_fast,
                                                                       idprop_items_num);
    if (!idprop_items) {
      Py_DECREF(items_fast);
      return false;
    }
    if (!IDP_EnumItemsValidate(idprop_items, idprop_items_num, [](const char *msg) {
          PyErr_SetString(PyExc_ValueError, msg);
        }))
    {
      Py_DECREF(items_fast);
      return false;
    }
    Py_DECREF(items_fast);
    ui_data.enum_items = idprop_items;
    ui_data.enum_items_num = idprop_items_num;
  }
  else {
    ui_data.enum_items = nullptr;
    ui_data.enum_items_num = 0;
  }

  /* Write back to the property's UI data. */
  IDP_ui_data_free_unique_contents(&ui_data_orig->base, IDP_ui_data_type(idprop), &ui_data.base);
  *ui_data_orig = ui_data;
  return true;
}

/**
 * \note The default value needs special handling because for array IDProperties it can
 * be a single value or an array, but for non-array properties it can only be a value.
 */
static bool idprop_ui_data_update_bool_default(IDProperty *idprop,
                                               IDPropertyUIDataBool *ui_data,
                                               PyObject *default_value)
{
  if (PySequence_Check(default_value)) {
    if (idprop->type != IDP_ARRAY) {
      PyErr_SetString(PyExc_TypeError, "Only array properties can have array default values");
      return false;
    }

    Py_ssize_t len = PySequence_Size(default_value);
    int8_t *new_default_array = MEM_malloc_arrayN<int8_t>(size_t(len), __func__);
    if (PyC_AsArray(new_default_array,
                    sizeof(int8_t),
                    default_value,
                    len,
                    &PyBool_Type,
                    "ui_data_update") == -1)
    {
      MEM_freeN(new_default_array);
      return false;
    }

    ui_data->default_array_len = len;
    ui_data->default_array = new_default_array;
  }
  else {
    const int value = PyC_Long_AsBool(default_value);
    if ((value == -1) && PyErr_Occurred()) {
      PyErr_SetString(PyExc_ValueError, "Cannot convert \"default\" argument to integer");
      return false;
    }

    /* Use the non-array default, even for arrays, also prevent dangling pointer, see #127952. */
    ui_data->default_array_len = 0;
    ui_data->default_array = nullptr;

    ui_data->default_value = (value != 0);
  }

  return true;
}

/**
 * \return False when parsing fails, in which case caller should return nullptr.
 */
static bool idprop_ui_data_update_bool(IDProperty *idprop, PyObject *args, PyObject *kwargs)
{
  const char *rna_subtype = nullptr;
  const char *description = nullptr;
  PyObject *default_value = nullptr;
  const char *kwlist[] = {"default", "subtype", "description", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "|$Ozz:update",
                                   (char **)kwlist,
                                   &default_value,
                                   &rna_subtype,
                                   &description))
  {
    return false;
  }

  /* Write to a temporary copy of the UI data in case some part of the parsing fails. */
  IDPropertyUIDataBool *ui_data_orig = (IDPropertyUIDataBool *)idprop->ui_data;
  IDPropertyUIDataBool ui_data = *ui_data_orig;

  if (!idprop_ui_data_update_base(&ui_data.base, rna_subtype, description)) {
    IDP_ui_data_free_unique_contents(&ui_data.base, IDP_ui_data_type(idprop), &ui_data_orig->base);
    return false;
  }

  if (!ELEM(default_value, nullptr, Py_None)) {
    if (!idprop_ui_data_update_bool_default(idprop, &ui_data, default_value)) {
      IDP_ui_data_free_unique_contents(
          &ui_data.base, IDP_ui_data_type(idprop), &ui_data_orig->base);
      return false;
    }
  }

  /* Write back to the property's UI data. */
  IDP_ui_data_free_unique_contents(&ui_data_orig->base, IDP_ui_data_type(idprop), &ui_data.base);
  *ui_data_orig = ui_data;
  return true;
}

/**
 * \note The default value needs special handling because for array IDProperties it can
 * be a single value or an array, but for non-array properties it can only be a value.
 */
static bool idprop_ui_data_update_float_default(IDProperty *idprop,
                                                IDPropertyUIDataFloat *ui_data,
                                                PyObject *default_value)
{
  if (PySequence_Check(default_value)) {
    if (idprop->type != IDP_ARRAY) {
      PyErr_SetString(PyExc_TypeError, "Only array properties can have array default values");
      return false;
    }

    Py_ssize_t len = PySequence_Size(default_value);
    double *new_default_array = MEM_malloc_arrayN<double>(size_t(len), __func__);
    if (PyC_AsArray(new_default_array,
                    sizeof(double),
                    default_value,
                    len,
                    &PyFloat_Type,
                    "ui_data_update") == -1)
    {
      MEM_freeN(new_default_array);
      return false;
    }

    ui_data->default_array_len = len;
    ui_data->default_array = new_default_array;
  }
  else {
    const double value = PyFloat_AsDouble(default_value);
    if ((value == -1.0) && PyErr_Occurred()) {
      PyErr_SetString(PyExc_ValueError, "Cannot convert \"default\" argument to double");
      return false;
    }

    /* Use the non-array default, even for arrays, also prevent dangling pointer, see #127952. */
    ui_data->default_array_len = 0;
    ui_data->default_array = nullptr;

    ui_data->default_value = value;
  }

  return true;
}

/**
 * \return False when parsing fails, in which case caller should return nullptr.
 */
static bool idprop_ui_data_update_float(IDProperty *idprop, PyObject *args, PyObject *kwargs)
{
  const char *rna_subtype = nullptr;
  const char *description = nullptr;
  int precision;
  double min, max, soft_min, soft_max, step;
  PyObject *default_value = nullptr;
  const char *kwlist[] = {"min",
                          "max",
                          "soft_min",
                          "soft_max",
                          "step",
                          "precision",
                          "default",
                          "subtype",
                          "description",
                          nullptr};
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "|$dddddiOzz:update",
                                   (char **)kwlist,
                                   &min,
                                   &max,
                                   &soft_min,
                                   &soft_max,
                                   &step,
                                   &precision,
                                   &default_value,
                                   &rna_subtype,
                                   &description))
  {
    return false;
  }

  /* Write to a temporary copy of the UI data in case some part of the parsing fails. */
  IDPropertyUIDataFloat *ui_data_orig = (IDPropertyUIDataFloat *)idprop->ui_data;
  IDPropertyUIDataFloat ui_data = *ui_data_orig;

  if (!idprop_ui_data_update_base(&ui_data.base, rna_subtype, description)) {
    IDP_ui_data_free_unique_contents(&ui_data.base, IDP_ui_data_type(idprop), &ui_data_orig->base);
    return false;
  }

  if (args_contain_key(kwargs, "min")) {
    ui_data.min = min;
    ui_data.soft_min = std::max(ui_data.soft_min, ui_data.min);
    ui_data.max = std::max(ui_data.min, ui_data.max);
  }
  if (args_contain_key(kwargs, "max")) {
    ui_data.max = max;
    ui_data.soft_max = std::min(ui_data.soft_max, ui_data.max);
    ui_data.min = std::min(ui_data.min, ui_data.max);
  }
  if (args_contain_key(kwargs, "soft_min")) {
    ui_data.soft_min = soft_min;
    ui_data.soft_min = std::max(ui_data.soft_min, ui_data.min);
    ui_data.soft_max = std::max(ui_data.soft_min, ui_data.soft_max);
  }
  if (args_contain_key(kwargs, "soft_max")) {
    ui_data.soft_max = soft_max;
    ui_data.soft_max = std::min(ui_data.soft_max, ui_data.max);
    ui_data.soft_min = std::min(ui_data.soft_min, ui_data.soft_max);
  }
  if (args_contain_key(kwargs, "step")) {
    ui_data.step = float(step);
  }
  if (args_contain_key(kwargs, "precision")) {
    ui_data.precision = precision;
  }

  if (!ELEM(default_value, nullptr, Py_None)) {
    if (!idprop_ui_data_update_float_default(idprop, &ui_data, default_value)) {
      IDP_ui_data_free_unique_contents(
          &ui_data.base, IDP_ui_data_type(idprop), &ui_data_orig->base);
      return false;
    }
  }

  /* Write back to the property's UI data. */
  IDP_ui_data_free_unique_contents(&ui_data_orig->base, IDP_ui_data_type(idprop), &ui_data.base);
  *ui_data_orig = ui_data;
  return true;
}

/**
 * \return False when parsing fails, in which case caller should return nullptr.
 */
static bool idprop_ui_data_update_string(IDProperty *idprop, PyObject *args, PyObject *kwargs)
{
  const char *rna_subtype = nullptr;
  const char *description = nullptr;
  const char *default_value = nullptr;
  const char *kwlist[] = {"default", "subtype", "description", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "|$zzz:update",
                                   (char **)kwlist,
                                   &default_value,
                                   &rna_subtype,
                                   &description))
  {
    return false;
  }

  /* Write to a temporary copy of the UI data in case some part of the parsing fails. */
  IDPropertyUIDataString *ui_data_orig = (IDPropertyUIDataString *)idprop->ui_data;
  IDPropertyUIDataString ui_data = *ui_data_orig;

  if (!idprop_ui_data_update_base(&ui_data.base, rna_subtype, description)) {
    IDP_ui_data_free_unique_contents(&ui_data.base, IDP_ui_data_type(idprop), &ui_data_orig->base);
    return false;
  }

  if (default_value != nullptr) {
    ui_data.default_value = BLI_strdup(default_value);
  }

  /* Write back to the property's UI data. */
  IDP_ui_data_free_unique_contents(&ui_data_orig->base, IDP_ui_data_type(idprop), &ui_data.base);
  *ui_data_orig = ui_data;
  return true;
}

/**
 * \return False when parsing fails, in which case caller should return nullptr.
 */
static bool idprop_ui_data_update_id(IDProperty *idprop, PyObject *args, PyObject *kwargs)
{
  const char *rna_subtype = nullptr;
  const char *description = nullptr;
  const char *id_type = nullptr;
  const char *kwlist[] = {"subtype", "description", "id_type", nullptr};
  if (!PyArg_ParseTupleAndKeywords(
          args, kwargs, "|$zzz:update", (char **)kwlist, &rna_subtype, &description, &id_type))
  {
    return false;
  }

  /* Write to a temporary copy of the UI data in case some part of the parsing fails. */
  IDPropertyUIDataID *ui_data_orig = (IDPropertyUIDataID *)idprop->ui_data;
  IDPropertyUIDataID ui_data = *ui_data_orig;

  if (!idprop_ui_data_update_base(&ui_data.base, rna_subtype, description)) {
    IDP_ui_data_free_unique_contents(&ui_data.base, IDP_ui_data_type(idprop), &ui_data_orig->base);
    return false;
  }

  if (id_type != nullptr) {
    int id_type_tmp;
    if (pyrna_enum_value_from_id(
            rna_enum_id_type_items, id_type, &id_type_tmp, "IDPropertyUIManager.update") == -1)
    {
      return false;
    }

    ui_data.id_type = short(id_type_tmp);
  }

  /* Write back to the property's UI data. */
  IDP_ui_data_free_unique_contents(&ui_data_orig->base, IDP_ui_data_type(idprop), &ui_data.base);
  *ui_data_orig = ui_data;
  return true;
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDPropertyUIManager_update_doc,
    ".. method:: update(*, "
    "subtype=None, "
    "min=None, "
    "max=None, "
    "soft_min=None, "
    "soft_max=None, "
    "precision=None, "
    "step=None, "
    "default=None, "
    "id_type=None, "
    "items=None, "
    "description=None)\n"
    "\n"
    "   Update the RNA information of the IDProperty used for interaction and\n"
    "   display in the user interface. The required types for many of the keyword\n"
    "   arguments depend on the type of the property.\n");
static PyObject *BPy_IDPropertyUIManager_update(BPy_IDPropertyUIManager *self,
                                                PyObject *args,
                                                PyObject *kwargs)
{
  IDProperty *property = self->property;
  BLI_assert(IDP_ui_data_supported(property));

  switch (IDP_ui_data_type(property)) {
    case IDP_UI_DATA_TYPE_INT:
      IDP_ui_data_ensure(property);
      if (!idprop_ui_data_update_int(property, args, kwargs)) {
        return nullptr;
      }
      Py_RETURN_NONE;
    case IDP_UI_DATA_TYPE_BOOLEAN:
      IDP_ui_data_ensure(property);
      if (!idprop_ui_data_update_bool(property, args, kwargs)) {
        return nullptr;
      }
      Py_RETURN_NONE;
    case IDP_UI_DATA_TYPE_FLOAT:
      IDP_ui_data_ensure(property);
      if (!idprop_ui_data_update_float(property, args, kwargs)) {
        return nullptr;
      }
      Py_RETURN_NONE;
    case IDP_UI_DATA_TYPE_STRING:
      IDP_ui_data_ensure(property);
      if (!idprop_ui_data_update_string(property, args, kwargs)) {
        return nullptr;
      }
      Py_RETURN_NONE;
    case IDP_UI_DATA_TYPE_ID:
      IDP_ui_data_ensure(property);
      if (!idprop_ui_data_update_id(property, args, kwargs)) {
        return nullptr;
      }
      Py_RETURN_NONE;
    case IDP_UI_DATA_TYPE_UNSUPPORTED:
      PyErr_Format(PyExc_TypeError, "IDProperty \"%s\" does not support RNA data", property->name);
      return nullptr;
  }

  BLI_assert_unreachable();
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Data As Dictionary
 * \{ */

static void idprop_ui_data_to_dict_int(IDProperty *property, PyObject *dict)
{
  IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)property->ui_data;
  PyObject *item;

  PyDict_SetItemString(dict, "min", item = PyLong_FromLong(ui_data->min));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "max", item = PyLong_FromLong(ui_data->max));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "soft_min", item = PyLong_FromLong(ui_data->soft_min));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "soft_max", item = PyLong_FromLong(ui_data->soft_max));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "step", item = PyLong_FromLong(ui_data->step));
  Py_DECREF(item);
  if ((property->type == IDP_ARRAY) && ui_data->default_array) {
    PyObject *list = PyList_New(ui_data->default_array_len);
    for (int i = 0; i < ui_data->default_array_len; i++) {
      PyList_SET_ITEM(list, i, PyLong_FromLong(ui_data->default_array[i]));
    }
    PyDict_SetItemString(dict, "default", list);
    Py_DECREF(list);
  }
  else {
    PyDict_SetItemString(dict, "default", item = PyLong_FromLong(ui_data->default_value));
    Py_DECREF(item);
  }

  if (ui_data->enum_items_num > 0) {
    PyObject *items_list = PyList_New(ui_data->enum_items_num);
    for (int i = 0; i < ui_data->enum_items_num; ++i) {
      const IDPropertyUIDataEnumItem &item = ui_data->enum_items[i];
      BLI_assert(item.identifier != nullptr);
      BLI_assert(item.name != nullptr);

      PyObject *item_tuple = PyTuple_New(5);
      PyTuple_SET_ITEM(item_tuple, 0, PyUnicode_FromString(item.identifier));
      PyTuple_SET_ITEM(item_tuple, 1, PyUnicode_FromString(item.name));
      PyTuple_SET_ITEM(
          item_tuple, 2, PyUnicode_FromString(item.description ? item.description : ""));
      PyTuple_SET_ITEM(item_tuple, 3, PyLong_FromLong(item.icon));
      PyTuple_SET_ITEM(item_tuple, 4, PyLong_FromLong(item.value));

      PyList_SET_ITEM(items_list, i, item_tuple);
    }
    PyDict_SetItemString(dict, "items", items_list);
    Py_DECREF(items_list);
  }
}

static void idprop_ui_data_to_dict_bool(IDProperty *property, PyObject *dict)
{
  IDPropertyUIDataBool *ui_data = (IDPropertyUIDataBool *)property->ui_data;
  PyObject *item;

  if ((property->type == IDP_ARRAY) && ui_data->default_array) {
    PyObject *list = PyList_New(ui_data->default_array_len);
    for (int i = 0; i < ui_data->default_array_len; i++) {
      PyList_SET_ITEM(list, i, PyBool_FromLong(ui_data->default_array[i]));
    }
    PyDict_SetItemString(dict, "default", list);
    Py_DECREF(list);
  }
  else {
    PyDict_SetItemString(dict, "default", item = PyBool_FromLong(ui_data->default_value));
    Py_DECREF(item);
  }
}

static void idprop_ui_data_to_dict_float(IDProperty *property, PyObject *dict)
{
  IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)property->ui_data;
  PyObject *item;

  PyDict_SetItemString(dict, "min", item = PyFloat_FromDouble(ui_data->min));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "max", item = PyFloat_FromDouble(ui_data->max));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "soft_min", item = PyFloat_FromDouble(ui_data->soft_min));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "soft_max", item = PyFloat_FromDouble(ui_data->soft_max));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "step", item = PyFloat_FromDouble(double(ui_data->step)));
  Py_DECREF(item);
  PyDict_SetItemString(dict, "precision", item = PyLong_FromDouble(double(ui_data->precision)));
  Py_DECREF(item);
  if ((property->type == IDP_ARRAY) && ui_data->default_array) {
    PyObject *list = PyList_New(ui_data->default_array_len);
    for (int i = 0; i < ui_data->default_array_len; i++) {
      PyList_SET_ITEM(list, i, PyFloat_FromDouble(ui_data->default_array[i]));
    }
    PyDict_SetItemString(dict, "default", list);
    Py_DECREF(list);
  }
  else {
    PyDict_SetItemString(dict, "default", item = PyFloat_FromDouble(ui_data->default_value));
    Py_DECREF(item);
  }
}

static void idprop_ui_data_to_dict_string(IDProperty *property, PyObject *dict)
{
  IDPropertyUIDataString *ui_data = (IDPropertyUIDataString *)property->ui_data;
  PyObject *item;

  const char *default_value = (ui_data->default_value == nullptr) ? "" : ui_data->default_value;

  PyDict_SetItemString(dict, "default", item = PyUnicode_FromString(default_value));
  Py_DECREF(item);
}

static void idprop_ui_data_to_dict_id(IDProperty *property, PyObject *dict)
{
  IDPropertyUIDataID *ui_data = reinterpret_cast<IDPropertyUIDataID *>(property->ui_data);

  short id_type_value = ui_data->id_type;
  if (id_type_value == 0) {
    /* While UI exposed custom properties do not allow the 'all ID types' `0` value, in
     * py-defined IDProperties it is accepted. So force defining a valid id_type value when this
     * function is called. */
    ID *id = IDP_ID_get(property);
    id_type_value = id ? GS(id->name) : ID_OB;
  }

  const char *id_type = nullptr;
  if (!RNA_enum_identifier(rna_enum_id_type_items, id_type_value, &id_type)) {
    /* Same fall-back as above, in case it is an unknown ID type (from a future version of
     * Blender e.g.). */
    RNA_enum_identifier(rna_enum_id_type_items, ID_OB, &id_type);
  }
  PyObject *item = PyUnicode_FromString(id_type);
  PyDict_SetItemString(dict, "id_type", item);
  Py_DECREF(item);
}

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDPropertyUIManager_as_dict_doc,
    ".. method:: as_dict()\n"
    "\n"
    "   Return a dictionary of the property's RNA UI data. The fields in the\n"
    "   returned dictionary and their types will depend on the property's type.\n");
static PyObject *BPy_IDIDPropertyUIManager_as_dict(BPy_IDPropertyUIManager *self)
{
  IDProperty *property = self->property;
  BLI_assert(IDP_ui_data_supported(property));

  IDPropertyUIData *ui_data = IDP_ui_data_ensure(property);

  PyObject *dict = PyDict_New();

  /* RNA subtype. */
  {
    const char *subtype_id = nullptr;
    RNA_enum_identifier(rna_enum_property_subtype_items, ui_data->rna_subtype, &subtype_id);
    PyObject *item = PyUnicode_FromString(subtype_id);
    PyDict_SetItemString(dict, "subtype", item);
    Py_DECREF(item);
  }

  /* Description. */
  if (ui_data->description != nullptr) {
    PyObject *item = PyUnicode_FromString(ui_data->description);
    PyDict_SetItemString(dict, "description", item);
    Py_DECREF(item);
  }

  /* Type specific data. */
  switch (IDP_ui_data_type(property)) {
    case IDP_UI_DATA_TYPE_STRING:
      idprop_ui_data_to_dict_string(property, dict);
      break;
    case IDP_UI_DATA_TYPE_ID:
      idprop_ui_data_to_dict_id(property, dict);
      break;
    case IDP_UI_DATA_TYPE_INT:
      idprop_ui_data_to_dict_int(property, dict);
      break;
    case IDP_UI_DATA_TYPE_BOOLEAN:
      idprop_ui_data_to_dict_bool(property, dict);
      break;
    case IDP_UI_DATA_TYPE_FLOAT:
      idprop_ui_data_to_dict_float(property, dict);
      break;
    case IDP_UI_DATA_TYPE_UNSUPPORTED:
      BLI_assert_unreachable();
      break;
  }

  return dict;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Data Clear
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDPropertyUIManager_clear_doc,
    ".. method:: clear()\n"
    "\n"
    "   Remove the RNA UI data from this IDProperty.\n");
static PyObject *BPy_IDPropertyUIManager_clear(BPy_IDPropertyUIManager *self)
{
  IDProperty *property = self->property;
  BLI_assert(IDP_ui_data_supported(property));

  if (property == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "IDPropertyUIManager missing property");
    BLI_assert_unreachable();
    return nullptr;
  }

  if (property->ui_data != nullptr) {
    IDP_ui_data_free(property);
  }

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Data Copying
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    BPy_IDPropertyUIManager_update_from_doc,
    ".. method:: update_from(ui_manager_source)\n"
    "\n"
    "   Copy UI data from an IDProperty in the source group to a property in this group.\n "
    "   If the source property has no UI data, the target UI data will be reset if it exists.\n"
    "\n"
    "   :raises TypeError: If the types of the two properties don't match.\n");
static PyObject *BPy_IDPropertyUIManager_update_from(BPy_IDPropertyUIManager *self, PyObject *args)
{
  IDProperty *property = self->property;
  BLI_assert(IDP_ui_data_supported(property));

  BPy_IDPropertyUIManager *ui_manager_src;
  if (!PyArg_ParseTuple(args, "O!:update_from", &BPy_IDPropertyUIManager_Type, &ui_manager_src)) {
    return nullptr;
  }

  if (property->ui_data != nullptr) {
    IDP_ui_data_free(property);
  }

  if (ui_manager_src->property && ui_manager_src->property->ui_data) {
    property->ui_data = IDP_ui_data_copy(ui_manager_src->property);
  }

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Data Manager Definition
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

static PyMethodDef BPy_IDPropertyUIManager_methods[] = {
    {"update",
     (PyCFunction)BPy_IDPropertyUIManager_update,
     METH_VARARGS | METH_KEYWORDS,
     BPy_IDPropertyUIManager_update_doc},
    {"as_dict",
     (PyCFunction)BPy_IDIDPropertyUIManager_as_dict,
     METH_NOARGS,
     BPy_IDPropertyUIManager_as_dict_doc},
    {"clear",
     (PyCFunction)BPy_IDPropertyUIManager_clear,
     METH_NOARGS,
     BPy_IDPropertyUIManager_clear_doc},
    {"update_from",
     (PyCFunction)BPy_IDPropertyUIManager_update_from,
     METH_VARARGS,
     BPy_IDPropertyUIManager_update_from_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyObject *BPy_IDPropertyUIManager_repr(BPy_IDPropertyUIManager *self)
{
  return PyUnicode_FromFormat(
      "<bpy id prop ui manager: name=\"%s\", address=%p>", self->property->name, self->property);
}

static Py_hash_t BPy_IDPropertyUIManager_hash(BPy_IDPropertyUIManager *self)
{
  return Py_HashPointer(self->property);
}

PyTypeObject BPy_IDPropertyUIManager_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /* For printing, in format `<module>.<name>`. */
    /*tp_name*/ "IDPropertyUIManager",
    /*tp_basicsize*/ sizeof(BPy_IDPropertyUIManager),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)BPy_IDPropertyUIManager_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ (hashfunc)BPy_IDPropertyUIManager_hash,
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
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_IDPropertyUIManager_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
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

void IDPropertyUIData_Init_Types()
{
  PyType_Ready(&BPy_IDPropertyUIManager_Type);
}

/** \} */
