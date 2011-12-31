/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_app_ffmpeg.c
 *  \ingroup pythonintern
 */

#include <Python.h>
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"

#include "RNA_types.h"
#include "RNA_access.h"
#include "bpy_rna.h"

#ifdef WITH_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#endif

static PyTypeObject BlenderAppFFmpegType;

#define DEF_FFMPEG_LIB_VERSION(lib) \
	{(char *)(#lib "_version"), (char *)("The " #lib " version  as a tuple of 3 numbers")}, \
	{(char *)(#lib "_version_string"), (char *)("The " #lib " version formatted as a string")},

static PyStructSequence_Field app_ffmpeg_info_fields[] = {
	{(char *)"supported", (char *)("Boolean, True when Blender is built with FFmpeg support")},

	DEF_FFMPEG_LIB_VERSION(avcodec)
	DEF_FFMPEG_LIB_VERSION(avdevice)
	DEF_FFMPEG_LIB_VERSION(avformat)
	DEF_FFMPEG_LIB_VERSION(avutil)
	DEF_FFMPEG_LIB_VERSION(swscale)
	{NULL}
};

#undef DEF_FFMPEG_LIB_VERSION

static PyStructSequence_Desc app_ffmpeg_info_desc = {
	(char *)"bpy.app.ffmpeg",     /* name */
	(char *)"This module contains information about FFmpeg blender is linked against",    /* doc */
	app_ffmpeg_info_fields,    /* fields */
	(sizeof(app_ffmpeg_info_fields) / sizeof(PyStructSequence_Field)) - 1
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

#define SetIntItem(flag) \
	PyStructSequence_SET_ITEM(ffmpeg_info, pos++, PyLong_FromLong(flag))
#define SetStrItem(str) \
	PyStructSequence_SET_ITEM(ffmpeg_info, pos++, PyUnicode_FromString(str))
#define SetObjItem(obj) \
	PyStructSequence_SET_ITEM(ffmpeg_info, pos++, obj)

#ifdef WITH_FFMPEG
	#define FFMPEG_LIB_VERSION(lib) \
		curversion = lib ## _version(); \
		SetObjItem(Py_BuildValue("(iii)", \
				curversion >> 16, (curversion >> 8) % 256, curversion % 256)); \
		SetObjItem(PyUnicode_FromFormat("%2d, %2d, %2d", \
				curversion >> 16, (curversion >> 8) % 256, curversion % 256));
#else
	#define FFMPEG_LIB_VERSION(lib) \
		SetStrItem("Unknown"); \
		SetStrItem("Unknown");
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

	if (PyErr_Occurred()) {
		Py_CLEAR(ffmpeg_info);
		return NULL;
	}

#undef SetIntItem
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
	BlenderAppFFmpegType.tp_hash = (hashfunc)_Py_HashPointer; /* without this we can't do set(sys.modules) [#29635] */

	return ret;
}
