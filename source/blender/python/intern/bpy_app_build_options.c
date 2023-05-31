/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "bpy_app_build_options.h"

static PyTypeObject BlenderAppBuildOptionsType;

static PyStructSequence_Field app_builtopts_info_fields[] = {
    /* names mostly follow CMake options, lowercase, after `WITH_` */
    {"bullet", NULL},
    {"codec_avi", NULL},
    {"codec_ffmpeg", NULL},
    {"codec_sndfile", NULL},
    {"compositor_cpu", NULL},
    {"cycles", NULL},
    {"cycles_osl", NULL},
    {"freestyle", NULL},
    {"image_cineon", NULL},
    {"image_dds", NULL},
    {"image_hdr", NULL},
    {"image_openexr", NULL},
    {"image_openjpeg", NULL},
    {"image_tiff", NULL},
    {"input_ndof", NULL},
    {"audaspace", NULL},
    {"international", NULL},
    {"openal", NULL},
    {"opensubdiv", NULL},
    {"sdl", NULL},
    {"sdl_dynload", NULL},
    {"coreaudio", NULL},
    {"jack", NULL},
    {"pulseaudio", NULL},
    {"wasapi", NULL},
    {"libmv", NULL},
    {"mod_oceansim", NULL},
    {"mod_remesh", NULL},
    {"collada", NULL},
    {"io_wavefront_obj", NULL},
    {"io_ply", NULL},
    {"io_stl", NULL},
    {"io_gpencil", NULL},
    {"opencolorio", NULL},
    {"openmp", NULL},
    {"openvdb", NULL},
    {"alembic", NULL},
    {"usd", NULL},
    {"fluid", NULL},
    {"xr_openxr", NULL},
    {"potrace", NULL},
    {"pugixml", NULL},
    {"haru", NULL},
    /* Sentinel (this line prevents `clang-format` wrapping into columns). */
    {NULL},
};

static PyStructSequence_Desc app_builtopts_info_desc = {
    "bpy.app.build_options",                                                /* name */
    "This module contains information about options blender is built with", /* doc */
    app_builtopts_info_fields,                                              /* fields */
    ARRAY_SIZE(app_builtopts_info_fields) - 1,
};

static PyObject *make_builtopts_info(void)
{
  PyObject *builtopts_info;
  int pos = 0;

  builtopts_info = PyStructSequence_New(&BlenderAppBuildOptionsType);
  if (builtopts_info == NULL) {
    return NULL;
  }

#define SetObjIncref(item) \
  PyStructSequence_SET_ITEM(builtopts_info, pos++, (Py_IncRef(item), item))

#ifdef WITH_BULLET
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_AVI
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_FFMPEG
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_SNDFILE
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_COMPOSITOR_CPU
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_CYCLES
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_CYCLES_OSL
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_FREESTYLE
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_CINEON
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

  /* DDS */
  SetObjIncref(Py_True);

  /* HDR */
  SetObjIncref(Py_True);

#ifdef WITH_OPENEXR
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_OPENJPEG
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

  /* TIFF */
  SetObjIncref(Py_True);

#ifdef WITH_INPUT_NDOF
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_AUDASPACE
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_INTERNATIONAL
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_OPENAL
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_OPENSUBDIV
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_SDL
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_SDL_DYNLOAD
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_COREAUDIO
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_JACK
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_PULSEAUDIO
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_WASAPI
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_LIBMV
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_OCEANSIM
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_MOD_REMESH
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_COLLADA
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_IO_WAVEFRONT_OBJ
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_IO_PLY
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_IO_STL
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_IO_GPENCIL
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_OCIO
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef _OPENMP
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_OPENVDB
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_ALEMBIC
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_USD
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_FLUID
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_XR_OPENXR
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_POTRACE
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_PUGIXML
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#ifdef WITH_HARU
  SetObjIncref(Py_True);
#else
  SetObjIncref(Py_False);
#endif

#undef SetObjIncref

  return builtopts_info;
}

PyObject *BPY_app_build_options_struct(void)
{
  PyObject *ret;

  PyStructSequence_InitType(&BlenderAppBuildOptionsType, &app_builtopts_info_desc);

  ret = make_builtopts_info();

  /* prevent user from creating new instances */
  BlenderAppBuildOptionsType.tp_init = NULL;
  BlenderAppBuildOptionsType.tp_new = NULL;
  /* Without this we can't do `set(sys.modules)` #29635. */
  BlenderAppBuildOptionsType.tp_hash = (hashfunc)_Py_HashPointer;

  return ret;
}
