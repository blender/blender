#include "BinaryPredicate0D.h"

#include "Convert.h"
#include "Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////


/*-----------------------Python API function prototypes for the BinaryPredicate0D module--*/
//static PyObject *Freestyle_testOutput( BPy_Freestyle * self );
/*-----------------------BinaryPredicate0D module doc strings-----------------------------*/
static char M_BinaryPredicate0D_doc[] = "The Blender.Freestyle.BinaryPredicate0D submodule";
/*----------------------BinaryPredicate0D module method def----------------------------*/
struct PyMethodDef M_BinaryPredicate0D_methods[] = {
//	{"testOutput", ( PyCFunction ) Freestyle_testOutput, METH_NOARGS, "() - Return Curve Data name"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Freestyle method def------------------------------*/

PyTypeObject BinaryPredicate0D_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"BinaryPredicate0D",				/* tp_name */
	sizeof( BPy_BinaryPredicate0D ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	NULL,						/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */
	NULL,						/* tp_repr */

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
	NULL,						/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,         				/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	
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
PyObject *BinaryPredicate0D_Init( void )
{
	PyObject *submodule;
	
	if( PyType_Ready( &BinaryPredicate0D_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Freestyle.BinaryPredicate0D", M_BinaryPredicate0D_methods, M_BinaryPredicate0D_doc );
	
	return submodule;
}

//------------------------INSTANCE METHODS ----------------------------------

PyObject *BinaryPredicate0D_getName( BPy_BinaryPredicate0D *self, PyObject *args)
{
	return PyString_FromString( self->bp0D->getName().c_str() );
}

PyObject *BinaryPredicate0D___call__( BPy_BinaryPredicate0D *self, PyObject *args)
{
	BPy_BinaryPredicate0D *obj1;
	BPy_Interface0D *obj2, *obj3;
	bool b;

	if (!PyArg_ParseTuple(args,(char *)"OOO:BinaryPredicate0D___call__", &obj1, obj2, &obj3))
		cout << "ERROR: BinaryPredicate0D___call__ " << endl;		
	
	b = self->bp0D->operator()( *(obj2->if0D) , *(obj3->if0D) );
	return PyBool_from_bool( b );

}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
