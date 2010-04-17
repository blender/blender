#include "BPy_ViewVertex.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ViewVertex___doc__[] =
"Class to define a view vertex.  A view vertex is a feature vertex\n"
"corresponding to a point of the image graph, where the characteristics\n"
"of an edge (e.g., nature and visibility) might change.  A\n"
":class:`ViewVertex` can be of two kinds: A :class:`TVertex` when it\n"
"corresponds to the intersection between two ViewEdges or a\n"
":class:`NonTVertex` when it corresponds to a vertex of the initial\n"
"input mesh (it is the case for vertices such as corners for example).\n"
"Thus, this class can be specialized into two classes, the\n"
":class:`TVertex` class and the :class:`NonTVertex` class.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A ViewVertex object.\n"
"   :type iBrother: :class:`ViewVertex`\n";

static int ViewVertex___init__( BPy_ViewVertex *self, PyObject *args, PyObject *kwds )
{	
    if( !PyArg_ParseTuple(args, "") )
        return -1;
	self->vv = 0; // ViewVertex is abstract
	self->py_if0D.if0D = self->vv;
	self->py_if0D.borrowed = 0;
	return 0;
}

static char ViewVertex_setNature___doc__[] =
".. method:: setNature(iNature)\n"
"\n"
"   Sets the nature of the vertex.\n"
"\n"
"   :arg iNature: A Nature object.\n"
"   :type iNature: :class:`Nature`\n";

static PyObject * ViewVertex_setNature( BPy_ViewVertex *self, PyObject *args ) {
	PyObject *py_n;

	if( !self->vv )
		Py_RETURN_NONE;
		
	if(!( PyArg_ParseTuple(args, "O!", &Nature_Type, &py_n) ))
		return NULL;
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	((ViewVertex *) self->py_if0D.if0D)->setNature( PyLong_AsLong(i) );

	Py_RETURN_NONE;
}

static char ViewVertex_edgesBegin___doc__[] =
".. method:: edgesBegin()\n"
"\n"
"   Returns an iterator over the ViewEdges that goes to or comes from\n"
"   this ViewVertex pointing to the first ViewEdge of the list. The\n"
"   orientedViewEdgeIterator allows to iterate in CCW order over these\n"
"   ViewEdges and to get the orientation for each ViewEdge\n"
"   (incoming/outgoing).\n"
"\n"
"   :return: An orientedViewEdgeIterator pointing to the first ViewEdge.\n"
"   :rtype: :class:`orientedViewEdgeIterator`\n";

static PyObject * ViewVertex_edgesBegin( BPy_ViewVertex *self ) {
	if( !self->vv )
		Py_RETURN_NONE;
		
	ViewVertexInternal::orientedViewEdgeIterator ove_it( self->vv->edgesBegin() );
	return BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator( ove_it, 0 );
}

static char ViewVertex_edgesEnd___doc__[] =
".. method:: edgesEnd()\n"
"\n"
"   Returns an orientedViewEdgeIterator over the ViewEdges around this\n"
"   ViewVertex, pointing after the last ViewEdge.\n"
"\n"
"   :return: An orientedViewEdgeIterator pointing after the last ViewEdge.\n"
"   :rtype: :class:`orientedViewEdgeIterator`\n";

static PyObject * ViewVertex_edgesEnd( BPy_ViewVertex *self ) {
#if 0
	if( !self->vv )
		Py_RETURN_NONE;
		
	ViewVertexInternal::orientedViewEdgeIterator ove_it( self->vv->edgesEnd() );
	return BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator( ove_it, 1 );
#else
	PyErr_SetString(PyExc_NotImplementedError, "edgesEnd method currently disabled");
	return NULL;
#endif
}

static char ViewVertex_edgesIterator___doc__[] =
".. method:: edgesIterator(iEdge)\n"
"\n"
"   Returns an orientedViewEdgeIterator pointing to the ViewEdge given\n"
"   as argument.\n"
"\n"
"   :arg iEdge: A ViewEdge object.\n"
"   :type iEdge: :class:`ViewEdge`\n"
"   :return: An orientedViewEdgeIterator pointing to the given ViewEdge.\n"
"   :rtype: :class:`orientedViewEdgeIterator`\n";

static PyObject * ViewVertex_edgesIterator( BPy_ViewVertex *self, PyObject *args ) {
	PyObject *py_ve;

	if( !self->vv )
		Py_RETURN_NONE;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;
	
	ViewEdge *ve = ((BPy_ViewEdge *) py_ve)->ve;
	ViewVertexInternal::orientedViewEdgeIterator ove_it( self->vv->edgesIterator( ve ) );
	return BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator( ove_it, 0 );
}

/*----------------------ViewVertex instance definitions ----------------------------*/
static PyMethodDef BPy_ViewVertex_methods[] = {
	{"setNature", ( PyCFunction ) ViewVertex_setNature, METH_VARARGS, ViewVertex_setNature___doc__},
	{"edgesBegin", ( PyCFunction ) ViewVertex_edgesBegin, METH_NOARGS, ViewVertex_edgesBegin___doc__},
	{"edgesEnd", ( PyCFunction ) ViewVertex_edgesEnd, METH_NOARGS, ViewVertex_edgesEnd___doc__},
	{"edgesIterator", ( PyCFunction ) ViewVertex_edgesIterator, METH_VARARGS, ViewVertex_edgesIterator___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewVertex type definition ------------------------------*/
PyTypeObject ViewVertex_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ViewVertex",                   /* tp_name */
	sizeof(BPy_ViewVertex),         /* tp_basicsize */
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
	ViewVertex___doc__,             /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ViewVertex_methods,         /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Interface0D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ViewVertex___init__,  /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

