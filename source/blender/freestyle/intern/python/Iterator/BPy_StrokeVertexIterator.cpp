#include "BPy_StrokeVertexIterator.h"

#include "BPy_Interface0DIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for StrokeVertexIterator instance  -----------*/
static int StrokeVertexIterator___init__(BPy_StrokeVertexIterator *self, PyObject *args);
static PyObject * StrokeVertexIterator_t( BPy_StrokeVertexIterator *self );
static PyObject * StrokeVertexIterator_u( BPy_StrokeVertexIterator *self );
static PyObject * StrokeVertexIterator_castToInterface0DIterator( BPy_StrokeVertexIterator *self );

/*----------------------StrokeVertexIterator instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeVertexIterator_methods[] = {
	{"t", ( PyCFunction ) StrokeVertexIterator_t, METH_NOARGS, "（ ）Returns the curvilinear abscissa."},
	{"u", ( PyCFunction ) StrokeVertexIterator_u, METH_NOARGS, "（ ）Returns the point parameter in the curve 0<=u<=1."},
	{"castToInterface0DIterator", ( PyCFunction ) StrokeVertexIterator_castToInterface0DIterator, METH_NOARGS, "() Casts this StrokeVertexIterator into an Interface0DIterator. Useful for any call to a function of the type UnaryFunction0D."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_StrokeVertexIterator type definition ------------------------------*/

PyTypeObject StrokeVertexIterator_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"StrokeVertexIterator",				/* tp_name */
	sizeof( BPy_StrokeVertexIterator ),	/* tp_basicsize */
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
	BPy_StrokeVertexIterator_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Iterator_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)StrokeVertexIterator___init__,                       	/* initproc tp_init; */
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

int StrokeVertexIterator___init__(BPy_StrokeVertexIterator *self, PyObject *args )
{	
	PyObject *obj = 0;

	if (! PyArg_ParseTuple(args, "|O", &obj) )
	    return -1;

	if( !obj ){
		self->sv_it = new StrokeVertexIterator();
		
	} else if( BPy_StrokeVertexIterator_Check(obj) ) {
		self->sv_it = new StrokeVertexIterator(*( ((BPy_StrokeVertexIterator *) obj)->sv_it ));
	
	} else {
		return -1;
	}

	self->py_it.it = self->sv_it;

	return 0;
}

PyObject * StrokeVertexIterator_t( BPy_StrokeVertexIterator *self ) {
	return PyFloat_FromDouble( self->sv_it->t() );
}

PyObject * StrokeVertexIterator_u( BPy_StrokeVertexIterator *self ) {
	return PyFloat_FromDouble( self->sv_it->u() );
}

PyObject * StrokeVertexIterator_castToInterface0DIterator( BPy_StrokeVertexIterator *self ) {
	PyObject *py_if0D_it =  Interface0DIterator_Type.tp_new( &Interface0DIterator_Type, 0, 0 );
	((BPy_Interface0DIterator *) py_if0D_it)->if0D_it = new Interface0DIterator( self->sv_it->castToInterface0DIterator() );

	return py_if0D_it;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
