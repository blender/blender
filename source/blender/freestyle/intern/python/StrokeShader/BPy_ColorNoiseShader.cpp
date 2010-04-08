#include "BPy_ColorNoiseShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ColorNoiseShader instance  -----------*/
static int ColorNoiseShader___init__( BPy_ColorNoiseShader* self, PyObject *args);

/*-----------------------BPy_ColorNoiseShader type definition ------------------------------*/

PyTypeObject ColorNoiseShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ColorNoiseShader",             /* tp_name */
	sizeof(BPy_ColorNoiseShader),   /* tp_basicsize */
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
	"ColorNoiseShader objects",     /* tp_doc */
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
	(initproc)ColorNoiseShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int ColorNoiseShader___init__( BPy_ColorNoiseShader* self, PyObject *args)
{
	float f1, f2;

	if(!( PyArg_ParseTuple(args, "ff", &f1, &f2) ))
		return -1;

	self->py_ss.ss = new StrokeShaders::ColorNoiseShader(f1, f2);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
