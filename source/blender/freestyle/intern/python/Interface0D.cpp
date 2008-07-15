#include "Interface0D.h"

#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Interface0D instance  -----------*/
static PyObject * Interface0D___new__(PyTypeObject *type, PyObject *args, PyObject *kwds);
static void Interface0D___dealloc__(BPy_Interface0D *self);
static PyObject * Interface0D___repr__(BPy_Interface0D *self);

static PyObject *Interface0D_getExactTypeName( BPy_Interface0D *self );
static PyObject *Interface0D_getX( BPy_Interface0D *self );
static PyObject *Interface0D_getY( BPy_Interface0D *self );
static PyObject *Interface0D_getZ( BPy_Interface0D *self );
static PyObject *Interface0D_getPoint3D( BPy_Interface0D *self );
static PyObject *Interface0D_getProjectedX( BPy_Interface0D *self );
static PyObject *Interface0D_getProjectedY( BPy_Interface0D *self );
static PyObject *Interface0D_getProjectedZ( BPy_Interface0D *self );
static PyObject *Interface0D_getPoint2D( BPy_Interface0D *self );
static PyObject *Interface0D_getFEdge( BPy_Interface0D *self );
static PyObject *Interface0D_getId( BPy_Interface0D *self );
static PyObject *Interface0D_getNature( BPy_Interface0D *self ); 

/*----------------------Interface0D instance definitions ----------------------------*/
static PyMethodDef BPy_Interface0D_methods[] = {
	{"getExactTypeName", ( PyCFunction ) Interface0D_getExactTypeName, METH_NOARGS, "（ ）Returns the string of the name of the interface."},
	{"getX", ( PyCFunction ) Interface0D_getX, METH_NOARGS, "（ ）Returns the 3D x coordinate of the point. "},
	{"getY", ( PyCFunction ) Interface0D_getY, METH_NOARGS, "（ ）Returns the 3D y coordinate of the point. "},
	{"getZ", ( PyCFunction ) Interface0D_getZ, METH_NOARGS, "（ ）Returns the 3D z coordinate of the point. "},
	{"getPoint3D", ( PyCFunction ) Interface0D_getPoint3D, METH_NOARGS, "() Returns the 3D point."},
	{"getProjectedX", ( PyCFunction ) Interface0D_getProjectedX, METH_NOARGS, "() Returns the 2D x coordinate of the point."},
	{"getProjectedY", ( PyCFunction ) Interface0D_getProjectedY, METH_NOARGS, "() Returns the 2D y coordinate of the point."},
	{"getProjectedZ", ( PyCFunction ) Interface0D_getProjectedZ, METH_NOARGS, "() Returns the 2D z coordinate of the point."},
	{"getPoint2D", ( PyCFunction ) Interface0D_getPoint2D, METH_NOARGS, "() Returns the 2D point."},
	{"getFEdge", ( PyCFunction ) Interface0D_getFEdge, METH_NOARGS, "() Returns the FEdge that lies between this Interface0D and the Interface0D given as argument."},
	{"getId", ( PyCFunction ) Interface0D_getId, METH_NOARGS, "() Returns the Id of the point."},
	{"getNature", ( PyCFunction ) Interface0D_getNature, METH_NOARGS, "() Returns the nature of the point."},	
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Interface0D type definition ------------------------------*/

PyTypeObject Interface0D_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Interface0D",				/* tp_name */
	sizeof( BPy_Interface0D ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)Interface0D___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)Interface0D___repr__,					/* tp_repr */

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
	BPy_Interface0D_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	NULL,                       	/* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	(newfunc)Interface0D___new__,		/* newfunc tp_new; */
	
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
PyMODINIT_FUNC Interface0D_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &Interface0D_Type ) < 0 )
		return;

	Py_INCREF( &Interface0D_Type );
	PyModule_AddObject(module, "Interface0D", (PyObject *)&Interface0D_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

PyObject * Interface0D___new__(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    BPy_Interface0D *self;

    self = (BPy_Interface0D *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->if0D = new Interface0D();
    }

    return (PyObject *)self;
}

void Interface0D___dealloc__(BPy_Interface0D* self)
{
	delete self->if0D;
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * Interface0D___repr__(BPy_Interface0D* self)
{
    return PyString_FromFormat("type: %s - address: %p", self->if0D->getExactTypeName().c_str(), self->if0D );
}

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
	return BPy_Id_from_Id( self->if0D->getId() );
}


PyObject *Interface0D_getNature( BPy_Interface0D *self ) {
	// VertexNature
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

