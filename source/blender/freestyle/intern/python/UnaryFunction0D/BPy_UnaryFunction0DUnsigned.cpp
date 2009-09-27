#include "BPy_UnaryFunction0DUnsigned.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_unsigned_int/BPy_QuantitativeInvisibilityF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction0DUnsigned instance  -----------*/
static int UnaryFunction0DUnsigned___init__(BPy_UnaryFunction0DUnsigned* self, PyObject *args, PyObject *kwds);
static void UnaryFunction0DUnsigned___dealloc__(BPy_UnaryFunction0DUnsigned* self);
static PyObject * UnaryFunction0DUnsigned___repr__(BPy_UnaryFunction0DUnsigned* self);

static PyObject * UnaryFunction0DUnsigned_getName( BPy_UnaryFunction0DUnsigned *self);
static PyObject * UnaryFunction0DUnsigned___call__( BPy_UnaryFunction0DUnsigned *self, PyObject *args, PyObject *kwds);

/*----------------------UnaryFunction0DUnsigned instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction0DUnsigned_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction0DUnsigned_getName, METH_NOARGS, "() Returns the string of the name of the unary 0D function."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction0DUnsigned type definition ------------------------------*/

PyTypeObject UnaryFunction0DUnsigned_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction0DUnsigned",      /* tp_name */
	sizeof(BPy_UnaryFunction0DUnsigned), /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction0DUnsigned___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction0DUnsigned___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryFunction0DUnsigned___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"UnaryFunction0DUnsigned objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryFunction0DUnsigned_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0D_Type,          /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryFunction0DUnsigned___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DUnsigned_Init( PyObject *module ) {

	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction0DUnsigned_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction0DUnsigned_Type );
	PyModule_AddObject(module, "UnaryFunction0DUnsigned", (PyObject *)&UnaryFunction0DUnsigned_Type);
	
	if( PyType_Ready( &QuantitativeInvisibilityF0D_Type ) < 0 )
		return -1;
	Py_INCREF( &QuantitativeInvisibilityF0D_Type );
	PyModule_AddObject(module, "QuantitativeInvisibilityF0D", (PyObject *)&QuantitativeInvisibilityF0D_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryFunction0DUnsigned___init__(BPy_UnaryFunction0DUnsigned* self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->uf0D_unsigned = new UnaryFunction0D<unsigned int>();
	self->uf0D_unsigned->py_uf0D = (PyObject *)self;
	return 0;
}

void UnaryFunction0DUnsigned___dealloc__(BPy_UnaryFunction0DUnsigned* self)
{
	if (self->uf0D_unsigned)
		delete self->uf0D_unsigned;
	UnaryFunction0D_Type.tp_dealloc((PyObject*)self);
}


PyObject * UnaryFunction0DUnsigned___repr__(BPy_UnaryFunction0DUnsigned* self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", self->uf0D_unsigned->getName().c_str(), self->uf0D_unsigned );
}

PyObject * UnaryFunction0DUnsigned_getName( BPy_UnaryFunction0DUnsigned *self )
{
	return PyUnicode_FromFormat( self->uf0D_unsigned->getName().c_str() );
}

PyObject * UnaryFunction0DUnsigned___call__( BPy_UnaryFunction0DUnsigned *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if(!PyArg_ParseTuple(args, "O!", &Interface0DIterator_Type, &obj))
		return NULL;
	
	if( typeid(*(self->uf0D_unsigned)) == typeid(UnaryFunction0D<unsigned int>) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->uf0D_unsigned->operator()(*( ((BPy_Interface0DIterator *) obj)->if0D_it )) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->uf0D_unsigned->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	return PyLong_FromLong( self->uf0D_unsigned->result );

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
