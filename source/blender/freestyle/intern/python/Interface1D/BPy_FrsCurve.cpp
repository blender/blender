#include "BPy_FrsCurve.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_CurvePoint.h"
#include "../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char FrsCurve___doc__[] =
"Base class for curves made of CurvePoints.  :class:`SVertex` is the\n"
"type of the initial curve vertices.  A :class:`Chain` is a\n"
"specialization of a Curve.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default Constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy Constructor.\n"
"\n"
"   :arg iBrother: A Curve object.\n"
"   :type iBrother: :class:`Curve`\n"
"\n"
".. method:: __init__(iId)\n"
"\n"
"   Builds a Curve from its Id.\n"
"\n"
"   :arg iId: An Id object.\n"
"   :type iId: :class:`Id`\n";

static int FrsCurve___init__(BPy_FrsCurve *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj = 0;

    if (! PyArg_ParseTuple(args, "|O", &obj) )
        return -1;

	if( !obj ){
		self->c = new Curve();
		
	} else if( BPy_FrsCurve_Check(obj) ) {
		self->c = new Curve(*( ((BPy_FrsCurve *) obj)->c ));
		
	} else if( BPy_Id_Check(obj) ) {
		self->c = new Curve(*( ((BPy_Id *) obj)->id ));
			
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return -1;
	}

	self->py_if1D.if1D = self->c;
	self->py_if1D.borrowed = 0;

	return 0;
}

static char FrsCurve_push_vertex_back___doc__[] =
".. method:: push_vertex_back(iVertex)\n"
"\n"
"   Adds a single vertex at the end of the Curve.\n"
"\n"
"   :arg iVertex: A vertex object.\n"
"   :type iVertex: :class:`SVertex` or :class:`CurvePoint`\n";

static PyObject * FrsCurve_push_vertex_back( BPy_FrsCurve *self, PyObject *args ) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) ))
		return NULL;

	if( BPy_CurvePoint_Check(obj) ) {
		self->c->push_vertex_back( ((BPy_CurvePoint *) obj)->cp );
	} else if( BPy_SVertex_Check(obj) ) {
		self->c->push_vertex_back( ((BPy_SVertex *) obj)->sv );
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return NULL;
	}

	Py_RETURN_NONE;
}

static char FrsCurve_push_vertex_front___doc__[] =
".. method:: push_vertex_front(iVertex)\n"
"\n"
"   Adds a single vertex at the front of the Curve.\n"
"\n"
"   :arg iVertex: A vertex object.\n"
"   :type iVertex: :class:`SVertex` or :class:`CurvePoint`\n";

static PyObject * FrsCurve_push_vertex_front( BPy_FrsCurve *self, PyObject *args ) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) ))
		return NULL;

	if( BPy_CurvePoint_Check(obj) ) {
		self->c->push_vertex_front( ((BPy_CurvePoint *) obj)->cp );
	} else if( BPy_SVertex_Check(obj) ) {
		self->c->push_vertex_front( ((BPy_SVertex *) obj)->sv );
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return NULL;
	}

	Py_RETURN_NONE;
}

static char FrsCurve_empty___doc__[] =
".. method:: empty()\n"
"\n"
"   Returns true if the Curve doesn't have any Vertex yet.\n"
"\n"
"   :return: True if the Curve has no vertices.\n"
"   :rtype: bool\n";

static PyObject * FrsCurve_empty( BPy_FrsCurve *self ) {
	return PyBool_from_bool( self->c->empty() );
}

static char FrsCurve_nSegments___doc__[] =
".. method:: nSegments()\n"
"\n"
"   Returns the number of segments in the polyline constituing the\n"
"   Curve.\n"
"\n"
"   :return: The number of segments.\n"
"   :rtype: int\n";

static PyObject * FrsCurve_nSegments( BPy_FrsCurve *self ) {
	return PyLong_FromLong( self->c->nSegments() );
}

/*----------------------FrsCurve instance definitions ----------------------------*/
static PyMethodDef BPy_FrsCurve_methods[] = {	
	{"push_vertex_back", ( PyCFunction ) FrsCurve_push_vertex_back, METH_VARARGS, FrsCurve_push_vertex_back___doc__},
	{"push_vertex_front", ( PyCFunction ) FrsCurve_push_vertex_front, METH_VARARGS, FrsCurve_push_vertex_front___doc__},
	{"empty", ( PyCFunction ) FrsCurve_empty, METH_NOARGS, FrsCurve_empty___doc__},
	{"nSegments", ( PyCFunction ) FrsCurve_nSegments, METH_NOARGS, FrsCurve_nSegments___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FrsCurve type definition ------------------------------*/

PyTypeObject FrsCurve_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Curve",                        /* tp_name */
	sizeof(BPy_FrsCurve),           /* tp_basicsize */
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
	FrsCurve___doc__,               /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_FrsCurve_methods,           /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Interface1D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)FrsCurve___init__,    /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
