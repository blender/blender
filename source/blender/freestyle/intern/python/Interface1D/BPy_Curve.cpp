#include "BPy_Curve.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_CurvePoint.h"
#include "../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Curve instance  -----------*/
static int Curve___init__(BPy_Curve *self, PyObject *args, PyObject *kwds);
static PyObject * Curve_push_vertex_back( BPy_Curve *self, PyObject *args );
static PyObject * Curve_push_vertex_front( BPy_Curve *self, PyObject *args );
static PyObject * Curve_empty( BPy_Curve *self );
static PyObject * Curve_nSegments( BPy_Curve *self );
// point_iterator 	points_begin (float step=0)
static PyObject * Curve_verticesBegin( BPy_Curve *self );
static PyObject * Curve_verticesEnd( BPy_Curve *self );
static PyObject * Curve_pointsBegin( BPy_Curve *self, PyObject *args );
static PyObject * Curve_pointsEnd( BPy_Curve *self, PyObject *args );

/*----------------------Curve instance definitions ----------------------------*/
static PyMethodDef BPy_Curve_methods[] = {	
	{"push_vertex_back", ( PyCFunction ) Curve_push_vertex_back, METH_VARARGS, "(CurvePoint cp | SVertex sv) Adds a single vertex at the front of the Curve."},
	{"push_vertex_front", ( PyCFunction ) Curve_push_vertex_front, METH_VARARGS, "(CurvePoint cp | SVertex sv) Adds a single vertex at the end of the Curve."},
	{"empty", ( PyCFunction ) Curve_empty, METH_NOARGS, "() Returns true is the Curve doesn't have any Vertex yet."},
	{"nSegments", ( PyCFunction ) Curve_nSegments, METH_NOARGS, "() Returns the number of segments in the oplyline constituing the Curve."},
	{"verticesBegin", ( PyCFunction ) Curve_verticesBegin, METH_NOARGS, "() Returns an Interface0DIterator pointing onto the first vertex of the Curve and that can iterate over the vertices of the Curve."},
	{"verticesEnd", ( PyCFunction ) Curve_verticesEnd, METH_NOARGS, "() Returns an Interface0DIterator pointing after the last vertex of the Curve and that can iterate over the vertices of the Curve."},
	{"pointsBegin", ( PyCFunction ) Curve_pointsBegin, METH_VARARGS, "(float t=0) Returns an Interface0DIterator pointing onto the first point of the Curve and that can iterate over the points of the Curve at any resolution t. At each iteration a virtual temporary CurvePoint is created."},
	{"pointsEnd", ( PyCFunction ) Curve_pointsEnd, METH_VARARGS, "(float t=0) Returns an Interface0DIterator pointing after the last point of the Curve and that can iterate over the points of the Curve at any resolution t. At each iteration a virtual temporary CurvePoint is created."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Curve type definition ------------------------------*/

PyTypeObject Curve_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Curve",				/* tp_name */
	sizeof( BPy_Curve ),	/* tp_basicsize */
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
	BPy_Curve_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Interface1D_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)Curve___init__,                       	/* initproc tp_init; */
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
	
int Curve___init__(BPy_Curve *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj = 0;

    if (! PyArg_ParseTuple(args, "|O", &obj) )
        return -1;

	if( !obj ){
		self->c = new Curve();
		
	} else if( BPy_Curve_Check(obj) ) {
		if( ((BPy_Curve *) obj)->c )
			self->c = new Curve(*( ((BPy_Curve *) obj)->c ));
		else
			return -1;
		
	} else if( BPy_Id_Check(obj) ) {
		if( ((BPy_Id *) obj)->id )
			self->c = new Curve(*( ((BPy_Id *) obj)->id ));
		else
			return -1;
			
	} else {
		return -1;
	}

	self->py_if1D.if1D = self->c;

	return 0;
}


PyObject * Curve_push_vertex_back( BPy_Curve *self, PyObject *args ) {
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) )) {
		cout << "ERROR: Curve_push_vertex_back" << endl;
		Py_RETURN_NONE;
	}

	if( BPy_CurvePoint_Check(obj) ) {
		self->c->push_vertex_back( ((BPy_CurvePoint *) obj)->cp );
	} else if( BPy_SVertex_Check(obj) ) {
		self->c->push_vertex_back( ((BPy_SVertex *) obj)->sv );
	}

	Py_RETURN_NONE;
}

PyObject * Curve_push_vertex_front( BPy_Curve *self, PyObject *args ) {
		PyObject *obj;

		if(!( PyArg_ParseTuple(args, "O", &obj) )) {
			cout << "ERROR: Curve_push_vertex_front" << endl;
			Py_RETURN_NONE;
		}

		if( BPy_CurvePoint_Check(obj) ) {
			self->c->push_vertex_front( ((BPy_CurvePoint *) obj)->cp );
		} else if( BPy_SVertex_Check(obj) ) {
			self->c->push_vertex_front( ((BPy_SVertex *) obj)->sv );
		}

		Py_RETURN_NONE;
	}

PyObject * Curve_empty( BPy_Curve *self ) {
	return PyBool_from_bool( self->c->empty() );
}

PyObject * Curve_nSegments( BPy_Curve *self ) {
	return PyInt_FromLong( self->c->nSegments() );
}

// point_iterator 	points_begin (float step=0)
// not implemented


PyObject * Curve_verticesBegin( BPy_Curve *self ) {
	Interface0DIterator if0D_it( self->c->verticesBegin() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it );
}

PyObject * Curve_verticesEnd( BPy_Curve *self ) {
	Interface0DIterator if0D_it( self->c->verticesEnd() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it );
}


PyObject * Curve_pointsBegin( BPy_Curve *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  )) {
		cout << "ERROR: FEdge_pointsBegin" << endl;
		Py_RETURN_NONE;
	}
	
	Interface0DIterator if0D_it( self->c->pointsBegin(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it );
}

PyObject * Curve_pointsEnd( BPy_Curve *self, PyObject *args ) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  )) {
		cout << "ERROR: FEdge_pointsEnd" << endl;
		Py_RETURN_NONE;
	}
	
	Interface0DIterator if0D_it( self->c->pointsEnd(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it );
}



///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
