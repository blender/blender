#include "BPy_ChainPredicateIterator.h"

#include "../BPy_Convert.h"
#include "../BPy_BinaryPredicate1D.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ChainPredicateIterator___doc__[] =
"A \"generic\" user-controlled ViewEdge iterator.  This iterator is in\n"
"particular built from a unary predicate and a binary predicate.\n"
"First, the unary predicate is evaluated for all potential next\n"
"ViewEdges in order to only keep the ones respecting a certain\n"
"constraint.  Then, the binary predicate is evaluated on the current\n"
"ViewEdge together with each ViewEdge of the previous selection.  The\n"
"first ViewEdge respecting both the unary predicate and the binary\n"
"predicate is kept as the next one.  If none of the potential next\n"
"ViewEdge respects these two predicates, None is returned.\n"
"\n"
".. method:: __init__(iRestrictToSelection=True, iRestrictToUnvisited=True, begin=None, orientation=True)\n"
"\n"
"   Builds a ChainPredicateIterator from a starting ViewEdge and its\n"
"   orientation.\n"
"\n"
"   :arg iRestrictToSelection: Indicates whether to force the chaining\n"
"      to stay within the set of selected ViewEdges or not.\n"
"   :type iRestrictToSelection: bool\n"
"   :arg iRestrictToUnvisited: Indicates whether a ViewEdge that has\n"
"      already been chained must be ignored ot not.\n"
"   :type iRestrictToUnvisited: bool\n"
"   :arg begin: The ViewEdge from where to start the iteration.\n"
"   :type begin: :class:`ViewEdge` or None\n"
"   :arg orientation: If true, we'll look for the next ViewEdge among\n"
"      the ViewEdges that surround the ending ViewVertex of begin.  If\n"
"      false, we'll search over the ViewEdges surrounding the ending\n"
"      ViewVertex of begin. \n"
"   :type orientation: bool\n"
"\n"
".. method:: __init__(upred, bpred, iRestrictToSelection=True, iRestrictToUnvisited=True, begin=None, orientation=True)\n"
"\n"
"   Builds a ChainPredicateIterator from a unary predicate, a binary\n"
"   predicate, a starting ViewEdge and its orientation.\n"
"\n"
"   :arg upred: The unary predicate that the next ViewEdge must satisfy.\n"
"   :type upred: :class:`UnaryPredicate1D`\n"
"   :arg bpred: The binary predicate that the next ViewEdge must\n"
"      satisfy together with the actual pointed ViewEdge.\n"
"   :type bpred: :class:`BinaryPredicate1D`\n"
"   :arg iRestrictToSelection: Indicates whether to force the chaining\n"
"      to stay within the set of selected ViewEdges or not.\n"
"   :type iRestrictToSelection: bool\n"
"   :arg iRestrictToUnvisited: Indicates whether a ViewEdge that has\n"
"      already been chained must be ignored ot not.\n"
"   :type iRestrictToUnvisited: bool\n"
"   :arg begin: The ViewEdge from where to start the iteration.\n"
"   :type begin: :class:`ViewEdge`\n"
"   :arg orientation: If true, we'll look for the next ViewEdge among\n"
"      the ViewEdges that surround the ending ViewVertex of begin.  If\n"
"      false, we'll search over the ViewEdges surrounding the ending\n"
"      ViewVertex of begin.\n"
"   :type orientation: bool\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: A ChainPredicateIterator object.\n"
"   :type brother: :class:`ChainPredicateIterator`\n";

static int ChainPredicateIterator___init__(BPy_ChainPredicateIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0, *obj5 = 0, *obj6 = 0;

	if (!( PyArg_ParseTuple(args, "|OOOOOO", &obj1, &obj2, &obj3, &obj4, &obj5, &obj6) ))
	    return -1;

	if( obj1 && BPy_ChainPredicateIterator_Check(obj1)  ) { 
		self->cp_it = new ChainPredicateIterator(*( ((BPy_ChainPredicateIterator *) obj1)->cp_it ));
		self->upred = NULL;
		self->bpred = NULL;
	
	} else if( 	obj1 && BPy_UnaryPredicate1D_Check(obj1) &&
	  			obj2 && BPy_BinaryPredicate1D_Check(obj2) ) {
				
		if (!((BPy_UnaryPredicate1D *) obj1)->up1D) {
			PyErr_SetString(PyExc_TypeError, "1st argument: invalid UnaryPredicate1D object");
			return -1;
		}
		if (!((BPy_BinaryPredicate1D *) obj2)->bp1D) {
			PyErr_SetString(PyExc_TypeError, "2nd argument: invalid BinaryPredicate1D object");
			return -1;
		}
		UnaryPredicate1D *up1D = ((BPy_UnaryPredicate1D *) obj1)->up1D;
		BinaryPredicate1D *bp1D = ((BPy_BinaryPredicate1D *) obj2)->bp1D;
		bool restrictToSelection = ( obj3 ) ? bool_from_PyBool(obj3) : true;
		bool restrictToUnvisited = ( obj4 ) ? bool_from_PyBool(obj4) : true;
		ViewEdge *begin;
		if ( !obj5 || obj5 == Py_None )
			begin = NULL;
		else if ( BPy_ViewEdge_Check(obj5) )
			begin = ((BPy_ViewEdge *) obj5)->ve;
		else {
			PyErr_SetString(PyExc_TypeError, "5th argument must be either a ViewEdge object or None");
			return -1;
		}
		bool orientation = ( obj6 ) ? bool_from_PyBool(obj6) : true;
	
		self->cp_it = new ChainPredicateIterator( *up1D, *bp1D, restrictToSelection, restrictToUnvisited, begin, orientation);
		self->upred = obj1;
		self->bpred = obj2;
		Py_INCREF( self->upred );
		Py_INCREF( self->bpred );
	
	} else {
		bool restrictToSelection = ( obj1 ) ? bool_from_PyBool(obj1) : true;
		bool restrictToUnvisited = ( obj2 ) ? bool_from_PyBool(obj2) : true;
		ViewEdge *begin;
		if ( !obj3 || obj3 == Py_None )
			begin = NULL;
		else if ( BPy_ViewEdge_Check(obj3) )
			begin = ((BPy_ViewEdge *) obj3)->ve;
		else {
			PyErr_SetString(PyExc_TypeError, "3rd argument must be either a ViewEdge object or None");
			return -1;
		}
		bool orientation = ( obj4 ) ? bool_from_PyBool(obj4) : true;
		
		self->cp_it = new ChainPredicateIterator( restrictToSelection, restrictToUnvisited, begin, orientation);	
		self->upred = NULL;
		self->bpred = NULL;
	}
	
	self->py_c_it.c_it = self->cp_it;
	self->py_c_it.py_ve_it.ve_it = self->cp_it;
	self->py_c_it.py_ve_it.py_it.it = self->cp_it;
	
	
	return 0;
	
}

static void ChainPredicateIterator___dealloc__(BPy_ChainPredicateIterator *self)
{
	Py_XDECREF( self->upred );
	Py_XDECREF( self->bpred );
	ChainingIterator_Type.tp_dealloc((PyObject *)self);
}

/*-----------------------BPy_ChainPredicateIterator type definition ------------------------------*/

PyTypeObject ChainPredicateIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ChainPredicateIterator",       /* tp_name */
	sizeof(BPy_ChainPredicateIterator), /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)ChainPredicateIterator___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	ChainPredicateIterator___doc__, /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&ChainingIterator_Type,         /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ChainPredicateIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
