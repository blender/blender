#include "BPy_fstreamShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char fstreamShader___doc__[] =
"[Output shader]\n"
"\n"
".. method:: __init__(iFileName)\n"
"\n"
"   Builds a fstreamShader object.\n"
"\n"
"   :arg iFileName: The output file name.\n"
"   :type iFileName: string\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Streams the Stroke in a file.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int fstreamShader___init__( BPy_fstreamShader* self, PyObject *args)
{
	const char *s;

	if(!( PyArg_ParseTuple(args, "s", &s)  ))
		return -1;

	self->py_ss.ss = new StrokeShaders::fstreamShader(s);
	return 0;
}

/*-----------------------BPy_fstreamShader type definition ------------------------------*/

PyTypeObject fstreamShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"fstreamShader",                /* tp_name */
	sizeof(BPy_fstreamShader),      /* tp_basicsize */
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
	fstreamShader___doc__,          /* tp_doc */
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
	(initproc)fstreamShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
