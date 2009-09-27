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

/*---------------  Python API function prototypes for FEdge instance  -----------*/
static int FEdge___init__(BPy_FEdge *self, PyObject *args, PyObject *kwds);
static PyObject * FEdge___copy__( BPy_FEdge *self );
static PyObject * FEdge_vertexA( BPy_FEdge *self );
static PyObject * FEdge_vertexB( BPy_FEdge *self );
static PyObject * FEdge___getitem__( BPy_FEdge *self, PyObject *args );
static PyObject * FEdge_nextEdge( BPy_FEdge *self );
static PyObject * FEdge_previousEdge( BPy_FEdge *self );
static PyObject * FEdge_viewedge( BPy_FEdge *self );
static PyObject * FEdge_isSmooth( BPy_FEdge *self );
static PyObject * FEdge_setVertexA( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setVertexB( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setId( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setNextEdge( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setPreviousEdge( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setSmooth( BPy_FEdge *self , PyObject *args); 
static PyObject * FEdge_setNature( BPy_FEdge *self, PyObject *args );
static PyObject * FEdge_setViewEdge( BPy_FEdge *self, PyObject *args );
static PyObject * FEdge_verticesBegin( BPy_FEdge *self );
static PyObject * FEdge_verticesEnd( BPy_FEdge *self );
static PyObject * FEdge_pointsBegin( BPy_FEdge *self, PyObject *args );
static PyObject * FEdge_pointsEnd( BPy_FEdge *self, PyObject *args );

/*----------------------FEdge instance definitions ----------------------------*/
static PyMethodDef BPy_FEdge_methods[] = {	
	{"__copy__", ( PyCFunction ) FEdge___copy__, METH_NOARGS, "() Cloning method."},
	{"vertexA", ( PyCFunction ) FEdge_vertexA, METH_NOARGS, "() Returns the first SVertex."},
	{"vertexB", ( PyCFunction ) FEdge_vertexB, METH_NOARGS, "() Returns the second SVertex."},
	{"__getitem__", ( PyCFunction ) FEdge___getitem__, METH_VARARGS, "(int i) Returns the first SVertex if i=0, the seccond SVertex if i=1."},
	{"nextEdge", ( PyCFunction ) FEdge_nextEdge, METH_NOARGS, "() Returns the FEdge following this one in the ViewEdge. If this FEdge is the last of the ViewEdge, 0 is returned."},
	{"previousEdge", ( PyCFunction ) FEdge_previousEdge, METH_NOARGS, "Returns the Edge preceding this one in the ViewEdge. If this FEdge is the first one of the ViewEdge, 0 is returned."},
	{"viewedge", ( PyCFunction ) FEdge_viewedge, METH_NOARGS, "Returns a pointer to the ViewEdge to which this FEdge belongs to."},
	{"isSmooth", ( PyCFunction ) FEdge_isSmooth, METH_NOARGS, "() Returns true if this FEdge is a smooth FEdge."},
	{"setVertexA", ( PyCFunction ) FEdge_setVertexA, METH_VARARGS, "(SVertex v) Sets the first SVertex. ."},
	{"setVertexB", ( PyCFunction ) FEdge_setVertexB, METH_VARARGS, "(SVertex v) Sets the second SVertex. "},
	{"setId", ( PyCFunction ) FEdge_setId, METH_VARARGS, "(Id id) Sets the FEdge Id ."},
	{"setNextEdge", ( PyCFunction ) FEdge_setNextEdge, METH_VARARGS, "(FEdge e) Sets the pointer to the next FEdge. "},
	{"setPreviousEdge", ( PyCFunction ) FEdge_setPreviousEdge, METH_VARARGS, "(FEdge e) Sets the pointer to the previous FEdge. "},
	{"setSmooth", ( PyCFunction ) FEdge_setSmooth, METH_VARARGS, "(bool b) Sets the flag telling whether this FEdge is smooth or sharp. true for Smooth, false for Sharp. "},
	{"setViewEdge", ( PyCFunction ) FEdge_setViewEdge, METH_VARARGS, "(ViewEdge ve) Sets the ViewEdge to which this FEdge belongs to."},
	{"setNature", ( PyCFunction ) FEdge_setNature, METH_VARARGS, "(Nature n) Sets the nature of this FEdge. "},
	{"verticesBegin", ( PyCFunction ) FEdge_verticesBegin, METH_NOARGS, "() Returns an iterator over the 2 (!) SVertex pointing to the first SVertex."},
	{"verticesEnd", ( PyCFunction ) FEdge_verticesEnd, METH_NOARGS, "() Returns an iterator over the 2 (!) SVertex pointing after the last SVertex. "},
	{"pointsBegin", ( PyCFunction ) FEdge_pointsBegin, METH_VARARGS, "(float t=0) Returns an iterator over the FEdge points, pointing to the first point. The difference with verticesBegin() is that here we can iterate over points of the FEdge at a any given sampling t. Indeed, for each iteration, a virtual point is created."},
	{"pointsEnd", ( PyCFunction ) FEdge_pointsEnd, METH_VARARGS, "(float t=0) Returns an iterator over the FEdge points, pointing after the last point. The difference with verticesEnd() is that here we can iterate over points of the FEdge at a any given sampling t. Indeed, for each iteration, a virtual point is created."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FEdge type definition ------------------------------*/

PyTypeObject FEdge_Type = {
	PyObject_HEAD_INIT(NULL)
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
	"FEdge objects",                /* tp_doc */
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

//------------------------INSTANCE METHODS ----------------------------------
	
int FEdge___init__(BPy_FEdge *self, PyObject *args, PyObject *kwds)
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


PyObject * FEdge___copy__( BPy_FEdge *self ) {
	BPy_FEdge *py_fe;
	
	py_fe = (BPy_FEdge *) FEdge_Type.tp_new( &FEdge_Type, 0, 0 );
	
	py_fe->fe = new FEdge( *(self->fe) );
	py_fe->py_if1D.if1D = py_fe->fe;
	py_fe->py_if1D.borrowed = 0;

	return (PyObject *) py_fe;
}

PyObject * FEdge_vertexA( BPy_FEdge *self ) {	
	SVertex *A = self->fe->vertexA();
	if( A ){
		return BPy_SVertex_from_SVertex( *A );
	}
		
	Py_RETURN_NONE;
}

PyObject * FEdge_vertexB( BPy_FEdge *self ) {
	SVertex *B = self->fe->vertexB();
	if( B ){
		return BPy_SVertex_from_SVertex( *B );
	}
		
	Py_RETURN_NONE;
}


PyObject * FEdge___getitem__( BPy_FEdge *self, PyObject *args ) {
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

PyObject * FEdge_nextEdge( BPy_FEdge *self ) {
	FEdge *fe = self->fe->nextEdge();
	if( fe )
		return Any_BPy_FEdge_from_FEdge( *fe );

	Py_RETURN_NONE;
}

PyObject * FEdge_previousEdge( BPy_FEdge *self ) {
	FEdge *fe = self->fe->previousEdge();
	if( fe )
		return Any_BPy_FEdge_from_FEdge( *fe );

	Py_RETURN_NONE;
}

PyObject * FEdge_viewedge( BPy_FEdge *self ) {
	ViewEdge *ve = self->fe->viewedge();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}

PyObject * FEdge_isSmooth( BPy_FEdge *self ) {
	return PyBool_from_bool( self->fe->isSmooth() );
}
	
PyObject *FEdge_setVertexA( BPy_FEdge *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->fe->setVertexA( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject *FEdge_setVertexB( BPy_FEdge *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv) )) {
		cout << "ERROR: FEdge_setVertexB" << endl;
		Py_RETURN_NONE;
	}

	self->fe->setVertexB( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject *FEdge_setId( BPy_FEdge *self , PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	self->fe->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}


PyObject *FEdge_setNextEdge( BPy_FEdge *self , PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	self->fe->setNextEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject *FEdge_setPreviousEdge( BPy_FEdge *self , PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	self->fe->setPreviousEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject * FEdge_setNature( BPy_FEdge *self, PyObject *args ) {
	PyObject *py_n;

	if(!( PyArg_ParseTuple(args, "O!", &Nature_Type, &py_n) ))
		return NULL;
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	self->fe->setNature( PyLong_AsLong(i) );

	Py_RETURN_NONE;
}


PyObject * FEdge_setViewEdge( BPy_FEdge *self, PyObject *args ) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;

	ViewEdge *ve = ((BPy_ViewEdge *) py_ve)->ve;
	self->fe->setViewEdge( ve );

	Py_RETURN_NONE;
}



PyObject *FEdge_setSmooth( BPy_FEdge *self , PyObject *args) {
	PyObject *py_b;

	if(!( PyArg_ParseTuple(args, "O", &py_b) ))
		return NULL;

	self->fe->setSmooth( bool_from_PyBool(py_b) );

	Py_RETURN_NONE;
}


PyObject * FEdge_verticesBegin( BPy_FEdge *self ) {
	Interface0DIterator if0D_it( self->fe->verticesBegin() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

PyObject * FEdge_verticesEnd( BPy_FEdge *self ) {
	Interface0DIterator if0D_it( self->fe->verticesEnd() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}


PyObject * FEdge_pointsBegin( BPy_FEdge *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->fe->pointsBegin(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

PyObject * FEdge_pointsEnd( BPy_FEdge *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->fe->pointsEnd(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}
///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
