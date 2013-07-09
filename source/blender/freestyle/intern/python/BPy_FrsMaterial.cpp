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

/** \file source/blender/freestyle/intern/python/BPy_FrsMaterial.cpp
 *  \ingroup freestyle
 */

#include "BPy_FrsMaterial.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int FrsMaterial_Init(PyObject *module)
{
	if (module == NULL)
		return -1;

	if (PyType_Ready(&FrsMaterial_Type) < 0)
		return -1;
	Py_INCREF(&FrsMaterial_Type);
	PyModule_AddObject(module, "Material", (PyObject *)&FrsMaterial_Type);

	FrsMaterial_mathutils_register_callback();

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(FrsMaterial_doc,
"Class defining a material.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: A Material object.\n"
"   :type brother: :class:`Material`\n"
"\n"
".. method:: __init__(diffuse, ambient, specular, emission, shininess)\n"
"\n"
"   Builds a Material from its diffuse, ambient, specular, emissive\n"
"   colors and a shininess coefficient.\n"
"\n"
"   :arg diffuse: The diffuse color.\n"
"   :type diffuse: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
"   :arg ambient: The ambient color.\n"
"   :type ambient: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
"   :arg specular: The specular color.\n"
"   :type specular: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
"   :arg emission: The emissive color.\n"
"   :type emission: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
"   :arg shininess: The shininess coefficient.\n"
"   :type shininess: :class:float");

static int convert_v4(PyObject *obj, void *v)
{
	return float_array_from_PyObject(obj, (float *)v, 4);
}

static int FrsMaterial_init(BPy_FrsMaterial *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist_1[] = {"brother", NULL};
	static const char *kwlist_2[] = {"diffuse", "ambient", "specular", "emission", "shininess", NULL};
	PyObject *brother = 0;
	float diffuse[4], ambient[4], specular[4], emission[4], shininess;

	if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &FrsMaterial_Type, &brother)) {
		if (!brother) {
			self->m = new FrsMaterial();
		}
		else {
			FrsMaterial *m = ((BPy_FrsMaterial *)brother)->m;
			if (!m) {
				PyErr_SetString(PyExc_RuntimeError, "invalid Material object");
				return -1;
			}
			self->m = new FrsMaterial(*m);
		}
	}
	else if (PyErr_Clear(),
	         PyArg_ParseTupleAndKeywords(args, kwds, "O&O&O&O&f", (char **)kwlist_2,
	                                     convert_v4, diffuse,
	                                     convert_v4, ambient,
	                                     convert_v4, specular,
	                                     convert_v4, emission,
	                                     &shininess))
	{
		self->m = new FrsMaterial(diffuse, ambient, specular, emission, shininess);
	}
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}
	return 0;
}

static void FrsMaterial_dealloc(BPy_FrsMaterial *self)
{
	delete self->m;
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *FrsMaterial_repr(BPy_FrsMaterial *self)
{
	return PyUnicode_FromFormat("Material - address: %p", self->m);
}

/*----------------------mathutils callbacks ----------------------------*/

/* subtype */
#define MATHUTILS_SUBTYPE_DIFFUSE   1
#define MATHUTILS_SUBTYPE_SPECULAR  2
#define MATHUTILS_SUBTYPE_AMBIENT   3
#define MATHUTILS_SUBTYPE_EMISSION  4

static int FrsMaterial_mathutils_check(BaseMathObject *bmo)
{
	if (!BPy_FrsMaterial_Check(bmo->cb_user))
		return -1;
	return 0;
}

static int FrsMaterial_mathutils_get(BaseMathObject *bmo, int subtype)
{
	BPy_FrsMaterial *self = (BPy_FrsMaterial *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_DIFFUSE:
		bmo->data[0] = self->m->diffuseR();
		bmo->data[1] = self->m->diffuseG();
		bmo->data[2] = self->m->diffuseB();
		bmo->data[3] = self->m->diffuseA();
		break;
	case MATHUTILS_SUBTYPE_SPECULAR:
		bmo->data[0] = self->m->specularR();
		bmo->data[1] = self->m->specularG();
		bmo->data[2] = self->m->specularB();
		bmo->data[3] = self->m->specularA();
		break;
	case MATHUTILS_SUBTYPE_AMBIENT:
		bmo->data[0] = self->m->ambientR();
		bmo->data[1] = self->m->ambientG();
		bmo->data[2] = self->m->ambientB();
		bmo->data[3] = self->m->ambientA();
		break;
	case MATHUTILS_SUBTYPE_EMISSION:
		bmo->data[0] = self->m->emissionR();
		bmo->data[1] = self->m->emissionG();
		bmo->data[2] = self->m->emissionB();
		bmo->data[3] = self->m->emissionA();
		break;
	default:
		return -1;
	}
	return 0;
}

static int FrsMaterial_mathutils_set(BaseMathObject *bmo, int subtype)
{
	BPy_FrsMaterial *self = (BPy_FrsMaterial *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_DIFFUSE:
		self->m->setDiffuse(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
		break;
	case MATHUTILS_SUBTYPE_SPECULAR:
		self->m->setSpecular(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
		break;
	case MATHUTILS_SUBTYPE_AMBIENT:
		self->m->setAmbient(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
		break;
	case MATHUTILS_SUBTYPE_EMISSION:
		self->m->setEmission(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
		break;
	default:
		return -1;
	}
	return 0;
}

static int FrsMaterial_mathutils_get_index(BaseMathObject *bmo, int subtype, int index)
{
	BPy_FrsMaterial *self = (BPy_FrsMaterial *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_DIFFUSE:
		{
			const float *color = self->m->diffuse();
			bmo->data[index] = color[index];
		}
		break;
	case MATHUTILS_SUBTYPE_SPECULAR:
		{
			const float *color = self->m->specular();
			bmo->data[index] = color[index];
		}
		break;
	case MATHUTILS_SUBTYPE_AMBIENT:
		{
			const float *color = self->m->ambient();
			bmo->data[index] = color[index];
		}
		break;
	case MATHUTILS_SUBTYPE_EMISSION:
		{
			const float *color = self->m->emission();
			bmo->data[index] = color[index];
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int FrsMaterial_mathutils_set_index(BaseMathObject *bmo, int subtype, int index)
{
	BPy_FrsMaterial *self = (BPy_FrsMaterial *)bmo->cb_user;
	float color[4];
	switch (subtype) {
	case MATHUTILS_SUBTYPE_DIFFUSE:
		copy_v4_v4(color, self->m->diffuse());
		color[index] = bmo->data[index];
		self->m->setDiffuse(color[0], color[1], color[2], color[3]);
		break;
	case MATHUTILS_SUBTYPE_SPECULAR:
		copy_v4_v4(color, self->m->specular());
		color[index] = bmo->data[index];
		self->m->setSpecular(color[0], color[1], color[2], color[3]);
		break;
	case MATHUTILS_SUBTYPE_AMBIENT:
		copy_v4_v4(color, self->m->ambient());
		color[index] = bmo->data[index];
		self->m->setAmbient(color[0], color[1], color[2], color[3]);
		break;
	case MATHUTILS_SUBTYPE_EMISSION:
		copy_v4_v4(color, self->m->emission());
		color[index] = bmo->data[index];
		self->m->setEmission(color[0], color[1], color[2], color[3]);
		break;
	default:
		return -1;
	}
	return 0;
}

static Mathutils_Callback FrsMaterial_mathutils_cb = {
	FrsMaterial_mathutils_check,
	FrsMaterial_mathutils_get,
	FrsMaterial_mathutils_set,
	FrsMaterial_mathutils_get_index,
	FrsMaterial_mathutils_set_index
};

static unsigned char FrsMaterial_mathutils_cb_index = -1;

void FrsMaterial_mathutils_register_callback()
{
	FrsMaterial_mathutils_cb_index = Mathutils_RegisterCallback(&FrsMaterial_mathutils_cb);
}

/*----------------------FrsMaterial get/setters ----------------------------*/

PyDoc_STRVAR(FrsMaterial_diffuse_doc,
"RGBA components of the diffuse color of the material.\n"
"\n"
":type: mathutils.Vector");

static PyObject *FrsMaterial_diffuse_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_DIFFUSE);
}

static int FrsMaterial_diffuse_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float color[4];
	if (!float_array_from_PyObject(value, color, 4)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 4-dimensional vector");
		return -1;
	}
	self->m->setDiffuse(color[0], color[1], color[2], color[3]);
	return 0;
}

PyDoc_STRVAR(FrsMaterial_specular_doc,
"RGBA components of the specular color of the material.\n"
"\n"
":type: mathutils.Vector");

static PyObject *FrsMaterial_specular_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_SPECULAR);
}

static int FrsMaterial_specular_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float color[4];
	if (!float_array_from_PyObject(value, color, 4)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 4-dimensional vector");
		return -1;
	}
	self->m->setSpecular(color[0], color[1], color[2], color[3]);
	return 0;
}

PyDoc_STRVAR(FrsMaterial_ambient_doc,
"RGBA components of the ambient color of the material.\n"
"\n"
":type: mathutils.Color");

static PyObject *FrsMaterial_ambient_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_AMBIENT);
}

static int FrsMaterial_ambient_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float color[4];
	if (!float_array_from_PyObject(value, color, 4)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 4-dimensional vector");
		return -1;
	}
	self->m->setAmbient(color[0], color[1], color[2], color[3]);
	return 0;
}

PyDoc_STRVAR(FrsMaterial_emission_doc,
"RGBA components of the emissive color of the material.\n"
"\n"
":type: mathutils.Color");

static PyObject *FrsMaterial_emission_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_EMISSION);
}

static int FrsMaterial_emission_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float color[4];
	if (!float_array_from_PyObject(value, color, 4)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 4-dimensional vector");
		return -1;
	}
	self->m->setEmission(color[0], color[1], color[2], color[3]);
	return 0;
}

PyDoc_STRVAR(FrsMaterial_shininess_doc,
"Shininess coefficient of the material.\n"
"\n"
":type: float");

static PyObject *FrsMaterial_shininess_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->m->shininess());
}

static int FrsMaterial_shininess_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float scalar;
	if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "value must be a number");
		return -1;
	}
	self->m->setShininess(scalar);
	return 0;
}

static PyGetSetDef BPy_FrsMaterial_getseters[] = {
	{(char *)"diffuse", (getter)FrsMaterial_diffuse_get, (setter)FrsMaterial_diffuse_set,
	                    (char *)FrsMaterial_diffuse_doc, NULL},
	{(char *)"specular", (getter)FrsMaterial_specular_get, (setter)FrsMaterial_specular_set,
	                     (char *)FrsMaterial_specular_doc, NULL},
	{(char *)"ambient", (getter)FrsMaterial_ambient_get, (setter)FrsMaterial_ambient_set,
	                    (char *)FrsMaterial_ambient_doc, NULL},
	{(char *)"emission", (getter)FrsMaterial_emission_get, (setter)FrsMaterial_emission_set,
	                     (char *)FrsMaterial_emission_doc, NULL},
	{(char *)"shininess", (getter)FrsMaterial_shininess_get, (setter)FrsMaterial_shininess_set,
	                      (char *)FrsMaterial_shininess_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_FrsMaterial type definition ------------------------------*/

PyTypeObject FrsMaterial_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Material",                     /* tp_name */
	sizeof(BPy_FrsMaterial),        /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)FrsMaterial_dealloc, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)FrsMaterial_repr,     /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	FrsMaterial_doc,                /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	BPy_FrsMaterial_getseters,      /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)FrsMaterial_init,     /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
