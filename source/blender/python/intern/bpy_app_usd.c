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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "bpy_app_usd.h"

#include "../generic/py_capi_utils.h"

#ifdef WITH_USD
#  include "usd.h"
#endif

static PyTypeObject BlenderAppUSDType;

static PyStructSequence_Field app_usd_info_fields[] = {
    {"supported", "Boolean, True when Blender is built with USD support"},
    {"version", "The USD version as a tuple of 3 numbers"},
    {"version_string", "The USD version formatted as a string"},
    {NULL},
};

static PyStructSequence_Desc app_usd_info_desc = {
    "bpy.app.usd", /* name */
    "This module contains information about the Universal Scene Description library Bender is "
    "linked against",    /* doc */
    app_usd_info_fields, /* fields */
    ARRAY_SIZE(app_usd_info_fields) - 1,
};

static PyObject *make_usd_info(void)
{
  PyObject *usd_info = PyStructSequence_New(&BlenderAppUSDType);

  if (usd_info == NULL) {
    return NULL;
  }

  int pos = 0;

#ifndef WITH_USD
#  define SetStrItem(str) PyStructSequence_SET_ITEM(usd_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) PyStructSequence_SET_ITEM(usd_info, pos++, obj)

#ifdef WITH_USD
  const int curversion = USD_get_version();
  const int major = curversion / 10000;
  const int minor = (curversion / 100) % 100;
  const int patch = curversion % 100;

  SetObjItem(PyBool_FromLong(1));
  SetObjItem(PyC_Tuple_Pack_I32(major, minor, patch));
  SetObjItem(PyUnicode_FromFormat("%2d, %2d, %2d", major, minor, patch));
#else
  SetObjItem(PyBool_FromLong(0));
  SetObjItem(PyC_Tuple_Pack_I32(0, 0, 0));
  SetStrItem("Unknown");
#endif

  if (UNLIKELY(PyErr_Occurred())) {
    Py_DECREF(usd_info);
    return NULL;
  }

#undef SetStrItem
#undef SetObjItem

  return usd_info;
}

PyObject *BPY_app_usd_struct(void)
{
  PyStructSequence_InitType(&BlenderAppUSDType, &app_usd_info_desc);

  PyObject *ret = make_usd_info();

  /* prevent user from creating new instances */
  BlenderAppUSDType.tp_init = NULL;
  BlenderAppUSDType.tp_new = NULL;
  BlenderAppUSDType.tp_hash = (hashfunc)
      _Py_HashPointer; /* without this we can't do set(sys.modules) T29635. */

  return ret;
}
