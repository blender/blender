#include "BPy_CurvePointIterator.h"

#include "../BPy_Convert.h"
#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for CurvePointIterator instance  -----------*/
static int CurvePointIterator___init__(BPy_CurvePointIterator *self, PyObject *args);
static PyObject * CurvePointIterator_t( BPy_CurvePointIterator *self );
static PyObject * CurvePointIterator_u( BPy_CurvePointIterator *self );
static PyObject * CurvePointIterator_castToInterface0DIterator( BPy_CurvePointIterator *self );

static PyObject * CurvePointIterator_getObject(BPy_CurvePointIterator *self);

/*----------------------CurvePointIterator instance definitions ----------------------------*/
static PyMethodDef BPy_CurvePointIterator_methods[] = {
	{"t", ( PyCFunction ) CurvePointIterator_t, METH_NOARGS, "() Returns the curvilinear abscissa."},
	{"u", ( PyCFunction ) CurvePointIterator_u, METH_NOARGS, "() Returns the point parameter in the curve 0<=u<=1."},
	{"castToInterface0DIterator", ( PyCFunction ) CurvePointIterator_castToInterface0DIterator, METH_NOARGS, "() Casts this CurvePointIterator into an Interface0DIterator. Useful for any call to a function of the type UnaryFunction0D."},
	{"getObject", ( PyCFunction ) CurvePointIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_CurvePointIterator type definition ------------------------------*/

PyTypeObject CurvePointIterator_Type = {
	PyObject_HEAD_INIT(NULL)
	"CurvePointIterator",           /* tp_name */
	sizeof(BPy_CurvePointIterator), /* tp_basicsize */
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
	"CurvePointIterator objects",   /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_CurvePointIterator_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)CurvePointIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int CurvePointIterator___init__(BPy_CurvePointIterator *self, PyObject *args )
{	
	PyObject *obj = 0;

	if (! PyArg_ParseTuple(args, "|O", &obj) )
	    return -1;

	if( !obj ){
		self->cp_it = new CurveInternal::CurvePointIterator();
		
	} else if( BPy_CurvePointIterator_Check(obj) ) {
		self->cp_it = new CurveInternal::CurvePointIterator(*( ((BPy_CurvePointIterator *) obj)->cp_it ));
	
	} else if( PyFloat_Check(obj) ) {
		self->cp_it = new CurveInternal::CurvePointIterator( PyFloat_AsDouble(obj) );
			
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return -1;
	}

	self->py_it.it = self->cp_it;

	return 0;
}

PyObject * CurvePointIterator_t( BPy_CurvePointIterator *self ) {
	return PyFloat_FromDouble( self->cp_it->t() );
}

PyObject * CurvePointIterator_u( BPy_CurvePointIterator *self ) {
	return PyFloat_FromDouble( self->cp_it->u() );
}

PyObject * CurvePointIterator_castToInterface0DIterator( BPy_CurvePointIterator *self ) {
	Interface0DIterator it( self->cp_it->castToInterface0DIterator() );
	return BPy_Interface0DIterator_from_Interface0DIterator( it, 0 );
}

PyObject * CurvePointIterator_getObject(BPy_CurvePointIterator *self) {
	return BPy_CurvePoint_from_CurvePoint( self->cp_it->operator*() );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
