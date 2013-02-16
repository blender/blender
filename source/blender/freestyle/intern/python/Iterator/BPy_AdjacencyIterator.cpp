#include "BPy_AdjacencyIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_ViewVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(AdjacencyIterator_doc,
"Class hierarchy: :class:`Iterator` > :class:`AdjacencyIterator`\n"
"\n"
"Class for representing adjacency iterators used in the chaining\n"
"process.  An AdjacencyIterator is created in the increment() and\n"
"decrement() methods of a :class:`ChainingIterator` and passed to the\n"
"traverse() method of the ChainingIterator.\n"
"\n"
".. method:: __init__(iVertex, iRestrictToSelection=True, iRestrictToUnvisited=True)\n"
"\n"
"   Builds a AdjacencyIterator object.\n"
"\n"
"   :arg iVertex: The vertex which is the next crossing.\n"
"   :type iVertex: :class:`ViewVertex`\n"
"   :arg iRestrictToSelection: Indicates whether to force the chaining\n"
"      to stay within the set of selected ViewEdges or not.\n"
"   :type iRestrictToSelection: bool\n"
"   :arg iRestrictToUnvisited: Indicates whether a ViewEdge that has\n"
"      already been chained must be ignored ot not.\n"
"   :type iRestrictToUnvisited: bool\n"
"\n"
".. method:: __init__(it)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg it: An AdjacencyIterator object.\n"
"   :type it: :class:`AdjacencyIterator`");

static int AdjacencyIterator_init(BPy_AdjacencyIterator *self, PyObject *args)
{
	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0;

	if (!PyArg_ParseTuple(args, "|OOO", &obj1, &obj2, &obj3))
		return -1;

	if (!obj1) {
		self->a_it = new AdjacencyIterator();

	} else if (BPy_AdjacencyIterator_Check(obj1) && !obj2) {
		self->a_it = new AdjacencyIterator(*(((BPy_AdjacencyIterator *)obj1)->a_it));

	} else if (BPy_ViewVertex_Check(obj1) && (!obj2 || PyBool_Check(obj2)) && (!obj3 || PyBool_Check(obj3))) {
		bool restrictToSelection = (obj2) ? bool_from_PyBool(obj2) : true;
		bool restrictToUnvisited = (obj3) ? bool_from_PyBool(obj3) : true;
		
		self->a_it = new AdjacencyIterator(((BPy_ViewVertex *)obj1)->vv, restrictToSelection, restrictToUnvisited);

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_it.it = self->a_it;
	
	return 0;
}

static PyObject * AdjacencyIterator_iternext(BPy_AdjacencyIterator *self)
{
	if (self->a_it->isEnd()) {
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}
	ViewEdge *ve = self->a_it->operator*();
	self->a_it->increment();
	return BPy_ViewEdge_from_ViewEdge(*ve);
}

static PyMethodDef BPy_AdjacencyIterator_methods[] = {
	{NULL, NULL, 0, NULL}
};

/*----------------------AdjacencyIterator get/setters ----------------------------*/

PyDoc_STRVAR(AdjacencyIterator_object_doc,
"The ViewEdge object currently pointed by this iterator.\n"
"\n"
":type: :class:`ViewEdge`");

static PyObject *AdjacencyIterator_object_get(BPy_AdjacencyIterator *self, void *UNUSED(closure))
{
	ViewEdge *ve = self->a_it->operator*();
	if (ve)
		return BPy_ViewEdge_from_ViewEdge(*ve);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(AdjacencyIterator_is_incoming_doc,
"True if the current ViewEdge is coming towards the iteration vertex, and\n"
"False otherwise.\n"
"\n"
":type: bool");

static PyObject *AdjacencyIterator_is_incoming_get(BPy_AdjacencyIterator *self, void *UNUSED(closure))
{
	return PyBool_from_bool(self->a_it->isIncoming());
}

static PyGetSetDef BPy_AdjacencyIterator_getseters[] = {
	{(char *)"is_incoming", (getter)AdjacencyIterator_is_incoming_get, (setter)NULL, (char *)AdjacencyIterator_is_incoming_doc, NULL},
	{(char *)"object", (getter)AdjacencyIterator_object_get, (setter)NULL, (char *)AdjacencyIterator_object_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_AdjacencyIterator type definition ------------------------------*/

PyTypeObject AdjacencyIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"AdjacencyIterator",            /* tp_name */
	sizeof(BPy_AdjacencyIterator),  /* tp_basicsize */
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
	AdjacencyIterator_doc,          /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	PyObject_SelfIter,              /* tp_iter */
	(iternextfunc)AdjacencyIterator_iternext, /* tp_iternext */
	BPy_AdjacencyIterator_methods,  /* tp_methods */
	0,                              /* tp_members */
	BPy_AdjacencyIterator_getseters, /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)AdjacencyIterator_init, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
