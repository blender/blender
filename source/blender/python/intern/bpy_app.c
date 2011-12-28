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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_app.c
 *  \ingroup pythonintern
 *
 * This file defines a 'PyStructSequence' accessed via 'bpy.app', mostly
 * exposing static applications variables such as version and buildinfo
 * however some writable variables have been added such as 'debug' and 'tempdir'
 */


#include <Python.h>

#include "bpy_app.h"

#include "bpy_app_ffmpeg.h"

#include "bpy_app_handlers.h"
#include "bpy_driver.h"

#include "BLI_path_util.h"
#include "BLI_utildefines.h"


#include "BKE_blender.h"
#include "BKE_global.h"
#include "structseq.h"

#include "../generic/py_capi_utils.h"

#ifdef BUILD_DATE
extern char build_date[];
extern char build_time[];
extern char build_rev[];
extern char build_platform[];
extern char build_type[];
extern char build_cflags[];
extern char build_cxxflags[];
extern char build_linkflags[];
extern char build_system[];
#endif

static PyTypeObject BlenderAppType;

static PyStructSequence_Field app_info_fields[] = {
	{(char *)"version", (char *)"The Blender version as a tuple of 3 numbers. eg. (2, 50, 11)"},
	{(char *)"version_string", (char *)"The Blender version formatted as a string"},
	{(char *)"version_char", (char *)"The Blender version character (for minor releases)"},
	{(char *)"version_cycle", (char *)"The release status of this build alpha/beta/rc/release"},
	{(char *)"binary_path", (char *)"The location of blenders executable, useful for utilities that spawn new instances"},
	{(char *)"background", (char *)"Boolean, True when blender is running without a user interface (started with -b)"},

	/* buildinfo */
	{(char *)"build_date", (char *)"The date this blender instance was built"},
	{(char *)"build_time", (char *)"The time this blender instance was built"},
	{(char *)"build_revision", (char *)"The subversion revision this blender instance was built with"},
	{(char *)"build_platform", (char *)"The platform this blender instance was built for"},
	{(char *)"build_type", (char *)"The type of build (Release, Debug)"},
	{(char *)"build_cflags", (char *)"C compiler flags"},
	{(char *)"build_cxxflags", (char *)"C++ compiler flags"},
	{(char *)"build_linkflags", (char *)"Binary linking flags"},
	{(char *)"build_system", (char *)"Build system used"},

	/* submodules */
	{(char *)"ffmpeg", (char *)"FFmpeg library information backend"},
	{(char *)"handlers", (char *)"Application handler callbacks"},
	{NULL}
};

static PyStructSequence_Desc app_info_desc = {
	(char *)"bpy.app",     /* name */
	(char *)"This module contains application values that remain unchanged during runtime.",    /* doc */
	app_info_fields,    /* fields */
	(sizeof(app_info_fields) / sizeof(PyStructSequence_Field)) - 1
};

#define DO_EXPAND(VAL)  VAL ## 1
#define EXPAND(VAL)     DO_EXPAND(VAL)

static PyObject *make_app_info(void)
{
	PyObject *app_info;
	int pos = 0;

	app_info = PyStructSequence_New(&BlenderAppType);
	if (app_info == NULL) {
		return NULL;
	}

#define SetIntItem(flag) \
	PyStructSequence_SET_ITEM(app_info, pos++, PyLong_FromLong(flag))
#define SetStrItem(str) \
	PyStructSequence_SET_ITEM(app_info, pos++, PyUnicode_FromString(str))
#define SetObjItem(obj) \
	PyStructSequence_SET_ITEM(app_info, pos++, obj)

	SetObjItem(Py_BuildValue("(iii)",
	                         BLENDER_VERSION / 100, BLENDER_VERSION % 100, BLENDER_SUBVERSION));
	SetObjItem(PyUnicode_FromFormat("%d.%02d (sub %d)",
	                                BLENDER_VERSION / 100, BLENDER_VERSION % 100, BLENDER_SUBVERSION));

#if defined(BLENDER_VERSION_CHAR) && EXPAND(BLENDER_VERSION_CHAR) != 1
	SetStrItem(STRINGIFY(BLENDER_VERSION_CHAR));
#else
	SetStrItem("");
#endif
	SetStrItem(STRINGIFY(BLENDER_VERSION_CYCLE));
	SetStrItem(BLI_program_path());
	SetObjItem(PyBool_FromLong(G.background));

	/* build info */
#ifdef BUILD_DATE
	SetStrItem(build_date);
	SetStrItem(build_time);
	SetStrItem(build_rev);
	SetStrItem(build_platform);
	SetStrItem(build_type);
	SetStrItem(build_cflags);
	SetStrItem(build_cxxflags);
	SetStrItem(build_linkflags);
	SetStrItem(build_system);
#else
	SetStrItem("Unknown");
	SetStrItem("Unknown");
	SetStrItem("Unknown");
	SetStrItem("Unknown");
	SetStrItem("Unknown");
	SetStrItem("Unknown");
	SetStrItem("Unknown");
	SetStrItem("Unknown");
	SetStrItem("Unknown");
#endif

	SetObjItem(BPY_app_ffmpeg_struct());
	SetObjItem(BPY_app_handlers_struct());

#undef SetIntItem
#undef SetStrItem
#undef SetObjItem

	if (PyErr_Occurred()) {
		Py_CLEAR(app_info);
		return NULL;
	}
	return app_info;
}

/* a few getsets because it makes sense for them to be in bpy.app even though
 * they are not static */

PyDoc_STRVAR(bpy_app_debug_doc,
"Boolean, set when blender is running in debug mode (started with --debug)"
);
static PyObject *bpy_app_debug_get(PyObject *UNUSED(self), void *UNUSED(closure))
{
	return PyBool_FromLong(G.f & G_DEBUG);
}

static int bpy_app_debug_set(PyObject *UNUSED(self), PyObject *value, void *UNUSED(closure))
{
	int param = PyObject_IsTrue(value);

	if (param < 0) {
		PyErr_SetString(PyExc_TypeError, "bpy.app.debug can only be True/False");
		return -1;
	}
	
	if (param)  G.f |=  G_DEBUG;
	else        G.f &= ~G_DEBUG;
	
	return 0;
}

PyDoc_STRVAR(bpy_app_debug_value_doc,
"Int, number which can be set to non-zero values for testing purposes"
);
static PyObject *bpy_app_debug_value_get(PyObject *UNUSED(self), void *UNUSED(closure))
{
	return PyLong_FromSsize_t(G.rt);
}

static int bpy_app_debug_value_set(PyObject *UNUSED(self), PyObject *value, void *UNUSED(closure))
{
	int param = PyLong_AsSsize_t(value);

	if (param == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "bpy.app.debug_value can only be set to a whole number");
		return -1;
	}
	
	G.rt = param;

	return 0;
}

PyDoc_STRVAR(bpy_app_tempdir_doc,
"String, the temp directory used by blender (read-only)"
);
static PyObject *bpy_app_tempdir_get(PyObject *UNUSED(self), void *UNUSED(closure))
{
	return PyC_UnicodeFromByte(BLI_temporary_dir());
}

PyDoc_STRVAR(bpy_app_driver_dict_doc,
"Dictionary for drivers namespace, editable in-place, reset on file load (read-only)"
);
static PyObject *bpy_app_driver_dict_get(PyObject *UNUSED(self), void *UNUSED(closure))
{
	if (bpy_pydriver_Dict == NULL)
		if (bpy_pydriver_create_dict() != 0) {
			PyErr_SetString(PyExc_RuntimeError, "bpy.app.driver_namespace failed to create dictionary");
			return NULL;
	}

	Py_INCREF(bpy_pydriver_Dict);
	return bpy_pydriver_Dict;
}


static PyGetSetDef bpy_app_getsets[] = {
	{(char *)"debug", bpy_app_debug_get, bpy_app_debug_set, (char *)bpy_app_debug_doc, NULL},
	{(char *)"debug_value", bpy_app_debug_value_get, bpy_app_debug_value_set, (char *)bpy_app_debug_value_doc, NULL},
	{(char *)"tempdir", bpy_app_tempdir_get, NULL, (char *)bpy_app_tempdir_doc, NULL},
	{(char *)"driver_namespace", bpy_app_driver_dict_get, NULL, (char *)bpy_app_driver_dict_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}
};

static void py_struct_seq_getset_init(void)
{
	/* tricky dynamic members, not to py-spec! */
	PyGetSetDef *getset;

	for (getset = bpy_app_getsets; getset->name; getset++) {
		PyDict_SetItemString(BlenderAppType.tp_dict, getset->name, PyDescr_NewGetSet(&BlenderAppType, getset));
	}
}
/* end dynamic bpy.app */


PyObject *BPY_app_struct(void)
{
	PyObject *ret;
	
	PyStructSequence_InitType(&BlenderAppType, &app_info_desc);

	ret = make_app_info();

	/* prevent user from creating new instances */
	BlenderAppType.tp_init = NULL;
	BlenderAppType.tp_new = NULL;
	BlenderAppType.tp_hash = (hashfunc)_Py_HashPointer; /* without this we can't do set(sys.modules) [#29635] */

	/* kindof a hack ontop of PyStructSequence */
	py_struct_seq_getset_init();

	return ret;
}
