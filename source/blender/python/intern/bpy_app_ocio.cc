/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "bpy_app_ocio.hh"

#include "../generic/py_capi_utils.hh"

#include "OCIO_version.hh"

namespace ocio = blender::ocio;

static PyTypeObject BlenderAppOCIOType;

static PyStructSequence_Field app_ocio_info_fields[] = {
    {"supported", "Boolean, True when Blender is built with OpenColorIO support"},
    {"version", "The OpenColorIO version as a tuple of 3 numbers"},
    {"version_string", "The OpenColorIO version formatted as a string"},
    {nullptr},
};

static PyStructSequence_Desc app_ocio_info_desc = {
    /*name*/ "bpy.app.ocio",
    /*doc*/ "This module contains information about OpenColorIO blender is linked against",
    /*fields*/ app_ocio_info_fields,
    /*n_in_sequence*/ ARRAY_SIZE(app_ocio_info_fields) - 1,
};

static PyObject *make_ocio_info()
{
  PyObject *ocio_info;
  int pos = 0;

  ocio_info = PyStructSequence_New(&BlenderAppOCIOType);
  if (ocio_info == nullptr) {
    return nullptr;
  }

#ifndef WITH_OPENCOLORIO
#  define SetStrItem(str) PyStructSequence_SET_ITEM(ocio_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) PyStructSequence_SET_ITEM(ocio_info, pos++, obj)

#ifdef WITH_OPENCOLORIO
  const ocio::Version ocio_version = ocio::get_version();
  SetObjItem(PyBool_FromLong(1));
  SetObjItem(PyC_Tuple_Pack_I32({ocio_version.major, ocio_version.minor, ocio_version.patch}));
  SetObjItem(PyUnicode_FromFormat(
      "%2d, %2d, %2d", ocio_version.major, ocio_version.minor, ocio_version.patch));
#else
  SetObjItem(PyBool_FromLong(0));
  SetObjItem(PyC_Tuple_Pack_I32({0, 0, 0}));
  SetStrItem("Unknown");
#endif

  if (UNLIKELY(PyErr_Occurred())) {
    Py_DECREF(ocio_info);
    return nullptr;
  }

#undef SetStrItem
#undef SetObjItem

  return ocio_info;
}

PyObject *BPY_app_ocio_struct()
{
  PyObject *ret;

  PyStructSequence_InitType(&BlenderAppOCIOType, &app_ocio_info_desc);

  ret = make_ocio_info();

  /* prevent user from creating new instances */
  BlenderAppOCIOType.tp_init = nullptr;
  BlenderAppOCIOType.tp_new = nullptr;
  /* Without this we can't do `set(sys.modules)` #29635. */
  BlenderAppOCIOType.tp_hash = (hashfunc)Py_HashPointer;

  return ret;
}
