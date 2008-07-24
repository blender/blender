#include "BPy_ViewEdge.h"

#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ViewEdge instance  -----------*/
static int ViewEdge___init__(BPy_ViewEdge *self, PyObject *args, PyObject *kwds);

static PyObject * ViewEdge_A( BPy_ViewEdge *self );
static PyObject * ViewEdge_B( BPy_ViewEdge *self );


/*----------------------ViewEdge instance definitions ----------------------------*/
static PyMethodDef BPy_ViewEdge_methods[] = {	

	{"A", ( PyCFunction ) ViewEdge_A, METH_NOARGS, "() Returns the first ViewVertex."},
	{"B", ( PyCFunction ) ViewEdge_B, METH_NOARGS, "() Returns the second ViewVertex."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewEdge type definition ------------------------------*/

PyTypeObject ViewEdge_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"ViewEdge",				/* tp_name */
	sizeof( BPy_ViewEdge ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	NULL,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	NULL,					/* tp_repr */

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
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_ViewEdge_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Interface1D_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)ViewEdge___init__,                       	/* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	NULL,		/* newfunc tp_new; */
	
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

//------------------------INSTANCE METHODS ----------------------------------
	
int ViewEdge___init__(BPy_ViewEdge *self, PyObject *args, PyObject *kwds)
{
	self->ve = new ViewEdge();
	self->py_if1D.if1D = self->ve;

	return 0;
}


PyObject * ViewEdge_A( BPy_ViewEdge *self ) {	
	// if( self->ve->A() ){
	// 	return BPy_ViewVertex_from_ViewVertex_ptr( self->ve->A() );
	// }
	// 	
	Py_RETURN_NONE;
}

PyObject * ViewEdge_B( BPy_ViewEdge *self ) {	
	// if( self->ve->B() ){
	// 	return BPy_ViewVertex_from_ViewVertex_ptr( self->ve->B() );
	// }
		
	Py_RETURN_NONE;
}

// 
// PyObject * ViewEdge___getitem__( BPy_ViewEdge *self, PyObject *args ) {
// 	int i;
// 
// 	if(!( PyArg_ParseTuple(args, "i", &i) && (i == 0 || i == 1)  )) {
// 		cout << "ERROR: ViewEdge___getitem__" << endl;
// 		Py_RETURN_NONE;
// 	}
// 	
// 	if( SVertex *v = self->ve->operator[](i) )
// 		return BPy_SVertex_from_SVertex( *v );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject * ViewEdge_nextEdge( BPy_ViewEdge *self ) {
// 	if( ViewEdge *fe = self->ve->nextEdge() )
// 		return BPy_ViewEdge_from_ViewEdge( *fe );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject * ViewEdge_previousEdge( BPy_ViewEdge *self ) {
// 	if( ViewEdge *fe = self->ve->previousEdge() )
// 		return BPy_ViewEdge_from_ViewEdge( *fe );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject * ViewEdge_isSmooth( BPy_ViewEdge *self ) {
// 	return PyBool_from_bool( self->ve->isSmooth() );
// }
// 	
// PyObject *ViewEdge_setVertexA( BPy_ViewEdge *self , PyObject *args) {
// 	PyObject *py_sv;
// 
// 	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv) )) {
// 		cout << "ERROR: ViewEdge_setVertexA" << endl;
// 		Py_RETURN_NONE;
// 	}
// 
// 	self->ve->setVertexA( ((BPy_SVertex *) py_sv)->sv );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject *ViewEdge_setVertexB( BPy_ViewEdge *self , PyObject *args) {
// 	PyObject *py_sv;
// 
// 	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv) )) {
// 		cout << "ERROR: ViewEdge_setVertexB" << endl;
// 		Py_RETURN_NONE;
// 	}
// 
// 	self->ve->setVertexB( ((BPy_SVertex *) py_sv)->sv );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject *ViewEdge_setId( BPy_ViewEdge *self , PyObject *args) {
// 	PyObject *py_id;
// 
// 	if(!( PyArg_ParseTuple(args, "O", &py_id) && BPy_Id_Check(py_id) )) {
// 		cout << "ERROR: ViewEdge_setId" << endl;
// 		Py_RETURN_NONE;
// 	}
// 
// 	self->ve->setId(*( ((BPy_Id *) py_id)->id ));
// 
// 	Py_RETURN_NONE;
// }
// 
// 
// PyObject *ViewEdge_setNextEdge( BPy_ViewEdge *self , PyObject *args) {
// 	PyObject *py_fe;
// 
// 	if(!( PyArg_ParseTuple(args, "O", &py_fe) && BPy_ViewEdge_Check(py_fe) )) {
// 		cout << "ERROR: ViewEdge_setNextEdge" << endl;
// 		Py_RETURN_NONE;
// 	}
// 
// 	self->ve->setNextEdge( ((BPy_ViewEdge *) py_fe)->ve );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject *ViewEdge_setPreviousEdge( BPy_ViewEdge *self , PyObject *args) {
// 	PyObject *py_fe;
// 
// 	if(!( PyArg_ParseTuple(args, "O", &py_fe) && BPy_ViewEdge_Check(py_fe) )) {
// 		cout << "ERROR: ViewEdge_setPreviousEdge" << endl;
// 		Py_RETURN_NONE;
// 	}
// 
// 	self->ve->setPreviousEdge( ((BPy_ViewEdge *) py_fe)->ve );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject *ViewEdge_setSmooth( BPy_ViewEdge *self , PyObject *args) {
// 	int b;
// 
// 	if(!( PyArg_ParseTuple(args, "i", &b) )) {
// 		cout << "ERROR: ViewEdge_setSmooth" << endl;
// 		Py_RETURN_NONE;
// 	}
// 
// 	self->ve->setSmooth( (bool) b );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject * ViewEdge_setNature( BPy_ViewEdge *self, PyObject *args ) {
// 	PyObject *py_n;
// 
// 	if(!( PyArg_ParseTuple(args, "O", &py_n) && BPy_Nature_Check(py_n) )) {
// 		cout << "ERROR: ViewEdge_setNature" << endl;
// 		Py_RETURN_NONE;
// 	}
// 	
// 	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
// 	((ViewEdge *) self->py_if1D.if1D)->setNature( PyInt_AsLong(i) );
// 
// 	Py_RETURN_NONE;
// }
// 
// PyObject *ViewEdge_getVertices( BPy_ViewEdge *self ) {
// 	PyObject *py_vertices = PyList_New(NULL);
// 		
// 	for( Interface0DIterator it = self->ve->verticesBegin(); it != self->ve->verticesEnd(); it++ ) {
// 		PyList_Append( py_vertices, BPy_Interface0D_from_Interface0D( *it ) );
// 	}
// 	
// 	return py_vertices;
// }
// 
// PyObject *ViewEdge_getPoints( BPy_ViewEdge *self ) {
// 	PyObject *py_points = PyList_New(NULL);
// 	
// 	for( Interface0DIterator it = self->ve->pointsBegin(); it != self->ve->pointsEnd(); it++ ) {
// 		PyList_Append( py_points, BPy_Interface0D_from_Interface0D( *it ) );
// 	}
// 	
// 	return py_points;
// }
// 
// 
// 
// 
// 
// FEdge * 	fedgeA ()
// FEdge * 	fedgeB ()
// ViewShape * 	viewShape ()
// ViewShape * 	aShape ()
// bool 	isClosed ()
// unsigned 	getChainingTimeStamp ()
// void 	SetA (ViewVertex *iA)
// void 	SetB (ViewVertex *iB)
// void 	SetNature (Nature::EdgeNature iNature)
// void 	SetFEdgeA (FEdge *iFEdge)
// void 	SetFEdgeB (FEdge *iFEdge)
// void 	SetShape (ViewShape *iVShape)
// void 	SetId (const Id &id)
// void 	UpdateFEdges ()
// void 	SetaShape (ViewShape *iShape)
// void 	SetQI (int qi)
// void 	setChainingTimeStamp (unsigned ts)
// real 	getLength2D () const
// virtual Interface0DIterator 	verticesBegin ()
// virtual Interface0DIterator 	verticesEnd ()
// virtual Interface0DIterator 	pointsBegin (float t=0.f)
// virtual Interface0DIterator 	pointsEnd (float t=0.f)



///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
