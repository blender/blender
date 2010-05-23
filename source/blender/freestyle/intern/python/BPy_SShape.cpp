#include "BPy_SShape.h"

#include "BPy_Convert.h"
#include "BPy_BBox.h"
#include "BPy_Id.h"
#include "Interface0D/BPy_SVertex.h"
#include "Interface1D/BPy_FEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int SShape_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &SShape_Type ) < 0 )
		return -1;

	Py_INCREF( &SShape_Type );
	PyModule_AddObject(module, "SShape", (PyObject *)&SShape_Type);
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char SShape___doc__[] =
"Class to define a feature shape.  It is the gathering of feature\n"
"elements from an identified input shape.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: An SShape object.\n"
"   :type iBrother: :class:`SShape`\n";

static int SShape___init__(BPy_SShape *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj = NULL;

    if (! PyArg_ParseTuple(args, "|O!", &SShape_Type, &obj) )
        return -1;

	if( !obj ) {
		self->ss = new SShape();
	
	} else {
		self->ss = new SShape(*( ((BPy_SShape *) obj)->ss ));
	}
	self->borrowed = 0;

    return 0;
}

static void SShape___dealloc__(BPy_SShape *self)
{
	if( self->ss && !self->borrowed )
		delete self->ss;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * SShape___repr__(BPy_SShape *self)
{
    return PyUnicode_FromFormat("SShape - address: %p", self->ss );
}

static char SShape_AddEdge___doc__[] =
".. method:: AddEdge(iEdge)\n"
"\n"
"   Adds an FEdge to the list of FEdges.\n"
"\n"
"   :arg iEdge: An FEdge object.\n"
"   :type iEdge: :class:`FEdge`\n";

static PyObject * SShape_AddEdge( BPy_SShape *self , PyObject *args) {
	PyObject *py_fe = 0;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;
	
	self->ss->AddEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

static char SShape_AddNewVertex___doc__[] =
".. method:: AddNewVertex(iv)\n"
"\n"
"   Adds an SVertex to the list of SVertex of this Shape.  The SShape\n"
"   attribute of the SVertex is also set to this SShape.\n"
"\n"
"   :arg iv: An SVertex object.\n"
"   :type iv: :class:`SVertex`\n";

static PyObject * SShape_AddNewVertex( BPy_SShape *self , PyObject *args) {
	PyObject *py_sv = 0;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;
	
	self->ss->AddNewVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

static char SShape_setBBox___doc__[] =
".. method:: setBBox(iBBox)\n"
"\n"
"   Sets the bounding box of the SShape.\n"
"\n"
"   :arg iBBox: The bounding box of the SShape.\n"
"   :type iBBox: :class:`BBox`\n";

static PyObject * SShape_setBBox( BPy_SShape *self , PyObject *args) {
	PyObject *py_bb = 0;

	if(!( PyArg_ParseTuple(args, "O!", &BBox_Type, &py_bb) ))
		return NULL;
	
	self->ss->setBBox(*( ((BPy_BBox*) py_bb)->bb ));

	Py_RETURN_NONE;
}

static char SShape_ComputeBBox___doc__[] =
".. method:: ComputeBBox()\n"
"\n"
"   Compute the bbox of the SShape.\n";

static PyObject * SShape_ComputeBBox( BPy_SShape *self ) {
	self->ss->ComputeBBox();

	Py_RETURN_NONE;
}

static char SShape_bbox___doc__[] =
".. method:: bbox()\n"
"\n"
"   Returns the bounding box of the SShape.\n"
"\n"
"   :return: the bounding box of the SShape.\n"
"   :rtype: :class:`BBox`\n";

static PyObject * SShape_bbox( BPy_SShape *self ) {
	BBox<Vec3r> bb( self->ss->bbox() );
	return BPy_BBox_from_BBox( bb );
}

static char SShape_getVertexList___doc__[] =
".. method:: getVertexList()\n"
"\n"
"   Returns the list of vertices of the SShape.\n"
"\n"
"   :return: The list of vertices objects.\n"
"   :rtype: List of :class:`SVertex` objects\n";

static PyObject * SShape_getVertexList( BPy_SShape *self ) {
	PyObject *py_vertices = PyList_New(0);

	vector< SVertex * > vertices = self->ss->getVertexList();
	vector< SVertex * >::iterator it;
	
	for( it = vertices.begin(); it != vertices.end(); it++ ) {
		PyList_Append( py_vertices, BPy_SVertex_from_SVertex(*( *it )) );
	}
	
	return py_vertices;
}

static char SShape_getEdgeList___doc__[] =
".. method:: getEdgeList()\n"
"\n"
"   Returns the list of edges of the SShape.\n"
"\n"
"   :return: The list of edges of the SShape.\n"
"   :rtype: List of :class:`FEdge` objects\n";

static PyObject * SShape_getEdgeList( BPy_SShape *self ) {
	PyObject *py_edges = PyList_New(0);

	vector< FEdge * > edges = self->ss->getEdgeList();
	vector< FEdge * >::iterator it;
	
	for( it = edges.begin(); it != edges.end(); it++ ) {
		PyList_Append( py_edges, Any_BPy_FEdge_from_FEdge(*( *it )) );
	}
	
	return py_edges;
}

static char SShape_getId___doc__[] =
".. method:: getId()\n"
"\n"
"   Returns the Id of the SShape.\n"
"\n"
"   :return: The Id of the SShape.\n"
"   :rtype: :class:`Id`\n";

static PyObject * SShape_getId( BPy_SShape *self ) {
	Id id( self->ss->getId() );
	return BPy_Id_from_Id( id );
}

static char SShape_setId___doc__[] =
".. method:: setId(id)\n"
"\n"
"   Sets the Id of the SShape.\n"
"\n"
"   :arg id: The Id of the SShape.\n"
"   :type id: :class:`Id`\n";

static PyObject * SShape_setId( BPy_SShape *self , PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	self->ss->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}

static char SShape_getName___doc__[] =
".. method:: getName()\n"
"\n"
"   Returns the name of the SShape.\n"
"\n"
"   :return: The name string.\n"
"   :rtype: str\n";

static PyObject * SShape_getName( BPy_SShape *self ) {
	return PyUnicode_FromString( self->ss->getName().c_str() );
}

static char SShape_setName___doc__[] =
".. method:: setName(name)\n"
"\n"
"   Sets the name of the SShape.\n"
"\n"
"   :arg name: A name string.\n"
"   :type name: str\n";

static PyObject * SShape_setName( BPy_SShape *self , PyObject *args) {
	char *s;

	if(!( PyArg_ParseTuple(args, "s", &s) ))
		return NULL;

	self->ss->setName(s);

	Py_RETURN_NONE;
}

// const Material & 	material (unsigned i) const
// const vector< Material > & 	materials () const
// void 	SetMaterials (const vector< Material > &iMaterials)

/*----------------------SShape instance definitions ----------------------------*/
static PyMethodDef BPy_SShape_methods[] = {
	{"AddEdge", ( PyCFunction ) SShape_AddEdge, METH_VARARGS, SShape_AddEdge___doc__},
	{"AddNewVertex", ( PyCFunction ) SShape_AddNewVertex, METH_VARARGS, SShape_AddNewVertex___doc__},
	{"setBBox", ( PyCFunction ) SShape_setBBox, METH_VARARGS, SShape_setBBox___doc__},
	{"ComputeBBox", ( PyCFunction ) SShape_ComputeBBox, METH_NOARGS, SShape_ComputeBBox___doc__},
	{"bbox", ( PyCFunction ) SShape_bbox, METH_NOARGS, SShape_bbox___doc__},
	{"getVertexList", ( PyCFunction ) SShape_getVertexList, METH_NOARGS, SShape_getVertexList___doc__},
	{"getEdgeList", ( PyCFunction ) SShape_getEdgeList, METH_NOARGS, SShape_getEdgeList___doc__},
	{"getId", ( PyCFunction ) SShape_getId, METH_NOARGS, SShape_getId___doc__},
	{"setId", ( PyCFunction ) SShape_setId, METH_VARARGS, SShape_setId___doc__},
	{"getName", ( PyCFunction ) SShape_getName, METH_NOARGS, SShape_getName___doc__},
	{"setName", ( PyCFunction ) SShape_setName, METH_VARARGS, SShape_setName___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_SShape type definition ------------------------------*/

PyTypeObject SShape_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SShape",                       /* tp_name */
	sizeof(BPy_SShape),             /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)SShape___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)SShape___repr__,      /* tp_repr */
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
	SShape___doc__,                 /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_SShape_methods,             /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)SShape___init__,      /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
