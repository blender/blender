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
 
/* This file defines the '_bpy' module which is used by python's 'bpy' package.
 * a script writer should never directly access this module */
 

#include "bpy_util.h" 
#include "bpy_rna.h"
#include "bpy_app.h"
#include "bpy_props.h"
#include "bpy_operator.h"
 
#include "BLI_path_util.h"
 
 /* external util modules */
#include "../generic/Geometry.h"
#include "../generic/bgl.h"
#include "../generic/blf.h"
#include "../generic/IDProp.h"

static char bpy_home_paths_doc[] =
".. function:: home_paths(subfolder)\n"
"\n"
"   return 3 paths to blender home directories.\n"
"\n"
"   :arg subfolder: The name of a subfolder to find within the blenders home directory.\n"
"   :type subfolder: string\n"
"   :return: (system, local, user) strings will be empty when not found.\n"
"   :rtype: tuple of strigs\n";

PyObject *bpy_home_paths(PyObject *self, PyObject *args)
{
	PyObject *ret= PyTuple_New(3);
	char *path;
	char *subfolder= "";
    
	if (!PyArg_ParseTuple(args, "|s:blender_homes", &subfolder))
		return NULL;

	path= BLI_gethome_folder(subfolder, BLI_GETHOME_SYSTEM);
	PyTuple_SET_ITEM(ret, 0, PyUnicode_FromString(path?path:""));

	path= BLI_gethome_folder(subfolder, BLI_GETHOME_LOCAL);
	PyTuple_SET_ITEM(ret, 1, PyUnicode_FromString(path?path:""));

	path= BLI_gethome_folder(subfolder, BLI_GETHOME_USER);
	PyTuple_SET_ITEM(ret, 2, PyUnicode_FromString(path?path:""));
    
	return ret;
}

static PyMethodDef meth_bpy_home_paths[] = {{ "home_paths", (PyCFunction)bpy_home_paths, METH_VARARGS, bpy_home_paths_doc}};

static void bpy_import_test(char *modname)
{
	PyObject *mod= PyImport_ImportModuleLevel(modname, NULL, NULL, NULL, 0);
	if(mod) {
		Py_DECREF(mod);
	}
	else {
		PyErr_Print();
		PyErr_Clear();
	}	
}

/*****************************************************************************
* Description: Creates the bpy module and adds it to sys.modules for importing
*****************************************************************************/
void BPy_init_modules( void )
{
	extern BPy_StructRNA *bpy_context_module;
	PyObject *mod;

	/* Needs to be first since this dir is needed for future modules */
	char *modpath= BLI_gethome_folder("scripts/modules", BLI_GETHOME_ALL);
	if(modpath) {
		PyObject *sys_path= PySys_GetObject("path"); /* borrow */
		PyObject *py_modpath= PyUnicode_FromString(modpath);
		PyList_Insert(sys_path, 0, py_modpath); /* add first */
		Py_DECREF(py_modpath);
	}
	
	/* stand alone utility modules not related to blender directly */
	Geometry_Init();
	Mathutils_Init();
	BGL_Init();
	BLF_Init();
	IDProp_Init_Types();


	mod = PyModule_New("_bpy");

	/* add the module so we can import it */
	PyDict_SetItemString(PySys_GetObject("modules"), "_bpy", mod);
	Py_DECREF(mod);

	/* run first, initializes rna types */
	BPY_rna_init();

	PyModule_AddObject( mod, "types", BPY_rna_types() ); /* needs to be first so bpy_types can run */
	bpy_import_test("bpy_types");
	PyModule_AddObject( mod, "data", BPY_rna_module() ); /* imports bpy_types by running this */
	bpy_import_test("bpy_types");
	PyModule_AddObject( mod, "props", BPY_rna_props() );
	PyModule_AddObject( mod, "ops", BPY_operator_module() ); /* ops is now a python module that does the conversion from SOME_OT_foo -> some.foo */
	PyModule_AddObject( mod, "app", BPY_app_struct() );

	/* bpy context */
	bpy_context_module= ( BPy_StructRNA * ) PyObject_NEW( BPy_StructRNA, &pyrna_struct_Type );
	RNA_pointer_create(NULL, &RNA_Context, BPy_GetContext(), &bpy_context_module->ptr);
	bpy_context_module->freeptr= 0;
	PyModule_AddObject(mod, "context", (PyObject *)bpy_context_module);

	/* utility func's that have nowhere else to go */
	PyModule_AddObject(mod, meth_bpy_home_paths->ml_name, (PyObject *)PyCFunction_New(meth_bpy_home_paths, NULL));

	/* add our own modules dir, this is a python package */
	bpy_import_test("bpy");
}
