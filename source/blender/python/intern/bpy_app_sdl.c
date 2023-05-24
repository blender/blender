/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "bpy_app_sdl.h"

#include "../generic/py_capi_utils.h"

#ifdef WITH_SDL
/* SDL force defines __SSE__ and __SSE2__ flags, which generates warnings
 * because we pass those defines via command line as well. For until there's
 * proper ifndef added to SDL headers we ignore the redefinition warning.
 */
#  ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4005)
#  endif
#  include "SDL.h"
#  ifdef _MSC_VER
#    pragma warning(pop)
#  endif
#  ifdef WITH_SDL_DYNLOAD
#    include "sdlew.h"
#  endif
#endif

static PyTypeObject BlenderAppSDLType;

static PyStructSequence_Field app_sdl_info_fields[] = {
    {"supported", ("Boolean, True when Blender is built with SDL support")},
    {"version", ("The SDL version as a tuple of 3 numbers")},
    {"version_string", ("The SDL version formatted as a string")},
    {"available",
     ("Boolean, True when SDL is available. This is False when "
      "either *supported* is False, or *dynload* is True and "
      "Blender cannot find the correct library.")},
    {NULL},
};

static PyStructSequence_Desc app_sdl_info_desc = {
    "bpy.app.sdl",                                                          /* name */
    "This module contains information about SDL blender is linked against", /* doc */
    app_sdl_info_fields,                                                    /* fields */
    ARRAY_SIZE(app_sdl_info_fields) - 1,
};

static PyObject *make_sdl_info(void)
{
  PyObject *sdl_info;
  int pos = 0;
#ifdef WITH_SDL
  bool sdl_available = false;
  SDL_version version = {0, 0, 0};
#endif

  sdl_info = PyStructSequence_New(&BlenderAppSDLType);
  if (sdl_info == NULL) {
    return NULL;
  }

#define SetStrItem(str) PyStructSequence_SET_ITEM(sdl_info, pos++, PyUnicode_FromString(str))

#define SetObjItem(obj) PyStructSequence_SET_ITEM(sdl_info, pos++, obj)

#ifdef WITH_SDL
  SetObjItem(PyBool_FromLong(1));

#  ifdef WITH_SDL_DYNLOAD
  if (sdlewInit() == SDLEW_SUCCESS) {
    SDL_GetVersion(&version);
    sdl_available = true;
  }
#  else /* WITH_SDL_DYNLOAD=OFF */
  sdl_available = true;
#    if SDL_MAJOR_VERSION >= 2
  SDL_GetVersion(&version);
#    else
  SDL_VERSION(&version);
#    endif
#  endif

  SetObjItem(PyC_Tuple_Pack_I32(version.major, version.minor, version.patch));
  if (sdl_available) {
    SetObjItem(PyUnicode_FromFormat("%d.%d.%d", version.major, version.minor, version.patch));
  }
  else {
    SetStrItem("Unknown");
  }
  SetObjItem(PyBool_FromLong(sdl_available));

#else /* WITH_SDL=OFF */
  SetObjItem(PyBool_FromLong(0));
  SetObjItem(PyC_Tuple_Pack_I32(0, 0, 0));
  SetStrItem("Unknown");
  SetObjItem(PyBool_FromLong(0));
#endif

  if (UNLIKELY(PyErr_Occurred())) {
    Py_DECREF(sdl_info);
    return NULL;
  }

#undef SetStrItem
#undef SetObjItem

  return sdl_info;
}

PyObject *BPY_app_sdl_struct(void)
{
  PyObject *ret;

  PyStructSequence_InitType(&BlenderAppSDLType, &app_sdl_info_desc);

  ret = make_sdl_info();

  /* prevent user from creating new instances */
  BlenderAppSDLType.tp_init = NULL;
  BlenderAppSDLType.tp_new = NULL;
  /* Without this we can't do `set(sys.modules)` #29635. */
  BlenderAppSDLType.tp_hash = (hashfunc)_Py_HashPointer;

  return ret;
}
