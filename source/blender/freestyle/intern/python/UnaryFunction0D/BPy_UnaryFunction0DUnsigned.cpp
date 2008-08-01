#include "BPy_UnaryFunction0DUnsigned.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_unsigned_int/BPy_QuantitativeInvisibilityF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction0DUnsigned instance  -----------*/
static int UnaryFunction0DUnsigned___init__(BPy_UnaryFunction0DUnsigned* self);
static void UnaryFunction0DUnsigned___dealloc__(BPy_UnaryFunction0DUnsigned* self);
static PyObject * UnaryFunction0DUnsigned___repr__(BPy_UnaryFunction0DUnsigned* self);

static PyObject * UnaryFunction0DUnsigned_getName( BPy_UnaryFunction0DUnsigned *self);
static PyObject * UnaryFunction0DUnsigned___call__( BPy_UnaryFunction0DUnsigned *self, PyObject *args);

/*----------------------UnaryFunction0DUnsigned instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction0DUnsigned_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction0DUnsigned_getName, METH_NOARGS, "（ ）Returns the string of the name of the unary 0D function."},
	{"__call__", ( PyCFunction ) UnaryFunction0DUnsigned___call__, METH_VARARGS, "（Interface0DIterator it ）Executes the operator ()	on the iterator it pointing onto the point at which we wish to evaluate the function." },
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction0DUnsigned type definition ------------------------------*/

PyTypeObject UnaryFunction0DUnsigned_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"UnaryFunction0DUnsigned",				/* tp_name */
	sizeof( BPy_UnaryFunction0DUnsigned ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)UnaryFunction0DUnsigned___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)UnaryFunction0DUnsigned___repr__,					/* tp_repr */

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
	BPy_UnaryFunction0DUnsigned_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&UnaryFunction0D_Type,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)UnaryFunction0DUnsigned___init__, /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	NULL,		/* newfunc tp_new; */
	
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

PyMODINIT_FUNC UnaryFunction0DUnsigned_Init( PyObject *module ) {

	if( module == NULL )
		return;

	if( PyType_Ready( &UnaryFunction0DUnsigned_Type ) < 0 )
		return;
	Py_INCREF( &UnaryFunction0DUnsigned_Type );
	PyModule_AddObject(module, "UnaryFunction0DUnsigned", (PyObject *)&UnaryFunction0DUnsigned_Type);
	
	if( PyType_Ready( &QuantitativeInvisibilityF0D_Type ) < 0 )
		return;
	Py_INCREF( &QuantitativeInvisibilityF0D_Type );
	PyModule_AddObject(module, "QuantitativeInvisibilityF0D", (PyObject *)&QuantitativeInvisibilityF0D_Type);
			
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryFunction0DUnsigned___init__(BPy_UnaryFunction0DUnsigned* self)
{
	self->uf0D_unsigned = new UnaryFunction0D<unsigned int>();
	self->uf0D_unsigned->py_uf0D = (PyObject *)self;
	return 0;
}

void UnaryFunction0DUnsigned___dealloc__(BPy_UnaryFunction0DUnsigned* self)
{
	delete self->uf0D_unsigned;
	UnaryFunction0D_Type.tp_dealloc((PyObject*)self);
}


PyObject * UnaryFunction0DUnsigned___repr__(BPy_UnaryFunction0DUnsigned* self)
{
	return PyString_FromFormat("type: %s - address: %p", self->uf0D_unsigned->getName().c_str(), self->uf0D_unsigned );
}

PyObject * UnaryFunction0DUnsigned_getName( BPy_UnaryFunction0DUnsigned *self )
{
	return PyString_FromString( self->uf0D_unsigned->getName().c_str() );
}

PyObject * UnaryFunction0DUnsigned___call__( BPy_UnaryFunction0DUnsigned *self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O", &obj) && BPy_Interface0DIterator_Check(obj) ) {
		cout << "ERROR: UnaryFunction0DUnsigned___call__ " << endl;		
		return NULL;
	}
	
	unsigned int i = self->uf0D_unsigned->operator()(*( ((BPy_Interface0DIterator *) obj)->if0D_it ));
	return PyInt_FromLong( i );

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
