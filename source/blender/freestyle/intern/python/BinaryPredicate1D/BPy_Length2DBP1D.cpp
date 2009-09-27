#include "BPy_Length2DBP1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Length2DBP1D instance  -----------*/
static int Length2DBP1D___init__(BPy_Length2DBP1D* self, PyObject *args);

/*-----------------------BPy_Length2DBP1D type definition ------------------------------*/
PyTypeObject Length2DBP1D_Type = {
	PyObject_HEAD_INIT(NULL)
	"Length2DBP1D",                 /* tp_name */
	sizeof(BPy_Length2DBP1D),       /* tp_basicsize */
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
	"Length2DBP1D objects",         /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&BinaryPredicate1D_Type,        /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Length2DBP1D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int Length2DBP1D___init__( BPy_Length2DBP1D* self, PyObject *args )
{
	if(!( PyArg_ParseTuple(args, "") ))
		return -1;
	self->py_bp1D.bp1D = new Predicates1D::Length2DBP1D();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
