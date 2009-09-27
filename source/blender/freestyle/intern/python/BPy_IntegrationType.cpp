#include "BPy_IntegrationType.h"

#include "BPy_Convert.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DFloat.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DUnsigned.h"
#include "Iterator/BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

static PyObject * Integrator_integrate( PyObject *self, PyObject *args );

/*-----------------------Integrator module docstring---------------------------------------*/

static char module_docstring[] = "The Blender Freestyle.Integrator submodule\n\n";

/*-----------------------Integrator module functions definitions---------------------------*/

static PyMethodDef module_functions[] = {
  {"integrate", (PyCFunction)Integrator_integrate, METH_VARARGS, ""},
  {NULL, NULL, 0, NULL}
};

/*-----------------------Integrator module definition--------------------------------------*/

static PyModuleDef module_definition = {
    PyModuleDef_HEAD_INIT,
    "Freestyle.Integrator",
    module_docstring,
    -1,
    module_functions
};

/*-----------------------BPy_IntegrationType type definition ------------------------------*/

PyTypeObject IntegrationType_Type = {
	PyObject_HEAD_INIT(NULL)
	"IntegrationType",              /* tp_name */
	sizeof(PyLongObject),           /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,             /* tp_flags */
	"IntegrationType objects",      /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&PyLong_Type,                   /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	0,                              /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

/*-----------------------BPy_IntegrationType instance definitions -------------------------*/

static PyLongObject _IntegrationType_MEAN = {
	PyVarObject_HEAD_INIT(&IntegrationType_Type, 1)
	{ MEAN }
};
static PyLongObject _IntegrationType_MIN = {
	PyVarObject_HEAD_INIT(&IntegrationType_Type, 1)
	{ MIN }
};
static PyLongObject _IntegrationType_MAX = {
	PyVarObject_HEAD_INIT(&IntegrationType_Type, 1)
	{ MAX }
};
static PyLongObject _IntegrationType_FIRST = {
	PyVarObject_HEAD_INIT(&IntegrationType_Type, 1)
	{ FIRST }
};
static PyLongObject _IntegrationType_LAST = {
	PyVarObject_HEAD_INIT(&IntegrationType_Type, 1)
	{ LAST }
};

#define BPy_IntegrationType_MEAN  ((PyObject *)&_IntegrationType_MEAN)
#define BPy_IntegrationType_MIN   ((PyObject *)&_IntegrationType_MIN)
#define BPy_IntegrationType_MAX   ((PyObject *)&_IntegrationType_MAX)
#define BPy_IntegrationType_FIRST ((PyObject *)&_IntegrationType_FIRST)
#define BPy_IntegrationType_LAST  ((PyObject *)&_IntegrationType_LAST)

//-------------------MODULE INITIALIZATION--------------------------------
int IntegrationType_Init( PyObject *module )
{	
	PyObject *m, *d, *f;
	
	if( module == NULL )
		return -1;

	if( PyType_Ready( &IntegrationType_Type ) < 0 )
		return -1;
	Py_INCREF( &IntegrationType_Type );
	PyModule_AddObject(module, "IntegrationType", (PyObject *)&IntegrationType_Type);

	PyDict_SetItemString( IntegrationType_Type.tp_dict, "MEAN", BPy_IntegrationType_MEAN);
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "MIN", BPy_IntegrationType_MIN);
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "MAX", BPy_IntegrationType_MAX);
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "FIRST", BPy_IntegrationType_FIRST);
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "LAST", BPy_IntegrationType_LAST);
	
    m = PyModule_Create(&module_definition);
    if (m == NULL)
        return -1;
	Py_INCREF(m);
	PyModule_AddObject(module, "Integrator", m);

	// from Integrator import *
	d = PyModule_GetDict(m);
	for (PyMethodDef *p = module_functions; p->ml_name; p++) {
		f = PyDict_GetItemString(d, p->ml_name);
		Py_INCREF(f);
		PyModule_AddObject(module, p->ml_name, f);
	}

	return 0;
}

//------------------------ MODULE FUNCTIONS ----------------------------------

static PyObject * Integrator_integrate( PyObject *self, PyObject *args )
{
	PyObject *obj1, *obj4 = 0;
	BPy_Interface0DIterator *obj2, *obj3;

#if 1
	if(!( PyArg_ParseTuple(args, "O!O!O!|O!", &UnaryFunction0D_Type, &obj1,
		&Interface0DIterator_Type, &obj2, &Interface0DIterator_Type, &obj3,
		&IntegrationType_Type, &obj4) ))
		return NULL;
#else
	if(!( PyArg_ParseTuple(args, "OOO|O", &obj1, &obj2, &obj3, &obj4) ))
		return NULL;
	if(!BPy_UnaryFunction0D_Check(obj1)) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a UnaryFunction0D object");
		return NULL;
	}
	if(!BPy_Interface0DIterator_Check(obj2)) {
		PyErr_SetString(PyExc_TypeError, "argument 2 must be a Interface0DIterator object");
		return NULL;
	}
	if(!BPy_Interface0DIterator_Check(obj3)) {
		PyErr_SetString(PyExc_TypeError, "argument 3 must be a Interface0DIterator object");
		return NULL;
	}
	if(obj4 && !BPy_IntegrationType_Check(obj4)) {
		PyErr_SetString(PyExc_TypeError, "argument 4 must be a IntegrationType object");
		return NULL;
	}
#endif

	Interface0DIterator it(*(obj2->if0D_it)), it_end(*(obj3->if0D_it));
	IntegrationType t = ( obj4 ) ? IntegrationType_from_BPy_IntegrationType( obj4 ) : MEAN;

	if( BPy_UnaryFunction0DDouble_Check(obj1) ) {
		UnaryFunction0D<double> *fun = ((BPy_UnaryFunction0DDouble *)obj1)->uf0D_double;
		double res = integrate( *fun, it, it_end, t );
		return PyFloat_FromDouble( res );

	} else if( BPy_UnaryFunction0DFloat_Check(obj1) ) {
		UnaryFunction0D<float> *fun = ((BPy_UnaryFunction0DFloat *)obj1)->uf0D_float;
		float res = integrate( *fun, it, it_end, t );
		return PyFloat_FromDouble( res );

	} else if( BPy_UnaryFunction0DUnsigned_Check(obj1) ) {
		UnaryFunction0D<unsigned int> *fun = ((BPy_UnaryFunction0DUnsigned *)obj1)->uf0D_unsigned;
		unsigned int res = integrate( *fun, it, it_end, t );
		return PyLong_FromLong( res );

	} else {
		string msg("unsupported function type: " + string(obj1->ob_type->tp_name));
		PyErr_SetString(PyExc_TypeError, msg.c_str());
		return NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

