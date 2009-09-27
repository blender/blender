#include "BPy_FrsNoise.h"
#include "BPy_Convert.h"

#include <sstream>

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
	{"turbulence1", ( PyCFunction ) FrsNoise_turbulence1, METH_VARARGS, "(float arg, float freq, float amp, unsigned oct=4) Returns a noise value for a 1D element"},
	{"turbulence2", ( PyCFunction ) FrsNoise_turbulence2, METH_VARARGS, "([x, y], float freq, float amp, unsigned oct=4) Returns a noise value for a 2D element"},
	{"turbulence3", ( PyCFunction ) FrsNoise_turbulence3, METH_VARARGS, "([x, y, z], float freq, float amp, unsigned oct=4) Returns a noise value for a 3D element"},
	{"smoothNoise1", ( PyCFunction ) FrsNoise_smoothNoise1, METH_VARARGS, "(float arg) Returns a smooth noise value for a 1D element "},
	{"smoothNoise2", ( PyCFunction ) FrsNoise_smoothNoise2, METH_VARARGS, "([x, y]) Returns a smooth noise value for a 2D element "},
	{"smoothNoise3", ( PyCFunction ) FrsNoise_smoothNoise3, METH_VARARGS, "([x, y, z]) Returns a smooth noise value for a 3D element "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FrsNoise type definition ------------------------------*/

PyTypeObject FrsNoise_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"FrsNoise",                     /* tp_name */
	sizeof(BPy_FrsNoise),           /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)FrsNoise___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)FrsNoise___repr__,    /* tp_repr */
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
	"FrsNoise objects",             /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_FrsNoise_methods,           /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)FrsNoise___init__,    /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------
int FrsNoise_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &FrsNoise_Type ) < 0 )
		return -1;

	Py_INCREF( &FrsNoise_Type );
	PyModule_AddObject(module, "FrsNoise", (PyObject *)&FrsNoise_Type);
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int FrsNoise___init__(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
	if(!( PyArg_ParseTuple(args, "") ))
		return -1;
	self->n = new Noise();
	return 0;
}

void FrsNoise___dealloc__(BPy_FrsNoise* self)
{
	delete self->n;
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject * FrsNoise___repr__(BPy_FrsNoise* self)
{
    return PyUnicode_FromFormat("FrsNoise - address: %p", self->n );
}


PyObject * FrsNoise_turbulence1( BPy_FrsNoise *self , PyObject *args) {
	float f1, f2, f3;
	unsigned int i = 4;

	if(!( PyArg_ParseTuple(args, "fff|I", &f1, &f2, &f3, &i) ))
		return NULL;

	return PyFloat_FromDouble( self->n->turbulence1(f1, f2, f3, i) );
}

PyObject * FrsNoise_turbulence2( BPy_FrsNoise *self , PyObject *args) {
	PyObject *obj1;
	float f2, f3;
	unsigned int i = 4;

	if(!( PyArg_ParseTuple(args, "Off|I", &obj1, &f2, &f3, &i) ))
		return NULL;
	Vec2f *v = Vec2f_ptr_from_PyObject(obj1);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 2D vector (either a list of 2 elements or Vector)");
		return NULL;
	}
	float t = self->n->turbulence2(*v, f2, f3, i);
	delete v;
	return PyFloat_FromDouble( t );
}

PyObject * FrsNoise_turbulence3( BPy_FrsNoise *self , PyObject *args) {
	PyObject *obj1;
	float f2, f3;
	unsigned int i = 4;

	if(!( PyArg_ParseTuple(args, "Off|I", &obj1, &f2, &f3, &i) ))
		return NULL;
	Vec3f *v = Vec3f_ptr_from_PyObject(obj1);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	float t = self->n->turbulence3(*v, f2, f3, i);
	delete v;
	return PyFloat_FromDouble( t );
}

PyObject * FrsNoise_smoothNoise1( BPy_FrsNoise *self , PyObject *args) {
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f) ))
		return NULL;

	return PyFloat_FromDouble( self->n->smoothNoise1(f) );
}

PyObject * FrsNoise_smoothNoise2( BPy_FrsNoise *self , PyObject *args) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) ))
		return NULL;
	Vec2f *v = Vec2f_ptr_from_PyObject(obj);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 2D vector (either a list of 2 elements or Vector)");
		return NULL;
	}
	float t = self->n->smoothNoise2(*v);
	delete v;
	return PyFloat_FromDouble( t );
}

PyObject * FrsNoise_smoothNoise3( BPy_FrsNoise *self , PyObject *args) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) ))
		return NULL;
	Vec3f *v = Vec3f_ptr_from_PyObject(obj);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	float t = self->n->smoothNoise3(*v);
	delete v;
	return PyFloat_FromDouble( t );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
