#include "BPy_StrokeVertexIterator.h"

#include "../BPy_Convert.h"
#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for StrokeVertexIterator instance  -----------*/
static int StrokeVertexIterator___init__(BPy_StrokeVertexIterator *self, PyObject *args);
static PyObject * StrokeVertexIterator_iternext( BPy_StrokeVertexIterator *self );
static PyObject * StrokeVertexIterator_t( BPy_StrokeVertexIterator *self );
static PyObject * StrokeVertexIterator_u( BPy_StrokeVertexIterator *self );
static PyObject * StrokeVertexIterator_castToInterface0DIterator( BPy_StrokeVertexIterator *self );

static PyObject * StrokeVertexIterator_getObject( BPy_StrokeVertexIterator *self);


/*----------------------StrokeVertexIterator instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeVertexIterator_methods[] = {
	{"t", ( PyCFunction ) StrokeVertexIterator_t, METH_NOARGS, "() Returns the curvilinear abscissa."},
	{"u", ( PyCFunction ) StrokeVertexIterator_u, METH_NOARGS, "() Returns the point parameter in the curve 0<=u<=1."},
	{"castToInterface0DIterator", ( PyCFunction ) StrokeVertexIterator_castToInterface0DIterator, METH_NOARGS, "() Casts this StrokeVertexIterator into an Interface0DIterator. Useful for any call to a function of the type UnaryFunction0D."},
	{"getObject", ( PyCFunction ) StrokeVertexIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_StrokeVertexIterator type definition ------------------------------*/

PyTypeObject StrokeVertexIterator_Type = {
	PyObject_HEAD_INIT(NULL)
	"StrokeVertexIterator",         /* tp_name */
	sizeof(BPy_StrokeVertexIterator), /* tp_basicsize */
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
	"StrokeVertexIterator objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	PyObject_SelfIter,              /* tp_iter */
	(iternextfunc)StrokeVertexIterator_iternext, /* tp_iternext */
	BPy_StrokeVertexIterator_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)StrokeVertexIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int StrokeVertexIterator___init__(BPy_StrokeVertexIterator *self, PyObject *args )
{	
	PyObject *obj = 0;

	if (! PyArg_ParseTuple(args, "|O", &obj) )
	    return -1;

	if( !obj ){
		self->sv_it = new StrokeInternal::StrokeVertexIterator();
		
	} else if( BPy_StrokeVertexIterator_Check(obj) ) {
		self->sv_it = new StrokeInternal::StrokeVertexIterator(*( ((BPy_StrokeVertexIterator *) obj)->sv_it ));
	
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return -1;
	}

	self->py_it.it = self->sv_it;
	self->reversed = 0;

	return 0;
}

PyObject * StrokeVertexIterator_iternext( BPy_StrokeVertexIterator *self ) {
	StrokeVertex *sv;
	if (self->reversed) {
		if (self->sv_it->isBegin()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		self->sv_it->decrement();
		sv = self->sv_it->operator->();
	} else {
		if (self->sv_it->isEnd()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		sv = self->sv_it->operator->();
		self->sv_it->increment();
	}
	return BPy_StrokeVertex_from_StrokeVertex( *sv );
}

PyObject * StrokeVertexIterator_t( BPy_StrokeVertexIterator *self ) {
	return PyFloat_FromDouble( self->sv_it->t() );
}

PyObject * StrokeVertexIterator_u( BPy_StrokeVertexIterator *self ) {
	return PyFloat_FromDouble( self->sv_it->u() );
}

PyObject * StrokeVertexIterator_castToInterface0DIterator( BPy_StrokeVertexIterator *self ) {
	Interface0DIterator it( self->sv_it->castToInterface0DIterator() );
	return BPy_Interface0DIterator_from_Interface0DIterator( it, 0 );
}

PyObject * StrokeVertexIterator_getObject( BPy_StrokeVertexIterator *self) {
	StrokeVertex *sv = self->sv_it->operator->();
	if( sv )	
		return BPy_StrokeVertex_from_StrokeVertex( *sv );

	Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
