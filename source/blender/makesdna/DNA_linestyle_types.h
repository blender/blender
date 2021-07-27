/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup DNA
 */

#include "DNA_ID.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct LineStyleModifier {
  struct LineStyleModifier *next, *prev;

  /** MAX_NAME. */
  char name[64];
  int type;
  float influence;
  int flags;
  int blend;
} LineStyleModifier;

/* LineStyleModifier::type */
#define LS_MODIFIER_ALONG_STROKE 1
#define LS_MODIFIER_DISTANCE_FROM_CAMERA 2
#define LS_MODIFIER_DISTANCE_FROM_OBJECT 3
#define LS_MODIFIER_MATERIAL 4
#define LS_MODIFIER_SAMPLING 5
#define LS_MODIFIER_BEZIER_CURVE 6
#define LS_MODIFIER_SINUS_DISPLACEMENT 7
#define LS_MODIFIER_SPATIAL_NOISE 8
#define LS_MODIFIER_PERLIN_NOISE_1D 9
#define LS_MODIFIER_PERLIN_NOISE_2D 10
#define LS_MODIFIER_BACKBONE_STRETCHER 11
#define LS_MODIFIER_TIP_REMOVER 12
#define LS_MODIFIER_CALLIGRAPHY 13
#define LS_MODIFIER_POLYGONIZATION 14
#define LS_MODIFIER_GUIDING_LINES 15
#define LS_MODIFIER_BLUEPRINT 16
#define LS_MODIFIER_2D_OFFSET 17
#define LS_MODIFIER_2D_TRANSFORM 18
#define LS_MODIFIER_TANGENT 19
#define LS_MODIFIER_NOISE 20
#define LS_MODIFIER_CREASE_ANGLE 21
#define LS_MODIFIER_SIMPLIFICATION 22
#define LS_MODIFIER_CURVATURE_3D 23
#define LS_MODIFIER_NUM 24

/* LineStyleModifier::flags */
#define LS_MODIFIER_ENABLED 1
#define LS_MODIFIER_EXPANDED 2

/* flags (for color) */
#define LS_MODIFIER_USE_RAMP 1

/* flags (for alpha & thickness) */
#define LS_MODIFIER_USE_CURVE 1
#define LS_MODIFIER_INVERT 2

/* flags (for asymmetric thickness application) */
#define LS_THICKNESS_ASYMMETRIC 1

/* blend (for alpha & thickness) */
#define LS_VALUE_BLEND 0
#define LS_VALUE_ADD 1
#define LS_VALUE_MULT 2
#define LS_VALUE_SUB 3
#define LS_VALUE_DIV 4
#define LS_VALUE_DIFF 5
#define LS_VALUE_MIN 6
#define LS_VALUE_MAX 7

/* Along Stroke modifiers */

typedef struct LineStyleColorModifier_AlongStroke {
  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp;
} LineStyleColorModifier_AlongStroke;

typedef struct LineStyleAlphaModifier_AlongStroke {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  char _pad[4];
} LineStyleAlphaModifier_AlongStroke;

typedef struct LineStyleThicknessModifier_AlongStroke {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  float value_min, value_max;
  char _pad[4];
} LineStyleThicknessModifier_AlongStroke;

/* Distance from Camera modifiers */

typedef struct LineStyleColorModifier_DistanceFromCamera {
  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp;
  float range_min, range_max;
} LineStyleColorModifier_DistanceFromCamera;

typedef struct LineStyleAlphaModifier_DistanceFromCamera {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  float range_min, range_max;
  char _pad[4];
} LineStyleAlphaModifier_DistanceFromCamera;

typedef struct LineStyleThicknessModifier_DistanceFromCamera {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  float range_min, range_max;
  float value_min, value_max;
  char _pad[4];
} LineStyleThicknessModifier_DistanceFromCamera;

/* Distance from Object modifiers */

typedef struct LineStyleColorModifier_DistanceFromObject {
  struct LineStyleModifier modifier;

  struct Object *target;
  struct ColorBand *color_ramp;
  float range_min, range_max;
} LineStyleColorModifier_DistanceFromObject;

typedef struct LineStyleAlphaModifier_DistanceFromObject {
  struct LineStyleModifier modifier;

  struct Object *target;
  struct CurveMapping *curve;
  int flags;
  float range_min, range_max;
  char _pad[4];
} LineStyleAlphaModifier_DistanceFromObject;

typedef struct LineStyleThicknessModifier_DistanceFromObject {
  struct LineStyleModifier modifier;

  struct Object *target;
  struct CurveMapping *curve;
  int flags;
  float range_min, range_max;
  float value_min, value_max;
  char _pad[4];
} LineStyleThicknessModifier_DistanceFromObject;

/* 3D curvature modifiers */

typedef struct LineStyleColorModifier_Curvature_3D {
  struct LineStyleModifier modifier;

  float min_curvature, max_curvature;
  struct ColorBand *color_ramp;
  float range_min, range_max;
} LineStyleColorModifier_Curvature_3D;

typedef struct LineStyleAlphaModifier_Curvature_3D {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  float min_curvature, max_curvature;
  char _pad[4];
} LineStyleAlphaModifier_Curvature_3D;

typedef struct LineStyleThicknessModifier_Curvature_3D {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  char _pad[4];
  float min_curvature, max_curvature;
  float min_thickness, max_thickness;
} LineStyleThicknessModifier_Curvature_3D;

/* Noise modifiers (for color, alpha and thickness) */

typedef struct LineStyleColorModifier_Noise {
  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp;
  float period, amplitude;
  int seed;
  char _pad[4];
} LineStyleColorModifier_Noise;

typedef struct LineStyleAlphaModifier_Noise {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  float period, amplitude;
  int seed;
} LineStyleAlphaModifier_Noise;

typedef struct LineStyleThicknessModifier_Noise {
  struct LineStyleModifier modifier;

  float period, amplitude;
  int flags;
  int seed;
} LineStyleThicknessModifier_Noise;

/* Crease Angle modifiers */

typedef struct LineStyleColorModifier_CreaseAngle {
  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp;
  float min_angle, max_angle;
} LineStyleColorModifier_CreaseAngle;

typedef struct LineStyleAlphaModifier_CreaseAngle {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  float min_angle, max_angle;
  char _pad[4];
} LineStyleAlphaModifier_CreaseAngle;

typedef struct LineStyleThicknessModifier_CreaseAngle {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  char _pad[4];
  float min_angle, max_angle;
  float min_thickness, max_thickness;
} LineStyleThicknessModifier_CreaseAngle;

/* Tangent modifiers */

typedef struct LineStyleColorModifier_Tangent {
  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp;
} LineStyleColorModifier_Tangent;

typedef struct LineStyleAlphaModifier_Tangent {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  char _pad[4];
} LineStyleAlphaModifier_Tangent;

typedef struct LineStyleThicknessModifier_Tangent {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  float min_thickness, max_thickness;
  char _pad[4];
} LineStyleThicknessModifier_Tangent;

/* Material modifiers */

/* mat_attr */
#define LS_MODIFIER_MATERIAL_DIFF 1
#define LS_MODIFIER_MATERIAL_DIFF_R 2
#define LS_MODIFIER_MATERIAL_DIFF_G 3
#define LS_MODIFIER_MATERIAL_DIFF_B 4
#define LS_MODIFIER_MATERIAL_SPEC 5
#define LS_MODIFIER_MATERIAL_SPEC_R 6
#define LS_MODIFIER_MATERIAL_SPEC_G 7
#define LS_MODIFIER_MATERIAL_SPEC_B 8
#define LS_MODIFIER_MATERIAL_SPEC_HARD 9
#define LS_MODIFIER_MATERIAL_ALPHA 10
#define LS_MODIFIER_MATERIAL_LINE 11
#define LS_MODIFIER_MATERIAL_LINE_R 12
#define LS_MODIFIER_MATERIAL_LINE_G 13
#define LS_MODIFIER_MATERIAL_LINE_B 14
#define LS_MODIFIER_MATERIAL_LINE_A 15

typedef struct LineStyleColorModifier_Material {
  struct LineStyleModifier modifier;

  struct ColorBand *color_ramp;
  int flags;
  int mat_attr;
} LineStyleColorModifier_Material;

typedef struct LineStyleAlphaModifier_Material {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  int mat_attr;
} LineStyleAlphaModifier_Material;

typedef struct LineStyleThicknessModifier_Material {
  struct LineStyleModifier modifier;

  struct CurveMapping *curve;
  int flags;
  float value_min, value_max;
  int mat_attr;
} LineStyleThicknessModifier_Material;

/* Geometry modifiers */

typedef struct LineStyleGeometryModifier_Sampling {
  struct LineStyleModifier modifier;

  float sampling;
  char _pad[4];
} LineStyleGeometryModifier_Sampling;

typedef struct LineStyleGeometryModifier_BezierCurve {
  struct LineStyleModifier modifier;

  float error;
  char _pad[4];
} LineStyleGeometryModifier_BezierCurve;

typedef struct LineStyleGeometryModifier_SinusDisplacement {
  struct LineStyleModifier modifier;

  float wavelength, amplitude, phase;
  char _pad[4];
} LineStyleGeometryModifier_SinusDisplacement;

/* LineStyleGeometryModifier_SpatialNoise::flags */
#define LS_MODIFIER_SPATIAL_NOISE_SMOOTH 1
#define LS_MODIFIER_SPATIAL_NOISE_PURERANDOM 2

typedef struct LineStyleGeometryModifier_SpatialNoise {
  struct LineStyleModifier modifier;

  float amplitude, scale;
  unsigned int octaves;
  int flags;
} LineStyleGeometryModifier_SpatialNoise;

typedef struct LineStyleGeometryModifier_PerlinNoise1D {
  struct LineStyleModifier modifier;

  float frequency, amplitude;
  /** In radians. */
  float angle;
  unsigned int octaves;
  int seed;
  char _pad1[4];
} LineStyleGeometryModifier_PerlinNoise1D;

typedef struct LineStyleGeometryModifier_PerlinNoise2D {
  struct LineStyleModifier modifier;

  float frequency, amplitude;
  /** In radians. */
  float angle;
  unsigned int octaves;
  int seed;
  char _pad1[4];
} LineStyleGeometryModifier_PerlinNoise2D;

typedef struct LineStyleGeometryModifier_BackboneStretcher {
  struct LineStyleModifier modifier;

  float backbone_length;
  char _pad[4];
} LineStyleGeometryModifier_BackboneStretcher;

typedef struct LineStyleGeometryModifier_TipRemover {
  struct LineStyleModifier modifier;

  float tip_length;
  char _pad[4];
} LineStyleGeometryModifier_TipRemover;

typedef struct LineStyleGeometryModifier_Polygonalization {
  struct LineStyleModifier modifier;

  float error;
  char _pad[4];
} LineStyleGeometryModifier_Polygonalization;

typedef struct LineStyleGeometryModifier_GuidingLines {
  struct LineStyleModifier modifier;

  float offset;
  char _pad[4];
} LineStyleGeometryModifier_GuidingLines;

/* LineStyleGeometryModifier_BluePrintLines::shape */
#define LS_MODIFIER_BLUEPRINT_CIRCLES 1
#define LS_MODIFIER_BLUEPRINT_ELLIPSES 2
#define LS_MODIFIER_BLUEPRINT_SQUARES 4

typedef struct LineStyleGeometryModifier_Blueprint {
  struct LineStyleModifier modifier;

  int flags;
  unsigned int rounds;
  float backbone_length;
  unsigned int random_radius;
  unsigned int random_center;
  unsigned int random_backbone;
} LineStyleGeometryModifier_Blueprint;

typedef struct LineStyleGeometryModifier_2DOffset {
  struct LineStyleModifier modifier;

  float start, end;
  float x, y;
} LineStyleGeometryModifier_2DOffset;

/* LineStyleGeometryModifier_2DTransform::pivot */
#define LS_MODIFIER_2D_TRANSFORM_PIVOT_CENTER 1
#define LS_MODIFIER_2D_TRANSFORM_PIVOT_START 2
#define LS_MODIFIER_2D_TRANSFORM_PIVOT_END 3
#define LS_MODIFIER_2D_TRANSFORM_PIVOT_PARAM 4
#define LS_MODIFIER_2D_TRANSFORM_PIVOT_ABSOLUTE 5

typedef struct LineStyleGeometryModifier_2DTransform {
  struct LineStyleModifier modifier;

  int pivot;
  float scale_x, scale_y;
  /** In radians. */
  float angle;
  float pivot_u;
  float pivot_x, pivot_y;
  char _pad[4];
} LineStyleGeometryModifier_2DTransform;

typedef struct LineStyleGeometryModifier_Simplification {
  struct LineStyleModifier modifier;

  float tolerance;
  char _pad[4];
} LineStyleGeometryModifier_Simplification;

/* Calligraphic thickness modifier */

typedef struct LineStyleThicknessModifier_Calligraphy {
  struct LineStyleModifier modifier;

  float min_thickness, max_thickness;
  /** In radians. */
  float orientation;
  char _pad[4];
} LineStyleThicknessModifier_Calligraphy;

/* FreestyleLineStyle::panel */
#define LS_PANEL_STROKES 1
#define LS_PANEL_COLOR 2
#define LS_PANEL_ALPHA 3
#define LS_PANEL_THICKNESS 4
#define LS_PANEL_GEOMETRY 5
#define LS_PANEL_TEXTURE 6
#define LS_PANEL_MISC 7

/* FreestyleLineStyle::flag */
#define LS_DS_EXPAND (1 << 0) /* for animation editors */
#define LS_SAME_OBJECT (1 << 1)
#define LS_DASHED_LINE (1 << 2)
#define LS_MATERIAL_BOUNDARY (1 << 3)
#define LS_MIN_2D_LENGTH (1 << 4)
#define LS_MAX_2D_LENGTH (1 << 5)
#define LS_NO_CHAINING (1 << 6)
#define LS_MIN_2D_ANGLE (1 << 7)
#define LS_MAX_2D_ANGLE (1 << 8)
#define LS_SPLIT_LENGTH (1 << 9)
#define LS_SPLIT_PATTERN (1 << 10)
#define LS_NO_SORTING (1 << 11)
#define LS_REVERSE_ORDER (1 << 12) /* for sorting */
#define LS_TEXTURE (1 << 13)
#define LS_CHAIN_COUNT (1 << 14)

/* FreestyleLineStyle::chaining */
#define LS_CHAINING_PLAIN 1
#define LS_CHAINING_SKETCHY 2

/* FreestyleLineStyle::caps */
#define LS_CAPS_BUTT 1
#define LS_CAPS_ROUND 2
#define LS_CAPS_SQUARE 3

/* FreestyleLineStyle::thickness_position */
#define LS_THICKNESS_CENTER 1
#define LS_THICKNESS_INSIDE 2
#define LS_THICKNESS_OUTSIDE 3
#define LS_THICKNESS_RELATIVE 4 /* thickness_ratio is used */

/* FreestyleLineStyle::sort_key */
#define LS_SORT_KEY_DISTANCE_FROM_CAMERA 1
#define LS_SORT_KEY_2D_LENGTH 2
#define LS_SORT_KEY_PROJECTED_X 3
#define LS_SORT_KEY_PROJECTED_Y 4

/* FreestyleLineStyle::integration_type */
#define LS_INTEGRATION_MEAN 1
#define LS_INTEGRATION_MIN 2
#define LS_INTEGRATION_MAX 3
#define LS_INTEGRATION_FIRST 4
#define LS_INTEGRATION_LAST 5

typedef struct FreestyleLineStyle {
  ID id;
  struct AnimData *adt;

  float r, g, b, alpha;
  float thickness;
  int thickness_position;
  float thickness_ratio;
  int flag, caps;
  int chaining;
  unsigned int rounds;
  float split_length;
  /** In radians, for splitting. */
  float min_angle, max_angle;
  float min_length, max_length;
  unsigned int chain_count;
  unsigned short split_dash1, split_gap1;
  unsigned short split_dash2, split_gap2;
  unsigned short split_dash3, split_gap3;
  int sort_key, integration_type;
  float texstep;
  short texact, pr_texture;
  short use_nodes;
  char _pad[6];
  unsigned short dash1, gap1, dash2, gap2, dash3, gap3;
  /** For UI. */
  int panel;
  /** MAX_MTEX. */
  struct MTex *mtex[18];
  /* nodes */
  struct bNodeTree *nodetree;

  ListBase color_modifiers;
  ListBase alpha_modifiers;
  ListBase thickness_modifiers;
  ListBase geometry_modifiers;
} FreestyleLineStyle;

#ifdef __cplusplus
}
#endif
