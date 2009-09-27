#include "BPy_UnaryPredicate0D.h"

#include "BPy_Convert.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "UnaryPredicate0D/BPy_FalseUP0D.h"
#include "UnaryPredicate0D/BPy_TrueUP0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryPredicate0D instance  -----------*/
static int UnaryPredicate0D___init__(BPy_UnaryPredicate0D *self, PyObject *args, PyObject *kwds);
static void UnaryPredicate0D___dealloc__(BPy_UnaryPredicate0D *self);
static PyObject * UnaryPredicate0D___repr__(BPy_UnaryPredicate0D *self);
static PyObject * UnaryPredicate0D___call__( BPy_UnaryPredicate0D *self, PyObject *args, PyObject *kwds);

static PyObject * UnaryPredicate0D_getName( BPy_UnaryPredicate0D *self );

/*----------------------UnaryPredicate0D instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryPredicate0D_methods[] = {
	{"getName", ( PyCFunction ) UnaryPredicate0D_getName, METH_NOARGS, "Returns the string of the name of the UnaryPredicate0D."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryPredicate0D type definition ------------------------------*/

PyTypeObject UnaryPredicate0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryPredicate0D",             /* tp_name */
	sizeof(BPy_UnaryPredicate0D),   /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryPredicate0D___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryPredicate0D___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryPredicate0D___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"UnaryPredicate0D objects",      /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryPredicate0D_methods,   /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryPredicate0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------
int UnaryPredicate0D_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryPredicate0D_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryPredicate0D_Type );
	PyModule_AddObject(module, "UnaryPredicate0D", (PyObject *)&UnaryPredicate0D_Type);
	
	if( PyType_Ready( &FalseUP0D_Type ) < 0 )
		return -1;
	Py_INCREF( &FalseUP0D_Type );
	PyModule_AddObject(module, "FalseUP0D", (PyObject *)&FalseUP0D_Type);
	
	if( PyType_Ready( &TrueUP0D_Type ) < 0 )
		return -1;
	Py_INCREF( &TrueUP0D_Type );
	PyModule_AddObject(module, "TrueUP0D", (PyObject *)&TrueUP0D_Type);
	
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryPredicate0D___init__(BPy_UnaryPredicate0D *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->up0D = new UnaryPredicate0D();
	self->up0D->py_up0D = (PyObject *) self;
	return 0;
}

void UnaryPredicate0D___dealloc__(BPy_UnaryPredicate0D* self)
{
	if (self->up0D)
		delete self->up0D;
    Py_TYPE(self)->tp_free((PyObject*)self);
}


PyObject * UnaryPredicate0D___repr__(BPy_UnaryPredicate0D* self)
{
    return PyUnicode_FromFormat("type: %s - address: %p", self->up0D->getName().c_str(), self->up0D );
}


PyObject * UnaryPredicate0D_getName( BPy_UnaryPredicate0D *self )
{
	return PyUnicode_FromFormat( self->up0D->getName().c_str() );
}

PyObject * UnaryPredicate0D___call__( BPy_UnaryPredicate0D *self, PyObject *args, PyObject *kwds)
{
	PyObject *py_if0D_it;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if( !PyArg_ParseTuple(args, "O!", &Interface0DIterator_Type, &py_if0D_it) )
		return NULL;

	Interface0DIterator *if0D_it = ((BPy_Interface0DIterator *) py_if0D_it)->if0D_it;

	if( !if0D_it ) {
		string msg(self->up0D->getName() + " has no Interface0DIterator");
		PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		return NULL;
	}
	if( typeid(*(self->up0D)) == typeid(UnaryPredicate0D) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->up0D->operator()(*if0D_it) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->up0D->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	return PyBool_from_bool( self->up0D->result );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif



