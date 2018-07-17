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
 *  \ingroup pygpu
 *
 * Experimental Python API, not considered public yet (called '_gpu'),
 * we may re-expose as public later.
 */

#include <Python.h>

#include "GPU_batch.h"
#include "GPU_vertex_format.h"

#include "gpu_py_api.h"
#include "gpu_py_types.h"

#include "BLI_utildefines.h"

#include "../generic/python_utildefines.h"

PyDoc_STRVAR(GPU_doc,
"This module provides access to gpu drawing functions."
);
static struct PyModuleDef GPU_module_def = {
	PyModuleDef_HEAD_INIT,
	.m_name = "_gpu",  /* m_name */
	.m_doc = GPU_doc,  /* m_doc */
};

PyObject *BPyInit_gpu(void)
{
	PyObject *sys_modules = PyThreadState_GET()->interp->modules;
	PyObject *submodule;
	PyObject *mod;

	mod = PyModule_Create(&GPU_module_def);

	/* _gpu.types */
	PyModule_AddObject(mod, "types", (submodule = BPyInit_gpu_types()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	return mod;
}
