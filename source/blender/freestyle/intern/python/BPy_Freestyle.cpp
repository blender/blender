/* SPDX-FileCopyrightText: 2008-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

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

#include "BKE_appdir.hh"
#include "DNA_scene_types.h"
#include "FRS_freestyle.h"
#include "RNA_access.hh"
#include "RNA_prototypes.hh"
#include "bpy_rna.hh" /* pyrna_struct_CreatePyObject() */

#include "../generic/py_capi_utils.hh" /* #PyC_UnicodeFromBytes */

#include "BKE_colorband.hh"  /* BKE_colorband_evaluate() */
#include "BKE_colortools.hh" /* BKE_curvemapping_evaluateF() */
#include "BKE_material.hh"   /* ramp_blend() */

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------ MODULE FUNCTIONS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    Freestyle_getCurrentScene___doc__,
    ".. function:: getCurrentScene()\n"
    "\n"
    "   Returns the current scene.\n"
    "\n"
    "   :return: The current scene.\n"
    "   :rtype: :class:`bpy.types.Scene`\n");
static PyObject *Freestyle_getCurrentScene(PyObject * /*self*/)
{
  Scene *scene = g_freestyle.scene;
  if (!scene) {
    PyErr_SetString(PyExc_TypeError, "current scene not available");
    return nullptr;
  }
  PointerRNA ptr_scene = RNA_pointer_create_discrete(&scene->id, &RNA_Scene, scene);
  return pyrna_struct_CreatePyObject(&ptr_scene);
}

#include "DNA_material_types.h"

static int ramp_blend_type(const char *type)
{
  if (STREQ(type, "MIX")) {
    return MA_RAMP_BLEND;
  }
  if (STREQ(type, "ADD")) {
    return MA_RAMP_ADD;
  }
  if (STREQ(type, "MULTIPLY")) {
    return MA_RAMP_MULT;
  }
  if (STREQ(type, "SUBTRACT")) {
    return MA_RAMP_SUB;
  }
  if (STREQ(type, "SCREEN")) {
    return MA_RAMP_SCREEN;
  }
  if (STREQ(type, "DIVIDE")) {
    return MA_RAMP_DIV;
  }
  if (STREQ(type, "DIFFERENCE")) {
    return MA_RAMP_DIFF;
  }
  if (STREQ(type, "EXCLUSION")) {
    return MA_RAMP_EXCLUSION;
  }
  if (STREQ(type, "DARKEN")) {
    return MA_RAMP_DARK;
  }
  if (STREQ(type, "LIGHTEN")) {
    return MA_RAMP_LIGHT;
  }
  if (STREQ(type, "OVERLAY")) {
    return MA_RAMP_OVERLAY;
  }
  if (STREQ(type, "DODGE")) {
    return MA_RAMP_DODGE;
  }
  if (STREQ(type, "BURN")) {
    return MA_RAMP_BURN;
  }
  if (STREQ(type, "HUE")) {
    return MA_RAMP_HUE;
  }
  if (STREQ(type, "SATURATION")) {
    return MA_RAMP_SAT;
  }
  if (STREQ(type, "VALUE")) {
    return MA_RAMP_VAL;
  }
  if (STREQ(type, "COLOR")) {
    return MA_RAMP_COLOR;
  }
  if (STREQ(type, "SOFT_LIGHT")) {
    return MA_RAMP_SOFT;
  }
  if (STREQ(type, "LINEAR_LIGHT")) {
    return MA_RAMP_LINEAR;
  }
  return -1;
}

PyDoc_STRVAR(
    /* Wrap. */
    Freestyle_blendRamp___doc__,
    ".. function:: blendRamp(type, color1, fac, color2)\n"
    "\n"
    "   Blend two colors according to a ramp blend type.\n"
    "\n"
    "   :arg type: Ramp blend type.\n"
    "   :type type: int\n"
    "   :arg color1: 1st color.\n"
    "   :type color1: :class:`mathutils.Vector` | tuple[float, float, float] | list[float]\n"
    "   :arg fac: Blend factor.\n"
    "   :type fac: float\n"
    "   :arg color2: 1st color.\n"
    "   :type color2: :class:`mathutils.Vector` | tuple[float, float, float] | list[float]\n"
    "   :return: Blended color in RGB format.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *Freestyle_blendRamp(PyObject * /*self*/, PyObject *args)
{
  PyObject *obj1, *obj2;
  char *s;
  int type;
  float a[4], fac, b[4];

  if (!PyArg_ParseTuple(args, "sOfO", &s, &obj1, &fac, &obj2)) {
    return nullptr;
  }
  type = ramp_blend_type(s);
  if (type < 0) {
    PyErr_SetString(PyExc_TypeError, "argument 1 is an unknown ramp blend type");
    return nullptr;
  }
  if (mathutils_array_parse(a,
                            3,
                            3,
                            obj1,
                            "argument 2 must be a 3D vector "
                            "(either a tuple/list of 3 elements or Vector)") == -1)
  {
    return nullptr;
  }
  if (mathutils_array_parse(b,
                            3,
                            3,
                            obj2,
                            "argument 4 must be a 3D vector "
                            "(either a tuple/list of 3 elements or Vector)") == -1)
  {
    return nullptr;
  }
  ramp_blend(type, a, fac, b);
  return Vector_CreatePyObject(a, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    Freestyle_evaluateColorRamp___doc__,
    ".. function:: evaluateColorRamp(ramp, in)\n"
    "\n"
    "   Evaluate a color ramp at a point in the interval 0 to 1.\n"
    "\n"
    "   :arg ramp: Color ramp object.\n"
    "   :type ramp: :class:`bpy.types.ColorRamp`\n"
    "   :arg in: Value in the interval 0 to 1.\n"
    "   :type in: float\n"
    "   :return: color in RGBA format.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *Freestyle_evaluateColorRamp(PyObject * /*self*/, PyObject *args)
{
  BPy_StructRNA *py_srna;
  ColorBand *coba;
  float in, out[4];

  if (!PyArg_ParseTuple(args, "O!f", &pyrna_struct_Type, &py_srna, &in)) {
    return nullptr;
  }
  if (!RNA_struct_is_a(py_srna->ptr->type, &RNA_ColorRamp)) {
    PyErr_SetString(PyExc_TypeError, "1st argument is not a ColorRamp object");
    return nullptr;
  }
  coba = (ColorBand *)py_srna->ptr->data;
  if (!BKE_colorband_evaluate(coba, in, out)) {
    PyErr_SetString(PyExc_ValueError, "failed to evaluate the color ramp");
    return nullptr;
  }
  return Vector_CreatePyObject(out, 4, nullptr);
}

#include "DNA_color_types.h"

PyDoc_STRVAR(
    /* Wrap. */
    Freestyle_evaluateCurveMappingF___doc__,
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
    "   :rtype: float\n");
static PyObject *Freestyle_evaluateCurveMappingF(PyObject * /*self*/, PyObject *args)
{
  BPy_StructRNA *py_srna;
  CurveMapping *cumap;
  int cur;
  float value;

  if (!PyArg_ParseTuple(args, "O!if", &pyrna_struct_Type, &py_srna, &cur, &value)) {
    return nullptr;
  }
  if (!RNA_struct_is_a(py_srna->ptr->type, &RNA_CurveMapping)) {
    PyErr_SetString(PyExc_TypeError, "1st argument is not a CurveMapping object");
    return nullptr;
  }
  if (cur < 0 || cur > 3) {
    PyErr_SetString(PyExc_ValueError, "2nd argument is out of range");
    return nullptr;
  }
  cumap = (CurveMapping *)py_srna->ptr->data;
  BKE_curvemapping_init(cumap);
  /* disable extrapolation if enabled */
  if (cumap->flag & CUMA_EXTEND_EXTRAPOLATE) {
    cumap->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
    BKE_curvemapping_changed(cumap, false);
  }
  return PyFloat_FromDouble(BKE_curvemapping_evaluateF(cumap, cur, value));
}

/*-----------------------Freestyle module docstring----------------------------*/

PyDoc_STRVAR(
    /* Force wrapped line. */
    module_docstring,
    "This module provides classes for defining line drawing rules (such as\n"
    "predicates, functions, chaining iterators, and stroke shaders), as well\n"
    "as helper functions for style module writing.\n"
    "\n"
    "Class hierarchy:\n"
    "\n"
    "- :class:`BBox`\n"
    "- :class:`BinaryPredicate0D`\n"
    "- :class:`BinaryPredicate1D`\n"
    "\n"
    "  - :class:`FalseBP1D`\n"
    "  - :class:`Length2DBP1D`\n"
    "  - :class:`SameShapeIdBP1D`\n"
    "  - :class:`TrueBP1D`\n"
    "  - :class:`ViewMapGradientNormBP1D`\n"
    "\n"
    "- :class:`Id`\n"
    "- :class:`Interface0D`\n"
    "\n"
    "  - :class:`CurvePoint`\n"
    "\n"
    "    - :class:`StrokeVertex`\n"
    "\n"
    "  - :class:`SVertex`\n"
    "  - :class:`ViewVertex`\n"
    "\n"
    "    - :class:`NonTVertex`\n"
    "    - :class:`TVertex`\n"
    "\n"
    "- :class:`Interface1D`\n"
    "\n"
    "  - :class:`Curve`\n"
    "\n"
    "    - :class:`Chain`\n"
    "\n"
    "  - :class:`FEdge`\n"
    "\n"
    "    - :class:`FEdgeSharp`\n"
    "    - :class:`FEdgeSmooth`\n"
    "\n"
    "  - :class:`Stroke`\n"
    "  - :class:`ViewEdge`\n"
    "\n"
    "- :class:`Iterator`\n"
    "\n"
    "  - :class:`AdjacencyIterator`\n"
    "  - :class:`CurvePointIterator`\n"
    "  - :class:`Interface0DIterator`\n"
    "  - :class:`SVertexIterator`\n"
    "  - :class:`StrokeVertexIterator`\n"
    "  - :class:`ViewEdgeIterator`\n"
    "\n"
    "    - :class:`ChainingIterator`\n"
    "\n"
    "      - :class:`ChainPredicateIterator`\n"
    "      - :class:`ChainSilhouetteIterator`\n"
    "\n"
    "  - :class:`orientedViewEdgeIterator`\n"
    "\n"
    "- :class:`Material`\n"
    "- :class:`Noise`\n"
    "- :class:`Operators`\n"
    "- :class:`SShape`\n"
    "- :class:`StrokeAttribute`\n"
    "- :class:`StrokeShader`\n"
    "\n"
    "  - :class:`BackboneStretcherShader`\n"
    "  - :class:`BezierCurveShader`\n"
    "  - :class:`BlenderTextureShader`\n"
    "  - :class:`CalligraphicShader`\n"
    "  - :class:`ColorNoiseShader`\n"
    "  - :class:`ColorVariationPatternShader`\n"
    "  - :class:`ConstantColorShader`\n"
    "  - :class:`ConstantThicknessShader`\n"
    "  - :class:`ConstrainedIncreasingThicknessShader`\n"
    "  - :class:`GuidingLinesShader`\n"
    "  - :class:`IncreasingColorShader`\n"
    "  - :class:`IncreasingThicknessShader`\n"
    "  - :class:`PolygonalizationShader`\n"
    "  - :class:`SamplingShader`\n"
    "  - :class:`SmoothingShader`\n"
    "  - :class:`SpatialNoiseShader`\n"
    "  - :class:`StrokeTextureShader`\n"
    "  - :class:`StrokeTextureStepShader`\n"
    "  - :class:`TextureAssignerShader`\n"
    "  - :class:`ThicknessNoiseShader`\n"
    "  - :class:`ThicknessVariationPatternShader`\n"
    "  - :class:`TipRemoverShader`\n"
    "  - :class:`fstreamShader`\n"
    "  - :class:`streamShader`\n"
    "\n"
    "- :class:`UnaryFunction0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DDouble`\n"
    "\n"
    "    - :class:`Curvature2DAngleF0D`\n"
    "    - :class:`DensityF0D`\n"
    "    - :class:`GetProjectedXF0D`\n"
    "    - :class:`GetProjectedYF0D`\n"
    "    - :class:`GetProjectedZF0D`\n"
    "    - :class:`GetXF0D`\n"
    "    - :class:`GetYF0D`\n"
    "    - :class:`GetZF0D`\n"
    "    - :class:`LocalAverageDepthF0D`\n"
    "    - :class:`ZDiscontinuityF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DEdgeNature`\n"
    "\n"
    "    - :class:`CurveNatureF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DFloat`\n"
    "\n"
    "    - :class:`GetCurvilinearAbscissaF0D`\n"
    "    - :class:`GetParameterF0D`\n"
    "    - :class:`GetViewMapGradientNormF0D`\n"
    "    - :class:`ReadCompleteViewMapPixelF0D`\n"
    "    - :class:`ReadMapPixelF0D`\n"
    "    - :class:`ReadSteerableViewMapPixelF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DId`\n"
    "\n"
    "    - :class:`ShapeIdF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DMaterial`\n"
    "\n"
    "    - :class:`MaterialF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DUnsigned`\n"
    "\n"
    "    - :class:`QuantitativeInvisibilityF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DVec2f`\n"
    "\n"
    "    - :class:`Normal2DF0D`\n"
    "    - :class:`VertexOrientation2DF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DVec3f`\n"
    "\n"
    "    - :class:`VertexOrientation3DF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DVectorViewShape`\n"
    "\n"
    "    - :class:`GetOccludersF0D`\n"
    "\n"
    "  - :class:`UnaryFunction0DViewShape`\n"
    "\n"
    "    - :class:`GetOccludeeF0D`\n"
    "    - :class:`GetShapeF0D`\n"
    "\n"
    "- :class:`UnaryFunction1D`\n"
    "\n"
    "  - :class:`UnaryFunction1DDouble`\n"
    "\n"
    "    - :class:`Curvature2DAngleF1D`\n"
    "    - :class:`DensityF1D`\n"
    "    - :class:`GetCompleteViewMapDensityF1D`\n"
    "    - :class:`GetDirectionalViewMapDensityF1D`\n"
    "    - :class:`GetProjectedXF1D`\n"
    "    - :class:`GetProjectedYF1D`\n"
    "    - :class:`GetProjectedZF1D`\n"
    "    - :class:`GetSteerableViewMapDensityF1D`\n"
    "    - :class:`GetViewMapGradientNormF1D`\n"
    "    - :class:`GetXF1D`\n"
    "    - :class:`GetYF1D`\n"
    "    - :class:`GetZF1D`\n"
    "    - :class:`LocalAverageDepthF1D`\n"
    "    - :class:`ZDiscontinuityF1D`\n"
    "\n"
    "  - :class:`UnaryFunction1DEdgeNature`\n"
    "\n"
    "    - :class:`CurveNatureF1D`\n"
    "\n"
    "  - :class:`UnaryFunction1DFloat`\n"
    "  - :class:`UnaryFunction1DUnsigned`\n"
    "\n"
    "    - :class:`QuantitativeInvisibilityF1D`\n"
    "\n"
    "  - :class:`UnaryFunction1DVec2f`\n"
    "\n"
    "    - :class:`Normal2DF1D`\n"
    "    - :class:`Orientation2DF1D`\n"
    "\n"
    "  - :class:`UnaryFunction1DVec3f`\n"
    "\n"
    "    - :class:`Orientation3DF1D`\n"
    "\n"
    "  - :class:`UnaryFunction1DVectorViewShape`\n"
    "\n"
    "    - :class:`GetOccludeeF1D`\n"
    "    - :class:`GetOccludersF1D`\n"
    "    - :class:`GetShapeF1D`\n"
    "\n"
    "  - :class:`UnaryFunction1DVoid`\n"
    "\n"
    "    - :class:`ChainingTimeStampF1D`\n"
    "    - :class:`IncrementChainingTimeStampF1D`\n"
    "    - :class:`TimeStampF1D`\n"
    "\n"
    "- :class:`UnaryPredicate0D`\n"
    "\n"
    "  - :class:`FalseUP0D`\n"
    "  - :class:`TrueUP0D`\n"
    "\n"
    "- :class:`UnaryPredicate1D`\n"
    "\n"
    "  - :class:`ContourUP1D`\n"
    "  - :class:`DensityLowerThanUP1D`\n"
    "  - :class:`EqualToChainingTimeStampUP1D`\n"
    "  - :class:`EqualToTimeStampUP1D`\n"
    "  - :class:`ExternalContourUP1D`\n"
    "  - :class:`FalseUP1D`\n"
    "  - :class:`QuantitativeInvisibilityUP1D`\n"
    "  - :class:`ShapeUP1D`\n"
    "  - :class:`TrueUP1D`\n"
    "  - :class:`WithinImageBoundaryUP1D`\n"
    "\n"
    "- :class:`ViewMap`\n"
    "- :class:`ViewShape`\n"
    "- :class:`IntegrationType`\n"
    "- :class:`MediumType`\n"
    "- :class:`Nature`\n"
    "\n");

/*-----------------------Freestyle module method def---------------------------*/

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef module_functions[] = {
    {"getCurrentScene",
     (PyCFunction)Freestyle_getCurrentScene,
     METH_NOARGS,
     Freestyle_getCurrentScene___doc__},
    {"blendRamp", (PyCFunction)Freestyle_blendRamp, METH_VARARGS, Freestyle_blendRamp___doc__},
    {"evaluateColorRamp",
     (PyCFunction)Freestyle_evaluateColorRamp,
     METH_VARARGS,
     Freestyle_evaluateColorRamp___doc__},
    {"evaluateCurveMappingF",
     (PyCFunction)Freestyle_evaluateCurveMappingF,
     METH_VARARGS,
     Freestyle_evaluateCurveMappingF___doc__},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/*-----------------------Freestyle module definition---------------------------*/

static PyModuleDef module_definition = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "_freestyle",
    /*m_doc*/ module_docstring,
    /*m_size*/ -1,
    /*m_methods*/ module_functions,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

//-------------------MODULE INITIALIZATION--------------------------------
PyObject *Freestyle_Init()
{
  PyObject *module;

  // initialize modules
  module = PyModule_Create(&module_definition);
  if (!module) {
    return nullptr;
  }
  PyDict_SetItemString(PySys_GetObject("modules"), module_definition.m_name, module);

  // update 'sys.path' for Freestyle Python API modules
  const std::optional<std::string> path = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS,
                                                               "freestyle");
  if (path.has_value()) {
    char modpath[FILE_MAX];
    BLI_path_join(modpath, sizeof(modpath), path->c_str(), "modules");
    PyObject *sys_path = PySys_GetObject("path"); /* borrow */
    PyObject *py_modpath = PyC_UnicodeFromBytes(modpath);
    PyList_Append(sys_path, py_modpath);
    Py_DECREF(py_modpath);
#if 0
    printf("Adding Python path: %s\n", modpath);
#endif
  }
  else {
    printf(
        "Freestyle: couldn't find 'scripts/freestyle/modules', Freestyle won't work properly.\n");
  }

  // attach its classes (adding the object types to the module)

  // those classes have to be initialized before the others
  MediumType_Init(module);
  Nature_Init(module);

  BBox_Init(module);
  BinaryPredicate0D_Init(module);
  BinaryPredicate1D_Init(module);
  ContextFunctions_Init(module);
  FrsMaterial_Init(module);
  FrsNoise_Init(module);
  Id_Init(module);
  IntegrationType_Init(module);
  Interface0D_Init(module);
  Interface1D_Init(module);
  Iterator_Init(module);
  Operators_Init(module);
  SShape_Init(module);
  StrokeAttribute_Init(module);
  StrokeShader_Init(module);
  UnaryFunction0D_Init(module);
  UnaryFunction1D_Init(module);
  UnaryPredicate0D_Init(module);
  UnaryPredicate1D_Init(module);
  ViewMap_Init(module);
  ViewShape_Init(module);

  return module;
}

///////////////////////////////////////////////////////////////////////////////////////////
