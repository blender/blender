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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/gpu/gpu_py_api.c
 *  \ingroup bpygpu
 *
 * Experimental Python API, not considered public yet (called '_gpu'),
 * we may re-expose as public later.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "../generic/python_utildefines.h"

#include "gpu_py_matrix.h"
#include "gpu_py_select.h"
#include "gpu_py_types.h"

#include "gpu_py_api.h" /* own include */

PyDoc_STRVAR(GPU_doc,
"This module to provide functions concerning the GPU implementation in Blender."
"\n\n"
"Submodules:\n"
"\n"
".. toctree::\n"
"   :maxdepth: 1\n"
"\n"
"   gpu.types.rst\n"
"   gpu.matrix.rst\n"
"   gpu.select.rst\n"
"   gpu.shader.rst\n"
"\n"
);
static struct PyModuleDef GPU_module_def = {
	PyModuleDef_HEAD_INIT,
	.m_name = "gpu",
	.m_doc = GPU_doc,
};

PyObject *BPyInit_gpu(void)
{
	PyObject *sys_modules = PyImport_GetModuleDict();
	PyObject *subsubmodule;
	PyObject *submodule;
	PyObject *mod;

	mod = PyModule_Create(&GPU_module_def);

	PyModule_AddObject(mod, "types", (submodule = BPyInit_gpu_types()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(mod, "matrix", (submodule = BPyInit_gpu_matrix()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(mod, "select", (submodule = BPyInit_gpu_select()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(mod, "shader", (submodule = BPyInit_gpu_shader()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(submodule, "builtin", (subsubmodule = BPyInit_gpu_shader_builtin()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(subsubmodule), subsubmodule);
	Py_INCREF(subsubmodule);

	return mod;
}
