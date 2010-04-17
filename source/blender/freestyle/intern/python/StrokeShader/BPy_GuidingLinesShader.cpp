#include "BPy_GuidingLinesShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char GuidingLinesShader___doc__[] =
"[Geometry shader]\n"
"\n"
".. method:: __init__(iOffset)\n"
"\n"
"   Builds a GuidingLinesShader object.\n"
"\n"
"   :arg iOffset: The line that replaces the stroke is initially in the\n"
"      middle of the initial stroke bounding box.  iOffset is the value\n"
"      of the displacement which is applied to this line along its\n"
"      normal.\n"
"   :type iOffset: float\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Shader to modify the Stroke geometry so that it corresponds to its\n"
"   main direction line.  This shader must be used together with the\n"
"   splitting operator using the curvature criterion. Indeed, the\n"
"   precision of the approximation will depend on the size of the\n"
"   stroke's pieces.  The bigger the pieces are, the rougher the\n"
"   approximation is.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int GuidingLinesShader___init__( BPy_GuidingLinesShader* self, PyObject *args)
{
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f) ))
		return -1;

	self->py_ss.ss = new StrokeShaders::GuidingLinesShader(f);
	return 0;
}

/*-----------------------BPy_GuidingLinesShader type definition ------------------------------*/

PyTypeObject GuidingLinesShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"GuidingLinesShader",           /* tp_name */
	sizeof(BPy_GuidingLinesShader), /* tp_basicsize */
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
	GuidingLinesShader___doc__,     /* tp_doc */
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
	(initproc)GuidingLinesShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
