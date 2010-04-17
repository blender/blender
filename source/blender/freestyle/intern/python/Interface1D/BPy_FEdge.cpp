#include "BPy_FEdge.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_SVertex.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------
	
static char FEdge___doc__[] =
"Base Class for feature edges.  This FEdge can represent a silhouette,\n"
"a crease, a ridge/valley, a border or a suggestive contour.  For\n"
"silhouettes, the FEdge is oriented so that the visible face lies on\n"
"the left of the edge.  For borders, the FEdge is oriented so that the\n"
"face lies on the left of the edge.  An FEdge can represent an initial\n"
"edge of the mesh or runs accross a face of the initial mesh depending\n"
"on the smoothness or sharpness of the mesh.  This class is specialized\n"
"into a smooth and a sharp version since their properties slightly vary\n"
"from one to the other.\n"
"\n"
".. method:: FEdge()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: FEdge(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: An FEdge object.\n"
"   :type iBrother: :class:`FEdge`\n"
"\n"
".. method:: FEdge(vA, vB)\n"
"\n"
"   Builds an FEdge going from vA to vB.\n"
"\n"
"   :arg vA: The first SVertex.\n"
"   :type vA: :class:`SVertex`\n"
"   :arg vB: The second SVertex.\n"
"   :type vB: :class:`SVertex`\n";

static int FEdge___init__(BPy_FEdge *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj1 = 0, *obj2 = 0;

    if (! PyArg_ParseTuple(args, "|OO", &obj1, &obj2) )
        return -1;

	if( !obj1 ){
		self->fe = new FEdge();

	} else if( !obj2 && BPy_FEdge_Check(obj1) ) {
		self->fe = new FEdge(*( ((BPy_FEdge *) obj1)->fe ));
		
	} else if( obj2 && BPy_SVertex_Check(obj1) && BPy_SVertex_Check(obj2) ) {
		self->fe = new FEdge( ((BPy_SVertex *) obj1)->sv, ((BPy_SVertex *) obj2)->sv );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_if1D.if1D = self->fe;
	self->py_if1D.borrowed = 0;

	return 0;
}


static PyObject * FEdge___copy__( BPy_FEdge *self ) {
	BPy_FEdge *py_fe;
	
	py_fe = (BPy_FEdge *) FEdge_Type.tp_new( &FEdge_Type, 0, 0 );
	
	py_fe->fe = new FEdge( *(self->fe) );
	py_fe->py_if1D.if1D = py_fe->fe;
	py_fe->py_if1D.borrowed = 0;

	return (PyObject *) py_fe;
}

static char FEdge_vertexA___doc__[] =
".. method:: vertexA()\n"
"\n"
"   Returns the first SVertex.\n"
"\n"
"   :return: The first SVertex.\n"
"   :rtype: :class:`SVertex`\n";

static PyObject * FEdge_vertexA( BPy_FEdge *self ) {	
	SVertex *A = self->fe->vertexA();
	if( A ){
		return BPy_SVertex_from_SVertex( *A );
	}
		
	Py_RETURN_NONE;
}

static char FEdge_vertexB___doc__[] =
".. method:: vertexB()\n"
"\n"
"   Returns the second SVertex.\n"
"\n"
"   :return: The second SVertex.\n"
"   :rtype: :class:`SVertex`\n";

static PyObject * FEdge_vertexB( BPy_FEdge *self ) {
	SVertex *B = self->fe->vertexB();
	if( B ){
		return BPy_SVertex_from_SVertex( *B );
	}
		
	Py_RETURN_NONE;
}


static PyObject * FEdge___getitem__( BPy_FEdge *self, PyObject *args ) {
	int i;

	if(!( PyArg_ParseTuple(args, "i", &i) ))
		return NULL;
	if(!(i == 0 || i == 1)) {
		PyErr_SetString(PyExc_IndexError, "index must be either 0 or 1");
		return NULL;
	}
	
	SVertex *v = self->fe->operator[](i);
	if( v )
		return BPy_SVertex_from_SVertex( *v );

	Py_RETURN_NONE;
}

static char FEdge_nextEdge___doc__[] =
".. method:: nextEdge()\n"
"\n"
"   Returns the FEdge following this one in the ViewEdge.  If this FEdge\n"
"   is the last of the ViewEdge, None is returned.\n"
"\n"
"   :return: The edge following this one in the ViewEdge.\n"
"   :rtype: :class:`FEdge`\n";

static PyObject * FEdge_nextEdge( BPy_FEdge *self ) {
	FEdge *fe = self->fe->nextEdge();
	if( fe )
		return Any_BPy_FEdge_from_FEdge( *fe );

	Py_RETURN_NONE;
}

static char FEdge_previousEdge___doc__[] =
".. method:: previousEdge()\n"
"\n"
"   Returns the FEdge preceding this one in the ViewEdge.  If this FEdge\n"
"   is the first one of the ViewEdge, None is returned.\n"
"\n"
"   :return: The edge preceding this one in the ViewEdge.\n"
"   :rtype: :class:`FEdge`\n";

static PyObject * FEdge_previousEdge( BPy_FEdge *self ) {
	FEdge *fe = self->fe->previousEdge();
	if( fe )
		return Any_BPy_FEdge_from_FEdge( *fe );

	Py_RETURN_NONE;
}

static char FEdge_viewedge___doc__[] =
".. method:: viewedge()\n"
"\n"
"   Returns the ViewEdge to which this FEdge belongs to.\n"
"\n"
"   :return: The ViewEdge to which this FEdge belongs to.\n"
"   :rtype: :class:`ViewEdge`\n";

static PyObject * FEdge_viewedge( BPy_FEdge *self ) {
	ViewEdge *ve = self->fe->viewedge();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}

static char FEdge_isSmooth___doc__[] =
".. method:: isSmooth()\n"
"\n"
"   Returns true if this FEdge is a smooth FEdge.\n"
"\n"
"   :return: True if this FEdge is a smooth FEdge.\n"
"   :rtype: bool\n";

static PyObject * FEdge_isSmooth( BPy_FEdge *self ) {
	return PyBool_from_bool( self->fe->isSmooth() );
}
	
static char FEdge_setVertexA___doc__[] =
".. method:: setVertexA(vA)\n"
"\n"
"   Sets the first SVertex.\n"
"\n"
"   :arg vA: An SVertex object.\n"
"   :type vA: :class:`SVertex`\n";

static PyObject *FEdge_setVertexA( BPy_FEdge *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->fe->setVertexA( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

static char FEdge_setVertexB___doc__[] =
".. method:: setVertexB(vB)\n"
"\n"
"   Sets the second SVertex.\n"
"\n"
"   :arg vB: An SVertex object.\n"
"   :type vB: :class:`SVertex`\n";

static PyObject *FEdge_setVertexB( BPy_FEdge *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv) )) {
		cout << "ERROR: FEdge_setVertexB" << endl;
		Py_RETURN_NONE;
	}

	self->fe->setVertexB( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

static char FEdge_setId___doc__[] =
".. method:: setId(id)\n"
"\n"
"   Sets the Id of this FEdge.\n"
"\n"
"   :arg id: An Id object.\n"
"   :type id: :class:`Id`\n";

static PyObject *FEdge_setId( BPy_FEdge *self , PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	self->fe->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}

static char FEdge_setNextEdge___doc__[] =
".. method:: setNextEdge(iEdge)\n"
"\n"
"   Sets the pointer to the next FEdge.\n"
"\n"
"   :arg iEdge: An FEdge object.\n"
"   :type iEdge: :class:`FEdge`\n";

static PyObject *FEdge_setNextEdge( BPy_FEdge *self , PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	self->fe->setNextEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

static char FEdge_setPreviousEdge___doc__[] =
".. method:: setPreviousEdge(iEdge)\n"
"\n"
"   Sets the pointer to the previous FEdge.\n"
"\n"
"   :arg iEdge: An FEdge object.\n"
"   :type iEdge: :class:`FEdge`\n";

static PyObject *FEdge_setPreviousEdge( BPy_FEdge *self , PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	self->fe->setPreviousEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

static char FEdge_setNature___doc__[] =
".. method:: setNature(iNature)\n"
"\n"
"   Sets the nature of this FEdge.\n"
"\n"
"   :arg iNature: A Nature object.\n"
"   :type iNature: :class:`Nature`\n";

static PyObject * FEdge_setNature( BPy_FEdge *self, PyObject *args ) {
	PyObject *py_n;

	if(!( PyArg_ParseTuple(args, "O!", &Nature_Type, &py_n) ))
		return NULL;
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	self->fe->setNature( PyLong_AsLong(i) );

	Py_RETURN_NONE;
}

static char FEdge_setViewEdge___doc__[] =
".. method:: setViewEdge(iViewEdge)\n"
"\n"
"   Sets the ViewEdge to which this FEdge belongs to.\n"
"\n"
"   :arg iViewEdge: A ViewEdge object.\n"
"   :type iViewEdge: :class:`ViewEdge`\n";

static PyObject * FEdge_setViewEdge( BPy_FEdge *self, PyObject *args ) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;

	ViewEdge *ve = ((BPy_ViewEdge *) py_ve)->ve;
	self->fe->setViewEdge( ve );

	Py_RETURN_NONE;
}

static char FEdge_setSmooth___doc__[] =
".. method:: setSmooth(iFlag)\n"
"\n"
"   Sets the flag telling whether this FEdge is smooth or sharp.  True\n"
"   for Smooth, false for Sharp.\n"
"\n"
"   :arg iFlag: True for Smooth, false for Sharp.\n"
"   :type iFlag: bool\n";

static PyObject *FEdge_setSmooth( BPy_FEdge *self , PyObject *args) {
	PyObject *py_b;

	if(!( PyArg_ParseTuple(args, "O", &py_b) ))
		return NULL;

	self->fe->setSmooth( bool_from_PyBool(py_b) );

	Py_RETURN_NONE;
}

/*----------------------FEdge instance definitions ----------------------------*/

static PyMethodDef BPy_FEdge_methods[] = {	
	{"__copy__", ( PyCFunction ) FEdge___copy__, METH_NOARGS, "() Cloning method."},
	{"vertexA", ( PyCFunction ) FEdge_vertexA, METH_NOARGS, FEdge_vertexA___doc__},
	{"vertexB", ( PyCFunction ) FEdge_vertexB, METH_NOARGS, FEdge_vertexB___doc__},
	{"__getitem__", ( PyCFunction ) FEdge___getitem__, METH_VARARGS, "(int i) Returns the first SVertex if i=0, the seccond SVertex if i=1."},
	{"nextEdge", ( PyCFunction ) FEdge_nextEdge, METH_NOARGS, FEdge_nextEdge___doc__},
	{"previousEdge", ( PyCFunction ) FEdge_previousEdge, METH_NOARGS, FEdge_previousEdge___doc__},
	{"viewedge", ( PyCFunction ) FEdge_viewedge, METH_NOARGS, FEdge_viewedge___doc__},
	{"isSmooth", ( PyCFunction ) FEdge_isSmooth, METH_NOARGS, FEdge_isSmooth___doc__},
	{"setVertexA", ( PyCFunction ) FEdge_setVertexA, METH_VARARGS, FEdge_setVertexA___doc__},
	{"setVertexB", ( PyCFunction ) FEdge_setVertexB, METH_VARARGS, FEdge_setVertexB___doc__},
	{"setId", ( PyCFunction ) FEdge_setId, METH_VARARGS, FEdge_setId___doc__},
	{"setNextEdge", ( PyCFunction ) FEdge_setNextEdge, METH_VARARGS, FEdge_setNextEdge___doc__},
	{"setPreviousEdge", ( PyCFunction ) FEdge_setPreviousEdge, METH_VARARGS, FEdge_setPreviousEdge___doc__},
	{"setSmooth", ( PyCFunction ) FEdge_setSmooth, METH_VARARGS, FEdge_setSmooth___doc__},
	{"setViewEdge", ( PyCFunction ) FEdge_setViewEdge, METH_VARARGS, FEdge_setViewEdge___doc__},
	{"setNature", ( PyCFunction ) FEdge_setNature, METH_VARARGS, FEdge_setNature___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FEdge type definition ------------------------------*/

PyTypeObject FEdge_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"FEdge",                        /* tp_name */
	sizeof(BPy_FEdge),              /* tp_basicsize */
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
	FEdge___doc__,                  /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_FEdge_methods,              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Interface1D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)FEdge___init__,       /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
