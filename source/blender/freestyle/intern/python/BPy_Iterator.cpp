#include "BPy_Iterator.h"

#include "BPy_Convert.h"
#include "Iterator/BPy_AdjacencyIterator.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "Iterator/BPy_CurvePointIterator.h"
#include "Iterator/BPy_StrokeVertexIterator.h"
#include "Iterator/BPy_SVertexIterator.h"
#include "Iterator/BPy_orientedViewEdgeIterator.h"
#include "Iterator/BPy_ViewEdgeIterator.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "Iterator/BPy_ChainPredicateIterator.h"
#include "Iterator/BPy_ChainSilhouetteIterator.h"


	
#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Iterator instance  -----------*/
static void Iterator___dealloc__(BPy_Iterator *self);
static PyObject * Iterator___repr__(BPy_Iterator* self);

static PyObject * Iterator_getExactTypeName(BPy_Iterator* self);
static PyObject * Iterator_increment(BPy_Iterator* self);
static PyObject * Iterator_decrement(BPy_Iterator* self);
static PyObject * Iterator_isBegin(BPy_Iterator* self);
static PyObject * Iterator_isEnd(BPy_Iterator* self);

/*----------------------Iterator instance definitions ----------------------------*/
static PyMethodDef BPy_Iterator_methods[] = {
	{"getExactTypeName", ( PyCFunction ) Iterator_getExactTypeName, METH_NOARGS, "（ ）Returns the string of the name of the iterator."},
	{"increment", ( PyCFunction ) Iterator_increment, METH_NOARGS, "（ ）Increments iterator."},
	{"decrement", ( PyCFunction ) Iterator_decrement, METH_NOARGS, "（ ）Decrements iterator."},
	{"isBegin", ( PyCFunction ) Iterator_isBegin, METH_NOARGS, "（ ）Tests if iterator points to beginning."},
	{"isEnd", ( PyCFunction ) Iterator_isEnd, METH_NOARGS, "（ ）Tests if iterator points to end."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Iterator type definition ------------------------------*/

PyTypeObject Iterator_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Iterator",				/* tp_name */
	sizeof( BPy_Iterator ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)Iterator___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)Iterator___repr__,					/* tp_repr */

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
	BPy_Iterator_methods,	/* struct PyMethodDef *tp_methods; */
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
PyMODINIT_FUNC Iterator_Init( PyObject *module )
{	
	if( module == NULL )
		return;

	if( PyType_Ready( &Iterator_Type ) < 0 )
		return;
	Py_INCREF( &Iterator_Type );
	PyModule_AddObject(module, "Iterator", (PyObject *)&Iterator_Type);

	if( PyType_Ready( &AdjacencyIterator_Type ) < 0 )
		return;
	Py_INCREF( &AdjacencyIterator_Type );
	PyModule_AddObject(module, "AdjacencyIterator", (PyObject *)&AdjacencyIterator_Type);

	if( PyType_Ready( &Interface0DIterator_Type ) < 0 )
		return;
	Py_INCREF( &Interface0DIterator_Type );
	PyModule_AddObject(module, "Interface0DIterator", (PyObject *)&Interface0DIterator_Type);
	
	if( PyType_Ready( &CurvePointIterator_Type ) < 0 )
		return;
	Py_INCREF( &CurvePointIterator_Type );
	PyModule_AddObject(module, "CurvePointIterator", (PyObject *)&CurvePointIterator_Type);
	
	if( PyType_Ready( &StrokeVertexIterator_Type ) < 0 )
		return;
	Py_INCREF( &StrokeVertexIterator_Type );
	PyModule_AddObject(module, "StrokeVertexIterator", (PyObject *)&StrokeVertexIterator_Type);
	
	if( PyType_Ready( &SVertexIterator_Type ) < 0 )
		return;
	Py_INCREF( &SVertexIterator_Type );
	PyModule_AddObject(module, "SVertexIterator", (PyObject *)&SVertexIterator_Type);
	
	if( PyType_Ready( &orientedViewEdgeIterator_Type ) < 0 )
		return;
	Py_INCREF( &orientedViewEdgeIterator_Type );
	PyModule_AddObject(module, "orientedViewEdgeIterator", (PyObject *)&orientedViewEdgeIterator_Type);
	
	if( PyType_Ready( &ViewEdgeIterator_Type ) < 0 )
		return;
	Py_INCREF( &ViewEdgeIterator_Type );
	PyModule_AddObject(module, "ViewEdgeIterator", (PyObject *)&ViewEdgeIterator_Type);
	
	if( PyType_Ready( &ChainingIterator_Type ) < 0 )
		return;
	Py_INCREF( &ChainingIterator_Type );
	PyModule_AddObject(module, "ChainingIterator", (PyObject *)&ChainingIterator_Type);
	
	if( PyType_Ready( &ChainPredicateIterator_Type ) < 0 )
		return;
	Py_INCREF( &ChainPredicateIterator_Type );
	PyModule_AddObject(module, "ChainPredicateIterator", (PyObject *)&ChainPredicateIterator_Type);
	
	if( PyType_Ready( &ChainSilhouetteIterator_Type ) < 0 )
		return;
	Py_INCREF( &ChainSilhouetteIterator_Type );
	PyModule_AddObject(module, "ChainSilhouetteIterator", (PyObject *)&ChainSilhouetteIterator_Type);
	
	
}

//------------------------INSTANCE METHODS ----------------------------------

void Iterator___dealloc__(BPy_Iterator* self)
{
	delete self->it;
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * Iterator___repr__(BPy_Iterator* self)
{
    return PyString_FromFormat("type: %s - address: %p", self->it->getExactTypeName().c_str(), self->it );
}

PyObject * Iterator_getExactTypeName(BPy_Iterator* self) {
	return PyString_FromString( self->it->getExactTypeName().c_str() );	
}


PyObject * Iterator_increment(BPy_Iterator* self) {
	self->it->increment();
		
	Py_RETURN_NONE;
}

PyObject * Iterator_decrement(BPy_Iterator* self) {
	self->it->decrement();
		
	Py_RETURN_NONE;
}

PyObject * Iterator_isBegin(BPy_Iterator* self) {
	return PyBool_from_bool( self->it->isBegin() );
}

PyObject * Iterator_isEnd(BPy_Iterator* self) {
	return PyBool_from_bool( self->it->isEnd() );
}

PyObject * Iterator_getObject(BPy_Iterator* self) {
	return PyBool_from_bool( self->it->isEnd() );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


