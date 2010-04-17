#include "BPy_BezierCurveShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char BezierCurveShader___doc__[] =
"[Geometry shader]\n"
"\n"
".. method:: __init__(error=4.0)\n"
"\n"
"   Builds a BezierCurveShader object.\n"
"\n"
"   :arg error: The error we're allowing for the approximation.  This\n"
"     error is the max distance allowed between the new curve and the\n"
"     original geometry.\n"
"   :type error: float\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Transforms the stroke backbone geometry so that it corresponds to a\n"
"   Bezier Curve approximation of the original backbone geometry.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int BezierCurveShader___init__( BPy_BezierCurveShader* self, PyObject *args)
{
	float f = 4.0;

	if(!( PyArg_ParseTuple(args, "|f", &f) ))
		return -1;

	self->py_ss.ss = new StrokeShaders::BezierCurveShader(f);
	return 0;
}

/*-----------------------BPy_BezierCurveShader type definition ------------------------------*/

PyTypeObject BezierCurveShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BezierCurveShader",            /* tp_name */
	sizeof(BPy_BezierCurveShader),  /* tp_basicsize */
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
	BezierCurveShader___doc__,      /* tp_doc */
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
	(initproc)BezierCurveShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
