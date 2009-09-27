#include "BPy_LocalAverageDepthF0D.h"

#include "../../../stroke/AdvancedFunctions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for LocalAverageDepthF0D instance  -----------*/
static int LocalAverageDepthF0D___init__(BPy_LocalAverageDepthF0D* self, PyObject *args);

/*-----------------------BPy_LocalAverageDepthF0D type definition ------------------------------*/

PyTypeObject LocalAverageDepthF0D_Type = {
	PyObject_HEAD_INIT(NULL)
	"LocalAverageDepthF0D",         /* tp_name */
	sizeof(BPy_LocalAverageDepthF0D), /* tp_basicsize */
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
	"LocalAverageDepthF0D objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DDouble_Type,    /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)LocalAverageDepthF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int LocalAverageDepthF0D___init__( BPy_LocalAverageDepthF0D* self, PyObject *args)
{
	double d = 5.0;

	if( !PyArg_ParseTuple(args, "|d", &d) )
		return -1;
	self->py_uf0D_double.uf0D_double = new Functions0D::LocalAverageDepthF0D(d);
	self->py_uf0D_double.uf0D_double->py_uf0D = (PyObject *)self;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
