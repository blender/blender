#include "BPy_AdjacencyIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_ViewVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for AdjacencyIterator instance  -----------*/
static int AdjacencyIterator___init__(BPy_AdjacencyIterator *self, PyObject *args);
static PyObject * AdjacencyIterator_iternext(BPy_AdjacencyIterator *self);

static PyObject * AdjacencyIterator_isIncoming(BPy_AdjacencyIterator *self);
static PyObject * AdjacencyIterator_getObject(BPy_AdjacencyIterator *self);

/*----------------------AdjacencyIterator instance definitions ----------------------------*/
static PyMethodDef BPy_AdjacencyIterator_methods[] = {
	{"isIncoming", ( PyCFunction ) AdjacencyIterator_isIncoming, METH_NOARGS, "() Returns true if the current ViewEdge is is coming towards the iteration vertex. False otherwise."},
	{"getObject", ( PyCFunction ) AdjacencyIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_AdjacencyIterator type definition ------------------------------*/

PyTypeObject AdjacencyIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"AdjacencyIterator",            /* tp_name */
	sizeof(BPy_AdjacencyIterator),  /* tp_basicsize */
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
	"AdjacencyIterator objects",    /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	PyObject_SelfIter,              /* tp_iter */
	(iternextfunc)AdjacencyIterator_iternext, /* tp_iternext */
	BPy_AdjacencyIterator_methods,  /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)AdjacencyIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int AdjacencyIterator___init__(BPy_AdjacencyIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0;

	if (! PyArg_ParseTuple(args, "|OOO", &obj1, &obj2, &obj3) )
	    return -1;

	if( !obj1 && !obj2 && !obj3 ){
		self->a_it = new AdjacencyIterator();
		
	} else if( BPy_AdjacencyIterator_Check(obj1) ) {
		self->a_it = new AdjacencyIterator(*( ((BPy_AdjacencyIterator *) obj1)->a_it ));
	
	} else if( BPy_ViewVertex_Check(obj1) ) {
		bool restrictToSelection = ( obj2 ) ? bool_from_PyBool(obj2) : true;
		bool restrictToUnvisited = ( obj3 ) ? bool_from_PyBool(obj3) : true;
		
		self->a_it = new AdjacencyIterator( ((BPy_ViewVertex *) obj1)->vv, restrictToSelection, restrictToUnvisited );
			
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_it.it = self->a_it;
	
	return 0;

}

PyObject * AdjacencyIterator_iternext(BPy_AdjacencyIterator *self) {
	if (self->a_it->isEnd()) {
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}
	ViewEdge *ve = self->a_it->operator*();
	self->a_it->increment();
	return BPy_ViewEdge_from_ViewEdge( *ve );
}

PyObject * AdjacencyIterator_isIncoming(BPy_AdjacencyIterator *self) {
	return PyBool_from_bool(self->a_it->isIncoming());
}

PyObject * AdjacencyIterator_getObject(BPy_AdjacencyIterator *self) {
	
	ViewEdge *ve = self->a_it->operator*();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
