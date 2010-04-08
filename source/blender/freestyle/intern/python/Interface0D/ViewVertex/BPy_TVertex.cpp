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

static PyObject * TVertex_frontSVertex( BPy_TVertex *self );
static PyObject * TVertex_backSVertex( BPy_TVertex *self );
static PyObject * TVertex_setFrontSVertex( BPy_TVertex *self, PyObject *args); 
static PyObject * TVertex_setBackSVertex( BPy_TVertex *self, PyObject *args); 
static PyObject * TVertex_setId( BPy_TVertex *self, PyObject *args);
static PyObject * TVertex_getSVertex( BPy_TVertex *self, PyObject *args);
static PyObject * TVertex_mate( BPy_TVertex *self, PyObject *args);

/*----------------------TVertex instance definitions ----------------------------*/
static PyMethodDef BPy_TVertex_methods[] = {	
//	{"__copy__", ( PyCFunction ) TVertex___copy__, METH_NOARGS, "() Cloning method."},
	{"frontSVertex", ( PyCFunction ) TVertex_frontSVertex, METH_NOARGS, "() Returns the SVertex that is closer to the viewpoint. "},
	{"backSVertex", ( PyCFunction ) TVertex_backSVertex, METH_NOARGS, "() Returns the SVertex that is further away from the viewpoint. "},
	{"setFrontSVertex", ( PyCFunction ) TVertex_setFrontSVertex, METH_VARARGS, "(SVertex sv) Sets the SVertex that is closer to the viewpoint. "},
	{"setBackSVertex", ( PyCFunction ) TVertex_setBackSVertex, METH_VARARGS, "(SVertex sv) Sets the SVertex that is further away from the viewpoint. "},
	{"setId", ( PyCFunction ) TVertex_setId, METH_VARARGS, "(Id id) Sets the Id."},
	{"getSVertex", ( PyCFunction ) TVertex_getSVertex, METH_VARARGS, "(FEdge fe) Returns the SVertex (among the 2) belonging to the FEdge iFEdge "},
	{"mate", ( PyCFunction ) TVertex_mate, METH_VARARGS, "(ViewEdge ve) Returns the mate edge of iEdgeA. For example, if iEdgeA is frontEdgeA, then frontEdgeB is returned. If iEdgeA is frontEdgeB then frontEdgeA is returned. Same for back edges"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_TVertex type definition ------------------------------*/

PyTypeObject TVertex_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"TVertex",                      /* tp_name */
	sizeof(BPy_TVertex),            /* tp_basicsize */
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
	"TVertex objects",              /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_TVertex_methods,            /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&ViewVertex_Type,               /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)TVertex___init__,     /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int TVertex___init__(BPy_TVertex *self, PyObject *args, PyObject *kwds)
{
    if( !PyArg_ParseTuple(args, "") )
        return -1;
	self->tv = new TVertex();
	self->py_vv.vv = self->tv;
	self->py_vv.py_if0D.if0D = self->tv;
	self->py_vv.py_if0D.borrowed = 0;

	return 0;
}


PyObject * TVertex_frontSVertex( BPy_TVertex *self ) {
	SVertex *v = self->tv->frontSVertex();
	if( v ){
		return BPy_SVertex_from_SVertex( *v );
	}

	Py_RETURN_NONE;
}

PyObject * TVertex_backSVertex( BPy_TVertex *self ) {
	SVertex *v = self->tv->backSVertex();
	if( v ){
		return BPy_SVertex_from_SVertex( *v );
	}

	Py_RETURN_NONE;
}

PyObject * TVertex_setFrontSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &SVertex_Type, &py_sv) ))
		return NULL;

	self->tv->setFrontSVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject * TVertex_setBackSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O", &SVertex_Type, &py_sv) ))
		return NULL;

	self->tv->setBackSVertex( ((BPy_SVertex *) py_sv)->sv );

	Py_RETURN_NONE;
}

PyObject * TVertex_setId( BPy_TVertex *self, PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	if( ((BPy_Id *) py_id)->id )
		self->tv->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}

PyObject * TVertex_getSVertex( BPy_TVertex *self, PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	SVertex *sv = self->tv->getSVertex( ((BPy_FEdge *) py_fe)->fe );
	if( sv ){
		return BPy_SVertex_from_SVertex( *sv );
	}

	Py_RETURN_NONE;
}

PyObject * TVertex_mate( BPy_TVertex *self, PyObject *args) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;

	ViewEdge *ve = self->tv->mate( ((BPy_ViewEdge *) py_ve)->ve );
	if( ve ){
		return BPy_ViewEdge_from_ViewEdge( *ve );
	}

	Py_RETURN_NONE;
}

	
///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
