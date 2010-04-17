#include "BPy_SpatialNoiseShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char SpatialNoiseShader___doc__[] =
"[Geometry shader]\n"
"\n"
".. method:: __init__(iAmount, ixScale, nbOctave, smooth, pureRandom)\n"
"\n"
"   Builds a SpatialNoiseShader object.\n"
"\n"
"   :arg iAmount: The amplitude of the noise.\n"
"   :type iAmount: float\n"
"   :arg ixScale: The noise frequency.\n"
"   :type ixScale: float\n"
"   :arg nbOctave: The number of octaves\n"
"   :type nbOctave: int\n"
"   :arg smooth: True if you want the noise to be smooth.\n"
"   :type smooth: bool\n"
"   :arg pureRandom: True if you don't want any coherence.\n"
"   :type pureRandom: bool\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Spatial Noise stroke shader.  Moves the vertices to make the stroke\n"
"   more noisy.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int SpatialNoiseShader___init__( BPy_SpatialNoiseShader* self, PyObject *args)
{
	float f1, f2;
	int i3;
	PyObject *obj4 = 0, *obj5 = 0;
	
	if(!( PyArg_ParseTuple(args, "ffiOO", &f1, &f2, &i3, &obj4, &obj5) ))
		return -1;

	self->py_ss.ss = new SpatialNoiseShader(f1, f2, i3, bool_from_PyBool(obj4), bool_from_PyBool(obj5) );
	return 0;
}

/*-----------------------BPy_SpatialNoiseShader type definition ------------------------------*/

PyTypeObject SpatialNoiseShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SpatialNoiseShader",          /* tp_name */
	sizeof(BPy_SpatialNoiseShader), /* tp_basicsize */
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
	SpatialNoiseShader___doc__,     /* tp_doc */
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
	(initproc)SpatialNoiseShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
