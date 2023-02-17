/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "bpy_app_oiio.h"

#include "../generic/py_capi_utils.h"

#ifdef WITH_OPENIMAGEIO
#  include "openimageio_api.h"
#endif

static PyTypeObject BlenderAppOIIOType;

static PyStructSequence_Field app_oiio_info_fields[] = {
    {"supported", "Boolean, True when Blender is built with OpenImageIO support"},
    {"version", "The OpenImageIO version as a tuple of 3 numbers"},
    {"version_string", "The OpenImageIO version formatted as a string"},
    {NULL},
};

static PyStructSequence_Desc app_oiio_info_desc = {
    "bpy.app.oiio",                                                                /* name */
    "This module contains information about OpeImageIO blender is linked against", /* doc */
    app_oiio_info_fields,                                                          /* fields */
    ARRAY_SIZE(app_oiio_info_fields) - 1,
};

static PyObject *make_oiio_info(void)
{
  PyObject *oiio_info;
  int pos = 0;

#ifdef WITH_OPENIMAGEIO
  int curversion;
#endif

  oiio_info = PyStructSequence_New(&BlenderAppOIIOType);
  if (oiio_info == NULL) {
    return NULL;
  }

#ifndef WITH_OPENIMAGEIO
#  define SetStrItem(str) PyStructSequence_SET_ITEM(oiio_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) PyStructSequence_SET_ITEM(oiio_info, pos++, obj)

#ifdef WITH_OPENIMAGEIO
  curversion = OIIO_getVersionHex();
  SetObjItem(PyBool_FromLong(1));
  SetObjItem(PyC_Tuple_Pack_I32(curversion / 10000, (curversion / 100) % 100, curversion % 100));
  SetObjItem(PyUnicode_FromFormat(
      "%2d, %2d, %2d", curversion / 10000, (curversion / 100) % 100, curversion % 100));
#else
  SetObjItem(PyBool_FromLong(0));
  SetObjItem(PyC_Tuple_Pack_I32(0, 0, 0));
  SetStrItem("Unknown");
#endif

  if (UNLIKELY(PyErr_Occurred())) {
    Py_DECREF(oiio_info);
    return NULL;
  }

#undef SetStrItem
#undef SetObjItem

  return oiio_info;
}

PyObject *BPY_app_oiio_struct(void)
{
  PyObject *ret;

  PyStructSequence_InitType(&BlenderAppOIIOType, &app_oiio_info_desc);

  ret = make_oiio_info();

  /* prevent user from creating new instances */
  BlenderAppOIIOType.tp_init = NULL;
  BlenderAppOIIOType.tp_new = NULL;
  BlenderAppOIIOType.tp_hash = (hashfunc)
      _Py_HashPointer; /* without this we can't do set(sys.modules) #29635. */

  return ret;
}
