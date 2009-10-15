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

/*---------------  Python API function prototypes for SShape instance  -----------*/
static int SShape___init__(BPy_SShape *self, PyObject *args, PyObject *kwds);
static void SShape___dealloc__(BPy_SShape *self);
static PyObject * SShape___repr__(BPy_SShape* self);

static PyObject * SShape_AddEdge( BPy_SShape *self , PyObject *args);
static PyObject * SShape_AddNewVertex( BPy_SShape *self , PyObject *args);
static PyObject * SShape_setBBox( BPy_SShape *self , PyObject *args);
static PyObject * SShape_ComputeBBox( BPy_SShape *self );
static PyObject * SShape_bbox( BPy_SShape *self );
static PyObject * SShape_getVertexList( BPy_SShape *self );
static PyObject * SShape_getEdgeList( BPy_SShape *self );
static PyObject * SShape_getId( BPy_SShape *self );
static PyObject * SShape_setId( BPy_SShape *self , PyObject *args);

/*----------------------SShape instance definitions ----------------------------*/
static PyMethodDef BPy_SShape_methods[] = {
	{"AddEdge", ( PyCFunction ) SShape_AddEdge, METH_VARARGS, "(FEdge fe) Adds a FEdge to the list of FEdges. "},
	{"AddNewVertex", ( PyCFunction ) SShape_AddNewVertex, METH_VARARGS, "(SVertex sv) Adds a SVertex to the list of SVertex of this Shape. The SShape attribute of the SVertex is also set to 'this'."},
	{"setBBox", ( PyCFunction ) SShape_setBBox, METH_VARARGS, "(BBox bb) Sets the Bounding Box of the Shape"},
	{"ComputeBBox", ( PyCFunction ) SShape_ComputeBBox, METH_NOARGS, "() Compute the bbox of the SShape"},
	{"bbox", ( PyCFunction ) SShape_bbox, METH_NOARGS, "() Returns the bounding box of the shape."},
	{"getVertexList", ( PyCFunction ) SShape_getVertexList, METH_NOARGS, "() Returns the list of SVertex of the Shape"},
	{"getEdgeList", ( PyCFunction ) SShape_getEdgeList, METH_NOARGS, "() Returns the list of FEdges of the Shape."},
	{"getId", ( PyCFunction ) SShape_getId, METH_NOARGS, "() Returns the Id of the Shape. "},
	{"setId", ( PyCFunction ) SShape_setId, METH_VARARGS, "(Id id) Sets the Id of the shape. "},
	
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
	"SShape objects",               /* tp_doc */
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

int SShape___init__(BPy_SShape *self, PyObject *args, PyObject *kwds)
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

void SShape___dealloc__(BPy_SShape *self)
{
	if( self->ss && !self->borrowed )
		delete self->ss;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyObject * SShape___repr__(BPy_SShape *self)
{
    return PyUnicode_FromFormat("SShape - address: %p", self->ss );
}

PyObject * SShape_AddEdge( BPy_SShape *self , PyObject *args) {
	PyObject *py_fe = 0;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;
	
	self->ss->AddEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject * SShape_AddNewVertex( BPy_SShape *self , PyObject *args) {
	PyObject *py_sv = 0;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;
	
	self->ss->AddNewVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject * SShape_setBBox( BPy_SShape *self , PyObject *args) {
	PyObject *py_bb = 0;

	if(!( PyArg_ParseTuple(args, "O!", &BBox_Type, &py_bb) ))
		return NULL;
	
	self->ss->setBBox(*( ((BPy_BBox*) py_bb)->bb ));

	Py_RETURN_NONE;
}

PyObject * SShape_ComputeBBox( BPy_SShape *self ) {
	self->ss->ComputeBBox();

	Py_RETURN_NONE;
}

PyObject * SShape_bbox( BPy_SShape *self ) {
	BBox<Vec3r> bb( self->ss->bbox() );
	return BPy_BBox_from_BBox( bb );
}


PyObject * SShape_getVertexList( BPy_SShape *self ) {
	PyObject *py_vertices = PyList_New(0);

	vector< SVertex * > vertices = self->ss->getVertexList();
	vector< SVertex * >::iterator it;
	
	for( it = vertices.begin(); it != vertices.end(); it++ ) {
		PyList_Append( py_vertices, BPy_SVertex_from_SVertex(*( *it )) );
	}
	
	return py_vertices;
}


PyObject * SShape_getEdgeList( BPy_SShape *self ) {
	PyObject *py_edges = PyList_New(0);

	vector< FEdge * > edges = self->ss->getEdgeList();
	vector< FEdge * >::iterator it;
	
	for( it = edges.begin(); it != edges.end(); it++ ) {
		PyList_Append( py_edges, Any_BPy_FEdge_from_FEdge(*( *it )) );
	}
	
	return py_edges;
}

PyObject * SShape_getId( BPy_SShape *self ) {
	Id id( self->ss->getId() );
	return BPy_Id_from_Id( id );
}

PyObject * SShape_setId( BPy_SShape *self , PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	self->ss->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}


// const Material & 	material (unsigned i) const
// const vector< Material > & 	materials () const
// void 	SetMaterials (const vector< Material > &iMaterials)


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
