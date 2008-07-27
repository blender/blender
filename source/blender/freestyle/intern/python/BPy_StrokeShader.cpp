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

/*---------------  Python API function prototypes for StrokeShader instance  -----------*/
static int StrokeShader___init__(BPy_StrokeShader *self, PyObject *args, PyObject *kwds);
static void StrokeShader___dealloc__(BPy_StrokeShader *self);
static PyObject * StrokeShader___repr__(BPy_StrokeShader *self);

static PyObject * StrokeShader_getName( BPy_StrokeShader *self, PyObject *args);
static PyObject * StrokeShader_shade( BPy_StrokeShader *self , PyObject *args);

/*----------------------StrokeShader instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeShader_methods[] = {
	{"getName", ( PyCFunction ) StrokeShader_getName, METH_NOARGS, "（ ）Returns the string of the name of the binary predicate."},
	{"shade", ( PyCFunction ) StrokeShader_shade, METH_VARARGS, "（Stroke s ）The shading method. This method must be overloaded by inherited classes. The shading method is designed to modify any Stroke's attribute such as Thickness, Color, Geometry, Texture, Blending mode... The basic way to achieve this operation consists in iterating over the StrokeVertices of the Stroke and to modify each one's StrokeAttribute."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_StrokeShader type definition ------------------------------*/

PyTypeObject StrokeShader_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"StrokeShader",				/* tp_name */
	sizeof( BPy_StrokeShader ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)StrokeShader___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)StrokeShader___repr__,					/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, 		/* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_StrokeShader_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)StrokeShader___init__, /* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,		/* newfunc tp_new; */
	
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

//-------------------MODULE INITIALIZATION--------------------------------
PyMODINIT_FUNC StrokeShader_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &StrokeShader_Type ) < 0 )
		return;
	Py_INCREF( &StrokeShader_Type );
	PyModule_AddObject(module, "StrokeShader", (PyObject *)&StrokeShader_Type);

	if( PyType_Ready( &BackboneStretcherShader_Type ) < 0 )
		return;
	Py_INCREF( &BackboneStretcherShader_Type );
	PyModule_AddObject(module, "BackboneStretcherShader", (PyObject *)&BackboneStretcherShader_Type);

	if( PyType_Ready( &BezierCurveShader_Type ) < 0 )
		return;
	Py_INCREF( &BezierCurveShader_Type );
	PyModule_AddObject(module, "BezierCurveShader", (PyObject *)&BezierCurveShader_Type);

	if( PyType_Ready( &CalligraphicShader_Type ) < 0 )
		return;
	Py_INCREF( &CalligraphicShader_Type );
	PyModule_AddObject(module, "CalligraphicShader", (PyObject *)&CalligraphicShader_Type);

	if( PyType_Ready( &ColorNoiseShader_Type ) < 0 )
		return;
	Py_INCREF( &ColorNoiseShader_Type );
	PyModule_AddObject(module, "ColorNoiseShader", (PyObject *)&ColorNoiseShader_Type);

	if( PyType_Ready( &ColorVariationPatternShader_Type ) < 0 )
		return;
	Py_INCREF( &ColorVariationPatternShader_Type );
	PyModule_AddObject(module, "ColorVariationPatternShader", (PyObject *)&ColorVariationPatternShader_Type);

	if( PyType_Ready( &ConstantColorShader_Type ) < 0 )
		return;
	Py_INCREF( &ConstantColorShader_Type );
	PyModule_AddObject(module, "ConstantColorShader", (PyObject *)&ConstantColorShader_Type);

	if( PyType_Ready( &ConstantThicknessShader_Type ) < 0 )
		return;
	Py_INCREF( &ConstantThicknessShader_Type );
	PyModule_AddObject(module, "ConstantThicknessShader", (PyObject *)&ConstantThicknessShader_Type);

	if( PyType_Ready( &ConstrainedIncreasingThicknessShader_Type ) < 0 )
		return;
	Py_INCREF( &ConstrainedIncreasingThicknessShader_Type );
	PyModule_AddObject(module, "ConstrainedIncreasingThicknessShader", (PyObject *)&ConstrainedIncreasingThicknessShader_Type);

	if( PyType_Ready( &fstreamShader_Type ) < 0 )
		return;
	Py_INCREF( &fstreamShader_Type );
	PyModule_AddObject(module, "fstreamShader", (PyObject *)&fstreamShader_Type);

	if( PyType_Ready( &GuidingLinesShader_Type ) < 0 )
		return;
	Py_INCREF( &GuidingLinesShader_Type );
	PyModule_AddObject(module, "GuidingLinesShader", (PyObject *)&GuidingLinesShader_Type);

	if( PyType_Ready( &IncreasingColorShader_Type ) < 0 )
		return;
	Py_INCREF( &IncreasingColorShader_Type );
	PyModule_AddObject(module, "IncreasingColorShader", (PyObject *)&IncreasingColorShader_Type);

	if( PyType_Ready( &IncreasingThicknessShader_Type ) < 0 )
		return;
	Py_INCREF( &IncreasingThicknessShader_Type );
	PyModule_AddObject(module, "IncreasingThicknessShader", (PyObject *)&IncreasingThicknessShader_Type);

	if( PyType_Ready( &PolygonalizationShader_Type ) < 0 )
		return;
	Py_INCREF( &PolygonalizationShader_Type );
	PyModule_AddObject(module, "PolygonalizationShader", (PyObject *)&PolygonalizationShader_Type);

	if( PyType_Ready( &SamplingShader_Type ) < 0 )
		return;
	Py_INCREF( &SamplingShader_Type );
	PyModule_AddObject(module, "SamplingShader", (PyObject *)&SamplingShader_Type);

	if( PyType_Ready( &SpatialNoiseShader_Type ) < 0 )
		return;
	Py_INCREF( &SpatialNoiseShader_Type );
	PyModule_AddObject(module, "SpatialNoiseShader", (PyObject *)&SpatialNoiseShader_Type);

	if( PyType_Ready( &streamShader_Type ) < 0 )
		return;
	Py_INCREF( &streamShader_Type );
	PyModule_AddObject(module, "streamShader", (PyObject *)&streamShader_Type);

	if( PyType_Ready( &StrokeTextureShader_Type ) < 0 )
		return;
	Py_INCREF( &StrokeTextureShader_Type );
	PyModule_AddObject(module, "StrokeTextureShader", (PyObject *)&StrokeTextureShader_Type);

	if( PyType_Ready( &TextureAssignerShader_Type ) < 0 )
		return;
	Py_INCREF( &TextureAssignerShader_Type );
	PyModule_AddObject(module, "TextureAssignerShader", (PyObject *)&TextureAssignerShader_Type);

	if( PyType_Ready( &ThicknessNoiseShader_Type ) < 0 )
		return;
	Py_INCREF( &ThicknessNoiseShader_Type );
	PyModule_AddObject(module, "ThicknessNoiseShader", (PyObject *)&ThicknessNoiseShader_Type);

	if( PyType_Ready( &ThicknessVariationPatternShader_Type ) < 0 )
		return;
	Py_INCREF( &ThicknessVariationPatternShader_Type );
	PyModule_AddObject(module, "ThicknessVariationPatternShader", (PyObject *)&ThicknessVariationPatternShader_Type);

	if( PyType_Ready( &TipRemoverShader_Type ) < 0 )
		return;
	Py_INCREF( &TipRemoverShader_Type );
	PyModule_AddObject(module, "TipRemoverShader", (PyObject *)&TipRemoverShader_Type);
	
}

//------------------------INSTANCE METHODS ----------------------------------

int StrokeShader___init__(BPy_StrokeShader *self, PyObject *args, PyObject *kwds)
{
	self->ss = new StrokeShader();
	return 0;
}

void StrokeShader___dealloc__(BPy_StrokeShader* self)
{
	delete self->ss;
    self->ob_type->tp_free((PyObject*)self);
}


PyObject * StrokeShader___repr__(BPy_StrokeShader* self)
{
    return PyString_FromFormat("type: %s - address: %p", self->ss->getName().c_str(), self->ss );
}


PyObject * StrokeShader_getName( BPy_StrokeShader *self, PyObject *args)
{
	return PyString_FromString( self->ss->getName().c_str() );
}

PyObject *StrokeShader_shade( BPy_StrokeShader *self , PyObject *args) {
	PyObject *py_s = 0;

	if(!( PyArg_ParseTuple(args, "O", &py_s) && BPy_Stroke_Check(py_s) )) {
		cout << "ERROR: StrokeShader_shade" << endl;
		Py_RETURN_NONE;
	}
	
	self->ss->shade(*( ((BPy_Stroke *) py_s)->s ));

	Py_RETURN_NONE;
}



///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
