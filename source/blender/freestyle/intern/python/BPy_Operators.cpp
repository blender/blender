/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/freestyle/intern/python/BPy_Operators.cpp
 *  \ingroup freestyle
 */

#include "BPy_Operators.h"

#include "BPy_BinaryPredicate1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVoid.h"
#include "Iterator/BPy_ViewEdgeIterator.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "BPy_StrokeShader.h"
#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int Operators_Init(PyObject *module)
{
	if (module == NULL)
		return -1;

	if (PyType_Ready(&Operators_Type) < 0)
		return -1;
	Py_INCREF(&Operators_Type);
	PyModule_AddObject(module, "Operators", (PyObject *)&Operators_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(Operators_doc,
"Class defining the operators used in a style module.  There are five\n"
"types of operators: Selection, chaining, splitting, sorting and\n"
"creation.  All these operators are user controlled through functors,\n"
"predicates and shaders that are taken as arguments.");

static void Operators_dealloc(BPy_Operators *self)
{
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(Operators_select_doc,
".. staticmethod:: select(pred)\n"
"\n"
"   Selects the ViewEdges of the ViewMap verifying a specified\n"
"   condition.\n"
"\n"
"   :arg pred: The predicate expressing this condition.\n"
"   :type pred: :class:`UnaryPredicate1D`");

static PyObject *Operators_select(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"pred", NULL};
	PyObject *obj = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &UnaryPredicate1D_Type, &obj))
		return NULL;
	if (!((BPy_UnaryPredicate1D *)obj)->up1D) {
		PyErr_SetString(PyExc_TypeError, "Operators.select(): 1st argument: invalid UnaryPredicate1D object");
		return NULL;
	}
	if (Operators::select(*(((BPy_UnaryPredicate1D *)obj)->up1D)) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.select() failed");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Operators_chain_doc,
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
"   :type pred: :class:`UnaryPredicate1D`");

static PyObject *Operators_chain(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"it", "pred", "modifier", NULL};
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!O!|O!", (char **)kwlist,
	                                 &ChainingIterator_Type, &obj1,
	                                 &UnaryPredicate1D_Type, &obj2,
	                                 &UnaryFunction1DVoid_Type, &obj3))
	{
		return NULL;
	}
	if (!((BPy_ChainingIterator *)obj1)->c_it) {
		PyErr_SetString(PyExc_TypeError, "Operators.chain(): 1st argument: invalid ChainingIterator object");
		return NULL;
	}
	if (!((BPy_UnaryPredicate1D *)obj2)->up1D) {
		PyErr_SetString(PyExc_TypeError, "Operators.chain(): 2nd argument: invalid UnaryPredicate1D object");
		return NULL;
	}
	if (!obj3) {
		if (Operators::chain(*(((BPy_ChainingIterator *)obj1)->c_it),
		                     *(((BPy_UnaryPredicate1D *)obj2)->up1D)) < 0)
		{
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.chain() failed");
			return NULL;
		}
	}
	else {
		if (!((BPy_UnaryFunction1DVoid *)obj3)->uf1D_void) {
			PyErr_SetString(PyExc_TypeError, "Operators.chain(): 3rd argument: invalid UnaryFunction1DVoid object");
			return NULL;
		}
		if (Operators::chain(*(((BPy_ChainingIterator *)obj1)->c_it),
		                     *(((BPy_UnaryPredicate1D *)obj2)->up1D),
		                     *(((BPy_UnaryFunction1DVoid *)obj3)->uf1D_void)) < 0)
		{
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.chain() failed");
			return NULL;
		}
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Operators_bidirectional_chain_doc,
".. staticmethod:: bidirectional_chain(it, pred)\n"
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
".. staticmethod:: bidirectional_chain(it)\n"
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
"   :type it: :class:`ChainingIterator`");

static PyObject *Operators_bidirectional_chain(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"it", "pred", NULL};
	PyObject *obj1 = 0, *obj2 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!|O!", (char **)kwlist,
	                                 &ChainingIterator_Type, &obj1, &UnaryPredicate1D_Type, &obj2))
	{
		return NULL;
	}
	if (!((BPy_ChainingIterator *)obj1)->c_it) {
		PyErr_SetString(PyExc_TypeError,
		                "Operators.bidirectional_chain(): 1st argument: invalid ChainingIterator object");
		return NULL;
	}
	if (!obj2) {
		if (Operators::bidirectionalChain(*(((BPy_ChainingIterator *)obj1)->c_it)) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.bidirectional_chain() failed");
			return NULL;
		}
	}
	else {
		if (!((BPy_UnaryPredicate1D *)obj2)->up1D) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.bidirectional_chain(): 2nd argument: invalid UnaryPredicate1D object");
			return NULL;
		}
		if (Operators::bidirectionalChain(*(((BPy_ChainingIterator *)obj1)->c_it),
		                                  *(((BPy_UnaryPredicate1D *)obj2)->up1D)) < 0)
		{
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.bidirectional_chain() failed");
			return NULL;
		}
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Operators_sequential_split_doc,
".. staticmethod:: sequential_split(starting_pred, stopping_pred, sampling=0.0)\n"
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
"   :arg starting_pred: The predicate on a point that expresses the\n"
"      starting condition.\n"
"   :type starting_pred: :class:`UnaryPredicate0D`\n"
"   :arg stopping_pred: The predicate on a point that expresses the\n"
"      stopping condition.\n"
"   :type stopping_pred: :class:`UnaryPredicate0D`\n"
"   :arg sampling: The resolution used to sample the chain for the\n"
"      predicates evaluation. (The chain is not actually resampled;\n"
"      a virtual point only progresses along the curve using this\n"
"      resolution.)\n"
"   :type sampling: float\n"
"\n"
".. staticmethod:: sequential_split(pred, sampling=0.0)\n"
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
"   :type sampling: float");

static PyObject *Operators_sequential_split(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist_1[] = {"starting_pred", "stopping_pred", "sampling", NULL};
	static const char *kwlist_2[] = {"pred", "sampling", NULL};
	PyObject *obj1 = 0, *obj2 = 0;
	float f = 0.0f;

	if (PyArg_ParseTupleAndKeywords(args, kwds, "O!O!|f", (char **)kwlist_1,
	                                &UnaryPredicate0D_Type, &obj1, &UnaryPredicate0D_Type, &obj2, &f))
	{
		if (!((BPy_UnaryPredicate0D *)obj1)->up0D) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.sequential_split(): 1st argument: invalid UnaryPredicate0D object");
			return NULL;
		}
		if (!((BPy_UnaryPredicate0D *)obj2)->up0D) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.sequential_split(): 2nd argument: invalid UnaryPredicate0D object");
			return NULL;
		}
		if (Operators::sequentialSplit(*(((BPy_UnaryPredicate0D *)obj1)->up0D),
		                               *(((BPy_UnaryPredicate0D *)obj2)->up0D),
		                               f) < 0)
		{
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.sequential_split() failed");
			return NULL;
		}
	}
	else if (PyErr_Clear(), (f = 0.0f),
	         PyArg_ParseTupleAndKeywords(args, kwds, "O!|f", (char **)kwlist_2,
	                                     &UnaryPredicate0D_Type, &obj1, &f))
	{
		if (!((BPy_UnaryPredicate0D *)obj1)->up0D) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.sequential_split(): 1st argument: invalid UnaryPredicate0D object");
			return NULL;
		}
		if (Operators::sequentialSplit(*(((BPy_UnaryPredicate0D *)obj1)->up0D), f) < 0) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.sequential_split() failed");
			return NULL;
		}
	}
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Operators_recursive_split_doc,
".. staticmethod:: recursive_split(func, pred_1d, sampling=0.0)\n"
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
"   :arg pred_1d: The Unary Predicate expressing the recursivity stopping\n"
"      condition.  This predicate is evaluated for each curve before it\n"
"      actually gets split.  If pred_1d(chain) is true, the curve won't be\n"
"      split anymore.\n"
"   :type pred_1d: :class:`UnaryPredicate1D`\n"
"   :arg sampling: The resolution used to sample the chain for the\n"
"      predicates evaluation. (The chain is not actually resampled, a\n"
"      virtual point only progresses along the curve using this\n"
"      resolution.)\n"
"   :type sampling: float\n"
"\n"
".. staticmethod:: recursive_split(func, pred_0d, pred_1d, sampling=0.0)\n"
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
"   :arg pred_0d: The Unary Predicate 0D used to select the candidate\n"
"      points where the split can occur.  For example, it is very likely\n"
"      that would rather have your chain splitting around its middle\n"
"      point than around one of its extremities.  A 0D predicate working\n"
"      on the curvilinear abscissa allows to add this kind of constraints.\n"
"   :type pred_0d: :class:`UnaryPredicate0D`\n"
"   :arg pred_1d: The Unary Predicate expressing the recursivity stopping\n"
"      condition. This predicate is evaluated for each curve before it\n"
"      actually gets split.  If pred_1d(chain) is true, the curve won't be\n"
"      split anymore.\n"
"   :type pred_1d: :class:`UnaryPredicate1D`\n"
"   :arg sampling: The resolution used to sample the chain for the\n"
"      predicates evaluation. (The chain is not actually resampled; a\n"
"      virtual point only progresses along the curve using this\n"
"      resolution.)\n"
"   :type sampling: float");

static PyObject *Operators_recursive_split(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist_1[] = {"func", "pred_1d", "sampling", NULL};
	static const char *kwlist_2[] = {"func", "pred_0d", "pred_1d", "sampling", NULL};
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0;
	float f = 0.0f;

	if (PyArg_ParseTupleAndKeywords(args, kwds, "O!O!|f", (char **)kwlist_1,
	                                &UnaryFunction0DDouble_Type, &obj1, &UnaryPredicate1D_Type, &obj2, &f))
	{
		if (!((BPy_UnaryFunction0DDouble *)obj1)->uf0D_double) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.recursive_split(): 1st argument: invalid UnaryFunction0DDouble object");
			return NULL;
		}
		if (!((BPy_UnaryPredicate1D *)obj2)->up1D) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.recursive_split(): 2nd argument: invalid UnaryPredicate1D object");
			return NULL;
		}
		if (Operators::recursiveSplit(*(((BPy_UnaryFunction0DDouble *)obj1)->uf0D_double),
		                              *(((BPy_UnaryPredicate1D *)obj2)->up1D),
		                              f) < 0)
		{
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.recursive_split() failed");
			return NULL;
		}
	}
	else if (PyErr_Clear(), (f = 0.0f),
	         PyArg_ParseTupleAndKeywords(args, kwds, "O!O!O!|f", (char **)kwlist_2,
	                                &UnaryFunction0DDouble_Type, &obj1, &UnaryPredicate0D_Type, &obj2,
	                                &UnaryPredicate1D_Type, &obj3, &f))
	{
		if (!((BPy_UnaryFunction0DDouble *)obj1)->uf0D_double) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.recursive_split(): 1st argument: invalid UnaryFunction0DDouble object");
			return NULL;
		}
		if (!((BPy_UnaryPredicate0D *)obj2)->up0D) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.recursive_split(): 2nd argument: invalid UnaryPredicate0D object");
			return NULL;
		}
		if (!((BPy_UnaryPredicate1D *)obj3)->up1D) {
			PyErr_SetString(PyExc_TypeError,
			                "Operators.recursive_split(): 3rd argument: invalid UnaryPredicate1D object");
			return NULL;
		}
		if (Operators::recursiveSplit(*(((BPy_UnaryFunction0DDouble *)obj1)->uf0D_double),
		                              *(((BPy_UnaryPredicate0D *)obj2)->up0D),
		                              *(((BPy_UnaryPredicate1D *)obj3)->up1D),
		                              f) < 0)
		{
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_RuntimeError, "Operators.recursive_split() failed");
			return NULL;
		}
	}
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Operators_sort_doc,
".. staticmethod:: sort(pred)\n"
"\n"
"   Sorts the current set of chains (or viewedges) according to the\n"
"   comparison predicate given as argument.\n"
"\n"
"   :arg pred: The binary predicate used for the comparison.\n"
"   :type pred: :class:`BinaryPredicate1D`");

static PyObject *Operators_sort(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"pred", NULL};
	PyObject *obj = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &BinaryPredicate1D_Type, &obj))
		return NULL;
	if (!((BPy_BinaryPredicate1D *)obj)->bp1D) {
		PyErr_SetString(PyExc_TypeError, "Operators.sort(): 1st argument: invalid BinaryPredicate1D object");
		return NULL;
	}
	if (Operators::sort(*(((BPy_BinaryPredicate1D *)obj)->bp1D)) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.sort() failed");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Operators_create_doc,
".. staticmethod:: create(pred, shaders)\n"
"\n"
"   Creates and shades the strokes from the current set of chains.  A\n"
"   predicate can be specified to make a selection pass on the chains.\n"
"\n"
"   :arg pred: The predicate that a chain must verify in order to be\n"
"      transform as a stroke.\n"
"   :type pred: :class:`UnaryPredicate1D`\n"
"   :arg shaders: The list of shaders used to shade the strokes.\n"
"   :type shaders: list of :class:`StrokeShader` objects");

static PyObject *Operators_create(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"pred", "shaders", NULL};
	PyObject *obj1 = 0, *obj2 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!O!", (char **)kwlist,
	                                 &UnaryPredicate1D_Type, &obj1, &PyList_Type, &obj2))
	{
		return NULL;
	}
	if (!((BPy_UnaryPredicate1D *)obj1)->up1D) {
		PyErr_SetString(PyExc_TypeError, "Operators.create(): 1st argument: invalid UnaryPredicate1D object");
		return NULL;
	}
	vector<StrokeShader *> shaders;
	for (int i = 0; i < PyList_Size(obj2); i++) {
		PyObject *py_ss = PyList_GetItem(obj2, i);
		if (!BPy_StrokeShader_Check(py_ss)) {
			PyErr_SetString(PyExc_TypeError, "Operators.create(): 2nd argument must be a list of StrokeShader objects");
			return NULL;
		}
		shaders.push_back(((BPy_StrokeShader *)py_ss)->ss);
	}
	if (Operators::create(*(((BPy_UnaryPredicate1D *)obj1)->up1D), shaders) < 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_RuntimeError, "Operators.create() failed");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Operators_reset_doc,
".. staticmethod:: reset(delete_strokes=True)\n"
"\n"
"   Resets the stroke selection (and therefore chaining, splitting, sorting and shading)\n"
"\n"
"   :arg delete_strokes: Delete the strokes that are currently stored\n"
"   :type delete_strokes: bool\n");

static PyObject *Operators_reset(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"delete_strokes", NULL};
	PyObject *obj1 = 0;
	if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &PyBool_Type, &obj1)) {
		// true is the default
		Operators::reset(obj1 ? bool_from_PyBool(obj1) : true);
	}
	else {
		PyErr_SetString(PyExc_RuntimeError, "Operators.reset() failed");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Operators_get_viewedge_from_index_doc,
".. staticmethod:: get_viewedge_from_index(i)\n"
"\n"
"   Returns the ViewEdge at the index in the current set of ViewEdges.\n"
"\n"
"   :arg i: index (0 <= i < Operators.get_view_edges_size()).\n"
"   :type i: int\n"
"   :return: The ViewEdge object.\n"
"   :rtype: :class:`ViewEdge`");

static PyObject *Operators_get_viewedge_from_index(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"i", NULL};
	unsigned int i;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I", (char **)kwlist, &i))
		return NULL;
	if (i >= Operators::getViewEdgesSize()) {
		PyErr_SetString(PyExc_IndexError, "index out of range");
		return NULL;
	}
	return BPy_ViewEdge_from_ViewEdge(*(Operators::getViewEdgeFromIndex(i)));
}

PyDoc_STRVAR(Operators_get_chain_from_index_doc,
".. staticmethod:: get_chain_from_index(i)\n"
"\n"
"   Returns the Chain at the index in the current set of Chains.\n"
"\n"
"   :arg i: index (0 <= i < Operators.get_chains_size()).\n"
"   :type i: int\n"
"   :return: The Chain object.\n"
"   :rtype: :class:`Chain`");

static PyObject *Operators_get_chain_from_index(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"i", NULL};
	unsigned int i;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I", (char **)kwlist, &i))
		return NULL;
	if (i >= Operators::getChainsSize()) {
		PyErr_SetString(PyExc_IndexError, "index out of range");
		return NULL;
	}
	return BPy_Chain_from_Chain(*(Operators::getChainFromIndex(i)));
}

PyDoc_STRVAR(Operators_get_stroke_from_index_doc,
".. staticmethod:: get_stroke_from_index(i)\n"
"\n"
"   Returns the Stroke at the index in the current set of Strokes.\n"
"\n"
"   :arg i: index (0 <= i < Operators.get_strokes_size()).\n"
"   :type i: int\n"
"   :return: The Stroke object.\n"
"   :rtype: :class:`Stroke`");

static PyObject *Operators_get_stroke_from_index(BPy_Operators *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"i", NULL};
	unsigned int i;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I", (char **)kwlist, &i))
		return NULL;
	if (i >= Operators::getStrokesSize()) {
		PyErr_SetString(PyExc_IndexError, "index out of range");
		return NULL;
	}
	return BPy_Stroke_from_Stroke(*(Operators::getStrokeFromIndex(i)));
}

PyDoc_STRVAR(Operators_get_view_edges_size_doc,
".. staticmethod:: get_view_edges_size()\n"
"\n"
"   Returns the number of ViewEdges.\n"
"\n"
"   :return: The number of ViewEdges.\n"
"   :rtype: int");

static PyObject *Operators_get_view_edges_size(BPy_Operators *self)
{
	return PyLong_FromLong(Operators::getViewEdgesSize());
}

PyDoc_STRVAR(Operators_get_chains_size_doc,
".. staticmethod:: get_chains_size()\n"
"\n"
"   Returns the number of Chains.\n"
"\n"
"   :return: The number of Chains.\n"
"   :rtype: int");

static PyObject *Operators_get_chains_size(BPy_Operators *self)
{
	return PyLong_FromLong(Operators::getChainsSize());
}

PyDoc_STRVAR(Operators_get_strokes_size_doc,
".. staticmethod:: get_strokes_size()\n"
"\n"
"   Returns the number of Strokes.\n"
"\n"
"   :return: The number of Strokes.\n"
"   :rtype: int");

static PyObject *Operators_get_strokes_size(BPy_Operators *self)
{
	return PyLong_FromLong(Operators::getStrokesSize());
}

/*----------------------Operators instance definitions ----------------------------*/
static PyMethodDef BPy_Operators_methods[] = {
	{"select", (PyCFunction) Operators_select, METH_VARARGS | METH_KEYWORDS | METH_STATIC, Operators_select_doc},
	{"chain", (PyCFunction) Operators_chain, METH_VARARGS | METH_KEYWORDS | METH_STATIC, Operators_chain_doc},
	{"bidirectional_chain", (PyCFunction) Operators_bidirectional_chain, METH_VARARGS | METH_KEYWORDS | METH_STATIC,
	                        Operators_bidirectional_chain_doc},
	{"sequential_split", (PyCFunction) Operators_sequential_split, METH_VARARGS | METH_KEYWORDS | METH_STATIC,
	                     Operators_sequential_split_doc},
	{"recursive_split", (PyCFunction) Operators_recursive_split, METH_VARARGS | METH_KEYWORDS | METH_STATIC,
	                    Operators_recursive_split_doc},
	{"sort", (PyCFunction) Operators_sort, METH_VARARGS | METH_KEYWORDS | METH_STATIC, Operators_sort_doc},
	{"create", (PyCFunction) Operators_create, METH_VARARGS | METH_KEYWORDS | METH_STATIC, Operators_create_doc},
	{"reset", (PyCFunction) Operators_reset, METH_VARARGS | METH_KEYWORDS | METH_STATIC, Operators_reset_doc},
	{"get_viewedge_from_index", (PyCFunction) Operators_get_viewedge_from_index,
	                            METH_VARARGS | METH_KEYWORDS | METH_STATIC, Operators_get_viewedge_from_index_doc},
	{"get_chain_from_index", (PyCFunction) Operators_get_chain_from_index, METH_VARARGS | METH_KEYWORDS | METH_STATIC,
	                         Operators_get_chain_from_index_doc},
	{"get_stroke_from_index", (PyCFunction) Operators_get_stroke_from_index, METH_VARARGS | METH_KEYWORDS | METH_STATIC,
	                          Operators_get_stroke_from_index_doc},
	{"get_view_edges_size", (PyCFunction) Operators_get_view_edges_size, METH_NOARGS | METH_STATIC,
	                        Operators_get_view_edges_size_doc},
	{"get_chains_size", (PyCFunction) Operators_get_chains_size, METH_NOARGS | METH_STATIC,
	                    Operators_get_chains_size_doc},
	{"get_strokes_size", (PyCFunction) Operators_get_strokes_size, METH_NOARGS | METH_STATIC,
	                     Operators_get_strokes_size_doc},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Operators type definition ------------------------------*/

PyTypeObject Operators_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Operators",                    /* tp_name */
	sizeof(BPy_Operators),          /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)Operators_dealloc,  /* tp_dealloc */
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
	Operators_doc,                  /* tp_doc */
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
