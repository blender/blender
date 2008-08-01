#include "BPy_FrsNoise.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for FrsNoise instance  -----------*/
static int FrsNoise___init__(BPy_FrsNoise *self, PyObject *args, PyObject *kwds);
static void FrsNoise___dealloc__(BPy_FrsNoise *self);
static PyObject * FrsNoise___repr__(BPy_FrsNoise *self);

static PyObject * FrsNoise_turbulence1( BPy_FrsNoise *self, PyObject *args);
static PyObject * FrsNoise_turbulence2( BPy_FrsNoise *self, PyObject *args);
static PyObject * FrsNoise_turbulence3( BPy_FrsNoise *self, PyObject *args);
static PyObject * FrsNoise_smoothNoise1( BPy_FrsNoise *self, PyObject *args);
static PyObject * FrsNoise_smoothNoise2( BPy_FrsNoise *self, PyObject *args);
static PyObject * FrsNoise_smoothNoise3( BPy_FrsNoise *self, PyObject *args);

/*----------------------FrsNoise instance definitions ----------------------------*/
static PyMethodDef BPy_FrsNoise_methods[] = {
	{"turbulence1", ( PyCFunction ) FrsNoise_turbulence1, METH_VARARGS, "(float arg, float freq, float amp, unsigned oct=4)）Returns a noise value for a 1D element"},
	{"turbulence2", ( PyCFunction ) FrsNoise_turbulence2, METH_VARARGS, "([x, y], float freq, float amp, unsigned oct=4)））Returns a noise value for a 2D element"},
	{"turbulence3", ( PyCFunction ) FrsNoise_turbulence3, METH_VARARGS, "([x, y, z], float freq, float amp, unsigned oct=4)））Returns a noise value for a 3D element"},
	{"smoothNoise1", ( PyCFunction ) FrsNoise_smoothNoise1, METH_VARARGS, "(float arg)）Returns a smooth noise value for a 1D element "},
	{"smoothNoise2", ( PyCFunction ) FrsNoise_smoothNoise2, METH_VARARGS, "([x, y])）Returns a smooth noise value for a 2D element "},
	{"smoothNoise3", ( PyCFunction ) FrsNoise_smoothNoise3, METH_VARARGS, "([x, y, z)）Returns a smooth noise value for a 3D element "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FrsNoise type definition ------------------------------*/

PyTypeObject FrsNoise_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"FrsNoise",				/* tp_name */
	sizeof( BPy_FrsNoise ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)FrsNoise___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)FrsNoise___repr__,					/* tp_repr */

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
	BPy_FrsNoise_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)FrsNoise___init__, /* initproc tp_init; */
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
PyMODINIT_FUNC FrsNoise_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &FrsNoise_Type ) < 0 )
		return;

	Py_INCREF( &FrsNoise_Type );
	PyModule_AddObject(module, "FrsNoise", (PyObject *)&FrsNoise_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

int FrsNoise___init__(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
	self->n = new Noise();
	return 0;
}

void FrsNoise___dealloc__(BPy_FrsNoise* self)
{
	delete self->n;
    self->ob_type->tp_free((PyObject*)self);
}


PyObject * FrsNoise___repr__(BPy_FrsNoise* self)
{
    return PyString_FromFormat("FrsNoise - address: %p", self->n );
}


PyObject * FrsNoise_turbulence1( BPy_FrsNoise *self , PyObject *args) {
	float f1, f2, f3;
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "fff|I", &f1, &f2, &f3, &i) )) {
		cout << "ERROR: FrsNoise_turbulence1" << endl;
		Py_RETURN_NONE;
	}

	return PyFloat_FromDouble( self->n->turbulence1(f1, f2, f3, i) );
}

PyObject * FrsNoise_turbulence2( BPy_FrsNoise *self , PyObject *args) {
	PyObject *obj1;
	float f2, f3;
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "Off|I", &obj1, &f2, &f3, &i) && PyList_Check(obj1) && PyList_Size(obj1) > 1 )) {
		cout << "ERROR: FrsNoise_turbulence2" << endl;
		Py_RETURN_NONE;
	}

	Vec2f v( PyFloat_AsDouble(PyList_GetItem(obj1, 0)), PyFloat_AsDouble(PyList_GetItem(obj1, 1)) );
	
	return PyFloat_FromDouble( self->n->turbulence2(v, f2, f3, i) );
}

PyObject * FrsNoise_turbulence3( BPy_FrsNoise *self , PyObject *args) {
	PyObject *obj1;
	float f2, f3;
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "Off|I", &obj1, &f2, &f3, &i) && PyList_Check(obj1) && PyList_Size(obj1) > 2 )) {
		cout << "ERROR: FrsNoise_turbulence3" << endl;
		Py_RETURN_NONE;
	}

	Vec3f v( PyFloat_AsDouble(PyList_GetItem(obj1, 0)),
			 PyFloat_AsDouble(PyList_GetItem(obj1, 1)),
			 PyFloat_AsDouble(PyList_GetItem(obj1, 2)) );
	
	return PyFloat_FromDouble( self->n->turbulence3(v, f2, f3, i) );
}

PyObject * FrsNoise_smoothNoise1( BPy_FrsNoise *self , PyObject *args) {
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f) )) {
		cout << "ERROR: FrsNoise_smoothNoise1" << endl;
		Py_RETURN_NONE;
	}

	return PyFloat_FromDouble( self->n->smoothNoise1(f) );
}

PyObject * FrsNoise_smoothNoise2( BPy_FrsNoise *self , PyObject *args) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) && PyList_Check(obj) && PyList_Size(obj) > 1 )) {
		cout << "ERROR: FrsNoise_smoothNoise2" << endl;
		Py_RETURN_NONE;
	}

	Vec2f v( PyFloat_AsDouble(PyList_GetItem(obj, 0)), PyFloat_AsDouble(PyList_GetItem(obj, 1)) );
	
	return PyFloat_FromDouble( self->n->smoothNoise2(v) );
}

PyObject * FrsNoise_smoothNoise3( BPy_FrsNoise *self , PyObject *args) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) && PyList_Check(obj) && PyList_Size(obj) > 2 )) {
		cout << "ERROR: FrsNoise_smoothNoise3" << endl;
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
