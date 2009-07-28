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

static PyObject *BPy_IntegrationType_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

static PyObject * Integrator_integrate( PyObject *self, PyObject *args );

/*-----------------------Integrator module docstring---------------------------------------*/

static char module_docstring[] = "The Blender.Freestyle.Integrator submodule";

/*-----------------------Integrator module functions definitions---------------------------*/

static PyMethodDef module_functions[] = {
  {"integrate", (PyCFunction)Integrator_integrate, METH_VARARGS, ""},
  {NULL, NULL, 0, NULL}
};

/*-----------------------BPy_IntegrationType type definition ------------------------------*/

PyTypeObject IntegrationType_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"IntegrationType",				/* tp_name */
	sizeof( BPy_IntegrationType ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	NULL,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	NULL,					/* tp_repr */

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
	NULL,							/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&PyInt_Type,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	NULL,                       	/* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	BPy_IntegrationType_new,		/* newfunc tp_new; */
	
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

static PyObject *
BPy_IntegrationType_FromIntegrationType(int t)
{
	BPy_IntegrationType *obj;

	obj = PyObject_New(BPy_IntegrationType, &IntegrationType_Type);
	if (!obj)
		return NULL;
	((PyIntObject *)obj)->ob_ival = t;
	return (PyObject *)obj;
}

static PyObject *
BPy_IntegrationType_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	int x = 0;
	static char *kwlist[] = {"x", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, &x))
		return NULL;
	return BPy_IntegrationType_FromIntegrationType(x);
}

//-------------------MODULE INITIALIZATION--------------------------------
PyMODINIT_FUNC IntegrationType_Init( PyObject *module )
{	
	PyObject *tmp, *m, *d, *f;
	
	if( module == NULL )
		return;

	if( PyType_Ready( &IntegrationType_Type ) < 0 )
		return;
	Py_INCREF( &IntegrationType_Type );
	PyModule_AddObject(module, "IntegrationType", (PyObject *)&IntegrationType_Type);

	tmp = BPy_IntegrationType_FromIntegrationType( MEAN );
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "MEAN", tmp);
	Py_DECREF(tmp);
	
	tmp = BPy_IntegrationType_FromIntegrationType( MIN );
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "MIN", tmp);
	Py_DECREF(tmp);
	
	tmp = BPy_IntegrationType_FromIntegrationType( MAX );
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "MAX", tmp);
	Py_DECREF(tmp);
	
	tmp = BPy_IntegrationType_FromIntegrationType( FIRST );
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "FIRST", tmp);
	Py_DECREF(tmp);
	
	tmp = BPy_IntegrationType_FromIntegrationType( LAST );
	PyDict_SetItemString( IntegrationType_Type.tp_dict, "LAST", tmp);
	Py_DECREF(tmp);
	
	m = Py_InitModule3("Blender.Freestyle.Integrator", module_functions, module_docstring);
	if (m == NULL)
		return;
	PyModule_AddObject(module, "Integrator", m);

	// from Integrator import *
	d = PyModule_GetDict(m);
	for (PyMethodDef *p = module_functions; p->ml_name; p++) {
		f = PyDict_GetItemString(d, p->ml_name);
		Py_INCREF(f);
		PyModule_AddObject(module, p->ml_name, f);
	}
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
		return PyInt_FromLong( res );

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

