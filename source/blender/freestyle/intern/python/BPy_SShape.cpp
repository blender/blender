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
	{"AddEdge", ( PyCFunction ) SShape_AddEdge, METH_VARARGS, "（FEdge fe ）Adds a FEdge to the list of FEdges. "},
	{"AddNewVertex", ( PyCFunction ) SShape_AddNewVertex, METH_VARARGS, "（SVertex sv ）Adds a SVertex to the list of SVertex of this Shape. The SShape attribute of the SVertex is also set to 'this'."},
	{"setBBox", ( PyCFunction ) SShape_setBBox, METH_VARARGS, "（BBox bb ）Sets the Bounding Box of the Shape"},
	{"ComputeBBox", ( PyCFunction ) SShape_ComputeBBox, METH_NOARGS, "（ ）Compute the bbox of the SShape"},
	{"bbox", ( PyCFunction ) SShape_bbox, METH_NOARGS, "（ ）Returns the bounding box of the shape."},
	{"getVertexList", ( PyCFunction ) SShape_getVertexList, METH_NOARGS, "（ ）Returns the list of SVertex of the Shape"},
	{"getEdgeList", ( PyCFunction ) SShape_getEdgeList, METH_NOARGS, "（ ）Returns the list of FEdges of the Shape."},
	{"getId", ( PyCFunction ) SShape_getId, METH_NOARGS, "（ ）Returns the Id of the Shape. "},
	{"setId", ( PyCFunction ) SShape_setId, METH_VARARGS, "（Id id ）Sets the Id of the shape. "},
	
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_SShape type definition ------------------------------*/

PyTypeObject SShape_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"SShape",						/* tp_name */
	sizeof( BPy_SShape ),			/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)SShape___dealloc__,	/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */
	(reprfunc)SShape___repr__,				/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, 		/* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                   /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_SShape_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)SShape___init__,           /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,						/* newfunc tp_new; */
	
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

//-------------------MODULE INITIALIZATION--------------------------------
PyMODINIT_FUNC SShape_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &SShape_Type ) < 0 )
		return;

	Py_INCREF( &SShape_Type );
	PyModule_AddObject(module, "SShape", (PyObject *)&SShape_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

int SShape___init__(BPy_SShape *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

    if (! PyArg_ParseTuple(args, "|O", &obj) )
        return -1;

	if( !obj ) {
		self->ss = new SShape();
	
	} else if( BPy_SShape_Check(obj) ) {
		self->ss = new SShape(*( ((BPy_SShape *) obj)->ss ));
	}
	
    return 0;
}

void SShape___dealloc__(BPy_SShape *self)
{
	delete self->ss;
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * SShape___repr__(BPy_SShape *self)
{
    return PyString_FromFormat("SShape - address: %p", self->ss );
}

PyObject * SShape_AddEdge( BPy_SShape *self , PyObject *args) {
	PyObject *py_fe = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_fe) && BPy_FEdge_Check(py_fe) )) {
		cout << "ERROR: SShape_AddEdge" << endl;
		Py_RETURN_NONE;
	}
	
	self->ss->AddEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject * SShape_AddNewVertex( BPy_SShape *self , PyObject *args) {
	PyObject *py_sv = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv) )) {
		cout << "ERROR: SShape_AddNewVertex" << endl;
		Py_RETURN_NONE;
	}
	
	self->ss->AddNewVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject * SShape_setBBox( BPy_SShape *self , PyObject *args) {
	PyObject *py_bb = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_bb) && BPy_BBox_Check(py_bb) )) {
		cout << "ERROR: SShape_SetBBox" << endl;
		Py_RETURN_NONE;
	}
	
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
	PyObject *py_vertices = PyList_New(NULL);

	vector< SVertex * > vertices = self->ss->getVertexList();
	vector< SVertex * >::iterator it;
	
	for( it = vertices.begin(); it != vertices.end(); it++ ) {
		PyList_Append( py_vertices, BPy_SVertex_from_SVertex(*( *it )) );
	}
	
	return py_vertices;
}


PyObject * SShape_getEdgeList( BPy_SShape *self ) {
	PyObject *py_edges = PyList_New(NULL);

	vector< FEdge * > edges = self->ss->getEdgeList();
	vector< FEdge * >::iterator it;
	
	for( it = edges.begin(); it != edges.end(); it++ ) {
		PyList_Append( py_edges, BPy_FEdge_from_FEdge(*( *it )) );
	}
	
	return py_edges;
}

PyObject * SShape_getId( BPy_SShape *self ) {
	Id id( self->ss->getId() );
	return BPy_Id_from_Id( id );
}

PyObject * SShape_setId( BPy_SShape *self , PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O", &py_id) && BPy_Id_Check(py_id) )) {
		cout << "ERROR: SShape_setId" << endl;
		Py_RETURN_NONE;
	}

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