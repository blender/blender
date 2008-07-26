#include "BPy_ChainPredicateIterator.h"

#include "../BPy_Convert.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ChainPredicateIterator instance  -----------*/
static int ChainPredicateIterator___init__(BPy_ChainPredicateIterator *self, PyObject *args);

static PyObject * ChainPredicateIterator_traverse( BPy_ViewEdgeIterator *self, PyObject *args );


/*----------------------ChainPredicateIterator instance definitions ----------------------------*/
static PyMethodDef BPy_ChainPredicateIterator_methods[] = {

	{"traverse", ( PyCFunction ) ChainPredicateIterator_traverse, METH_VARARGS, "(AdjacencyIterator ai) This method iterates over the potential next ViewEdges and returns the one that will be followed next. Returns the next ViewEdge to follow or 0 when the end of the chain is reached. "},

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

// ChainPredicateIterator (bool iRestrictToSelection=true, bool iRestrictToUnvisited=true, ViewEdge *begin=NULL, bool orientation=true)
//  	ChainPredicateIterator (UnaryPredicate1D &upred, BinaryPredicate1D &bpred, bool iRestrictToSelection=true, bool iRestrictToUnvisited=true, ViewEdge *begin=NULL, bool orientation=true)
//  	ChainPredicateIterator (const ChainPredicateIterator &brother)

int ChainPredicateIterator___init__(BPy_ChainPredicateIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0;

	if (!( PyArg_ParseTuple(args, "O|OOOOO", &obj1, &obj2, &obj3, &obj4) ))
	    return -1;

	if( obj1 && BPy_ChainPredicateIterator_Check(obj1)  ) {
		self->c_it = new ChainPredicateIterator(*( ((BPy_ChainPredicateIterator *) obj1)->c_it ));
	
	} else 	if( obj1 && BPy_ChainPredicateIterator_Check(obj1)  ) {
			self->c_it = new ChainPredicateIterator(*( ((BPy_ChainPredicateIterator *) obj1)->c_it ));
	
	} else {
		bool restrictToSelection = ( obj1 && PyBool_Check(obj1) ) ? bool_from_PyBool(obj1) : true;
		bool restrictToUnvisited = ( obj2 && PyBool_Check(obj2) ) ? bool_from_PyBool(obj2) : true;
		ViewEdge *begin = ( obj3 && BPy_ViewEdge_Check(obj3) ) ? ((BPy_ViewEdge *) obj3)->ve : 0;
		bool orientation = ( obj4 && PyBool_Check(obj4) ) ? bool_from_PyBool(obj4) : true;
		
		self->c_it = new ChainPredicateIterator( restrictToSelection, restrictToUnvisited, begin, orientation);	
	}
	
	self->py_ve_it.ve_it = self->c_it;
	self->py_ve_it.py_it.it = self->c_it;
	
	return 0;
}

//virtual ViewEdge * 	traverse (const AdjacencyIterator &it)
PyObject *ChainPredicateIterator_traverse( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_a_it;

	if(!( PyArg_ParseTuple(args, "O", &py_a_it) && BPy_AdjacencyIterator_Check(py_a_it) )) {
		cout << "ERROR: ChainPredicateIterator_traverse" << endl;
		Py_RETURN_NONE;
	}
	
	if( ((BPy_AdjacencyIterator *) py_a_it)->a_it )
		self->ve_it->traverse(*( ((BPy_AdjacencyIterator *) py_a_it)->a_it ));
		
	Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
