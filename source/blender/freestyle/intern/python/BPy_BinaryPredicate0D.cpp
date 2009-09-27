#include "BPy_BinaryPredicate0D.h"

#include "BPy_Convert.h"
#include "BPy_Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for BinaryPredicate0D instance  -----------*/
static int BinaryPredicate0D___init__(BPy_BinaryPredicate0D *self, PyObject *args, PyObject *kwds);
static void BinaryPredicate0D___dealloc__(BPy_BinaryPredicate0D *self);
static PyObject * BinaryPredicate0D___repr__(BPy_BinaryPredicate0D *self);

static PyObject * BinaryPredicate0D_getName( BPy_BinaryPredicate0D *self, PyObject *args);
static PyObject * BinaryPredicate0D___call__( BPy_BinaryPredicate0D *self, PyObject *args, PyObject *kwds);

/*----------------------BinaryPredicate0D instance definitions ----------------------------*/
static PyMethodDef BPy_BinaryPredicate0D_methods[] = {
	{"getName", ( PyCFunction ) BinaryPredicate0D_getName, METH_NOARGS, "() Returns the string of the name of the binary predicate."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_BinaryPredicate0D type definition ------------------------------*/

PyTypeObject BinaryPredicate0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BinaryPredicate0D",            /* tp_name */
	sizeof(BPy_BinaryPredicate0D),  /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)BinaryPredicate0D___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)BinaryPredicate0D___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)BinaryPredicate0D___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"BinaryPredicate0D objects",    /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_BinaryPredicate0D_methods,  /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)BinaryPredicate0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------
int BinaryPredicate0D_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &BinaryPredicate0D_Type ) < 0 )
		return -1;

	Py_INCREF( &BinaryPredicate0D_Type );
	PyModule_AddObject(module, "BinaryPredicate0D", (PyObject *)&BinaryPredicate0D_Type);
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int BinaryPredicate0D___init__(BPy_BinaryPredicate0D *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->bp0D = new BinaryPredicate0D();
	self->bp0D->py_bp0D = (PyObject *) self;
	
	return 0;
}

void BinaryPredicate0D___dealloc__(BPy_BinaryPredicate0D* self)
{
	if (self->bp0D)
		delete self->bp0D;
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject * BinaryPredicate0D___repr__(BPy_BinaryPredicate0D* self)
{
    return PyUnicode_FromFormat("type: %s - address: %p", self->bp0D->getName().c_str(), self->bp0D );
}


PyObject * BinaryPredicate0D_getName( BPy_BinaryPredicate0D *self, PyObject *args)
{
	return PyUnicode_FromFormat( self->bp0D->getName().c_str() );
}

PyObject * BinaryPredicate0D___call__( BPy_BinaryPredicate0D *self, PyObject *args, PyObject *kwds)
{
	BPy_Interface0D *obj1, *obj2;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if( !PyArg_ParseTuple(args, "O!O!", &Interface0D_Type, &obj1, &Interface0D_Type, &obj2) )
		return NULL;
	
	if( typeid(*(self->bp0D)) == typeid(BinaryPredicate0D) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->bp0D->operator()( *(obj1->if0D) , *(obj2->if0D) ) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->bp0D->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	return PyBool_from_bool( self->bp0D->result );

}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
