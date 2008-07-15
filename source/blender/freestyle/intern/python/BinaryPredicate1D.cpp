#include "BinaryPredicate1D.h"

#include "Convert.h"
#include "Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for BinaryPredicate1D instance  -----------*/
static PyObject * BinaryPredicate1D___new__(PyTypeObject *type, PyObject *args, PyObject *kwds);
static void BinaryPredicate1D___dealloc__(BPy_BinaryPredicate1D *self);
static PyObject * BinaryPredicate1D___repr__(BPy_BinaryPredicate1D *self);

static PyObject * BinaryPredicate1D_getName( BPy_BinaryPredicate1D *self, PyObject *args);
static PyObject * BinaryPredicate1D___call__( BPy_BinaryPredicate1D *self, PyObject *args);

/*----------------------BinaryPredicate1D instance definitions ----------------------------*/
static PyMethodDef BPy_BinaryPredicate1D_methods[] = {
	{"getName", ( PyCFunction ) BinaryPredicate1D_getName, METH_NOARGS, "（ ）Returns the string of the name of the binary predicate."},
	{"__call__", ( PyCFunction ) BinaryPredicate1D___call__, METH_VARARGS, "BinaryPredicate1D（Interface1D, Interface1D ）. Must be overloaded by inherited classes. It evaluates a relation between two Interface1D." },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_BinaryPredicate1D type definition ------------------------------*/
PyTypeObject BinaryPredicate1D_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,									/* ob_size */
	"BinaryPredicate1D",				/* tp_name */
	sizeof( BPy_BinaryPredicate1D ),	/* tp_basicsize */
	0,									/* tp_itemsize */
	
	/* methods */
	(destructor)BinaryPredicate1D___dealloc__,		/* tp_dealloc */
	NULL,                       					/* printfunc tp_print; */
	NULL,                       					/* getattrfunc tp_getattr; */
	NULL,                       					/* setattrfunc tp_setattr; */
	NULL,											/* tp_compare */
	(reprfunc)BinaryPredicate1D___repr__,						/* tp_repr */

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
	Py_TPFLAGS_DEFAULT, 		/* long tp_flags; */

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
	BPy_BinaryPredicate1D_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,							/*  struct PyMemberDef *tp_members; */
	NULL,							/*  struct PyGetSetDef *tp_getset; */
	NULL,							/*  struct _typeobject *tp_base; */
	NULL,                       	/* PyObject *tp_dict; */
	NULL,                       	/* descrgetfunc tp_descr_get; */
	NULL,                       	/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	NULL,                       	/* initproc tp_init; */
	NULL,                       	/* allocfunc tp_alloc; */
	BinaryPredicate1D___new__,		/* newfunc tp_new; */
	
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
PyMODINIT_FUNC BinaryPredicate1D_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &BinaryPredicate1D_Type ) < 0 )
		return;

	Py_INCREF( &BinaryPredicate1D_Type );
	PyModule_AddObject(module, "BinaryPredicate1D", (PyObject *)&BinaryPredicate1D_Type);
}


//------------------------INSTANCE METHODS ----------------------------------

PyObject * BinaryPredicate1D___new__(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    BPy_BinaryPredicate1D *self;

    self = (BPy_BinaryPredicate1D *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->bp1D = new BinaryPredicate1D();
    }

    return (PyObject *)self;
}

void BinaryPredicate1D___dealloc__(BPy_BinaryPredicate1D* self)
{
	delete self->bp1D;
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * BinaryPredicate1D___repr__(BPy_BinaryPredicate1D* self)
{
    return PyString_FromFormat("type: %s - address: %p", self->bp1D->getName().c_str(), self->bp1D );
}

PyObject *BinaryPredicate1D_getName( BPy_BinaryPredicate1D *self, PyObject *args)
{
	return PyString_FromString( self->bp1D->getName().c_str() );
}

PyObject *BinaryPredicate1D___call__( BPy_BinaryPredicate1D *self, PyObject *args)
{
	BPy_Interface1D *obj1, *obj2;
	bool b;
	
	if( !PyArg_ParseTuple(args,(char *)"OO:BinaryPredicate1D___call__", &obj1, &obj2) ) {
		cout << "ERROR: BinaryPredicate1D___call__ " << endl;		
		return NULL;
	}
	
	b = self->bp1D->operator()( *(obj1->if1D) , *(obj2->if1D) );
	return PyBool_from_bool( b );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
