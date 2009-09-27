#include "BPy_ColorVariationPatternShader.h"

#include "../../stroke/BasicStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ColorVariationPatternShader instance  -----------*/
static int ColorVariationPatternShader___init__( BPy_ColorVariationPatternShader* self, PyObject *args);

/*-----------------------BPy_ColorVariationPatternShader type definition ------------------------------*/

PyTypeObject ColorVariationPatternShader_Type = {
	PyObject_HEAD_INIT(NULL)
	"ColorVariationPatternShader",  /* tp_name */
	sizeof(BPy_ColorVariationPatternShader), /* tp_basicsize */
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
	"ColorVariationPatternShader objects", /* tp_doc */
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
	(initproc)ColorVariationPatternShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int ColorVariationPatternShader___init__( BPy_ColorVariationPatternShader* self, PyObject *args)
{
	const char *s;
	PyObject *obj = 0;
	
	if(!( PyArg_ParseTuple(args, "s|O", &s, &obj) ))
		return -1;

	bool b = (obj) ? bool_from_PyBool(obj) : true;
	self->py_ss.ss = new StrokeShaders::ColorVariationPatternShader(s,b);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
