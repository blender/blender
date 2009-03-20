#include "BPy_Operators.h"

#include "BPy_BinaryPredicate1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVoid.h"
#include "Iterator/BPy_ViewEdgeIterator.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Operators instance  -----------*/
static void Operators___dealloc__(BPy_Operators *self);

static PyObject * Operators_select(BPy_Operators* self, PyObject *args);
static PyObject * Operators_chain(BPy_Operators* self, PyObject *args);
static PyObject * Operators_bidirectionalChain(BPy_Operators* self, PyObject *args);
static PyObject * Operators_sequentialSplit(BPy_Operators* self, PyObject *args);
static PyObject * Operators_recursiveSplit(BPy_Operators* self, PyObject *args);
static PyObject * Operators_sort(BPy_Operators* self, PyObject *args);
static PyObject * Operators_create(BPy_Operators* self, PyObject *args);
static PyObject * Operators_getViewEdgesSize( BPy_Operators* self);
static PyObject * Operators_getChainsSize( BPy_Operators* self);
static PyObject * Operators_getStrokesSize( BPy_Operators* self);

/*----------------------Operators instance definitions ----------------------------*/
static PyMethodDef BPy_Operators_methods[] = {
	{"select", ( PyCFunction ) Operators_select, METH_VARARGS | METH_STATIC, 
	"select operator"},
	{"chain", ( PyCFunction ) Operators_chain, METH_VARARGS | METH_STATIC,
	 "chain operator"},
	{"bidirectionalChain", ( PyCFunction ) Operators_bidirectionalChain, METH_VARARGS | METH_STATIC,
	 "bidirectionalChain operator"},
	{"sequentialSplit", ( PyCFunction ) Operators_sequentialSplit, METH_VARARGS | METH_STATIC,
	 "sequentialSplit operator"},
	{"recursiveSplit", ( PyCFunction ) Operators_recursiveSplit, METH_VARARGS | METH_STATIC, 
	"recursiveSplit operator"},
	{"sort", ( PyCFunction ) Operators_sort, METH_VARARGS | METH_STATIC, 
	"sort operator"},
	{"create", ( PyCFunction ) Operators_create, METH_VARARGS | METH_STATIC, 
	"create operator"},
	{"getViewEdgesSize", ( PyCFunction ) Operators_getViewEdgesSize, METH_NOARGS | METH_STATIC, ""},
	{"getChainsSize", ( PyCFunction ) Operators_getChainsSize, METH_NOARGS | METH_STATIC, ""},
	{"getStrokesSize", ( PyCFunction ) Operators_getStrokesSize, METH_NOARGS | METH_STATIC, ""},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Operators type definition ------------------------------*/

PyTypeObject Operators_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Operators",				/* tp_name */
	sizeof( BPy_Operators ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)Operators___dealloc__,	/* tp_dealloc */
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
	/*   Operatorss */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_Operators_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	NULL,                       	/* initproc tp_init; */
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
PyMODINIT_FUNC Operators_Init( PyObject *module )
{	
	if( module == NULL )
		return;

	if( PyType_Ready( &Operators_Type ) < 0 )
		return;
	Py_INCREF( &Operators_Type );
	PyModule_AddObject(module, "Operators", (PyObject *)&Operators_Type);	
	
}

//------------------------INSTANCE METHODS ----------------------------------

void Operators___dealloc__(BPy_Operators* self)
{
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * Operators_select(BPy_Operators* self, PyObject *args)
{
	PyObject *obj = 0;

	if ( !PyArg_ParseTuple(args, "O!", &UnaryPredicate1D_Type, &obj) )
		return NULL;
	if ( !((BPy_UnaryPredicate1D *) obj)->up1D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.select(): 1st argument: invalid UnaryPredicate1D object");
		return NULL;
	}

	if (Operators::select(*( ((BPy_UnaryPredicate1D *) obj)->up1D )) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.select() failed");
		return NULL;
	}

	Py_RETURN_NONE;
}

// CHANGE: first parameter is a chaining iterator, not just a view

PyObject * Operators_chain(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;

	if ( !PyArg_ParseTuple(args, "O!O!|O!", &ChainingIterator_Type, &obj1,
										    &UnaryPredicate1D_Type, &obj2,
										    &UnaryFunction1DVoid_Type, &obj3) )
		return NULL;
	if ( !((BPy_ChainingIterator *) obj1)->c_it ) {
		PyErr_SetString(PyExc_TypeError, "Operators.chain(): 1st argument: invalid ChainingIterator object");
		return NULL;
	}
	if ( !((BPy_UnaryPredicate1D *) obj2)->up1D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.chain(): 2nd argument: invalid UnaryPredicate1D object");
		return NULL;
	}

	if( !obj3 ) {
		
		if (Operators::chain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
								*( ((BPy_UnaryPredicate1D *) obj2)->up1D )  ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.chain() failed");
			return NULL;
		}
							
	} else {
		
		if ( !((BPy_UnaryFunction1DVoid *) obj3)->uf1D_void ) {
			PyErr_SetString(PyExc_TypeError, "Operators.chain(): 3rd argument: invalid UnaryFunction1DVoid object");
			return NULL;
		}
		if (Operators::chain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
								*( ((BPy_UnaryPredicate1D *) obj2)->up1D ),
								*( ((BPy_UnaryFunction1DVoid *) obj3)->uf1D_void )  ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.chain() failed");
			return NULL;
		}
		
	}
	
	Py_RETURN_NONE;
}

PyObject * Operators_bidirectionalChain(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;

	if( !PyArg_ParseTuple(args, "O!|O!", &ChainingIterator_Type, &obj1, &UnaryPredicate1D_Type, &obj2) )
		return NULL;
	if ( !((BPy_ChainingIterator *) obj1)->c_it ) {
		PyErr_SetString(PyExc_TypeError, "Operators.bidirectionalChain(): 1st argument: invalid ChainingIterator object");
		return NULL;
	}

	if( !obj2 ) {

		if (Operators::bidirectionalChain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ) ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.bidirectionalChain() failed");
			return NULL;
		}
							
	} else {

		if ( !((BPy_UnaryPredicate1D *) obj2)->up1D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.bidirectionalChain(): 2nd argument: invalid UnaryPredicate1D object");
			return NULL;
		}
		if (Operators::bidirectionalChain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
											*( ((BPy_UnaryPredicate1D *) obj2)->up1D ) ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.bidirectionalChain() failed");
			return NULL;
		}
		
	}
	
	Py_RETURN_NONE;
}

PyObject * Operators_sequentialSplit(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;
	float f = 0.0;

	if( !PyArg_ParseTuple(args, "O!|Of", &UnaryPredicate0D_Type, &obj1, &obj2, &f) )
		return NULL;
	if ( !((BPy_UnaryPredicate0D *) obj1)->up0D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.sequentialSplit(): 1st argument: invalid UnaryPredicate0D object");
		return NULL;
	}

	if( obj2 && BPy_UnaryPredicate0D_Check(obj2) ) {
		
		if ( !((BPy_UnaryPredicate0D *) obj2)->up0D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.sequentialSplit(): 2nd argument: invalid UnaryPredicate0D object");
			return NULL;
		}
		if (Operators::sequentialSplit(	*( ((BPy_UnaryPredicate0D *) obj1)->up0D ),
										*( ((BPy_UnaryPredicate0D *) obj2)->up0D ),
										f ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.sequentialSplit() failed");
			return NULL;
		}

	} else {
		
		if ( obj2 ) {
			if ( !PyFloat_Check(obj2) ) {
				PyErr_SetString(PyExc_TypeError, "Operators.sequentialSplit(): invalid 2nd argument");
				return NULL;
			}
			f = PyFloat_AsDouble(obj2);
		}
		if (Operators::sequentialSplit( *( ((BPy_UnaryPredicate0D *) obj1)->up0D ), f ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.sequentialSplit() failed");
			return NULL;
		}
		
	}
	
	Py_RETURN_NONE;
}

PyObject * Operators_recursiveSplit(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;
	float f = 0.0;

	if ( !PyArg_ParseTuple(args, "O!O|Of", &UnaryFunction0DDouble_Type, &obj1, &obj2, &obj3, &f) )
		return NULL;
	if ( !((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double ) {
		PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): 1st argument: invalid UnaryFunction0DDouble object");
		return NULL;
	}
	
	if ( BPy_UnaryPredicate1D_Check(obj2) ) {

		if ( !((BPy_UnaryPredicate1D *) obj2)->up1D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): 2nd argument: invalid UnaryPredicate1D object");
			return NULL;
		}
		if ( obj3 ) {
			if ( !PyFloat_Check(obj3) ) {
				PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): invalid 3rd argument");
				return NULL;
			}
			f = PyFloat_AsDouble(obj3);
		}
		if (Operators::recursiveSplit( 	*( ((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double ),
										*( ((BPy_UnaryPredicate1D *) obj2)->up1D ),
										f ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.recursiveSplit() failed");
			return NULL;
		}
	
	} else {

		if ( !BPy_UnaryPredicate0D_Check(obj2) || !((BPy_UnaryPredicate0D *) obj2)->up0D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): invalid 2nd argument");
			return NULL;
		}
		if ( !BPy_UnaryPredicate1D_Check(obj3) || !((BPy_UnaryPredicate1D *) obj3)->up1D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): invalid 3rd argument");
			return NULL;
		}
		if (Operators::recursiveSplit( 	*( ((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double ),
										*( ((BPy_UnaryPredicate0D *) obj2)->up0D ),
										*( ((BPy_UnaryPredicate1D *) obj3)->up1D ),
										f ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.recursiveSplit() failed");
			return NULL;
		}

	}
	
	Py_RETURN_NONE;
}

PyObject * Operators_sort(BPy_Operators* self, PyObject *args)
{
	PyObject *obj = 0;

	if ( !PyArg_ParseTuple(args, "O!", &BinaryPredicate1D_Type, &obj) )
		return NULL;
	if ( !((BPy_BinaryPredicate1D *) obj)->bp1D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.sort(): 1st argument: invalid BinaryPredicate1D object");
		return NULL;
	}

	if (Operators::sort(*( ((BPy_BinaryPredicate1D *) obj)->bp1D )) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.sort() failed");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyObject * Operators_create(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;

	if ( !PyArg_ParseTuple(args, "O!O!", &UnaryPredicate1D_Type, &obj1, &PyList_Type, &obj2) )
		return NULL;
	if ( !((BPy_UnaryPredicate1D *) obj1)->up1D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.create(): 1st argument: invalid UnaryPredicate1D object");
		return NULL;
	}

	vector<StrokeShader *> shaders;
	for( int i = 0; i < PyList_Size(obj2); i++) {
		PyObject *py_ss = PyList_GetItem(obj2,i);
		
		if ( !BPy_StrokeShader_Check(py_ss) ) {
			PyErr_SetString(PyExc_TypeError, "Operators.create() 2nd argument must be a list of StrokeShader objects");
			return NULL;
		}
		shaders.push_back( ((BPy_StrokeShader *) py_ss)->ss );
	}
	
	if (Operators::create( *( ((BPy_UnaryPredicate1D *) obj1)->up1D ), shaders) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.create() failed");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject * Operators_getViewEdgesSize( BPy_Operators* self) {
	return PyInt_FromLong( Operators::getViewEdgesSize() );
}

PyObject * Operators_getChainsSize( BPy_Operators* self ) {
	return PyInt_FromLong( Operators::getChainsSize() );
}

PyObject * Operators_getStrokesSize( BPy_Operators* self) {
	return PyInt_FromLong( Operators::getStrokesSize() );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


