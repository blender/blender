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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Benoit Bolsee.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/gpu.c
 *  \ingroup pythonintern
 *
 * This file defines the 'gpu' module, used to get GLSL shader code and data
 * from blender materials.
 */

#include <Python.h>

#include "DNA_scene_types.h"
#include "DNA_material_types.h"
#include "DNA_ID.h"
#include "DNA_customdata_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "RNA_access.h"

#include "bpy_rna.h"

#include "../generic/py_capi_utils.h"

#include "GPU_material.h"

#include "gpu.h"

#define PY_MODULE_ADD_CONSTANT(module, name) PyModule_AddIntConstant(module, # name, name)

PyDoc_STRVAR(M_gpu_doc,
"This module provides access to GPU offscreen rendering, matrix stacks and selection."
);
static struct PyModuleDef gpumodule = {
	PyModuleDef_HEAD_INIT,
	"gpu",     /* name of module */
	M_gpu_doc, /* module documentation */
	-1,        /* size of per-interpreter state of the module,
	            * or -1 if the module keeps state in global variables. */
	NULL, NULL, NULL, NULL, NULL
};

static PyObject *PyInit_gpu(void)
{
	PyObject *m;

	m = PyModule_Create(&gpumodule);
	if (m == NULL)
		return NULL;

	/* Take care to update docs when editing: 'doc/python_api/rst/gpu.rst' */
	return m;
}

/* -------------------------------------------------------------------- */
/* Initialize Module */

PyObject *GPU_initPython(void)
{
	PyObject *module;
	PyObject *submodule;
	PyObject *sys_modules = PyThreadState_GET()->interp->modules;

	module = PyInit_gpu();

	/* gpu.offscreen */
	PyModule_AddObject(module, "offscreen", (submodule = BPyInit_gpu_offscreen()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(module, "matrix", (submodule = BPyInit_gpu_matrix()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(module, "select", (submodule = BPyInit_gpu_select()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	PyDict_SetItem(PyImport_GetModuleDict(), PyModule_GetNameObject(module), module);
	return module;
}
