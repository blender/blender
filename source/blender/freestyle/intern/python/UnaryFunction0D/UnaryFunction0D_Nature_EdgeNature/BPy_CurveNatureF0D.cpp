#include "BPy_CurveNatureF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for CurveNatureF0D instance  -----------*/
	static int CurveNatureF0D___init__(BPy_CurveNatureF0D* self, PyObject *args);

/*-----------------------BPy_CurveNatureF0D type definition ------------------------------*/

PyTypeObject CurveNatureF0D_Type = {
	PyObject_HEAD_INIT(NULL)
	"CurveNatureF0D",               /* tp_name */
	sizeof(BPy_CurveNatureF0D),     /* tp_basicsize */
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
	"CurveNatureF0D objects",       /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DEdgeNature_Type, /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)CurveNatureF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int CurveNatureF0D___init__( BPy_CurveNatureF0D* self, PyObject *args )
{
	if( !PyArg_ParseTuple(args, "") )
		return -1;
	self->py_uf0D_edgenature.uf0D_edgenature = new Functions0D::CurveNatureF0D();
	self->py_uf0D_edgenature.uf0D_edgenature->py_uf0D = (PyObject *)self;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
