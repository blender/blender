/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __NODES_H__
#define __NODES_H__

#include "render/graph.h"
#include "graph/node.h"

#include "util/util_array.h"
#include "util/util_string.h"

CCL_NAMESPACE_BEGIN

class ImageManager;
class LightManager;
class Scene;
class Shader;

/* Texture Mapping */

class TextureMapping {
 public:
  TextureMapping();
  Transform compute_transform();
  bool skip();
  void compile(SVMCompiler &compiler, int offset_in, int offset_out);
  int compile(SVMCompiler &compiler, ShaderInput *vector_in);
  void compile(OSLCompiler &compiler);

  int compile_begin(SVMCompiler &compiler, ShaderInput *vector_in);
  void compile_end(SVMCompiler &compiler, ShaderInput *vector_in, int vector_offset);

  float3 translation;
  float3 rotation;
  float3 scale;

  float3 min, max;
  bool use_minmax;

  enum Type { POINT = 0, TEXTURE = 1, VECTOR = 2, NORMAL = 3 };
  Type type;

  enum Mapping { NONE = 0, X = 1, Y = 2, Z = 3 };
  Mapping x_mapping, y_mapping, z_mapping;

  enum Projection { FLAT, CUBE, TUBE, SPHERE };
  Projection projection;
};

/* Nodes */

class TextureNode : public ShaderNode {
 public:
  explicit TextureNode(const NodeType *node_type) : ShaderNode(node_type)
  {
  }
  TextureMapping tex_mapping;
};

/* Any node which uses image manager's slot should be a subclass of this one. */
class ImageSlotTextureNode : public TextureNode {
 public:
  explicit ImageSlotTextureNode(const NodeType *node_type) : TextureNode(node_type)
  {
    special_type = SHADER_SPECIAL_TYPE_IMAGE_SLOT;
  }
  int slot;
};

class ImageTextureNode : public ImageSlotTextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(ImageTextureNode)
  ~ImageTextureNode();
  ShaderNode *clone() const;
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }

  virtual bool equals(const ShaderNode &other)
  {
    const ImageTextureNode &image_node = (const ImageTextureNode &)other;
    return ImageSlotTextureNode::equals(other) && builtin_data == image_node.builtin_data &&
           animated == image_node.animated;
  }

  /* Parameters. */
  ustring filename;
  void *builtin_data;
  ustring colorspace;
  ImageAlphaType alpha_type;
  NodeImageProjection projection;
  InterpolationType interpolation;
  ExtensionType extension;
  float projection_blend;
  bool animated;
  float3 vector;

  /* Runtime. */
  ImageManager *image_manager;
  int is_float;
  bool compress_as_srgb;
  ustring known_colorspace;
};

class EnvironmentTextureNode : public ImageSlotTextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(EnvironmentTextureNode)
  ~EnvironmentTextureNode();
  ShaderNode *clone() const;
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  virtual bool equals(const ShaderNode &other)
  {
    const EnvironmentTextureNode &env_node = (const EnvironmentTextureNode &)other;
    return ImageSlotTextureNode::equals(other) && builtin_data == env_node.builtin_data &&
           animated == env_node.animated;
  }

  /* Parameters. */
  ustring filename;
  void *builtin_data;
  ustring colorspace;
  ImageAlphaType alpha_type;
  NodeEnvironmentProjection projection;
  InterpolationType interpolation;
  bool animated;
  float3 vector;

  /* Runtime. */
  ImageManager *image_manager;
  int is_float;
  bool compress_as_srgb;
  ustring known_colorspace;
};

class SkyTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(SkyTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NodeSkyType type;
  float3 sun_direction;
  float turbidity;
  float ground_albedo;
  float3 vector;
};

class OutputNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(OutputNode)

  void *surface;
  void *volume;
  float3 displacement;
  float3 normal;

  /* Don't allow output node de-duplication. */
  virtual bool equals(const ShaderNode & /*other*/)
  {
    return false;
  }
};

class GradientTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(GradientTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NodeGradientType type;
  float3 vector;
};

class NoiseTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(NoiseTextureNode)

  float scale, detail, distortion;
  float3 vector;
};

class VoronoiTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(VoronoiTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NodeVoronoiColoring coloring;
  NodeVoronoiDistanceMetric metric;
  NodeVoronoiFeature feature;
  float scale, exponent;
  float3 vector;
};

class MusgraveTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(MusgraveTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NodeMusgraveType type;
  float scale, detail, dimension, lacunarity, offset, gain;
  float3 vector;
};

class WaveTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(WaveTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NodeWaveType type;
  NodeWaveProfile profile;

  float scale, distortion, detail, detail_scale;
  float3 vector;
};

class MagicTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(MagicTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  int depth;
  float3 vector;
  float scale, distortion;
};

class CheckerTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(CheckerTextureNode)

  float3 vector, color1, color2;
  float scale;

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }
};

class BrickTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(BrickTextureNode)

  float offset, squash;
  int offset_frequency, squash_frequency;

  float3 color1, color2, mortar;
  float scale, mortar_size, mortar_smooth, bias, brick_width, row_height;
  float3 vector;

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }
};

class PointDensityTextureNode : public ShaderNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(PointDensityTextureNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  ~PointDensityTextureNode();
  ShaderNode *clone() const;
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }

  bool has_spatial_varying()
  {
    return true;
  }
  bool has_object_dependency()
  {
    return true;
  }

  void add_image();

  /* Parameters. */
  ustring filename;
  NodeTexVoxelSpace space;
  InterpolationType interpolation;
  Transform tfm;
  float3 vector;
  void *builtin_data;

  /* Runtime. */
  ImageManager *image_manager;
  int slot;

  virtual bool equals(const ShaderNode &other)
  {
    const PointDensityTextureNode &point_dendity_node = (const PointDensityTextureNode &)other;
    return ShaderNode::equals(other) && builtin_data == point_dendity_node.builtin_data;
  }
};

class IESLightNode : public TextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(IESLightNode)

  ~IESLightNode();
  ShaderNode *clone() const;
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  ustring filename;
  ustring ies;

  float strength;
  float3 vector;

 private:
  LightManager *light_manager;
  int slot;

  void get_slot();
};

class MappingNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MappingNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  float3 vector;
  TextureMapping tex_mapping;
};

class RGBToBWNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(RGBToBWNode)
  void constant_fold(const ConstantFolder &folder);

  float3 color;
};

class ConvertNode : public ShaderNode {
 public:
  ConvertNode(SocketType::Type from, SocketType::Type to, bool autoconvert = false);
  SHADER_NODE_BASE_CLASS(ConvertNode)

  void constant_fold(const ConstantFolder &folder);

  SocketType::Type from, to;

  union {
    float value_float;
    int value_int;
    float3 value_color;
    float3 value_vector;
    float3 value_point;
    float3 value_normal;
  };
  ustring value_string;

 private:
  static const int MAX_TYPE = 12;
  static bool register_types();
  static Node *create(const NodeType *type);
  static const NodeType *node_types[MAX_TYPE][MAX_TYPE];
  static bool initialized;
};

class BsdfBaseNode : public ShaderNode {
 public:
  BsdfBaseNode(const NodeType *node_type);

  bool has_spatial_varying()
  {
    return true;
  }
  virtual ClosureType get_closure_type()
  {
    return closure;
  }
  virtual bool has_bump();

  virtual bool equals(const ShaderNode & /*other*/)
  {
    /* TODO(sergey): With some care BSDF nodes can be de-duplicated. */
    return false;
  }

  ClosureType closure;
};

class BsdfNode : public BsdfBaseNode {
 public:
  explicit BsdfNode(const NodeType *node_type);
  SHADER_NODE_BASE_CLASS(BsdfNode)

  void compile(SVMCompiler &compiler,
               ShaderInput *param1,
               ShaderInput *param2,
               ShaderInput *param3 = NULL,
               ShaderInput *param4 = NULL);

  float3 color;
  float3 normal;
  float surface_mix_weight;
};

class AnisotropicBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(AnisotropicBsdfNode)

  float3 tangent;
  float roughness, anisotropy, rotation;
  ClosureType distribution;

  ClosureType get_closure_type()
  {
    return distribution;
  }
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
};

class DiffuseBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(DiffuseBsdfNode)

  float roughness;
};

/* Disney principled BRDF */
class PrincipledBsdfNode : public BsdfBaseNode {
 public:
  SHADER_NODE_CLASS(PrincipledBsdfNode)

  void expand(ShaderGraph *graph);
  bool has_surface_bssrdf();
  bool has_bssrdf_bump();
  void compile(SVMCompiler &compiler,
               ShaderInput *metallic,
               ShaderInput *subsurface,
               ShaderInput *subsurface_radius,
               ShaderInput *specular,
               ShaderInput *roughness,
               ShaderInput *specular_tint,
               ShaderInput *anisotropic,
               ShaderInput *sheen,
               ShaderInput *sheen_tint,
               ShaderInput *clearcoat,
               ShaderInput *clearcoat_roughness,
               ShaderInput *ior,
               ShaderInput *transmission,
               ShaderInput *anisotropic_rotation,
               ShaderInput *transmission_roughness);

  float3 base_color;
  float3 subsurface_color, subsurface_radius;
  float metallic, subsurface, specular, roughness, specular_tint, anisotropic, sheen, sheen_tint,
      clearcoat, clearcoat_roughness, ior, transmission, anisotropic_rotation,
      transmission_roughness;
  float3 normal, clearcoat_normal, tangent;
  float surface_mix_weight;
  ClosureType distribution, distribution_orig;
  ClosureType subsurface_method;
  float3 emission;
  float alpha;

  bool has_integrator_dependency();
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
};

class TranslucentBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(TranslucentBsdfNode)
};

class TransparentBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(TransparentBsdfNode)

  bool has_surface_transparent()
  {
    return true;
  }
};

class VelvetBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(VelvetBsdfNode)

  float sigma;
};

class GlossyBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(GlossyBsdfNode)

  void simplify_settings(Scene *scene);
  bool has_integrator_dependency();
  ClosureType get_closure_type()
  {
    return distribution;
  }

  float roughness, roughness_orig;
  ClosureType distribution, distribution_orig;
};

class GlassBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(GlassBsdfNode)

  void simplify_settings(Scene *scene);
  bool has_integrator_dependency();
  ClosureType get_closure_type()
  {
    return distribution;
  }

  float roughness, roughness_orig, IOR;
  ClosureType distribution, distribution_orig;
};

class RefractionBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(RefractionBsdfNode)

  void simplify_settings(Scene *scene);
  bool has_integrator_dependency();
  ClosureType get_closure_type()
  {
    return distribution;
  }

  float roughness, roughness_orig, IOR;
  ClosureType distribution, distribution_orig;
};

class ToonBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(ToonBsdfNode)

  float smooth, size;
  ClosureType component;
};

class SubsurfaceScatteringNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(SubsurfaceScatteringNode)
  bool has_surface_bssrdf()
  {
    return true;
  }
  bool has_bssrdf_bump();
  ClosureType get_closure_type()
  {
    return falloff;
  }

  float scale;
  float3 radius;
  float sharpness;
  float texture_blur;
  ClosureType falloff;
};

class EmissionNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(EmissionNode)
  void constant_fold(const ConstantFolder &folder);

  bool has_surface_emission()
  {
    return true;
  }
  bool has_volume_support()
  {
    return true;
  }

  float3 color;
  float strength;
  float surface_mix_weight;
};

class BackgroundNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BackgroundNode)
  void constant_fold(const ConstantFolder &folder);

  float3 color;
  float strength;
  float surface_mix_weight;
};

class HoldoutNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(HoldoutNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
  virtual ClosureType get_closure_type()
  {
    return CLOSURE_HOLDOUT_ID;
  }

  float surface_mix_weight;
  float volume_mix_weight;
};

class AmbientOcclusionNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(AmbientOcclusionNode)

  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }
  virtual bool has_raytrace()
  {
    return true;
  }

  float3 color;
  float distance;
  float3 normal;
  int samples;

  bool only_local;
  bool inside;
};

class VolumeNode : public ShaderNode {
 public:
  VolumeNode(const NodeType *node_type);
  SHADER_NODE_BASE_CLASS(VolumeNode)

  void compile(SVMCompiler &compiler, ShaderInput *param1, ShaderInput *param2);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
  virtual int get_feature()
  {
    return ShaderNode::get_feature() | NODE_FEATURE_VOLUME;
  }
  virtual ClosureType get_closure_type()
  {
    return closure;
  }
  virtual bool has_volume_support()
  {
    return true;
  }

  float3 color;
  float density;
  float volume_mix_weight;
  ClosureType closure;

  virtual bool equals(const ShaderNode & /*other*/)
  {
    /* TODO(sergey): With some care Volume nodes can be de-duplicated. */
    return false;
  }
};

class AbsorptionVolumeNode : public VolumeNode {
 public:
  SHADER_NODE_CLASS(AbsorptionVolumeNode)
};

class ScatterVolumeNode : public VolumeNode {
 public:
  SHADER_NODE_CLASS(ScatterVolumeNode)

  float anisotropy;
};

class PrincipledVolumeNode : public VolumeNode {
 public:
  SHADER_NODE_CLASS(PrincipledVolumeNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }

  ustring density_attribute;
  ustring color_attribute;
  ustring temperature_attribute;

  float anisotropy;
  float3 absorption_color;
  float emission_strength;
  float3 emission_color;
  float blackbody_intensity;
  float3 blackbody_tint;
  float temperature;
};

/* Interface between the I/O sockets and the SVM/OSL backend. */
class PrincipledHairBsdfNode : public BsdfBaseNode {
 public:
  SHADER_NODE_CLASS(PrincipledHairBsdfNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);

  /* Longitudinal roughness. */
  float roughness;
  /* Azimuthal roughness. */
  float radial_roughness;
  /* Randomization factor for roughnesses. */
  float random_roughness;
  /* Longitudinal roughness factor for only the diffuse bounce (shiny undercoat). */
  float coat;
  /* Index of reflection. */
  float ior;
  /* Cuticle tilt angle. */
  float offset;
  /* Direct coloring's color. */
  float3 color;
  /* Melanin concentration. */
  float melanin;
  /* Melanin redness ratio. */
  float melanin_redness;
  /* Dye color. */
  float3 tint;
  /* Randomization factor for melanin quantities. */
  float random_color;
  /* Absorption coefficient (unfiltered). */
  float3 absorption_coefficient;

  float3 normal;
  float surface_mix_weight;
  /* If linked, here will be the given random number. */
  float random;
  /* Selected coloring parametrization. */
  NodePrincipledHairParametrization parametrization;
};

class HairBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(HairBsdfNode)
  ClosureType get_closure_type()
  {
    return component;
  }

  ClosureType component;
  float offset;
  float roughness_u;
  float roughness_v;
  float3 tangent;
};

class GeometryNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(GeometryNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }
  int get_group();

  float3 normal_osl;
};

class TextureCoordinateNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(TextureCoordinateNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }
  bool has_object_dependency()
  {
    return use_transform;
  }

  float3 normal_osl;
  bool from_dupli;
  bool use_transform;
  Transform ob_tfm;
};

class UVMapNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(UVMapNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }

  ustring attribute;
  bool from_dupli;
};

class LightPathNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(LightPathNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
};

class LightFalloffNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(LightFalloffNode)
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  float strength;
  float smooth;
};

class ObjectInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ObjectInfoNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
};

class ParticleInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ParticleInfoNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
};

class HairInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(HairInfoNode)

  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
  virtual int get_feature()
  {
    return ShaderNode::get_feature() | NODE_FEATURE_HAIR;
  }
};

class ValueNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ValueNode)

  void constant_fold(const ConstantFolder &folder);

  float value;
};

class ColorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ColorNode)

  void constant_fold(const ConstantFolder &folder);

  float3 value;
};

class AddClosureNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(AddClosureNode)
  void constant_fold(const ConstantFolder &folder);
};

class MixClosureNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixClosureNode)
  void constant_fold(const ConstantFolder &folder);

  float fac;
};

class MixClosureWeightNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixClosureWeightNode)

  float weight;
  float fac;
};

class InvertNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(InvertNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float fac;
  float3 color;
};

class MixNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixNode)
  void constant_fold(const ConstantFolder &folder);

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NodeMix type;
  bool use_clamp;
  float3 color1;
  float3 color2;
  float fac;
};

class CombineRGBNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CombineRGBNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float r, g, b;
};

class CombineHSVNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CombineHSVNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float h, s, v;
};

class CombineXYZNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CombineXYZNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float x, y, z;
};

class GammaNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(GammaNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }

  float3 color;
  float gamma;
};

class BrightContrastNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BrightContrastNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }

  float3 color;
  float bright;
  float contrast;
};

class SeparateRGBNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SeparateRGBNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float3 color;
};

class SeparateHSVNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SeparateHSVNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float3 color;
};

class SeparateXYZNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SeparateXYZNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float3 vector;
};

class HSVNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(HSVNode)

  float hue;
  float saturation;
  float value;
  float fac;
  float3 color;
};

class AttributeNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(AttributeNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }

  ustring attribute;
};

class CameraNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CameraNode)
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }
};

class FresnelNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(FresnelNode)
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }

  float3 normal;
  float IOR;
};

class LayerWeightNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(LayerWeightNode)
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }

  float3 normal;
  float blend;
};

class WireframeNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(WireframeNode)
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float size;
  bool use_pixel_size;
};

class WavelengthNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(WavelengthNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float wavelength;
};

class BlackbodyNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BlackbodyNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  float temperature;
};

class MathNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MathNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
  void constant_fold(const ConstantFolder &folder);

  float value1;
  float value2;
  NodeMath type;
  bool use_clamp;
};

class NormalNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(NormalNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  float3 direction;
  float3 normal;
};

class VectorMathNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorMathNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
  void constant_fold(const ConstantFolder &folder);

  float3 vector1;
  float3 vector2;
  NodeVectorMath type;
};

class VectorTransformNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorTransformNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NodeVectorTransformType type;
  NodeVectorTransformConvertSpace convert_from;
  NodeVectorTransformConvertSpace convert_to;
  float3 vector;
};

class BumpNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BumpNode)
  void constant_fold(const ConstantFolder &folder);
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_feature()
  {
    return NODE_FEATURE_BUMP;
  }

  bool invert;
  bool use_object_space;
  float height;
  float sample_center;
  float sample_x;
  float sample_y;
  float3 normal;
  float strength;
  float distance;
};

class CurvesNode : public ShaderNode {
 public:
  explicit CurvesNode(const NodeType *node_type);
  SHADER_NODE_BASE_CLASS(CurvesNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  array<float3> curves;
  float min_x, max_x, fac;
  float3 value;

 protected:
  using ShaderNode::constant_fold;
  void constant_fold(const ConstantFolder &folder, ShaderInput *value_in);
  void compile(SVMCompiler &compiler, int type, ShaderInput *value_in, ShaderOutput *value_out);
  void compile(OSLCompiler &compiler, const char *name);
};

class RGBCurvesNode : public CurvesNode {
 public:
  SHADER_NODE_CLASS(RGBCurvesNode)
  void constant_fold(const ConstantFolder &folder);
};

class VectorCurvesNode : public CurvesNode {
 public:
  SHADER_NODE_CLASS(VectorCurvesNode)
  void constant_fold(const ConstantFolder &folder);
};

class RGBRampNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(RGBRampNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }

  array<float3> ramp;
  array<float> ramp_alpha;
  float fac;
  bool interpolate;
};

class SetNormalNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SetNormalNode)
  float3 direction;
};

class OSLNode : public ShaderNode {
 public:
  static OSLNode *create(size_t num_inputs, const OSLNode *from = NULL);
  ~OSLNode();

  ShaderNode *clone() const;

  char *input_default_value();
  void add_input(ustring name, SocketType::Type type);
  void add_output(ustring name, SocketType::Type type);

  SHADER_NODE_NO_CLONE_CLASS(OSLNode)

  /* ideally we could beter detect this, but we can't query this now */
  bool has_spatial_varying()
  {
    return true;
  }
  bool has_volume_support()
  {
    return true;
  }

  virtual bool equals(const ShaderNode & /*other*/)
  {
    return false;
  }

  string filepath;
  string bytecode_hash;
};

class NormalMapNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(NormalMapNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NodeNormalMapSpace space;
  ustring attribute;
  float strength;
  float3 color;
  float3 normal_osl;
};

class TangentNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(TangentNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NodeTangentDirectionType direction_type;
  NodeTangentAxis axis;
  ustring attribute;
  float3 normal_osl;
};

class BevelNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BevelNode)
  bool has_spatial_varying()
  {
    return true;
  }
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }
  virtual bool has_raytrace()
  {
    return true;
  }

  float radius;
  float3 normal;
  int samples;
};

class DisplacementNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(DisplacementNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_feature()
  {
    return NODE_FEATURE_BUMP;
  }

  NodeNormalMapSpace space;
  float height;
  float midlevel;
  float scale;
  float3 normal;
};

class VectorDisplacementNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorDisplacementNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  void constant_fold(const ConstantFolder &folder);
  virtual int get_feature()
  {
    return NODE_FEATURE_BUMP;
  }

  NodeNormalMapSpace space;
  ustring attribute;
  float3 vector;
  float midlevel;
  float scale;
};

CCL_NAMESPACE_END

#endif /* __NODES_H__ */
