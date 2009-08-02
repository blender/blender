#include "BPy_NonTVertex.h"

#include "../../BPy_Convert.h"
#include "../BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for NonTVertex___init__ instance  -----------*/
static int NonTVertex___init__(BPy_NonTVertex *self, PyObject *args, PyObject *kwds);

static PyObject * NonTVertex_svertex( BPy_NonTVertex *self );
static PyObject * NonTVertex_setSVertex( BPy_NonTVertex *self, PyObject *args);

/*----------------------NonTVertex instance definitions ----------------------------*/
static PyMethodDef BPy_NonTVertex_methods[] = {	
//	{"__copy__", ( PyCFunction ) NonTVertex___copy__, METH_NOARGS, "（ ）Cloning method."},
	{"svertex", ( PyCFunction ) NonTVertex_svertex, METH_NOARGS, "（ ）Returns the SVertex on top of which this NonTVertex is built. "},
	{"setSVertex", ( PyCFunction ) NonTVertex_setSVertex, METH_VARARGS, "（SVertex sv ）Sets the SVertex on top of which this NonTVertex is built. "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_NonTVertex type definition ------------------------------*/

PyTypeObject NonTVertex_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"NonTVertex",				/* tp_name */
	sizeof( BPy_NonTVertex ),	/* tp_basicsize */
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
	BPy_NonTVertex_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	&ViewVertex_Type,				/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)NonTVertex___init__,                       	/* initproc tp_init; */
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

int NonTVertex___init__(BPy_NonTVertex *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj = 0;

    if (! PyArg_ParseTuple(args, "|O!", &SVertex_Type, &obj) )
        return -1;

	if( !obj ){
		self->ntv = new NonTVertex();

	} else if( ((BPy_SVertex *) obj)->sv ) {
		self->ntv = new NonTVertex( ((BPy_SVertex *) obj)->sv );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return -1;
	}

	self->py_vv.vv = self->ntv;
	self->py_vv.py_if0D.if0D = self->ntv;
	self->py_vv.py_if0D.borrowed = 0;

	return 0;
}

PyObject * NonTVertex_svertex( BPy_NonTVertex *self ) {
	SVertex *v = self->ntv->svertex();
	if( v ){
		return BPy_SVertex_from_SVertex( *v );
	}

	Py_RETURN_NONE;
}

PyObject * NonTVertex_setSVertex( BPy_NonTVertex *self, PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->ntv->setSVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
