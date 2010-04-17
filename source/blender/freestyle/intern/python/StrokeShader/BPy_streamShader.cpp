#include "BPy_streamShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char streamShader___doc__[] =
"[Output shader]\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Builds a streamShader object.\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Streams the Stroke into stdout.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int streamShader___init__( BPy_streamShader* self, PyObject *args)
{
	if(!( PyArg_ParseTuple(args, "") ))
		return -1;
	self->py_ss.ss = new StrokeShaders::streamShader();
	return 0;
}

/*-----------------------BPy_streamShader type definition ------------------------------*/

PyTypeObject streamShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"streamShader",                 /* tp_name */
	sizeof(BPy_streamShader),       /* tp_basicsize */
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
	streamShader___doc__,           /* tp_doc */
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
	(initproc)streamShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
