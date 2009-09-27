#include "BPy_UnaryFunction0DId.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_Id/BPy_ShapeIdF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction0DId instance  -----------*/
static int UnaryFunction0DId___init__(BPy_UnaryFunction0DId* self, PyObject *args, PyObject *kwds);
static void UnaryFunction0DId___dealloc__(BPy_UnaryFunction0DId* self);
static PyObject * UnaryFunction0DId___repr__(BPy_UnaryFunction0DId* self);

static PyObject * UnaryFunction0DId_getName( BPy_UnaryFunction0DId *self);
static PyObject * UnaryFunction0DId___call__( BPy_UnaryFunction0DId *self, PyObject *args, PyObject *kwds);

/*----------------------UnaryFunction0DId instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction0DId_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction0DId_getName, METH_NOARGS, "() Returns the string of the name of the unary 0D function."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction0DId type definition ------------------------------*/

PyTypeObject UnaryFunction0DId_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction0DId",            /* tp_name */
	sizeof(BPy_UnaryFunction0DId),  /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction0DId___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction0DId___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryFunction0DId___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"UnaryFunction0DId objects",    /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryFunction0DId_methods,  /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0D_Type,          /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryFunction0DId___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DId_Init( PyObject *module ) {

	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction0DId_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction0DId_Type );
	PyModule_AddObject(module, "UnaryFunction0DId", (PyObject *)&UnaryFunction0DId_Type);
	
	if( PyType_Ready( &ShapeIdF0D_Type ) < 0 )
		return -1;
	Py_INCREF( &ShapeIdF0D_Type );
	PyModule_AddObject(module, "ShapeIdF0D", (PyObject *)&ShapeIdF0D_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryFunction0DId___init__(BPy_UnaryFunction0DId* self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->uf0D_id = new UnaryFunction0D<Id>();
	self->uf0D_id->py_uf0D = (PyObject *)self;
	return 0;
}

void UnaryFunction0DId___dealloc__(BPy_UnaryFunction0DId* self)
{
	if (self->uf0D_id)
		delete self->uf0D_id;
	UnaryFunction0D_Type.tp_dealloc((PyObject*)self);
}


PyObject * UnaryFunction0DId___repr__(BPy_UnaryFunction0DId* self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", self->uf0D_id->getName().c_str(), self->uf0D_id );
}

PyObject * UnaryFunction0DId_getName( BPy_UnaryFunction0DId *self )
{
	return PyUnicode_FromFormat( self->uf0D_id->getName().c_str() );
}

PyObject * UnaryFunction0DId___call__( BPy_UnaryFunction0DId *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if(!PyArg_ParseTuple(args, "O!", &Interface0DIterator_Type, &obj))
		return NULL;
	
	if( typeid(*(self->uf0D_id)) == typeid(UnaryFunction0D<Id>) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->uf0D_id->operator()(*( ((BPy_Interface0DIterator *) obj)->if0D_it )) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->uf0D_id->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	return BPy_Id_from_Id( self->uf0D_id->result );
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
