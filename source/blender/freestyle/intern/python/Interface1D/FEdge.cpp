#include "FEdge.h"

#include "../Convert.h"
#include "../Interface0D/SVertex.h"

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
static PyObject * FEdge_getVertices( BPy_FEdge *self );
static PyObject * FEdge_getPoints( BPy_FEdge *self );
static PyObject * FEdge_isSmooth( BPy_FEdge *self );
static PyObject * FEdge_setVertexA( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setVertexB( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setId( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setNextEdge( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setPreviousEdge( BPy_FEdge *self , PyObject *args);
static PyObject * FEdge_setSmooth( BPy_FEdge *self , PyObject *args); 
static PyObject * FEdge_setNature( BPy_FEdge *self, PyObject *args );

/*----------------------FEdge instance definitions ----------------------------*/
static PyMethodDef BPy_FEdge_methods[] = {	
	{"__copy__", ( PyCFunction ) FEdge___copy__, METH_NOARGS, "() Cloning method."},
	{"vertexA", ( PyCFunction ) FEdge_vertexA, METH_NOARGS, "() Returns the first SVertex."},
	{"vertexB", ( PyCFunction ) FEdge_vertexB, METH_NOARGS, "() Returns the second SVertex."},
	{"__getitem__", ( PyCFunction ) FEdge___getitem__, METH_VARARGS, "(int i) Returns the first SVertex if i=0, the seccond SVertex if i=1."},
	{"nextEdge", ( PyCFunction ) FEdge_nextEdge, METH_NOARGS, "() Returns the FEdge following this one in the ViewEdge. If this FEdge is the last of the ViewEdge, 0 is returned."},
	{"previousEdge", ( PyCFunction ) FEdge_previousEdge, METH_NOARGS, "Returns the Edge preceding this one in the ViewEdge. If this FEdge is the first one of the ViewEdge, 0 is returned."},
	{"getVertices", ( PyCFunction ) FEdge_getVertices, METH_NOARGS, "Returns the vertices"},
	{"getPoints", ( PyCFunction ) FEdge_getPoints, METH_NOARGS, "Returns the points. The difference with getVertices() is that here we can iterate over points of the 1D element at any given sampling. At each call, a virtual point is created."}, 
	{"isSmooth", ( PyCFunction ) FEdge_isSmooth, METH_NOARGS, "() Returns true if this FEdge is a smooth FEdge."},
	{"setVertexA", ( PyCFunction ) FEdge_setVertexA, METH_VARARGS, "(SVertex v) Sets the first SVertex. ."},
	{"setVertexB", ( PyCFunction ) FEdge_setVertexB, METH_VARARGS, "(SVertex v) Sets the second SVertex. "},
	{"setId", ( PyCFunction ) FEdge_setId, METH_VARARGS, "(Id id) Sets the FEdge Id ."},
	{"setNextEdge", ( PyCFunction ) FEdge_setNextEdge, METH_VARARGS, "(FEdge e) Sets the pointer to the next FEdge. "},
	{"setPreviousEdge", ( PyCFunction ) FEdge_setPreviousEdge, METH_VARARGS, "(FEdge e) Sets the pointer to the previous FEdge. "},
	{"setSmooth", ( PyCFunction ) FEdge_setSmooth, METH_VARARGS, "(bool b) Sets the flag telling whether this FEdge is smooth or sharp. true for Smooth, false for Sharp. "},
	{"setNature", ( PyCFunction ) FEdge_setNature, METH_VARARGS, "(Nature n) Sets the nature of this FEdge. "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_FEdge type definition ------------------------------*/

PyTypeObject FEdge_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"FEdge",				/* tp_name */
	sizeof( BPy_FEdge ),	/* tp_basicsize */
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
	BPy_FEdge_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Interface1D_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)FEdge___init__,                       	/* initproc tp_init; */
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

//-------------------MODULE INITIALIZATION--------------------------------


//------------------------INSTANCE METHODS ----------------------------------
	
int FEdge___init__(BPy_FEdge *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj1 = 0, *obj2 = 0;

    if (! PyArg_ParseTuple(args, "|OO", &obj1, &obj2) )
        return -1;

	if( !obj1 && !obj2 ){
		self->fe = new FEdge();
	} else if( BPy_SVertex_Check(obj1) && BPy_SVertex_Check(obj2) ) {
		self->fe = new FEdge( ((BPy_SVertex *) obj1)->sv, ((BPy_SVertex *) obj2)->sv );
	} else {
		return -1;
	}

	self->py_if1D.if1D = self->fe;

	return 0;
}


PyObject * FEdge___copy__( BPy_FEdge *self ) {
	BPy_FEdge *py_fe;
	
	py_fe = (BPy_FEdge *) FEdge_Type.tp_new( &FEdge_Type, 0, 0 );
	
	py_fe->fe = new FEdge( *(self->fe) );
	py_fe->py_if1D.if1D = py_fe->fe;

	return (PyObject *) py_fe;
}

PyObject * FEdge_vertexA( BPy_FEdge *self ) {	
	if( self->fe->vertexA() ){
		return BPy_SVertex_from_SVertex( *(self->fe->vertexA()) );
	}
		
	Py_RETURN_NONE;
}

PyObject * FEdge_vertexB( BPy_FEdge *self ) {
	if( self->fe->vertexB() ){
		return BPy_SVertex_from_SVertex( *(self->fe->vertexB()) );
	}
		
	Py_RETURN_NONE;
}


PyObject * FEdge___getitem__( BPy_FEdge *self, PyObject *args ) {
	int i;

	if(!( PyArg_ParseTuple(args, "i", &i) && (i == 0 || i == 1)  )) {
		cout << "ERROR: FEdge___getitem__" << endl;
		Py_RETURN_NONE;
	}
	
	if( SVertex *v = self->fe->operator[](i) )
		return BPy_SVertex_from_SVertex( *v );

	Py_RETURN_NONE;
}

PyObject * FEdge_nextEdge( BPy_FEdge *self ) {
	if( FEdge *fe = self->fe->nextEdge() )
		return BPy_FEdge_from_FEdge( *fe );

	Py_RETURN_NONE;
}

PyObject * FEdge_previousEdge( BPy_FEdge *self ) {
	if( FEdge *fe = self->fe->previousEdge() )
		return BPy_FEdge_from_FEdge( *fe );

	Py_RETURN_NONE;
}

PyObject * FEdge_isSmooth( BPy_FEdge *self ) {
	return PyBool_from_bool( self->fe->isSmooth() );
}
	
PyObject *FEdge_setVertexA( BPy_FEdge *self , PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv) )) {
		cout << "ERROR: FEdge_setVertexA" << endl;
		Py_RETURN_NONE;
	}

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

	if(!( PyArg_ParseTuple(args, "O", &py_id) && BPy_Id_Check(py_id) )) {
		cout << "ERROR: FEdge_setId" << endl;
		Py_RETURN_NONE;
	}

	self->fe->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}


PyObject *FEdge_setNextEdge( BPy_FEdge *self , PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O", &py_fe) && BPy_FEdge_Check(py_fe) )) {
		cout << "ERROR: FEdge_setNextEdge" << endl;
		Py_RETURN_NONE;
	}

	self->fe->setNextEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject *FEdge_setPreviousEdge( BPy_FEdge *self , PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O", &py_fe) && BPy_FEdge_Check(py_fe) )) {
		cout << "ERROR: FEdge_setPreviousEdge" << endl;
		Py_RETURN_NONE;
	}

	self->fe->setPreviousEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

PyObject *FEdge_setSmooth( BPy_FEdge *self , PyObject *args) {
	int b;

	if(!( PyArg_ParseTuple(args, "i", &b) )) {
		cout << "ERROR: FEdge_setSmooth" << endl;
		Py_RETURN_NONE;
	}

	self->fe->setSmooth( (bool) b );

	Py_RETURN_NONE;
}

PyObject * FEdge_setNature( BPy_FEdge *self, PyObject *args ) {
	PyObject *py_n;

	if(!( PyArg_ParseTuple(args, "O", &py_n) && BPy_Nature_Check(py_n) )) {
		cout << "ERROR: FEdge_setNature" << endl;
		Py_RETURN_NONE;
	}
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	((FEdge *) self->py_if1D.if1D)->setNature( PyInt_AsLong(i) );

	Py_RETURN_NONE;
}

PyObject *FEdge_getVertices( BPy_FEdge *self ) {
	PyObject *py_vertices = PyList_New(NULL);
		
	for( Interface0DIterator it = self->fe->verticesBegin(); it != self->fe->verticesEnd(); it++ ) {
		PyList_Append( py_vertices, BPy_Interface0D_from_Interface0D( *it ) );
	}
	
	return py_vertices;
}

PyObject *FEdge_getPoints( BPy_FEdge *self ) {
	PyObject *py_points = PyList_New(NULL);
	
	for( Interface0DIterator it = self->fe->pointsBegin(); it != self->fe->pointsEnd(); it++ ) {
		PyList_Append( py_points, BPy_Interface0D_from_Interface0D( *it ) );
	}
	
	return py_points;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
