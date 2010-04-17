#include "BPy_ChainingIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_ViewVertex.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "BPy_AdjacencyIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ChainingIterator___doc__[] =
"Base class for chaining iterators.  This class is designed to be\n"
"overloaded in order to describe chaining rules.  It makes the\n"
"description of chaining rules easier.  The two main methods that need\n"
"to overloaded are traverse() and init().  traverse() tells which\n"
":class:`ViewEdge` to follow, among the adjacent ones.  If you specify\n"
"restriction rules (such as \"Chain only ViewEdges of the selection\"),\n"
"they will be included in the adjacency iterator (i.e, the adjacent\n"
"iterator will only stop on \"valid\" edges).\n"
"\n"
".. method:: __init__(iRestrictToSelection=True, iRestrictToUnvisited=True, begin=None, orientation=True)\n"
"\n"
"   Builds a Chaining Iterator from the first ViewEdge used for\n"
"   iteration and its orientation.\n"
"\n"
"   :arg iRestrictToSelection: Indicates whether to force the chaining\n"
"      to stay within the set of selected ViewEdges or not.\n"
"   :type iRestrictToSelection: bool\n"
"   :arg iRestrictToUnvisited: Indicates whether a ViewEdge that has\n"
"      already been chained must be ignored ot not.\n"
"   :type iRestrictToUnvisited: bool\n"
"   :arg begin: The ViewEdge from which to start the chain.\n"
"   :type begin: :class:`ViewEdge` or None\n"
"   :arg orientation: The direction to follow to explore the graph.  If\n"
"      true, the direction indicated by the first ViewEdge is used.\n"
"   :type orientation: bool\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: \n"
"   :type brother: ChainingIterator\n";

static int ChainingIterator___init__(BPy_ChainingIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0;

	if (!( PyArg_ParseTuple(args, "|OOOO", &obj1, &obj2, &obj3, &obj4) ))
	    return -1;

	if( obj1 && BPy_ChainingIterator_Check(obj1)  ) {
		self->c_it = new ChainingIterator(*( ((BPy_ChainingIterator *) obj1)->c_it ));
	
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
		
		self->c_it = new ChainingIterator( restrictToSelection, restrictToUnvisited, begin, orientation);	
	}
	
	self->py_ve_it.ve_it = self->c_it;
	self->py_ve_it.py_it.it = self->c_it;
	
	self->c_it->py_c_it = (PyObject *) self;
	
	return 0;
}

static char ChainingIterator_init___doc__[] =
".. method:: init()\n"
"\n"
"   Initializes the iterator context.  This method is called each\n"
"   time a new chain is started.  It can be used to reset some\n"
"   history information that you might want to keep.\n";

static PyObject *ChainingIterator_init( BPy_ChainingIterator *self ) {
	if( typeid(*(self->c_it)) == typeid(ChainingIterator) ) {
		PyErr_SetString(PyExc_TypeError, "init() method not properly overridden");
		return NULL;
	}
	self->c_it->init();
	
	Py_RETURN_NONE;
}

static char ChainingIterator_traverse___doc__[] =
".. method:: traverse(it)\n"
"\n"
"   This method iterates over the potential next ViewEdges and returns\n"
"   the one that will be followed next.  Returns the next ViewEdge to\n"
"   follow or None when the end of the chain is reached.\n"
"\n"
"   :arg it: The iterator over the ViewEdges adjacent to the end vertex\n"
"      of the current ViewEdge.  The adjacency iterator reflects the\n"
"      restriction rules by only iterating over the valid ViewEdges.\n"
"   :type it: :class:`AdjacencyIterator`\n"
"   :return: Returns the next ViewEdge to follow, or None if chaining ends.\n"
"   :rtype: :class:`ViewEdge` or None\n";

static PyObject *ChainingIterator_traverse( BPy_ChainingIterator *self, PyObject *args ) {
	PyObject *py_a_it;

	if(!( PyArg_ParseTuple(args, "O!", &AdjacencyIterator_Type, &py_a_it) ))
		return NULL;
	
	if( typeid(*(self->c_it)) == typeid(ChainingIterator) ) {
		PyErr_SetString(PyExc_TypeError, "traverse() method not properly overridden");
		return NULL;
	}
	if( ((BPy_AdjacencyIterator *) py_a_it)->a_it )
		self->c_it->traverse(*( ((BPy_AdjacencyIterator *) py_a_it)->a_it ));
		
	Py_RETURN_NONE;
}

static char ChainingIterator_getVertex___doc__[] =
".. method:: getVertex()\n"
"\n"
"   Returns the vertex which is the next crossing.\n"
"\n"
"   :return: The vertex which is the next crossing.\n"
"   :rtype: :class:`ViewVertex`\n";

static PyObject *ChainingIterator_getVertex( BPy_ChainingIterator *self ) {
	ViewVertex *v = self->c_it->getVertex();
	if( v )
		return Any_BPy_ViewVertex_from_ViewVertex( *v );
		
	Py_RETURN_NONE;
}

static char ChainingIterator_isIncrementing___doc__[] =
".. method:: isIncrementing()\n"
"\n"
"   Returns true if the current iteration is an incrementation.\n"
"\n"
"   :return: True if the current iteration is an incrementation.\n"
"   :rtype: bool\n";

static PyObject *ChainingIterator_isIncrementing( BPy_ChainingIterator *self ) {
	return PyBool_from_bool( self->c_it->isIncrementing() );
}

static char ChainingIterator_getObject___doc__[] =
".. method:: getObject()\n"
"\n"
"   Returns the pointed ViewEdge.\n"
"\n"
"   :return: The pointed ViewEdge.\n"
"   :rtype: :class:`ViewEdge`\n";

static PyObject * ChainingIterator_getObject( BPy_ChainingIterator *self) {
	
	ViewEdge *ve = self->c_it->operator*();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}

/*----------------------ChainingIterator instance definitions ----------------------------*/
static PyMethodDef BPy_ChainingIterator_methods[] = {
	{"init", ( PyCFunction ) ChainingIterator_init, METH_NOARGS, ChainingIterator_init___doc__},
	{"traverse", ( PyCFunction ) ChainingIterator_traverse, METH_VARARGS, ChainingIterator_traverse___doc__},
	{"getVertex", ( PyCFunction ) ChainingIterator_getVertex, METH_NOARGS, ChainingIterator_getVertex___doc__},
	{"isIncrementing", ( PyCFunction ) ChainingIterator_isIncrementing, METH_NOARGS, ChainingIterator_isIncrementing___doc__},
	{"getObject", ( PyCFunction ) ChainingIterator_getObject, METH_NOARGS, ChainingIterator_getObject___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ChainingIterator type definition ------------------------------*/

PyTypeObject ChainingIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ChainingIterator",             /* tp_name */
	sizeof(BPy_ChainingIterator),   /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
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
	ChainingIterator___doc__,       /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ChainingIterator_methods,   /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&ViewEdgeIterator_Type,         /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ChainingIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
