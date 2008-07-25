#include "BPy_TVertex.h"

#include "../../BPy_Convert.h"
#include "../BPy_SVertex.h"
#include "../../BPy_Id.h"
#include "../../Interface1D/BPy_FEdge.h"
#include "../../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for TVertex___init__ instance  -----------*/
static int TVertex___init__(BPy_TVertex *self, PyObject *args, PyObject *kwds);

static PyObject * TVertex_castToViewVertex( BPy_TVertex *self );
static PyObject * TVertex_castToTVertex( BPy_TVertex *self );
static PyObject * TVertex_frontSVertex( BPy_TVertex *self );
static PyObject * TVertex_backSVertex( BPy_TVertex *self );
static PyObject * TVertex_setFrontSVertex( BPy_TVertex *self, PyObject *args); 
static PyObject * TVertex_setBackSVertex( BPy_TVertex *self, PyObject *args); 
static PyObject * TVertex_setId( BPy_TVertex *self, PyObject *args);
static PyObject * TVertex_getSVertex( BPy_TVertex *self, PyObject *args);
static PyObject * TVertex_mate( BPy_TVertex *self, PyObject *args);

/*----------------------TVertex instance definitions ----------------------------*/
static PyMethodDef BPy_TVertex_methods[] = {	
//	{"__copy__", ( PyCFunction ) TVertex___copy__, METH_NOARGS, "（ ）Cloning method."},
	{"castToViewVertex", ( PyCFunction ) TVertex_castToViewVertex, METH_NOARGS, "（ ）Cast the Interface0D in ViewVertex if it can be. "},
	{"castToTVertex", ( PyCFunction ) TVertex_castToTVertex, METH_NOARGS, "（ ）Cast the Interface0D in TVertex if it can be. "},
	{"frontSVertex", ( PyCFunction ) TVertex_frontSVertex, METH_NOARGS, "（ ）Returns the SVertex that is closer to the viewpoint. "},
	{"backSVertex", ( PyCFunction ) TVertex_backSVertex, METH_NOARGS, "（ ）Returns the SVertex that is further away from the viewpoint. "},
	{"setFrontSVertex", ( PyCFunction ) TVertex_setFrontSVertex, METH_VARARGS, "（SVertex sv ）Sets the SVertex that is closer to the viewpoint. "},
	{"setBackSVertex", ( PyCFunction ) TVertex_setBackSVertex, METH_VARARGS, "（SVertex sv ）Sets the SVertex that is further away from the viewpoint. "},
	{"setId", ( PyCFunction ) TVertex_setId, METH_VARARGS, "（Id id ）Sets the Id."},
	{"getSVertex", ( PyCFunction ) TVertex_getSVertex, METH_VARARGS, "（FEdge fe ）Returns the SVertex (among the 2) belonging to the FEdge iFEdge "},
	{"mate", ( PyCFunction ) TVertex_mate, METH_VARARGS, "（ViewEdge ve ）Returns the mate edge of iEdgeA. For example, if iEdgeA is frontEdgeA, then frontEdgeB is returned. If iEdgeA is frontEdgeB then frontEdgeA is returned. Same for back edges"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_TVertex type definition ------------------------------*/

PyTypeObject TVertex_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"TVertex",				/* tp_name */
	sizeof( BPy_TVertex ),	/* tp_basicsize */
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
	BPy_TVertex_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&ViewVertex_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)TVertex___init__,                       	/* initproc tp_init; */
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

int TVertex___init__(BPy_TVertex *self, PyObject *args, PyObject *kwds)
{
	self->tv = new TVertex();
	self->py_vv.vv = self->tv;
	self->py_vv.py_if0D.if0D = self->tv;

	return 0;
}


PyObject * TVertex_castToViewVertex( BPy_TVertex *self ) {
	PyObject *py_vv =  ViewVertex_Type.tp_new( &ViewVertex_Type, 0, 0 );
	((BPy_ViewVertex *) py_vv)->vv = self->tv->castToViewVertex();

	return py_vv;
}

PyObject * TVertex_castToTVertex( BPy_TVertex *self ) {
	PyObject *py_tv =  TVertex_Type.tp_new( &TVertex_Type, 0, 0 );
	((BPy_TVertex *) py_tv)->tv = self->tv->castToTVertex();

	return py_tv;
}

PyObject * TVertex_frontSVertex( BPy_TVertex *self ) {
	if( self->tv->frontSVertex() ){
		return BPy_SVertex_from_SVertex(*( self->tv->frontSVertex() ));
	}

	Py_RETURN_NONE;
}

PyObject * TVertex_backSVertex( BPy_TVertex *self ) {
	if( self->tv->backSVertex() ){
		return BPy_SVertex_from_SVertex(*( self->tv->backSVertex() ));
	}

	Py_RETURN_NONE;
}

PyObject * TVertex_setFrontSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv) )) {
		cout << "ERROR: TVertex_setFrontSVertex" << endl;
		Py_RETURN_NONE;
	}

	self->tv->setFrontSVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject * TVertex_setBackSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &py_sv) && BPy_SVertex_Check(py_sv) )) {
		cout << "ERROR: TVertex_setBackSVertex" << endl;
		Py_RETURN_NONE;
	}

	self->tv->setBackSVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject * TVertex_setId( BPy_TVertex *self, PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O", &py_id) && BPy_Id_Check(py_id) )) {
		cout << "ERROR: TVertex_setId" << endl;
		Py_RETURN_NONE;
	}

	if( ((BPy_Id *) py_id)->id )
		self->tv->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}

PyObject * TVertex_getSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O", &py_fe) && BPy_FEdge_Check(py_fe) )) {
		cout << "ERROR: TVertex_getSVertex" << endl;
		Py_RETURN_NONE;
	}

	SVertex *sv = self->tv->getSVertex( ((BPy_FEdge *) py_fe)->fe );
	if( sv ){
		return BPy_SVertex_from_SVertex(*( sv ));
	}

	Py_RETURN_NONE;
}

PyObject * TVertex_mate( BPy_TVertex *self, PyObject *args) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O", &py_ve) && BPy_ViewEdge_Check(py_ve) )) {
		cout << "ERROR: TVertex_mate" << endl;
		Py_RETURN_NONE;
	}

	ViewEdge *ve = self->tv->mate( ((BPy_ViewEdge *) py_ve)->ve );
	if( ve ){
		return BPy_ViewEdge_from_ViewEdge(*( ve ));
	}

	Py_RETURN_NONE;
}

	
///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
