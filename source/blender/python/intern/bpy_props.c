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
 * \ingroup pythonintern
 *
 * This file defines 'bpy.props' module used so scripts can define their own
 * rna properties for use with python operators or adding new properties to
 * existing blender types.
 */

/* Future-proof, See https://docs.python.org/3/c-api/arg.html#strings-and-buffers */
#define PY_SSIZE_T_CLEAN

#include <Python.h>

#include "RNA_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "bpy_capi_utils.h"
#include "bpy_props.h"
#include "bpy_rna.h"

#include "BKE_idprop.h"

#include "RNA_access.h"
#include "RNA_define.h" /* for defining our own rna */
#include "RNA_enum_types.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h" /* MAX_IDPROP_NAME */

#include "../generic/py_capi_utils.h"

/* -------------------------------------------------------------------- */
/** \name Shared Enums & Doc-Strings
 * \{ */

static const EnumPropertyItem property_flag_items[] = {
    {PROP_HIDDEN, "HIDDEN", 0, "Hidden", ""},
    {PROP_SKIP_SAVE, "SKIP_SAVE", 0, "Skip Save", ""},
    {PROP_ANIMATABLE, "ANIMATABLE", 0, "Animatable", ""},
    {PROP_LIB_EXCEPTION, "LIBRARY_EDITABLE", 0, "Library Editable", ""},
    {PROP_PROPORTIONAL, "PROPORTIONAL", 0, "Adjust values proportionally to eachother", ""},
    {PROP_TEXTEDIT_UPDATE,
     "TEXTEDIT_UPDATE",
     0,
     "Update on every keystroke in textedit 'mode'",
     ""},
    {0, NULL, 0, NULL, NULL},
};

#define BPY_PROPDEF_OPTIONS_DOC \
  "   :arg options: Enumerator in ['HIDDEN', 'SKIP_SAVE', 'ANIMATABLE', 'LIBRARY_EDITABLE', " \
  "'PROPORTIONAL'," \
  "'TEXTEDIT_UPDATE'].\n" \
  "   :type options: set\n"

static const EnumPropertyItem property_flag_enum_items[] = {
    {PROP_HIDDEN, "HIDDEN", 0, "Hidden", ""},
    {PROP_SKIP_SAVE, "SKIP_SAVE", 0, "Skip Save", ""},
    {PROP_ANIMATABLE, "ANIMATABLE", 0, "Animatable", ""},
    {PROP_LIB_EXCEPTION, "LIBRARY_EDITABLE", 0, "Library Editable", ""},
    {PROP_ENUM_FLAG, "ENUM_FLAG", 0, "Enum Flag", ""},
    {0, NULL, 0, NULL, NULL},
};

#define BPY_PROPDEF_OPTIONS_ENUM_DOC \
  "   :arg options: Enumerator in ['HIDDEN', 'SKIP_SAVE', 'ANIMATABLE', 'ENUM_FLAG', " \
  "'LIBRARY_EDITABLE'].\n" \
  "   :type options: set\n"

static const EnumPropertyItem property_flag_override_items[] = {
    {PROPOVERRIDE_OVERRIDABLE_LIBRARY,
     "LIBRARY_OVERRIDABLE",
     0,
     "Library Overridable",
     "Make that property editable in library overrides of linked data-blocks"},
    {0, NULL, 0, NULL, NULL},
};

#define BPY_PROPDEF_OPTIONS_OVERRIDE_DOC \
  "   :arg override: Enumerator in ['LIBRARY_OVERRIDABLE'].\n" \
  "   :type override: set\n"

static const EnumPropertyItem property_flag_override_collection_items[] = {
    {PROPOVERRIDE_OVERRIDABLE_LIBRARY,
     "LIBRARY_OVERRIDABLE",
     0,
     "Library Overridable",
     "Make that property editable in library overrides of linked data-blocks"},
    {PROPOVERRIDE_NO_PROP_NAME,
     "NO_PROPERTY_NAME",
     0,
     "No Name",
     "Do not use the names of the items, only their indices in the collection"},
    {PROPOVERRIDE_LIBRARY_INSERTION,
     "USE_INSERTION",
     0,
     "Use Insertion",
     "Allow users to add new items in that collection in library overrides"},
    {0, NULL, 0, NULL, NULL},
};

#define BPY_PROPDEF_OPTIONS_OVERRIDE_COLLECTION_DOC \
  "   :arg override: Enumerator in ['LIBRARY_OVERRIDABLE', 'NO_PROPERTY_NAME', " \
  "'USE_INSERTION'].\n" \
  "   :type override: set\n"

/* subtypes */
/* Keep in sync with RNA_types.h PropertySubType and rna_rna.c's rna_enum_property_subtype_items */
static const EnumPropertyItem property_subtype_string_items[] = {
    {PROP_FILEPATH, "FILE_PATH", 0, "File Path", ""},
    {PROP_DIRPATH, "DIR_PATH", 0, "Directory Path", ""},
    {PROP_FILENAME, "FILE_NAME", 0, "Filename", ""},
    {PROP_BYTESTRING, "BYTE_STRING", 0, "Byte String", ""},
    {PROP_PASSWORD, "PASSWORD", 0, "Password", "A string that is displayed hidden ('********')"},

    {PROP_NONE, "NONE", 0, "None", ""},
    {0, NULL, 0, NULL, NULL},
};

#define BPY_PROPDEF_SUBTYPE_STRING_DOC \
  "   :arg subtype: Enumerator in ['FILE_PATH', 'DIR_PATH', 'FILE_NAME', 'BYTE_STRING', " \
  "'PASSWORD', 'NONE'].\n" \
  "   :type subtype: string\n"

static const EnumPropertyItem property_subtype_number_items[] = {
    {PROP_PIXEL, "PIXEL", 0, "Pixel", ""},
    {PROP_UNSIGNED, "UNSIGNED", 0, "Unsigned", ""},
    {PROP_PERCENTAGE, "PERCENTAGE", 0, "Percentage", ""},
    {PROP_FACTOR, "FACTOR", 0, "Factor", ""},
    {PROP_ANGLE, "ANGLE", 0, "Angle", ""},
    {PROP_TIME, "TIME", 0, "Time", ""},
    {PROP_DISTANCE, "DISTANCE", 0, "Distance", ""},
    {PROP_DISTANCE_CAMERA, "DISTANCE_CAMERA", 0, "Camera Distance", ""},
    {PROP_POWER, "POWER", 0, "Power", ""},
    {PROP_TEMPERATURE, "TEMPERATURE", 0, "Temperature", ""},

    {PROP_NONE, "NONE", 0, "None", ""},
    {0, NULL, 0, NULL, NULL},
};

#define BPY_PROPDEF_SUBTYPE_NUMBER_DOC \
  "   :arg subtype: Enumerator in ['PIXEL', 'UNSIGNED', 'PERCENTAGE', 'FACTOR', 'ANGLE', " \
  "'TIME', 'DISTANCE', 'DISTANCE_CAMERA', 'POWER', 'TEMPERATURE', 'NONE'].\n" \
  "   :type subtype: string\n"

static const EnumPropertyItem property_subtype_array_items[] = {
    {PROP_COLOR, "COLOR", 0, "Color", ""},
    {PROP_TRANSLATION, "TRANSLATION", 0, "Translation", ""},
    {PROP_DIRECTION, "DIRECTION", 0, "Direction", ""},
    {PROP_VELOCITY, "VELOCITY", 0, "Velocity", ""},
    {PROP_ACCELERATION, "ACCELERATION", 0, "Acceleration", ""},
    {PROP_MATRIX, "MATRIX", 0, "Matrix", ""},
    {PROP_EULER, "EULER", 0, "Euler", ""},
    {PROP_QUATERNION, "QUATERNION", 0, "Quaternion", ""},
    {PROP_AXISANGLE, "AXISANGLE", 0, "Axis Angle", ""},
    {PROP_XYZ, "XYZ", 0, "XYZ", ""},
    {PROP_XYZ_LENGTH, "XYZ_LENGTH", 0, "XYZ Length", ""},
    {PROP_COLOR_GAMMA, "COLOR_GAMMA", 0, "Color Gamma", ""},
    {PROP_COORDS, "COORDINATES", 0, "Vector Coordinates", ""},
    {PROP_LAYER, "LAYER", 0, "Layer", ""},
    {PROP_LAYER_MEMBER, "LAYER_MEMBER", 0, "Layer Member", ""},

    {PROP_NONE, "NONE", 0, "None", ""},
    {0, NULL, 0, NULL, NULL},
};

#define BPY_PROPDEF_SUBTYPE_ARRAY_DOC \
  "   :arg subtype: Enumerator in ['COLOR', 'TRANSLATION', 'DIRECTION', " \
  "'VELOCITY', 'ACCELERATION', 'MATRIX', 'EULER', 'QUATERNION', 'AXISANGLE', " \
  "'XYZ', 'XYZ_LENGTH', 'COLOR_GAMMA', 'COORDINATES', 'LAYER', 'LAYER_MEMBER', 'NONE'].\n" \
  "   :type subtype: string\n"

/** \} */

/* -------------------------------------------------------------------- */
/** \name Python Property Storage API
 *
 * Functionality needed to use Python native callbacks from generic C RNA callbacks.
 * \{ */

/**
 * Store #PyObject data for a dynamically defined property.
 * Currently this is only used to store call-back functions.
 * Properties that don't use custom-callbacks wont allocate this struct.
 *
 * Memory/Reference Management
 * ---------------------------
 *
 * This struct adds/removes the user-count of each #PyObject it references,
 * it's needed in case the function is removed from the class (unlikely but possible),
 * also when an annotation evaluates to a `lambda` with Python 3.10 and newer e.g: T86332.
 *
 * Pointers to this struct are held in:
 *
 * - #PropertyRNA.py_data (owns the memory).
 *   Freed when the RNA property is freed.
 *
 * - #g_bpy_prop_store_list (borrows the memory)
 *   Having a global list means the users can be visited by the GC and cleared on exit.
 *
 *   This list can't be used for freeing as #BPyPropStore doesn't hold a #PropertyRNA back-pointer,
 *   (while it could be supported it would only complicate things).
 *
 *   All RNA properties are freed after Python has been shut-down.
 *   At that point Python user counts can't be touched and must have already been dealt with.
 *
 * Decrementing users is handled by:
 *
 * - #bpy_prop_py_data_remove manages decrementing at run-time (when a property is removed),
 *
 * - #BPY_rna_props_clear_all does this on exit for all dynamic properties.
 */
struct BPyPropStore {
  struct BPyPropStore *next, *prev;

  /**
   * Only store #PyObject types, so this member can be cast to an array and iterated over.
   * NULL members are skipped.
   */
  struct {
    /** Wrap: `RNA_def_property_*_funcs` (depending on type). */
    PyObject *get_fn;
    PyObject *set_fn;
    /** Wrap: #RNA_def_property_update_runtime */
    PyObject *update_fn;

    /** Arguments by type. */
    union {
      /** #PROP_ENUM type. */
      struct {
        /** Wrap: #RNA_def_property_enum_funcs_runtime */
        PyObject *itemf_fn;
      } enum_data;
      /** #PROP_POINTER type. */
      struct {
        /** Wrap: #RNA_def_property_poll_runtime */
        PyObject *poll_fn;
      } pointer_data;
    };
  } py_data;
};

#define BPY_PROP_STORE_PY_DATA_SIZE \
  (sizeof(((struct BPyPropStore *)NULL)->py_data) / sizeof(PyObject *))

#define ASSIGN_PYOBJECT_INCREF(a, b) \
  { \
    BLI_assert((a) == NULL); \
    Py_INCREF(b); \
    a = b; \
  } \
  ((void)0)

/**
 * Maintain a list of Python defined properties, so the GC can visit them,
 * and so they can be cleared on exit.
 */
static ListBase g_bpy_prop_store_list = {NULL, NULL};

static struct BPyPropStore *bpy_prop_py_data_ensure(struct PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  if (prop_store == NULL) {
    prop_store = MEM_callocN(sizeof(*prop_store), __func__);
    RNA_def_py_data(prop, prop_store);
    BLI_addtail(&g_bpy_prop_store_list, prop_store);
  }
  return prop_store;
}

/**
 * Perform all removal actions except for freeing, which is handled by RNA.
 */
static void bpy_prop_py_data_remove(PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  if (prop_store == NULL) {
    return;
  }

  PyObject **py_data = (PyObject **)&prop_store->py_data;
  for (int i = 0; i < BPY_PROP_STORE_PY_DATA_SIZE; i++) {
    Py_XDECREF(py_data[i]);
  }
  BLI_remlink(&g_bpy_prop_store_list, prop_store);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Property Type
 *
 * Operators and classes use this so it can store the arguments given but defer
 * running it until the operator runs where these values are used to setup
 * the default arguments for that operator instance.
 * \{ */

static void bpy_prop_deferred_dealloc(BPy_PropDeferred *self)
{
  PyObject_GC_UnTrack(self);
  Py_CLEAR(self->kw);
  PyObject_GC_Del(self);
}

static int bpy_prop_deferred_traverse(BPy_PropDeferred *self, visitproc visit, void *arg)
{
  Py_VISIT(self->kw);
  return 0;
}

static int bpy_prop_deferred_clear(BPy_PropDeferred *self)
{
  Py_CLEAR(self->kw);
  return 0;
}

static PyObject *bpy_prop_deferred_repr(BPy_PropDeferred *self)
{
  return PyUnicode_FromFormat("<%.200s, %R, %R>", Py_TYPE(self)->tp_name, self->fn, self->kw);
}

/**
 * HACK: needed by `typing.get_type_hints`
 * with `from __future__ import annotations` enabled or when using Python 3.10 or newer.
 *
 * When callable this object type passes the test for being an acceptable annotation.
 */
static PyObject *bpy_prop_deferred_call(BPy_PropDeferred *UNUSED(self),
                                        PyObject *UNUSED(args),
                                        PyObject *UNUSED(kw))
{
  /* Dummy value. */
  Py_RETURN_NONE;
}

/* Get/Set Items. */

/**
 * Expose the function in case scripts need to introspect this information
 * (not currently used by Blender it's self).
 */
static PyObject *bpy_prop_deferred_function_get(BPy_PropDeferred *self, void *UNUSED(closure))
{
  PyObject *ret = self->fn;
  Py_IncRef(ret);
  return ret;
}

/**
 * Expose keywords in case scripts need to introspect this information
 * (not currently used by Blender it's self).
 */
static PyObject *bpy_prop_deferred_keywords_get(BPy_PropDeferred *self, void *UNUSED(closure))
{
  PyObject *ret = self->kw;
  Py_IncRef(ret);
  return ret;
}

static PyGetSetDef bpy_prop_deferred_getset[] = {
    {"function", (getter)bpy_prop_deferred_function_get, (setter)NULL, NULL, NULL},
    {"keywords", (getter)bpy_prop_deferred_keywords_get, (setter)NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

PyDoc_STRVAR(bpy_prop_deferred_doc,
             "Intermediate storage for properties before registration.\n"
             "\n"
             ".. note::\n"
             "\n"
             "   This is not part of the stable API and may change between releases.");

PyTypeObject bpy_prop_deferred_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)

        .tp_name = "_PropertyDeferred",
    .tp_basicsize = sizeof(BPy_PropDeferred),
    .tp_dealloc = (destructor)bpy_prop_deferred_dealloc,
    .tp_repr = (reprfunc)bpy_prop_deferred_repr,
    .tp_call = (ternaryfunc)bpy_prop_deferred_call,

    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,

    .tp_doc = bpy_prop_deferred_doc,
    .tp_traverse = (traverseproc)bpy_prop_deferred_traverse,
    .tp_clear = (inquiry)bpy_prop_deferred_clear,

    .tp_getset = bpy_prop_deferred_getset,
};

static PyObject *bpy_prop_deferred_data_CreatePyObject(PyObject *fn, PyObject *kw)
{
  BPy_PropDeferred *self = PyObject_GC_New(BPy_PropDeferred, &bpy_prop_deferred_Type);
  self->fn = fn;
  if (kw == NULL) {
    kw = PyDict_New();
  }
  else {
    Py_INCREF(kw);
  }
  self->kw = kw;
  PyObject_GC_Track(self);
  return (PyObject *)self;
}

/** \} */

/* PyObject's */
static PyObject *pymeth_BoolProperty = NULL;
static PyObject *pymeth_BoolVectorProperty = NULL;
static PyObject *pymeth_IntProperty = NULL;
static PyObject *pymeth_IntVectorProperty = NULL;
static PyObject *pymeth_FloatProperty = NULL;
static PyObject *pymeth_FloatVectorProperty = NULL;
static PyObject *pymeth_StringProperty = NULL;
static PyObject *pymeth_EnumProperty = NULL;
static PyObject *pymeth_PointerProperty = NULL;
static PyObject *pymeth_CollectionProperty = NULL;
static PyObject *pymeth_RemoveProperty = NULL;

static PyObject *pyrna_struct_as_instance(PointerRNA *ptr)
{
  PyObject *self = NULL;
  /* first get self */
  /* operators can store their own instance for later use */
  if (ptr->data) {
    void **instance = RNA_struct_instance(ptr);

    if (instance) {
      if (*instance) {
        self = *instance;
        Py_INCREF(self);
      }
    }
  }

  /* in most cases this will run */
  if (self == NULL) {
    self = pyrna_struct_CreatePyObject(ptr);
  }

  return self;
}

static void bpy_prop_assign_flag(PropertyRNA *prop, const int flag)
{
  const int flag_mask = ((PROP_ANIMATABLE) & ~flag);

  if (flag) {
    RNA_def_property_flag(prop, flag);
  }

  if (flag_mask) {
    RNA_def_property_clear_flag(prop, flag_mask);
  }
}

static void bpy_prop_assign_flag_override(PropertyRNA *prop, const int flag_override)
{
  RNA_def_property_override_flag(prop, flag_override);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Property Callbacks
 *
 * Unique data is accessed via #RNA_property_py_data_get
 * \{ */

/* callbacks */
static void bpy_prop_update_fn(struct bContext *C,
                               struct PointerRNA *ptr,
                               struct PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyGILState_STATE gilstate;
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  const bool is_write_ok = pyrna_write_check();

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  bpy_context_set(C, &gilstate);

  py_func = prop_store->py_data.update_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  PyTuple_SET_ITEM(args, 1, (PyObject *)bpy_context_module);
  Py_INCREF(bpy_context_module);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  bpy_context_clear(C, &gilstate);

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boolean Property Callbacks
 * \{ */

static bool bpy_prop_boolean_get_fn(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  bool value;

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
    value = false;
  }
  else {
    const int value_i = PyC_Long_AsBool(ret);

    if (value_i == -1 && PyErr_Occurred()) {
      PyC_Err_PrintWithFunc(py_func);
      value = false;
    }
    else {
      value = (bool)value_i;
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }

  return value;
}

static void bpy_prop_boolean_set_fn(struct PointerRNA *ptr, struct PropertyRNA *prop, bool value)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.set_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  PyTuple_SET_ITEM(args, 1, PyBool_FromLong(value));

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

static void bpy_prop_boolean_array_get_fn(struct PointerRNA *ptr,
                                          struct PropertyRNA *prop,
                                          bool *values)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  int i, len = RNA_property_array_length(ptr, prop);

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);

    for (i = 0; i < len; i++) {
      values[i] = false;
    }
  }
  else {
    if (PyC_AsArray(values, ret, len, &PyBool_Type, false, "BoolVectorProperty get") == -1) {
      PyC_Err_PrintWithFunc(py_func);

      for (i = 0; i < len; i++) {
        values[i] = false;
      }

      /* PyC_AsArray decrements refcount internally on error */
    }
    else {
      Py_DECREF(ret);
    }
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

static void bpy_prop_boolean_array_set_fn(struct PointerRNA *ptr,
                                          struct PropertyRNA *prop,
                                          const bool *values)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyObject *py_values;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  const int len = RNA_property_array_length(ptr, prop);

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.set_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  py_values = PyC_Tuple_PackArray_Bool(values, len);
  PyTuple_SET_ITEM(args, 1, py_values);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Int Property Callbacks
 * \{ */

static int bpy_prop_int_get_fn(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  int value;

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
    value = 0.0f;
  }
  else {
    value = PyC_Long_AsI32(ret);

    if (value == -1 && PyErr_Occurred()) {
      PyC_Err_PrintWithFunc(py_func);
      value = 0;
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }

  return value;
}

static void bpy_prop_int_set_fn(struct PointerRNA *ptr, struct PropertyRNA *prop, int value)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.set_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  PyTuple_SET_ITEM(args, 1, PyLong_FromLong(value));

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

static void bpy_prop_int_array_get_fn(struct PointerRNA *ptr,
                                      struct PropertyRNA *prop,
                                      int *values)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  int i, len = RNA_property_array_length(ptr, prop);

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);

    for (i = 0; i < len; i++) {
      values[i] = 0;
    }
  }
  else {
    if (PyC_AsArray(values, ret, len, &PyLong_Type, false, "IntVectorProperty get") == -1) {
      PyC_Err_PrintWithFunc(py_func);

      for (i = 0; i < len; i++) {
        values[i] = 0;
      }

      /* PyC_AsArray decrements refcount internally on error */
    }
    else {
      Py_DECREF(ret);
    }
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

static void bpy_prop_int_array_set_fn(struct PointerRNA *ptr,
                                      struct PropertyRNA *prop,
                                      const int *values)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyObject *py_values;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  const int len = RNA_property_array_length(ptr, prop);

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.set_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  py_values = PyC_Tuple_PackArray_I32(values, len);
  PyTuple_SET_ITEM(args, 1, py_values);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Float Property Callbacks
 * \{ */

static float bpy_prop_float_get_fn(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  float value;

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
    value = 0.0f;
  }
  else {
    value = PyFloat_AsDouble(ret);

    if (value == -1.0f && PyErr_Occurred()) {
      PyC_Err_PrintWithFunc(py_func);
      value = 0.0f;
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }

  return value;
}

static void bpy_prop_float_set_fn(struct PointerRNA *ptr, struct PropertyRNA *prop, float value)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.set_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  PyTuple_SET_ITEM(args, 1, PyFloat_FromDouble(value));

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

static void bpy_prop_float_array_get_fn(struct PointerRNA *ptr,
                                        struct PropertyRNA *prop,
                                        float *values)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  int i, len = RNA_property_array_length(ptr, prop);

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);

    for (i = 0; i < len; i++) {
      values[i] = 0.0f;
    }
  }
  else {
    if (PyC_AsArray(values, ret, len, &PyFloat_Type, false, "FloatVectorProperty get") == -1) {
      PyC_Err_PrintWithFunc(py_func);

      for (i = 0; i < len; i++) {
        values[i] = 0.0f;
      }

      /* PyC_AsArray decrements refcount internally on error */
    }
    else {
      Py_DECREF(ret);
    }
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

static void bpy_prop_float_array_set_fn(struct PointerRNA *ptr,
                                        struct PropertyRNA *prop,
                                        const float *values)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyObject *py_values;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  const int len = RNA_property_array_length(ptr, prop);

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.set_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  py_values = PyC_Tuple_PackArray_F32(values, len);
  PyTuple_SET_ITEM(args, 1, py_values);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name String Property Callbacks
 * \{ */

static void bpy_prop_string_get_fn(struct PointerRNA *ptr, struct PropertyRNA *prop, char *value)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
    value[0] = '\0';
  }
  else if (!PyUnicode_Check(ret)) {
    PyErr_Format(
        PyExc_TypeError, "return value must be a string, not %.200s", Py_TYPE(ret)->tp_name);
    PyC_Err_PrintWithFunc(py_func);
    value[0] = '\0';
    Py_DECREF(ret);
  }
  else {
    Py_ssize_t length;
    const char *buffer = PyUnicode_AsUTF8AndSize(ret, &length);
    memcpy(value, buffer, length + 1);
    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

static int bpy_prop_string_length_fn(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  int length;

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
    length = 0;
  }
  else if (!PyUnicode_Check(ret)) {
    PyErr_Format(
        PyExc_TypeError, "return value must be a string, not %.200s", Py_TYPE(ret)->tp_name);
    PyC_Err_PrintWithFunc(py_func);
    length = 0;
    Py_DECREF(ret);
  }
  else {
    Py_ssize_t length_ssize_t = 0;
    PyUnicode_AsUTF8AndSize(ret, &length_ssize_t);
    length = length_ssize_t;
    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }

  return length;
}

static void bpy_prop_string_set_fn(struct PointerRNA *ptr,
                                   struct PropertyRNA *prop,
                                   const char *value)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  PyObject *py_value;

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.set_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  py_value = PyUnicode_FromString(value);
  if (!py_value) {
    PyErr_SetString(PyExc_ValueError, "the return value must be a string");
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    PyTuple_SET_ITEM(args, 1, py_value);
  }

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pointer Property Callbacks
 * \{ */

static bool bpy_prop_pointer_poll_fn(struct PointerRNA *self,
                                     PointerRNA candidate,
                                     struct PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_self;
  PyObject *py_candidate;
  PyObject *py_func;
  PyObject *args;
  PyObject *ret;
  bool result;
  const int is_write_ok = pyrna_write_check();
  const PyGILState_STATE gilstate = PyGILState_Ensure();

  BLI_assert(self != NULL);

  py_self = pyrna_struct_as_instance(self);
  py_candidate = pyrna_struct_as_instance(&candidate);
  py_func = prop_store->py_data.pointer_data.poll_fn;

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  args = PyTuple_New(2);
  PyTuple_SET_ITEM(args, 0, py_self);
  PyTuple_SET_ITEM(args, 1, py_candidate);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
    result = false;
  }
  else {
    result = PyObject_IsTrue(ret);
    Py_DECREF(ret);
  }

  PyGILState_Release(gilstate);
  if (!is_write_ok) {
    pyrna_write_set(false);
  }

  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enum Property Callbacks
 * \{ */

static int bpy_prop_enum_get_fn(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();
  int value;

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.get_fn;

  args = PyTuple_New(1);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
    value = RNA_property_enum_get_default(ptr, prop);
  }
  else {
    value = PyC_Long_AsI32(ret);

    if (value == -1 && PyErr_Occurred()) {
      PyC_Err_PrintWithFunc(py_func);
      value = RNA_property_enum_get_default(ptr, prop);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }

  return value;
}

static void bpy_prop_enum_set_fn(struct PointerRNA *ptr, struct PropertyRNA *prop, int value)
{
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func;
  PyObject *args;
  PyObject *self;
  PyObject *ret;
  PyGILState_STATE gilstate;
  bool use_gil;
  const bool is_write_ok = pyrna_write_check();

  BLI_assert(prop_store != NULL);

  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  py_func = prop_store->py_data.set_fn;

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  PyTuple_SET_ITEM(args, 1, PyLong_FromLong(value));

  ret = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (ret == NULL) {
    PyC_Err_PrintWithFunc(py_func);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(py_func);
    }

    Py_DECREF(ret);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
}

/* utility function we need for parsing int's in an if statement */
static bool py_long_as_int(PyObject *py_long, int *r_int)
{
  if (PyLong_CheckExact(py_long)) {
    *r_int = (int)PyLong_AS_LONG(py_long);
    return true;
  }

  return false;
}

#if 0
/* copies orig to buf, then sets orig to buf, returns copy length */
static size_t strswapbufcpy(char *buf, const char **orig)
{
  const char *src = *orig;
  char *dst = buf;
  size_t i = 0;
  *orig = buf;
  while ((*dst = *src)) {
    dst++;
    src++;
    i++;
  }
  return i + 1; /* include '\0' */
}
#endif

static int icon_id_from_name(const char *name)
{
  const EnumPropertyItem *item;
  int id;

  if (name[0]) {
    for (item = rna_enum_icon_items, id = 0; item->identifier; item++, id++) {
      if (STREQ(item->name, name)) {
        return item->value;
      }
    }
  }

  return 0;
}

static const EnumPropertyItem *enum_items_from_py(PyObject *seq_fast,
                                                  PyObject *def,
                                                  int *defvalue,
                                                  const bool is_enum_flag)
{
  EnumPropertyItem *items;
  PyObject *item;
  const Py_ssize_t seq_len = PySequence_Fast_GET_SIZE(seq_fast);
  PyObject **seq_fast_items = PySequence_Fast_ITEMS(seq_fast);
  Py_ssize_t totbuf = 0;
  int i;
  short def_used = 0;
  const char *def_string_cmp = NULL;
  int def_int_cmp = 0;

  if (is_enum_flag) {
    if (seq_len > RNA_ENUM_BITFLAG_SIZE) {
      PyErr_SetString(PyExc_TypeError,
                      "EnumProperty(...): maximum " STRINGIFY(
                          RNA_ENUM_BITFLAG_SIZE) " members for a ENUM_FLAG type property");
      return NULL;
    }
    if (def && !PySet_Check(def)) {
      PyErr_Format(PyExc_TypeError,
                   "EnumProperty(...): default option must be a 'set' "
                   "type when ENUM_FLAG is enabled, not a '%.200s'",
                   Py_TYPE(def)->tp_name);
      return NULL;
    }
  }
  else {
    if (def) {
      if (!py_long_as_int(def, &def_int_cmp)) {
        def_string_cmp = PyUnicode_AsUTF8(def);
        if (def_string_cmp == NULL) {
          PyErr_Format(PyExc_TypeError,
                       "EnumProperty(...): default option must be a 'str' or 'int' "
                       "type when ENUM_FLAG is disabled, not a '%.200s'",
                       Py_TYPE(def)->tp_name);
          return NULL;
        }
      }
    }
  }

  /* blank value */
  *defvalue = 0;

  items = MEM_callocN(sizeof(EnumPropertyItem) * (seq_len + 1), "enum_items_from_py1");

  for (i = 0; i < seq_len; i++) {
    EnumPropertyItem tmp = {0, "", 0, "", ""};
    const char *tmp_icon = NULL;
    Py_ssize_t item_size;
    Py_ssize_t id_str_size;
    Py_ssize_t name_str_size;
    Py_ssize_t desc_str_size;

    item = seq_fast_items[i];

    if ((PyTuple_CheckExact(item)) && (item_size = PyTuple_GET_SIZE(item)) &&
        (item_size >= 3 && item_size <= 5) &&
        (tmp.identifier = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(item, 0), &id_str_size)) &&
        (tmp.name = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(item, 1), &name_str_size)) &&
        (tmp.description = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(item, 2), &desc_str_size)) &&
        /* TODO, number isn't ensured to be unique from the script author */
        (item_size != 4 || py_long_as_int(PyTuple_GET_ITEM(item, 3), &tmp.value)) &&
        (item_size != 5 || ((py_long_as_int(PyTuple_GET_ITEM(item, 3), &tmp.icon) ||
                             (tmp_icon = PyUnicode_AsUTF8(PyTuple_GET_ITEM(item, 3)))) &&
                            py_long_as_int(PyTuple_GET_ITEM(item, 4), &tmp.value)))) {
      if (is_enum_flag) {
        if (item_size < 4) {
          tmp.value = 1 << i;
        }

        if (def && PySet_Contains(def, PyTuple_GET_ITEM(item, 0))) {
          *defvalue |= tmp.value;
          def_used++;
        }
      }
      else {
        if (item_size < 4) {
          tmp.value = i;
        }

        if (def && def_used == 0) {
          if ((def_string_cmp != NULL && STREQ(def_string_cmp, tmp.identifier)) ||
              (def_string_cmp == NULL && def_int_cmp == tmp.value)) {
            *defvalue = tmp.value;
            def_used++; /* only ever 1 */
          }
        }
      }

      if (tmp_icon) {
        tmp.icon = icon_id_from_name(tmp_icon);
      }

      items[i] = tmp;

      /* calculate combine string length */
      totbuf += id_str_size + name_str_size + desc_str_size + 3; /* 3 is for '\0's */
    }
    else if (item == Py_None) {
      /* Only set since the rest is cleared. */
      items[i].identifier = "";
    }
    else {
      MEM_freeN(items);
      PyErr_SetString(PyExc_TypeError,
                      "EnumProperty(...): expected a tuple containing "
                      "(identifier, name, description) and optionally an "
                      "icon name and unique number");
      return NULL;
    }
  }

  if (is_enum_flag) {
    /* strict check that all set members were used */
    if (def && def_used != PySet_GET_SIZE(def)) {
      MEM_freeN(items);

      PyErr_Format(PyExc_TypeError,
                   "EnumProperty(..., default={...}): set has %d unused member(s)",
                   PySet_GET_SIZE(def) - def_used);
      return NULL;
    }
  }
  else {
    if (def && def_used == 0) {
      MEM_freeN(items);

      if (def_string_cmp) {
        PyErr_Format(PyExc_TypeError,
                     "EnumProperty(..., default=\'%s\'): not found in enum members",
                     def_string_cmp);
      }
      else {
        PyErr_Format(PyExc_TypeError,
                     "EnumProperty(..., default=%d): not found in enum members",
                     def_int_cmp);
      }
      return NULL;
    }
  }

  /* disabled duplicating strings because the array can still be freed and
   * the strings from it referenced, for now we can't support dynamically
   * created strings from python. */
#if 0
  /* this would all work perfectly _but_ the python strings may be freed
   * immediately after use, so we need to duplicate them, ugh.
   * annoying because it works most of the time without this. */
  {
    EnumPropertyItem *items_dup = MEM_mallocN((sizeof(EnumPropertyItem) * (seq_len + 1)) +
                                                  (sizeof(char) * totbuf),
                                              "enum_items_from_py2");
    EnumPropertyItem *items_ptr = items_dup;
    char *buf = ((char *)items_dup) + (sizeof(EnumPropertyItem) * (seq_len + 1));
    memcpy(items_dup, items, sizeof(EnumPropertyItem) * (seq_len + 1));
    for (i = 0; i < seq_len; i++, items_ptr++) {
      buf += strswapbufcpy(buf, &items_ptr->identifier);
      buf += strswapbufcpy(buf, &items_ptr->name);
      buf += strswapbufcpy(buf, &items_ptr->description);
    }
    MEM_freeN(items);
    items = items_dup;
  }
  /* end string duplication */
#endif

  return items;
}

static const EnumPropertyItem *bpy_prop_enum_itemf_fn(struct bContext *C,
                                                      PointerRNA *ptr,
                                                      PropertyRNA *prop,
                                                      bool *r_free)
{
  PyGILState_STATE gilstate;
  struct BPyPropStore *prop_store = RNA_property_py_data_get(prop);
  PyObject *py_func = prop_store->py_data.enum_data.itemf_fn;
  PyObject *self = NULL;
  PyObject *args;
  PyObject *items; /* returned from the function call */

  const EnumPropertyItem *eitems = NULL;
  int err = 0;

  if (C) {
    bpy_context_set(C, &gilstate);
  }
  else {
    gilstate = PyGILState_Ensure();
  }

  args = PyTuple_New(2);
  self = pyrna_struct_as_instance(ptr);
  PyTuple_SET_ITEM(args, 0, self);

  /* now get the context */
  if (C) {
    PyTuple_SET_ITEM(args, 1, (PyObject *)bpy_context_module);
    Py_INCREF(bpy_context_module);
  }
  else {
    PyTuple_SET_ITEM(args, 1, Py_None);
    Py_INCREF(Py_None);
  }

  items = PyObject_CallObject(py_func, args);

  Py_DECREF(args);

  if (items == NULL) {
    err = -1;
  }
  else {
    PyObject *items_fast;
    int defvalue_dummy = 0;

    if (!(items_fast = PySequence_Fast(items,
                                       "EnumProperty(...): "
                                       "return value from the callback was not a sequence"))) {
      err = -1;
    }
    else {
      eitems = enum_items_from_py(
          items_fast, NULL, &defvalue_dummy, (RNA_property_flag(prop) & PROP_ENUM_FLAG) != 0);

      Py_DECREF(items_fast);

      if (!eitems) {
        err = -1;
      }
    }

    Py_DECREF(items);
  }

  if (err != -1) { /* worked */
    *r_free = true;
  }
  else {
    PyC_Err_PrintWithFunc(py_func);

    eitems = DummyRNA_NULL_items;
  }

  if (C) {
    bpy_context_clear(C, &gilstate);
  }
  else {
    PyGILState_Release(gilstate);
  }

  return eitems;
}

static int bpy_prop_callback_check(PyObject *py_func, const char *keyword, int argcount)
{
  if (py_func && py_func != Py_None) {
    if (!PyFunction_Check(py_func)) {
      PyErr_Format(PyExc_TypeError,
                   "%s keyword: expected a function type, not a %.200s",
                   keyword,
                   Py_TYPE(py_func)->tp_name);
      return -1;
    }

    PyCodeObject *f_code = (PyCodeObject *)PyFunction_GET_CODE(py_func);
    if (f_code->co_argcount != argcount) {
      PyErr_Format(PyExc_TypeError,
                   "%s keyword: expected a function taking %d arguments, not %d",
                   keyword,
                   argcount,
                   f_code->co_argcount);
      return -1;
    }
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Callback Assignment
 * \{ */

static void bpy_prop_callback_assign_update(struct PropertyRNA *prop, PyObject *update_fn)
{
  /* assume this is already checked for type and arg length */
  if (update_fn && update_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    RNA_def_property_update_runtime(prop, bpy_prop_update_fn);
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.update_fn, update_fn);

    RNA_def_property_flag(prop, PROP_CONTEXT_PROPERTY_UPDATE);
  }
}

static void bpy_prop_callback_assign_pointer(struct PropertyRNA *prop, PyObject *poll_fn)
{
  if (poll_fn && poll_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    RNA_def_property_poll_runtime(prop, bpy_prop_pointer_poll_fn);
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.pointer_data.poll_fn, poll_fn);
  }
}

static void bpy_prop_callback_assign_boolean(struct PropertyRNA *prop,
                                             PyObject *get_fn,
                                             PyObject *set_fn)
{
  BooleanPropertyGetFunc rna_get_fn = NULL;
  BooleanPropertySetFunc rna_set_fn = NULL;

  if (get_fn && get_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_get_fn = bpy_prop_boolean_get_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.get_fn, get_fn);
  }

  if (set_fn && set_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_set_fn = bpy_prop_boolean_set_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.set_fn, set_fn);
  }

  RNA_def_property_boolean_funcs_runtime(prop, rna_get_fn, rna_set_fn);
}

static void bpy_prop_callback_assign_boolean_array(struct PropertyRNA *prop,
                                                   PyObject *get_fn,
                                                   PyObject *set_fn)
{
  BooleanArrayPropertyGetFunc rna_get_fn = NULL;
  BooleanArrayPropertySetFunc rna_set_fn = NULL;

  if (get_fn && get_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_get_fn = bpy_prop_boolean_array_get_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.get_fn, get_fn);
  }

  if (set_fn && set_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_set_fn = bpy_prop_boolean_array_set_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.set_fn, set_fn);
  }

  RNA_def_property_boolean_array_funcs_runtime(prop, rna_get_fn, rna_set_fn);
}

static void bpy_prop_callback_assign_int(struct PropertyRNA *prop,
                                         PyObject *get_fn,
                                         PyObject *set_fn)
{
  IntPropertyGetFunc rna_get_fn = NULL;
  IntPropertySetFunc rna_set_fn = NULL;

  if (get_fn && get_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_get_fn = bpy_prop_int_get_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.get_fn, get_fn);
  }

  if (set_fn && set_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_set_fn = bpy_prop_int_set_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.set_fn, set_fn);
  }

  RNA_def_property_int_funcs_runtime(prop, rna_get_fn, rna_set_fn, NULL);
}

static void bpy_prop_callback_assign_int_array(struct PropertyRNA *prop,
                                               PyObject *get_fn,
                                               PyObject *set_fn)
{
  IntArrayPropertyGetFunc rna_get_fn = NULL;
  IntArrayPropertySetFunc rna_set_fn = NULL;

  if (get_fn && get_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_get_fn = bpy_prop_int_array_get_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.get_fn, get_fn);
  }

  if (set_fn && set_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_set_fn = bpy_prop_int_array_set_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.set_fn, set_fn);
  }

  RNA_def_property_int_array_funcs_runtime(prop, rna_get_fn, rna_set_fn, NULL);
}

static void bpy_prop_callback_assign_float(struct PropertyRNA *prop,
                                           PyObject *get_fn,
                                           PyObject *set_fn)
{
  FloatPropertyGetFunc rna_get_fn = NULL;
  FloatPropertySetFunc rna_set_fn = NULL;

  if (get_fn && get_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_get_fn = bpy_prop_float_get_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.get_fn, get_fn);
  }

  if (set_fn && set_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_set_fn = bpy_prop_float_set_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.set_fn, set_fn);
  }

  RNA_def_property_float_funcs_runtime(prop, rna_get_fn, rna_set_fn, NULL);
}

static void bpy_prop_callback_assign_float_array(struct PropertyRNA *prop,
                                                 PyObject *get_fn,
                                                 PyObject *set_fn)
{
  FloatArrayPropertyGetFunc rna_get_fn = NULL;
  FloatArrayPropertySetFunc rna_set_fn = NULL;

  if (get_fn && get_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_get_fn = bpy_prop_float_array_get_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.get_fn, get_fn);
  }

  if (set_fn && set_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_set_fn = bpy_prop_float_array_set_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.set_fn, set_fn);
  }

  RNA_def_property_float_array_funcs_runtime(prop, rna_get_fn, rna_set_fn, NULL);
}

static void bpy_prop_callback_assign_string(struct PropertyRNA *prop,
                                            PyObject *get_fn,
                                            PyObject *set_fn)
{
  StringPropertyGetFunc rna_get_fn = NULL;
  StringPropertyLengthFunc rna_length_fn = NULL;
  StringPropertySetFunc rna_set_fn = NULL;

  if (get_fn && get_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_get_fn = bpy_prop_string_get_fn;
    rna_length_fn = bpy_prop_string_length_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.get_fn, get_fn);
  }

  if (set_fn && set_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_set_fn = bpy_prop_string_set_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.set_fn, set_fn);
  }

  RNA_def_property_string_funcs_runtime(prop, rna_get_fn, rna_length_fn, rna_set_fn);
}

static void bpy_prop_callback_assign_enum(struct PropertyRNA *prop,
                                          PyObject *get_fn,
                                          PyObject *set_fn,
                                          PyObject *itemf_fn)
{
  EnumPropertyGetFunc rna_get_fn = NULL;
  EnumPropertyItemFunc rna_itemf_fn = NULL;
  EnumPropertySetFunc rna_set_fn = NULL;

  if (get_fn && get_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_get_fn = bpy_prop_enum_get_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.get_fn, get_fn);
  }

  if (set_fn && set_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);

    rna_set_fn = bpy_prop_enum_set_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.set_fn, set_fn);
  }

  if (itemf_fn && itemf_fn != Py_None) {
    struct BPyPropStore *prop_store = bpy_prop_py_data_ensure(prop);
    rna_itemf_fn = bpy_prop_enum_itemf_fn;
    ASSIGN_PYOBJECT_INCREF(prop_store->py_data.enum_data.itemf_fn, itemf_fn);
  }

  RNA_def_property_enum_funcs_runtime(prop, rna_get_fn, rna_set_fn, rna_itemf_fn);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Method Utilities
 * \{ */

/* this define runs at the start of each function and deals with
 * returning a deferred property (to be registered later) */
#define BPY_PROPDEF_HEAD(_func) \
  if (PyTuple_GET_SIZE(args) == 1) { \
    PyObject *ret; \
    self = PyTuple_GET_ITEM(args, 0); \
    args = PyTuple_New(0); \
    ret = BPy_##_func(self, args, kw); \
    Py_DECREF(args); \
    return ret; \
  } \
  if (PyTuple_GET_SIZE(args) > 1) { \
    PyErr_SetString(PyExc_ValueError, "all args must be keywords"); \
    return NULL; \
  } \
  srna = srna_from_self(self, #_func "(...):"); \
  if (srna == NULL) { \
    if (PyErr_Occurred()) { \
      return NULL; \
    } \
    return bpy_prop_deferred_data_CreatePyObject(pymeth_##_func, kw); \
  } \
  (void)0

/* terse macros for error checks shared between all funcs cant use function
 * calls because of static strings passed to pyrna_set_to_enum_bitfield */
#define BPY_PROPDEF_CHECK(_func, _property_flag_items, _property_flag_override_items) \
  if (UNLIKELY(id_len >= MAX_IDPROP_NAME)) { \
    PyErr_Format(PyExc_TypeError, \
                 #_func "(): '%.200s' too long, max length is %d", \
                 id, \
                 MAX_IDPROP_NAME - 1); \
    return NULL; \
  } \
  if (UNLIKELY(RNA_def_property_free_identifier(srna, id) == -1)) { \
    PyErr_Format(PyExc_TypeError, #_func "(): '%s' is defined as a non-dynamic type", id); \
    return NULL; \
  } \
  if (UNLIKELY(pyopts && pyrna_set_to_enum_bitfield( \
                             _property_flag_items, pyopts, &opts, #_func "(options={ ...}):"))) { \
    return NULL; \
  } \
  if (UNLIKELY(pyopts_override && pyrna_set_to_enum_bitfield(_property_flag_override_items, \
                                                             pyopts_override, \
                                                             &opts_override, \
                                                             #_func "(override={ ...}):"))) { \
    return NULL; \
  } \
  { \
    const EnumPropertyItem *tag_defines = RNA_struct_property_tag_defines(srna); \
    if (py_tags && !tag_defines) { \
      PyErr_Format(PyExc_TypeError, \
                   #_func "(): property-tags not available for '%s'", \
                   RNA_struct_identifier(srna)); \
      return NULL; \
    } \
    if (UNLIKELY(py_tags && pyrna_set_to_enum_bitfield( \
                                tag_defines, py_tags, &prop_tags, #_func "(tags={ ...}):"))) { \
      return NULL; \
    } \
  } \
  (void)0

#define BPY_PROPDEF_SUBTYPE_CHECK( \
    _func, _property_flag_items, _property_flag_override_items, _subtype) \
  BPY_PROPDEF_CHECK(_func, _property_flag_items, _property_flag_override_items); \
  if (UNLIKELY(pysubtype && RNA_enum_value_from_id(_subtype, pysubtype, &subtype) == 0)) { \
    const char *enum_str = BPy_enum_as_string(_subtype); \
    PyErr_Format(PyExc_TypeError, \
                 #_func \
                 "(subtype='%s'): " \
                 "subtype not found in (%s)", \
                 pysubtype, \
                 enum_str); \
    MEM_freeN((void *)enum_str); \
    return NULL; \
  } \
  (void)0

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Method Doc-Strings
 * \{ */

#define BPY_PROPDEF_NAME_DOC \
  "   :arg name: Name used in the user interface.\n" \
  "   :type name: string\n"

#define BPY_PROPDEF_DESC_DOC \
  "   :arg description: Text used for the tooltip and api documentation.\n" \
  "   :type description: string\n"

#define BPY_PROPDEF_UNIT_DOC \
  "   :arg unit: Enumerator in ['NONE', 'LENGTH', 'AREA', 'VOLUME', 'ROTATION', 'TIME', " \
  "'VELOCITY', 'ACCELERATION', 'MASS', 'CAMERA', 'POWER'].\n" \
  "   :type unit: string\n"

#define BPY_PROPDEF_NUM_MIN_DOC \
  "   :arg min: Hard minimum, trying to assign a value below will silently assign this minimum " \
  "instead.\n"

#define BPY_PROPDEF_NUM_MAX_DOC \
  "   :arg max: Hard maximum, trying to assign a value above will silently assign this maximum " \
  "instead.\n"

#define BPY_PROPDEF_NUM_SOFTMIN_DOC \
  "   :arg soft_min: Soft minimum (>= *min*), user won't be able to drag the widget below this " \
  "value in the UI.\n"

#define BPY_PROPDEF_NUM_SOFTMAX_DOC \
  "   :arg soft_max: Soft maximum (<= *max*), user won't be able to drag the widget above this " \
  "value in the UI.\n"

#define BPY_PROPDEF_VECSIZE_DOC \
  "   :arg size: Vector dimensions in [1, " STRINGIFY(PYRNA_STACK_ARRAY) "].\n" \
"   :type size: int\n"

#define BPY_PROPDEF_INT_STEP_DOC \
  "   :arg step: Step of increment/decrement in UI, in [1, 100], defaults to 1 (WARNING: unused " \
  "currently!).\n" \
  "   :type step: int\n"

#define BPY_PROPDEF_FLOAT_STEP_DOC \
  "   :arg step: Step of increment/decrement in UI, in [1, 100], defaults to 3 (WARNING: actual " \
  "value is /100).\n" \
  "   :type step: int\n"

#define BPY_PROPDEF_FLOAT_PREC_DOC \
  "   :arg precision: Maximum number of decimal digits to display, in [0, 6].\n" \
  "   :type precision: int\n"

#define BPY_PROPDEF_UPDATE_DOC \
  "   :arg update: Function to be called when this value is modified,\n" \
  "      This function must take 2 values (self, context) and return None.\n" \
  "      *Warning* there are no safety checks to avoid infinite recursion.\n" \
  "   :type update: function\n"

#define BPY_PROPDEF_POLL_DOC \
  "   :arg poll: function to be called to determine whether an item is valid for this " \
  "property.\n" \
  "              The function must take 2 values (self, object) and return Bool.\n" \
  "   :type poll: function\n"

#define BPY_PROPDEF_GET_DOC \
  "   :arg get: Function to be called when this value is 'read',\n" \
  "      This function must take 1 value (self) and return the value of the property.\n" \
  "   :type get: function\n"

#define BPY_PROPDEF_SET_DOC \
  "   :arg set: Function to be called when this value is 'written',\n" \
  "      This function must take 2 values (self, value) and return None.\n" \
  "   :type set: function\n"

#define BPY_PROPDEF_TYPE_DOC \
  "   :arg type: A subclass of :class:`bpy.types.PropertyGroup` or :class:`bpy.types.ID`.\n" \
  "   :type type: class\n"

#define BPY_PROPDEF_TAGS_DOC \
  "   :arg tags: Enumerator of tags that are defined by parent class.\n" \
  "   :type tags: set\n"

#if 0
static int bpy_struct_id_used(StructRNA *srna, char *identifier)
{
  PointerRNA ptr;
  RNA_pointer_create(NULL, srna, NULL, &ptr);
  return (RNA_struct_find_property(&ptr, identifier) != NULL);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module Methods
 * \{ */

/* Function that sets RNA, NOTE - self is NULL when called from python,
 * but being abused from C so we can pass the srna along.
 * This isn't incorrect since its a python object - but be careful */
PyDoc_STRVAR(BPy_BoolProperty_doc,
             ".. function:: BoolProperty(name=\"\", "
             "description=\"\", "
             "default=False, "
             "options={'ANIMATABLE'}, "
             "override=set(), "
             "tags=set(), "
             "subtype='NONE', "
             "update=None, "
             "get=None, "
             "set=None)\n"
             "\n"
             "   Returns a new boolean property definition.\n"
             "\n" BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC BPY_PROPDEF_OPTIONS_DOC
                 BPY_PROPDEF_OPTIONS_OVERRIDE_DOC BPY_PROPDEF_TAGS_DOC
                     BPY_PROPDEF_SUBTYPE_NUMBER_DOC BPY_PROPDEF_UPDATE_DOC BPY_PROPDEF_GET_DOC
                         BPY_PROPDEF_SET_DOC);
static PyObject *BPy_BoolProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(BoolProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "";
    Py_ssize_t id_len;
    bool def = false;
    PropertyRNA *prop;
    PyObject *pyopts = NULL;
    PyObject *pyopts_override = NULL;
    int opts = 0;
    int opts_override = 0;
    int prop_tags = 0;
    const char *pysubtype = NULL;
    int subtype = PROP_NONE;
    PyObject *update_fn = NULL;
    PyObject *get_fn = NULL;
    PyObject *set_fn = NULL;
    PyObject *py_tags = NULL;

    static const char *_keywords[] = {
        "attr",
        "name",
        "description",
        "default",
        "options",
        "override",
        "tags",
        "subtype",
        "update",
        "get",
        "set",
        NULL,
    };
    static _PyArg_Parser _parser = {"s#|ssO&O!O!O!sOOO:BoolProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &name,
                                          &description,
                                          PyC_ParseBool,
                                          &def,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &pysubtype,
                                          &update_fn,
                                          &get_fn,
                                          &set_fn)) {
      return NULL;
    }

    BPY_PROPDEF_SUBTYPE_CHECK(BoolProperty,
                              property_flag_items,
                              property_flag_override_items,
                              property_subtype_number_items);

    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(get_fn, "get", 1) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(set_fn, "set", 2) == -1) {
      return NULL;
    }

    prop = RNA_def_property(srna, id, PROP_BOOLEAN, subtype);
    RNA_def_property_boolean_default(prop, def);
    RNA_def_property_ui_text(prop, name ? name : id, description);

    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_boolean(prop, get_fn, set_fn);
    RNA_def_property_duplicate_pointers(srna, prop);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    BPy_BoolVectorProperty_doc,
    ".. function:: BoolVectorProperty(name=\"\", "
    "description=\"\", "
    "default=(False, False, False), "
    "options={'ANIMATABLE'}, "
    "override=set(), "
    "tags=set(), "
    "subtype='NONE', "
    "size=3, "
    "update=None, "
    "get=None, "
    "set=None)\n"
    "\n"
    "   Returns a new vector boolean property definition.\n"
    "\n" BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC
    "   :arg default: sequence of booleans the length of *size*.\n"
    "   :type default: sequence\n" BPY_PROPDEF_OPTIONS_DOC BPY_PROPDEF_OPTIONS_OVERRIDE_DOC
        BPY_PROPDEF_TAGS_DOC BPY_PROPDEF_SUBTYPE_ARRAY_DOC BPY_PROPDEF_VECSIZE_DOC
            BPY_PROPDEF_UPDATE_DOC BPY_PROPDEF_GET_DOC BPY_PROPDEF_SET_DOC);
static PyObject *BPy_BoolVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(BoolVectorProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "";
    Py_ssize_t id_len;
    bool def[PYRNA_STACK_ARRAY] = {0};
    int size = 3;
    PropertyRNA *prop;
    PyObject *pydef = NULL;
    PyObject *pyopts = NULL;
    PyObject *pyopts_override = NULL;
    int opts = 0;
    int opts_override = 0;
    int prop_tags = 0;
    const char *pysubtype = NULL;
    int subtype = PROP_NONE;
    PyObject *update_fn = NULL;
    PyObject *get_fn = NULL;
    PyObject *set_fn = NULL;
    PyObject *py_tags = NULL;

    static const char *_keywords[] = {
        "attr",
        "name",
        "description",
        "default",
        "options",
        "override",
        "tags",
        "subtype",
        "size",
        "update",
        "get",
        "set",
        NULL,
    };
    static _PyArg_Parser _parser = {"s#|ssOO!O!O!siOOO:BoolVectorProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &name,
                                          &description,
                                          &pydef,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &pysubtype,
                                          &size,
                                          &update_fn,
                                          &get_fn,
                                          &set_fn)) {
      return NULL;
    }

    BPY_PROPDEF_SUBTYPE_CHECK(BoolVectorProperty,
                              property_flag_items,
                              property_flag_override_items,
                              property_subtype_array_items);

    if (size < 1 || size > PYRNA_STACK_ARRAY) {
      PyErr_Format(
          PyExc_TypeError,
          "BoolVectorProperty(size=%d): size must be between 0 and " STRINGIFY(PYRNA_STACK_ARRAY),
          size);
      return NULL;
    }

    if (pydef &&
        PyC_AsArray(
            def, pydef, size, &PyBool_Type, false, "BoolVectorProperty(default=sequence)") == -1) {
      return NULL;
    }

    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(get_fn, "get", 1) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(set_fn, "set", 2) == -1) {
      return NULL;
    }

#if 0
    prop = RNA_def_boolean_array(
        srna, id, size, pydef ? def : NULL, name ? name : id, description);
#endif
    prop = RNA_def_property(srna, id, PROP_BOOLEAN, subtype);
    RNA_def_property_array(prop, size);
    if (pydef) {
      RNA_def_property_boolean_array_default(prop, def);
    }
    RNA_def_property_ui_text(prop, name ? name : id, description);

    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_boolean_array(prop, get_fn, set_fn);
    RNA_def_property_duplicate_pointers(srna, prop);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    BPy_IntProperty_doc,
    ".. function:: IntProperty(name=\"\", "
    "description=\"\", "
    "default=0, "
    "min=-2**31, max=2**31-1, "
    "soft_min=-2**31, soft_max=2**31-1, "
    "step=1, "
    "options={'ANIMATABLE'}, "
    "override=set(), "
    "tags=set(), "
    "subtype='NONE', "
    "update=None, "
    "get=None, "
    "set=None)\n"
    "\n"
    "   Returns a new int property definition.\n"
    "\n" BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC BPY_PROPDEF_NUM_MIN_DOC
    "   :type min: int\n" BPY_PROPDEF_NUM_MAX_DOC "   :type max: int\n" BPY_PROPDEF_NUM_SOFTMAX_DOC
    "   :type soft_min: int\n" BPY_PROPDEF_NUM_SOFTMIN_DOC
    "   :type soft_max: int\n" BPY_PROPDEF_INT_STEP_DOC BPY_PROPDEF_OPTIONS_DOC
        BPY_PROPDEF_OPTIONS_OVERRIDE_DOC BPY_PROPDEF_TAGS_DOC BPY_PROPDEF_SUBTYPE_NUMBER_DOC
            BPY_PROPDEF_UPDATE_DOC BPY_PROPDEF_GET_DOC BPY_PROPDEF_SET_DOC);
static PyObject *BPy_IntProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(IntProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "";
    Py_ssize_t id_len;
    int min = INT_MIN, max = INT_MAX, soft_min = INT_MIN, soft_max = INT_MAX, step = 1, def = 0;
    PropertyRNA *prop;
    PyObject *pyopts = NULL;
    int opts = 0;
    PyObject *pyopts_override = NULL;
    int opts_override = 0;
    int prop_tags = 0;
    const char *pysubtype = NULL;
    int subtype = PROP_NONE;
    PyObject *update_fn = NULL;
    PyObject *get_fn = NULL;
    PyObject *set_fn = NULL;
    PyObject *py_tags = NULL;

    static const char *_keywords[] = {
        "attr",
        "name",
        "description",
        "default",
        "min",
        "max",
        "soft_min",
        "soft_max",
        "step",
        "options",
        "override",
        "tags",
        "subtype",
        "update",
        "get",
        "set",
        NULL,
    };
    static _PyArg_Parser _parser = {"s#|ssiiiiiiO!O!O!sOOO:IntProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &name,
                                          &description,
                                          &def,
                                          &min,
                                          &max,
                                          &soft_min,
                                          &soft_max,
                                          &step,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &pysubtype,
                                          &update_fn,
                                          &get_fn,
                                          &set_fn)) {
      return NULL;
    }

    BPY_PROPDEF_SUBTYPE_CHECK(IntProperty,
                              property_flag_items,
                              property_flag_override_items,
                              property_subtype_number_items);

    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(get_fn, "get", 1) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(set_fn, "set", 2) == -1) {
      return NULL;
    }

    prop = RNA_def_property(srna, id, PROP_INT, subtype);
    RNA_def_property_int_default(prop, def);
    RNA_def_property_ui_text(prop, name ? name : id, description);
    RNA_def_property_range(prop, min, max);
    RNA_def_property_ui_range(prop, MAX2(soft_min, min), MIN2(soft_max, max), step, 3);

    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_int(prop, get_fn, set_fn);
    RNA_def_property_duplicate_pointers(srna, prop);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_IntVectorProperty_doc,
             ".. function:: IntVectorProperty(name=\"\", "
             "description=\"\", "
             "default=(0, 0, 0), min=-2**31, max=2**31-1, "
             "soft_min=-2**31, "
             "soft_max=2**31-1, "
             "step=1, "
             "options={'ANIMATABLE'}, "
             "override=set(), "
             "tags=set(), "
             "subtype='NONE', "
             "size=3, "
             "update=None, "
             "get=None, "
             "set=None)\n"
             "\n"
             "   Returns a new vector int property definition.\n"
             "\n" BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC
             "   :arg default: sequence of ints the length of *size*.\n"
             "   :type default: sequence\n" BPY_PROPDEF_NUM_MIN_DOC
             "   :type min: int\n" BPY_PROPDEF_NUM_MAX_DOC
             "   :type max: int\n" BPY_PROPDEF_NUM_SOFTMIN_DOC
             "   :type soft_min: int\n" BPY_PROPDEF_NUM_SOFTMAX_DOC
             "   :type soft_max: int\n" BPY_PROPDEF_INT_STEP_DOC BPY_PROPDEF_OPTIONS_DOC
                 BPY_PROPDEF_OPTIONS_OVERRIDE_DOC BPY_PROPDEF_TAGS_DOC
                     BPY_PROPDEF_SUBTYPE_ARRAY_DOC BPY_PROPDEF_VECSIZE_DOC BPY_PROPDEF_UPDATE_DOC
                         BPY_PROPDEF_GET_DOC BPY_PROPDEF_SET_DOC);
static PyObject *BPy_IntVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(IntVectorProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "";
    Py_ssize_t id_len;
    int min = INT_MIN, max = INT_MAX, soft_min = INT_MIN, soft_max = INT_MAX, step = 1;
    int def[PYRNA_STACK_ARRAY] = {0};
    int size = 3;
    PropertyRNA *prop;
    PyObject *pydef = NULL;
    PyObject *pyopts = NULL;
    int opts = 0;
    PyObject *pyopts_override = NULL;
    int opts_override = 0;
    int prop_tags = 0;
    const char *pysubtype = NULL;
    int subtype = PROP_NONE;
    PyObject *update_fn = NULL;
    PyObject *get_fn = NULL;
    PyObject *set_fn = NULL;
    PyObject *py_tags = NULL;

    static const char *_keywords[] = {
        "attr",
        "name",
        "description",
        "default",
        "min",
        "max",
        "soft_min",
        "soft_max",
        "step",
        "options",
        "override",
        "tags",
        "subtype",
        "size",
        "update",
        "get",
        "set",
        NULL,
    };
    static _PyArg_Parser _parser = {"s#|ssOiiiiiO!O!O!siOOO:IntVectorProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &name,
                                          &description,
                                          &pydef,
                                          &min,
                                          &max,
                                          &soft_min,
                                          &soft_max,
                                          &step,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &pysubtype,
                                          &size,
                                          &update_fn,
                                          &get_fn,
                                          &set_fn)) {
      return NULL;
    }

    BPY_PROPDEF_SUBTYPE_CHECK(IntVectorProperty,
                              property_flag_items,
                              property_flag_override_items,
                              property_subtype_array_items);

    if (size < 1 || size > PYRNA_STACK_ARRAY) {
      PyErr_Format(
          PyExc_TypeError,
          "IntVectorProperty(size=%d): size must be between 0 and " STRINGIFY(PYRNA_STACK_ARRAY),
          size);
      return NULL;
    }

    if (pydef &&
        PyC_AsArray(
            def, pydef, size, &PyLong_Type, false, "IntVectorProperty(default=sequence)") == -1) {
      return NULL;
    }

    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(get_fn, "get", 1) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(set_fn, "set", 2) == -1) {
      return NULL;
    }

    prop = RNA_def_property(srna, id, PROP_INT, subtype);
    RNA_def_property_array(prop, size);
    if (pydef) {
      RNA_def_property_int_array_default(prop, def);
    }
    RNA_def_property_range(prop, min, max);
    RNA_def_property_ui_text(prop, name ? name : id, description);
    RNA_def_property_ui_range(prop, MAX2(soft_min, min), MIN2(soft_max, max), step, 3);

    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_int_array(prop, get_fn, set_fn);
    RNA_def_property_duplicate_pointers(srna, prop);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_FloatProperty_doc,
             ".. function:: FloatProperty(name=\"\", "
             "description=\"\", "
             "default=0.0, "
             "min=-3.402823e+38, max=3.402823e+38, "
             "soft_min=-3.402823e+38, soft_max=3.402823e+38, "
             "step=3, "
             "precision=2, "
             "options={'ANIMATABLE'}, "
             "override=set(), "
             "tags=set(), "
             "subtype='NONE', "
             "unit='NONE', "
             "update=None, "
             "get=None, "
             "set=None)\n"
             "\n"
             "   Returns a new float (single precision) property definition.\n"
             "\n" BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC BPY_PROPDEF_NUM_MIN_DOC
             "   :type min: float\n" BPY_PROPDEF_NUM_MAX_DOC
             "   :type max: float\n" BPY_PROPDEF_NUM_SOFTMIN_DOC
             "   :type soft_min: float\n" BPY_PROPDEF_NUM_SOFTMAX_DOC
             "   :type soft_max: float\n" BPY_PROPDEF_FLOAT_STEP_DOC BPY_PROPDEF_FLOAT_PREC_DOC
                 BPY_PROPDEF_OPTIONS_DOC BPY_PROPDEF_OPTIONS_OVERRIDE_DOC BPY_PROPDEF_TAGS_DOC
                     BPY_PROPDEF_SUBTYPE_NUMBER_DOC BPY_PROPDEF_UNIT_DOC BPY_PROPDEF_UPDATE_DOC
                         BPY_PROPDEF_GET_DOC BPY_PROPDEF_SET_DOC);
static PyObject *BPy_FloatProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(FloatProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "";
    Py_ssize_t id_len;
    float min = -FLT_MAX, max = FLT_MAX, soft_min = -FLT_MAX, soft_max = FLT_MAX, step = 3,
          def = 0.0f;
    int precision = 2;
    PropertyRNA *prop;
    PyObject *pyopts = NULL;
    int opts = 0;
    PyObject *pyopts_override = NULL;
    int opts_override = 0;
    int prop_tags = 0;
    const char *pysubtype = NULL;
    int subtype = PROP_NONE;
    const char *pyunit = NULL;
    int unit = PROP_UNIT_NONE;
    PyObject *update_fn = NULL;
    PyObject *get_fn = NULL;
    PyObject *set_fn = NULL;
    PyObject *py_tags = NULL;

    static const char *_keywords[] = {
        "attr",     "name",   "description", "default", "min",      "max",  "soft_min",
        "soft_max", "step",   "precision",   "options", "override", "tags", "subtype",
        "unit",     "update", "get",         "set",     NULL,
    };
    static _PyArg_Parser _parser = {"s#|ssffffffiO!O!O!ssOOO:FloatProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &name,
                                          &description,
                                          &def,
                                          &min,
                                          &max,
                                          &soft_min,
                                          &soft_max,
                                          &step,
                                          &precision,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &pysubtype,
                                          &pyunit,
                                          &update_fn,
                                          &get_fn,
                                          &set_fn)) {
      return NULL;
    }

    BPY_PROPDEF_SUBTYPE_CHECK(FloatProperty,
                              property_flag_items,
                              property_flag_override_items,
                              property_subtype_number_items);

    if (pyunit && RNA_enum_value_from_id(rna_enum_property_unit_items, pyunit, &unit) == 0) {
      PyErr_Format(PyExc_TypeError, "FloatProperty(unit='%s'): invalid unit", pyunit);
      return NULL;
    }

    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(get_fn, "get", 1) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(set_fn, "set", 2) == -1) {
      return NULL;
    }

    prop = RNA_def_property(srna, id, PROP_FLOAT, subtype | unit);
    RNA_def_property_float_default(prop, def);
    RNA_def_property_range(prop, min, max);
    RNA_def_property_ui_text(prop, name ? name : id, description);
    RNA_def_property_ui_range(prop, MAX2(soft_min, min), MIN2(soft_max, max), step, precision);

    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_float(prop, get_fn, set_fn);
    RNA_def_property_duplicate_pointers(srna, prop);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_FloatVectorProperty_doc,
             ".. function:: FloatVectorProperty(name=\"\", "
             "description=\"\", "
             "default=(0.0, 0.0, 0.0), "
             "min=sys.float_info.min, max=sys.float_info.max, "
             "soft_min=sys.float_info.min, soft_max=sys.float_info.max, "
             "step=3, "
             "precision=2, "
             "options={'ANIMATABLE'}, "
             "override=set(), "
             "tags=set(), "
             "subtype='NONE', "
             "unit='NONE', "
             "size=3, "
             "update=None, "
             "get=None, "
             "set=None)\n"
             "\n"
             "   Returns a new vector float property definition.\n"
             "\n" BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC
             "   :arg default: sequence of floats the length of *size*.\n"
             "   :type default: sequence\n" BPY_PROPDEF_NUM_MIN_DOC
             "   :type min: float\n" BPY_PROPDEF_NUM_MAX_DOC
             "   :type max: float\n" BPY_PROPDEF_NUM_SOFTMIN_DOC
             "   :type soft_min: float\n" BPY_PROPDEF_NUM_SOFTMAX_DOC
             "   :type soft_max: float\n" BPY_PROPDEF_OPTIONS_DOC BPY_PROPDEF_OPTIONS_OVERRIDE_DOC
                 BPY_PROPDEF_TAGS_DOC BPY_PROPDEF_FLOAT_STEP_DOC BPY_PROPDEF_FLOAT_PREC_DOC
                     BPY_PROPDEF_SUBTYPE_ARRAY_DOC BPY_PROPDEF_UNIT_DOC BPY_PROPDEF_VECSIZE_DOC
                         BPY_PROPDEF_UPDATE_DOC BPY_PROPDEF_GET_DOC BPY_PROPDEF_SET_DOC);
static PyObject *BPy_FloatVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(FloatVectorProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "";
    Py_ssize_t id_len;
    float min = -FLT_MAX, max = FLT_MAX, soft_min = -FLT_MAX, soft_max = FLT_MAX, step = 3;
    float def[PYRNA_STACK_ARRAY] = {0.0f};
    int precision = 2, size = 3;
    PropertyRNA *prop;
    PyObject *pydef = NULL;
    PyObject *pyopts = NULL;
    int opts = 0;
    PyObject *pyopts_override = NULL;
    int opts_override = 0;
    int prop_tags = 0;
    const char *pysubtype = NULL;
    int subtype = PROP_NONE;
    const char *pyunit = NULL;
    int unit = PROP_UNIT_NONE;
    PyObject *update_fn = NULL;
    PyObject *get_fn = NULL;
    PyObject *set_fn = NULL;
    PyObject *py_tags = NULL;

    static const char *_keywords[] = {
        "attr",     "name", "description", "default", "min",      "max",  "soft_min",
        "soft_max", "step", "precision",   "options", "override", "tags", "subtype",
        "unit",     "size", "update",      "get",     "set",      NULL,
    };
    static _PyArg_Parser _parser = {"s#|ssOfffffiO!O!O!ssiOOO:FloatVectorProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &name,
                                          &description,
                                          &pydef,
                                          &min,
                                          &max,
                                          &soft_min,
                                          &soft_max,
                                          &step,
                                          &precision,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &pysubtype,
                                          &pyunit,
                                          &size,
                                          &update_fn,
                                          &get_fn,
                                          &set_fn)) {
      return NULL;
    }

    BPY_PROPDEF_SUBTYPE_CHECK(FloatVectorProperty,
                              property_flag_items,
                              property_flag_override_items,
                              property_subtype_array_items);

    if (pyunit && RNA_enum_value_from_id(rna_enum_property_unit_items, pyunit, &unit) == 0) {
      PyErr_Format(PyExc_TypeError, "FloatVectorProperty(unit='%s'): invalid unit", pyunit);
      return NULL;
    }

    if (size < 1 || size > PYRNA_STACK_ARRAY) {
      PyErr_Format(
          PyExc_TypeError,
          "FloatVectorProperty(size=%d): size must be between 0 and " STRINGIFY(PYRNA_STACK_ARRAY),
          size);
      return NULL;
    }

    if (pydef &&
        PyC_AsArray(
            def, pydef, size, &PyFloat_Type, false, "FloatVectorProperty(default=sequence)") ==
            -1) {
      return NULL;
    }

    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(get_fn, "get", 1) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(set_fn, "set", 2) == -1) {
      return NULL;
    }

    prop = RNA_def_property(srna, id, PROP_FLOAT, subtype | unit);
    RNA_def_property_array(prop, size);
    if (pydef) {
      RNA_def_property_float_array_default(prop, def);
    }
    RNA_def_property_range(prop, min, max);
    RNA_def_property_ui_text(prop, name ? name : id, description);
    RNA_def_property_ui_range(prop, MAX2(soft_min, min), MIN2(soft_max, max), step, precision);

    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_float_array(prop, get_fn, set_fn);
    RNA_def_property_duplicate_pointers(srna, prop);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_StringProperty_doc,
             ".. function:: StringProperty(name=\"\", "
             "description=\"\", "
             "default=\"\", "
             "maxlen=0, "
             "options={'ANIMATABLE'}, "
             "override=set(), "
             "tags=set(), "
             "subtype='NONE', "
             "update=None, "
             "get=None, "
             "set=None)\n"
             "\n"
             "   Returns a new string property definition.\n"
             "\n" BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC
             "   :arg default: initializer string.\n"
             "   :type default: string\n"
             "   :arg maxlen: maximum length of the string.\n"
             "   :type maxlen: int\n" BPY_PROPDEF_OPTIONS_DOC BPY_PROPDEF_OPTIONS_OVERRIDE_DOC
                 BPY_PROPDEF_TAGS_DOC BPY_PROPDEF_SUBTYPE_STRING_DOC BPY_PROPDEF_UPDATE_DOC
                     BPY_PROPDEF_GET_DOC BPY_PROPDEF_SET_DOC);
static PyObject *BPy_StringProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(StringProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "", *def = "";
    Py_ssize_t id_len;
    int maxlen = 0;
    PropertyRNA *prop;
    PyObject *pyopts = NULL;
    int opts = 0;
    PyObject *pyopts_override = NULL;
    int opts_override = 0;
    int prop_tags = 0;
    const char *pysubtype = NULL;
    int subtype = PROP_NONE;
    PyObject *update_fn = NULL;
    PyObject *get_fn = NULL;
    PyObject *set_fn = NULL;
    PyObject *py_tags = NULL;

    static const char *_keywords[] = {
        "attr",
        "name",
        "description",
        "default",
        "maxlen",
        "options",
        "override",
        "tags",
        "subtype",
        "update",
        "get",
        "set",
        NULL,
    };
    static _PyArg_Parser _parser = {"s#|sssiO!O!O!sOOO:StringProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &name,
                                          &description,
                                          &def,
                                          &maxlen,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &pysubtype,
                                          &update_fn,
                                          &get_fn,
                                          &set_fn)) {
      return NULL;
    }

    BPY_PROPDEF_SUBTYPE_CHECK(StringProperty,
                              property_flag_items,
                              property_flag_override_items,
                              property_subtype_string_items);

    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(get_fn, "get", 1) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(set_fn, "set", 2) == -1) {
      return NULL;
    }

    prop = RNA_def_property(srna, id, PROP_STRING, subtype);
    if (maxlen != 0) {
      /* +1 since it includes null terminator. */
      RNA_def_property_string_maxlength(prop, maxlen + 1);
    }
    if (def && def[0]) {
      RNA_def_property_string_default(prop, def);
    }
    RNA_def_property_ui_text(prop, name ? name : id, description);

    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_string(prop, get_fn, set_fn);
    RNA_def_property_duplicate_pointers(srna, prop);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    BPy_EnumProperty_doc,
    ".. function:: EnumProperty(items, "
    "name=\"\", "
    "description=\"\", "
    "default=None, "
    "options={'ANIMATABLE'}, "
    "override=set(), "
    "tags=set(), "
    "update=None, "
    "get=None, "
    "set=None)\n"
    "\n"
    "   Returns a new enumerator property definition.\n"
    "\n"
    "   :arg items: sequence of enum items formatted:\n"
    "      ``[(identifier, name, description, icon, number), ...]``.\n"
    "\n"
    "      The first three elements of the tuples are mandatory.\n"
    "\n"
    "      :identifier: The identifier is used for Python access.\n"
    "      :name: Name for the interface.\n"
    "      :description: Used for documentation and tooltips.\n"
    "      :icon: An icon string identifier or integer icon value\n"
    "         (e.g. returned by :class:`bpy.types.UILayout.icon`)\n"
    "      :number: Unique value used as the identifier for this item (stored in file data).\n"
    "         Use when the identifier may need to change. If the *ENUM_FLAG* option is used,\n"
    "         the values are bitmasks and should be powers of two.\n"
    "\n"
    "      When an item only contains 4 items they define ``(identifier, name, description, "
    "number)``.\n"
    "\n"
    "      Separators may be added using None instead of a tuple."
    "\n"
    "      For dynamic values a callback can be passed which returns a list in\n"
    "      the same format as the static list.\n"
    "      This function must take 2 arguments ``(self, context)``, **context may be None**.\n"
    "\n"
    "      .. warning::\n"
    "\n"
    "         There is a known bug with using a callback,\n"
    "         Python must keep a reference to the strings returned by the callback or Blender\n"
    "         will misbehave or even crash."
    "\n"
    "   :type items: sequence of string tuples or a function\n" BPY_PROPDEF_NAME_DOC
        BPY_PROPDEF_DESC_DOC
    "   :arg default: The default value for this enum, a string from the identifiers used in "
    "*items*, or integer matching an item number.\n"
    "      If the *ENUM_FLAG* option is used this must be a set of such string identifiers "
    "instead.\n"
    "      WARNING: Strings can not be specified for dynamic enums\n"
    "      (i.e. if a callback function is given as *items* parameter).\n"
    "   :type default: string, integer or set\n" BPY_PROPDEF_OPTIONS_ENUM_DOC
        BPY_PROPDEF_OPTIONS_OVERRIDE_DOC BPY_PROPDEF_TAGS_DOC BPY_PROPDEF_UPDATE_DOC
            BPY_PROPDEF_GET_DOC BPY_PROPDEF_SET_DOC);
static PyObject *BPy_EnumProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(EnumProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "";
    PyObject *def = NULL;
    Py_ssize_t id_len;
    int defvalue = 0;
    PyObject *items, *items_fast;
    const EnumPropertyItem *eitems;
    PropertyRNA *prop;
    PyObject *pyopts = NULL;
    int opts = 0;
    PyObject *pyopts_override = NULL;
    int opts_override = 0;
    int prop_tags = 0;
    bool is_itemf = false;
    PyObject *update_fn = NULL;
    PyObject *get_fn = NULL;
    PyObject *set_fn = NULL;
    PyObject *py_tags = NULL;

    static const char *_keywords[] = {
        "attr",
        "items",
        "name",
        "description",
        "default",
        "options",
        "override",
        "tags",
        "update",
        "get",
        "set",
        NULL,
    };
    static _PyArg_Parser _parser = {"s#O|ssOO!O!O!OOO:EnumProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &items,
                                          &name,
                                          &description,
                                          &def,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &update_fn,
                                          &get_fn,
                                          &set_fn)) {
      return NULL;
    }

    BPY_PROPDEF_CHECK(EnumProperty, property_flag_enum_items, property_flag_override_items);

    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(get_fn, "get", 1) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(set_fn, "set", 2) == -1) {
      return NULL;
    }

    if (def == Py_None) {
      /* This allows to get same behavior when explicitly passing None as default value,
       * and not defining a default value at all! */
      def = NULL;
    }

    /* items can be a list or a callable */
    if (PyFunction_Check(
            items)) { /* don't use PyCallable_Check because we need the function code for errors */
      PyCodeObject *f_code = (PyCodeObject *)PyFunction_GET_CODE(items);
      if (f_code->co_argcount != 2) {
        PyErr_Format(PyExc_ValueError,
                     "EnumProperty(...): expected 'items' function to take 2 arguments, not %d",
                     f_code->co_argcount);
        return NULL;
      }

      if (def) {
        /* Only support getting integer default values here. */
        if (!py_long_as_int(def, &defvalue)) {
          /* note, using type error here is odd but python does this for invalid arguments */
          PyErr_SetString(
              PyExc_TypeError,
              "EnumProperty(...): 'default' can only be an integer when 'items' is a function");
          return NULL;
        }
      }

      is_itemf = true;
      eitems = DummyRNA_NULL_items;
    }
    else {
      if (!(items_fast = PySequence_Fast(
                items,
                "EnumProperty(...): "
                "expected a sequence of tuples for the enum items or a function"))) {
        return NULL;
      }

      eitems = enum_items_from_py(items_fast, def, &defvalue, (opts & PROP_ENUM_FLAG) != 0);

      if (!eitems) {
        Py_DECREF(items_fast);
        return NULL;
      }
    }

    if (opts & PROP_ENUM_FLAG) {
      prop = RNA_def_enum_flag(srna, id, eitems, defvalue, name ? name : id, description);
    }
    else {
      prop = RNA_def_enum(srna, id, eitems, defvalue, name ? name : id, description);
    }

    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_enum(prop, get_fn, set_fn, (is_itemf ? items : NULL));
    RNA_def_property_duplicate_pointers(srna, prop);

    if (is_itemf == false) {
      /* note: this must be postponed until after #RNA_def_property_duplicate_pointers
       * otherwise if this is a generator it may free the strings before we copy them */
      Py_DECREF(items_fast);

      MEM_freeN((void *)eitems);
    }
  }
  Py_RETURN_NONE;
}

StructRNA *pointer_type_from_py(PyObject *value, const char *error_prefix)
{
  StructRNA *srna;

  srna = srna_from_self(value, "");
  if (!srna) {
    if (PyErr_Occurred()) {
      PyObject *msg = PyC_ExceptionBuffer();
      const char *msg_char = PyUnicode_AsUTF8(msg);
      PyErr_Format(
          PyExc_TypeError, "%.200s expected an RNA type, failed with: %s", error_prefix, msg_char);
      Py_DECREF(msg);
    }
    else {
      PyErr_Format(PyExc_TypeError,
                   "%.200s expected an RNA type, failed with type '%s'",
                   error_prefix,
                   Py_TYPE(value)->tp_name);
    }
    return NULL;
  }

  return srna;
}

PyDoc_STRVAR(BPy_PointerProperty_doc,
             ".. function:: PointerProperty(type=None, "
             "name=\"\", "
             "description=\"\", "
             "options={'ANIMATABLE'}, "
             "override=set(), "
             "tags=set(), "
             "poll=None, "
             "update=None)\n"
             "\n"
             "   Returns a new pointer property definition.\n"
             "\n" BPY_PROPDEF_TYPE_DOC BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC
                 BPY_PROPDEF_OPTIONS_DOC BPY_PROPDEF_OPTIONS_OVERRIDE_DOC BPY_PROPDEF_TAGS_DOC
                     BPY_PROPDEF_POLL_DOC BPY_PROPDEF_UPDATE_DOC);
PyObject *BPy_PointerProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(PointerProperty);

  if (srna) {
    const char *id = NULL, *name = NULL, *description = "";
    Py_ssize_t id_len;
    PropertyRNA *prop;
    StructRNA *ptype;
    PyObject *type = Py_None;
    PyObject *pyopts = NULL;
    PyObject *pyopts_override = NULL;
    PyObject *py_tags = NULL;
    int opts = 0;
    int opts_override = 0;
    int prop_tags = 0;
    PyObject *update_fn = NULL, *poll_fn = NULL;

    static const char *_keywords[] = {
        "attr",
        "type",
        "name",
        "description",
        "options",
        "override",
        "tags",
        "poll",
        "update",
        NULL,
    };
    static _PyArg_Parser _parser = {"s#O|ssO!O!O!OO:PointerProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &type,
                                          &name,
                                          &description,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags,
                                          &poll_fn,
                                          &update_fn)) {
      return NULL;
    }

    BPY_PROPDEF_CHECK(PointerProperty, property_flag_items, property_flag_override_items);

    ptype = pointer_type_from_py(type, "PointerProperty(...)");
    if (!ptype) {
      return NULL;
    }
    if (!RNA_struct_is_a(ptype, &RNA_PropertyGroup) && !RNA_struct_is_ID(ptype)) {
      PyErr_Format(PyExc_TypeError,
                   "PointerProperty(...) expected an RNA type derived from %.200s or %.200s",
                   RNA_struct_ui_name(&RNA_ID),
                   RNA_struct_ui_name(&RNA_PropertyGroup));
      return NULL;
    }
    if (bpy_prop_callback_check(update_fn, "update", 2) == -1) {
      return NULL;
    }
    if (bpy_prop_callback_check(poll_fn, "poll", 2) == -1) {
      return NULL;
    }
    prop = RNA_def_pointer_runtime(srna, id, ptype, name ? name : id, description);
    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }

    if (RNA_struct_idprops_contains_datablock(ptype)) {
      if (RNA_struct_is_a(srna, &RNA_PropertyGroup)) {
        RNA_def_struct_flag(srna, STRUCT_CONTAINS_DATABLOCK_IDPROPERTIES);
      }
    }
    bpy_prop_callback_assign_update(prop, update_fn);
    bpy_prop_callback_assign_pointer(prop, poll_fn);
    RNA_def_property_duplicate_pointers(srna, prop);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_CollectionProperty_doc,
             ".. function:: CollectionProperty(type=None, "
             "name=\"\", "
             "description=\"\", "
             "options={'ANIMATABLE'}, "
             "override=set(), "
             "tags=set())\n"
             "\n"
             "   Returns a new collection property definition.\n"
             "\n" BPY_PROPDEF_TYPE_DOC BPY_PROPDEF_NAME_DOC BPY_PROPDEF_DESC_DOC
                 BPY_PROPDEF_OPTIONS_DOC BPY_PROPDEF_OPTIONS_OVERRIDE_COLLECTION_DOC
                     BPY_PROPDEF_TAGS_DOC);
PyObject *BPy_CollectionProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  BPY_PROPDEF_HEAD(CollectionProperty);

  if (srna) {
    Py_ssize_t id_len;
    const char *id = NULL, *name = NULL, *description = "";
    PropertyRNA *prop;
    StructRNA *ptype;
    PyObject *type = Py_None;
    PyObject *pyopts = NULL;
    PyObject *pyopts_override = NULL;
    PyObject *py_tags = NULL;
    int opts = 0;
    int opts_override = 0;
    int prop_tags = 0;

    static const char *_keywords[] = {
        "attr",
        "type",
        "name",
        "description",
        "options",
        "override",
        "tags",
        NULL,
    };
    static _PyArg_Parser _parser = {"s#O|ssO!O!O!:CollectionProperty", _keywords, 0};
    if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                          kw,
                                          &_parser,
                                          &id,
                                          &id_len,
                                          &type,
                                          &name,
                                          &description,
                                          &PySet_Type,
                                          &pyopts,
                                          &PySet_Type,
                                          &pyopts_override,
                                          &PySet_Type,
                                          &py_tags)) {
      return NULL;
    }

    BPY_PROPDEF_CHECK(
        CollectionProperty, property_flag_items, property_flag_override_collection_items);

    ptype = pointer_type_from_py(type, "CollectionProperty(...):");
    if (!ptype) {
      return NULL;
    }

    if (!RNA_struct_is_a(ptype, &RNA_PropertyGroup)) {
      PyErr_Format(PyExc_TypeError,
                   "CollectionProperty(...) expected an RNA type derived from %.200s",
                   RNA_struct_ui_name(&RNA_PropertyGroup));
      return NULL;
    }

    prop = RNA_def_collection_runtime(srna, id, ptype, name ? name : id, description);
    if (py_tags) {
      RNA_def_property_tags(prop, prop_tags);
    }
    if (pyopts) {
      bpy_prop_assign_flag(prop, opts);
    }
    if (pyopts_override) {
      bpy_prop_assign_flag_override(prop, opts_override);
    }

    if (RNA_struct_idprops_contains_datablock(ptype)) {
      if (RNA_struct_is_a(srna, &RNA_PropertyGroup)) {
        RNA_def_struct_flag(srna, STRUCT_CONTAINS_DATABLOCK_IDPROPERTIES);
      }
    }
    RNA_def_property_duplicate_pointers(srna, prop);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_RemoveProperty_doc,
             ".. function:: RemoveProperty(cls, attr)\n"
             "\n"
             "   Removes a dynamically defined property.\n"
             "\n"
             "   :arg cls: The class containing the property (must be a positional argument).\n"
             "   :type cls: type\n"
             "   :arg attr: Property name (must be passed as a keyword).\n"
             "   :type attr: string\n"
             "\n"
             ".. note:: Typically this function doesn't need to be accessed directly.\n"
             "   Instead use ``del cls.attr``\n");
static PyObject *BPy_RemoveProperty(PyObject *self, PyObject *args, PyObject *kw)
{
  StructRNA *srna;

  if (PyTuple_GET_SIZE(args) == 1) {
    PyObject *ret;
    self = PyTuple_GET_ITEM(args, 0);
    args = PyTuple_New(0);
    ret = BPy_RemoveProperty(self, args, kw);
    Py_DECREF(args);
    return ret;
  }
  if (PyTuple_GET_SIZE(args) > 1) {
    PyErr_SetString(PyExc_ValueError, "expected one positional arg, one keyword arg");
    return NULL;
  }

  srna = srna_from_self(self, "RemoveProperty(...):");
  if (srna == NULL && PyErr_Occurred()) {
    return NULL; /* self's type was compatible but error getting the srna */
  }
  if (srna == NULL) {
    PyErr_SetString(PyExc_TypeError, "RemoveProperty(): struct rna not available for this type");
    return NULL;
  }

  const char *id = NULL;

  static const char *_keywords[] = {
      "attr",
      NULL,
  };
  static _PyArg_Parser _parser = {"s:RemoveProperty", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &id)) {
    return NULL;
  }

  if (RNA_def_property_free_identifier(srna, id) != 1) {
    PyErr_Format(PyExc_TypeError, "RemoveProperty(): '%s' not a defined dynamic property", id);
    return NULL;
  }

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Module `bpy.props`
 * \{ */

static struct PyMethodDef props_methods[] = {
    {"BoolProperty",
     (PyCFunction)BPy_BoolProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_BoolProperty_doc},
    {"BoolVectorProperty",
     (PyCFunction)BPy_BoolVectorProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_BoolVectorProperty_doc},
    {"IntProperty",
     (PyCFunction)BPy_IntProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_IntProperty_doc},
    {"IntVectorProperty",
     (PyCFunction)BPy_IntVectorProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_IntVectorProperty_doc},
    {"FloatProperty",
     (PyCFunction)BPy_FloatProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_FloatProperty_doc},
    {"FloatVectorProperty",
     (PyCFunction)BPy_FloatVectorProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_FloatVectorProperty_doc},
    {"StringProperty",
     (PyCFunction)BPy_StringProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_StringProperty_doc},
    {"EnumProperty",
     (PyCFunction)BPy_EnumProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_EnumProperty_doc},
    {"PointerProperty",
     (PyCFunction)BPy_PointerProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_PointerProperty_doc},
    {"CollectionProperty",
     (PyCFunction)BPy_CollectionProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_CollectionProperty_doc},

    {"RemoveProperty",
     (PyCFunction)BPy_RemoveProperty,
     METH_VARARGS | METH_KEYWORDS,
     BPy_RemoveProperty_doc},
    {NULL, NULL, 0, NULL},
};

static int props_visit(PyObject *UNUSED(self), visitproc visit, void *arg)
{
  LISTBASE_FOREACH (struct BPyPropStore *, prop_store, &g_bpy_prop_store_list) {
    PyObject **py_data = (PyObject **)&prop_store->py_data;
    for (int i = 0; i < BPY_PROP_STORE_PY_DATA_SIZE; i++) {
      Py_VISIT(py_data[i]);
    }
  }
  return 0;
}

static int props_clear(PyObject *UNUSED(self))
{
  LISTBASE_FOREACH (struct BPyPropStore *, prop_store, &g_bpy_prop_store_list) {
    PyObject **py_data = (PyObject **)&prop_store->py_data;
    for (int i = 0; i < BPY_PROP_STORE_PY_DATA_SIZE; i++) {
      Py_CLEAR(py_data[i]);
    }
  }
  return 0;
}

static struct PyModuleDef props_module = {
    PyModuleDef_HEAD_INIT,
    "bpy.props",
    "This module defines properties to extend Blender's internal data. The result of these "
    "functions"
    " is used to assign properties to classes registered with Blender and can't be used "
    "directly.\n"
    "\n"
    ".. note:: All parameters to these functions must be passed as keywords.\n",
    -1, /* multiple "initialization" just copies the module dict. */
    props_methods,
    NULL,
    props_visit,
    props_clear,
    NULL,
};

PyObject *BPY_rna_props(void)
{
  PyObject *submodule;
  PyObject *submodule_dict;

  submodule = PyModule_Create(&props_module);
  PyDict_SetItemString(PyImport_GetModuleDict(), props_module.m_name, submodule);

  /* api needs the PyObjects internally */
  submodule_dict = PyModule_GetDict(submodule);

#define ASSIGN_STATIC(_name) pymeth_##_name = PyDict_GetItemString(submodule_dict, #_name)

  ASSIGN_STATIC(BoolProperty);
  ASSIGN_STATIC(BoolVectorProperty);
  ASSIGN_STATIC(IntProperty);
  ASSIGN_STATIC(IntVectorProperty);
  ASSIGN_STATIC(FloatProperty);
  ASSIGN_STATIC(FloatVectorProperty);
  ASSIGN_STATIC(StringProperty);
  ASSIGN_STATIC(EnumProperty);
  ASSIGN_STATIC(PointerProperty);
  ASSIGN_STATIC(CollectionProperty);
  ASSIGN_STATIC(RemoveProperty);

  if (PyType_Ready(&bpy_prop_deferred_Type) < 0) {
    return NULL;
  }
  PyModule_AddType(submodule, &bpy_prop_deferred_Type);

  /* Run this when properties are freed. */
  RNA_def_property_free_pointers_set_py_data_callback(bpy_prop_py_data_remove);

  return submodule;
}

/**
 * Run this on exit, clearing all Python callback users and disable the RNA callback,
 * as it would be called after Python has already finished.
 */
void BPY_rna_props_clear_all(void)
{
  /* Remove all user counts, so this isn't considered a leak from Python's perspective. */
  props_clear(NULL);

  /* Running is harmless, but redundant. */
  RNA_def_property_free_pointers_set_py_data_callback(NULL);

  /* Include as it's correct, in practice this should never be used again. */
  BLI_listbase_clear(&g_bpy_prop_store_list);
}

/** \} */
