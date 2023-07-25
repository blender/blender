/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * Store python versions of strings frequently used for python lookups
 * to avoid converting, creating the hash and freeing every time as
 * PyDict_GetItemString and PyObject_GetAttrString do.
 */

#include <Python.h>

#include "bpy_intern_string.h"

#include "BLI_utildefines.h"

static PyObject *bpy_intern_str_arr[17];

PyObject *bpy_intern_str___annotations__;
PyObject *bpy_intern_str___doc__;
PyObject *bpy_intern_str___main__;
PyObject *bpy_intern_str___module__;
PyObject *bpy_intern_str___name__;
PyObject *bpy_intern_str___slots__;
PyObject *bpy_intern_str_attr;
PyObject *bpy_intern_str_bl_property;
PyObject *bpy_intern_str_bl_rna;
PyObject *bpy_intern_str_bl_target_properties;
PyObject *bpy_intern_str_bpy_types;
PyObject *bpy_intern_str_frame;
PyObject *bpy_intern_str_properties;
PyObject *bpy_intern_str_register;
PyObject *bpy_intern_str_self;
PyObject *bpy_intern_str_depsgraph;
PyObject *bpy_intern_str_unregister;

void bpy_intern_string_init()
{
  uint i = 0;

#define BPY_INTERN_STR(var, str) \
  { \
    var = bpy_intern_str_arr[i++] = PyUnicode_FromString(str); \
  } \
  (void)0

  BPY_INTERN_STR(bpy_intern_str___annotations__, "__annotations__");
  BPY_INTERN_STR(bpy_intern_str___doc__, "__doc__");
  BPY_INTERN_STR(bpy_intern_str___main__, "__main__");
  BPY_INTERN_STR(bpy_intern_str___module__, "__module__");
  BPY_INTERN_STR(bpy_intern_str___name__, "__name__");
  BPY_INTERN_STR(bpy_intern_str___slots__, "__slots__");
  BPY_INTERN_STR(bpy_intern_str_attr, "attr");
  BPY_INTERN_STR(bpy_intern_str_bl_property, "bl_property");
  BPY_INTERN_STR(bpy_intern_str_bl_rna, "bl_rna");
  BPY_INTERN_STR(bpy_intern_str_bl_target_properties, "bl_target_properties");
  BPY_INTERN_STR(bpy_intern_str_bpy_types, "bpy.types");
  BPY_INTERN_STR(bpy_intern_str_frame, "frame");
  BPY_INTERN_STR(bpy_intern_str_properties, "properties");
  BPY_INTERN_STR(bpy_intern_str_register, "register");
  BPY_INTERN_STR(bpy_intern_str_self, "self");
  BPY_INTERN_STR(bpy_intern_str_depsgraph, "depsgraph");
  BPY_INTERN_STR(bpy_intern_str_unregister, "unregister");

#undef BPY_INTERN_STR

  BLI_assert(i == ARRAY_SIZE(bpy_intern_str_arr));
}

void bpy_intern_string_exit()
{
  uint i = ARRAY_SIZE(bpy_intern_str_arr);
  while (i--) {
    Py_DECREF(bpy_intern_str_arr[i]);
  }
}
