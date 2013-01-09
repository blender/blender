#include "BPy_FrsMaterial.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

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

static char FrsMaterial_diffuse___doc__[] =
".. method:: diffuse()\n"
"\n"
"   Returns the diffuse color.\n"
"\n"
"   :return: The diffuse color.\n"
"   :rtype: Tuple of 4 float values\n";

static PyObject * FrsMaterial_diffuse( BPy_FrsMaterial* self) {
	const float *diffuse = self->m->diffuse();
	PyObject *py_diffuse = PyTuple_New(4);
	
	PyTuple_SetItem( py_diffuse, 0, PyFloat_FromDouble( diffuse[0] ) );
	PyTuple_SetItem( py_diffuse, 1, PyFloat_FromDouble( diffuse[1] ) );
	PyTuple_SetItem( py_diffuse, 2, PyFloat_FromDouble( diffuse[2] ) );
	PyTuple_SetItem( py_diffuse, 3, PyFloat_FromDouble( diffuse[3] ) );
	
	return py_diffuse;
}

static char FrsMaterial_diffuseR___doc__[] =
".. method:: diffuseR()\n"
"\n"
"   Returns the red component of the diffuse color.\n"
"\n"
"   :return: The red component of the diffuse color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_diffuseR( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->diffuseR() );
}

static char FrsMaterial_diffuseG___doc__[] =
".. method:: diffuseG()\n"
"\n"
"   Returns the green component of the diffuse color.\n"
"\n"
"   :return: The green component of the diffuse color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_diffuseG( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->diffuseG() );
}

static char FrsMaterial_diffuseB___doc__[] =
".. method:: diffuseB()\n"
"\n"
"   Returns the blue component of the diffuse color.\n"
"\n"
"   :return: The blue component of the diffuse color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_diffuseB( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->diffuseB() );
}

static char FrsMaterial_diffuseA___doc__[] =
".. method:: diffuseA()\n"
"\n"
"   Returns the alpha component of the diffuse color.\n"
"\n"
"   :return: The alpha component of the diffuse color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_diffuseA( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->diffuseA() );
}

static char FrsMaterial_specular___doc__[] =
".. method:: specular()\n"
"\n"
"   Returns the specular color.\n"
"\n"
"   :return: The specular color.\n"
"   :rtype: Tuple of 4 float values\n";

static PyObject * FrsMaterial_specular( BPy_FrsMaterial* self) {
	const float *specular = self->m->specular();
	PyObject *py_specular = PyTuple_New(4);
	
	PyTuple_SetItem( py_specular, 0, PyFloat_FromDouble( specular[0] ) );
	PyTuple_SetItem( py_specular, 1, PyFloat_FromDouble( specular[1] ) );
	PyTuple_SetItem( py_specular, 2, PyFloat_FromDouble( specular[2] ) );
	PyTuple_SetItem( py_specular, 3, PyFloat_FromDouble( specular[3] ) );
	
	return py_specular;
}

static char FrsMaterial_specularR___doc__[] =
".. method:: specularR()\n"
"\n"
"   Returns the red component of the specular color.\n"
"\n"
"   :return: The red component of the specular color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_specularR( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->specularR() );
}

static char FrsMaterial_specularG___doc__[] =
".. method:: specularG()\n"
"\n"
"   Returns the green component of the specular color.\n"
"\n"
"   :return: The green component of the specular color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_specularG( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->specularG() );
}

static char FrsMaterial_specularB___doc__[] =
".. method:: specularB()\n"
"\n"
"   Returns the blue component of the specular color.\n"
"\n"
"   :return: The blue component of the specular color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_specularB( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->specularB() );
}

static char FrsMaterial_specularA___doc__[] =
".. method:: specularA()\n"
"\n"
"   Returns the alpha component of the specular color.\n"
"\n"
"   :return: The alpha component of the specular color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_specularA( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->specularA() );
}

static char FrsMaterial_ambient___doc__[] =
".. method:: ambient()\n"
"\n"
"   Returns the ambiant color.\n"
"\n"
"   :return: The ambiant color.\n"
"   :rtype: Tuple of 4 float values\n";

static PyObject * FrsMaterial_ambient( BPy_FrsMaterial* self) {
	const float *ambient = self->m->ambient();
	PyObject *py_ambient = PyTuple_New(4);
	
	PyTuple_SetItem( py_ambient, 0, PyFloat_FromDouble( ambient[0] ) );
	PyTuple_SetItem( py_ambient, 1, PyFloat_FromDouble( ambient[1] ) );
	PyTuple_SetItem( py_ambient, 2, PyFloat_FromDouble( ambient[2] ) );
	PyTuple_SetItem( py_ambient, 3, PyFloat_FromDouble( ambient[3] ) );
	
	return py_ambient;
}

static char FrsMaterial_ambientR___doc__[] =
".. method:: ambientR()\n"
"\n"
"   Returns the red component of the ambiant color.\n"
"\n"
"   :return: The red component of the ambiant color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_ambientR( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->ambientR() );
}

static char FrsMaterial_ambientG___doc__[] =
".. method:: ambientG()\n"
"\n"
"   Returns the green component of the ambiant color.\n"
"\n"
"   :return: The green component of the ambiant color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_ambientG( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->ambientG() );
}

static char FrsMaterial_ambientB___doc__[] =
".. method:: ambientB()\n"
"\n"
"   Returns the blue component of the ambiant color.\n"
"\n"
"   :return: The blue component of the ambiant color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_ambientB( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->ambientB() );
}

static char FrsMaterial_ambientA___doc__[] =
".. method:: ambientA()\n"
"\n"
"   Returns the alpha component of the ambiant color.\n"
"\n"
"   :return: The alpha component of the ambiant color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_ambientA( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->ambientA() );
}

static char FrsMaterial_emission___doc__[] =
".. method:: emission()\n"
"\n"
"   Returns the emissive color.\n"
"\n"
"   :return: the emissive color.\n"
"   :rtype: Tuple of 4 float values\n";

static PyObject * FrsMaterial_emission( BPy_FrsMaterial* self) {
	const float *emission = self->m->emission();
	PyObject *py_emission = PyTuple_New(4);
	
	PyTuple_SetItem( py_emission, 0, PyFloat_FromDouble( emission[0] ) );
	PyTuple_SetItem( py_emission, 1, PyFloat_FromDouble( emission[1] ) );
	PyTuple_SetItem( py_emission, 2, PyFloat_FromDouble( emission[2] ) );
	PyTuple_SetItem( py_emission, 3, PyFloat_FromDouble( emission[3] ) );
	
	return py_emission;
}

static char FrsMaterial_emissionR___doc__[] =
".. method:: emissionR()\n"
"\n"
"   Returns the red component of the emissive color.\n"
"\n"
"   :return: The red component of the emissive color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_emissionR( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->emissionR() );
}

static char FrsMaterial_emissionG___doc__[] =
".. method:: emissionG()\n"
"\n"
"   Returns the green component of the emissive color.\n"
"\n"
"   :return: The green component of the emissive color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_emissionG( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->emissionG() );
}

static char FrsMaterial_emissionB___doc__[] =
".. method:: emissionB()\n"
"\n"
"   Returns the blue component of the emissive color.\n"
"\n"
"   :return: The blue component of the emissive color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_emissionB( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->emissionB() );
}

static char FrsMaterial_emissionA___doc__[] =
".. method:: emissionA()\n"
"\n"
"   Returns the alpha component of the emissive color.\n"
"\n"
"   :return: The alpha component of the emissive color.\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_emissionA( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->emissionA() );
}

static char FrsMaterial_shininess___doc__[] =
".. method:: shininess()\n"
"\n"
"   Returns the shininess coefficient.\n"
"\n"
"   :return: Shininess\n"
"   :rtype: float\n";

static PyObject * FrsMaterial_shininess( BPy_FrsMaterial* self) {
	return PyFloat_FromDouble( self->m->shininess() );
}

static char FrsMaterial_setDiffuse___doc__[] =
".. method:: setDiffuse(r, g, b, a)\n"
"\n"
"   Sets the diffuse color.\n"
"\n"
"   :arg r: Red component.\n"
"   :type r: float\n"
"   :arg g: Green component.\n"
"   :type g: float\n"
"   :arg b: Blue component.\n"
"   :type b: float\n"
"   :arg a: Alpha component.\n"
"   :type a: float\n";

static PyObject * FrsMaterial_setDiffuse( BPy_FrsMaterial *self, PyObject *args ) {
	float f1, f2, f3, f4;

	if(!( PyArg_ParseTuple(args, "ffff", &f1, &f2, &f3, &f4)  ))
		return NULL;

	self->m->setDiffuse(f1, f2, f3, f4);

	Py_RETURN_NONE;
}
 
static char FrsMaterial_setSpecular___doc__[] =
".. method:: setSpecular(r, g, b, a)\n"
"\n"
"   Sets the specular color.\n"
"\n"
"   :arg r: Red component.\n"
"   :type r: float\n"
"   :arg g: Green component.\n"
"   :type g: float\n"
"   :arg b: Blue component.\n"
"   :type b: float\n"
"   :arg a: Alpha component.\n"
"   :type a: float\n";

static PyObject * FrsMaterial_setSpecular( BPy_FrsMaterial *self, PyObject *args ) {
	float f1, f2, f3, f4;

	if(!( PyArg_ParseTuple(args, "ffff", &f1, &f2, &f3, &f4)  ))
		return NULL;

	self->m->setSpecular(f1, f2, f3, f4);

	Py_RETURN_NONE;
}

static char FrsMaterial_setAmbient___doc__[] =
".. method:: setAmbient(r, g, b, a)\n"
"\n"
"   Sets the ambiant color.\n"
"\n"
"   :arg r: Red component.\n"
"   :type r: float\n"
"   :arg g: Green component.\n"
"   :type g: float\n"
"   :arg b: Blue component.\n"
"   :type b: float\n"
"   :arg a: Alpha component.\n"
"   :type a: float\n";

static PyObject * FrsMaterial_setAmbient( BPy_FrsMaterial *self, PyObject *args ) {
	float f1, f2, f3, f4;

	if(!( PyArg_ParseTuple(args, "ffff", &f1, &f2, &f3, &f4)  ))
		return NULL;

	self->m->setAmbient(f1, f2, f3, f4);

	Py_RETURN_NONE;
}

static char FrsMaterial_setEmission___doc__[] =
".. method:: setEmission(r, g, b, a)\n"
"\n"
"   Sets the emissive color.\n"
"\n"
"   :arg r: Red component.\n"
"   :type r: float\n"
"   :arg g: Green component.\n"
"   :type g: float\n"
"   :arg b: Blue component.\n"
"   :type b: float\n"
"   :arg a: Alpha component.\n"
"   :type a: float\n";

static PyObject * FrsMaterial_setEmission( BPy_FrsMaterial *self, PyObject *args ) {
	float f1, f2, f3, f4;

	if(!( PyArg_ParseTuple(args, "ffff", &f1, &f2, &f3, &f4)  ))
		return NULL;

	self->m->setEmission(f1, f2, f3, f4);

	Py_RETURN_NONE;
}

static char FrsMaterial_setShininess___doc__[] =
".. method:: setShininess(s)\n"
"\n"
"   Sets the shininess.\n"
"\n"
"   :arg s: Shininess.\n"
"   :type s: float\n";

static PyObject * FrsMaterial_setShininess( BPy_FrsMaterial *self, PyObject *args ) {
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f)  ))
		return NULL;

	self->m->setShininess(f);

	Py_RETURN_NONE;
}

/*----------------------FrsMaterial instance definitions ----------------------------*/
static PyMethodDef BPy_FrsMaterial_methods[] = {
	{"diffuse", ( PyCFunction ) FrsMaterial_diffuse, METH_NOARGS, FrsMaterial_diffuse___doc__},
	{"diffuseR", ( PyCFunction ) FrsMaterial_diffuseR, METH_NOARGS, FrsMaterial_diffuseR___doc__},
	{"diffuseG", ( PyCFunction ) FrsMaterial_diffuseG, METH_NOARGS, FrsMaterial_diffuseG___doc__},
	{"diffuseB", ( PyCFunction ) FrsMaterial_diffuseB, METH_NOARGS, FrsMaterial_diffuseB___doc__},
	{"diffuseA", ( PyCFunction ) FrsMaterial_diffuseA, METH_NOARGS, FrsMaterial_diffuseA___doc__},
	{"specular", ( PyCFunction ) FrsMaterial_specular, METH_NOARGS, FrsMaterial_specular___doc__},
	{"specularR", ( PyCFunction ) FrsMaterial_specularR, METH_NOARGS, FrsMaterial_specularR___doc__},
	{"specularG", ( PyCFunction ) FrsMaterial_specularG, METH_NOARGS, FrsMaterial_specularG___doc__},
	{"specularB", ( PyCFunction ) FrsMaterial_specularB, METH_NOARGS, FrsMaterial_specularB___doc__},
	{"specularA", ( PyCFunction ) FrsMaterial_specularA, METH_NOARGS, FrsMaterial_specularA___doc__},
	{"ambient", ( PyCFunction ) FrsMaterial_ambient, METH_NOARGS, FrsMaterial_ambient___doc__},
	{"ambientR", ( PyCFunction ) FrsMaterial_ambientR, METH_NOARGS, FrsMaterial_ambientR___doc__},
	{"ambientG", ( PyCFunction ) FrsMaterial_ambientG, METH_NOARGS, FrsMaterial_ambientG___doc__},
	{"ambientB", ( PyCFunction ) FrsMaterial_ambientB, METH_NOARGS, FrsMaterial_ambientB___doc__},
	{"ambientA", ( PyCFunction ) FrsMaterial_ambientA, METH_NOARGS, FrsMaterial_ambientA___doc__},
	{"emission", ( PyCFunction ) FrsMaterial_emission, METH_NOARGS, FrsMaterial_emission___doc__},
	{"emissionR", ( PyCFunction ) FrsMaterial_emissionR, METH_NOARGS, FrsMaterial_emissionR___doc__},
	{"emissionG", ( PyCFunction ) FrsMaterial_emissionG, METH_NOARGS, FrsMaterial_emissionG___doc__},
	{"emissionB", ( PyCFunction ) FrsMaterial_emissionB, METH_NOARGS, FrsMaterial_emissionB___doc__},
	{"emissionA", ( PyCFunction ) FrsMaterial_emissionA, METH_NOARGS, FrsMaterial_emissionA___doc__},
	{"shininess", ( PyCFunction ) FrsMaterial_shininess, METH_NOARGS, FrsMaterial_shininess___doc__},
	{"setDiffuse", ( PyCFunction ) FrsMaterial_setDiffuse, METH_NOARGS, FrsMaterial_setDiffuse___doc__},
	{"setSpecular", ( PyCFunction ) FrsMaterial_setSpecular, METH_NOARGS, FrsMaterial_setSpecular___doc__},
	{"setAmbient", ( PyCFunction ) FrsMaterial_setAmbient, METH_NOARGS, FrsMaterial_setAmbient___doc__},
	{"setEmission", ( PyCFunction ) FrsMaterial_setEmission, METH_NOARGS, FrsMaterial_setEmission___doc__},
	{"setShininess", ( PyCFunction ) FrsMaterial_setShininess, METH_NOARGS, FrsMaterial_setShininess___doc__},
	{NULL, NULL, 0, NULL}
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
	0,                              /* tp_getset */
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
