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

/** \file blender/python/gpu/gpu_py_types.c
 *  \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_types.h" /* own include */

/* -------------------------------------------------------------------- */


/** \name GPU Types Module
 * \{ */

static struct PyModuleDef BPyGPU_types_module_def = {
	PyModuleDef_HEAD_INIT,
	.m_name = "gpu.types",
};

PyObject *BPyInit_gpu_types(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPyGPU_types_module_def);

	if (PyType_Ready(&BPyGPUVertFormat_Type) < 0)
		return NULL;
	if (PyType_Ready(&BPyGPUVertBuf_Type) < 0)
		return NULL;
	if (PyType_Ready(&BPyGPUBatch_Type) < 0)
		return NULL;
	if (PyType_Ready(&BPyGPUOffScreen_Type) < 0)
		return NULL;

#define MODULE_TYPE_ADD(s, t) \
	PyModule_AddObject(s, t.tp_name, (PyObject *)&t); Py_INCREF((PyObject *)&t)

	MODULE_TYPE_ADD(submodule, BPyGPUVertFormat_Type);
	MODULE_TYPE_ADD(submodule, BPyGPUVertBuf_Type);
	MODULE_TYPE_ADD(submodule, BPyGPUBatch_Type);
	MODULE_TYPE_ADD(submodule, BPyGPUOffScreen_Type);

#undef MODULE_TYPE_ADD

	return submodule;
}

/** \} */
