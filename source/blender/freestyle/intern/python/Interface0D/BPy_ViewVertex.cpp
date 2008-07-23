#include "BPy_ViewVertex.h"

#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ViewVertex instance  -----------*/
static int ViewVertex___init__(BPy_ViewVertex *self);
static PyObject * ViewVertex_setNature( BPy_ViewVertex *self, PyObject *args );


/*----------------------ViewVertex instance definitions ----------------------------*/
static PyMethodDef BPy_ViewVertex_methods[] = {
	{"setNature", ( PyCFunction ) ViewVertex_setNature, METH_VARARGS, "（Nature n ）Sets the nature of the vertex."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewVertex type definition ------------------------------*/

PyTypeObject ViewVertex_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"ViewVertex",				/* tp_name */
	sizeof( BPy_ViewVertex ),	/* tp_basicsize */
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
	BPy_ViewVertex_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&Interface0D_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)ViewVertex___init__,                       	/* initproc tp_init; */
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

int ViewVertex___init__(BPy_ViewVertex *self )
{	
	self->py_if0D.if0D = new Interface0D();
	return 0;
}

// PyObject * ViewVertex___copy__( BPy_ViewVertex *self ) {
// 	BPy_ViewVertex *py_vv;
// 	
// 	py_vv = (BPy_ViewVertex *) ViewVertex_Type.tp_new( &ViewVertex_Type, 0, 0 );
// 	
// 	py_vv->vv = self->vv->duplicate();
// 	py_svertex->py_if0D.if->sv;
// 
// 	return (PyObject *) py_svertex;
// }

PyObject * ViewVertex_setNature( BPy_ViewVertex *self, PyObject *args ) {
	PyObject *py_n;

	if(!( PyArg_ParseTuple(args, "O", &py_n) && BPy_Nature_Check(py_n) )) {
		cout << "ERROR: ViewVertex_setNature" << endl;
		Py_RETURN_NONE;
	}
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	((ViewVertex *) self->py_if0D.if0D)->setNature( PyInt_AsLong(i) );

	Py_RETURN_NONE;
}



///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


// virtual string 	getExactTypeName () const

// void 	setNature (Nature::VertexNature iNature)
// virtual ViewVertexInternal::orientedViewEdgeIterator 	edgesBegin ()=0
// virtual ViewVertexInternal::orientedViewEdgeIterator 	edgesEnd ()=0
// virtual ViewVertexInternal::orientedViewEdgeIterator 	edgesIterator (ViewEdge *iEdge)=0
// 
