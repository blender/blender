/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Typed SVM node parameter structs.
 *
 * Each struct defines the named parameters for one SVM node type, with a memory
 * layout that directly matches the SVM byte-code stream. No encoding or decoding
 * is needed - the struct is read directly from the byte-code via pointer cast.
 *
 * Structs must be a multiple of sizeof(uint) since the byte-code stream is
 * stored as an array of uint.
 *
 * - Kernel side: use svm_node_get<T>() to get a reference into the stream.
 * - Compiler side: use designated initializers and compiler.add_node(struct).
 */

#pragma once

#include "kernel/svm/types.h"

CCL_NAMESPACE_BEGIN

/* NODE_SHADER_JUMP */
struct SVMNodeShaderJump {
  int offset_surface;
  int offset_volume;
  int offset_displacement;
};
static_assert(alignof(SVMNodeShaderJump) <= alignof(uint));
static_assert(sizeof(SVMNodeShaderJump) % sizeof(uint) == 0);

/* NODE_JUMP_IF_ZERO */
struct SVMNodeJumpIfZero {
  int jump_offset;
  SVMStackOffset stack_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeJumpIfZero) <= alignof(uint));
static_assert(sizeof(SVMNodeJumpIfZero) % sizeof(uint) == 0);

/* NODE_JUMP_IF_ONE */
struct SVMNodeJumpIfOne {
  int jump_offset;
  SVMStackOffset stack_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeJumpIfOne) <= alignof(uint));
static_assert(sizeof(SVMNodeJumpIfOne) % sizeof(uint) == 0);

/* NODE_MATH */
struct SVMNodeMath {
  NodeMathType math_type;
  SVMInputFloat value1;
  SVMInputFloat value2;
  SVMInputFloat value3;
  SVMStackOffset result_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeMath) <= alignof(uint));
static_assert(sizeof(SVMNodeMath) % sizeof(uint) == 0);

/* NODE_CLAMP */
struct SVMNodeClamp {
  NodeClampType clamp_type;
  SVMInputFloat min;
  SVMInputFloat max;
  SVMInputFloat value;
  SVMStackOffset result_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeClamp) <= alignof(uint));
static_assert(sizeof(SVMNodeClamp) % sizeof(uint) == 0);

/* NODE_MAPPING / NODE_MAPPING_DERIVATIVE */
struct SVMNodeMapping {
  NodeMappingType mapping_type;
  SVMInputFloat3 vector;
  SVMInputFloat3 location;
  SVMInputFloat3 rotation;
  SVMInputFloat3 scale;
  SVMStackOffset result_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeMapping) <= alignof(uint));
static_assert(sizeof(SVMNodeMapping) % sizeof(uint) == 0);

/* NODE_NORMAL_MAP */
struct SVMNodeNormalMap {
  NodeNormalMapSpace space;
  int invert_green;
  int use_original_base;
  int attr;
  int attr_sign;
  SVMInputFloat3 color;
  SVMInputFloat strength;
  SVMStackOffset normal_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeNormalMap) <= alignof(uint));
static_assert(sizeof(SVMNodeNormalMap) % sizeof(uint) == 0);

/* NODE_VALUE_F / NODE_VALUE_F_DERIVATIVE */
struct SVMNodeValueF {
  float value;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeValueF) <= alignof(uint));
static_assert(sizeof(SVMNodeValueF) % sizeof(uint) == 0);

/* NODE_VALUE_V / NODE_VALUE_V_DERIVATIVE */
struct SVMNodeValueV {
  SVMStackOffset out_offset;
  uint8_t _pad[3];
  packed_float3 value;
};
static_assert(alignof(SVMNodeValueV) <= alignof(uint));
static_assert(sizeof(SVMNodeValueV) % sizeof(uint) == 0);

/* NODE_CONVERT / NODE_CONVERT_DERIVATIVE */
struct SVMNodeConvert {
  NodeConvert convert_type;
  SVMStackOffset from_offset;
  SVMStackOffset to_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeConvert) <= alignof(uint));
static_assert(sizeof(SVMNodeConvert) % sizeof(uint) == 0);

/* NODE_GEOMETRY / NODE_GEOMETRY_DERIVATIVE */
struct SVMNodeGeometry {
  NodeGeometry geom_type;
  NodeBumpOffset bump_offset;
  uint8_t store_derivatives;
  SVMStackOffset out_offset;
  float bump_filter_width;
};
static_assert(alignof(SVMNodeGeometry) <= alignof(uint));
static_assert(sizeof(SVMNodeGeometry) % sizeof(uint) == 0);

/* NODE_OBJECT_INFO */
struct SVMNodeObjectInfo {
  NodeObjectInfo info_type;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeObjectInfo) <= alignof(uint));
static_assert(sizeof(SVMNodeObjectInfo) % sizeof(uint) == 0);

/* NODE_PARTICLE_INFO */
struct SVMNodeParticleInfo {
  NodeParticleInfo info_type;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeParticleInfo) <= alignof(uint));
static_assert(sizeof(SVMNodeParticleInfo) % sizeof(uint) == 0);

/* NODE_HAIR_INFO */
struct SVMNodeHairInfo {
  NodeHairInfo info_type;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeHairInfo) <= alignof(uint));
static_assert(sizeof(SVMNodeHairInfo) % sizeof(uint) == 0);

/* NODE_POINT_INFO */
struct SVMNodePointInfo {
  NodePointInfo info_type;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodePointInfo) <= alignof(uint));
static_assert(sizeof(SVMNodePointInfo) % sizeof(uint) == 0);

/* NODE_LIGHT_PATH */
struct SVMNodeLightPath {
  NodeLightPath path_type;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeLightPath) <= alignof(uint));
static_assert(sizeof(SVMNodeLightPath) % sizeof(uint) == 0);

/* NODE_ATTR / NODE_ATTR_DERIVATIVE */
struct SVMNodeAttr {
  int attr;
  SVMStackOffset out_offset;
  NodeAttributeOutputType output_type;
  NodeBumpOffset bump_offset;
  uint8_t store_derivatives;
  float bump_filter_width;
};
static_assert(alignof(SVMNodeAttr) <= alignof(uint));
static_assert(sizeof(SVMNodeAttr) % sizeof(uint) == 0);

/* NODE_VERTEX_COLOR / NODE_VERTEX_COLOR_DERIVATIVE */
struct SVMNodeVertexColor {
  uint8_t layer_id;
  SVMStackOffset color_offset;
  SVMStackOffset alpha_offset;
  NodeBumpOffset bump_offset;
  float bump_filter_width;
};
static_assert(alignof(SVMNodeVertexColor) <= alignof(uint));
static_assert(sizeof(SVMNodeVertexColor) % sizeof(uint) == 0);

/* NODE_TEX_COORD / NODE_TEX_COORD_DERIVATIVE */
struct SVMNodeTexCoord {
  NodeTexCoord texco_type;
  NodeBumpOffset bump_offset;
  uint8_t store_derivatives;
  SVMStackOffset out_offset;
  float bump_filter_width;
};
static_assert(alignof(SVMNodeTexCoord) <= alignof(uint));
static_assert(sizeof(SVMNodeTexCoord) % sizeof(uint) == 0);

/* NODE_GAMMA */
struct SVMNodeGamma {
  SVMInputFloat3 color;
  SVMInputFloat gamma;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeGamma) <= alignof(uint));
static_assert(sizeof(SVMNodeGamma) % sizeof(uint) == 0);

/* Bright/ NODE_BRIGHTCONTRAST */
struct SVMNodeBrightContrast {
  SVMInputFloat3 color;
  SVMInputFloat bright;
  SVMInputFloat contrast;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeBrightContrast) <= alignof(uint));
static_assert(sizeof(SVMNodeBrightContrast) % sizeof(uint) == 0);

/* NODE_INVERT */
struct SVMNodeInvert {
  SVMInputFloat3 color;
  SVMInputFloat fac;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeInvert) <= alignof(uint));
static_assert(sizeof(SVMNodeInvert) % sizeof(uint) == 0);

/* NODE_FRESNEL */
struct SVMNodeFresnel {
  SVMInputFloat ior;
  SVMStackOffset normal_offset;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeFresnel) <= alignof(uint));
static_assert(sizeof(SVMNodeFresnel) % sizeof(uint) == 0);

/* NODE_LAYER_WEIGHT */
struct SVMNodeLayerWeight {
  NodeBlendWeightType weight_type;
  SVMInputFloat blend;
  SVMStackOffset normal_offset;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeLayerWeight) <= alignof(uint));
static_assert(sizeof(SVMNodeLayerWeight) % sizeof(uint) == 0);

/* NODE_HSV */
struct SVMNodeHSV {
  SVMInputFloat3 color;
  SVMInputFloat hue;
  SVMInputFloat sat;
  SVMInputFloat val;
  SVMInputFloat fac;
  SVMStackOffset out_color_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeHSV) <= alignof(uint));
static_assert(sizeof(SVMNodeHSV) % sizeof(uint) == 0);

/* NODE_CAMERA */
struct SVMNodeCamera {
  SVMStackOffset vector_offset;
  SVMStackOffset zdepth_offset;
  SVMStackOffset distance_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeCamera) <= alignof(uint));
static_assert(sizeof(SVMNodeCamera) % sizeof(uint) == 0);

/* NODE_BLACKBODY */
struct SVMNodeBlackbody {
  SVMInputFloat temperature;
  SVMStackOffset color_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeBlackbody) <= alignof(uint));
static_assert(sizeof(SVMNodeBlackbody) % sizeof(uint) == 0);

/* NODE_WAVELENGTH */
struct SVMNodeWavelength {
  SVMInputFloat wavelength;
  SVMStackOffset color_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeWavelength) <= alignof(uint));
static_assert(sizeof(SVMNodeWavelength) % sizeof(uint) == 0);

/* NODE_VECTOR_MATH / NODE_VECTOR_MATH_DERIVATIVE */
struct SVMNodeVectorMath {
  NodeVectorMathType math_type;
  SVMInputFloat3 a;
  SVMInputFloat3 b;
  SVMInputFloat3 c;
  SVMInputFloat param1;
  SVMStackOffset value_offset;
  SVMStackOffset vector_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeVectorMath) <= alignof(uint));
static_assert(sizeof(SVMNodeVectorMath) % sizeof(uint) == 0);

/* NODE_VECTOR_ROTATE */
struct SVMNodeVectorRotate {
  NodeVectorRotateType rotate_type;
  SVMInputFloat3 vector;
  SVMInputFloat3 center;
  SVMInputFloat3 axis;
  SVMInputFloat3 rotation;
  SVMInputFloat angle;
  uint8_t invert;
  SVMStackOffset result_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeVectorRotate) <= alignof(uint));
static_assert(sizeof(SVMNodeVectorRotate) % sizeof(uint) == 0);

/* NODE_VECTOR_TRANSFORM */
struct SVMNodeVectorTransform {
  NodeVectorTransformType transform_type;
  NodeVectorTransformConvertSpace convert_from;
  NodeVectorTransformConvertSpace convert_to;
  SVMInputFloat3 vector_in;
  SVMStackOffset vector_out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeVectorTransform) <= alignof(uint));
static_assert(sizeof(SVMNodeVectorTransform) % sizeof(uint) == 0);

/* NODE_SEPARATE_VECTOR / NODE_SEPARATE_VECTOR_DERIVATIVE
 * One node is emitted per output component (vector_index 0, 1, 2). */
struct SVMNodeSeparateVector {
  SVMInputFloat3 vector;
  uint8_t vector_index;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeSeparateVector) <= alignof(uint));
static_assert(sizeof(SVMNodeSeparateVector) % sizeof(uint) == 0);

/* NODE_COMBINE_VECTOR / NODE_COMBINE_VECTOR_DERIVATIVE
 * One node is emitted per input component (vector_index 0, 1, 2). */
struct SVMNodeCombineVector {
  SVMInputFloat in;
  uint8_t vector_index;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeCombineVector) <= alignof(uint));
static_assert(sizeof(SVMNodeCombineVector) % sizeof(uint) == 0);

/* NODE_SEPARATE_COLOR */
struct SVMNodeSeparateColor {
  NodeCombSepColorType color_type;
  SVMInputFloat3 color;
  SVMStackOffset red_offset;
  SVMStackOffset green_offset;
  SVMStackOffset blue_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeSeparateColor) <= alignof(uint));
static_assert(sizeof(SVMNodeSeparateColor) % sizeof(uint) == 0);

/* NODE_COMBINE_COLOR */
struct SVMNodeCombineColor {
  NodeCombSepColorType color_type;
  SVMInputFloat red;
  SVMInputFloat green;
  SVMInputFloat blue;
  SVMStackOffset color_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeCombineColor) <= alignof(uint));
static_assert(sizeof(SVMNodeCombineColor) % sizeof(uint) == 0);

/* NODE_MIX_COLOR */
struct SVMNodeMixColor {
  NodeMix blend_type;
  SVMInputFloat3 a;
  SVMInputFloat3 b;
  SVMInputFloat fac;
  uint8_t use_clamp;
  uint8_t use_clamp_result;
  SVMStackOffset result_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeMixColor) <= alignof(uint));
static_assert(sizeof(SVMNodeMixColor) % sizeof(uint) == 0);

/* NODE_MIX_FLOAT */
struct SVMNodeMixFloat {
  SVMInputFloat fac;
  SVMInputFloat a;
  SVMInputFloat b;
  uint8_t use_clamp;
  SVMStackOffset result_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeMixFloat) <= alignof(uint));
static_assert(sizeof(SVMNodeMixFloat) % sizeof(uint) == 0);

/* Mix node (legacy) NODE_MIX */
struct SVMNodeMix {
  NodeMix mix_type;
  SVMInputFloat3 c1;
  SVMInputFloat3 c2;
  SVMInputFloat fac;
  SVMStackOffset result_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeMix) <= alignof(uint));
static_assert(sizeof(SVMNodeMix) % sizeof(uint) == 0);

/* NODE_MIX_VECTOR */
struct SVMNodeMixVector {
  SVMInputFloat3 a;
  SVMInputFloat3 b;
  SVMInputFloat fac;
  uint8_t use_clamp;
  SVMStackOffset result_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeMixVector) <= alignof(uint));
static_assert(sizeof(SVMNodeMixVector) % sizeof(uint) == 0);

/* Mix Vector Non- NODE_MIX_VECTOR_NON_UNIFORM */
struct SVMNodeMixVectorNonUniform {
  SVMInputFloat3 a;
  SVMInputFloat3 b;
  SVMInputFloat3 fac;
  uint8_t use_clamp;
  SVMStackOffset result_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeMixVectorNonUniform) <= alignof(uint));
static_assert(sizeof(SVMNodeMixVectorNonUniform) % sizeof(uint) == 0);

/* NODE_MAP_RANGE */
struct SVMNodeMapRange {
  NodeMapRangeType range_type;
  SVMInputFloat value;
  SVMInputFloat from_min;
  SVMInputFloat from_max;
  SVMInputFloat to_min;
  SVMInputFloat to_max;
  SVMInputFloat steps;
  SVMStackOffset result_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeMapRange) <= alignof(uint));
static_assert(sizeof(SVMNodeMapRange) % sizeof(uint) == 0);

/* NODE_VECTOR_MAP_RANGE */
struct SVMNodeVectorMapRange {
  NodeMapRangeType range_type;
  uint8_t use_clamp;
  uint8_t _pad[3];
  SVMInputFloat3 value;
  SVMInputFloat3 from_min;
  SVMInputFloat3 from_max;
  SVMInputFloat3 to_min;
  SVMInputFloat3 to_max;
  SVMInputFloat3 steps;
  SVMStackOffset result_offset;
  uint8_t _pad2[3];
};
static_assert(alignof(SVMNodeVectorMapRange) <= alignof(uint));
static_assert(sizeof(SVMNodeVectorMapRange) % sizeof(uint) == 0);

/* NODE_NORMAL */
struct SVMNodeNormal {
  SVMInputFloat3 in_normal;
  SVMStackOffset out_normal_offset;
  SVMStackOffset out_dot_offset;
  uint8_t _pad[2];
  float direction_x;
  float direction_y;
  float direction_z;
};
static_assert(alignof(SVMNodeNormal) <= alignof(uint));
static_assert(sizeof(SVMNodeNormal) % sizeof(uint) == 0);

/* NODE_SET_BUMP */
struct SVMNodeSetBump {
  SVMInputFloat scale;
  SVMInputFloat strength;
  float bump_filter_width;
  SVMStackOffset normal_offset;
  uint8_t invert;
  uint8_t use_object_space;
  SVMStackOffset center_offset;
  SVMStackOffset dx_offset;
  SVMStackOffset dy_offset;
  SVMStackOffset out_offset;
  SVMStackOffset bump_state_offset;
};
static_assert(alignof(SVMNodeSetBump) <= alignof(uint));
static_assert(sizeof(SVMNodeSetBump) % sizeof(uint) == 0);

/* NODE_ENTER_BUMP_EVAL */
struct SVMNodeEnterBumpEval {
  SVMStackOffset state_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeEnterBumpEval) <= alignof(uint));
static_assert(sizeof(SVMNodeEnterBumpEval) % sizeof(uint) == 0);

/* NODE_SET_DISPLACEMENT */
struct SVMNodeSetDisplacement {
  SVMStackOffset fac_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeSetDisplacement) <= alignof(uint));
static_assert(sizeof(SVMNodeSetDisplacement) % sizeof(uint) == 0);

/* NODE_DISPLACEMENT */
struct SVMNodeDisplacement {
  NodeNormalMapSpace space;
  SVMInputFloat height;
  SVMInputFloat midlevel;
  SVMInputFloat scale;
  SVMStackOffset normal_offset;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeDisplacement) <= alignof(uint));
static_assert(sizeof(SVMNodeDisplacement) % sizeof(uint) == 0);

/* NODE_VECTOR_DISPLACEMENT */
struct SVMNodeVectorDisplacement {
  NodeNormalMapSpace space;
  SVMInputFloat3 vector;
  SVMInputFloat midlevel;
  SVMInputFloat scale;
  int attr;
  int attr_sign;
  SVMStackOffset displacement_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeVectorDisplacement) <= alignof(uint));
static_assert(sizeof(SVMNodeVectorDisplacement) % sizeof(uint) == 0);

/* NODE_LEAVE_BUMP_EVAL */
struct SVMNodeLeaveBumpEval {
  SVMStackOffset state_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeLeaveBumpEval) <= alignof(uint));
static_assert(sizeof(SVMNodeLeaveBumpEval) % sizeof(uint) == 0);

/* NODE_TEX_NOISE */
struct SVMNodeTexNoise {
  uint dimensions;
  NodeNoiseType noise_type;
  uint normalize;
  SVMInputFloat w;
  SVMInputFloat scale;
  SVMInputFloat detail;
  SVMInputFloat roughness;
  SVMInputFloat lacunarity;
  SVMInputFloat offset;
  SVMInputFloat gain;
  SVMInputFloat distortion;
  SVMStackOffset vector;
  SVMStackOffset value_offset;
  SVMStackOffset color_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeTexNoise) <= alignof(uint));
static_assert(sizeof(SVMNodeTexNoise) % sizeof(uint) == 0);

/* NODE_TEX_VORONOI */
struct SVMNodeTexVoronoi {
  uint dimensions;
  NodeVoronoiFeature feature;
  NodeVoronoiDistanceMetric metric;
  SVMInputFloat w;
  SVMInputFloat scale;
  SVMInputFloat detail;
  SVMInputFloat roughness;
  SVMInputFloat lacunarity;
  SVMInputFloat smoothness;
  SVMInputFloat exponent;
  SVMInputFloat randomness;
  uint8_t normalize;
  SVMStackOffset coord;
  SVMStackOffset distance_offset;
  SVMStackOffset color_offset;
  SVMStackOffset position_offset;
  SVMStackOffset w_out_offset;
  SVMStackOffset radius_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeTexVoronoi) <= alignof(uint));
static_assert(sizeof(SVMNodeTexVoronoi) % sizeof(uint) == 0);

/* NODE_TEX_GABOR */
struct SVMNodeTexGabor {
  NodeGaborType gabor_type;
  SVMInputFloat3 orientation_3d;
  SVMInputFloat scale;
  SVMInputFloat frequency;
  SVMInputFloat anisotropy;
  SVMInputFloat orientation_2d;
  SVMStackOffset coordinates;
  SVMStackOffset value_offset;
  SVMStackOffset phase_offset;
  SVMStackOffset intensity_offset;
};
static_assert(alignof(SVMNodeTexGabor) <= alignof(uint));
static_assert(sizeof(SVMNodeTexGabor) % sizeof(uint) == 0);

/* NODE_TEX_WAVE */
struct SVMNodeTexWave {
  NodeWaveType wave_type;
  NodeWaveBandsDirection bands_direction;
  NodeWaveRingsDirection rings_direction;
  NodeWaveProfile profile;
  SVMInputFloat scale;
  SVMInputFloat distortion;
  SVMInputFloat detail;
  SVMInputFloat dscale;
  SVMInputFloat droughness;
  SVMInputFloat phase;
  SVMStackOffset co;
  SVMStackOffset color_offset;
  SVMStackOffset fac_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeTexWave) <= alignof(uint));
static_assert(sizeof(SVMNodeTexWave) % sizeof(uint) == 0);

/* NODE_TEX_MAGIC */
struct SVMNodeTexMagic {
  SVMInputFloat scale;
  SVMInputFloat distortion;
  uint8_t depth;
  SVMStackOffset co;
  SVMStackOffset color_offset;
  SVMStackOffset fac_offset;
};
static_assert(alignof(SVMNodeTexMagic) <= alignof(uint));
static_assert(sizeof(SVMNodeTexMagic) % sizeof(uint) == 0);

/* NODE_TEX_CHECKER */
struct SVMNodeTexChecker {
  SVMInputFloat3 color1;
  SVMInputFloat3 color2;
  SVMInputFloat scale;
  SVMStackOffset co;
  SVMStackOffset color_offset;
  SVMStackOffset fac_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeTexChecker) <= alignof(uint));
static_assert(sizeof(SVMNodeTexChecker) % sizeof(uint) == 0);

/* NODE_TEX_BRICK */
struct SVMNodeTexBrick {
  SVMInputFloat3 color1;
  SVMInputFloat3 color2;
  SVMInputFloat3 mortar;
  SVMInputFloat scale;
  SVMInputFloat mortar_size;
  SVMInputFloat bias;
  SVMInputFloat brick_width;
  SVMInputFloat row_height;
  SVMInputFloat mortar_smooth;
  float offset_amount;
  float squash_amount;
  uint8_t offset_frequency;
  uint8_t squash_frequency;
  SVMStackOffset co;
  SVMStackOffset color_offset;
  SVMStackOffset fac_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeTexBrick) <= alignof(uint));
static_assert(sizeof(SVMNodeTexBrick) % sizeof(uint) == 0);

/* NODE_TEX_WHITE_NOISE */
struct SVMNodeTexWhiteNoise {
  uint dimensions;
  SVMInputFloat3 vector;
  SVMInputFloat w;
  SVMStackOffset value_offset;
  SVMStackOffset color_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeTexWhiteNoise) <= alignof(uint));
static_assert(sizeof(SVMNodeTexWhiteNoise) % sizeof(uint) == 0);

/* NODE_TEX_GRADIENT */
struct SVMNodeTexGradient {
  NodeGradientType gradient_type;
  SVMStackOffset co;
  SVMStackOffset fac_offset;
  SVMStackOffset color_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeTexGradient) <= alignof(uint));
static_assert(sizeof(SVMNodeTexGradient) % sizeof(uint) == 0);

/* NODE_TEX_IMAGE / NODE_TEX_IMAGE_DERIVATIVE */
struct SVMNodeTexImage {
  int id;
  uint projection; /* NodeImageProjection */
  uint8_t flags;
  SVMStackOffset co;
  SVMStackOffset out_offset;
  SVMStackOffset alpha_offset;
};
static_assert(alignof(SVMNodeTexImage) <= alignof(uint));
static_assert(sizeof(SVMNodeTexImage) % sizeof(uint) == 0);

/* NODE_TEX_IMAGE_BOX / NODE_TEX_IMAGE_BOX_DERIVATIVE */
struct SVMNodeTexImageBox {
  int id;
  float blend;
  uint8_t flags;
  SVMStackOffset co;
  SVMStackOffset out_offset;
  SVMStackOffset alpha_offset;
};
static_assert(alignof(SVMNodeTexImageBox) <= alignof(uint));
static_assert(sizeof(SVMNodeTexImageBox) % sizeof(uint) == 0);

/* NODE_TEX_ENVIRONMENT / NODE_TEX_ENVIRONMENT_DERIVATIVE */
struct SVMNodeTexEnvironment {
  int id;
  NodeEnvironmentProjection projection;
  uint8_t flags;
  SVMStackOffset co;
  SVMStackOffset out_offset;
  SVMStackOffset alpha_offset;
};
static_assert(alignof(SVMNodeTexEnvironment) <= alignof(uint));
static_assert(sizeof(SVMNodeTexEnvironment) % sizeof(uint) == 0);

/* NODE_TEX_SKY */
struct SVMNodeTexSky {
  NodeSkyType sky_type;
  SVMStackOffset dir_offset;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeTexSky) <= alignof(uint));
static_assert(sizeof(SVMNodeTexSky) % sizeof(uint) == 0);

/* Sky Texture Preetham/Hosek data: follows SVMNodeTexSky header. */
struct SVMNodeTexSkyPreethamData {
  float phi;
  float theta;
  float radiance_x;
  float radiance_y;
  float radiance_z;
  float config_x[9];
  float config_y[9];
  float config_z[9];
};
static_assert(alignof(SVMNodeTexSkyPreethamData) <= alignof(uint));
static_assert(sizeof(SVMNodeTexSkyPreethamData) % sizeof(uint) == 0);

/* Sky Texture Nishita data: follows SVMNodeTexSky header. */
struct SVMNodeTexSkyNishitaData {
  float pixel_bottom_x;
  float pixel_bottom_y;
  float pixel_bottom_z;
  float pixel_top_x;
  float pixel_top_y;
  float pixel_top_z;
  float sun_elevation;
  float sun_rotation;
  float angular_diameter;
  float sun_intensity;
  float earth_intersection_angle;
  uint texture_id;
};
static_assert(alignof(SVMNodeTexSkyNishitaData) <= alignof(uint));
static_assert(sizeof(SVMNodeTexSkyNishitaData) % sizeof(uint) == 0);

/* NODE_RGB_RAMP
 * Only covers the fixed header; variable-length table data follows. */
struct SVMNodeRGBRamp {
  uint table_size;
  SVMInputFloat fac;
  uint8_t interpolate;
  SVMStackOffset color_offset;
  SVMStackOffset alpha_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeRGBRamp) <= alignof(uint));
static_assert(sizeof(SVMNodeRGBRamp) % sizeof(uint) == 0);

/* NODE_CURVES
 * Only covers the fixed header; variable-length table data follows. */
struct SVMNodeCurves {
  SVMInputFloat3 color;
  SVMInputFloat fac;
  float min_x;
  float max_x;
  uint table_size;
  uint8_t extrapolate;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeCurves) <= alignof(uint));
static_assert(sizeof(SVMNodeCurves) % sizeof(uint) == 0);

/* NODE_FLOAT_CURVE
 * Only covers the fixed header; variable-length table data follows. */
struct SVMNodeFloatCurve {
  SVMInputFloat fac;
  SVMInputFloat value_in;
  float min_x;
  float max_x;
  uint table_size;
  uint8_t extrapolate;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeFloatCurve) <= alignof(uint));
static_assert(sizeof(SVMNodeFloatCurve) % sizeof(uint) == 0);

/* NODE_TANGENT / NODE_TANGENT_DERIVATIVE */
struct SVMNodeTangent {
  NodeTangentDirectionType direction_type;
  NodeTangentAxis axis;
  int attr;
  SVMStackOffset tangent_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeTangent) <= alignof(uint));
static_assert(sizeof(SVMNodeTangent) % sizeof(uint) == 0);

/* NODE_WIREFRAME */
struct SVMNodeWireframe {
  SVMInputFloat in_size;
  float bump_filter_width;
  uint8_t use_pixel_size;
  NodeBumpOffset bump_offset;
  SVMStackOffset out_fac_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeWireframe) <= alignof(uint));
static_assert(sizeof(SVMNodeWireframe) % sizeof(uint) == 0);

/* NODE_IES */
struct SVMNodeIES {
  SVMInputFloat strength;
  uint slot;
  SVMStackOffset vector_offset;
  SVMStackOffset fac_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeIES) <= alignof(uint));
static_assert(sizeof(SVMNodeIES) % sizeof(uint) == 0);

/* NODE_CLOSURE_SET_WEIGHT */
struct SVMNodeClosureSetWeight {
  packed_float3 rgb;
};
static_assert(alignof(SVMNodeClosureSetWeight) <= alignof(uint));
static_assert(sizeof(SVMNodeClosureSetWeight) % sizeof(uint) == 0);

/* NODE_CLOSURE_WEIGHT */
struct SVMNodeClosureWeight {
  SVMStackOffset weight_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeClosureWeight) <= alignof(uint));
static_assert(sizeof(SVMNodeClosureWeight) % sizeof(uint) == 0);

/* NODE_CLOSURE_SET_NORMAL */
struct SVMNodeClosureSetNormal {
  SVMStackOffset direction_offset;
  SVMStackOffset normal_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeClosureSetNormal) <= alignof(uint));
static_assert(sizeof(SVMNodeClosureSetNormal) % sizeof(uint) == 0);

/* NODE_CLOSURE_EMISSION */
struct SVMNodeClosureEmission {
  SVMStackOffset mix_weight_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeClosureEmission) <= alignof(uint));
static_assert(sizeof(SVMNodeClosureEmission) % sizeof(uint) == 0);

/* NODE_CLOSURE_BACKGROUND */
struct SVMNodeClosureBackground {
  SVMStackOffset mix_weight_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeClosureBackground) <= alignof(uint));
static_assert(sizeof(SVMNodeClosureBackground) % sizeof(uint) == 0);

/* NODE_CLOSURE_HOLDOUT */
struct SVMNodeClosureHoldout {
  SVMStackOffset mix_weight_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeClosureHoldout) <= alignof(uint));
static_assert(sizeof(SVMNodeClosureHoldout) % sizeof(uint) == 0);

/* NODE_EMISSION_WEIGHT */
struct SVMNodeEmissionWeight {
  SVMInputFloat3 color;
  SVMInputFloat strength;
};
static_assert(alignof(SVMNodeEmissionWeight) <= alignof(uint));
static_assert(sizeof(SVMNodeEmissionWeight) % sizeof(uint) == 0);

/* NODE_MIX_CLOSURE */
struct SVMNodeMixClosure {
  SVMInputFloat fac;
  SVMStackOffset in_weight_offset;
  SVMStackOffset weight1_offset;
  SVMStackOffset weight2_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeMixClosure) <= alignof(uint));
static_assert(sizeof(SVMNodeMixClosure) % sizeof(uint) == 0);

/* NODE_LIGHT_FALLOFF */
struct SVMNodeLightFalloff {
  NodeLightFalloff falloff_type;
  SVMInputFloat strength;
  SVMInputFloat smooth;
  SVMStackOffset out_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeLightFalloff) <= alignof(uint));
static_assert(sizeof(SVMNodeLightFalloff) % sizeof(uint) == 0);

/* NODE_CLOSURE_VOLUME */
struct SVMNodeClosureVolume {
  ClosureType closure_type;
  SVMInputFloat density;
  SVMInputFloat param1;
  SVMInputFloat param_extra;
  SVMStackOffset mix_weight_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeClosureVolume) <= alignof(uint));
static_assert(sizeof(SVMNodeClosureVolume) % sizeof(uint) == 0);

/* NODE_VOLUME_COEFFICIENTS */
struct SVMNodeVolumeCoefficients {
  ClosureType closure_type;
  SVMInputFloat3 absorption_coeffs;
  SVMInputFloat3 emission_coeffs;
  SVMInputFloat param1;
  SVMInputFloat param_extra;
  SVMStackOffset mix_weight_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeVolumeCoefficients) <= alignof(uint));
static_assert(sizeof(SVMNodeVolumeCoefficients) % sizeof(uint) == 0);

/* NODE_PRINCIPLED_VOLUME */
struct SVMNodePrincipledVolume {
  SVMInputFloat3 absorption_color;
  SVMInputFloat3 emission_color;
  SVMInputFloat3 blackbody_tint;
  SVMInputFloat density;
  SVMInputFloat anisotropy;
  SVMInputFloat emission;
  SVMInputFloat blackbody;
  SVMInputFloat temperature;
  int attr_density;
  int attr_color;
  int attr_temperature;
  SVMStackOffset mix_weight_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodePrincipledVolume) <= alignof(uint));
static_assert(sizeof(SVMNodePrincipledVolume) % sizeof(uint) == 0);

/* NODE_CLOSURE_BSDF. Common header for all BSDFs, followed by data defined below. */
struct SVMNodeClosureBsdf {
  ClosureType closure_type;
  SVMStackOffset mix_weight_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeClosureBsdf) <= alignof(uint));
static_assert(sizeof(SVMNodeClosureBsdf) % sizeof(uint) == 0);

/* Simple BSDFs. */
struct SVMNodeSimpleBsdfData {
  SVMInputFloat param1;
  SVMStackOffset normal_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeSimpleBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeSimpleBsdfData) % sizeof(uint) == 0);

/* Diffuse BSDF. */
struct SVMNodeDiffuseBsdfData {
  SVMInputFloat3 color;
  SVMInputFloat roughness;
  SVMStackOffset normal_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeDiffuseBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeDiffuseBsdfData) % sizeof(uint) == 0);

/* Toon BSDF. */
struct SVMNodeToonBsdfData {
  SVMInputFloat size;
  SVMInputFloat smooth;
  SVMStackOffset normal_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeToonBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeToonBsdfData) % sizeof(uint) == 0);

/* Ray Portal BSDF. */
struct SVMNodeRayPortalBsdfData {
  SVMInputFloat3 direction;
  SVMStackOffset position_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeRayPortalBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeRayPortalBsdfData) % sizeof(uint) == 0);

/* Glossy BSDF. */
struct SVMNodeGlossyBsdfData {
  SVMInputFloat3 color;
  SVMInputFloat roughness;
  SVMInputFloat anisotropy;
  SVMInputFloat rotation;
  SVMStackOffset normal_offset;
  SVMStackOffset tangent_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeGlossyBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeGlossyBsdfData) % sizeof(uint) == 0);

/* Refraction BSDF. */
struct SVMNodeRefractionBsdfData {
  SVMInputFloat roughness;
  SVMInputFloat ior;
  SVMStackOffset normal_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeRefractionBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeRefractionBsdfData) % sizeof(uint) == 0);

/* Glass BSDF. */
struct SVMNodeGlassBsdfData {
  SVMInputFloat3 color;
  SVMInputFloat roughness;
  SVMInputFloat ior;
  SVMInputFloat thin_film_thickness;
  SVMInputFloat thin_film_ior;
  SVMStackOffset normal_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeGlassBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeGlassBsdfData) % sizeof(uint) == 0);

/* Metallic BSDF. */
struct SVMNodeMetallicBsdfData {
  ClosureType distribution;
  SVMInputFloat3 base_ior;    /* IOR (physical) or Base Color (F82) */
  SVMInputFloat3 edge_tint_k; /* Extinction (physical) or Edge Tint (F82) */
  SVMInputFloat roughness;
  SVMInputFloat anisotropy;
  SVMInputFloat rotation;
  SVMInputFloat thin_film_thickness;
  SVMInputFloat thin_film_ior;
  SVMStackOffset normal_offset;
  SVMStackOffset tangent_offset;
  uint8_t _pad[2];
};
static_assert(alignof(SVMNodeMetallicBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeMetallicBsdfData) % sizeof(uint) == 0);

/* Hair BSDF. */
struct SVMNodeHairBsdfData {
  SVMInputFloat roughness1;
  SVMInputFloat roughness2;
  SVMInputFloat offset;
  SVMStackOffset tangent_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeHairBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeHairBsdfData) % sizeof(uint) == 0);

/* Bssrdf. */
struct SVMNodeBssrdfData {
  SVMInputFloat3 radius;
  SVMInputFloat scale;
  SVMInputFloat ior;
  SVMInputFloat anisotropy;
  SVMInputFloat roughness;
  SVMStackOffset normal_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeBssrdfData) <= alignof(uint));
static_assert(sizeof(SVMNodeBssrdfData) % sizeof(uint) == 0);

/* Principled BSDF. */
struct SVMNodePrincipledBsdfData {
  ClosureType distribution;
  SVMInputFloat ior;
  SVMInputFloat roughness;
  /* Weights */
  SVMInputFloat sheen_weight;
  SVMInputFloat coat_weight;
  SVMInputFloat metallic;
  SVMInputFloat transmission_weight;
  SVMInputFloat subsurface_weight;
  /* Base. */
  SVMInputFloat3 base_color;
  SVMInputFloat alpha;
  SVMInputFloat diffuse_roughness;
  /* Normals and tangents. */
  SVMStackOffset normal_offset;
  SVMStackOffset tangent_offset;
  SVMStackOffset coat_normal_offset;
  uint8_t _pad[1];
  /* Specular. */
  SVMInputFloat3 specular_tint;
  SVMInputFloat specular_ior_level;
  SVMInputFloat anisotropic;
  SVMInputFloat anisotropic_rotation;
  /* Emission. */
  SVMInputFloat3 emission_color;
  SVMInputFloat emission_strength;
  /* Sheen. */
  SVMInputFloat3 sheen_tint;
  SVMInputFloat sheen_roughness;
  /* Coat. */
  SVMInputFloat3 coat_tint;
  SVMInputFloat coat_roughness;
  SVMInputFloat coat_ior;
  /* Subsurface. */
  ClosureType subsurface_method;
  SVMInputFloat3 subsurface_radius;
  SVMInputFloat subsurface_scale;
  SVMInputFloat subsurface_ior;
  SVMInputFloat subsurface_anisotropy;
  /* Thin film. */
  SVMInputFloat thin_film_thickness;
  SVMInputFloat thin_film_ior;
};
static_assert(alignof(SVMNodePrincipledBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodePrincipledBsdfData) % sizeof(uint) == 0);

/* Principled Hair BSDF. */
struct SVMNodePrincipledHairBsdfData {
  NodePrincipledHairParametrization parametrization;
  SVMInputFloat3 color;
  SVMInputFloat3 tint;
  SVMInputFloat3 absorption_coefficient;
  SVMInputFloat roughness;
  SVMInputFloat random_roughness;
  SVMInputFloat offset;
  SVMInputFloat ior;
  SVMInputFloat random;
  SVMInputFloat melanin;
  SVMInputFloat melanin_redness;
  SVMInputFloat coat;
  SVMInputFloat aspect_ratio;
  SVMInputFloat radial_roughness;
  SVMInputFloat random_color;
  SVMInputFloat R;
  SVMInputFloat TT;
  SVMInputFloat TRT;
  int attr_random;
  int attr_normal;
};
static_assert(alignof(SVMNodePrincipledHairBsdfData) <= alignof(uint));
static_assert(sizeof(SVMNodePrincipledHairBsdfData) % sizeof(uint) == 0);

/* NODE_BEVEL */
struct SVMNodeBevel {
  SVMInputFloat radius;
  uint8_t num_samples;
  SVMStackOffset normal_offset;
  SVMStackOffset out_offset;
  uint8_t _pad[1];
};
static_assert(alignof(SVMNodeBevel) <= alignof(uint));
static_assert(sizeof(SVMNodeBevel) % sizeof(uint) == 0);

/* NODE_AMBIENT_OCCLUSION */
struct SVMNodeAmbientOcclusion {
  SVMInputFloat3 color;
  SVMInputFloat dist;
  uint8_t flags;
  uint8_t samples;
  SVMStackOffset normal_offset;
  SVMStackOffset out_ao_offset;
  SVMStackOffset out_color_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeAmbientOcclusion) <= alignof(uint));
static_assert(sizeof(SVMNodeAmbientOcclusion) % sizeof(uint) == 0);

/* NODE_RAYCAST */
struct SVMNodeRaycast {
  SVMInputFloat3 position;
  SVMInputFloat3 direction;
  SVMInputFloat distance;
  float bump_filter_width;
  uint8_t only_local;
  uint16_t num_attributes;
  SVMStackOffset is_hit_offset;
  SVMStackOffset is_self_hit_offset;
  SVMStackOffset hit_distance_offset;
  SVMStackOffset hit_position_offset;
  SVMStackOffset hit_normal_offset;
};
static_assert(alignof(SVMNodeRaycast) <= alignof(uint));
static_assert(sizeof(SVMNodeRaycast) % sizeof(uint) == 0);

/* NODE_AOV_COLOR */
struct SVMNodeAOVColor {
  int aov_offset;
  SVMInputFloat3 color;
};
static_assert(alignof(SVMNodeAOVColor) <= alignof(uint));
static_assert(sizeof(SVMNodeAOVColor) % sizeof(uint) == 0);

/* NODE_AOV_VALUE */
struct SVMNodeAOVValue {
  int aov_offset;
  SVMInputFloat value;
};
static_assert(alignof(SVMNodeAOVValue) <= alignof(uint));
static_assert(sizeof(SVMNodeAOVValue) % sizeof(uint) == 0);

/* NODE_RADIAL_TILING */
struct SVMNodeRadialTiling {
  SVMInputFloat3 vector;
  SVMInputFloat r_gon_sides;
  SVMInputFloat r_gon_roundness;
  uint8_t normalize_r_gon_parameter;
  SVMStackOffset segment_coordinates_offset;
  SVMStackOffset segment_id_offset;
  SVMStackOffset max_unit_parameter_offset;
  SVMStackOffset x_axis_A_angle_bisector_offset;
  uint8_t _pad[3];
};
static_assert(alignof(SVMNodeRadialTiling) <= alignof(uint));
static_assert(sizeof(SVMNodeRadialTiling) % sizeof(uint) == 0);

/* NODE_TEXTURE_MAPPING, followed by Transform data in the bytecode. */
struct SVMNodeTextureMapping {
  SVMStackOffset vec_offset;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
  PackedTransform tfm;
};
static_assert(alignof(SVMNodeTextureMapping) <= alignof(uint));
static_assert(sizeof(SVMNodeTextureMapping) % sizeof(uint) == 0);

/* NODE_MIN_MAX */
struct SVMNodeMinMax {
  SVMStackOffset vec_offset;
  SVMStackOffset out_offset;
  uint8_t _pad[2];
  packed_float3 mn;
  packed_float3 mx;
};
static_assert(alignof(SVMNodeMinMax) <= alignof(uint));
static_assert(sizeof(SVMNodeMinMax) % sizeof(uint) == 0);

CCL_NAMESPACE_END
