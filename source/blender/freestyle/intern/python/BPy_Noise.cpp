#include "BPy_Noise.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Noise instance  -----------*/
static int Noise___init__(BPy_Noise *self, PyObject *args, PyObject *kwds);
static void Noise___dealloc__(BPy_Noise *self);
static PyObject * Noise___repr__(BPy_Noise *self);

static PyObject * Noise_turbulence1( BPy_Noise *self, PyObject *args);
static PyObject * Noise_turbulence2( BPy_Noise *self, PyObject *args);
static PyObject * Noise_turbulence3( BPy_Noise *self, PyObject *args);
static PyObject * Noise_smoothNoise1( BPy_Noise *self, PyObject *args);
static PyObject * Noise_smoothNoise2( BPy_Noise *self, PyObject *args);
static PyObject * Noise_smoothNoise3( BPy_Noise *self, PyObject *args);

/*----------------------Noise instance definitions ----------------------------*/
static PyMethodDef BPy_Noise_methods[] = {
	{"turbulence1", ( PyCFunction ) Noise_turbulence1, METH_VARARGS, "(float arg, float freq, float amp, unsigned oct=4)）Returns a noise value for a 1D element"},
	{"turbulence2", ( PyCFunction ) Noise_turbulence2, METH_VARARGS, "([x, y], float freq, float amp, unsigned oct=4)））Returns a noise value for a 2D element"},
	{"turbulence3", ( PyCFunction ) Noise_turbulence3, METH_VARARGS, "([x, y, z], float freq, float amp, unsigned oct=4)））Returns a noise value for a 3D element"},
	{"smoothNoise1", ( PyCFunction ) Noise_smoothNoise1, METH_VARARGS, "(float arg)）Returns a smooth noise value for a 1D element "},
	{"smoothNoise2", ( PyCFunction ) Noise_smoothNoise2, METH_VARARGS, "([x, y])）Returns a smooth noise value for a 2D element "},
	{"smoothNoise3", ( PyCFunction ) Noise_smoothNoise3, METH_VARARGS, "([x, y, z)）Returns a smooth noise value for a 3D element "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Noise type definition ------------------------------*/

PyTypeObject Noise_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Noise",				/* tp_name */
	sizeof( BPy_Noise ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)Noise___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)Noise___repr__,					/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, 		/* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_Noise_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)Noise___init__, /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,		/* newfunc tp_new; */
	
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

//-------------------MODULE INITIALIZATION--------------------------------
PyMODINIT_FUNC Noise_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &Noise_Type ) < 0 )
		return;

	Py_INCREF( &Noise_Type );
	PyModule_AddObject(module, "Noise", (PyObject *)&Noise_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

int Noise___init__(BPy_Noise *self, PyObject *args, PyObject *kwds)
{
	self->n = new Noise();
	return 0;
}

void Noise___dealloc__(BPy_Noise* self)
{
	delete self->n;
    self->ob_type->tp_free((PyObject*)self);
}


PyObject * Noise___repr__(BPy_Noise* self)
{
    return PyString_FromFormat("Noise - address: %p", self->n );
}


PyObject * Noise_turbulence1( BPy_Noise *self , PyObject *args) {
	float f1, f2, f3;
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "fff|I", &f1, &f2, &f3, &i) )) {
		cout << "ERROR: Noise_turbulence1" << endl;
		Py_RETURN_NONE;
	}

	return PyFloat_FromDouble( self->n->turbulence1(f1, f2, f3, i) );
}

PyObject * Noise_turbulence2( BPy_Noise *self , PyObject *args) {
	PyObject *obj1;
	float f2, f3;
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "Off|I", &obj1, &f2, &f3, &i) && PyList_Check(obj1) && PyList_Size(obj1) > 1 )) {
		cout << "ERROR: Noise_turbulence2" << endl;
		Py_RETURN_NONE;
	}

	Vec2f v( PyFloat_AsDouble(PyList_GetItem(obj1, 0)), PyFloat_AsDouble(PyList_GetItem(obj1, 1)) );
	
	return PyFloat_FromDouble( self->n->turbulence2(v, f2, f3, i) );
}

PyObject * Noise_turbulence3( BPy_Noise *self , PyObject *args) {
	PyObject *obj1;
	float f2, f3;
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "Off|I", &obj1, &f2, &f3, &i) && PyList_Check(obj1) && PyList_Size(obj1) > 2 )) {
		cout << "ERROR: Noise_turbulence3" << endl;
		Py_RETURN_NONE;
	}

	Vec3f v( PyFloat_AsDouble(PyList_GetItem(obj1, 0)),
			 PyFloat_AsDouble(PyList_GetItem(obj1, 1)),
			 PyFloat_AsDouble(PyList_GetItem(obj1, 2)) );
	
	return PyFloat_FromDouble( self->n->turbulence3(v, f2, f3, i) );
}

PyObject * Noise_smoothNoise1( BPy_Noise *self , PyObject *args) {
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f) )) {
		cout << "ERROR: Noise_smoothNoise1" << endl;
		Py_RETURN_NONE;
	}

	return PyFloat_FromDouble( self->n->smoothNoise1(f) );
}

PyObject * Noise_smoothNoise2( BPy_Noise *self , PyObject *args) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) && PyList_Check(obj) && PyList_Size(obj) > 1 )) {
		cout << "ERROR: Noise_smoothNoise2" << endl;
		Py_RETURN_NONE;
	}

	Vec2f v( PyFloat_AsDouble(PyList_GetItem(obj, 0)), PyFloat_AsDouble(PyList_GetItem(obj, 1)) );
	
	return PyFloat_FromDouble( self->n->smoothNoise2(v) );
}

PyObject * Noise_smoothNoise3( BPy_Noise *self , PyObject *args) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) && PyList_Check(obj) && PyList_Size(obj) > 2 )) {
		cout << "ERROR: Noise_smoothNoise3" << endl;
		Py_RETURN_NONE;
	}

	Vec3f v( PyFloat_AsDouble(PyList_GetItem(obj, 0)),
			 PyFloat_AsDouble(PyList_GetItem(obj, 1)),
			 PyFloat_AsDouble(PyList_GetItem(obj, 2)) );
	
	return PyFloat_FromDouble( self->n->smoothNoise3(v) );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
