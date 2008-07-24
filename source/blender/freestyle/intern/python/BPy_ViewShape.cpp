#include "BPy_ViewShape.h"

#include "BPy_Convert.h"
#include "Interface0D/BPy_ViewVertex.h"
#include "Interface1D/BPy_ViewEdge.h"
#include "BPy_SShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ViewShape instance  -----------*/
static int ViewShape___init__(BPy_ViewShape *self, PyObject *args, PyObject *kwds);
static void ViewShape___dealloc__(BPy_ViewShape *self);
static PyObject * ViewShape___repr__(BPy_ViewShape* self);


/*---------------------- BPy_ViewShape instance definitions ----------------------------*/
static PyMethodDef BPy_ViewShape_methods[] = {
	//{"AddEdge", ( PyCFunction ) ViewShape_AddEdge, METH_VARARGS, "（FEdge fe ）Adds a FEdge to the list of FEdges. "},

	
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewShape type definition ------------------------------*/

PyTypeObject ViewShape_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"ViewShape",						/* tp_name */
	sizeof( BPy_ViewShape ),			/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)ViewShape___dealloc__,	/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */
	(reprfunc)ViewShape___repr__,				/* tp_repr */

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
	BPy_ViewShape_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)ViewShape___init__,           /* initproc tp_init; */
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
PyMODINIT_FUNC ViewShape_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &ViewShape_Type ) < 0 )
		return;

	Py_INCREF( &ViewShape_Type );
	PyModule_AddObject(module, "ViewShape", (PyObject *)&ViewShape_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

int ViewShape___init__(BPy_ViewShape *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

    if (! PyArg_ParseTuple(args, "|O", &obj) )
        return -1;

	if( !obj ) {
		self->vs = new ViewShape();
	
	} else if( BPy_ViewShape_Check(obj) ) {
		self->vs = new ViewShape( ((BPy_SShape *) obj)->ss );
	
	} else if( BPy_ViewShape_Check(obj) ) {
		self->vs = new ViewShape(*( ((BPy_ViewShape *) obj)->vs ));
	}
	
    return 0;
}

void ViewShape___dealloc__(BPy_ViewShape *self)
{
	delete self->vs;
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * ViewShape___repr__(BPy_ViewShape *self)
{
    return PyString_FromFormat("ViewShape - address: %p", self->vs );
}

PyObject * ViewShape_sshape( BPy_ViewShape *self ) {
	SShape ss(*( self->vs->sshape() ));
	return BPy_SShape_from_SShape( ss );
}


PyObject * ViewShape_vertices( BPy_ViewShape *self ) {
	PyObject *py_vertices = PyList_New(NULL);

	vector< ViewVertex * > vertices = self->vs->vertices();
	vector< ViewVertex * >::iterator it;
	
	for( it = vertices.begin(); it != vertices.end(); it++ ) {
		PyList_Append( py_vertices, BPy_ViewVertex_from_ViewVertex_ptr( *it ) );
	}
	
	return py_vertices;
}


PyObject * ViewShape_edges( BPy_ViewShape *self ) {
	PyObject *py_edges = PyList_New(NULL);

	vector< ViewEdge * > edges = self->vs->edges();
	vector< ViewEdge * >::iterator it;
	
	for( it = edges.begin(); it != edges.end(); it++ ) {
		PyList_Append( py_edges, BPy_ViewEdge_from_ViewEdge(*( *it )) );
	}
	
	return py_edges;
}

PyObject * ViewShape_getId( BPy_ViewShape *self ) {
	Id id( self->vs->getId() );
	return BPy_Id_from_Id( id );
}

PyObject * ViewShape_setSShape( BPy_ViewShape *self , PyObject *args) {
	PyObject *py_ss = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_ss) && BPy_SShape_Check(py_ss) )) {
		cout << "ERROR: ViewShape_SetSShape" << endl;
		Py_RETURN_NONE;
	}
	
	self->vs->setSShape( ((BPy_SShape *) py_ss)->ss );

	Py_RETURN_NONE;
}

PyObject * ViewShape_setVertices( BPy_ViewShape *self , PyObject *args) {
	PyObject *list = 0;
	PyObject *tmp;
	
	if(!( PyArg_ParseTuple(args, "O", &list) && PyList_Check(list) )) {
		cout << "ERROR: ViewShape_SetVertices" << endl;
		Py_RETURN_NONE;
	}
	
	vector< ViewVertex *> v;
	
	for( int i=0; i < PyList_Size(list); i++ ) {
		tmp = PyList_GetItem(list, i);
		if( BPy_ViewVertex_Check(tmp) )
			v.push_back( ((BPy_ViewVertex *) tmp)->vv );
		else
			Py_RETURN_NONE;
	}
		
	self->vs->setVertices( v );

	Py_RETURN_NONE;
}

//void 	SetEdges (const vector< ViewEdge * > &iEdges)
PyObject * ViewShape_setEdges( BPy_ViewShape *self , PyObject *args) {
	PyObject *list = 0;
	PyObject *tmp;

	if(!( PyArg_ParseTuple(args, "O", &list) && PyList_Check(list) )) {
		cout << "ERROR: ViewShape_SetVertices" << endl;
		Py_RETURN_NONE;
	}

	vector<ViewEdge *> v;

	for( int i=0; i < PyList_Size(list); i++ ) {
		tmp = PyList_GetItem(list, i);
		if( BPy_ViewEdge_Check(tmp) )
			v.push_back( ((BPy_ViewEdge *) tmp)->ve );
		else
			Py_RETURN_NONE;
	}

	self->vs->setEdges( v );

	Py_RETURN_NONE;
}

PyObject * ViewShape_AddEdge( BPy_ViewShape *self , PyObject *args) {
	PyObject *py_ve = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_ve) && BPy_ViewEdge_Check(py_ve) )) {
		cout << "ERROR: ViewShape_AddEdge" << endl;
		Py_RETURN_NONE;
	}
	
	self->vs->AddEdge( ((BPy_ViewEdge *) py_ve)->ve );

	Py_RETURN_NONE;
}

PyObject * ViewShape_AddVertex( BPy_ViewShape *self , PyObject *args) {
	PyObject *py_vv = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_vv) && BPy_ViewVertex_Check(py_vv) )) {
		cout << "ERROR: ViewShape_AddNewVertex" << endl;
		Py_RETURN_NONE;
	}
	
	self->vs->AddVertex( ((BPy_ViewVertex *) py_vv)->vv );

	Py_RETURN_NONE;
}

// virtual ViewShape * 	dupplicate ()

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif