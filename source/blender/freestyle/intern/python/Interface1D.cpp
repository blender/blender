#include "Interface1D.h"

#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////


/*-----------------------Python API function prototypes for the Interface1D module--*/
//static PyObject *Freestyle_testOutput( BPy_Freestyle * self );
/*-----------------------Interface1D module doc strings-----------------------------*/
static char M_Interface1D_doc[] = "The Blender.Freestyle.Interface1D submodule";
/*----------------------Interface1D module method def----------------------------*/
struct PyMethodDef M_Interface1D_methods[] = {
//	{"testOutput", ( PyCFunction ) Freestyle_testOutput, METH_NOARGS, "() - Return Curve Data name"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Freestyle method def------------------------------*/

PyTypeObject Interface1D_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Interface1D",				/* tp_name */
	sizeof( BPy_Interface1D ),	/* tp_basicsize */
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
PyObject *Interface1D_Init( void )
{
	PyObject *submodule;
	
	if( PyType_Ready( &Interface1D_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Freestyle.Interface1D", M_Interface1D_methods, M_Interface1D_doc );
	
	return submodule;
}

//------------------------INSTANCE METHODS ----------------------------------


PyObject *Interface1D_getExactTypeName( BPy_Interface1D *self ) {
	return PyString_FromString( self->if1D->getExactTypeName().c_str() );	
}

PyObject *Interface1D_getVertices( BPy_Interface1D *self ) {
	// Vector
}

PyObject *Interface1D_getPoints( BPy_Interface1D *self ) {
	// Vector
}

PyObject *Interface1D_getLength2D( BPy_Interface1D *self ) {
	return PyFloat_FromDouble( (double) self->if1D->getLength2D() );
}

PyObject *Interface1D_getId( BPy_Interface1D *self ) {
	// Id
}

PyObject *Interface1D_getNature( BPy_Interface1D *self ) {
	// EdgeNature
}

PyObject *Interface1D_getTimeStamp( BPy_Interface1D *self ) {
	return PyInt_FromLong( self->if1D->getTimeStamp() );
}

PyObject *Interface1D_setTimeStamp( BPy_Interface1D *self, PyObject *args) {
	PyObject * obj1 = 0 ;
	PyObject * obj2 = 0 ;
	unsigned int timestamp;
	
	if( !PyArg_ParseTuple(args, (char *)"OO:Interface1D_setTimeStamp", &obj1, &obj2) )
		cout << "ERROR: Interface1D_setTimeStamp" << endl;
	
	timestamp = static_cast<unsigned int>( PyInt_AsLong(obj2) );
	self->if1D->setTimeStamp( timestamp );

	Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


