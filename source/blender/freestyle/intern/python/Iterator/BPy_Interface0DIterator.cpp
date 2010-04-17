#include "BPy_Interface0DIterator.h"

#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char Interface0DIterator___doc__[] =
"Class defining an iterator over Interface0D elements.  An instance of\n"
"this iterator is always obtained from a 1D element.\n"
"\n"
".. method:: __init__(it)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`Interface0DIterator`\n";

static int Interface0DIterator___init__(BPy_Interface0DIterator *self, PyObject *args )
{	
	PyObject *obj = 0;

	if (!( PyArg_ParseTuple(args, "O!", &Interface0DIterator_Type, &obj) ))
	    return -1;

	self->if0D_it = new Interface0DIterator(*( ((BPy_Interface0DIterator *) obj)->if0D_it ));
	self->py_it.it = self->if0D_it;
	self->reversed = 0;
	
	return 0;
}

static PyObject * Interface0DIterator_iternext( BPy_Interface0DIterator *self ) {
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

static char Interface0DIterator_t___doc__[] =
".. method:: t()\n"
"\n"
"   Returns the curvilinear abscissa.\n"
"\n"
"   :return: The curvilinear abscissa.\n"
"   :rtype: float\n";

static PyObject * Interface0DIterator_t( BPy_Interface0DIterator *self ) {
	return PyFloat_FromDouble( self->if0D_it->t() );
}

static char Interface0DIterator_u___doc__[] =
".. method:: u()\n"
"\n"
"   Returns the point parameter in the curve 0<=u<=1.\n"
"\n"
"   :return: The point parameter.\n"
"   :rtype: float\n";

static PyObject * Interface0DIterator_u( BPy_Interface0DIterator *self ) {
	return PyFloat_FromDouble( self->if0D_it->u() );
}

static char Interface0DIterator_getObject___doc__[] =
".. method:: getObject()\n"
"\n"
"   Returns the pointed Interface0D.\n"
"\n"
"   :return: The pointed Interface0D.\n"
"   :rtype: :class:`Interface0D`\n";

static PyObject * Interface0DIterator_getObject(BPy_Interface0DIterator *self) {
	return Any_BPy_Interface0D_from_Interface0D( self->if0D_it->operator*() );
}

/*----------------------Interface0DIterator instance definitions ----------------------------*/
static PyMethodDef BPy_Interface0DIterator_methods[] = {
	{"t", ( PyCFunction ) Interface0DIterator_t, METH_NOARGS, Interface0DIterator_t___doc__},
	{"u", ( PyCFunction ) Interface0DIterator_u, METH_NOARGS, Interface0DIterator_u___doc__},
	{"getObject", ( PyCFunction ) Interface0DIterator_getObject, METH_NOARGS, Interface0DIterator_getObject___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Interface0DIterator type definition ------------------------------*/

PyTypeObject Interface0DIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
	Interface0DIterator___doc__,    /* tp_doc */
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

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
