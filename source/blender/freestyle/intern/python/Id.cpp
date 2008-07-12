#include "Id.h"

#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////


/*-----------------------Python API function prototypes for the Id module--*/
//static PyObject *Freestyle_testOutput( BPy_Freestyle * self );
/*-----------------------Id module doc strings-----------------------------*/
static char M_Id_doc[] = "The Blender.Freestyle.Id submodule";
/*----------------------Id module method def----------------------------*/
struct PyMethodDef M_Id_methods[] = {
//	{"testOutput", ( PyCFunction ) Freestyle_testOutput, METH_NOARGS, "() - Return Curve Data name"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Freestyle method def------------------------------*/

PyTypeObject Id_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Id",				/* tp_name */
	sizeof( BPy_Id ),	/* tp_basicsize */
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
PyObject *Id_Init( void )
{
	PyObject *submodule;
	
	if( PyType_Ready( &Id_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Freestyle.Id", M_Id_methods, M_Id_doc );
	
	return submodule;
}

//------------------------INSTANCE METHODS ----------------------------------

PyObject *Id_getFirst( BPy_Id *self ) {
	return PyInt_FromLong( self->id->getFirst() );
}


PyObject *Id_getSecond( BPy_Id *self) {
	return PyInt_FromLong( self->id->getSecond() );
}


PyObject *Id_setFirst( BPy_Id *self , PyObject *args) {
	PyObject * obj1 = 0 ;
	PyObject * obj2 = 0 ;
	unsigned int i;

	if( !PyArg_ParseTuple(args, (char *)"OO:Id_setFirst", &obj1, &obj2) )
		cout << "ERROR: Id_setFirst" << endl;

	i = static_cast<unsigned int>( PyInt_AsLong(obj2) );
	self->id->setFirst( i );

	Py_RETURN_NONE;
}


PyObject *Id_setSecond( BPy_Id *self , PyObject *args) {
	PyObject * obj1 = 0 ;
	PyObject * obj2 = 0 ;
	unsigned int i;

	if( !PyArg_ParseTuple(args, (char *)"OO:Id_setSecond", &obj1, &obj2) )
		cout << "ERROR: Id_setSecond" << endl;

	i = static_cast<unsigned int>( PyInt_AsLong(obj2) );
	self->id->setSecond( i );

	Py_RETURN_NONE;
}

PyObject *Id___eq__( BPy_Id *self , PyObject *args) {
	BPy_Id * obj1 = 0 ;
	BPy_Id * obj2 = 0 ;

	if( !PyArg_ParseTuple(args, (char *)"OO:Id___eq__", &obj1, &obj2) )
		cout << "ERROR: Id___eq__" << endl;
	
	return PyBool_from_bool( obj1->id == obj2->id );
}


PyObject *Id___ne__(PyObject *self , PyObject *args) {
	BPy_Id * obj1 = 0 ;
	BPy_Id * obj2 = 0 ;

	if( !PyArg_ParseTuple(args, (char *)"OO:Id___ne__", &obj1, &obj2) )
		cout << "ERROR: Id___ne__" << endl;

	return PyBool_from_bool( obj1->id != obj2->id );
}

PyObject *Id___lt__(PyObject *self , PyObject *args) {
	BPy_Id * obj1 = 0 ;
	BPy_Id * obj2 = 0 ;

	if( !PyArg_ParseTuple(args, (char *)"OO:Id___lt__", &obj1, &obj2) )
		cout << "ERROR: Id___lt__" << endl;

	return PyBool_from_bool( obj1->id < obj2->id );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif