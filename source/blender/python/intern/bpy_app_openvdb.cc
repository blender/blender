/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "bpy_app_openvdb.h"

#include "../generic/py_capi_utils.h"

#ifdef WITH_OPENVDB
#  include "openvdb_capi.h"
#endif

static PyTypeObject BlenderAppOVDBType;

static PyStructSequence_Field app_openvdb_info_fields[] = {
    {"supported", "Boolean, True when Blender is built with OpenVDB support"},
    {"version", "The OpenVDB version as a tuple of 3 numbers"},
    {"version_string", "The OpenVDB version formatted as a string"},
    {nullptr},
};

static PyStructSequence_Desc app_openvdb_info_desc = {
    "bpy.app.openvdb",                                                          /* name */
    "This module contains information about OpenVDB blender is linked against", /* doc */
    app_openvdb_info_fields,                                                    /* fields */
    ARRAY_SIZE(app_openvdb_info_fields) - 1,
};

static PyObject *make_openvdb_info()
{
  PyObject *openvdb_info;
  int pos = 0;

#ifdef WITH_OPENVDB
  int curversion;
#endif

  openvdb_info = PyStructSequence_New(&BlenderAppOVDBType);
  if (openvdb_info == nullptr) {
    return nullptr;
  }

#ifndef WITH_OPENVDB
#  define SetStrItem(str) PyStructSequence_SET_ITEM(openvdb_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) PyStructSequence_SET_ITEM(openvdb_info, pos++, obj)

#ifdef WITH_OPENVDB
  curversion = OpenVDB_getVersionHex();
  SetObjItem(PyBool_FromLong(1));
  SetObjItem(
      PyC_Tuple_Pack_I32({curversion >> 24, (curversion >> 16) % 256, (curversion >> 8) % 256}));
  SetObjItem(PyUnicode_FromFormat(
      "%2d, %2d, %2d", curversion >> 24, (curversion >> 16) % 256, (curversion >> 8) % 256));
#else
  SetObjItem(PyBool_FromLong(0));
  SetObjItem(PyC_Tuple_Pack_I32({0, 0, 0}));
  SetStrItem("Unknown");
#endif

  if (UNLIKELY(PyErr_Occurred())) {
    Py_DECREF(openvdb_info);
    return nullptr;
  }

#undef SetStrItem
#undef SetObjItem

  return openvdb_info;
}

PyObject *BPY_app_openvdb_struct()
{
  PyObject *ret;

  PyStructSequence_InitType(&BlenderAppOVDBType, &app_openvdb_info_desc);

  ret = make_openvdb_info();

  /* prevent user from creating new instances */
  BlenderAppOVDBType.tp_init = nullptr;
  BlenderAppOVDBType.tp_new = nullptr;
  /* Without this we can't do `set(sys.modules)` #29635. */
  BlenderAppOVDBType.tp_hash = (hashfunc)_Py_HashPointer;

  return ret;
}
