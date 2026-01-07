/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup DNA
 */

#include "BLI_math_constants.h"

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_texture_types.h"

namespace blender {

#ifndef MAX_MTEX
#  define MAX_MTEX 18
#endif

/* texco (also in DNA_material_types.h) */
#define TEXCO_STROKE 16 /* actually its UV */

struct AnimData;
struct ColorBand;
struct CurveMapping;
struct MTex;
struct Object;
struct bNodeTree;

/** #LineStyleModifier::type */
enum {
  LS_MODIFIER_ALONG_STROKE = 1,
  LS_MODIFIER_DISTANCE_FROM_CAMERA = 2,
  LS_MODIFIER_DISTANCE_FROM_OBJECT = 3,
  LS_MODIFIER_MATERIAL = 4,
  LS_MODIFIER_SAMPLING = 5,
  LS_MODIFIER_BEZIER_CURVE = 6,
  LS_MODIFIER_SINUS_DISPLACEMENT = 7,
  LS_MODIFIER_SPATIAL_NOISE = 8,
  LS_MODIFIER_PERLIN_NOISE_1D = 9,
  LS_MODIFIER_PERLIN_NOISE_2D = 10,
  LS_MODIFIER_BACKBONE_STRETCHER = 11,
  LS_MODIFIER_TIP_REMOVER = 12,
  LS_MODIFIER_CALLIGRAPHY = 13,
  LS_MODIFIER_POLYGONIZATION = 14,
  LS_MODIFIER_GUIDING_LINES = 15,
  LS_MODIFIER_BLUEPRINT = 16,
  LS_MODIFIER_2D_OFFSET = 17,
  LS_MODIFIER_2D_TRANSFORM = 18,
  LS_MODIFIER_TANGENT = 19,
  LS_MODIFIER_NOISE = 20,
  LS_MODIFIER_CREASE_ANGLE = 21,
  LS_MODIFIER_SIMPLIFICATION = 22,
  LS_MODIFIER_CURVATURE_3D = 23,
  LS_MODIFIER_NUM = 24,
};

/** #LineStyleModifier::flags */
enum {
  LS_MODIFIER_ENABLED = 1,
  LS_MODIFIER_EXPANDED = 2,
};

/** Flags (for color) */
enum {
  LS_MODIFIER_USE_RAMP = 1,
};

/** Flags (for alpha & thickness) */
enum {
  LS_MODIFIER_USE_CURVE = 1,
  LS_MODIFIER_INVERT = 2,
};

/** Flags (for asymmetric thickness application). */
enum {
  LS_THICKNESS_ASYMMETRIC = 1,
};

/** Blend (for alpha & thickness). */
enum {
  LS_VALUE_BLEND = 0,
  LS_VALUE_ADD = 1,
  LS_VALUE_MULT = 2,
  LS_VALUE_SUB = 3,
  LS_VALUE_DIV = 4,
  LS_VALUE_DIFF = 5,
  LS_VALUE_MIN = 6,
  LS_VALUE_MAX = 7,
};

/* mat_attr */
enum {
  LS_MODIFIER_MATERIAL_DIFF = 1,
  LS_MODIFIER_MATERIAL_DIFF_R = 2,
  LS_MODIFIER_MATERIAL_DIFF_G = 3,
  LS_MODIFIER_MATERIAL_DIFF_B = 4,
  LS_MODIFIER_MATERIAL_SPEC = 5,
  LS_MODIFIER_MATERIAL_SPEC_R = 6,
  LS_MODIFIER_MATERIAL_SPEC_G = 7,
  LS_MODIFIER_MATERIAL_SPEC_B = 8,
  LS_MODIFIER_MATERIAL_SPEC_HARD = 9,
  LS_MODIFIER_MATERIAL_ALPHA = 10,
  LS_MODIFIER_MATERIAL_LINE = 11,
  LS_MODIFIER_MATERIAL_LINE_R = 12,
  LS_MODIFIER_MATERIAL_LINE_G = 13,
  LS_MODIFIER_MATERIAL_LINE_B = 14,
  LS_MODIFIER_MATERIAL_LINE_A = 15,
};

/** #LineStyleGeometryModifier_SpatialNoise::flags */
enum {
  LS_MODIFIER_SPATIAL_NOISE_SMOOTH = 1,
  LS_MODIFIER_SPATIAL_NOISE_PURERANDOM = 2,
};

/** #LineStyleGeometryModifier_BluePrintLines::shape */
enum {
  LS_MODIFIER_BLUEPRINT_CIRCLES = 1,
  LS_MODIFIER_BLUEPRINT_ELLIPSES = 2,
  LS_MODIFIER_BLUEPRINT_SQUARES = 4,
};

/** #LineStyleGeometryModifier_2DTransform::pivot */
enum {
  LS_MODIFIER_2D_TRANSFORM_PIVOT_CENTER = 1,
  LS_MODIFIER_2D_TRANSFORM_PIVOT_START = 2,
  LS_MODIFIER_2D_TRANSFORM_PIVOT_END = 3,
  LS_MODIFIER_2D_TRANSFORM_PIVOT_PARAM = 4,
  LS_MODIFIER_2D_TRANSFORM_PIVOT_ABSOLUTE = 5,
};

/** #FreestyleLineStyle::panel */
enum {
  LS_PANEL_STROKES = 1,
  LS_PANEL_COLOR = 2,
  LS_PANEL_ALPHA = 3,
  LS_PANEL_THICKNESS = 4,
  LS_PANEL_GEOMETRY = 5,
  LS_PANEL_TEXTURE = 6,
  LS_PANEL_MISC = 7,
};

/** #FreestyleLineStyle::flag */
enum {
  LS_DS_EXPAND = 1 << 0, /* for animation editors */
  LS_SAME_OBJECT = 1 << 1,
  LS_DASHED_LINE = 1 << 2,
  LS_MATERIAL_BOUNDARY = 1 << 3,
  LS_MIN_2D_LENGTH = 1 << 4,
  LS_MAX_2D_LENGTH = 1 << 5,
  LS_NO_CHAINING = 1 << 6,
  LS_MIN_2D_ANGLE = 1 << 7,
  LS_MAX_2D_ANGLE = 1 << 8,
  LS_SPLIT_LENGTH = 1 << 9,
  LS_SPLIT_PATTERN = 1 << 10,
  LS_NO_SORTING = 1 << 11,
  LS_REVERSE_ORDER = 1 << 12, /* for sorting */
  LS_TEXTURE = 1 << 13,
  LS_CHAIN_COUNT = 1 << 14,
};

/** #FreestyleLineStyle::chaining */
enum {
  LS_CHAINING_PLAIN = 1,
  LS_CHAINING_SKETCHY = 2,
};

/** #FreestyleLineStyle::caps */
enum {
  LS_CAPS_BUTT = 1,
  LS_CAPS_ROUND = 2,
  LS_CAPS_SQUARE = 3,
};

/** #FreestyleLineStyle::thickness_position */
enum {
  LS_THICKNESS_CENTER = 1,
  LS_THICKNESS_INSIDE = 2,
  LS_THICKNESS_OUTSIDE = 3,
  /** Thickness_ratio is used. */
  LS_THICKNESS_RELATIVE = 4,
};

/** #FreestyleLineStyle::sort_key */
enum {
  LS_SORT_KEY_DISTANCE_FROM_CAMERA = 1,
  LS_SORT_KEY_2D_LENGTH = 2,
  LS_SORT_KEY_PROJECTED_X = 3,
  LS_SORT_KEY_PROJECTED_Y = 4,
};

/** #FreestyleLineStyle::integration_type */
enum {
  LS_INTEGRATION_MEAN = 1,
  LS_INTEGRATION_MIN = 2,
  LS_INTEGRATION_MAX = 3,
  LS_INTEGRATION_FIRST = 4,
  LS_INTEGRATION_LAST = 5,
};

struct LineStyleModifier {
  DNA_DEFINE_CXX_METHODS(LineStyleModifier)

  struct LineStyleModifier *next = nullptr, *prev = nullptr;

  char name[/*MAX_NAME*/ 64] = "";
  int type = 0;
  float influence = 0;
  int flags = 0;
  int blend = 0;
};

/* Along Stroke modifiers */

struct LineStyleColorModifier_AlongStroke {
  DNA_DEFINE_CXX_METHODS(LineStyleColorModifier_AlongStroke)

  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp = nullptr;
};

struct LineStyleAlphaModifier_AlongStroke {
  DNA_DEFINE_CXX_METHODS(LineStyleAlphaModifier_AlongStroke)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  char _pad[4] = {};
};

struct LineStyleThicknessModifier_AlongStroke {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_AlongStroke)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float value_min = 0, value_max = 0;
  char _pad[4] = {};
};

/* Distance from Camera modifiers */

struct LineStyleColorModifier_DistanceFromCamera {
  DNA_DEFINE_CXX_METHODS(LineStyleColorModifier_DistanceFromCamera)

  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp = nullptr;
  float range_min = 0, range_max = 0;
};

struct LineStyleAlphaModifier_DistanceFromCamera {
  DNA_DEFINE_CXX_METHODS(LineStyleAlphaModifier_DistanceFromCamera)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float range_min = 0, range_max = 0;
  char _pad[4] = {};
};

struct LineStyleThicknessModifier_DistanceFromCamera {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_DistanceFromCamera)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float range_min = 0, range_max = 0;
  float value_min = 0, value_max = 0;
  char _pad[4] = {};
};

/* Distance from Object modifiers */

struct LineStyleColorModifier_DistanceFromObject {
  DNA_DEFINE_CXX_METHODS(LineStyleColorModifier_DistanceFromObject)

  struct LineStyleModifier modifier;

  struct Object *target = nullptr;
  struct ColorBand *color_ramp = nullptr;
  float range_min = 0, range_max = 0;
};

struct LineStyleAlphaModifier_DistanceFromObject {
  DNA_DEFINE_CXX_METHODS(LineStyleAlphaModifier_DistanceFromObject)

  struct LineStyleModifier modifier;

  struct Object *target = nullptr;
  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float range_min = 0, range_max = 0;
  char _pad[4] = {};
};

struct LineStyleThicknessModifier_DistanceFromObject {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_DistanceFromObject)

  struct LineStyleModifier modifier;

  struct Object *target = nullptr;
  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float range_min = 0, range_max = 0;
  float value_min = 0, value_max = 0;
  char _pad[4] = {};
};

/* 3D curvature modifiers */

struct LineStyleColorModifier_Curvature_3D {
  DNA_DEFINE_CXX_METHODS(LineStyleColorModifier_Curvature_3D)

  struct LineStyleModifier modifier;

  float min_curvature = 0, max_curvature = 0;
  struct ColorBand *color_ramp = nullptr;
  float range_min = 0, range_max = 0;
};

struct LineStyleAlphaModifier_Curvature_3D {
  DNA_DEFINE_CXX_METHODS(LineStyleAlphaModifier_Curvature_3D)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float min_curvature = 0, max_curvature = 0;
  char _pad[4] = {};
};

struct LineStyleThicknessModifier_Curvature_3D {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_Curvature_3D)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  char _pad[4] = {};
  float min_curvature = 0, max_curvature = 0;
  float min_thickness = 0, max_thickness = 0;
};

/* Noise modifiers (for color, alpha and thickness) */

struct LineStyleColorModifier_Noise {
  DNA_DEFINE_CXX_METHODS(LineStyleColorModifier_Noise)

  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp = nullptr;
  float period = 0, amplitude = 0;
  int seed = 0;
  char _pad[4] = {};
};

struct LineStyleAlphaModifier_Noise {
  DNA_DEFINE_CXX_METHODS(LineStyleAlphaModifier_Noise)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float period = 0, amplitude = 0;
  int seed = 0;
};

struct LineStyleThicknessModifier_Noise {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_Noise)

  struct LineStyleModifier modifier;

  float period = 0, amplitude = 0;
  int flags = 0;
  int seed = 0;
};

/* Crease Angle modifiers */

struct LineStyleColorModifier_CreaseAngle {
  DNA_DEFINE_CXX_METHODS(LineStyleColorModifier_CreaseAngle)

  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp = nullptr;
  float min_angle = 0, max_angle = 0;
};

struct LineStyleAlphaModifier_CreaseAngle {
  DNA_DEFINE_CXX_METHODS(LineStyleAlphaModifier_CreaseAngle)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float min_angle = 0, max_angle = 0;
  char _pad[4] = {};
};

struct LineStyleThicknessModifier_CreaseAngle {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_CreaseAngle)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  char _pad[4] = {};
  float min_angle = 0, max_angle = 0;
  float min_thickness = 0, max_thickness = 0;
};

/* Tangent modifiers */

struct LineStyleColorModifier_Tangent {
  DNA_DEFINE_CXX_METHODS(LineStyleColorModifier_Tangent)

  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp = nullptr;
};

struct LineStyleAlphaModifier_Tangent {
  DNA_DEFINE_CXX_METHODS(LineStyleAlphaModifier_Tangent)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  char _pad[4] = {};
};

struct LineStyleThicknessModifier_Tangent {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_Tangent)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float min_thickness = 0, max_thickness = 0;
  char _pad[4] = {};
};

/* Material modifiers */

struct LineStyleColorModifier_Material {
  DNA_DEFINE_CXX_METHODS(LineStyleColorModifier_Material)

  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp = nullptr;
  int flags = 0;
  int mat_attr = 0;
};

struct LineStyleAlphaModifier_Material {
  DNA_DEFINE_CXX_METHODS(LineStyleAlphaModifier_Material)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  int mat_attr = 0;
};

struct LineStyleThicknessModifier_Material {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_Material)

  struct LineStyleModifier modifier;

  struct CurveMapping *curve = nullptr;
  int flags = 0;
  float value_min = 0, value_max = 0;
  int mat_attr = 0;
};

/* Geometry modifiers */

struct LineStyleGeometryModifier_Sampling {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_Sampling)

  struct LineStyleModifier modifier;

  float sampling = 0;
  char _pad[4] = {};
};

struct LineStyleGeometryModifier_BezierCurve {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_BezierCurve)

  struct LineStyleModifier modifier;

  float error = 0;
  char _pad[4] = {};
};

struct LineStyleGeometryModifier_SinusDisplacement {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_SinusDisplacement)

  struct LineStyleModifier modifier;

  float wavelength = 0, amplitude = 0, phase = 0;
  char _pad[4] = {};
};

struct LineStyleGeometryModifier_SpatialNoise {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_SpatialNoise)

  struct LineStyleModifier modifier;

  float amplitude = 0, scale = 0;
  unsigned int octaves = 0;
  int flags = 0;
};

struct LineStyleGeometryModifier_PerlinNoise1D {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_PerlinNoise1D)

  struct LineStyleModifier modifier;

  float frequency = 0, amplitude = 0;
  /** In radians. */
  float angle = 0;
  unsigned int octaves = 0;
  int seed = 0;
  char _pad1[4] = {};
};

struct LineStyleGeometryModifier_PerlinNoise2D {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_PerlinNoise2D)

  struct LineStyleModifier modifier;

  float frequency = 0, amplitude = 0;
  /** In radians. */
  float angle = 0;
  unsigned int octaves = 0;
  int seed = 0;
  char _pad1[4] = {};
};

struct LineStyleGeometryModifier_BackboneStretcher {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_BackboneStretcher)

  struct LineStyleModifier modifier;

  float backbone_length = 0;
  char _pad[4] = {};
};

struct LineStyleGeometryModifier_TipRemover {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_TipRemover)

  struct LineStyleModifier modifier;

  float tip_length = 0;
  char _pad[4] = {};
};

struct LineStyleGeometryModifier_Polygonalization {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_Polygonalization)

  struct LineStyleModifier modifier;

  float error = 0;
  char _pad[4] = {};
};

struct LineStyleGeometryModifier_GuidingLines {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_GuidingLines)

  struct LineStyleModifier modifier;

  float offset = 0;
  char _pad[4] = {};
};

struct LineStyleGeometryModifier_Blueprint {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_Blueprint)

  struct LineStyleModifier modifier;

  int flags = 0;
  unsigned int rounds = 0;
  float backbone_length = 0;
  unsigned int random_radius = 0;
  unsigned int random_center = 0;
  unsigned int random_backbone = 0;
};

struct LineStyleGeometryModifier_2DOffset {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_2DOffset)

  struct LineStyleModifier modifier;

  float start = 0, end = 0;
  float x = 0, y = 0;
};

struct LineStyleGeometryModifier_2DTransform {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_2DTransform)

  struct LineStyleModifier modifier;

  int pivot = 0;
  float scale_x = 0, scale_y = 0;
  /** In radians. */
  float angle = 0;
  float pivot_u = 0;
  float pivot_x = 0, pivot_y = 0;
  char _pad[4] = {};
};

struct LineStyleGeometryModifier_Simplification {
  DNA_DEFINE_CXX_METHODS(LineStyleGeometryModifier_Simplification)

  struct LineStyleModifier modifier;

  float tolerance = 0;
  char _pad[4] = {};
};

/* Calligraphic thickness modifier */

struct LineStyleThicknessModifier_Calligraphy {
  DNA_DEFINE_CXX_METHODS(LineStyleThicknessModifier_Calligraphy)

  struct LineStyleModifier modifier;

  float min_thickness = 0, max_thickness = 0;
  /** In radians. */
  float orientation = 0;
  char _pad[4] = {};
};

struct FreestyleLineStyle {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(FreestyleLineStyle)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_LS;
#endif

  ID id;
  struct AnimData *adt = nullptr;

  float r = 0, g = 0, b = 0, alpha = 1.0f;
  float thickness = 3.0f;
  int thickness_position = LS_THICKNESS_CENTER;
  float thickness_ratio = 0.5f;
  int flag = LS_SAME_OBJECT | LS_NO_SORTING | LS_TEXTURE, caps = LS_CAPS_BUTT;
  int chaining = LS_CHAINING_PLAIN;
  unsigned int rounds = 3;
  float split_length = 100;
  /** In radians, for splitting. */
  float min_angle = DEG2RADF(0.0f), max_angle = DEG2RADF(0.0f);
  float min_length = 0.0f, max_length = 10000.0f;
  unsigned int chain_count = 10;
  unsigned short split_dash1 = 0, split_gap1 = 0;
  unsigned short split_dash2 = 0, split_gap2 = 0;
  unsigned short split_dash3 = 0, split_gap3 = 0;
  int sort_key = LS_SORT_KEY_DISTANCE_FROM_CAMERA, integration_type = LS_INTEGRATION_MEAN;
  float texstep = 1.0f;
  short texact = 0, pr_texture = TEX_PR_TEXTURE;
  short use_nodes = 0;
  char _pad[6] = {};
  unsigned short dash1 = 0, gap1 = 0, dash2 = 0, gap2 = 0, dash3 = 0, gap3 = 0;
  /** For UI. */
  int panel = LS_PANEL_STROKES;
  struct MTex *mtex[/*MAX_MTEX*/ 18] = {};
  /* nodes */
  struct bNodeTree *nodetree = nullptr;

  ListBaseT<LineStyleModifier> color_modifiers = {nullptr, nullptr};
  ListBaseT<LineStyleModifier> alpha_modifiers = {nullptr, nullptr};
  ListBaseT<LineStyleModifier> thickness_modifiers = {nullptr, nullptr};
  ListBaseT<LineStyleModifier> geometry_modifiers = {nullptr, nullptr};
};

}  // namespace blender
