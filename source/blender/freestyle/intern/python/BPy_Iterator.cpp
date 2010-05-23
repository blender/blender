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

//-------------------MODULE INITIALIZATION--------------------------------
int Iterator_Init( PyObject *module )
{	
	if( module == NULL )
		return -1;

	if( PyType_Ready( &Iterator_Type ) < 0 )
		return -1;
	Py_INCREF( &Iterator_Type );
	PyModule_AddObject(module, "Iterator", (PyObject *)&Iterator_Type);

	if( PyType_Ready( &AdjacencyIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &AdjacencyIterator_Type );
	PyModule_AddObject(module, "AdjacencyIterator", (PyObject *)&AdjacencyIterator_Type);

	if( PyType_Ready( &Interface0DIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &Interface0DIterator_Type );
	PyModule_AddObject(module, "Interface0DIterator", (PyObject *)&Interface0DIterator_Type);
	
	if( PyType_Ready( &CurvePointIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &CurvePointIterator_Type );
	PyModule_AddObject(module, "CurvePointIterator", (PyObject *)&CurvePointIterator_Type);
	
	if( PyType_Ready( &StrokeVertexIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &StrokeVertexIterator_Type );
	PyModule_AddObject(module, "StrokeVertexIterator", (PyObject *)&StrokeVertexIterator_Type);
	
	if( PyType_Ready( &SVertexIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &SVertexIterator_Type );
	PyModule_AddObject(module, "SVertexIterator", (PyObject *)&SVertexIterator_Type);
	
	if( PyType_Ready( &orientedViewEdgeIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &orientedViewEdgeIterator_Type );
	PyModule_AddObject(module, "orientedViewEdgeIterator", (PyObject *)&orientedViewEdgeIterator_Type);
	
	if( PyType_Ready( &ViewEdgeIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &ViewEdgeIterator_Type );
	PyModule_AddObject(module, "ViewEdgeIterator", (PyObject *)&ViewEdgeIterator_Type);
	
	if( PyType_Ready( &ChainingIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &ChainingIterator_Type );
	PyModule_AddObject(module, "ChainingIterator", (PyObject *)&ChainingIterator_Type);
	
	if( PyType_Ready( &ChainPredicateIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &ChainPredicateIterator_Type );
	PyModule_AddObject(module, "ChainPredicateIterator", (PyObject *)&ChainPredicateIterator_Type);
	
	if( PyType_Ready( &ChainSilhouetteIterator_Type ) < 0 )
		return -1;
	Py_INCREF( &ChainSilhouetteIterator_Type );
	PyModule_AddObject(module, "ChainSilhouetteIterator", (PyObject *)&ChainSilhouetteIterator_Type);
	
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char Iterator___doc__[] =
"Base class to define iterators.\n";

static void Iterator___dealloc__(BPy_Iterator* self)
{
	if (self->it)
		delete self->it;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * Iterator___repr__(BPy_Iterator* self)
{
    return PyUnicode_FromFormat("type: %s - address: %p", self->it->getExactTypeName().c_str(), self->it );
}

static char Iterator_getExactTypeName___doc__[] =
".. method:: getExactTypeName()\n"
"\n"
"   Returns the name of the iterator.\n"
"\n"
"   :return: The name of the iterator.\n"
"   :rtype: string\n";

static PyObject * Iterator_getExactTypeName(BPy_Iterator* self) {
	return PyUnicode_FromString( self->it->getExactTypeName().c_str() );	
}

static char Iterator_increment___doc__[] =
".. method:: increment()\n"
"\n"
"   Makes the iterator point the next element.\n";

static PyObject * Iterator_increment(BPy_Iterator* self) {
	if (self->it->isEnd()) {
		PyErr_SetString(PyExc_RuntimeError , "cannot increment any more");
		return NULL;
	}
	self->it->increment();
		
	Py_RETURN_NONE;
}

static char Iterator_decrement___doc__[] =
".. method:: decrement()\n"
"\n"
"   Makes the iterator point the previous element.\n";

static PyObject * Iterator_decrement(BPy_Iterator* self) {
	if (self->it->isBegin()) {
		PyErr_SetString(PyExc_RuntimeError , "cannot decrement any more");
		return NULL;
	}
	self->it->decrement();
		
	Py_RETURN_NONE;
}

static char Iterator_isBegin___doc__[] =
".. method:: isBegin()\n"
"\n"
"   Returns true if the interator points the first element.\n"
"\n"
"   :return: True if the interator points the first element.\n"
"   :rtype: bool\n";

static PyObject * Iterator_isBegin(BPy_Iterator* self) {
	return PyBool_from_bool( self->it->isBegin() );
}

static char Iterator_isEnd___doc__[] =
".. method:: isEnd()\n"
"\n"
"   Returns true if the interator points the last element.\n"
"\n"
"   :return: True if the interator points the last element.\n"
"   :rtype: bool\n";

static PyObject * Iterator_isEnd(BPy_Iterator* self) {
	return PyBool_from_bool( self->it->isEnd() );
}

/*----------------------Iterator instance definitions ----------------------------*/
static PyMethodDef BPy_Iterator_methods[] = {
	{"getExactTypeName", ( PyCFunction ) Iterator_getExactTypeName, METH_NOARGS, Iterator_getExactTypeName___doc__},
	{"increment", ( PyCFunction ) Iterator_increment, METH_NOARGS, Iterator_increment___doc__},
	{"decrement", ( PyCFunction ) Iterator_decrement, METH_NOARGS, Iterator_decrement___doc__},
	{"isBegin", ( PyCFunction ) Iterator_isBegin, METH_NOARGS, Iterator_isBegin___doc__},
	{"isEnd", ( PyCFunction ) Iterator_isEnd, METH_NOARGS, Iterator_isEnd___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Iterator type definition ------------------------------*/

PyTypeObject Iterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Iterator",                     /* tp_name */
	sizeof(BPy_Iterator),           /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)Iterator___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)Iterator___repr__,    /* tp_repr */
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
	Iterator___doc__,               /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Iterator_methods,           /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	0,                              /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


