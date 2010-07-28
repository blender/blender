#include "BPy_Freestyle.h"

#include "BPy_BBox.h"
#include "BPy_BinaryPredicate0D.h"
#include "BPy_BinaryPredicate1D.h"
#include "BPy_ContextFunctions.h"
#include "BPy_Convert.h"
#include "BPy_FrsMaterial.h"
#include "BPy_FrsNoise.h"
#include "BPy_Id.h"
#include "BPy_IntegrationType.h"
#include "BPy_Interface0D.h"
#include "BPy_Interface1D.h"
#include "BPy_Iterator.h"
#include "BPy_MediumType.h"
#include "BPy_Nature.h"
#include "BPy_Operators.h"
#include "BPy_SShape.h"
#include "BPy_StrokeAttribute.h"
#include "BPy_StrokeShader.h"
#include "BPy_UnaryFunction0D.h"
#include "BPy_UnaryFunction1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "BPy_ViewMap.h"
#include "BPy_ViewShape.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------ MODULE FUNCTIONS ----------------------------------

#include "FRS_freestyle.h"
#include "bpy_rna.h" /* pyrna_struct_CreatePyObject() */

static char Freestyle_getCurrentScene___doc__[] =
".. function:: getCurrentScene()\n"
"\n"
"   Returns the current scene.\n"
"\n"
"   :return: The current scene.\n"
"   :rtype: :class:`bpy.types.Scene`\n";

static PyObject *Freestyle_getCurrentScene( PyObject *self )
{
	if (!freestyle_scene) {
		PyErr_SetString(PyExc_TypeError, "current scene not available");
		return NULL;
	}
	PointerRNA ptr_scene;
	RNA_pointer_create(NULL, &RNA_Scene, freestyle_scene, &ptr_scene);
	return pyrna_struct_CreatePyObject(&ptr_scene);
}

#include "DNA_material_types.h"

static int ramp_blend_type(const char *type)
{
	if (!strcmp(type, "MIX"))           return MA_RAMP_BLEND;
	if (!strcmp(type, "ADD"))           return MA_RAMP_ADD;
	if (!strcmp(type, "MULTIPLY"))      return MA_RAMP_MULT;
	if (!strcmp(type, "SUBTRACT"))      return MA_RAMP_SUB;
	if (!strcmp(type, "SCREEN"))        return MA_RAMP_SCREEN;
	if (!strcmp(type, "DIVIDE"))        return MA_RAMP_DIV;
	if (!strcmp(type, "DIFFERENCE"))    return MA_RAMP_DIFF;
	if (!strcmp(type, "DARKEN"))        return MA_RAMP_DARK;
	if (!strcmp(type, "LIGHTEN"))       return MA_RAMP_LIGHT;
	if (!strcmp(type, "OVERLAY"))       return MA_RAMP_OVERLAY;
	if (!strcmp(type, "DODGE"))         return MA_RAMP_DODGE;
	if (!strcmp(type, "BURN"))          return MA_RAMP_BURN;
	if (!strcmp(type, "HUE"))           return MA_RAMP_HUE;
	if (!strcmp(type, "SATURATION"))    return MA_RAMP_SAT;
	if (!strcmp(type, "VALUE"))         return MA_RAMP_VAL;
	if (!strcmp(type, "COLOR"))         return MA_RAMP_COLOR;
	if (!strcmp(type, "SOFT LIGHT"))    return MA_RAMP_SOFT;
	if (!strcmp(type, "LINEAR LIGHT"))  return MA_RAMP_LINEAR;
	return -1;
}

#include "BKE_material.h" /* ramp_blend() */

static char Freestyle_blendRamp___doc__[] =
".. function:: blendRamp(type, color1, fac, color2)\n"
"\n"
"   Blend two colors according to a ramp blend type.\n"
"\n"
"   :arg type: Ramp blend type.\n"
"   :type type: int\n"
"   :arg color1: 1st color.\n"
"   :type color1: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n"
"   :arg fac: Blend factor.\n"
"   :type fac: float\n"
"   :arg color2: 1st color.\n"
"   :type color2: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n"
"   :return: Blended color in RGB format.\n"
"   :rtype: :class:`mathutils.Vector`\n";

static PyObject *Freestyle_blendRamp( PyObject *self, PyObject *args )
{
	PyObject *obj1, *obj2;
	char *s;
	int type;
	Vec3f *v1 = NULL, *v2 = NULL;
	float a[3], fac, b[3];

	if (!PyArg_ParseTuple(args, "sOfO", &s, &obj1, &fac, &obj2))
		return NULL;
	type = ramp_blend_type(s);
	if (type < 0) {
		PyErr_SetString(PyExc_TypeError, "argument 1 is an unknown ramp blend type");
		goto error;
	}
	v1 = Vec3f_ptr_from_PyObject(obj1);
	if (!v1) {
		PyErr_SetString(PyExc_TypeError, "argument 2 must be a 3D vector (either a tuple/list of 3 elements or Vector)");
		goto error;
	}
	v2 = Vec3f_ptr_from_PyObject(obj2);
	if (!v2) {
		PyErr_SetString(PyExc_TypeError, "argument 4 must be a 3D vector (either a tuple/list of 3 elements or Vector)");
		goto error;
	}
	a[0] = v1->x(); b[0] = v2->x();
	a[1] = v1->y(); b[1] = v2->y();
	a[2] = v1->z(); b[2] = v2->z();
	ramp_blend(type, &a[0], &a[1], &a[2], fac, b);
	delete v1;
	delete v2;
	return newVectorObject( a, 3, Py_NEW, NULL);

error:
	if (v1) delete v1;
	if (v2) delete v2;
	return NULL;
}

#include "BKE_texture.h" /* do_colorband() */

static char Freestyle_evaluateColorRamp___doc__[] =
".. function:: evaluateColorRamp(ramp, in)\n"
"\n"
"   Evaluate a color ramp at a point in the interval 0 to 1.\n"
"\n"
"   :arg ramp: Color ramp object.\n"
"   :type ramp: :class:`bpy.types.ColorRamp`\n"
"   :arg in: Value in the interval 0 to 1.\n"
"   :type in: float\n"
"   :return: color in RGBA format.\n"
"   :rtype: :class:`mathutils.Vector`\n";

static PyObject *Freestyle_evaluateColorRamp( PyObject *self, PyObject *args )
{
	BPy_StructRNA *py_srna;
	ColorBand *coba;
	float in, out[4];

	if(!( PyArg_ParseTuple(args, "O!f", &pyrna_struct_Type, &py_srna, &in) ))
		return NULL;
	if(!RNA_struct_is_a(py_srna->ptr.type, &RNA_ColorRamp)) {
		PyErr_SetString(PyExc_TypeError, "1st argument is not a ColorRamp object");
		return NULL;
	}
	coba = (ColorBand *)py_srna->ptr.data;
	if (!do_colorband(coba, in, out)) {
		PyErr_SetString(PyExc_ValueError, "failed to evaluate the color ramp");
		return NULL;
	}
	return newVectorObject( out, 4, Py_NEW, NULL);
}

#include "BKE_colortools.h" /* curvemapping_evaluateF() */

static char Freestyle_evaluateCurveMappingF___doc__[] =
".. function:: evaluateCurveMappingF(cumap, cur, value)\n"
"\n"
"   Evaluate a curve mapping at a point in the interval 0 to 1.\n"
"\n"
"   :arg cumap: Curve mapping object.\n"
"   :type cumap: :class:`bpy.types.CurveMapping`\n"
"   :arg cur: Index of the curve to be used (0 <= cur <= 3).\n"
"   :type cur: int\n"
"   :arg value: Input value in the interval 0 to 1.\n"
"   :type value: float\n"
"   :return: Mapped output value.\n"
"   :rtype: float\n";

static PyObject *Freestyle_evaluateCurveMappingF( PyObject *self, PyObject *args )
{
	BPy_StructRNA *py_srna;
	CurveMapping *cumap;
	int cur;
	float value;

	if(!( PyArg_ParseTuple(args, "O!if", &pyrna_struct_Type, &py_srna, &cur, &value) ))
		return NULL;
	if(!RNA_struct_is_a(py_srna->ptr.type, &RNA_CurveMapping)) {
		PyErr_SetString(PyExc_TypeError, "1st argument is not a CurveMapping object");
		return NULL;
	}
	if (cur < 0 || cur > 3) {
		PyErr_SetString(PyExc_ValueError, "2nd argument is out of range");
		return NULL;
	}
	cumap = (CurveMapping *)py_srna->ptr.data;
	return PyFloat_FromDouble(curvemapping_evaluateF(cumap, cur, value));
}

/*-----------------------Freestyle module docstring----------------------------*/

static char module_docstring[] = "The Blender Freestyle module\n\n";

/*-----------------------Freestyle module method def---------------------------*/

static PyMethodDef module_functions[] = {
	{"getCurrentScene", ( PyCFunction ) Freestyle_getCurrentScene, METH_NOARGS, Freestyle_getCurrentScene___doc__},
	{"blendRamp", ( PyCFunction ) Freestyle_blendRamp, METH_VARARGS, Freestyle_blendRamp___doc__},
	{"evaluateColorRamp", ( PyCFunction ) Freestyle_evaluateColorRamp, METH_VARARGS, Freestyle_evaluateColorRamp___doc__},
	{"evaluateCurveMappingF", ( PyCFunction ) Freestyle_evaluateCurveMappingF, METH_VARARGS, Freestyle_evaluateCurveMappingF___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------Freestyle module definition---------------------------*/

static PyModuleDef module_definition = {
    PyModuleDef_HEAD_INIT,
    "Freestyle",
    module_docstring,
    -1,
    module_functions
};

//-------------------MODULE INITIALIZATION--------------------------------
PyObject *Freestyle_Init( void )
{
	PyObject *module;
	
	// initialize modules
	module = PyModule_Create(&module_definition);
    if (!module)
		return NULL;
	PyDict_SetItemString(PySys_GetObject("modules"), module_definition.m_name, module);
	
	// attach its classes (adding the object types to the module)
	
	// those classes have to be initialized before the others
	MediumType_Init( module );
	Nature_Init( module );
	
	BBox_Init( module );
	BinaryPredicate0D_Init( module );
	BinaryPredicate1D_Init( module );
	ContextFunctions_Init( module );
	FrsMaterial_Init( module );
	FrsNoise_Init( module );
	Id_Init( module );
	IntegrationType_Init( module );
	Interface0D_Init( module );
	Interface1D_Init( module );
	Iterator_Init( module );
	Operators_Init( module );
	SShape_Init( module );
	StrokeAttribute_Init( module );
	StrokeShader_Init( module );
	UnaryFunction0D_Init( module );
	UnaryFunction1D_Init( module );
	UnaryPredicate0D_Init( module );
	UnaryPredicate1D_Init( module );
	ViewMap_Init( module );
	ViewShape_Init( module );

	return module;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
