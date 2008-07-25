#include "BPy_ViewEdge.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_ViewVertex.h"
#include "../Interface1D/BPy_FEdge.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_Nature.h"
#include "../BPy_ViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ViewEdge instance  -----------*/
static int ViewEdge___init__(BPy_ViewEdge *self, PyObject *args, PyObject *kwds);

static PyObject * ViewEdge_A( BPy_ViewEdge *self );
static PyObject * ViewEdge_B( BPy_ViewEdge *self );
static PyObject * ViewEdge_fedgeA( BPy_ViewEdge *self ) ;
static PyObject * ViewEdge_fedgeB( BPy_ViewEdge *self ) ;
static PyObject * ViewEdge_viewShape( BPy_ViewEdge *self ) ;
static PyObject * ViewEdge_aShape( BPy_ViewEdge *self ) ;
static PyObject * ViewEdge_isClosed( BPy_ViewEdge *self );
static PyObject * ViewEdge_getChainingTimeStamp( BPy_ViewEdge *self );
static PyObject * ViewEdge_setChainingTimeStamp( BPy_ViewEdge *self, PyObject *args) ;
static PyObject * ViewEdge_setA( BPy_ViewEdge *self , PyObject *args) ;
static PyObject * ViewEdge_setB( BPy_ViewEdge *self , PyObject *args);
static PyObject * ViewEdge_setNature( BPy_ViewEdge *self, PyObject *args );
static PyObject * ViewEdge_setFEdgeA( BPy_ViewEdge *self, PyObject *args ) ;
static PyObject * ViewEdge_setFEdgeB( BPy_ViewEdge *self, PyObject *args ) ;
static PyObject * ViewEdge_setShape( BPy_ViewEdge *self, PyObject *args ) ;
static PyObject * ViewEdge_setId( BPy_ViewEdge *self, PyObject *args ) ;
static PyObject * ViewEdge_UpdateFEdges( BPy_ViewEdge *self );
static PyObject * ViewEdge_setaShape( BPy_ViewEdge *self, PyObject *args );
static PyObject * ViewEdge_setQI( BPy_ViewEdge *self, PyObject *args );
static PyObject * ViewEdge_verticesBegin( BPy_ViewEdge *self );
static PyObject * ViewEdge_verticesEnd( BPy_ViewEdge *self );
static PyObject * ViewEdge_pointsBegin( BPy_ViewEdge *self, PyObject *args );
static PyObject * ViewEdge_pointsEnd( BPy_ViewEdge *self, PyObject *args );


/*----------------------ViewEdge instance definitions ----------------------------*/
static PyMethodDef BPy_ViewEdge_methods[] = {	
	{"A", ( PyCFunction ) ViewEdge_A, METH_NOARGS, "() Returns the first ViewVertex."},
	{"B", ( PyCFunction ) ViewEdge_B, METH_NOARGS, "() Returns the second ViewVertex."},
	{"fedgeA", ( PyCFunction ) ViewEdge_fedgeA, METH_NOARGS, "() Returns the first FEdge that constitues this ViewEdge."},
	{"fedgeB", ( PyCFunction ) ViewEdge_fedgeB, METH_NOARGS, "() Returns the last FEdge that constitues this ViewEdge."},
	{"viewShape", ( PyCFunction ) ViewEdge_viewShape, METH_NOARGS, "() Returns the ViewShape to which this ViewEdge belongs to . "},
	{"aShape", ( PyCFunction ) ViewEdge_aShape, METH_NOARGS, "() Returns the shape that is occluded by the ViewShape to which this ViewEdge belongs to. If no object is occluded, 0 is returned."},
	{"isClosed", ( PyCFunction ) ViewEdge_isClosed, METH_NOARGS, "() Tells whether this ViewEdge forms a closed loop or not."},
	{"getChainingTimeStamp", ( PyCFunction ) ViewEdge_getChainingTimeStamp, METH_NOARGS, "() Returns the time stamp of this ViewEdge."},
	{"setChainingTimeStamp", ( PyCFunction ) ViewEdge_setChainingTimeStamp, METH_VARARGS, "(int ts) Sets the time stamp value."},
	{"setA", ( PyCFunction ) ViewEdge_setA, METH_VARARGS, "(ViewVertex sv) Sets the first ViewVertex of the ViewEdge."},
	{"setB", ( PyCFunction ) ViewEdge_setB, METH_VARARGS, "(ViewVertex sv) Sets the last ViewVertex of the ViewEdge."},
	{"setNature", ( PyCFunction ) ViewEdge_setNature, METH_VARARGS, "(Nature n) Sets the nature of the ViewEdge."},
	{"setFEdgeA", ( PyCFunction ) ViewEdge_setFEdgeA, METH_VARARGS, "(FEdge fe) Sets the first FEdge of the ViewEdge."},
	{"setFEdgeB", ( PyCFunction ) ViewEdge_setFEdgeB, METH_VARARGS, "(FEdge fe) Sets the last FEdge of the ViewEdge."},
	{"setShape", ( PyCFunction ) ViewEdge_setShape, METH_VARARGS, "(ViewShape vs) Sets the ViewShape to which this ViewEdge belongs to."},
	{"setId", ( PyCFunction ) ViewEdge_setId, METH_VARARGS, "(Id id) Sets the ViewEdge id."},
	{"UpdateFEdges", ( PyCFunction ) ViewEdge_UpdateFEdges, METH_NOARGS, "() Sets ViewEdge to this for all embedded fedges"},
	{"setaShape", ( PyCFunction ) ViewEdge_setaShape, METH_VARARGS, "(ViewShape vs) Sets the occluded ViewShape"},
	{"setQI", ( PyCFunction ) ViewEdge_setQI, METH_VARARGS, "(int qi) Sets the quantitative invisibility value."},
	{"verticesBegin", ( PyCFunction ) ViewEdge_verticesBegin, METH_NOARGS, "() Returns an Interface0DIterator to iterate over the SVertex constituing the embedding of this ViewEdge. The returned Interface0DIterator points to the first SVertex of the ViewEdge."},
	{"verticesEnd", ( PyCFunction ) ViewEdge_verticesEnd, METH_NOARGS, "() Returns an Interface0DIterator to iterate over the SVertex constituing the embedding of this ViewEdge. The returned Interface0DIterator points after the last SVertex of the ViewEdge."},
	{"pointsBegin", ( PyCFunction ) ViewEdge_pointsBegin, METH_VARARGS, "(float t=0) Returns an Interface0DIterator to iterate over the points of this ViewEdge at a given resolution t. The returned Interface0DIterator points on the first Point of the ViewEdge."},
	{"pointsEnd", ( PyCFunction ) ViewEdge_pointsEnd, METH_VARARGS, "(float t=0) Returns an Interface0DIterator to iterate over the points of this ViewEdge at a given resolution t. The returned Interface0DIterator points after the last Point of the ViewEdge."},
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
	if( self->ve->A() ){
		return BPy_ViewVertex_from_ViewVertex_ptr( self->ve->A() );
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_B( BPy_ViewEdge *self ) {	
	if( self->ve->B() ){
		return BPy_ViewVertex_from_ViewVertex_ptr( self->ve->B() );
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_fedgeA( BPy_ViewEdge *self ) {	
	if( self->ve->fedgeA() ){
		return BPy_FEdge_from_FEdge(*( self->ve->fedgeA() ));
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_fedgeB( BPy_ViewEdge *self ) {	
	if( self->ve->fedgeB() ){
		return BPy_FEdge_from_FEdge(*( self->ve->fedgeB() ));
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_viewShape( BPy_ViewEdge *self ) {	
	if( self->ve->viewShape() ){
		return BPy_ViewShape_from_ViewShape(*( self->ve->viewShape() ));
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_aShape( BPy_ViewEdge *self ) {	
	if( self->ve->aShape() ){
		return BPy_ViewShape_from_ViewShape(*( self->ve->aShape() ));
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_isClosed( BPy_ViewEdge *self ) {
	return PyBool_from_bool( self->ve->isClosed() );	
}

PyObject * ViewEdge_getChainingTimeStamp( BPy_ViewEdge *self ) {
	return PyInt_FromLong( self->ve->getChainingTimeStamp() );
}

PyObject * ViewEdge_setChainingTimeStamp( BPy_ViewEdge *self, PyObject *args) {
	int timestamp = 0 ;

	if( !PyArg_ParseTuple(args, "i", &timestamp) ) {
		cout << "ERROR: ViewEdge_setChainingTimeStamp" << endl;
		Py_RETURN_NONE;
	}
	
	self->ve->setChainingTimeStamp( timestamp );

	Py_RETURN_NONE;
}

PyObject *ViewEdge_setA( BPy_ViewEdge *self , PyObject *args) {
	PyObject *py_vv;

	if(!( PyArg_ParseTuple(args, "O", &py_vv) && BPy_ViewVertex_Check(py_vv) )) {
		cout << "ERROR: ViewEdge_setA" << endl;
		Py_RETURN_NONE;
	}

	self->ve->setA( ((BPy_ViewVertex *) py_vv)->vv );

	Py_RETURN_NONE;
}

PyObject *ViewEdge_setB( BPy_ViewEdge *self , PyObject *args) {
	PyObject *py_vv;

	if(!( PyArg_ParseTuple(args, "O", &py_vv) && BPy_ViewVertex_Check(py_vv) )) {
		cout << "ERROR: ViewEdge_setB" << endl;
		Py_RETURN_NONE;
	}

	self->ve->setB( ((BPy_ViewVertex *) py_vv)->vv );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setNature( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_n;

	if(!( PyArg_ParseTuple(args, "O", &py_n) && BPy_Nature_Check(py_n) )) {
		cout << "ERROR: ViewEdge_setNature" << endl;
		Py_RETURN_NONE;
	}
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	self->ve->setNature( PyInt_AsLong(i) );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setFEdgeA( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O", &py_fe) && BPy_FEdge_Check(py_fe) )) {
		cout << "ERROR: ViewEdge_setFEdgeA" << endl;
		Py_RETURN_NONE;
	}

	self->ve->setFEdgeA( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setFEdgeB( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O", &py_fe) && BPy_FEdge_Check(py_fe) )) {
		cout << "ERROR: ViewEdge_setFEdgeB" << endl;
		Py_RETURN_NONE;
	}

	self->ve->setFEdgeB( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setShape( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_vs;

	if(!( PyArg_ParseTuple(args, "O", &py_vs) && BPy_ViewShape_Check(py_vs) )) {
		cout << "ERROR: ViewEdge_setShape" << endl;
		Py_RETURN_NONE;
	}

	self->ve->setShape( ((BPy_ViewShape *) py_vs)->vs );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setId( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O", &py_id) && BPy_Id_Check(py_id) )) {
		cout << "ERROR: ViewEdge_setId" << endl;
		Py_RETURN_NONE;
	}

	Id id(*( ((BPy_Id *) py_id)->id ));
	self->ve->setId( id );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_UpdateFEdges( BPy_ViewEdge *self ) {
	self->ve->UpdateFEdges();

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setaShape( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_vs;

	if(!( PyArg_ParseTuple(args, "O", &py_vs) && BPy_ViewShape_Check(py_vs) )) {
		cout << "ERROR: ViewEdge_setaShape" << endl;
		Py_RETURN_NONE;
	}

	ViewShape *vs = ((BPy_ViewShape *) py_vs)->vs;
	self->ve->setaShape( vs );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setQI( BPy_ViewEdge *self, PyObject *args ) {
	int qi;

	if(!( PyArg_ParseTuple(args, "i", &qi) )) {
		cout << "ERROR: ViewEdge_setQI" << endl;
		Py_RETURN_NONE;
	}

	self->ve->setQI( qi );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_verticesBegin( BPy_ViewEdge *self ) {
	Interface0DIterator if0D_it( self->ve->verticesBegin() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it );
}

PyObject * ViewEdge_verticesEnd( BPy_ViewEdge *self ) {
	Interface0DIterator if0D_it( self->ve->verticesEnd() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it );
}


PyObject * ViewEdge_pointsBegin( BPy_ViewEdge *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  )) {
		cout << "ERROR: ViewEdge_pointsBegin" << endl;
		Py_RETURN_NONE;
	}
	
	Interface0DIterator if0D_it( self->ve->pointsBegin(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it );
}

PyObject * ViewEdge_pointsEnd( BPy_ViewEdge *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  )) {
		cout << "ERROR: ViewEdge_pointsEnd" << endl;
		Py_RETURN_NONE;
	}
	
	Interface0DIterator if0D_it( self->ve->pointsEnd(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it );
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
