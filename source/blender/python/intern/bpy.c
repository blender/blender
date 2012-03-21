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

/** \file blender/python/intern/bpy.c
 *  \ingroup pythonintern
 *
 * This file defines the '_bpy' module which is used by python's 'bpy' package
 * to access C defined builtin functions.
 * A script writer should never directly access this module.
 */
 
#define WITH_PYTHON /* for AUD_PyInit.h, possibly others */

#include <Python.h>

#include "bpy.h" 
#include "bpy_util.h" 
#include "bpy_rna.h"
#include "bpy_app.h"
#include "bpy_props.h"
#include "bpy_operator.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_bpath.h"
#include "BLI_utildefines.h"

#include "BKE_main.h"
#include "BKE_global.h" /* XXX, G.main only */
#include "BKE_blender.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

 /* external util modules */
#include "../generic/idprop_py_api.h"
#include "../generic/bgl.h"
#include "../generic/blf_py_api.h"
#include "../mathutils/mathutils.h"

PyObject *bpy_package_py = NULL;

PyDoc_STRVAR(bpy_script_paths_doc,
".. function:: script_paths()\n"
"\n"
"   Return 2 paths to blender scripts directories.\n"
"\n"
"   :return: (system, user) strings will be empty when not found.\n"
"   :rtype: tuple of strings\n"
);
static PyObject *bpy_script_paths(PyObject *UNUSED(self))
{
	PyObject *ret = PyTuple_New(2);
	PyObject *item;
	char *path;

	path = BLI_get_folder(BLENDER_SYSTEM_SCRIPTS, NULL);
	item = PyUnicode_DecodeFSDefault(path ? path : "");
	BLI_assert(item != NULL);
	PyTuple_SET_ITEM(ret, 0, item);
	path = BLI_get_folder(BLENDER_USER_SCRIPTS, NULL);
	item = PyUnicode_DecodeFSDefault(path ? path : "");
	BLI_assert(item != NULL);
	PyTuple_SET_ITEM(ret, 1, item);

	return ret;
}

static int bpy_blend_paths_visit_cb(void *userdata, char *UNUSED(path_dst), const char *path_src)
{
	PyObject *list = (PyObject *)userdata;
	PyObject *item = PyUnicode_DecodeFSDefault(path_src);
	PyList_Append(list, item);
	Py_DECREF(item);
	return FALSE; /* never edits the path */
}

PyDoc_STRVAR(bpy_blend_paths_doc,
".. function:: blend_paths(absolute=False, packed=False, local=False)\n"
"\n"
"   Returns a list of paths to external files referenced by the loaded .blend file.\n"
"\n"
"   :arg absolute: When true the paths returned are made absolute.\n"
"   :type absolute: boolean\n"
"   :arg packed: When true skip file paths for packed data.\n"
"   :type packed: boolean\n"
"   :arg local: When true skip linked library paths.\n"
"   :type local: boolean\n"
"   :return: path list.\n"
"   :rtype: list of strings\n"
);
static PyObject *bpy_blend_paths(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	int flag = 0;
	PyObject *list;

	int absolute = FALSE;
	int packed =   FALSE;
	int local =    FALSE;
	static const char *kwlist[] = {"absolute", "packed", "local", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kw, "|ii:blend_paths",
	                                 (char **)kwlist, &absolute, &packed))
	{
		return NULL;
	}

	if (absolute) flag |= BPATH_TRAVERSE_ABS;
	if (!packed)  flag |= BPATH_TRAVERSE_SKIP_PACKED;
	if (local)    flag |= BPATH_TRAVERSE_SKIP_LIBRARY;

	list = PyList_New(0);

	bpath_traverse_main(G.main, bpy_blend_paths_visit_cb, flag, (void *)list);

	return list;
}


// PyDoc_STRVAR(bpy_user_resource_doc[] = // now in bpy/utils.py
static PyObject *bpy_user_resource(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	char *type;
	char *subdir = NULL;
	int folder_id;
	static const char *kwlist[] = {"type", "subdir", NULL};

	char *path;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|s:user_resource", (char **)kwlist, &type, &subdir))
		return NULL;
	
	/* stupid string compare */
	if      (!strcmp(type, "DATAFILES")) folder_id = BLENDER_USER_DATAFILES;
	else if (!strcmp(type, "CONFIG"))    folder_id = BLENDER_USER_CONFIG;
	else if (!strcmp(type, "SCRIPTS"))   folder_id = BLENDER_USER_SCRIPTS;
	else if (!strcmp(type, "AUTOSAVE"))  folder_id = BLENDER_USER_AUTOSAVE;
	else {
		PyErr_SetString(PyExc_ValueError, "invalid resource argument");
		return NULL;
	}
	
	/* same logic as BLI_get_folder_create(), but best leave it up to the script author to create */
	path = BLI_get_folder(folder_id, subdir);

	if (!path)
		path = BLI_get_user_folder_notest(folder_id, subdir);

	return PyUnicode_DecodeFSDefault(path ? path : "");
}

PyDoc_STRVAR(bpy_resource_path_doc,
".. function:: resource_path(type, major=2, minor=57)\n"
"\n"
"   Return the base path for storing system files.\n"
"\n"
"   :arg type: string in ['USER', 'LOCAL', 'SYSTEM'].\n"
"   :type type: string\n"
"   :arg major: major version, defaults to current.\n"
"   :type major: int\n"
"   :arg minor: minor version, defaults to current.\n"
"   :type minor: string\n"
"   :return: the resource path (not necessarily existing).\n"
"   :rtype: string\n"
);
static PyObject *bpy_resource_path(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	char *type;
	int major = BLENDER_VERSION / 100, minor = BLENDER_VERSION % 100;
	static const char *kwlist[] = {"type", "major", "minor", NULL};
	int folder_id;
	char *path;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ii:resource_path", (char **)kwlist, &type, &major, &minor))
		return NULL;

	/* stupid string compare */
	if     (!strcmp(type, "USER"))     folder_id = BLENDER_RESOURCE_PATH_USER;
	else if (!strcmp(type, "LOCAL"))   folder_id = BLENDER_RESOURCE_PATH_LOCAL;
	else if (!strcmp(type, "SYSTEM"))  folder_id = BLENDER_RESOURCE_PATH_SYSTEM;
	else {
		PyErr_SetString(PyExc_ValueError, "invalid resource argument");
		return NULL;
	}

	path = BLI_get_folder_version(folder_id, (major * 100) + minor, FALSE);

	return PyUnicode_DecodeFSDefault(path ? path : "");
}

static PyMethodDef meth_bpy_script_paths =
	{"script_paths", (PyCFunction)bpy_script_paths, METH_NOARGS, bpy_script_paths_doc};
static PyMethodDef meth_bpy_blend_paths =
	{"blend_paths", (PyCFunction)bpy_blend_paths, METH_VARARGS|METH_KEYWORDS, bpy_blend_paths_doc};
static PyMethodDef meth_bpy_user_resource =
	{"user_resource", (PyCFunction)bpy_user_resource, METH_VARARGS|METH_KEYWORDS, NULL};
static PyMethodDef meth_bpy_resource_path =
	{"resource_path", (PyCFunction)bpy_resource_path, METH_VARARGS|METH_KEYWORDS, bpy_resource_path_doc};


static PyObject *bpy_import_test(const char *modname)
{
	PyObject *mod = PyImport_ImportModuleLevel((char *)modname, NULL, NULL, NULL, 0);
	if (mod) {
		Py_DECREF(mod);
	}
	else {
		PyErr_Print();
		PyErr_Clear();
	}

	return mod;
}

/******************************************************************************
 * Description: Creates the bpy module and adds it to sys.modules for importing
 ******************************************************************************/
void BPy_init_modules(void)
{
	extern BPy_StructRNA *bpy_context_module;
	extern int bpy_lib_init(PyObject *);
	PointerRNA ctx_ptr;
	PyObject *mod;

	/* Needs to be first since this dir is needed for future modules */
	char *modpath = BLI_get_folder(BLENDER_SYSTEM_SCRIPTS, "modules");
	if (modpath) {
		// printf("bpy: found module path '%s'.\n", modpath);
		PyObject *sys_path = PySys_GetObject("path"); /* borrow */
		PyObject *py_modpath = PyUnicode_FromString(modpath);
		PyList_Insert(sys_path, 0, py_modpath); /* add first */
		Py_DECREF(py_modpath);
	}
	else {
		printf("bpy: couldnt find 'scripts/modules', blender probably wont start.\n");
	}
	/* stand alone utility modules not related to blender directly */
	IDProp_Init_Types(); /* not actually a submodule, just types */

	mod = PyModule_New("_bpy");

	/* add the module so we can import it */
	PyDict_SetItemString(PyImport_GetModuleDict(), "_bpy", mod);
	Py_DECREF(mod);

	/* run first, initializes rna types */
	BPY_rna_init();

	/* needs to be first so bpy_types can run */
	PyModule_AddObject(mod, "types", BPY_rna_types());

	/* metaclass for idprop types, bpy_types.py needs access */
	PyModule_AddObject(mod, "StructMetaPropGroup", (PyObject *)&pyrna_struct_meta_idprop_Type);

	/* needs to be first so bpy_types can run */
	bpy_lib_init(mod);

	bpy_import_test("bpy_types");
	PyModule_AddObject(mod, "data", BPY_rna_module()); /* imports bpy_types by running this */
	bpy_import_test("bpy_types");
	PyModule_AddObject(mod, "props", BPY_rna_props());	
	 /* ops is now a python module that does the conversion from SOME_OT_foo -> some.foo */
	PyModule_AddObject(mod, "ops", BPY_operator_module());
	PyModule_AddObject(mod, "app", BPY_app_struct());

	/* bpy context */
	RNA_pointer_create(NULL, &RNA_Context, (void *)BPy_GetContext(), &ctx_ptr);
	bpy_context_module = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ctx_ptr);
	/* odd that this is needed, 1 ref on creation and another for the module
	 * but without we get a crash on exit */
	Py_INCREF(bpy_context_module);

	PyModule_AddObject(mod, "context", (PyObject *)bpy_context_module);

	/* utility func's that have nowhere else to go */
	PyModule_AddObject(mod, meth_bpy_script_paths.ml_name, (PyObject *)PyCFunction_New(&meth_bpy_script_paths, NULL));
	PyModule_AddObject(mod, meth_bpy_blend_paths.ml_name, (PyObject *)PyCFunction_New(&meth_bpy_blend_paths, NULL));
	PyModule_AddObject(mod, meth_bpy_user_resource.ml_name, (PyObject *)PyCFunction_New(&meth_bpy_user_resource, NULL));
	PyModule_AddObject(mod, meth_bpy_resource_path.ml_name, (PyObject *)PyCFunction_New(&meth_bpy_resource_path, NULL));

	/* register funcs (bpy_rna.c) */
	PyModule_AddObject(mod, meth_bpy_register_class.ml_name, (PyObject *)PyCFunction_New(&meth_bpy_register_class, NULL));
	PyModule_AddObject(mod, meth_bpy_unregister_class.ml_name, (PyObject *)PyCFunction_New(&meth_bpy_unregister_class, NULL));

	/* add our own modules dir, this is a python package */
	bpy_package_py = bpy_import_test("bpy");
}
