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
"This module provides access to the GLSL shader and Offscreen rendering functionalities."
);
static struct PyModuleDef gpumodule = {
	PyModuleDef_HEAD_INIT,
	"gpu",     /* name of module */
	M_gpu_doc, /* module documentation */
	-1,        /* size of per-interpreter state of the module,
	            *  or -1 if the module keeps state in global variables. */
	NULL, NULL, NULL, NULL, NULL
};

static PyObject *PyInit_gpu(void)
{
	PyObject *m;

	m = PyModule_Create(&gpumodule);
	if (m == NULL)
		return NULL;


	/* Take care to update docs when editing: 'doc/python_api/rst/gpu.rst' */


	/* -------------------------------------------------------------------- */
	/* GPUDynamicType */

	/* device constant groups */
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_GROUP_MISC);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_GROUP_LAMP);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_GROUP_OBJECT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_GROUP_SAMPLER);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_GROUP_MIST);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_GROUP_WORLD);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_GROUP_MAT);

	/* device constants */
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_NONE);
	/* GPU_DYNAMIC_GROUP_OBJECT */
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_OBJECT_VIEWMAT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_OBJECT_MAT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_OBJECT_VIEWIMAT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_OBJECT_IMAT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_OBJECT_LOCTOVIEWMAT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_OBJECT_LOCTOVIEWIMAT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_OBJECT_COLOR);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_OBJECT_AUTOBUMPSCALE);
	/* GPU_DYNAMIC_GROUP_LAMP */
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_DYNVEC);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_DYNCO);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_DYNIMAT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_DYNPERSMAT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_DYNENERGY);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_DYNCOL);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_ATT1);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_ATT2);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_DISTANCE);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_SPOTSIZE);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_SPOTBLEND);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_LAMP_SPOTSCALE);
	/* GPU_DYNAMIC_GROUP_SAMPLER */
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_SAMPLER_2DBUFFER);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_SAMPLER_2DIMAGE);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_SAMPLER_2DSHADOW);
	/* GPU_DYNAMIC_GROUP_MIST */
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MIST_ENABLE);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MIST_START);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MIST_DISTANCE);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MIST_INTENSITY);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MIST_TYPE);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MIST_COLOR);
	/* GPU_DYNAMIC_GROUP_WORLD */
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_HORIZON_COLOR);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_AMBIENT_COLOR);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_ZENITH_COLOR);
	/* GPU_DYNAMIC_GROUP_MAT */
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_DIFFRGB);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_REF);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_SPECRGB);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_SPEC);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_HARD);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_EMIT);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_AMB);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_ALPHA);
	PY_MODULE_ADD_CONSTANT(m, GPU_DYNAMIC_MAT_MIR);


	/* -------------------------------------------------------------------- */
	/* GPUDataType */

	PY_MODULE_ADD_CONSTANT(m, GPU_DATA_1I);
	PY_MODULE_ADD_CONSTANT(m, GPU_DATA_1F);
	PY_MODULE_ADD_CONSTANT(m, GPU_DATA_2F);
	PY_MODULE_ADD_CONSTANT(m, GPU_DATA_3F);
	PY_MODULE_ADD_CONSTANT(m, GPU_DATA_4F);
	PY_MODULE_ADD_CONSTANT(m, GPU_DATA_9F);
	PY_MODULE_ADD_CONSTANT(m, GPU_DATA_16F);
	PY_MODULE_ADD_CONSTANT(m, GPU_DATA_4UB);


	/* -------------------------------------------------------------------- */
	/* CustomDataType
	 *
	 * Intentionally only include the subset used by the GPU API.
	 */
	PY_MODULE_ADD_CONSTANT(m, CD_MTFACE);
	PY_MODULE_ADD_CONSTANT(m, CD_ORCO);
	PY_MODULE_ADD_CONSTANT(m, CD_TANGENT);
	PY_MODULE_ADD_CONSTANT(m, CD_MCOL);
	return m;
}

#define PY_DICT_ADD_STRING(d, s, f)      \
	val = PyUnicode_FromString(s->f);    \
	PyDict_SetItemString(d, # f, val);   \
	Py_DECREF(val)

#define PY_DICT_ADD_LONG(d, s, f)        \
	val = PyLong_FromLong(s->f);         \
	PyDict_SetItemString(d, # f, val);   \
	Py_DECREF(val)

#define PY_DICT_ADD_ID(d, s, f)                      \
	RNA_id_pointer_create((struct ID *)s->f, &tptr); \
	val = pyrna_struct_CreatePyObject(&tptr);        \
	PyDict_SetItemString(d, # f, val);               \
	Py_DECREF(val)

#if 0  /* UNUSED */
#define PY_OBJ_ADD_ID(d, s, f)                      \
	val = PyUnicode_FromString(&s->f->id.name[2]);  \
	PyObject_SetAttrString(d, # f, val);            \
	Py_DECREF(val)

#define PY_OBJ_ADD_LONG(d, s, f)         \
	val = PyLong_FromLong(s->f);         \
	PyObject_SetAttrString(d, # f, val); \
	Py_DECREF(val)

#define PY_OBJ_ADD_STRING(d, s, f)       \
	val = PyUnicode_FromString(s->f);    \
	PyObject_SetAttrString(d, # f, val); \
	Py_DECREF(val)
#endif

PyDoc_STRVAR(GPU_export_shader_doc,
"export_shader(scene, material)\n"
"\n"
"   Returns the GLSL shader that produces the visual effect of material in scene.\n"
"\n"
"   :return: Dictionary defining the shader, uniforms and attributes.\n"
"   :rtype: Dict"
);
static PyObject *GPU_export_shader(PyObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
	PyObject *pyscene;
	PyObject *pymat;
	PyObject *result;
	PyObject *dict;
	PyObject *val;
	PyObject *seq;

	int i;
	Scene *scene;
	PointerRNA tptr;
	Material *material;
	GPUShaderExport *shader;
	GPUInputUniform *uniform;
	GPUInputAttribute *attribute;

	static const char *kwlist[] = {"scene", "material", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO:export_shader", (char **)(kwlist), &pyscene, &pymat))
		return NULL;

	scene = (Scene *)PyC_RNA_AsPointer(pyscene, "Scene");
	if (scene == NULL) {
		return NULL;
	}

	material = (Material *)PyC_RNA_AsPointer(pymat, "Material");
	if (material == NULL) {
		return NULL;
	}

	/* we can call our internal function at last: */
	shader = GPU_shader_export(scene, material);
	if (!shader) {
		PyErr_SetString(PyExc_RuntimeError, "cannot export shader");
		return NULL;
	}
	/* build a dictionary */
	result = PyDict_New();
	if (shader->fragment) {
		PY_DICT_ADD_STRING(result, shader, fragment);
	}
	if (shader->vertex) {
		PY_DICT_ADD_STRING(result, shader, vertex);
	}
	seq = PyList_New(BLI_listbase_count(&shader->uniforms));
	for (i = 0, uniform = shader->uniforms.first; uniform; uniform = uniform->next, i++) {
		dict = PyDict_New();
		PY_DICT_ADD_STRING(dict, uniform, varname);
		PY_DICT_ADD_LONG(dict, uniform, datatype);
		PY_DICT_ADD_LONG(dict, uniform, type);
		if (uniform->lamp) {
			PY_DICT_ADD_ID(dict, uniform, lamp);
		}
		if (uniform->material) {
			PY_DICT_ADD_ID(dict, uniform, material);
		}
		if (uniform->image) {
			PY_DICT_ADD_ID(dict, uniform, image);
		}
		if (uniform->type == GPU_DYNAMIC_SAMPLER_2DBUFFER ||
		    uniform->type == GPU_DYNAMIC_SAMPLER_2DIMAGE ||
		    uniform->type == GPU_DYNAMIC_SAMPLER_2DSHADOW)
		{
			PY_DICT_ADD_LONG(dict, uniform, texnumber);
		}
		if (uniform->texpixels) {
			val = PyByteArray_FromStringAndSize((const char *)uniform->texpixels, uniform->texsize * 4);
			PyDict_SetItemString(dict, "texpixels", val);
			Py_DECREF(val);
			PY_DICT_ADD_LONG(dict, uniform, texsize);
		}
		PyList_SET_ITEM(seq, i, dict);
	}
	PyDict_SetItemString(result, "uniforms", seq);
	Py_DECREF(seq);

	seq = PyList_New(BLI_listbase_count(&shader->attributes));
	for (i = 0, attribute = shader->attributes.first; attribute; attribute = attribute->next, i++) {
		dict = PyDict_New();
		PY_DICT_ADD_STRING(dict, attribute, varname);
		PY_DICT_ADD_LONG(dict, attribute, datatype);
		PY_DICT_ADD_LONG(dict, attribute, type);
		PY_DICT_ADD_LONG(dict, attribute, number);
		if (attribute->name) {
			if (attribute->name[0] != 0) {
				PY_DICT_ADD_STRING(dict, attribute, name);
			}
			else {
				val = PyLong_FromLong(0);
				PyDict_SetItemString(dict, "name", val);
				Py_DECREF(val);
			}
		}
		PyList_SET_ITEM(seq, i, dict);
	}
	PyDict_SetItemString(result, "attributes", seq);
	Py_DECREF(seq);

	GPU_free_shader_export(shader);

	return result;
}

static PyMethodDef meth_export_shader[] = {
	{"export_shader", (PyCFunction)GPU_export_shader, METH_VARARGS | METH_KEYWORDS, GPU_export_shader_doc}
};

/* -------------------------------------------------------------------- */
/* Initialize Module */

PyObject *GPU_initPython(void)
{
	PyObject *module;
	PyObject *submodule;
	PyObject *sys_modules = PyThreadState_GET()->interp->modules;

	module = PyInit_gpu();

	PyModule_AddObject(module, "export_shader", (PyObject *)PyCFunction_New(meth_export_shader, NULL));

	/* gpu.offscreen */
	PyModule_AddObject(module, "offscreen", (submodule = BPyInit_gpu_offscreen()));
	PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
	Py_INCREF(submodule);

	PyDict_SetItem(PyImport_GetModuleDict(), PyModule_GetNameObject(module), module);
	return module;
}

