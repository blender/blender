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

#include "graph/node.h"
#include "render/graph.h"
#include "render/image.h"

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
  NODE_SOCKET_API_STRUCT_MEMBER(float3, tex_mapping, translation)
  NODE_SOCKET_API_STRUCT_MEMBER(float3, tex_mapping, rotation)
  NODE_SOCKET_API_STRUCT_MEMBER(float3, tex_mapping, scale)
  NODE_SOCKET_API_STRUCT_MEMBER(float3, tex_mapping, min)
  NODE_SOCKET_API_STRUCT_MEMBER(float3, tex_mapping, max)
  NODE_SOCKET_API_STRUCT_MEMBER(bool, tex_mapping, use_minmax)
  NODE_SOCKET_API_STRUCT_MEMBER(TextureMapping::Type, tex_mapping, type)
  NODE_SOCKET_API_STRUCT_MEMBER(TextureMapping::Mapping, tex_mapping, x_mapping)
  NODE_SOCKET_API_STRUCT_MEMBER(TextureMapping::Mapping, tex_mapping, y_mapping)
  NODE_SOCKET_API_STRUCT_MEMBER(TextureMapping::Mapping, tex_mapping, z_mapping)
  NODE_SOCKET_API_STRUCT_MEMBER(TextureMapping::Projection, tex_mapping, projection)
};

/* Any node which uses image manager's slot should be a subclass of this one. */
class ImageSlotTextureNode : public TextureNode {
 public:
  explicit ImageSlotTextureNode(const NodeType *node_type) : TextureNode(node_type)
  {
    special_type = SHADER_SPECIAL_TYPE_IMAGE_SLOT;
  }

  virtual bool equals(const ShaderNode &other)
  {
    const ImageSlotTextureNode &other_node = (const ImageSlotTextureNode &)other;
    return TextureNode::equals(other) && handle == other_node.handle;
  }

  ImageHandle handle;
};

class ImageTextureNode : public ImageSlotTextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(ImageTextureNode)
  ShaderNode *clone(ShaderGraph *graph) const;
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }

  virtual bool equals(const ShaderNode &other)
  {
    const ImageTextureNode &other_node = (const ImageTextureNode &)other;
    return ImageSlotTextureNode::equals(other) && animated == other_node.animated;
  }

  ImageParams image_params() const;

  /* Parameters. */
  NODE_SOCKET_API(ustring, filename)
  NODE_SOCKET_API(ustring, colorspace)
  NODE_SOCKET_API(ImageAlphaType, alpha_type)
  NODE_SOCKET_API(NodeImageProjection, projection)
  NODE_SOCKET_API(InterpolationType, interpolation)
  NODE_SOCKET_API(ExtensionType, extension)
  NODE_SOCKET_API(float, projection_blend)
  NODE_SOCKET_API(bool, animated)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(array<int>, tiles)

 protected:
  void cull_tiles(Scene *scene, ShaderGraph *graph);
};

class EnvironmentTextureNode : public ImageSlotTextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(EnvironmentTextureNode)
  ShaderNode *clone(ShaderGraph *graph) const;
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
    const EnvironmentTextureNode &other_node = (const EnvironmentTextureNode &)other;
    return ImageSlotTextureNode::equals(other) && animated == other_node.animated;
  }

  ImageParams image_params() const;

  /* Parameters. */
  NODE_SOCKET_API(ustring, filename)
  NODE_SOCKET_API(ustring, colorspace)
  NODE_SOCKET_API(ImageAlphaType, alpha_type)
  NODE_SOCKET_API(NodeEnvironmentProjection, projection)
  NODE_SOCKET_API(InterpolationType, interpolation)
  NODE_SOCKET_API(bool, animated)
  NODE_SOCKET_API(float3, vector)
};

class SkyTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(SkyTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NODE_SOCKET_API(NodeSkyType, sky_type)
  NODE_SOCKET_API(float3, sun_direction)
  NODE_SOCKET_API(float, turbidity)
  NODE_SOCKET_API(float, ground_albedo)
  NODE_SOCKET_API(bool, sun_disc)
  NODE_SOCKET_API(float, sun_size)
  NODE_SOCKET_API(float, sun_intensity)
  NODE_SOCKET_API(float, sun_elevation)
  NODE_SOCKET_API(float, sun_rotation)
  NODE_SOCKET_API(float, altitude)
  NODE_SOCKET_API(float, air_density)
  NODE_SOCKET_API(float, dust_density)
  NODE_SOCKET_API(float, ozone_density)
  NODE_SOCKET_API(float3, vector)
  ImageHandle handle;

  float get_sun_size()
  {
    /* Clamping for numerical precision. */
    return fmaxf(sun_size, 0.0005f);
  }
};

class OutputNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(OutputNode)

  NODE_SOCKET_API(Node *, surface)
  NODE_SOCKET_API(Node *, volume)
  NODE_SOCKET_API(float3, displacement)
  NODE_SOCKET_API(float3, normal)

  /* Don't allow output node de-duplication. */
  virtual bool equals(const ShaderNode & /*other*/)
  {
    return false;
  }
};

class OutputAOVNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(OutputAOVNode)
  virtual void simplify_settings(Scene *scene);

  NODE_SOCKET_API(float, value)
  NODE_SOCKET_API(float3, color)

  NODE_SOCKET_API(ustring, name)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_4;
  }

  /* Don't allow output node de-duplication. */
  virtual bool equals(const ShaderNode & /*other*/)
  {
    return false;
  }

  int slot;
  bool is_color;
};

class GradientTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(GradientTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NODE_SOCKET_API(NodeGradientType, gradient_type)
  NODE_SOCKET_API(float3, vector)
};

class NoiseTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(NoiseTextureNode)

  NODE_SOCKET_API(int, dimensions)
  NODE_SOCKET_API(float, w)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, detail)
  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, distortion)
  NODE_SOCKET_API(float3, vector)
};

class VoronoiTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(VoronoiTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  virtual int get_feature()
  {
    int result = ShaderNode::get_feature();
    if (dimensions == 4) {
      result |= NODE_FEATURE_VORONOI_EXTRA;
    }
    else if (dimensions >= 2 && feature == NODE_VORONOI_SMOOTH_F1) {
      result |= NODE_FEATURE_VORONOI_EXTRA;
    }
    return result;
  }

  NODE_SOCKET_API(int, dimensions)
  NODE_SOCKET_API(NodeVoronoiDistanceMetric, metric)
  NODE_SOCKET_API(NodeVoronoiFeature, feature)
  NODE_SOCKET_API(float, w)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, exponent)
  NODE_SOCKET_API(float, smoothness)
  NODE_SOCKET_API(float, randomness)
  NODE_SOCKET_API(float3, vector)
};

class MusgraveTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(MusgraveTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NODE_SOCKET_API(int, dimensions)
  NODE_SOCKET_API(NodeMusgraveType, musgrave_type)
  NODE_SOCKET_API(float, w)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, detail)
  NODE_SOCKET_API(float, dimension)
  NODE_SOCKET_API(float, lacunarity)
  NODE_SOCKET_API(float, offset)
  NODE_SOCKET_API(float, gain)
  NODE_SOCKET_API(float3, vector)
};

class WaveTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(WaveTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NODE_SOCKET_API(NodeWaveType, wave_type)
  NODE_SOCKET_API(NodeWaveBandsDirection, bands_direction)
  NODE_SOCKET_API(NodeWaveRingsDirection, rings_direction)
  NODE_SOCKET_API(NodeWaveProfile, profile)

  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, distortion)
  NODE_SOCKET_API(float, detail)
  NODE_SOCKET_API(float, detail_scale)
  NODE_SOCKET_API(float, detail_roughness)
  NODE_SOCKET_API(float, phase)
  NODE_SOCKET_API(float3, vector)
};

class MagicTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(MagicTextureNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NODE_SOCKET_API(int, depth)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, distortion)
};

class CheckerTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(CheckerTextureNode)

  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float3, color1)
  NODE_SOCKET_API(float3, color2)
  NODE_SOCKET_API(float, scale)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }
};

class BrickTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(BrickTextureNode)

  NODE_SOCKET_API(float, offset)
  NODE_SOCKET_API(float, squash)
  NODE_SOCKET_API(int, offset_frequency)
  NODE_SOCKET_API(int, squash_frequency)

  NODE_SOCKET_API(float3, color1)
  NODE_SOCKET_API(float3, color2)
  NODE_SOCKET_API(float3, mortar)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, mortar_size)
  NODE_SOCKET_API(float, mortar_smooth)
  NODE_SOCKET_API(float, bias)
  NODE_SOCKET_API(float, brick_width)
  NODE_SOCKET_API(float, row_height)
  NODE_SOCKET_API(float3, vector)

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
    return NODE_GROUP_LEVEL_4;
  }

  ~PointDensityTextureNode();
  ShaderNode *clone(ShaderGraph *graph) const;
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }

  bool has_spatial_varying()
  {
    return true;
  }

  /* Parameters. */
  NODE_SOCKET_API(ustring, filename)
  NODE_SOCKET_API(NodeTexVoxelSpace, space)
  NODE_SOCKET_API(InterpolationType, interpolation)
  NODE_SOCKET_API(Transform, tfm)
  NODE_SOCKET_API(float3, vector)

  /* Runtime. */
  ImageHandle handle;

  ImageParams image_params() const;

  virtual bool equals(const ShaderNode &other)
  {
    const PointDensityTextureNode &other_node = (const PointDensityTextureNode &)other;
    return ShaderNode::equals(other) && handle == other_node.handle;
  }
};

class IESLightNode : public TextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(IESLightNode)

  ~IESLightNode();
  ShaderNode *clone(ShaderGraph *graph) const;
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NODE_SOCKET_API(ustring, filename)
  NODE_SOCKET_API(ustring, ies)

  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float3, vector)

 private:
  LightManager *light_manager;
  int slot;

  void get_slot();
};

class WhiteNoiseTextureNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(WhiteNoiseTextureNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NODE_SOCKET_API(int, dimensions)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float, w)
};

class MappingNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MappingNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }
  void constant_fold(const ConstantFolder &folder);

  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float3, location)
  NODE_SOCKET_API(float3, rotation)
  NODE_SOCKET_API(float3, scale)
  NODE_SOCKET_API(NodeMappingType, mapping_type)
};

class RGBToBWNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(RGBToBWNode)
  void constant_fold(const ConstantFolder &folder);

  NODE_SOCKET_API(float3, color)
};

class ConvertNode : public ShaderNode {
 public:
  ConvertNode(SocketType::Type from, SocketType::Type to, bool autoconvert = false);
  SHADER_NODE_BASE_CLASS(ConvertNode)

  void constant_fold(const ConstantFolder &folder);

 private:
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

 protected:
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

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, surface_mix_weight)
};

class AnisotropicBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(AnisotropicBsdfNode)

  NODE_SOCKET_API(float3, tangent)
  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, anisotropy)
  NODE_SOCKET_API(float, rotation)
  NODE_SOCKET_API(ClosureType, distribution)

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

  NODE_SOCKET_API(float, roughness)
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

  NODE_SOCKET_API(float3, base_color)
  NODE_SOCKET_API(float3, subsurface_color)
  NODE_SOCKET_API(float3, subsurface_radius)
  NODE_SOCKET_API(float, metallic)
  NODE_SOCKET_API(float, subsurface)
  NODE_SOCKET_API(float, specular)
  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, specular_tint)
  NODE_SOCKET_API(float, anisotropic)
  NODE_SOCKET_API(float, sheen)
  NODE_SOCKET_API(float, sheen_tint)
  NODE_SOCKET_API(float, clearcoat)
  NODE_SOCKET_API(float, clearcoat_roughness)
  NODE_SOCKET_API(float, ior)
  NODE_SOCKET_API(float, transmission)
  NODE_SOCKET_API(float, anisotropic_rotation)
  NODE_SOCKET_API(float, transmission_roughness)
  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float3, clearcoat_normal)
  NODE_SOCKET_API(float3, tangent)
  NODE_SOCKET_API(float, surface_mix_weight)
  NODE_SOCKET_API(ClosureType, distribution)
  NODE_SOCKET_API(ClosureType, subsurface_method)
  NODE_SOCKET_API(float3, emission)
  NODE_SOCKET_API(float, emission_strength)
  NODE_SOCKET_API(float, alpha)

 private:
  ClosureType distribution_orig;

 public:
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

  NODE_SOCKET_API(float, sigma)
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

  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(ClosureType, distribution)

 private:
  float roughness_orig;
  ClosureType distribution_orig;
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

  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, IOR)
  NODE_SOCKET_API(ClosureType, distribution)

 private:
  float roughness_orig;
  ClosureType distribution_orig;
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

  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, IOR)
  NODE_SOCKET_API(ClosureType, distribution)

 private:
  float roughness_orig;
  ClosureType distribution_orig;
};

class ToonBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(ToonBsdfNode)

  NODE_SOCKET_API(float, smooth)
  NODE_SOCKET_API(float, size)
  NODE_SOCKET_API(ClosureType, component)
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

  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float3, radius)
  NODE_SOCKET_API(float, sharpness)
  NODE_SOCKET_API(float, texture_blur)
  NODE_SOCKET_API(ClosureType, falloff)
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

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float, surface_mix_weight)
};

class BackgroundNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BackgroundNode)
  void constant_fold(const ConstantFolder &folder);

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float, surface_mix_weight)
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

  NODE_SOCKET_API(float, surface_mix_weight)
  NODE_SOCKET_API(float, volume_mix_weight)
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

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, distance)
  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(int, samples)

  NODE_SOCKET_API(bool, only_local)
  NODE_SOCKET_API(bool, inside)
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

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, density)
  NODE_SOCKET_API(float, volume_mix_weight)

 protected:
  ClosureType closure;

 public:
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

  NODE_SOCKET_API(float, anisotropy)
};

class PrincipledVolumeNode : public VolumeNode {
 public:
  SHADER_NODE_CLASS(PrincipledVolumeNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }

  NODE_SOCKET_API(ustring, density_attribute)
  NODE_SOCKET_API(ustring, color_attribute)
  NODE_SOCKET_API(ustring, temperature_attribute)

  NODE_SOCKET_API(float, anisotropy)
  NODE_SOCKET_API(float3, absorption_color)
  NODE_SOCKET_API(float, emission_strength)
  NODE_SOCKET_API(float3, emission_color)
  NODE_SOCKET_API(float, blackbody_intensity)
  NODE_SOCKET_API(float3, blackbody_tint)
  NODE_SOCKET_API(float, temperature)
};

/* Interface between the I/O sockets and the SVM/OSL backend. */
class PrincipledHairBsdfNode : public BsdfBaseNode {
 public:
  SHADER_NODE_CLASS(PrincipledHairBsdfNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);

  /* Longitudinal roughness. */
  NODE_SOCKET_API(float, roughness)
  /* Azimuthal roughness. */
  NODE_SOCKET_API(float, radial_roughness)
  /* Randomization factor for roughnesses. */
  NODE_SOCKET_API(float, random_roughness)
  /* Longitudinal roughness factor for only the diffuse bounce (shiny undercoat). */
  NODE_SOCKET_API(float, coat)
  /* Index of reflection. */
  NODE_SOCKET_API(float, ior)
  /* Cuticle tilt angle. */
  NODE_SOCKET_API(float, offset)
  /* Direct coloring's color. */
  NODE_SOCKET_API(float3, color)
  /* Melanin concentration. */
  NODE_SOCKET_API(float, melanin)
  /* Melanin redness ratio. */
  NODE_SOCKET_API(float, melanin_redness)
  /* Dye color. */
  NODE_SOCKET_API(float3, tint)
  /* Randomization factor for melanin quantities. */
  NODE_SOCKET_API(float, random_color)
  /* Absorption coefficient (unfiltered). */
  NODE_SOCKET_API(float3, absorption_coefficient)

  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, surface_mix_weight)
  /* If linked, here will be the given random number. */
  NODE_SOCKET_API(float, random)
  /* Selected coloring parametrization. */
  NODE_SOCKET_API(NodePrincipledHairParametrization, parametrization)
};

class HairBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(HairBsdfNode)
  ClosureType get_closure_type()
  {
    return component;
  }

  NODE_SOCKET_API(ClosureType, component)
  NODE_SOCKET_API(float, offset)
  NODE_SOCKET_API(float, roughness_u)
  NODE_SOCKET_API(float, roughness_v)
  NODE_SOCKET_API(float3, tangent)
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

  NODE_SOCKET_API(float3, normal_osl)
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

  NODE_SOCKET_API(float3, normal_osl)
  NODE_SOCKET_API(bool, from_dupli)
  NODE_SOCKET_API(bool, use_transform)
  NODE_SOCKET_API(Transform, ob_tfm)
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

  NODE_SOCKET_API(ustring, attribute)
  NODE_SOCKET_API(bool, from_dupli)
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

  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float, smooth)
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

class VolumeInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VolumeInfoNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }
  void expand(ShaderGraph *graph);
};

class VertexColorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VertexColorNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes);
  bool has_attribute_dependency()
  {
    return true;
  }
  bool has_spatial_varying()
  {
    return true;
  }

  NODE_SOCKET_API(ustring, layer_name)
};

class ValueNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ValueNode)

  void constant_fold(const ConstantFolder &folder);

  NODE_SOCKET_API(float, value)
};

class ColorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ColorNode)

  void constant_fold(const ConstantFolder &folder);

  NODE_SOCKET_API(float3, value)
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

  NODE_SOCKET_API(float, fac)
};

class MixClosureWeightNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixClosureWeightNode)

  NODE_SOCKET_API(float, weight)
  NODE_SOCKET_API(float, fac)
};

class InvertNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(InvertNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(float3, color)
};

class MixNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixNode)
  void constant_fold(const ConstantFolder &folder);

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(NodeMix, mix_type)
  NODE_SOCKET_API(bool, use_clamp)
  NODE_SOCKET_API(float3, color1)
  NODE_SOCKET_API(float3, color2)
  NODE_SOCKET_API(float, fac)
};

class CombineRGBNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CombineRGBNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float, r)
  NODE_SOCKET_API(float, g)
  NODE_SOCKET_API(float, b)
};

class CombineHSVNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CombineHSVNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float, h)
  NODE_SOCKET_API(float, s)
  NODE_SOCKET_API(float, v)
};

class CombineXYZNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CombineXYZNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float, x)
  NODE_SOCKET_API(float, y)
  NODE_SOCKET_API(float, z)
};

class GammaNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(GammaNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, gamma)
};

class BrightContrastNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BrightContrastNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, bright)
  NODE_SOCKET_API(float, contrast)
};

class SeparateRGBNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SeparateRGBNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float3, color)
};

class SeparateHSVNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SeparateHSVNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float3, color)
};

class SeparateXYZNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SeparateXYZNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float3, vector)
};

class HSVNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(HSVNode)

  NODE_SOCKET_API(float, hue)
  NODE_SOCKET_API(float, saturation)
  NODE_SOCKET_API(float, value)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(float3, color)
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

  NODE_SOCKET_API(ustring, attribute)
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

  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, IOR)
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

  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, blend)
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

  NODE_SOCKET_API(float, size)
  NODE_SOCKET_API(bool, use_pixel_size)
};

class WavelengthNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(WavelengthNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float, wavelength)
};

class BlackbodyNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BlackbodyNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(float, temperature)
};

class MapRangeNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MapRangeNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }
  void expand(ShaderGraph *graph);

  NODE_SOCKET_API(float, value)
  NODE_SOCKET_API(float, from_min)
  NODE_SOCKET_API(float, from_max)
  NODE_SOCKET_API(float, to_min)
  NODE_SOCKET_API(float, to_max)
  NODE_SOCKET_API(float, steps)
  NODE_SOCKET_API(NodeMapRangeType, range_type)
  NODE_SOCKET_API(bool, clamp)
};

class ClampNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ClampNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }
  NODE_SOCKET_API(float, value)
  NODE_SOCKET_API(float, min)
  NODE_SOCKET_API(float, max)
  NODE_SOCKET_API(NodeClampType, clamp_type)
};

class MathNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MathNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
  void expand(ShaderGraph *graph);
  void constant_fold(const ConstantFolder &folder);

  NODE_SOCKET_API(float, value1)
  NODE_SOCKET_API(float, value2)
  NODE_SOCKET_API(float, value3)
  NODE_SOCKET_API(NodeMathType, math_type)
  NODE_SOCKET_API(bool, use_clamp)
};

class NormalNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(NormalNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_2;
  }

  NODE_SOCKET_API(float3, direction)
  NODE_SOCKET_API(float3, normal)
};

class VectorMathNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorMathNode)
  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_1;
  }
  void constant_fold(const ConstantFolder &folder);

  NODE_SOCKET_API(float3, vector1)
  NODE_SOCKET_API(float3, vector2)
  NODE_SOCKET_API(float3, vector3)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(NodeVectorMathType, math_type)
};

class VectorRotateNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorRotateNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }
  NODE_SOCKET_API(NodeVectorRotateType, rotate_type)
  NODE_SOCKET_API(bool, invert)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float3, center)
  NODE_SOCKET_API(float3, axis)
  NODE_SOCKET_API(float, angle)
  NODE_SOCKET_API(float3, rotation)
};

class VectorTransformNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorTransformNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(NodeVectorTransformType, transform_type)
  NODE_SOCKET_API(NodeVectorTransformConvertSpace, convert_from)
  NODE_SOCKET_API(NodeVectorTransformConvertSpace, convert_to)
  NODE_SOCKET_API(float3, vector)
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

  NODE_SOCKET_API(bool, invert)
  NODE_SOCKET_API(bool, use_object_space)
  NODE_SOCKET_API(float, height)
  NODE_SOCKET_API(float, sample_center)
  NODE_SOCKET_API(float, sample_x)
  NODE_SOCKET_API(float, sample_y)
  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float, distance)
};

class CurvesNode : public ShaderNode {
 public:
  explicit CurvesNode(const NodeType *node_type);
  SHADER_NODE_BASE_CLASS(CurvesNode)

  virtual int get_group()
  {
    return NODE_GROUP_LEVEL_3;
  }

  NODE_SOCKET_API(array<float3>, curves)
  NODE_SOCKET_API(float, min_x)
  NODE_SOCKET_API(float, max_x)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(float3, value)

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

  NODE_SOCKET_API(array<float3>, ramp)
  NODE_SOCKET_API(array<float>, ramp_alpha)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(bool, interpolate)
};

class SetNormalNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SetNormalNode)
  NODE_SOCKET_API(float3, direction)
};

class OSLNode : public ShaderNode {
 public:
  static OSLNode *create(ShaderGraph *graph, size_t num_inputs, const OSLNode *from = NULL);
  ~OSLNode();

  ShaderNode *clone(ShaderGraph *graph) const;

  char *input_default_value();
  void add_input(ustring name, SocketType::Type type);
  void add_output(ustring name, SocketType::Type type);

  SHADER_NODE_NO_CLONE_CLASS(OSLNode)

  /* Ideally we could better detect this, but we can't query this now. */
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

  NODE_SOCKET_API(NodeNormalMapSpace, space)
  NODE_SOCKET_API(ustring, attribute)
  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float3, normal_osl)
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

  NODE_SOCKET_API(NodeTangentDirectionType, direction_type)
  NODE_SOCKET_API(NodeTangentAxis, axis)
  NODE_SOCKET_API(ustring, attribute)
  NODE_SOCKET_API(float3, normal_osl)
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

  NODE_SOCKET_API(float, radius)
  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(int, samples)
};

class DisplacementNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(DisplacementNode)
  void constant_fold(const ConstantFolder &folder);
  virtual int get_feature()
  {
    return NODE_FEATURE_BUMP;
  }

  NODE_SOCKET_API(NodeNormalMapSpace, space)
  NODE_SOCKET_API(float, height)
  NODE_SOCKET_API(float, midlevel)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float3, normal)
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

  NODE_SOCKET_API(NodeNormalMapSpace, space)
  NODE_SOCKET_API(ustring, attribute)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float, midlevel)
  NODE_SOCKET_API(float, scale)
};

CCL_NAMESPACE_END

#endif /* __NODES_H__ */
