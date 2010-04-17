#include "BPy_IncreasingColorShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char IncreasingColorShader___doc__[] =
"[Color shader]\n"
"\n"
".. method:: __init__(iRm, iGm, iBm, iAlpham, iRM, iGM, iBM, iAlphaM)\n"
"\n"
"   Builds an IncreasingColorShader object.\n"
"\n"
"   :arg iRm: The first color red component.\n"
"   :type iRm: float\n"
"   :arg iGm: The first color green component.\n"
"   :type iGm: float\n"
"   :arg iBm: The first color blue component.\n"
"   :type iBm: float\n"
"   :arg iAlpham: The first color alpha value.\n"
"   :type iAlpham: float\n"
"   :arg iRM: The second color red component.\n"
"   :type iRM: float\n"
"   :arg iGM: The second color green component.\n"
"   :type iGM: float\n"
"   :arg iBM: The second color blue component.\n"
"   :type iBM: float\n"
"   :arg iAlphaM: The second color alpha value.\n"
"   :type iAlphaM: float\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Assigns a varying color to the stroke.  The user specifies two\n"
"   colors A and B.  The stroke color will change linearly from A to B\n"
"   between the first and the last vertex.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int IncreasingColorShader___init__( BPy_IncreasingColorShader* self, PyObject *args)
{
	float f1, f2, f3, f4, f5, f6, f7, f8;

	if(!( PyArg_ParseTuple(args, "ffffffff", &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8) ))
		return -1;

	self->py_ss.ss = new StrokeShaders::IncreasingColorShader(f1, f2, f3, f4, f5, f6, f7, f8);
	return 0;
}

/*-----------------------BPy_IncreasingColorShader type definition ------------------------------*/

PyTypeObject IncreasingColorShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"IncreasingColorShader",        /* tp_name */
	sizeof(BPy_IncreasingColorShader), /* tp_basicsize */
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
	IncreasingColorShader___doc__,  /* tp_doc */
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
	(initproc)IncreasingColorShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
