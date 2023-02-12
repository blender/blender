/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_utildefines.h"
#include <Python.h>

#include "bpy_app_ffmpeg.h"

#include "../generic/py_capi_utils.h"

#ifdef WITH_FFMPEG
#  include <libavcodec/avcodec.h>
#  include <libavdevice/avdevice.h>
#  include <libavformat/avformat.h>
#  include <libavutil/avutil.h>
#  include <libswscale/swscale.h>
#endif

static PyTypeObject BlenderAppFFmpegType;

#define DEF_FFMPEG_LIB_VERSION(lib) \
  {(#lib "_version"), ("The " #lib " version as a tuple of 3 numbers")}, \
  { \
    (#lib "_version_string"), ("The " #lib " version formatted as a string") \
  }

static PyStructSequence_Field app_ffmpeg_info_fields[] = {
    {"supported", "Boolean, True when Blender is built with FFmpeg support"},

    DEF_FFMPEG_LIB_VERSION(avcodec),
    DEF_FFMPEG_LIB_VERSION(avdevice),
    DEF_FFMPEG_LIB_VERSION(avformat),
    DEF_FFMPEG_LIB_VERSION(avutil),
    DEF_FFMPEG_LIB_VERSION(swscale),
    {NULL},
};

#undef DEF_FFMPEG_LIB_VERSION

static PyStructSequence_Desc app_ffmpeg_info_desc = {
    "bpy.app.ffmpeg",                                                          /* name */
    "This module contains information about FFmpeg blender is linked against", /* doc */
    app_ffmpeg_info_fields,                                                    /* fields */
    ARRAY_SIZE(app_ffmpeg_info_fields) - 1,
};

static PyObject *make_ffmpeg_info(void)
{
  PyObject *ffmpeg_info;
  int pos = 0;

#ifdef WITH_FFMPEG
  int curversion;
#endif

  ffmpeg_info = PyStructSequence_New(&BlenderAppFFmpegType);
  if (ffmpeg_info == NULL) {
    return NULL;
  }

#if 0 /* UNUSED */
#  define SetIntItem(flag) PyStructSequence_SET_ITEM(ffmpeg_info, pos++, PyLong_FromLong(flag))
#endif
#ifndef WITH_FFMPEG
#  define SetStrItem(str) PyStructSequence_SET_ITEM(ffmpeg_info, pos++, PyUnicode_FromString(str))
#endif
#define SetObjItem(obj) PyStructSequence_SET_ITEM(ffmpeg_info, pos++, obj)

#ifdef WITH_FFMPEG
#  define FFMPEG_LIB_VERSION(lib) \
    { \
      curversion = lib##_version(); \
      SetObjItem( \
          PyC_Tuple_Pack_I32(curversion >> 16, (curversion >> 8) % 256, curversion % 256)); \
      SetObjItem(PyUnicode_FromFormat( \
          "%2d, %2d, %2d", curversion >> 16, (curversion >> 8) % 256, curversion % 256)); \
    } \
    (void)0
#else
#  define FFMPEG_LIB_VERSION(lib) \
    { \
      SetStrItem("Unknown"); \
      SetStrItem("Unknown"); \
    } \
    (void)0
#endif

#ifdef WITH_FFMPEG
  SetObjItem(PyBool_FromLong(1));
#else
  SetObjItem(PyBool_FromLong(0));
#endif

  FFMPEG_LIB_VERSION(avcodec);
  FFMPEG_LIB_VERSION(avdevice);
  FFMPEG_LIB_VERSION(avformat);
  FFMPEG_LIB_VERSION(avutil);
  FFMPEG_LIB_VERSION(swscale);

#undef FFMPEG_LIB_VERSION

  if (UNLIKELY(PyErr_Occurred())) {
    Py_DECREF(ffmpeg_info);
    return NULL;
  }

// #undef SetIntItem
#undef SetStrItem
#undef SetObjItem

  return ffmpeg_info;
}

PyObject *BPY_app_ffmpeg_struct(void)
{
  PyObject *ret;

  PyStructSequence_InitType(&BlenderAppFFmpegType, &app_ffmpeg_info_desc);

  ret = make_ffmpeg_info();

  /* prevent user from creating new instances */
  BlenderAppFFmpegType.tp_init = NULL;
  BlenderAppFFmpegType.tp_new = NULL;
  BlenderAppFFmpegType.tp_hash = (hashfunc)
      _Py_HashPointer; /* without this we can't do set(sys.modules) #29635. */

  return ret;
}
