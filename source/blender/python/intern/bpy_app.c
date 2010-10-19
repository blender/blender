/**
 * $Id$
 *
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

#include "bpy_app.h"

#include "BLI_path_util.h"

#include "BKE_utildefines.h"
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
	{"version", "The Blender version as a tuple of 3 numbers. eg. (2, 50, 11)"},
	{"version_string", "The Blender version formatted as a string"},
	{"binary_path", "The location of blenders executable, useful for utilities that spawn new instances"},
	{"background", "Boolean, True when blender is running without a user interface (started with -b)"},

	/* buildinfo */
	{"build_date", "The date this blender instance was built"},
	{"build_time", "The time this blender instance was built"},
	{"build_revision", "The subversion revision this blender instance was built with"},
	{"build_platform", "The platform this blender instance was built for"},
	{"build_type", "The type of build (Release, Debug)"},
	{"build_cflags", ""},
	{"build_cxxflags", ""},
	{"build_linkflags", ""},
	{"build_system", ""},
	{0}
};

static PyStructSequence_Desc app_info_desc = {
	"bpy.app",     /* name */
	"This module contains application values that remain unchanged during runtime.",    /* doc */
	app_info_fields,    /* fields */
	(sizeof(app_info_fields)/sizeof(PyStructSequence_Field)) - 1
};

static PyObject *make_app_info(void)
{
	extern char bprogname[]; /* argv[0] from creator.c */

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

	SetObjItem(Py_BuildValue("(iii)", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION));
	SetObjItem(PyUnicode_FromFormat("%d.%02d (sub %d)", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION));
	SetStrItem(bprogname);
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
static PyObject *bpy_app_debug_get(PyObject *UNUSED(self), void *UNUSED(closure))
{
	return PyBool_FromLong(G.f & G_DEBUG);
}

static int bpy_app_debug_set(PyObject *UNUSED(self), PyObject *value, void *UNUSED(closure))
{
	int param= PyObject_IsTrue(value);

	if(param < 0) {
		PyErr_SetString(PyExc_TypeError, "bpy.app.debug can only be True/False");
		return -1;
	}
	
	if(param)	G.f |=  G_DEBUG;
	else		G.f &= ~G_DEBUG;
	
	return 0;
}

static PyObject *bpy_app_tempdir_get(PyObject *UNUSED(self), void *UNUSED(closure))
{
	extern char btempdir[];
	return PyC_UnicodeFromByte(btempdir);
}

PyGetSetDef bpy_app_debug_getset= {"debug", bpy_app_debug_get, bpy_app_debug_set, "Boolean, set when blender is running in debug mode (started with -d)", NULL};
PyGetSetDef bpy_app_tempdir_getset= {"tempdir", bpy_app_tempdir_get, NULL, "String, the temp directory used by blender (read-only)", NULL};

static void py_struct_seq_getset_init(void)
{
	/* tricky dynamic members, not to py-spec! */
	
	PyDict_SetItemString(BlenderAppType.tp_dict, bpy_app_debug_getset.name, PyDescr_NewGetSet(&BlenderAppType, &bpy_app_debug_getset));
	PyDict_SetItemString(BlenderAppType.tp_dict, bpy_app_tempdir_getset.name, PyDescr_NewGetSet(&BlenderAppType, &bpy_app_tempdir_getset));
}
/* end dynamic bpy.app */


PyObject *BPY_app_struct(void)
{
	PyObject *ret;
	
	PyStructSequence_InitType(&BlenderAppType, &app_info_desc);

	ret= make_app_info();

	/* prevent user from creating new instances */
	BlenderAppType.tp_init = NULL;
	BlenderAppType.tp_new = NULL;

	/* kindof a hack ontop of PyStructSequence */
	py_struct_seq_getset_init();

	return ret;
}

