#include "BPy_ViewVertex.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ViewVertex instance  -----------*/
static int ViewVertex___init__( BPy_ViewVertex *self, PyObject *args, PyObject *kwds );
static PyObject * ViewVertex_setNature( BPy_ViewVertex *self, PyObject *args );
static PyObject * ViewVertex_edgesBegin( BPy_ViewVertex *self );
static PyObject * ViewVertex_edgesEnd( BPy_ViewVertex *self );
static PyObject * ViewVertex_edgesIterator( BPy_ViewVertex *self, PyObject *args );

/*----------------------ViewVertex instance definitions ----------------------------*/
static PyMethodDef BPy_ViewVertex_methods[] = {
	{"setNature", ( PyCFunction ) ViewVertex_setNature, METH_VARARGS, "(Nature n) Sets the nature of the vertex."},
	{"edgesBegin", ( PyCFunction ) ViewVertex_edgesBegin, METH_NOARGS, "() Returns an iterator over the ViewEdges that goes to or comes from this ViewVertex pointing to the first ViewEdge of the list. The orientedViewEdgeIterator allows to iterate in CCW order over these ViewEdges and to get the orientation for each ViewEdge (incoming/outgoing). "},
	{"edgesEnd", ( PyCFunction ) ViewVertex_edgesEnd, METH_NOARGS, "() Returns an orientedViewEdgeIterator over the ViewEdges around this ViewVertex, pointing after the last ViewEdge."},
	{"edgesIterator", ( PyCFunction ) ViewVertex_edgesIterator, METH_VARARGS, "(ViewEdge ve) Returns an orientedViewEdgeIterator pointing to the ViewEdge given as argument. "},
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
	"ViewVertex objects",           /* tp_doc */
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

//------------------------INSTANCE METHODS ----------------------------------

int ViewVertex___init__( BPy_ViewVertex *self, PyObject *args, PyObject *kwds )
{	
    if( !PyArg_ParseTuple(args, "") )
        return -1;
	self->vv = 0; // ViewVertex is abstract
	self->py_if0D.if0D = self->vv;
	self->py_if0D.borrowed = 0;
	return 0;
}



PyObject * ViewVertex_setNature( BPy_ViewVertex *self, PyObject *args ) {
	PyObject *py_n;

	if( !self->vv )
		Py_RETURN_NONE;
		
	if(!( PyArg_ParseTuple(args, "O!", &Nature_Type, &py_n) ))
		return NULL;
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	((ViewVertex *) self->py_if0D.if0D)->setNature( PyLong_AsLong(i) );

	Py_RETURN_NONE;
}

PyObject * ViewVertex_edgesBegin( BPy_ViewVertex *self ) {
	if( !self->vv )
		Py_RETURN_NONE;
		
	ViewVertexInternal::orientedViewEdgeIterator ove_it( self->vv->edgesBegin() );
	return BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator( ove_it, 0 );
}

PyObject * ViewVertex_edgesEnd( BPy_ViewVertex *self ) {
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


PyObject * ViewVertex_edgesIterator( BPy_ViewVertex *self, PyObject *args ) {
	PyObject *py_ve;

	if( !self->vv )
		Py_RETURN_NONE;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;
	
	ViewEdge *ve = ((BPy_ViewEdge *) py_ve)->ve;
	ViewVertexInternal::orientedViewEdgeIterator ove_it( self->vv->edgesIterator( ve ) );
	return BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator( ove_it, 0 );
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

