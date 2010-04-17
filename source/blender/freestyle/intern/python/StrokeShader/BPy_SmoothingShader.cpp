#include "BPy_SmoothingShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char SmoothingShader___doc__[] =
"[Geometry shader]\n"
"\n"
".. method:: __init__(iNbIteration, iFactorPoint, ifactorCurvature, iFactorCurvatureDifference, iAnisoPoint, iAnisNormal, iAnisoCurvature, icarricatureFactor)\n"
"\n"
"   Builds a SmoothingShader object.\n"
"\n"
"   :arg iNbIteration: The number of iterations (400).\n"
"   :type iNbIteration: int\n"
"   :arg iFactorPoint: 0.0\n"
"   :type iFactorPoint: float\n"
"   :arg ifactorCurvature: 0.0\n"
"   :type ifactorCurvature: float\n"
"   :arg iFactorCurvatureDifference: 0.2\n"
"   :type iFactorCurvatureDifference: float\n"
"   :arg iAnisoPoint: \n"
"   :type iAnisoPoint: float\n"
"   :arg iAnisNormal: 0.0\n"
"   :type iAnisNormal: float\n"
"   :arg iAnisoCurvature: 0.0\n"
"   :type iAnisoCurvature: float\n"
"   :arg icarricatureFactor: 1.0\n"
"   :type icarricatureFactor: float\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Smoothes the stroke by moving the vertices to make the stroke\n"
"   smoother.  Uses curvature flow to converge towards a curve of\n"
"   constant curvature.  The diffusion method we use is anisotropic to\n"
"   prevent the diffusion accross corners.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int SmoothingShader___init__( BPy_SmoothingShader* self, PyObject *args)
{
	int i1;
	double d2, d3, d4, d5, d6, d7, d8;

	if(!( PyArg_ParseTuple(args, "iddddddd", &i1, &d2, &d3, &d4, &d5, &d6, &d7, &d8) ))
		return -1;

	self->py_ss.ss = new SmoothingShader(i1, d2, d3, d4, d5, d6, d7, d8);
	return 0;
}

/*-----------------------BPy_SmoothingShader type definition ------------------------------*/

PyTypeObject SmoothingShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SmoothingShader",              /* tp_name */
	sizeof(BPy_SmoothingShader),    /* tp_basicsize */
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
	SmoothingShader___doc__,        /* tp_doc */
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
	(initproc)SmoothingShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
