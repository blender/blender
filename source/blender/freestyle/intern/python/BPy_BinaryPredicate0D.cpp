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
static PyObject * BinaryPredicate0D___call__( BPy_BinaryPredicate0D *self, PyObject *args);

/*----------------------BinaryPredicate0D instance definitions ----------------------------*/
static PyMethodDef BPy_BinaryPredicate0D_methods[] = {
	{"getName", ( PyCFunction ) BinaryPredicate0D_getName, METH_NOARGS, "（ ）Returns the string of the name of the binary predicate."},
	{"__call__", ( PyCFunction ) BinaryPredicate0D___call__, METH_VARARGS, "BinaryPredicate0D（Interface0D, Interface0D ）. Must be overloaded by inherited classes. It evaluates a relation between two Interface0D." },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_BinaryPredicate0D type definition ------------------------------*/

PyTypeObject BinaryPredicate0D_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"BinaryPredicate0D",				/* tp_name */
	sizeof( BPy_BinaryPredicate0D ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)BinaryPredicate0D___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)BinaryPredicate0D___repr__,					/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, 		/* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_BinaryPredicate0D_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)BinaryPredicate0D___init__, /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,		/* newfunc tp_new; */
	
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

//-------------------MODULE INITIALIZATION--------------------------------
PyMODINIT_FUNC BinaryPredicate0D_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &BinaryPredicate0D_Type ) < 0 )
		return;

	Py_INCREF( &BinaryPredicate0D_Type );
	PyModule_AddObject(module, "BinaryPredicate0D", (PyObject *)&BinaryPredicate0D_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

int BinaryPredicate0D___init__(BPy_BinaryPredicate0D *self, PyObject *args, PyObject *kwds)
{
	self->bp0D = new BinaryPredicate0D();
	return 0;
}

void BinaryPredicate0D___dealloc__(BPy_BinaryPredicate0D* self)
{
	delete self->bp0D;
    self->ob_type->tp_free((PyObject*)self);
}


PyObject * BinaryPredicate0D___repr__(BPy_BinaryPredicate0D* self)
{
    return PyString_FromFormat("type: %s - address: %p", self->bp0D->getName().c_str(), self->bp0D );
}


PyObject * BinaryPredicate0D_getName( BPy_BinaryPredicate0D *self, PyObject *args)
{
	return PyString_FromString( self->bp0D->getName().c_str() );
}

PyObject * BinaryPredicate0D___call__( BPy_BinaryPredicate0D *self, PyObject *args)
{
	BPy_Interface0D *obj1, *obj2;
	bool b;

	if( !PyArg_ParseTuple(args,(char *)"OO", &obj1, &obj2) ) {
		cout << "ERROR: BinaryPredicate0D___call__ " << endl;		
		return NULL;
	}
	
	b = self->bp0D->operator()( *(obj1->if0D) , *(obj2->if0D) );
	return PyBool_from_bool( b );

}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
