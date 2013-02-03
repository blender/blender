#include "BPy_FrsMaterial.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_math.h"

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int FrsMaterial_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &FrsMaterial_Type ) < 0 )
		return -1;

	Py_INCREF( &FrsMaterial_Type );
	PyModule_AddObject(module, "Material", (PyObject *)&FrsMaterial_Type);

	FrsMaterial_mathutils_register_callback();
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char FrsMaterial___doc__[] =
"Class defining a material.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(m)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg m: A Material object.\n"
"   :type m: :class:`Material`\n"
"\n"
".. method:: __init__(iDiffuse, iAmbiant, iSpecular, iEmission, iShininess)\n"
"\n"
"   Builds a Material from its diffuse, ambiant, specular, emissive\n"
"   colors and a shininess coefficient.\n"
"\n"
"   :arg iDiffuse: The diffuse color.\n"
"   :type iDiffuse: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
"   :arg iAmbiant: The ambiant color.\n"
"   :type iAmbiant: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
"   :arg iSpecular: The specular color.\n"
"   :type iSpecular: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
"   :arg iEmission: The emissive color.\n"
"   :type iEmission: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
"   :arg iShininess: The shininess coefficient.\n"
"   :type iShininess: :class:float\n";

static int Vec4(PyObject *obj, float *v)
{
	if (VectorObject_Check(obj) && ((VectorObject *)obj)->size == 4) {
		for (int i = 0; i < 4; i++)
			v[i] = ((VectorObject *)obj)->vec[i];
	} else if( PyList_Check(obj) && PyList_Size(obj) == 4 ) {
		for (int i = 0; i < 4; i++)
			v[i] = PyFloat_AsDouble(PyList_GetItem(obj, i));
	} else if( PyTuple_Check(obj) && PyTuple_Size(obj) == 4 ) {
		for (int i = 0; i < 4; i++)
			v[i] = PyFloat_AsDouble(PyTuple_GetItem(obj, i));
	} else {
		return 0;
	}
	return 1;
}

static int FrsMaterial___init__(BPy_FrsMaterial *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0;
	float f1[4], f2[4], f3[4], f4[4], f5 = 0.;

    if (! PyArg_ParseTuple(args, "|OOOOf", &obj1, &obj2, &obj3, &obj4, &f5) )
        return -1;

	if( !obj1 ){
		self->m = new FrsMaterial();

	} else if( BPy_FrsMaterial_Check(obj1) && !obj2 ) {
		FrsMaterial *m = ((BPy_FrsMaterial *) obj1)->m;
		if( !m ) {
			PyErr_SetString(PyExc_RuntimeError, "invalid FrsMaterial object");
			return -1;
		}
		self->m = new FrsMaterial( *m );

	} else if( Vec4(obj1, f1) && obj2 && Vec4(obj2, f2) && obj3 && Vec4(obj3, f3) && obj4 && Vec4(obj4, f4) ) {
		self->m = new FrsMaterial(f1, f2, f3, f4, f5);

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}
	self->borrowed = 0;

	return 0;
}

static void FrsMaterial___dealloc__( BPy_FrsMaterial* self)
{
	if( self->m && !self->borrowed )
		delete self->m;
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject * FrsMaterial___repr__( BPy_FrsMaterial* self)
{
    return PyUnicode_FromFormat("Material - address: %p", self->m );
}

/*----------------------FrsMaterial instance definitions ----------------------------*/
static PyMethodDef BPy_FrsMaterial_methods[] = {
	{NULL, NULL, 0, NULL}
};

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
":type: mathutils.Vector"
);

static PyObject *FrsMaterial_diffuse_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_DIFFUSE);
}

static int FrsMaterial_diffuse_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float color[4];
	if (!Vec4((PyObject *)value, color)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 4-dimensional vector");
		return -1;
	}
	self->m->setDiffuse(color[0], color[1], color[2], color[3]);
	return 0;
}

PyDoc_STRVAR(FrsMaterial_specular_doc,
"RGBA components of the specular color of the material.\n"
"\n"
":type: mathutils.Vector"
);

static PyObject *FrsMaterial_specular_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_SPECULAR);
}

static int FrsMaterial_specular_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float color[4];
	if (!Vec4((PyObject *)value, color)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 4-dimensional vector");
		return -1;
	}
	self->m->setSpecular(color[0], color[1], color[2], color[3]);
	return 0;
}

PyDoc_STRVAR(FrsMaterial_ambient_doc,
"RGBA components of the ambient color of the material.\n"
"\n"
":type: mathutils.Color"
);

static PyObject *FrsMaterial_ambient_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_AMBIENT);
}

static int FrsMaterial_ambient_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float color[4];
	if (!Vec4((PyObject *)value, color)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 4-dimensional vector");
		return -1;
	}
	self->m->setAmbient(color[0], color[1], color[2], color[3]);
	return 0;
}

PyDoc_STRVAR(FrsMaterial_emission_doc,
"RGBA components of the emissive color of the material.\n"
"\n"
":type: mathutils.Color"
);

static PyObject *FrsMaterial_emission_get(BPy_FrsMaterial *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_EMISSION);
}

static int FrsMaterial_emission_set(BPy_FrsMaterial *self, PyObject *value, void *UNUSED(closure))
{
	float color[4];
	if (!Vec4((PyObject *)value, color)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 4-dimensional vector");
		return -1;
	}
	self->m->setEmission(color[0], color[1], color[2], color[3]);
	return 0;
}

PyDoc_STRVAR(FrsMaterial_shininess_doc,
"Shininess coefficient of the material.\n"
"\n"
":type: float"
);

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
	{(char *)"diffuse", (getter)FrsMaterial_diffuse_get, (setter)FrsMaterial_diffuse_set, (char *)FrsMaterial_diffuse_doc, NULL},
	{(char *)"specular", (getter)FrsMaterial_specular_get, (setter)FrsMaterial_specular_set, (char *)FrsMaterial_specular_doc, NULL},
	{(char *)"ambient", (getter)FrsMaterial_ambient_get, (setter)FrsMaterial_ambient_set, (char *)FrsMaterial_ambient_doc, NULL},
	{(char *)"emission", (getter)FrsMaterial_emission_get, (setter)FrsMaterial_emission_set, (char *)FrsMaterial_emission_doc, NULL},
	{(char *)"shininess", (getter)FrsMaterial_shininess_get, (setter)FrsMaterial_shininess_set, (char *)FrsMaterial_shininess_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_FrsMaterial type definition ------------------------------*/

PyTypeObject FrsMaterial_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Material",                     /* tp_name */
	sizeof(BPy_FrsMaterial),        /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)FrsMaterial___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)FrsMaterial___repr__, /* tp_repr */
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
	FrsMaterial___doc__,            /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_FrsMaterial_methods,        /* tp_methods */
	0,                              /* tp_members */
	BPy_FrsMaterial_getseters,      /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)FrsMaterial___init__, /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
