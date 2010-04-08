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
static PyObject * ViewEdge_qi( BPy_ViewEdge *self );


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
	{"qi", ( PyCFunction ) ViewEdge_qi, METH_NOARGS, "() Returns the quantitative invisibility of the ViewEdge."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewEdge type definition ------------------------------*/

PyTypeObject ViewEdge_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ViewEdge",                     /* tp_name */
	sizeof(BPy_ViewEdge),           /* tp_basicsize */
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
	"ViewEdge objects",             /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ViewEdge_methods,           /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Interface1D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ViewEdge___init__,    /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------
	
int ViewEdge___init__(BPy_ViewEdge *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->ve = new ViewEdge();
	self->py_if1D.if1D = self->ve;
	self->py_if1D.borrowed = 0;

	return 0;
}


PyObject * ViewEdge_A( BPy_ViewEdge *self ) {	
	ViewVertex *v = self->ve->A();
	if( v ){
		return Any_BPy_ViewVertex_from_ViewVertex( *v );
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_B( BPy_ViewEdge *self ) {	
	ViewVertex *v = self->ve->B();
	if( v ){
		return Any_BPy_ViewVertex_from_ViewVertex( *v );
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_fedgeA( BPy_ViewEdge *self ) {	
	FEdge *A = self->ve->fedgeA();
	if( A ){
		return Any_BPy_FEdge_from_FEdge( *A );
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_fedgeB( BPy_ViewEdge *self ) {	
	FEdge *B = self->ve->fedgeB();
	if( B ){
		return Any_BPy_FEdge_from_FEdge( *B );
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_viewShape( BPy_ViewEdge *self ) {	
	ViewShape *vs = self->ve->viewShape();
	if( vs ){
		return BPy_ViewShape_from_ViewShape( *vs );
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_aShape( BPy_ViewEdge *self ) {	
	ViewShape *vs = self->ve->aShape();
	if( vs ){
		return BPy_ViewShape_from_ViewShape( *vs );
	}
		
	Py_RETURN_NONE;
}

PyObject * ViewEdge_isClosed( BPy_ViewEdge *self ) {
	return PyBool_from_bool( self->ve->isClosed() );	
}

PyObject * ViewEdge_getChainingTimeStamp( BPy_ViewEdge *self ) {
	return PyLong_FromLong( self->ve->getChainingTimeStamp() );
}

PyObject * ViewEdge_setChainingTimeStamp( BPy_ViewEdge *self, PyObject *args) {
	int timestamp = 0 ;

	if( !PyArg_ParseTuple(args, "i", &timestamp) )
		return NULL;
	
	self->ve->setChainingTimeStamp( timestamp );

	Py_RETURN_NONE;
}

PyObject *ViewEdge_setA( BPy_ViewEdge *self , PyObject *args) {
	PyObject *py_vv;

	if(!( PyArg_ParseTuple(args, "O!", &ViewVertex_Type, &py_vv) ))
		return NULL;

	self->ve->setA( ((BPy_ViewVertex *) py_vv)->vv );

	Py_RETURN_NONE;
}

PyObject *ViewEdge_setB( BPy_ViewEdge *self , PyObject *args) {
	PyObject *py_vv;

	if(!( PyArg_ParseTuple(args, "O!", &ViewVertex_Type, &py_vv) ))
		return NULL;

	self->ve->setB( ((BPy_ViewVertex *) py_vv)->vv );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setNature( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_n;

	if(!( PyArg_ParseTuple(args, "O!", &Nature_Type, &py_n) ))
		return NULL;
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	self->ve->setNature( PyLong_AsLong(i) );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setFEdgeA( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	self->ve->setFEdgeA( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setFEdgeB( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	self->ve->setFEdgeB( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setShape( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_vs;

	if(!( PyArg_ParseTuple(args, "O", &ViewShape_Type, &py_vs) ))
		return NULL;

	self->ve->setShape( ((BPy_ViewShape *) py_vs)->vs );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setId( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

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

	if(!( PyArg_ParseTuple(args, "O!", &ViewShape_Type, &py_vs) ))
		return NULL;

	ViewShape *vs = ((BPy_ViewShape *) py_vs)->vs;
	self->ve->setaShape( vs );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_setQI( BPy_ViewEdge *self, PyObject *args ) {
	int qi;

	if(!( PyArg_ParseTuple(args, "i", &qi) ))
		return NULL;

	self->ve->setQI( qi );

	Py_RETURN_NONE;
}

PyObject * ViewEdge_verticesBegin( BPy_ViewEdge *self ) {
	Interface0DIterator if0D_it( self->ve->verticesBegin() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

PyObject * ViewEdge_verticesEnd( BPy_ViewEdge *self ) {
	Interface0DIterator if0D_it( self->ve->verticesEnd() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}


PyObject * ViewEdge_pointsBegin( BPy_ViewEdge *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->ve->pointsBegin(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

PyObject * ViewEdge_pointsEnd( BPy_ViewEdge *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->ve->pointsEnd(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}

PyObject * ViewEdge_qi( BPy_ViewEdge *self ) {
	return PyLong_FromLong( self->ve->qi() );
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
