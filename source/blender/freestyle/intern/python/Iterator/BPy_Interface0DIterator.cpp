#include "BPy_Interface0DIterator.h"

#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Interface0DIterator instance  -----------*/
static int Interface0DIterator___init__(BPy_Interface0DIterator *self, PyObject *args);
static PyObject * Interface0DIterator_iternext( BPy_Interface0DIterator *self );

static PyObject * Interface0DIterator_t( BPy_Interface0DIterator *self );
static PyObject * Interface0DIterator_u( BPy_Interface0DIterator *self );

static PyObject * Interface0DIterator_getObject(BPy_Interface0DIterator *self);

/*----------------------Interface0DIterator instance definitions ----------------------------*/
static PyMethodDef BPy_Interface0DIterator_methods[] = {
	{"t", ( PyCFunction ) Interface0DIterator_t, METH_NOARGS, "() Returns the curvilinear abscissa."},
	{"u", ( PyCFunction ) Interface0DIterator_u, METH_NOARGS, "() Returns the point parameter in the curve 0<=u<=1."},
	{"getObject", ( PyCFunction ) Interface0DIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Interface0DIterator type definition ------------------------------*/

PyTypeObject Interface0DIterator_Type = {
	PyObject_HEAD_INIT(NULL)
	"Interface0DIterator",          /* tp_name */
	sizeof(BPy_Interface0DIterator), /* tp_basicsize */
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
	"Interface0DIterator objects",  /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	PyObject_SelfIter,              /* tp_iter */
	(iternextfunc)Interface0DIterator_iternext, /* tp_iternext */
	BPy_Interface0DIterator_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Interface0DIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int Interface0DIterator___init__(BPy_Interface0DIterator *self, PyObject *args )
{	
	PyObject *obj = 0;

	if (!( PyArg_ParseTuple(args, "O!", &Interface0DIterator_Type, &obj) ))
	    return -1;

	self->if0D_it = new Interface0DIterator(*( ((BPy_Interface0DIterator *) obj)->if0D_it ));
	self->py_it.it = self->if0D_it;
	self->reversed = 0;
	
	return 0;
}

PyObject * Interface0DIterator_iternext( BPy_Interface0DIterator *self ) {
	Interface0D *if0D;
	if (self->reversed) {
		if (self->if0D_it->isBegin()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		self->if0D_it->decrement();
		if0D = self->if0D_it->operator->();
	} else {
		if (self->if0D_it->isEnd()) {
			PyErr_SetNone(PyExc_StopIteration);
			return NULL;
		}
		if0D = self->if0D_it->operator->();
		self->if0D_it->increment();
	}
	return Any_BPy_Interface0D_from_Interface0D( *if0D );
}

PyObject * Interface0DIterator_t( BPy_Interface0DIterator *self ) {
	return PyFloat_FromDouble( self->if0D_it->t() );
}

PyObject * Interface0DIterator_u( BPy_Interface0DIterator *self ) {
	return PyFloat_FromDouble( self->if0D_it->u() );
}

PyObject * Interface0DIterator_getObject(BPy_Interface0DIterator *self) {
	return Any_BPy_Interface0D_from_Interface0D( self->if0D_it->operator*() );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
