#include "BPy_ChainingIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_ViewVertex.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "BPy_AdjacencyIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ChainingIterator instance  -----------*/
static int ChainingIterator___init__(BPy_ChainingIterator *self, PyObject *args);
static PyObject * ChainingIterator_init( BPy_ChainingIterator *self );
static PyObject * ChainingIterator_traverse( BPy_ChainingIterator *self, PyObject *args );
static PyObject * ChainingIterator_getVertex( BPy_ChainingIterator *self );
static PyObject * ChainingIterator_isIncrementing( BPy_ChainingIterator *self );

static PyObject * ChainingIterator_getObject( BPy_ChainingIterator *self);

/*----------------------ChainingIterator instance definitions ----------------------------*/
static PyMethodDef BPy_ChainingIterator_methods[] = {
	{"init", ( PyCFunction ) ChainingIterator_init, METH_NOARGS, "() Inits the iterator context. This method is called each time a new chain is started. It can be used to reset some history information that you might want to keep."},
	{"traverse", ( PyCFunction ) ChainingIterator_traverse, METH_VARARGS, "(AdjacencyIterator ai) This method iterates over the potential next ViewEdges and returns the one that will be followed next. Returns the next ViewEdge to follow or 0 when the end of the chain is reached. "},
	{"getVertex", ( PyCFunction ) ChainingIterator_getVertex, METH_NOARGS, "() Returns the vertex which is the next crossing "},
	{"isIncrementing", ( PyCFunction ) ChainingIterator_isIncrementing, METH_NOARGS, "() Returns true if the current iteration is an incrementation."},
	{"getObject", ( PyCFunction ) ChainingIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ChainingIterator type definition ------------------------------*/

PyTypeObject ChainingIterator_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"ChainingIterator",				/* tp_name */
	sizeof( BPy_ChainingIterator ),	/* tp_basicsize */
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
	BPy_ChainingIterator_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&ViewEdgeIterator_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)ChainingIterator___init__,                       	/* initproc tp_init; */
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

int ChainingIterator___init__(BPy_ChainingIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0;

	if (!( PyArg_ParseTuple(args, "O|OOO", &obj1, &obj2, &obj3, &obj4) ))
	    return -1;

	if( obj1 && BPy_ChainingIterator_Check(obj1)  ) {
		self->c_it = new ChainingIterator(*( ((BPy_ChainingIterator *) obj1)->c_it ));
	
	} else {
		bool restrictToSelection = ( obj1 && PyBool_Check(obj1) ) ? bool_from_PyBool(obj1) : true;
		bool restrictToUnvisited = ( obj2 && PyBool_Check(obj2) ) ? bool_from_PyBool(obj2) : true;
		ViewEdge *begin = ( obj3 && BPy_ViewEdge_Check(obj3) ) ? ((BPy_ViewEdge *) obj3)->ve : 0;
		bool orientation = ( obj4 && PyBool_Check(obj4) ) ? bool_from_PyBool(obj4) : true;
		
		self->c_it = new ChainingIterator( restrictToSelection, restrictToUnvisited, begin, orientation);	
	}
	
	self->py_ve_it.ve_it = self->c_it;
	self->py_ve_it.py_it.it = self->c_it;
	
	return 0;
}

PyObject *ChainingIterator_init( BPy_ChainingIterator *self ) {
	self->c_it->init();
	
	Py_RETURN_NONE;
}

PyObject *ChainingIterator_traverse( BPy_ChainingIterator *self, PyObject *args ) {
	PyObject *py_a_it;

	if(!( PyArg_ParseTuple(args, "O", &py_a_it) && BPy_AdjacencyIterator_Check(py_a_it) )) {
		cout << "ERROR: ChainingIterator_traverse" << endl;
		Py_RETURN_NONE;
	}
	
	if( ((BPy_AdjacencyIterator *) py_a_it)->a_it )
		self->c_it->traverse(*( ((BPy_AdjacencyIterator *) py_a_it)->a_it ));
		
	Py_RETURN_NONE;
}


PyObject *ChainingIterator_getVertex( BPy_ChainingIterator *self ) {
	if( self->c_it->getVertex() )
		return BPy_ViewVertex_from_ViewVertex_ptr( self->c_it->getVertex()  );
		
	Py_RETURN_NONE;
}

PyObject *ChainingIterator_isIncrementing( BPy_ChainingIterator *self ) {
	return PyBool_from_bool( self->c_it->isIncrementing() );
}

PyObject * ChainingIterator_getObject( BPy_ChainingIterator *self) {
	
	ViewEdge *ve = self->c_it->operator*();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}



///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
