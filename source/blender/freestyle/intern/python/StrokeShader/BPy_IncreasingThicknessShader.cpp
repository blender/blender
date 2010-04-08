#include "BPy_IncreasingThicknessShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for IncreasingThicknessShader instance  -----------*/
static int IncreasingThicknessShader___init__( BPy_IncreasingThicknessShader* self, PyObject *args);

/*-----------------------BPy_IncreasingThicknessShader type definition ------------------------------*/

PyTypeObject IncreasingThicknessShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"IncreasingThicknessShader",    /* tp_name */
	sizeof(BPy_IncreasingThicknessShader), /* tp_basicsize */
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
	"IncreasingThicknessShader objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&StrokeShader_Type,             /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)IncreasingThicknessShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int IncreasingThicknessShader___init__( BPy_IncreasingThicknessShader* self, PyObject *args)
{
	float f1, f2;

	if(!( PyArg_ParseTuple(args, "ff", &f1, &f2) ))
		return -1;

	self->py_ss.ss = new StrokeShaders::IncreasingThicknessShader(f1, f2);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
