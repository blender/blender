#include "Interface0D.h"

#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////


/*-----------------------Python API function prototypes for the Interface0D module--*/
//static PyObject *Freestyle_testOutput( BPy_Freestyle * self );
/*-----------------------Interface0D module doc strings-----------------------------*/
static char M_Interface0D_doc[] = "The Blender.Freestyle.Interface0D submodule";
/*----------------------Interface0D module method def----------------------------*/
struct PyMethodDef M_Interface0D_methods[] = {
//	{"testOutput", ( PyCFunction ) Freestyle_testOutput, METH_NOARGS, "() - Return Curve Data name"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Freestyle method def------------------------------*/

PyTypeObject Interface0D_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Interface0D",				/* tp_name */
	sizeof( BPy_Interface0D ),	/* tp_basicsize */
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
PyObject *Interface0D_Init( void )
{
	PyObject *submodule;
	
	if( PyType_Ready( &Interface0D_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Freestyle.Interface0D", M_Interface0D_methods, M_Interface0D_doc );
	
	return submodule;
}

//------------------------INSTANCE METHODS ----------------------------------

PyObject *Interface0D_getExactTypeName( BPy_Interface0D *self ) {
	return PyString_FromString( self->if0D->getExactTypeName().c_str() );	
}


PyObject *Interface0D_getX( BPy_Interface0D *self ) {
	return PyFloat_FromDouble( self->if0D->getX() );
}


PyObject *Interface0D_getY( BPy_Interface0D *self ) {
	return PyFloat_FromDouble( self->if0D->getY() );
}


PyObject *Interface0D_getZ( BPy_Interface0D *self ) {
	return PyFloat_FromDouble( self->if0D->getZ() );
}


PyObject *Interface0D_getPoint3D( BPy_Interface0D *self ) {
	return Vector_from_Vec3f( self->if0D->getPoint3D() );
}


PyObject *Interface0D_getProjectedX( BPy_Interface0D *self ) {
	return PyFloat_FromDouble( self->if0D->getProjectedX() );
}


PyObject *Interface0D_getProjectedY( BPy_Interface0D *self ) {
	return PyFloat_FromDouble( self->if0D->getProjectedY() );
}


PyObject *Interface0D_getProjectedZ( BPy_Interface0D *self ) {
	return PyFloat_FromDouble( self->if0D->getProjectedZ() );
}


PyObject *Interface0D_getPoint2D( BPy_Interface0D *self ) {
	return Vector_from_Vec2f( self->if0D->getPoint2D() );
}


PyObject *Interface0D_getFEdge( BPy_Interface0D *self ) {
	// FEdge
}


PyObject *Interface0D_getId( BPy_Interface0D *self ) {
	// Id
}


PyObject *Interface0D_getNature( BPy_Interface0D *self ) {
	// VertexNature
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

