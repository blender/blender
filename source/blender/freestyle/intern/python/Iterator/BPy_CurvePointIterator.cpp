#include "BPy_CurvePointIterator.h"

#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for CurvePointIterator instance  -----------*/
static int CurvePointIterator___init__(BPy_CurvePointIterator *self, PyObject *args);
static PyObject * CurvePointIterator_t( BPy_CurvePointIterator *self );
static PyObject * CurvePointIterator_u( BPy_CurvePointIterator *self );
static PyObject * CurvePointIterator_castToInterface0DIterator( BPy_CurvePointIterator *self );


/*----------------------CurvePointIterator instance definitions ----------------------------*/
static PyMethodDef BPy_CurvePointIterator_methods[] = {
	{"t", ( PyCFunction ) CurvePointIterator_t, METH_NOARGS, "（ ）Returns the curvilinear abscissa."},
	{"u", ( PyCFunction ) CurvePointIterator_u, METH_NOARGS, "（ ）Returns the point parameter in the curve 0<=u<=1."},
	{"castToInterface0DIterator", ( PyCFunction ) CurvePointIterator_castToInterface0DIterator, METH_NOARGS, "() Casts this CurvePointIterator into an Interface0DIterator. Useful for any call to a function of the type UnaryFunction0D."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_CurvePointIterator type definition ------------------------------*/

PyTypeObject CurvePointIterator_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"CurvePointIterator",				/* tp_name */
	sizeof( BPy_CurvePointIterator ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	NULL,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	NULL,					/* tp_repr */

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
	BPy_CurvePointIterator_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Iterator_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)CurvePointIterator___init__,                       	/* initproc tp_init; */
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


//------------------------INSTANCE METHODS ----------------------------------

int CurvePointIterator___init__(BPy_CurvePointIterator *self, PyObject *args )
{	
	PyObject *obj = 0;

	if (! PyArg_ParseTuple(args, "|O", &obj) )
	    return -1;

	if( !obj ){
		self->cp_it = new CurveInternal::CurvePointIterator();
		
	} else if( BPy_CurvePointIterator_Check(obj) ) {
		self->cp_it = new CurveInternal::CurvePointIterator(*( ((BPy_CurvePointIterator *) obj)->cp_it ));
	
	} else if( PyFloat_Check(obj) ) {
		self->cp_it = new CurveInternal::CurvePointIterator( PyFloat_AsDouble(obj) );
			
	} else {
		return -1;
	}

	self->py_it.it = self->cp_it;

	return 0;
}

PyObject * CurvePointIterator_t( BPy_CurvePointIterator *self ) {
	return PyFloat_FromDouble( self->cp_it->t() );
}

PyObject * CurvePointIterator_u( BPy_CurvePointIterator *self ) {
	return PyFloat_FromDouble( self->cp_it->u() );
}

PyObject * CurvePointIterator_castToInterface0DIterator( BPy_CurvePointIterator *self ) {
	PyObject *py_if0D_it =  Interface0DIterator_Type.tp_new( &Interface0DIterator_Type, 0, 0 );
	((BPy_Interface0DIterator *) py_if0D_it)->if0D_it = new Interface0DIterator( self->cp_it->castToInterface0DIterator() );

	return py_if0D_it;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
