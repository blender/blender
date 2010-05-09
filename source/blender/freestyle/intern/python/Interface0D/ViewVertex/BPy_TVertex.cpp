#include "BPy_TVertex.h"

#include "../../BPy_Convert.h"
#include "../BPy_SVertex.h"
#include "../../BPy_Id.h"
#include "../../Interface1D/BPy_FEdge.h"
#include "../../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char TVertex___doc__[] =
"Class to define a T vertex, i.e. an intersection between two edges.\n"
"It points towards two SVertex and four ViewEdges.  Among the\n"
"ViewEdges, two are front and the other two are back.  Basically a\n"
"front edge hides part of a back edge.  So, among the back edges, one\n"
"is of invisibility N and the other of invisibility N+1.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A TVertex object.\n"
"   :type iBrother: :class:`TVertex`\n";

static int TVertex___init__(BPy_TVertex *self, PyObject *args, PyObject *kwds)
{
    if( !PyArg_ParseTuple(args, "") )
        return -1;
	self->tv = new TVertex();
	self->py_vv.vv = self->tv;
	self->py_vv.py_if0D.if0D = self->tv;
	self->py_vv.py_if0D.borrowed = 0;

	return 0;
}

static char TVertex_frontSVertex___doc__[] =
".. method:: frontSVertex()\n"
"\n"
"   Returns the SVertex that is closer to the viewpoint.\n"
"\n"
"   :return: The SVertex that is closer to the viewpoint.\n"
"   :rtype: :class:`SVertex`\n";

static PyObject * TVertex_frontSVertex( BPy_TVertex *self ) {
	SVertex *v = self->tv->frontSVertex();
	if( v ){
		return BPy_SVertex_from_SVertex( *v );
	}

	Py_RETURN_NONE;
}

static char TVertex_backSVertex___doc__[] =
".. method:: backSVertex()\n"
"\n"
"   Returns the SVertex that is further away from the viewpoint.\n"
"\n"
"   :return: The SVertex that is further away from the viewpoint.\n"
"   :rtype: :class:`SVertex`\n";

static PyObject * TVertex_backSVertex( BPy_TVertex *self ) {
	SVertex *v = self->tv->backSVertex();
	if( v ){
		return BPy_SVertex_from_SVertex( *v );
	}

	Py_RETURN_NONE;
}

static char TVertex_setFrontSVertex___doc__[] =
".. method:: setFrontSVertex(iFrontSVertex)\n"
"\n"
"   Sets the SVertex that is closer to the viewpoint.\n"
"\n"
"   :arg iFrontSVertex: An SVertex object.\n"
"   :type iFrontSVertex: :class:`SVertex`\n";

static PyObject * TVertex_setFrontSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->tv->setFrontSVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

static char TVertex_setBackSVertex___doc__[] =
".. method:: setBackSVertex(iBackSVertex)\n"
"\n"
"   Sets the SVertex that is further away from the viewpoint.\n"
"\n"
"   :arg iBackSVertex: An SVertex object.\n"
"   :type iBackSVertex: :class:`SVertex`\n";

static PyObject * TVertex_setBackSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &SVertex_Type, &py_sv) ))
		return NULL;

	self->tv->setBackSVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

static char TVertex_setId___doc__[] =
".. method:: setId(iId)\n"
"\n"
"   Sets the Id.\n"
"\n"
"   :arg iId: An Id object.\n"
"   :type iId: :class:`Id`\n";

static PyObject * TVertex_setId( BPy_TVertex *self, PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	if( ((BPy_Id *) py_id)->id )
		self->tv->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}

static char TVertex_getSVertex___doc__[] =
".. method:: getSVertex(iFEdge)\n"
"\n"
"   Returns the SVertex (among the 2) belonging to the given FEdge.\n"
"\n"
"   :arg iFEdge: An FEdge object.\n"
"   :type iFEdge: :class:`FEdge`\n"
"   :return: The SVertex belonging to the given FEdge.\n"
"   :rtype: :class:`SVertex`\n";

static PyObject * TVertex_getSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	SVertex *sv = self->tv->getSVertex( ((BPy_FEdge *) py_fe)->fe );
	if( sv ){
		return BPy_SVertex_from_SVertex( *sv );
	}

	Py_RETURN_NONE;
}

static char TVertex_mate___doc__[] =
".. method:: mate(iEdgeA)\n"
"\n"
"   Returns the mate edge of the ViewEdge given as argument.  If the\n"
"   ViewEdge is frontEdgeA, frontEdgeB is returned.  If the ViewEdge is\n"
"   frontEdgeB, frontEdgeA is returned.  Same for back edges.\n"
"\n"
"   :arg iEdgeA: A ViewEdge object.\n"
"   :type iEdgeA: :class:`ViewEdge`\n"
"   :return: The mate edge of the given ViewEdge.\n"
"   :rtype: :class:`ViewEdge`\n";

static PyObject * TVertex_mate( BPy_TVertex *self, PyObject *args) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;

	ViewEdge *ve = self->tv->mate( ((BPy_ViewEdge *) py_ve)->ve );
	if( ve ){
		return BPy_ViewEdge_from_ViewEdge( *ve );
	}

	Py_RETURN_NONE;
}

/*----------------------TVertex instance definitions ----------------------------*/
static PyMethodDef BPy_TVertex_methods[] = {	
	{"frontSVertex", ( PyCFunction ) TVertex_frontSVertex, METH_NOARGS, TVertex_frontSVertex___doc__},
	{"backSVertex", ( PyCFunction ) TVertex_backSVertex, METH_NOARGS, TVertex_backSVertex___doc__},
	{"setFrontSVertex", ( PyCFunction ) TVertex_setFrontSVertex, METH_VARARGS, TVertex_setFrontSVertex___doc__},
	{"setBackSVertex", ( PyCFunction ) TVertex_setBackSVertex, METH_VARARGS, TVertex_setBackSVertex___doc__},
	{"setId", ( PyCFunction ) TVertex_setId, METH_VARARGS, TVertex_setId___doc__},
	{"getSVertex", ( PyCFunction ) TVertex_getSVertex, METH_VARARGS, TVertex_getSVertex___doc__},
	{"mate", ( PyCFunction ) TVertex_mate, METH_VARARGS, TVertex_mate___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_TVertex type definition ------------------------------*/
PyTypeObject TVertex_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"TVertex",                      /* tp_name */
	sizeof(BPy_TVertex),            /* tp_basicsize */
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
	TVertex___doc__,                /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_TVertex_methods,            /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&ViewVertex_Type,               /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)TVertex___init__,     /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
