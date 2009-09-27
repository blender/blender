#include "BPy_ChainingIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_ViewVertex.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "BPy_AdjacencyIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ChainingIterator instance  -----------*/
static int ChainingIterator___init__(BPy_ChainingIterator *self, PyObject *args);
static PyObject * ChainingIterator_init( BPy_ChainingIterator *self );
static PyObject * ChainingIterator_traverse( BPy_ChainingIterator *self, PyObject *args );
static PyObject * ChainingIterator_getVertex( BPy_ChainingIterator *self );
static PyObject * ChainingIterator_isIncrementing( BPy_ChainingIterator *self );

static PyObject * ChainingIterator_getObject( BPy_ChainingIterator *self);

/*----------------------ChainingIterator instance definitions ----------------------------*/
static PyMethodDef BPy_ChainingIterator_methods[] = {
	{"init", ( PyCFunction ) ChainingIterator_init, METH_NOARGS, "() Inits the iterator context. This method is called each time a new chain is started. It can be used to reset some history information that you might want to keep."},
	{"traverse", ( PyCFunction ) ChainingIterator_traverse, METH_VARARGS, "(AdjacencyIterator ai) This method iterates over the potential next ViewEdges and returns the one that will be followed next. Returns the next ViewEdge to follow or 0 when the end of the chain is reached. "},
	{"getVertex", ( PyCFunction ) ChainingIterator_getVertex, METH_NOARGS, "() Returns the vertex which is the next crossing "},
	{"isIncrementing", ( PyCFunction ) ChainingIterator_isIncrementing, METH_NOARGS, "() Returns true if the current iteration is an incrementation."},
	{"getObject", ( PyCFunction ) ChainingIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ChainingIterator type definition ------------------------------*/

PyTypeObject ChainingIterator_Type = {
	PyObject_HEAD_INIT(NULL)
	"ChainingIterator",             /* tp_name */
	sizeof(BPy_ChainingIterator),   /* tp_basicsize */
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
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"ChainingIterator objects",     /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ChainingIterator_methods,   /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&ViewEdgeIterator_Type,         /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ChainingIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int ChainingIterator___init__(BPy_ChainingIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0;

	if (!( PyArg_ParseTuple(args, "|OOOO", &obj1, &obj2, &obj3, &obj4) ))
	    return -1;

	if( obj1 && BPy_ChainingIterator_Check(obj1)  ) {
		self->c_it = new ChainingIterator(*( ((BPy_ChainingIterator *) obj1)->c_it ));
	
	} else {
		bool restrictToSelection = ( obj1 ) ? bool_from_PyBool(obj1) : true;
		bool restrictToUnvisited = ( obj2 ) ? bool_from_PyBool(obj2) : true;
		ViewEdge *begin;
		if ( !obj3 || obj3 == Py_None )
			begin = NULL;
		else if ( BPy_ViewEdge_Check(obj3) )
			begin = ((BPy_ViewEdge *) obj3)->ve;
		else {
			PyErr_SetString(PyExc_TypeError, "3rd argument must be either a ViewEdge object or None");
			return -1;
		}
		bool orientation = ( obj4 ) ? bool_from_PyBool(obj4) : true;
		
		self->c_it = new ChainingIterator( restrictToSelection, restrictToUnvisited, begin, orientation);	
	}
	
	self->py_ve_it.ve_it = self->c_it;
	self->py_ve_it.py_it.it = self->c_it;
	
	self->c_it->py_c_it = (PyObject *) self;
	
	return 0;
}

PyObject *ChainingIterator_init( BPy_ChainingIterator *self ) {
	if( typeid(*(self->c_it)) == typeid(ChainingIterator) ) {
		PyErr_SetString(PyExc_TypeError, "init() method not properly overridden");
		return NULL;
	}
	self->c_it->init();
	
	Py_RETURN_NONE;
}

PyObject *ChainingIterator_traverse( BPy_ChainingIterator *self, PyObject *args ) {
	PyObject *py_a_it;

	if(!( PyArg_ParseTuple(args, "O!", &AdjacencyIterator_Type, &py_a_it) ))
		return NULL;
	
	if( typeid(*(self->c_it)) == typeid(ChainingIterator) ) {
		PyErr_SetString(PyExc_TypeError, "traverse() method not properly overridden");
		return NULL;
	}
	if( ((BPy_AdjacencyIterator *) py_a_it)->a_it )
		self->c_it->traverse(*( ((BPy_AdjacencyIterator *) py_a_it)->a_it ));
		
	Py_RETURN_NONE;
}


PyObject *ChainingIterator_getVertex( BPy_ChainingIterator *self ) {
	ViewVertex *v = self->c_it->getVertex();
	if( v )
		return Any_BPy_ViewVertex_from_ViewVertex( *v );
		
	Py_RETURN_NONE;
}

PyObject *ChainingIterator_isIncrementing( BPy_ChainingIterator *self ) {
	return PyBool_from_bool( self->c_it->isIncrementing() );
}

PyObject * ChainingIterator_getObject( BPy_ChainingIterator *self) {
	
	ViewEdge *ve = self->c_it->operator*();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}



///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
