#include "BPy_ChainPredicateIterator.h"

#include "../BPy_Convert.h"
#include "../BPy_BinaryPredicate1D.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ChainPredicateIterator instance  -----------*/
static int ChainPredicateIterator___init__(BPy_ChainPredicateIterator *self, PyObject *args);

/*----------------------ChainPredicateIterator instance definitions ----------------------------*/
static PyMethodDef BPy_ChainPredicateIterator_methods[] = {

	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ChainPredicateIterator type definition ------------------------------*/

PyTypeObject ChainPredicateIterator_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"ChainPredicateIterator",				/* tp_name */
	sizeof( BPy_ChainPredicateIterator ),	/* tp_basicsize */
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
	BPy_ChainPredicateIterator_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&ChainingIterator_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)ChainPredicateIterator___init__,                       	/* initproc tp_init; */
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

//------------------------INSTANCE METHODS ----------------------------------

int ChainPredicateIterator___init__(BPy_ChainPredicateIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0, *obj5 = 0, *obj6 = 0;

	if (!( PyArg_ParseTuple(args, "O|OOOOO", &obj1, &obj2, &obj3, &obj4, &obj5, &obj6) ))
	    return -1;

	if( obj1 && BPy_ChainPredicateIterator_Check(obj1)  ) { 
		self->cp_it = new ChainPredicateIterator(*( ((BPy_ChainPredicateIterator *) obj1)->cp_it ));
	
	} else if( 	obj1 && BPy_UnaryPredicate1D_Check(obj1) && ((BPy_UnaryPredicate1D *) obj1)->up1D &&
	  			obj2 && BPy_BinaryPredicate1D_Check(obj2) && ((BPy_BinaryPredicate1D *) obj2)->bp1D	) {
				
		UnaryPredicate1D *up1D = ((BPy_UnaryPredicate1D *) obj1)->up1D;
		BinaryPredicate1D *bp1D = ((BPy_BinaryPredicate1D *) obj2)->bp1D;
		bool restrictToSelection = ( obj3 && PyBool_Check(obj3) ) ? bool_from_PyBool(obj3) : true;
		bool restrictToUnvisited = ( obj4 && PyBool_Check(obj4) ) ? bool_from_PyBool(obj4) : true;
		ViewEdge *begin = ( obj5 && BPy_ViewEdge_Check(obj5) ) ? ((BPy_ViewEdge *) obj5)->ve : 0;
		bool orientation = ( obj6 && PyBool_Check(obj6) ) ? bool_from_PyBool(obj6) : true;
	
		self->cp_it = new ChainPredicateIterator( *up1D, *bp1D, restrictToSelection, restrictToUnvisited, begin, orientation);
	
	} else {
		bool restrictToSelection = ( obj1 && PyBool_Check(obj1) ) ? bool_from_PyBool(obj1) : true;
		bool restrictToUnvisited = ( obj2 && PyBool_Check(obj2) ) ? bool_from_PyBool(obj2) : true;
		ViewEdge *begin = ( obj3 && BPy_ViewEdge_Check(obj3) ) ? ((BPy_ViewEdge *) obj3)->ve : 0;
		bool orientation = ( obj4 && PyBool_Check(obj4) ) ? bool_from_PyBool(obj4) : true;
		
		self->cp_it = new ChainPredicateIterator( restrictToSelection, restrictToUnvisited, begin, orientation);	
	}
	
	self->py_c_it.c_it = self->cp_it;
	self->py_c_it.py_ve_it.ve_it = self->cp_it;
	self->py_c_it.py_ve_it.py_it.it = self->cp_it;
	
	return 0;
	
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
