#include "BPy_StrokeShader.h"

#include "BPy_Convert.h"
#include "Interface1D/BPy_Stroke.h"

#include "StrokeShader/BPy_BackboneStretcherShader.h"
#include "StrokeShader/BPy_BezierCurveShader.h"
#include "StrokeShader/BPy_CalligraphicShader.h"
#include "StrokeShader/BPy_ColorNoiseShader.h"
#include "StrokeShader/BPy_ColorVariationPatternShader.h"
#include "StrokeShader/BPy_ConstantColorShader.h"
#include "StrokeShader/BPy_ConstantThicknessShader.h"
#include "StrokeShader/BPy_ConstrainedIncreasingThicknessShader.h"
#include "StrokeShader/BPy_fstreamShader.h"
#include "StrokeShader/BPy_GuidingLinesShader.h"
#include "StrokeShader/BPy_IncreasingColorShader.h"
#include "StrokeShader/BPy_IncreasingThicknessShader.h"
#include "StrokeShader/BPy_PolygonalizationShader.h"
#include "StrokeShader/BPy_SamplingShader.h"
#include "StrokeShader/BPy_SmoothingShader.h"
#include "StrokeShader/BPy_SpatialNoiseShader.h"
#include "StrokeShader/BPy_streamShader.h"
#include "StrokeShader/BPy_StrokeTextureShader.h"
#include "StrokeShader/BPy_TextureAssignerShader.h"
#include "StrokeShader/BPy_ThicknessNoiseShader.h"
#include "StrokeShader/BPy_ThicknessVariationPatternShader.h"
#include "StrokeShader/BPy_TipRemoverShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int StrokeShader_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &StrokeShader_Type ) < 0 )
		return -1;
	Py_INCREF( &StrokeShader_Type );
	PyModule_AddObject(module, "StrokeShader", (PyObject *)&StrokeShader_Type);

	if( PyType_Ready( &BackboneStretcherShader_Type ) < 0 )
		return -1;
	Py_INCREF( &BackboneStretcherShader_Type );
	PyModule_AddObject(module, "BackboneStretcherShader", (PyObject *)&BackboneStretcherShader_Type);

	if( PyType_Ready( &BezierCurveShader_Type ) < 0 )
		return -1;
	Py_INCREF( &BezierCurveShader_Type );
	PyModule_AddObject(module, "BezierCurveShader", (PyObject *)&BezierCurveShader_Type);

	if( PyType_Ready( &CalligraphicShader_Type ) < 0 )
		return -1;
	Py_INCREF( &CalligraphicShader_Type );
	PyModule_AddObject(module, "CalligraphicShader", (PyObject *)&CalligraphicShader_Type);

	if( PyType_Ready( &ColorNoiseShader_Type ) < 0 )
		return -1;
	Py_INCREF( &ColorNoiseShader_Type );
	PyModule_AddObject(module, "ColorNoiseShader", (PyObject *)&ColorNoiseShader_Type);

	if( PyType_Ready( &ColorVariationPatternShader_Type ) < 0 )
		return -1;
	Py_INCREF( &ColorVariationPatternShader_Type );
	PyModule_AddObject(module, "ColorVariationPatternShader", (PyObject *)&ColorVariationPatternShader_Type);

	if( PyType_Ready( &ConstantColorShader_Type ) < 0 )
		return -1;
	Py_INCREF( &ConstantColorShader_Type );
	PyModule_AddObject(module, "ConstantColorShader", (PyObject *)&ConstantColorShader_Type);

	if( PyType_Ready( &ConstantThicknessShader_Type ) < 0 )
		return -1;
	Py_INCREF( &ConstantThicknessShader_Type );
	PyModule_AddObject(module, "ConstantThicknessShader", (PyObject *)&ConstantThicknessShader_Type);

	if( PyType_Ready( &ConstrainedIncreasingThicknessShader_Type ) < 0 )
		return -1;
	Py_INCREF( &ConstrainedIncreasingThicknessShader_Type );
	PyModule_AddObject(module, "ConstrainedIncreasingThicknessShader", (PyObject *)&ConstrainedIncreasingThicknessShader_Type);

	if( PyType_Ready( &fstreamShader_Type ) < 0 )
		return -1;
	Py_INCREF( &fstreamShader_Type );
	PyModule_AddObject(module, "fstreamShader", (PyObject *)&fstreamShader_Type);

	if( PyType_Ready( &GuidingLinesShader_Type ) < 0 )
		return -1;
	Py_INCREF( &GuidingLinesShader_Type );
	PyModule_AddObject(module, "GuidingLinesShader", (PyObject *)&GuidingLinesShader_Type);

	if( PyType_Ready( &IncreasingColorShader_Type ) < 0 )
		return -1;
	Py_INCREF( &IncreasingColorShader_Type );
	PyModule_AddObject(module, "IncreasingColorShader", (PyObject *)&IncreasingColorShader_Type);

	if( PyType_Ready( &IncreasingThicknessShader_Type ) < 0 )
		return -1;
	Py_INCREF( &IncreasingThicknessShader_Type );
	PyModule_AddObject(module, "IncreasingThicknessShader", (PyObject *)&IncreasingThicknessShader_Type);

	if( PyType_Ready( &PolygonalizationShader_Type ) < 0 )
		return -1;
	Py_INCREF( &PolygonalizationShader_Type );
	PyModule_AddObject(module, "PolygonalizationShader", (PyObject *)&PolygonalizationShader_Type);

	if( PyType_Ready( &SamplingShader_Type ) < 0 )
		return -1;
	Py_INCREF( &SamplingShader_Type );
	PyModule_AddObject(module, "SamplingShader", (PyObject *)&SamplingShader_Type);

	if( PyType_Ready( &SmoothingShader_Type ) < 0 )
		return -1;
	Py_INCREF( &SmoothingShader_Type );
	PyModule_AddObject(module, "SmoothingShader", (PyObject *)&SmoothingShader_Type);

	if( PyType_Ready( &SpatialNoiseShader_Type ) < 0 )
		return -1;
	Py_INCREF( &SpatialNoiseShader_Type );
	PyModule_AddObject(module, "SpatialNoiseShader", (PyObject *)&SpatialNoiseShader_Type);

	if( PyType_Ready( &streamShader_Type ) < 0 )
		return -1;
	Py_INCREF( &streamShader_Type );
	PyModule_AddObject(module, "streamShader", (PyObject *)&streamShader_Type);

	if( PyType_Ready( &StrokeTextureShader_Type ) < 0 )
		return -1;
	Py_INCREF( &StrokeTextureShader_Type );
	PyModule_AddObject(module, "StrokeTextureShader", (PyObject *)&StrokeTextureShader_Type);

	if( PyType_Ready( &TextureAssignerShader_Type ) < 0 )
		return -1;
	Py_INCREF( &TextureAssignerShader_Type );
	PyModule_AddObject(module, "TextureAssignerShader", (PyObject *)&TextureAssignerShader_Type);

	if( PyType_Ready( &ThicknessNoiseShader_Type ) < 0 )
		return -1;
	Py_INCREF( &ThicknessNoiseShader_Type );
	PyModule_AddObject(module, "ThicknessNoiseShader", (PyObject *)&ThicknessNoiseShader_Type);

	if( PyType_Ready( &ThicknessVariationPatternShader_Type ) < 0 )
		return -1;
	Py_INCREF( &ThicknessVariationPatternShader_Type );
	PyModule_AddObject(module, "ThicknessVariationPatternShader", (PyObject *)&ThicknessVariationPatternShader_Type);

	if( PyType_Ready( &TipRemoverShader_Type ) < 0 )
		return -1;
	Py_INCREF( &TipRemoverShader_Type );
	PyModule_AddObject(module, "TipRemoverShader", (PyObject *)&TipRemoverShader_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char StrokeShader___doc__[] =
"Base class for stroke shaders.  Any stroke shader must inherit from\n"
"this class and overload the shade() method.  A StrokeShader is\n"
"designed to modify stroke attributes such as thickness, color,\n"
"geometry, texture, blending mode, and so on.  The basic way for this\n"
"operation is to iterate over the stroke vertices of the :class:`Stroke`\n"
"and to modify the :class:`StrokeAttribute` of each vertex.  Here is a\n"
"code example of such an iteration::\n"
"\n"
"  it = ioStroke.strokeVerticesBegin()\n"
"  while it.isEnd() == 0:\n"
"      att = it.getObject().attribute()\n"
"      ## perform here any attribute modification\n"
"      it.increment()\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n";

static int StrokeShader___init__(BPy_StrokeShader *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->ss = new StrokeShader();
	self->ss->py_ss = (PyObject *) self;
	return 0;
}

static void StrokeShader___dealloc__(BPy_StrokeShader* self)
{
	if (self->ss)
		delete self->ss;
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject * StrokeShader___repr__(BPy_StrokeShader* self)
{
    return PyUnicode_FromFormat("type: %s - address: %p", self->ss->getName().c_str(), self->ss );
}

static char StrokeShader_getName___doc__[] =
".. method:: getName()\n"
"\n"
"   Returns the name of this stroke shader.\n"
"\n"
"   :return: The name of this stroke shader.\n"
"   :rtype: str\n";

static PyObject * StrokeShader_getName( BPy_StrokeShader *self, PyObject *args)
{
	return PyUnicode_FromString( self->ss->getName().c_str() );
}

static char StrokeShader_shade___doc__[] =
".. method:: shade(s)\n"
"\n"
"   The shading method.  Must be overloaded by inherited classes.\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static PyObject *StrokeShader_shade( BPy_StrokeShader *self , PyObject *args) {
	PyObject *py_s = 0;

	if(!( PyArg_ParseTuple(args, "O!", &Stroke_Type, &py_s) ))
		return NULL;
	
	if( typeid(*(self->ss)) == typeid(StrokeShader) ) {
		PyErr_SetString(PyExc_TypeError, "shade method not properly overridden");
		return NULL;
	}
	if (self->ss->shade(*( ((BPy_Stroke *) py_s)->s )) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->ss->getName() + " shade method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	Py_RETURN_NONE;
}

/*----------------------StrokeShader instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeShader_methods[] = {
	{"getName", ( PyCFunction ) StrokeShader_getName, METH_NOARGS, StrokeShader_getName___doc__},
	{"shade", ( PyCFunction ) StrokeShader_shade, METH_VARARGS, StrokeShader_shade___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_StrokeShader type definition ------------------------------*/

PyTypeObject StrokeShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"StrokeShader",                 /* tp_name */
	sizeof(BPy_StrokeShader),       /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)StrokeShader___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)StrokeShader___repr__, /* tp_repr */
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
	StrokeShader___doc__,           /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_StrokeShader_methods,       /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)StrokeShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
