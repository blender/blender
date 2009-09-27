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
//	{"__copy__", ( PyCFunction ) NonTVertex___copy__, METH_NOARGS, "() Cloning method."},
	{"svertex", ( PyCFunction ) NonTVertex_svertex, METH_NOARGS, "() Returns the SVertex on top of which this NonTVertex is built. "},
	{"setSVertex", ( PyCFunction ) NonTVertex_setSVertex, METH_VARARGS, "(SVertex sv) Sets the SVertex on top of which this NonTVertex is built. "},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_NonTVertex type definition ------------------------------*/

PyTypeObject NonTVertex_Type = {
	PyObject_HEAD_INIT(NULL)
	"NonTVertex",                   /* tp_name */
	sizeof(BPy_NonTVertex),         /* tp_basicsize */
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
	"NonTVertex objects",           /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_NonTVertex_methods,         /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&ViewVertex_Type,               /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)NonTVertex___init__,  /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
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
