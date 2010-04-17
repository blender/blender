#include "BPy_Operators.h"

#include "BPy_BinaryPredicate1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVoid.h"
#include "Iterator/BPy_ViewEdgeIterator.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int Operators_Init( PyObject *module )
{	
	if( module == NULL )
		return -1;

	if( PyType_Ready( &Operators_Type ) < 0 )
		return -1;

	Py_INCREF( &Operators_Type );
	PyModule_AddObject(module, "Operators", (PyObject *)&Operators_Type);	
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char Operators___doc__[] =
"Class defining the operators used in a style module.  There are five\n"
"types of operators: Selection, chaining, splitting, sorting and\n"
"creation.  All these operators are user controlled through functors,\n"
"predicates and shaders that are taken as arguments.\n";

static void Operators___dealloc__(BPy_Operators* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static char Operators_select___doc__[] =
".. staticmethod:: select(pred)\n"
"\n"
"   Selects the ViewEdges of the ViewMap verifying a specified\n"
"   condition.\n"
"\n"
"   :arg pred: The predicate expressing this condition.\n"
"   :type pred: UnaryPredicate1D\n";

static PyObject * Operators_select(BPy_Operators* self, PyObject *args)
{
	PyObject *obj = 0;

	if ( !PyArg_ParseTuple(args, "O!", &UnaryPredicate1D_Type, &obj) )
		return NULL;
	if ( !((BPy_UnaryPredicate1D *) obj)->up1D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.select(): 1st argument: invalid UnaryPredicate1D object");
		return NULL;
	}

	if (Operators::select(*( ((BPy_UnaryPredicate1D *) obj)->up1D )) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.select() failed");
		return NULL;
	}

	Py_RETURN_NONE;
}

static char Operators_chain___doc__[] =
".. staticmethod:: chain(it, pred, modifier)\n"
"\n"
"   Builds a set of chains from the current set of ViewEdges.  Each\n"
"   ViewEdge of the current list starts a new chain.  The chaining\n"
"   operator then iterates over the ViewEdges of the ViewMap using the\n"
"   user specified iterator.  This operator only iterates using the\n"
"   increment operator and is therefore unidirectional.\n"
"\n"
"   :arg it: The iterator on the ViewEdges of the ViewMap. It contains\n"
"      the chaining rule.\n"
"   :type it: :class:`ViewEdgeIterator`\n"
"   :arg pred: The predicate on the ViewEdge that expresses the\n"
"      stopping condition.\n"
"   :type pred: :class:`UnaryPredicate1D`\n"
"   :arg modifier: A function that takes a ViewEdge as argument and\n"
"      that is used to modify the processed ViewEdge state (the\n"
"      timestamp incrementation is a typical illustration of such a\n"
"      modifier).\n"
"   :type modifier: :class:`UnaryFunction1DVoid`\n"
"\n"
".. staticmethod:: chain(it, pred)\n"
"\n"
"   Builds a set of chains from the current set of ViewEdges.  Each\n"
"   ViewEdge of the current list starts a new chain.  The chaining\n"
"   operator then iterates over the ViewEdges of the ViewMap using the\n"
"   user specified iterator.  This operator only iterates using the\n"
"   increment operator and is therefore unidirectional.  This chaining\n"
"   operator is different from the previous one because it doesn't take\n"
"   any modifier as argument.  Indeed, the time stamp (insuring that a\n"
"   ViewEdge is processed one time) is automatically managed in this\n"
"   case.\n"
"\n"
"   :arg it: The iterator on the ViewEdges of the ViewMap. It contains\n"
"      the chaining rule. \n"
"   :type it: :class:`ViewEdgeIterator`\n"
"   :arg pred: The predicate on the ViewEdge that expresses the\n"
"      stopping condition.\n"
"   :type pred: :class:`UnaryPredicate1D`\n";

// CHANGE: first parameter is a chaining iterator, not just a view

static PyObject * Operators_chain(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;

	if ( !PyArg_ParseTuple(args, "O!O!|O!", &ChainingIterator_Type, &obj1,
										    &UnaryPredicate1D_Type, &obj2,
										    &UnaryFunction1DVoid_Type, &obj3) )
		return NULL;
	if ( !((BPy_ChainingIterator *) obj1)->c_it ) {
		PyErr_SetString(PyExc_TypeError, "Operators.chain(): 1st argument: invalid ChainingIterator object");
		return NULL;
	}
	if ( !((BPy_UnaryPredicate1D *) obj2)->up1D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.chain(): 2nd argument: invalid UnaryPredicate1D object");
		return NULL;
	}

	if( !obj3 ) {
		
		if (Operators::chain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
								*( ((BPy_UnaryPredicate1D *) obj2)->up1D )  ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.chain() failed");
			return NULL;
		}
							
	} else {
		
		if ( !((BPy_UnaryFunction1DVoid *) obj3)->uf1D_void ) {
			PyErr_SetString(PyExc_TypeError, "Operators.chain(): 3rd argument: invalid UnaryFunction1DVoid object");
			return NULL;
		}
		if (Operators::chain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
								*( ((BPy_UnaryPredicate1D *) obj2)->up1D ),
								*( ((BPy_UnaryFunction1DVoid *) obj3)->uf1D_void )  ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.chain() failed");
			return NULL;
		}
		
	}
	
	Py_RETURN_NONE;
}

static char Operators_bidirectionalChain___doc__[] =
".. staticmethod:: bidirectionalChain(it, pred)\n"
"\n"
"   Builds a set of chains from the current set of ViewEdges.  Each\n"
"   ViewEdge of the current list potentially starts a new chain.  The\n"
"   chaining operator then iterates over the ViewEdges of the ViewMap\n"
"   using the user specified iterator.  This operator iterates both using\n"
"   the increment and decrement operators and is therefore bidirectional.\n"
"   This operator works with a ChainingIterator which contains the\n"
"   chaining rules.  It is this last one which can be told to chain only\n"
"   edges that belong to the selection or not to process twice a ViewEdge\n"
"   during the chaining.  Each time a ViewEdge is added to a chain, its\n"
"   chaining time stamp is incremented.  This allows you to keep track of\n"
"   the number of chains to which a ViewEdge belongs to.\n"
"\n"
"   :arg it: The ChainingIterator on the ViewEdges of the ViewMap.  It\n"
"      contains the chaining rule.\n"
"   :type it: :class:`ChainingIterator`\n"
"   :arg pred: The predicate on the ViewEdge that expresses the\n"
"      stopping condition.\n"
"   :type pred: :class:`UnaryPredicate1D`\n"
"\n"
".. staticmethod:: bidirectionalChain(it)\n"
"\n"
"   The only difference with the above bidirectional chaining algorithm\n"
"   is that we don't need to pass a stopping criterion.  This might be\n"
"   desirable when the stopping criterion is already contained in the\n"
"   iterator definition.  Builds a set of chains from the current set of\n"
"   ViewEdges.  Each ViewEdge of the current list potentially starts a new\n"
"   chain.  The chaining operator then iterates over the ViewEdges of the\n"
"   ViewMap using the user specified iterator.  This operator iterates\n"
"   both using the increment and decrement operators and is therefore\n"
"   bidirectional.  This operator works with a ChainingIterator which\n"
"   contains the chaining rules.  It is this last one which can be told to\n"
"   chain only edges that belong to the selection or not to process twice\n"
"   a ViewEdge during the chaining.  Each time a ViewEdge is added to a\n"
"   chain, its chaining time stamp is incremented.  This allows you to\n"
"   keep track of the number of chains to which a ViewEdge belongs to.\n"
"\n"
"   :arg it: The ChainingIterator on the ViewEdges of the ViewMap.  It\n"
"      contains the chaining rule.\n"
"   :type it: :class:`ChainingIterator`\n";

static PyObject * Operators_bidirectionalChain(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;

	if( !PyArg_ParseTuple(args, "O!|O!", &ChainingIterator_Type, &obj1, &UnaryPredicate1D_Type, &obj2) )
		return NULL;
	if ( !((BPy_ChainingIterator *) obj1)->c_it ) {
		PyErr_SetString(PyExc_TypeError, "Operators.bidirectionalChain(): 1st argument: invalid ChainingIterator object");
		return NULL;
	}

	if( !obj2 ) {

		if (Operators::bidirectionalChain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ) ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.bidirectionalChain() failed");
			return NULL;
		}
							
	} else {

		if ( !((BPy_UnaryPredicate1D *) obj2)->up1D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.bidirectionalChain(): 2nd argument: invalid UnaryPredicate1D object");
			return NULL;
		}
		if (Operators::bidirectionalChain( 	*( ((BPy_ChainingIterator *) obj1)->c_it ),
											*( ((BPy_UnaryPredicate1D *) obj2)->up1D ) ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.bidirectionalChain() failed");
			return NULL;
		}
		
	}
	
	Py_RETURN_NONE;
}

static char Operators_sequentialSplit___doc__[] =
".. staticmethod:: sequentialSplit(startingPred, stoppingPred, sampling=0.0)\n"
"\n"
"   Splits each chain of the current set of chains in a sequential way.\n"
"   The points of each chain are processed (with a specified sampling)\n"
"   sequentially. Each time a user specified starting condition is\n"
"   verified, a new chain begins and ends as soon as a user-defined\n"
"   stopping predicate is verified. This allows chains overlapping rather\n"
"   than chains partitioning. The first point of the initial chain is the\n"
"   first point of one of the resulting chains. The splitting ends when\n"
"   no more chain can start.\n"
"\n"
"   :arg startingPred: The predicate on a point that expresses the\n"
"      starting condition.\n"
"   :type startingPred: :class:`UnaryPredicate0D`\n"
"   :arg stoppingPred: The predicate on a point that expresses the\n"
"      stopping condition.\n"
"   :type stoppingPred: :class:`UnaryPredicate0D`\n"
"   :arg sampling: The resolution used to sample the chain for the\n"
"      predicates evaluation. (The chain is not actually resampled;\n"
"      a virtual point only progresses along the curve using this\n"
"      resolution.)\n"
"   :type sampling: float\n"
"\n"
".. staticmethod:: sequentialSplit(pred, sampling=0.0)\n"
"\n"
"   Splits each chain of the current set of chains in a sequential way.\n"
"   The points of each chain are processed (with a specified sampling)\n"
"   sequentially and each time a user specified condition is verified,\n"
"   the chain is split into two chains.  The resulting set of chains is a\n"
"   partition of the initial chain\n"
"\n"
"   :arg pred: The predicate on a point that expresses the splitting\n"
"      condition.\n"
"   :type pred: :class:`UnaryPredicate0D`\n"
"   :arg sampling: The resolution used to sample the chain for the\n"
"      predicate evaluation. (The chain is not actually resampled; a\n"
"      virtual point only progresses along the curve using this\n"
"      resolution.)\n"
"   :type sampling: float\n";

static PyObject * Operators_sequentialSplit(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;
	float f = 0.0;

	if( !PyArg_ParseTuple(args, "O!|Of", &UnaryPredicate0D_Type, &obj1, &obj2, &f) )
		return NULL;
	if ( !((BPy_UnaryPredicate0D *) obj1)->up0D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.sequentialSplit(): 1st argument: invalid UnaryPredicate0D object");
		return NULL;
	}

	if( obj2 && BPy_UnaryPredicate0D_Check(obj2) ) {
		
		if ( !((BPy_UnaryPredicate0D *) obj2)->up0D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.sequentialSplit(): 2nd argument: invalid UnaryPredicate0D object");
			return NULL;
		}
		if (Operators::sequentialSplit(	*( ((BPy_UnaryPredicate0D *) obj1)->up0D ),
										*( ((BPy_UnaryPredicate0D *) obj2)->up0D ),
										f ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.sequentialSplit() failed");
			return NULL;
		}

	} else {
		
		if ( obj2 ) {
			if ( !PyFloat_Check(obj2) ) {
				PyErr_SetString(PyExc_TypeError, "Operators.sequentialSplit(): invalid 2nd argument");
				return NULL;
			}
			f = PyFloat_AsDouble(obj2);
		}
		if (Operators::sequentialSplit( *( ((BPy_UnaryPredicate0D *) obj1)->up0D ), f ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.sequentialSplit() failed");
			return NULL;
		}
		
	}
	
	Py_RETURN_NONE;
}

static char Operators_recursiveSplit___doc__[] =
".. staticmethod:: recursiveSplit(func, pred, sampling=0.0)\n"
"\n"
"   Splits the current set of chains in a recursive way.  We process the\n"
"   points of each chain (with a specified sampling) to find the point\n"
"   minimizing a specified function.  The chain is split in two at this\n"
"   point and the two new chains are processed in the same way.  The\n"
"   recursivity level is controlled through a predicate 1D that expresses\n"
"   a stopping condition on the chain that is about to be processed.\n"
"\n"
"   :arg func: The Unary Function evaluated at each point of the chain.\n"
"     The splitting point is the point minimizing this function.\n"
"   :type func: :class:`UnaryFunction0DDouble`\n"
"   :arg pred: The Unary Predicate expressing the recursivity stopping\n"
"      condition.  This predicate is evaluated for each curve before it\n"
"      actually gets split.  If pred(chain) is true, the curve won't be\n"
"      split anymore.\n"
"   :type pred: :class:`UnaryPredicate1D`\n"
"   :arg sampling: The resolution used to sample the chain for the\n"
"      predicates evaluation. (The chain is not actually resampled, a\n"
"      virtual point only progresses along the curve using this\n"
"      resolution.)\n"
"   :type sampling: float\n"
"\n"
".. staticmethod:: recursiveSplit(func, pred0d, pred, sampling=0.0)\n"
"\n"
"   Splits the current set of chains in a recursive way.  We process the\n"
"   points of each chain (with a specified sampling) to find the point\n"
"   minimizing a specified function.  The chain is split in two at this\n"
"   point and the two new chains are processed in the same way.  The user\n"
"   can specify a 0D predicate to make a first selection on the points\n"
"   that can potentially be split.  A point that doesn't verify the 0D\n"
"   predicate won't be candidate in realizing the min.  The recursivity\n"
"   level is controlled through a predicate 1D that expresses a stopping\n"
"   condition on the chain that is about to be processed.\n"
"\n"
"   :arg func: The Unary Function evaluated at each point of the chain.\n"
"      The splitting point is the point minimizing this function.\n"
"   :type func: :class:`UnaryFunction0DDouble`\n"
"   :arg pred0d: The Unary Predicate 0D used to select the candidate\n"
"      points where the split can occur.  For example, it is very likely\n"
"      that would rather have your chain splitting around its middle\n"
"      point than around one of its extremities.  A 0D predicate working\n"
"      on the curvilinear abscissa allows to add this kind of constraints.\n"
"   :type pred0d: :class:`UnaryPredicate0D`\n"
"   :arg pred: The Unary Predicate expressing the recursivity stopping\n"
"      condition. This predicate is evaluated for each curve before it\n"
"      actually gets split.  If pred(chain) is true, the curve won't be\n"
"      split anymore.\n"
"   :type pred: :class:`UnaryPredicate1D`\n"
"   :arg sampling: The resolution used to sample the chain for the\n"
"      predicates evaluation. (The chain is not actually resampled; a\n"
"      virtual point only progresses along the curve using this\n"
"      resolution.)\n"
"   :type sampling: float\n";

static PyObject * Operators_recursiveSplit(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;
	float f = 0.0;

	if ( !PyArg_ParseTuple(args, "O!O|Of", &UnaryFunction0DDouble_Type, &obj1, &obj2, &obj3, &f) )
		return NULL;
	if ( !((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double ) {
		PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): 1st argument: invalid UnaryFunction0DDouble object");
		return NULL;
	}
	
	if ( BPy_UnaryPredicate1D_Check(obj2) ) {

		if ( !((BPy_UnaryPredicate1D *) obj2)->up1D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): 2nd argument: invalid UnaryPredicate1D object");
			return NULL;
		}
		if ( obj3 ) {
			if ( !PyFloat_Check(obj3) ) {
				PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): invalid 3rd argument");
				return NULL;
			}
			f = PyFloat_AsDouble(obj3);
		}
		if (Operators::recursiveSplit( 	*( ((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double ),
										*( ((BPy_UnaryPredicate1D *) obj2)->up1D ),
										f ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.recursiveSplit() failed");
			return NULL;
		}
	
	} else {

		if ( !BPy_UnaryPredicate0D_Check(obj2) || !((BPy_UnaryPredicate0D *) obj2)->up0D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): invalid 2nd argument");
			return NULL;
		}
		if ( !BPy_UnaryPredicate1D_Check(obj3) || !((BPy_UnaryPredicate1D *) obj3)->up1D ) {
			PyErr_SetString(PyExc_TypeError, "Operators.recursiveSplit(): invalid 3rd argument");
			return NULL;
		}
		if (Operators::recursiveSplit( 	*( ((BPy_UnaryFunction0DDouble *) obj1)->uf0D_double ),
										*( ((BPy_UnaryPredicate0D *) obj2)->up0D ),
										*( ((BPy_UnaryPredicate1D *) obj3)->up1D ),
										f ) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.recursiveSplit() failed");
			return NULL;
		}

	}
	
	Py_RETURN_NONE;
}

static char Operators_sort___doc__[] =
".. staticmethod:: sort(pred)\n"
"\n"
"   Sorts the current set of chains (or viewedges) according to the\n"
"   comparison predicate given as argument.\n"
"\n"
"   :arg pred: The binary predicate used for the comparison.\n"
"   :type pred: BinaryPredicate1D\n";

static PyObject * Operators_sort(BPy_Operators* self, PyObject *args)
{
	PyObject *obj = 0;

	if ( !PyArg_ParseTuple(args, "O!", &BinaryPredicate1D_Type, &obj) )
		return NULL;
	if ( !((BPy_BinaryPredicate1D *) obj)->bp1D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.sort(): 1st argument: invalid BinaryPredicate1D object");
		return NULL;
	}

	if (Operators::sort(*( ((BPy_BinaryPredicate1D *) obj)->bp1D )) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.sort() failed");
		return NULL;
	}
	Py_RETURN_NONE;
}

static char Operators_create___doc__[] =
".. staticmethod:: create(pred, shaders)\n"
"\n"
"   Creates and shades the strokes from the current set of chains.  A\n"
"   predicate can be specified to make a selection pass on the chains.\n"
"\n"
"   :arg pred: The predicate that a chain must verify in order to be\n"
"      transform as a stroke.\n"
"   :type pred: :class:`UnaryPredicate1D`\n"
"   :arg shaders: The list of shaders used to shade the strokes.\n"
"   :type shaders: List of StrokeShader objects\n";

static PyObject * Operators_create(BPy_Operators* self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0;

	if ( !PyArg_ParseTuple(args, "O!O!", &UnaryPredicate1D_Type, &obj1, &PyList_Type, &obj2) )
		return NULL;
	if ( !((BPy_UnaryPredicate1D *) obj1)->up1D ) {
		PyErr_SetString(PyExc_TypeError, "Operators.create(): 1st argument: invalid UnaryPredicate1D object");
		return NULL;
	}

	vector<StrokeShader *> shaders;
	for( int i = 0; i < PyList_Size(obj2); i++) {
		PyObject *py_ss = PyList_GetItem(obj2,i);
		
		if ( !BPy_StrokeShader_Check(py_ss) ) {
			PyErr_SetString(PyExc_TypeError, "Operators.create() 2nd argument must be a list of StrokeShader objects");
			return NULL;
		}
		shaders.push_back( ((BPy_StrokeShader *) py_ss)->ss );
	}
	
	if (Operators::create( *( ((BPy_UnaryPredicate1D *) obj1)->up1D ), shaders) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.create() failed");
		return NULL;
	}

	Py_RETURN_NONE;
}

static char Operators_getViewEdgesSize___doc__[] =
".. staticmethod:: getViewEdgesSize()\n"
"\n"
"   Returns the number of ViewEdges.\n"
"\n"
"   :return: The number of ViewEdges.\n"
"   :rtype: int\n";

static PyObject * Operators_getViewEdgesSize( BPy_Operators* self) {
	return PyLong_FromLong( Operators::getViewEdgesSize() );
}

static char Operators_getChainsSize___doc__[] =
".. staticmethod:: getChainsSize()\n"
"\n"
"   Returns the number of Chains.\n"
"\n"
"   :return: The number of Chains.\n"
"   :rtype: int\n";

static PyObject * Operators_getChainsSize( BPy_Operators* self ) {
	return PyLong_FromLong( Operators::getChainsSize() );
}

static char Operators_getStrokesSize___doc__[] =
".. staticmethod:: getStrokesSize()\n"
"\n"
"   Returns the number of Strokes.\n"
"\n"
"   :return: The number of Strokes.\n"
"   :rtype: int\n";

static PyObject * Operators_getStrokesSize( BPy_Operators* self) {
	return PyLong_FromLong( Operators::getStrokesSize() );
}

/*----------------------Operators instance definitions ----------------------------*/
static PyMethodDef BPy_Operators_methods[] = {
	{"select", ( PyCFunction ) Operators_select, METH_VARARGS | METH_STATIC, Operators_select___doc__},
	{"chain", ( PyCFunction ) Operators_chain, METH_VARARGS | METH_STATIC, Operators_chain___doc__},
	{"bidirectionalChain", ( PyCFunction ) Operators_bidirectionalChain, METH_VARARGS | METH_STATIC, Operators_bidirectionalChain___doc__},
	{"sequentialSplit", ( PyCFunction ) Operators_sequentialSplit, METH_VARARGS | METH_STATIC, Operators_sequentialSplit___doc__},
	{"recursiveSplit", ( PyCFunction ) Operators_recursiveSplit, METH_VARARGS | METH_STATIC, Operators_recursiveSplit___doc__},
	{"sort", ( PyCFunction ) Operators_sort, METH_VARARGS | METH_STATIC, Operators_sort___doc__},
	{"create", ( PyCFunction ) Operators_create, METH_VARARGS | METH_STATIC, Operators_create___doc__},
	{"getViewEdgesSize", ( PyCFunction ) Operators_getViewEdgesSize, METH_NOARGS | METH_STATIC, Operators_getViewEdgesSize___doc__},
	{"getChainsSize", ( PyCFunction ) Operators_getChainsSize, METH_NOARGS | METH_STATIC, Operators_getChainsSize___doc__},
	{"getStrokesSize", ( PyCFunction ) Operators_getStrokesSize, METH_NOARGS | METH_STATIC, Operators_getStrokesSize___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Operators type definition ------------------------------*/

PyTypeObject Operators_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Operators",                    /* tp_name */
	sizeof(BPy_Operators),          /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)Operators___dealloc__, /* tp_dealloc */
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
	Py_TPFLAGS_DEFAULT,             /* tp_flags */
	Operators___doc__,              /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Operators_methods,          /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	0,                              /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


