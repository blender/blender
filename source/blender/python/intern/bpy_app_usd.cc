/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "bpy_app_usd.hh"

#include "../generic/py_capi_utils.hh"

#ifdef WITH_USD
#  include "usd.hh"
#endif

static PyTypeObject BlenderAppUSDType;

static PyStructSequence_Field app_usd_info_fields[] = {
    {"supported", "Boolean, True when Blender is built with USD support"},
    {"version", "The USD version as a tuple of 3 numbers"},
    {"version_string", "The USD version formatted as a string"},
    {nullptr},
};

static PyStructSequence_Desc app_usd_info_desc = {
    /*name*/ "bpy.app.usd",
    /*doc*/
    "This module contains information about the Universal Scene Description library Bender is "
    "linked against",
    /*fields*/ app_usd_info_fields,
    /*n_in_sequence*/ ARRAY_SIZE(app_usd_info_fields) - 1,
};

static PyObject *make_usd_info()
{
  PyObject *usd_info = PyStructSequence_New(&BlenderAppUSDType);

  if (usd_info == nullptr) {
    return nullptr;
  }

  int pos = 0;

#ifndef WITH_USD
#  define SetStrItem(str) PyStructSequence_SET_ITEM(usd_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) PyStructSequence_SET_ITEM(usd_info, pos++, obj)

#ifdef WITH_USD
  const int curversion = blender::io::usd::USD_get_version();
  const int major = curversion / 10000;
  const int minor = (curversion / 100) % 100;
  const int patch = curversion % 100;

  SetObjItem(PyBool_FromLong(1));
  SetObjItem(PyC_Tuple_Pack_I32({major, minor, patch}));
  SetObjItem(PyUnicode_FromFormat("%2d, %2d, %2d", major, minor, patch));
#else
  SetObjItem(PyBool_FromLong(0));
  SetObjItem(PyC_Tuple_Pack_I32({0, 0, 0}));
  SetStrItem("Unknown");
#endif

  if (UNLIKELY(PyErr_Occurred())) {
    Py_DECREF(usd_info);
    return nullptr;
  }

#undef SetStrItem
#undef SetObjItem

  return usd_info;
}

PyObject *BPY_app_usd_struct()
{
  PyStructSequence_InitType(&BlenderAppUSDType, &app_usd_info_desc);

  PyObject *ret = make_usd_info();

  /* prevent user from creating new instances */
  BlenderAppUSDType.tp_init = nullptr;
  BlenderAppUSDType.tp_new = nullptr;
  /* Without this we can't do `set(sys.modules)` #29635. */
  BlenderAppUSDType.tp_hash = (hashfunc)Py_HashPointer;

  return ret;
}
