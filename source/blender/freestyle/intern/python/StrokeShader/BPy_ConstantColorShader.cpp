#include "BPy_ConstantColorShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ConstantColorShader___doc__[] =
"[Color shader]\n"
"\n"
".. method:: __init__(iR, iG, iB, iAlpha=1.0)\n"
"\n"
"   Builds a ConstantColorShader object.\n"
"\n"
"   :arg iR: The red component.\n"
"   :type iR: float\n"
"   :arg iG: The green component.\n"
"   :type iG: float\n"
"   :arg iB: The blue component.\n"
"   :type iB: float\n"
"   :arg iAlpha: The alpha value.\n"
"   :type iAlpha: float\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Assigns a constant color to every vertex of the Stroke.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int ConstantColorShader___init__( BPy_ConstantColorShader* self, PyObject *args)
{
	float f1, f2, f3, f4 = 1.0;

	if(!( PyArg_ParseTuple(args, "fff|f", &f1, &f2, &f3, &f4) ))
		return -1;

	self->py_ss.ss = new StrokeShaders::ConstantColorShader(f1, f2, f3, f4);
	return 0;
}

/*-----------------------BPy_ConstantColorShader type definition ------------------------------*/

PyTypeObject ConstantColorShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ConstantColorShader",          /* tp_name */
	sizeof(BPy_ConstantColorShader), /* tp_basicsize */
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
	ConstantColorShader___doc__,    /* tp_doc */
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
	(initproc)ConstantColorShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
