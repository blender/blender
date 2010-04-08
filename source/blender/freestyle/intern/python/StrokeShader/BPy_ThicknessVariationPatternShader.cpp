#include "BPy_ThicknessVariationPatternShader.h"

#include "../../stroke/BasicStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ThicknessVariationPatternShader instance  -----------*/
static int ThicknessVariationPatternShader___init__( BPy_ThicknessVariationPatternShader* self, PyObject *args);

/*-----------------------BPy_ThicknessVariationPatternShader type definition ------------------------------*/

PyTypeObject ThicknessVariationPatternShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ThicknessVariationPatternShader", /* tp_name */
	sizeof(BPy_ThicknessVariationPatternShader), /* tp_basicsize */
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
	"ThicknessVariationPatternShader objects", /* tp_doc */
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
	(initproc)ThicknessVariationPatternShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//------------------------INSTANCE METHODS ----------------------------------

int ThicknessVariationPatternShader___init__( BPy_ThicknessVariationPatternShader* self, PyObject *args)
{
	const char *s1;
	float f2 = 1.0, f3 = 5.0;
	PyObject *obj4 = 0;

	if(!( PyArg_ParseTuple(args, "s|ffO", &s1, &f2, &f3, &obj4) ))
		return -1;

	bool b = (obj4) ? bool_from_PyBool(obj4) : true;
	self->py_ss.ss = new StrokeShaders::ThicknessVariationPatternShader(s1, f2, f3, b);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
