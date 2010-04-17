#include "BPy_ViewShape.h"

#include "BPy_Convert.h"
#include "Interface0D/BPy_ViewVertex.h"
#include "Interface1D/BPy_ViewEdge.h"
#include "BPy_SShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int ViewShape_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &ViewShape_Type ) < 0 )
		return -1;

	Py_INCREF( &ViewShape_Type );
	PyModule_AddObject(module, "ViewShape", (PyObject *)&ViewShape_Type);
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char ViewShape___doc__[] =
"Class gathering the elements of the ViewMap (i.e., :class:`ViewVertex`\n"
"and :class:`ViewEdge`) that are issued from the same input shape.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A ViewShape object.\n"
"   :type iBrother: :class:`ViewShape`\n"
"\n"
".. method:: __init__(iSShape)\n"
"\n"
"   Builds a ViewShape from an SShape.\n"
"\n"
"   :arg iSShape: An SShape object.\n"
"   :type iSShape: :class:`SShape`\n";

static int ViewShape___init__(BPy_ViewShape *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

    if (! PyArg_ParseTuple(args, "|O", &obj) )
        return -1;

	if( !obj ) {
		self->vs = new ViewShape();
	
	} else if( BPy_SShape_Check(obj) ) {
		self->vs = new ViewShape( ((BPy_SShape *) obj)->ss );
	
	} else if( BPy_ViewShape_Check(obj) ) {
		self->vs = new ViewShape(*( ((BPy_ViewShape *) obj)->vs ));

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return -1;
	}
	self->borrowed = 0;
	
    return 0;
}

static void ViewShape___dealloc__(BPy_ViewShape *self)
{
	if( self->vs && !self->borrowed )
		delete self->vs;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * ViewShape___repr__(BPy_ViewShape *self)
{
    return PyUnicode_FromFormat("ViewShape - address: %p", self->vs );
}

static char ViewShape_sshape___doc__[] =
".. method:: sshape()\n"
"\n"
"   Returns the SShape on top of which this ViewShape is built.\n"
"\n"
"   :return: The SShape on top of which this ViewShape is built.\n"
"   :rtype: :class:`SShape`\n";

static PyObject * ViewShape_sshape( BPy_ViewShape *self ) {
	return BPy_SShape_from_SShape( *(self->vs->sshape()) );
}

static char ViewShape_vertices___doc__[] =
".. method:: vertices()\n"
"\n"
"   Returns the list of ViewVertex objects contained in this ViewShape.\n"
"\n"
"   :return: The list of ViewVertex objects.\n"
"   :rtype: List of :class:`ViewVertex` objects\n";

static PyObject * ViewShape_vertices( BPy_ViewShape *self ) {
	PyObject *py_vertices = PyList_New(0);

	vector< ViewVertex * > vertices = self->vs->vertices();
	vector< ViewVertex * >::iterator it;
	
	for( it = vertices.begin(); it != vertices.end(); it++ ) {
		PyList_Append( py_vertices, Any_BPy_ViewVertex_from_ViewVertex(*( *it )) );
	}
	
	return py_vertices;
}

static char ViewShape_edges___doc__[] =
".. method:: edges()\n"
"\n"
"   Returns the list of ViewEdge objects contained in this ViewShape.\n"
"\n"
"   :return: The list of ViewEdge objects.\n"
"   :rtype: List of :class:`ViewEdge` objects\n";

static PyObject * ViewShape_edges( BPy_ViewShape *self ) {
	PyObject *py_edges = PyList_New(0);

	vector< ViewEdge * > edges = self->vs->edges();
	vector< ViewEdge * >::iterator it;
	
	for( it = edges.begin(); it != edges.end(); it++ ) {
		PyList_Append( py_edges, BPy_ViewEdge_from_ViewEdge(*( *it )) );
	}
	
	return py_edges;
}

static char ViewShape_getId___doc__[] =
".. method:: getId()\n"
"\n"
"   Returns the ViewShape id.\n"
"\n"
"   :return: An Id object.\n"
"   :rtype: :class:`Id`\n";

static PyObject * ViewShape_getId( BPy_ViewShape *self ) {
	Id id( self->vs->getId() );
	return BPy_Id_from_Id( id );
}

static char ViewShape_setSShape___doc__[] =
".. method:: setSShape(iSShape)\n"
"\n"
"   Sets the SShape on top of which the ViewShape is built.\n"
"\n"
"   :arg iSShape: An SShape object.\n"
"   :type iSShape: :class:`SShape`\n";

static PyObject * ViewShape_setSShape( BPy_ViewShape *self , PyObject *args) {
	PyObject *py_ss = 0;

	if(!( PyArg_ParseTuple(args, "O!", &SShape_Type, &py_ss) ))
		return NULL;
	
	self->vs->setSShape( ((BPy_SShape *) py_ss)->ss );

	Py_RETURN_NONE;
}

static char ViewShape_setVertices___doc__[] =
".. method:: setVertices(iVertices)\n"
"\n"
"   Sets the list of ViewVertex objects contained in this ViewShape.\n"
"\n"
"   :arg iVertices: The list of ViewVertex objects.\n"
"   :type iVertices: List of :class:`ViewVertex` objects\n";

static PyObject * ViewShape_setVertices( BPy_ViewShape *self , PyObject *args) {
	PyObject *list = 0;
	PyObject *tmp;
	
	if(!( PyArg_ParseTuple(args, "O!", &PyList_Type, &list) ))
		return NULL;
	
	vector< ViewVertex *> v;
	
	for( int i=0; i < PyList_Size(list); i++ ) {
		tmp = PyList_GetItem(list, i);
		if( BPy_ViewVertex_Check(tmp) )
			v.push_back( ((BPy_ViewVertex *) tmp)->vv );
		else {
			PyErr_SetString(PyExc_TypeError, "argument must be list of ViewVertex objects");
			return NULL;
		}
	}
		
	self->vs->setVertices( v );

	Py_RETURN_NONE;
}

static char ViewShape_setEdges___doc__[] =
".. method:: setEdges(iEdges)\n"
"\n"
"   Sets the list of ViewEdge objects contained in this ViewShape.\n"
"\n"
"   :arg iEdges: The list of ViewEdge objects.\n"
"   :type iEdges: List of :class:`ViewEdge` objects.\n";

static PyObject * ViewShape_setEdges( BPy_ViewShape *self , PyObject *args) {
	PyObject *list = 0;
	PyObject *tmp;

	if(!( PyArg_ParseTuple(args, "O!", &PyList_Type, &list) ))
		return NULL;

	vector<ViewEdge *> v;

	for( int i=0; i < PyList_Size(list); i++ ) {
		tmp = PyList_GetItem(list, i);
		if( BPy_ViewEdge_Check(tmp) )
			v.push_back( ((BPy_ViewEdge *) tmp)->ve );
		else {
			PyErr_SetString(PyExc_TypeError, "argument must be list of ViewEdge objects");
			return NULL;
		}
	}

	self->vs->setEdges( v );

	Py_RETURN_NONE;
}

static char ViewShape_AddEdge___doc__[] =
".. method:: AddEdge(iEdge)\n"
"\n"
"   Adds a ViewEdge to the list of ViewEdge objects.\n"
"\n"
"   :arg iEdge: A ViewEdge object.\n"
"   :type iEdge: :class:`ViewEdge`\n";

static PyObject * ViewShape_AddEdge( BPy_ViewShape *self , PyObject *args) {
	PyObject *py_ve = 0;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;
	
	self->vs->AddEdge( ((BPy_ViewEdge *) py_ve)->ve );

	Py_RETURN_NONE;
}

static char ViewShape_AddVertex___doc__[] =
".. method:: AddVertex(iVertex)\n"
"\n"
"   Adds a ViewVertex to the list of the ViewVertex objects.\n"
"\n"
"   :arg iVertex: A ViewVertex object.\n"
"   :type iVertex: :class:`ViewVertex`\n";

static PyObject * ViewShape_AddVertex( BPy_ViewShape *self , PyObject *args) {
	PyObject *py_vv = 0;

	if(!( PyArg_ParseTuple(args, "O!", &ViewVertex_Type, &py_vv) ))
		return NULL;
	
	self->vs->AddVertex( ((BPy_ViewVertex *) py_vv)->vv );

	Py_RETURN_NONE;
}

// virtual ViewShape * 	duplicate ()

/*---------------------- BPy_ViewShape instance definitions ----------------------------*/
static PyMethodDef BPy_ViewShape_methods[] = {
	{"sshape", ( PyCFunction ) ViewShape_sshape, METH_NOARGS, ViewShape_sshape___doc__},
	{"vertices", ( PyCFunction ) ViewShape_vertices, METH_NOARGS, ViewShape_vertices___doc__},
	{"edges", ( PyCFunction ) ViewShape_edges, METH_NOARGS, ViewShape_edges___doc__},
	{"getId", ( PyCFunction ) ViewShape_getId, METH_NOARGS, ViewShape_getId___doc__},
	{"setSShape", ( PyCFunction ) ViewShape_setSShape, METH_VARARGS, ViewShape_setSShape___doc__},
	{"setVertices", ( PyCFunction ) ViewShape_setVertices, METH_VARARGS, ViewShape_setVertices___doc__},
	{"setEdges", ( PyCFunction ) ViewShape_setEdges, METH_VARARGS, ViewShape_setEdges___doc__},
	{"AddEdge", ( PyCFunction ) ViewShape_AddEdge, METH_VARARGS, ViewShape_AddEdge___doc__},
	{"AddVertex", ( PyCFunction ) ViewShape_AddVertex, METH_VARARGS, ViewShape_AddVertex___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewShape type definition ------------------------------*/

PyTypeObject ViewShape_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ViewShape",                    /* tp_name */
	sizeof(BPy_ViewShape),          /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)ViewShape___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)ViewShape___repr__,   /* tp_repr */
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
	ViewShape___doc__,              /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ViewShape_methods,          /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ViewShape___init__,   /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
