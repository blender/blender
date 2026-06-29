/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.hh"
#include <Python.h>

#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "bpy_app_sdl.hh"

#include "../generic/py_capi_utils.hh"

namespace blender {

#ifdef WITH_SDL
#  include <SDL3/SDL.h>
#endif

static PyTypeObject BlenderAppSDLType;

static PyStructSequence_Field app_sdl_info_fields[] = {
    {"supported", ("Boolean, True when Blender is built with SDL support")},
    {"version", ("The SDL version as a tuple of 3 numbers")},
    {"version_string", ("The SDL version formatted as a string")},
    {nullptr},
};

static PyStructSequence_Desc app_sdl_info_desc = {
    /*name*/ "bpy.app.sdl",
    /*doc*/ "This module contains information about SDL blender is linked against",
    /*fields*/ app_sdl_info_fields,
    /*n_in_sequence*/ ARRAY_SIZE(app_sdl_info_fields) - 1,
};

static PyObject *make_sdl_info()
{
  PyObject *sdl_info;
  int pos = 0;

  sdl_info = PyStructSequence_New(&BlenderAppSDLType);
  if (sdl_info == nullptr) {
    return nullptr;
  }

#define SetStrItem(str) PyStructSequence_SET_ITEM(sdl_info, pos++, PyUnicode_FromString(str))

#define SetObjItem(obj) PyStructSequence_SET_ITEM(sdl_info, pos++, obj)

#ifdef WITH_SDL
  SetObjItem(PyBool_FromLong(1));
  {
    int sdl_ver = SDL_GetVersion();
    const int major = SDL_VERSIONNUM_MAJOR(sdl_ver);
    const int minor = SDL_VERSIONNUM_MINOR(sdl_ver);
    const int patch = SDL_VERSIONNUM_MICRO(sdl_ver);
    SetObjItem(PyC_Tuple_Pack_I32({major, minor, patch}));
    SetObjItem(PyUnicode_FromFormat("%d.%d.%d", major, minor, patch));
  }

#else /* WITH_SDL=OFF */
  SetObjItem(PyBool_FromLong(0));
  SetObjItem(PyC_Tuple_Pack_I32({0, 0, 0}));
  SetStrItem("Unknown");
#endif

  if (PyErr_Occurred()) [[unlikely]] {
    Py_DECREF(sdl_info);
    return nullptr;
  }

#undef SetStrItem
#undef SetObjItem

  return sdl_info;
}

PyObject *BPY_app_sdl_struct()
{
  PyObject *ret;

  PyStructSequence_InitType(&BlenderAppSDLType, &app_sdl_info_desc);

  ret = make_sdl_info();

  /* prevent user from creating new instances */
  BlenderAppSDLType.tp_init = nullptr;
  BlenderAppSDLType.tp_new = nullptr;
  /* Without this we can't do `set(sys.modules)` #29635. */
  BlenderAppSDLType.tp_hash = reinterpret_cast<hashfunc>(Py_HashPointer);

  return ret;
}

}  // namespace blender
