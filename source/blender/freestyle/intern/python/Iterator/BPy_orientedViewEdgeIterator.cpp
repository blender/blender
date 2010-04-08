#include "BPy_orientedViewEdgeIterator.h"

#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for orientedViewEdgeIterator instance  -----------*/
static int orientedViewEdgeIterator___init__(BPy_orientedViewEdgeIterator *self, PyObject *args);
static PyObject * orientedViewEdgeIterator_iternext(BPy_orientedViewEdgeIterator *self);

static PyObject * orientedViewEdgeIterator_getObject(BPy_orientedViewEdgeIterator *self);


/*----------------------orientedViewEdgeIterator instance definitions ----------------------------*/
static PyMethodDef BPy_orientedViewEdgeIterator_methods[] = {
	{"getObject", ( PyCFunction ) orientedViewEdgeIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_orientedViewEdgeIterator type definition ------------------------------*/

PyTypeObject orientedViewEdgeIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"orientedViewEdgeIterator",     /* tp_name */
	sizeof(BPy_orientedViewEdgeIterator), /* tp_basicsize */
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
	"orientedViewEdgeIterator objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	PyObject_SelfIter,              /* tp_iter */
	(iternextfunc)orientedViewEdgeIterator_iternext, /* tp_iternext */
	BPy_orientedViewEdgeIterator_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)orientedViewEdgeIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int orientedViewEdgeIterator___init__(BPy_orientedViewEdgeIterator *self, PyObject *args )
{	
	PyObject *obj = 0;

	if (!( PyArg_ParseTuple(args, "|O", &obj) ))
	    return -1;

	if( !obj )
		self->ove_it = new ViewVertexInternal::orientedViewEdgeIterator();
	else if( BPy_orientedViewEdgeIterator_Check(obj) )
		self->ove_it = new ViewVertexInternal::orientedViewEdgeIterator(*( ((BPy_orientedViewEdgeIterator *) obj)->ove_it ));
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return -1;
	}
	
	self->py_it.it = self->ove_it;
	self->reversed = 0;
	
	return 0;
}

PyObject * orientedViewEdgeIterator_iternext( BPy_orientedViewEdgeIterator *self ) {
	ViewVertex::directedViewEdge *dve;
	if (self->reversed) {
		if (self->ove_it->isBegin()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		self->ove_it->decrement();
		dve = self->ove_it->operator->();
	} else {
		if (self->ove_it->isEnd()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		dve = self->ove_it->operator->();
		self->ove_it->increment();
	}
	return BPy_directedViewEdge_from_directedViewEdge( *dve );
}

PyObject * orientedViewEdgeIterator_getObject( BPy_orientedViewEdgeIterator *self) {
	return BPy_directedViewEdge_from_directedViewEdge( self->ove_it->operator*() );
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
