#include "BPy_AdjacencyIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_ViewVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for AdjacencyIterator instance  -----------*/
static int AdjacencyIterator___init__(BPy_AdjacencyIterator *self, PyObject *args);

/*----------------------AdjacencyIterator instance definitions ----------------------------*/
static PyMethodDef BPy_AdjacencyIterator_methods[] = {
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_AdjacencyIterator type definition ------------------------------*/

PyTypeObject AdjacencyIterator_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"AdjacencyIterator",				/* tp_name */
	sizeof( BPy_AdjacencyIterator ),	/* tp_basicsize */
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
	BPy_AdjacencyIterator_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Iterator_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)AdjacencyIterator___init__,                       	/* initproc tp_init; */
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

int AdjacencyIterator___init__(BPy_AdjacencyIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0;

	if (! PyArg_ParseTuple(args, "|OOO", &obj1, &obj2, &obj3) )
	    return -1;

	if( !obj1 && !obj2 && !obj3 ){
		self->ai = new AdjacencyIterator();
		
	} else if( BPy_AdjacencyIterator_Check(obj1) ) {
		self->ai = new AdjacencyIterator(*( ((BPy_AdjacencyIterator *) obj1)->ai ));
	
	} else if( BPy_ViewVertex_Check(obj1) ) {
		bool restrictToSelection = ( obj2 && PyBool_Check(obj2) ) ? bool_from_PyBool(obj2) : true;
		bool restrictToUnvisited = ( obj3 && PyBool_Check(obj3) ) ? bool_from_PyBool(obj3) : true;
		
		self->ai = new AdjacencyIterator( ((BPy_ViewVertex *) obj1)->vv, restrictToSelection, restrictToUnvisited );
			
	} else {
		return -1;
	}

	self->py_it.it = self->ai;
	return 0;

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
