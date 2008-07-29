#include "BPy_SVertexIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_SVertex.h"
#include "../Interface1D/BPy_FEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for SVertexIterator instance  -----------*/
static int SVertexIterator___init__(BPy_SVertexIterator *self, PyObject *args);

static PyObject * SVertexIterator_t( BPy_SVertexIterator *self );
static PyObject * SVertexIterator_u( BPy_SVertexIterator *self );

static PyObject * SVertexIterator_getObject( BPy_SVertexIterator *self);

/*----------------------SVertexIterator instance definitions ----------------------------*/
static PyMethodDef BPy_SVertexIterator_methods[] = {
	{"t", ( PyCFunction ) SVertexIterator_t, METH_NOARGS, "（ ）Returns the curvilinear abscissa."},
	{"u", ( PyCFunction ) SVertexIterator_u, METH_NOARGS, "（ ）Returns the point parameter in the curve 0<=u<=1."},
	{"getObject", ( PyCFunction ) SVertexIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_SVertexIterator type definition ------------------------------*/

PyTypeObject SVertexIterator_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"SVertexIterator",				/* tp_name */
	sizeof( BPy_SVertexIterator ),	/* tp_basicsize */
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
	BPy_SVertexIterator_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Iterator_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)SVertexIterator___init__,                       	/* initproc tp_init; */
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

int SVertexIterator___init__(BPy_SVertexIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0;
	float f;

	if (! PyArg_ParseTuple(args, "|OOOOf", &obj1, &obj2, &obj3, &obj4, f) )
	    return -1;

	if( !obj1 ){
		self->sv_it = new ViewEdgeInternal::SVertexIterator();
		
	} else if( BPy_SVertexIterator_Check(obj1) ) {
		self->sv_it = new ViewEdgeInternal::SVertexIterator(*( ((BPy_SVertexIterator *) obj1)->sv_it ));
	
	} else if(  obj1 && BPy_SVertex_Check(obj1) &&
	 			obj2 && BPy_SVertex_Check(obj2) &&
				obj3 && BPy_FEdge_Check(obj3) &&
				obj4 && BPy_FEdge_Check(obj4) ) {

		self->sv_it = new ViewEdgeInternal::SVertexIterator(
							((BPy_SVertex *) obj1)->sv,
							((BPy_SVertex *) obj2)->sv,
							((BPy_FEdge *) obj3)->fe,
							((BPy_FEdge *) obj4)->fe,
							f );
			
	} else {
		return -1;
	}

	self->py_it.it = self->sv_it;

	return 0;
}

PyObject * SVertexIterator_t( BPy_SVertexIterator *self ) {
	return PyFloat_FromDouble( self->sv_it->t() );
}

PyObject * SVertexIterator_u( BPy_SVertexIterator *self ) {
	return PyFloat_FromDouble( self->sv_it->u() );
}

PyObject * SVertexIterator_getObject( BPy_SVertexIterator *self) {
	return BPy_SVertex_from_SVertex( self->sv_it->operator*() );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
