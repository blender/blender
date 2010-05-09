#include "BPy_FrsNoise.h"
#include "BPy_Convert.h"

#include <sstream>

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int FrsNoise_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &FrsNoise_Type ) < 0 )
		return -1;

	Py_INCREF( &FrsNoise_Type );
	PyModule_AddObject(module, "Noise", (PyObject *)&FrsNoise_Type);
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char FrsNoise___doc__[] =
"Class to provide Perlin noise functionalities.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Builds a Noise object.\n";

static int FrsNoise___init__(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
	if(!( PyArg_ParseTuple(args, "") ))
		return -1;
	self->n = new Noise();
	return 0;
}

static void FrsNoise___dealloc__(BPy_FrsNoise* self)
{
	delete self->n;
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject * FrsNoise___repr__(BPy_FrsNoise* self)
{
    return PyUnicode_FromFormat("Noise - address: %p", self->n );
}

static char FrsNoise_turbulence1___doc__[] =
".. method:: turbulence1(v, freq, amp, oct=4)\n"
"\n"
"   Returns a noise value for a 1D element.\n"
"\n"
"   :arg v: One-dimensional sample point.\n"
"   :type v: float\n"
"   :arg freq: Noise frequency.\n"
"   :type freq: float\n"
"   :arg amp: Amplitude.\n"
"   :type amp: float\n"
"   :arg oct: Number of octaves.\n"
"   :type oct: int\n"
"   :return: A noise value.\n"
"   :rtype: float\n";

static PyObject * FrsNoise_turbulence1( BPy_FrsNoise *self , PyObject *args) {
	float f1, f2, f3;
	unsigned int i = 4;

	if(!( PyArg_ParseTuple(args, "fff|I", &f1, &f2, &f3, &i) ))
		return NULL;

	return PyFloat_FromDouble( self->n->turbulence1(f1, f2, f3, i) );
}

static char FrsNoise_turbulence2___doc__[] =
".. method:: turbulence2(v, freq, amp, oct=4)\n"
"\n"
"   Returns a noise value for a 2D element.\n"
"\n"
"   :arg v: Two-dimensional sample point.\n"
"   :type v: :class:`mathutils.Vector`, list or tuple of 2 real numbers\n"
"   :arg freq: Noise frequency.\n"
"   :type freq: float\n"
"   :arg amp: Amplitude.\n"
"   :type amp: float\n"
"   :arg oct: Number of octaves.\n"
"   :type oct: int\n"
"   :return: A noise value.\n"
"   :rtype: float\n";

static PyObject * FrsNoise_turbulence2( BPy_FrsNoise *self , PyObject *args) {
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

static char FrsNoise_turbulence3___doc__[] =
".. method:: turbulence3(v, freq, amp, oct=4)\n"
"\n"
"   Returns a noise value for a 3D element.\n"
"\n"
"   :arg v: Three-dimensional sample point.\n"
"   :type v: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n"
"   :arg freq: Noise frequency.\n"
"   :type freq: float\n"
"   :arg amp: Amplitude.\n"
"   :type amp: float\n"
"   :arg oct: Number of octaves.\n"
"   :type oct: int\n"
"   :return: A noise value.\n"
"   :rtype: float\n";

static PyObject * FrsNoise_turbulence3( BPy_FrsNoise *self , PyObject *args) {
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

static char FrsNoise_smoothNoise1___doc__[] =
".. method:: smoothNoise1(v)\n"
"\n"
"   Returns a smooth noise value for a 1D element.\n"
"\n"
"   :arg v: One-dimensional sample point.\n"
"   :type v: float\n"
"   :return: A smooth noise value.\n"
"   :rtype: float\n";

static PyObject * FrsNoise_smoothNoise1( BPy_FrsNoise *self , PyObject *args) {
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f) ))
		return NULL;

	return PyFloat_FromDouble( self->n->smoothNoise1(f) );
}

static char FrsNoise_smoothNoise2___doc__[] =
".. method:: smoothNoise2(v)\n"
"\n"
"   Returns a smooth noise value for a 2D element.\n"
"\n"
"   :arg v: Two-dimensional sample point.\n"
"   :type v: :class:`mathutils.Vector`, list or tuple of 2 real numbers\n"
"   :return: A smooth noise value.\n"
"   :rtype: float\n";

static PyObject * FrsNoise_smoothNoise2( BPy_FrsNoise *self , PyObject *args) {
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

static char FrsNoise_smoothNoise3___doc__[] =
".. method:: smoothNoise3(v)\n"
"\n"
"   Returns a smooth noise value for a 3D element.\n"
"\n"
"   :arg v: Three-dimensional sample point.\n"
"   :type v: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n"
"   :return: A smooth noise value.\n"
"   :rtype: float\n";

static PyObject * FrsNoise_smoothNoise3( BPy_FrsNoise *self , PyObject *args) {
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

/*----------------------FrsNoise instance definitions ----------------------------*/
static PyMethodDef BPy_FrsNoise_methods[] = {
	{"turbulence1", ( PyCFunction ) FrsNoise_turbulence1, METH_VARARGS, FrsNoise_turbulence1___doc__},
	{"turbulence2", ( PyCFunction ) FrsNoise_turbulence2, METH_VARARGS, FrsNoise_turbulence2___doc__},
	{"turbulence3", ( PyCFunction ) FrsNoise_turbulence3, METH_VARARGS, FrsNoise_turbulence3___doc__},
	{"smoothNoise1", ( PyCFunction ) FrsNoise_smoothNoise1, METH_VARARGS, FrsNoise_smoothNoise1___doc__},
	{"smoothNoise2", ( PyCFunction ) FrsNoise_smoothNoise2, METH_VARARGS, FrsNoise_smoothNoise2___doc__},
	{"smoothNoise3", ( PyCFunction ) FrsNoise_smoothNoise3, METH_VARARGS, FrsNoise_smoothNoise3___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FrsNoise type definition ------------------------------*/

PyTypeObject FrsNoise_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Noise",                        /* tp_name */
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
	FrsNoise___doc__,               /* tp_doc */
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

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
