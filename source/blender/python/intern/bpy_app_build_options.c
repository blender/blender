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
 * Contributor(s): Bastien Montagne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_app_build_options.c
 *  \ingroup pythonintern
 */

#include <Python.h>
#include "BLI_utildefines.h"

#include "bpy_app_build_options.h"

static PyObject *make_build_options(void)
{
	PyObject *build_options = PyFrozenSet_New(NULL);

#define SetStrItem(str) \
	PySet_Add(build_options, PyUnicode_FromString(str));

#ifdef WITH_AUDASPACE
	SetStrItem("AUDASPACE");
#endif

#ifdef WITH_BULLET
	SetStrItem("BULLET");
#endif

#ifdef WITH_AVI
	SetStrItem("CODEC_AVI");
#endif

#ifdef WITH_FFMPEG
	SetStrItem("CODEC_FFMPEG");
#endif

#ifdef WITH_QUICKTIME
	SetStrItem("CODEC_QUICKTIME");
#endif

#ifdef WITH_SNDFILE
	SetStrItem("CODEC_SNDFILE");
#endif

#ifdef WITH_COMPOSITOR
	SetStrItem("COMPOSITOR");
#endif

#ifdef WITH_CYCLES
	SetStrItem("CYCLES");
#endif

#ifdef WITH_CYCLES_OSL
	SetStrItem("CYCLES_OSL");
#endif

#ifdef WITH_FREESTYLE
	SetStrItem("FREESTYLE");
#endif

#ifdef WITH_GAMEENGINE
	SetStrItem("GAMEENGINE");
#endif

#ifdef WITH_CINEON
	SetStrItem("IMAGE_CINEON");
#endif

#ifdef WITH_DDS
	SetStrItem("IMAGE_DDS");
#endif

#ifdef WITH_FRAMESERVER
	SetStrItem("IMAGE_FRAMESERVER");
#endif

#ifdef WITH_HDR
	SetStrItem("IMAGE_HDR");
#endif

#ifdef WITH_OPENEXR
	SetStrItem("IMAGE_OPENEXR");
#endif

#ifdef WITH_OPENJPEG
	SetStrItem("IMAGE_OPENJPEG");
#endif

#ifdef WITH_REDCODE
	SetStrItem("IMAGE_REDCODE");
#endif

#ifdef WITH_TIFF
	SetStrItem("IMAGE_TIFF");
#endif

#ifdef WITH_INPUT_NDOF
	SetStrItem("INPUT_NDOF");
#endif

#ifdef WITH_INTERNATIONAL
	SetStrItem("INTERNATIONAL");
#endif

#ifdef WITH_JACK
	SetStrItem("JACK");
#endif

#ifdef WITH_LIBMV
	SetStrItem("LIBMV");
#endif

#ifdef WITH_MOD_BOOLEAN
	SetStrItem("MOD_BOOLEAN");
#endif

#ifdef WITH_MOD_FLUID
	SetStrItem("MOD_FLUID");
#endif

#ifdef WITH_OCEANSIM
	SetStrItem("MOD_OCEANSIM");
#endif

#ifdef WITH_MOD_REMESH
	SetStrItem("MOD_REMESH");
#endif

#ifdef WITH_SMOKE
	SetStrItem("MOD_SMOKE");
#endif

#ifdef WITH_OPENAL
	SetStrItem("OPENAL");
#endif

#ifdef WITH_COLLADA
	SetStrItem("COLLADA");
#endif

#ifdef WITH_PLAYER
	SetStrItem("PLAYER");
#endif

#undef SetStrItem

	if (PyErr_Occurred()) {
		Py_CLEAR(build_options);
		return NULL;
	}

	return build_options;
}

PyObject *BPY_app_build_options_struct(void)
{
	PyObject *ret;

	ret = make_build_options();

	return ret;
}
