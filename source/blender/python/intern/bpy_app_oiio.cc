/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "bpy_app_oiio.hh"

#include "../generic/py_capi_utils.hh"

#include "openimageio_api.h"

static PyTypeObject BlenderAppOIIOType;

static PyStructSequence_Field app_oiio_info_fields[] = {
    {"supported", "Boolean, True when Blender is built with OpenImageIO support"},
    {"version", "The OpenImageIO version as a tuple of 3 numbers"},
    {"version_string", "The OpenImageIO version formatted as a string"},
    {nullptr},
};

static PyStructSequence_Desc app_oiio_info_desc = {
    /*name*/ "bpy.app.oiio",
    /*doc*/ "This module contains information about OpeImageIO blender is linked against",
    /*fields*/ app_oiio_info_fields,
    /*n_in_sequence*/ ARRAY_SIZE(app_oiio_info_fields) - 1,
};

static PyObject *make_oiio_info()
{
  PyObject *oiio_info;
  int pos = 0;

  int curversion;

  oiio_info = PyStructSequence_New(&BlenderAppOIIOType);
  if (oiio_info == nullptr) {
    return nullptr;
  }

#define SetObjItem(obj) PyStructSequence_SET_ITEM(oiio_info, pos++, obj)

  curversion = OIIO_getVersionHex();
  SetObjItem(PyBool_FromLong(1));
  SetObjItem(PyC_Tuple_Pack_I32({curversion / 10000, (curversion / 100) % 100, curversion % 100}));
  SetObjItem(PyUnicode_FromFormat(
      "%2d, %2d, %2d", curversion / 10000, (curversion / 100) % 100, curversion % 100));

  if (UNLIKELY(PyErr_Occurred())) {
    Py_DECREF(oiio_info);
    return nullptr;
  }

#undef SetStrItem
#undef SetObjItem

  return oiio_info;
}

PyObject *BPY_app_oiio_struct()
{
  PyObject *ret;

  PyStructSequence_InitType(&BlenderAppOIIOType, &app_oiio_info_desc);

  ret = make_oiio_info();

  /* prevent user from creating new instances */
  BlenderAppOIIOType.tp_init = nullptr;
  BlenderAppOIIOType.tp_new = nullptr;
  /* Without this we can't do `set(sys.modules)` #29635. */
  BlenderAppOIIOType.tp_hash = (hashfunc)Py_HashPointer;

  return ret;
}
