/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "graph/node.h"
#include "kernel/svm/types.h"
#include "scene/image.h"
#include "scene/shader_graph.h"

#include "util/array.h"
#include "util/string.h"
#include "util/unique_ptr.h"

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
  void compile(SVMCompiler &compiler, const int offset_in, const int offset_out);
  int compile(SVMCompiler &compiler, ShaderInput *vector_in);
  void compile(OSLCompiler &compiler);

  int compile_begin(SVMCompiler &compiler, ShaderInput *vector_in);
  void compile_end(SVMCompiler &compiler, ShaderInput *vector_in, const int vector_offset);

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
  explicit TextureNode(const NodeType *node_type) : ShaderNode(node_type) {}
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

  bool equals(const ShaderNode &other) override
  {
    const ImageSlotTextureNode &other_node = (const ImageSlotTextureNode &)other;
    return TextureNode::equals(other) && handle == other_node.handle;
  }

  ImageHandle handle;
};

class ImageTextureNode : public ImageSlotTextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(ImageTextureNode)
  ShaderNode *clone(ShaderGraph *graph) const override;
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }

  bool equals(const ShaderNode &other) override
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
  NODE_SOCKET_API_ARRAY(array<int>, tiles)

 protected:
  void cull_tiles(Scene *scene, ShaderGraph *graph);
};

class EnvironmentTextureNode : public ImageSlotTextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(EnvironmentTextureNode)
  ShaderNode *clone(ShaderGraph *graph) const override;
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }

  bool equals(const ShaderNode &other) override
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
  NODE_SOCKET_API(float, aerosol_density)
  NODE_SOCKET_API(float, ozone_density)
  NODE_SOCKET_API(float3, vector)
  ImageHandle handle;

  void simplify_settings(Scene *scene) override;

  float get_sun_size()
  {
    /* Clamping for numerical precision. */
    return fmaxf(sun_size, 0.0005f);
  }

  float get_sun_average_radiance();
};

class OutputNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(OutputNode)

  NODE_SOCKET_API(Node *, surface)
  NODE_SOCKET_API(Node *, volume)
  NODE_SOCKET_API(float3, displacement)
  NODE_SOCKET_API(float3, normal)

  /* Don't allow output node de-duplication. */
  bool equals(const ShaderNode & /*other*/) override
  {
    return false;
  }

  bool is_linear_operation() override
  {
    return true;
  }
};

class OutputAOVNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(OutputAOVNode)
  void simplify_settings(Scene *scene) override;

  NODE_SOCKET_API(float, value)
  NODE_SOCKET_API(float3, color)

  NODE_SOCKET_API(ustring, name)

  /* Don't allow output node de-duplication. */
  bool equals(const ShaderNode & /*other*/) override
  {
    return false;
  }

  bool is_linear_operation() override
  {
    return true;
  }

  int offset;
  bool is_color;
};

class GradientTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(GradientTextureNode)

  NODE_SOCKET_API(NodeGradientType, gradient_type)
  NODE_SOCKET_API(float3, vector)
};

class NoiseTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(NoiseTextureNode)

  NODE_SOCKET_API(int, dimensions)
  NODE_SOCKET_API(NodeNoiseType, type)
  NODE_SOCKET_API(bool, use_normalize)
  NODE_SOCKET_API(float, w)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, detail)
  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, lacunarity)
  NODE_SOCKET_API(float, offset)
  NODE_SOCKET_API(float, gain)
  NODE_SOCKET_API(float, distortion)
  NODE_SOCKET_API(float3, vector)
};

class GaborTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(GaborTextureNode)

  NODE_SOCKET_API(NodeGaborType, type)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, frequency)
  NODE_SOCKET_API(float, anisotropy)
  NODE_SOCKET_API(float, orientation_2d)
  NODE_SOCKET_API(float3, orientation_3d)
};

class VoronoiTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(VoronoiTextureNode)

  uint get_feature() override
  {
    int result = ShaderNode::get_feature();
    if (dimensions == 4) {
      result |= KERNEL_FEATURE_NODE_VORONOI_EXTRA;
    }
    else if (dimensions >= 2 && feature == NODE_VORONOI_SMOOTH_F1) {
      result |= KERNEL_FEATURE_NODE_VORONOI_EXTRA;
    }
    return result;
  }

  NODE_SOCKET_API(int, dimensions)
  NODE_SOCKET_API(NodeVoronoiDistanceMetric, metric)
  NODE_SOCKET_API(NodeVoronoiFeature, feature)
  NODE_SOCKET_API(bool, use_normalize)
  NODE_SOCKET_API(float, w)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float, detail)
  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, lacunarity)
  NODE_SOCKET_API(float, exponent)
  NODE_SOCKET_API(float, smoothness)
  NODE_SOCKET_API(float, randomness)
  NODE_SOCKET_API(float3, vector)
};

class WaveTextureNode : public TextureNode {
 public:
  SHADER_NODE_CLASS(WaveTextureNode)

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
};

class IESLightNode : public TextureNode {
 public:
  SHADER_NODE_NO_CLONE_CLASS(IESLightNode)

  ~IESLightNode() override;
  ShaderNode *clone(ShaderGraph *graph) const override;

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

  NODE_SOCKET_API(int, dimensions)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float, w)
};

class MappingNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MappingNode)
  void constant_fold(const ConstantFolder &folder) override;

  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float3, location)
  NODE_SOCKET_API(float3, rotation)
  NODE_SOCKET_API(float3, scale)
  NODE_SOCKET_API(NodeMappingType, mapping_type)
};

class RGBToBWNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(RGBToBWNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float3, color)
};

class ConvertNode : public ShaderNode {
 public:
  ConvertNode(SocketType::Type from, SocketType::Type to, bool autoconvert = false);
  ConvertNode(const ConvertNode &other);
  SHADER_NODE_BASE_CLASS(ConvertNode)

  void constant_fold(const ConstantFolder &folder) override;

  bool is_linear_operation() override
  {
    return true;
  }

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

  static const int MAX_TYPE = 13;
  static bool register_types();
  static unique_ptr<Node> create(const NodeType *type);
  static const NodeType *node_types[MAX_TYPE][MAX_TYPE];
  static bool initialized;
};

class BsdfBaseNode : public ShaderNode {
 public:
  BsdfBaseNode(const NodeType *node_type);

  bool is_linear_operation() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }
  ClosureType get_closure_type() override
  {
    return closure;
  }
  bool has_bump() override;

  bool equals(const ShaderNode & /*other*/) override
  {
    /* TODO(sergey): With some care BSDF nodes can be de-duplicated. */
    return false;
  }

  uint get_feature() override
  {
    return ShaderNode::get_feature() | KERNEL_FEATURE_NODE_BSDF;
  }

 protected:
  ClosureType closure;
};

class BsdfNode : public BsdfBaseNode {
 public:
  explicit BsdfNode(const NodeType *node_type);
  SHADER_NODE_BASE_CLASS(BsdfNode)

  void compile(SVMCompiler &compiler,
               ShaderInput *bsdf_y,
               ShaderInput *bsdf_z,
               ShaderInput *data_y = nullptr,
               ShaderInput *data_z = nullptr,
               ShaderInput *data_w = nullptr);

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, surface_mix_weight)
};

class DiffuseBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(DiffuseBsdfNode)
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float, roughness)
};

/* Disney principled BRDF */
class PrincipledBsdfNode : public BsdfBaseNode {
 public:
  SHADER_NODE_CLASS(PrincipledBsdfNode)

  bool has_surface_bssrdf() override;
  bool has_bssrdf_bump() override;
  void simplify_settings(Scene *scene) override;

  NODE_SOCKET_API(float3, base_color)
  NODE_SOCKET_API(float, metallic)
  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, ior)
  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, alpha)
  NODE_SOCKET_API(float, diffuse_roughness)
  NODE_SOCKET_API(ClosureType, subsurface_method)
  NODE_SOCKET_API(float, subsurface_weight)
  NODE_SOCKET_API(float3, subsurface_radius)
  NODE_SOCKET_API(float, subsurface_scale)
  NODE_SOCKET_API(float, subsurface_ior)
  NODE_SOCKET_API(float, subsurface_anisotropy)
  NODE_SOCKET_API(ClosureType, distribution)
  NODE_SOCKET_API(float, specular_ior_level)
  NODE_SOCKET_API(float3, specular_tint)
  NODE_SOCKET_API(float, anisotropic)
  NODE_SOCKET_API(float, anisotropic_rotation)
  NODE_SOCKET_API(float3, tangent)
  NODE_SOCKET_API(float, transmission_weight)
  NODE_SOCKET_API(float, sheen_weight)
  NODE_SOCKET_API(float, sheen_roughness)
  NODE_SOCKET_API(float3, sheen_tint)
  NODE_SOCKET_API(float, coat_weight)
  NODE_SOCKET_API(float, coat_roughness)
  NODE_SOCKET_API(float, coat_ior)
  NODE_SOCKET_API(float3, coat_tint)
  NODE_SOCKET_API(float3, coat_normal)
  NODE_SOCKET_API(float3, emission_color)
  NODE_SOCKET_API(float, emission_strength)
  NODE_SOCKET_API(float, surface_mix_weight)
  NODE_SOCKET_API(float, thin_film_thickness)
  NODE_SOCKET_API(float, thin_film_ior)

 public:
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_surface_transparent() override;
  bool has_surface_emission() override;

 protected:
  /* Checks whether the given weight input is potentially non-zero. */
  bool has_nonzero_weight(const char *name);
};

class TranslucentBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(TranslucentBsdfNode)
};

class TransparentBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(TransparentBsdfNode)

  bool has_surface_transparent() override
  {
    return true;
  }
};

class RayPortalBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(RayPortalBsdfNode)

  NODE_SOCKET_API(float3, position)
  NODE_SOCKET_API(float3, direction)

  bool has_surface_transparent() override
  {
    return true;
  }

  uint get_feature() override
  {
    return BsdfNode::get_feature() | KERNEL_FEATURE_NODE_PORTAL;
  }
};

class SheenBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(SheenBsdfNode)

  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(ClosureType, distribution)

  ClosureType get_closure_type() override
  {
    return distribution;
  }
};

class MetallicBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(MetallicBsdfNode)

  void simplify_settings(Scene *scene) override;
  ClosureType get_closure_type() override
  {
    return closure;
  }

  NODE_SOCKET_API(float3, edge_tint)
  NODE_SOCKET_API(float3, ior)
  NODE_SOCKET_API(float3, k)
  NODE_SOCKET_API(float3, tangent)
  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, anisotropy)
  NODE_SOCKET_API(float, rotation)
  NODE_SOCKET_API(float, thin_film_thickness)
  NODE_SOCKET_API(float, thin_film_ior)
  NODE_SOCKET_API(ClosureType, distribution)
  NODE_SOCKET_API(ClosureType, fresnel_type)

  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }

  bool is_isotropic();
};

class GlossyBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(GlossyBsdfNode)

  void simplify_settings(Scene *scene) override;
  ClosureType get_closure_type() override
  {
    return distribution;
  }

  NODE_SOCKET_API(float3, tangent)
  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, anisotropy)
  NODE_SOCKET_API(float, rotation)
  NODE_SOCKET_API(ClosureType, distribution)

  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }

  bool is_isotropic();
};

class GlassBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(GlassBsdfNode)

  ClosureType get_closure_type() override
  {
    return distribution;
  }

  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, IOR)
  NODE_SOCKET_API(float, thin_film_thickness)
  NODE_SOCKET_API(float, thin_film_ior)
  NODE_SOCKET_API(ClosureType, distribution)
};

class RefractionBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(RefractionBsdfNode)

  ClosureType get_closure_type() override
  {
    return distribution;
  }

  NODE_SOCKET_API(float, roughness)
  NODE_SOCKET_API(float, IOR)
  NODE_SOCKET_API(ClosureType, distribution)
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
  bool has_surface_bssrdf() override
  {
    return true;
  }
  bool has_bssrdf_bump() override;
  ClosureType get_closure_type() override
  {
    return method;
  }

  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(float3, radius)
  NODE_SOCKET_API(float, subsurface_ior)
  NODE_SOCKET_API(float, subsurface_roughness)
  NODE_SOCKET_API(float, subsurface_anisotropy)
  NODE_SOCKET_API(ClosureType, method)
};

class EmissionNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(EmissionNode)
  void constant_fold(const ConstantFolder &folder) override;

  bool has_surface_emission() override
  {
    return true;
  }
  bool has_volume_support() override
  {
    return true;
  }
  bool is_linear_operation() override
  {
    return true;
  }

  uint get_feature() override
  {
    return ShaderNode::get_feature() | KERNEL_FEATURE_NODE_EMISSION;
  }

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float, surface_mix_weight)
  NODE_SOCKET_API(float, volume_mix_weight)

  bool from_auto_conversion = false;
};

class BackgroundNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BackgroundNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }

  uint get_feature() override
  {
    return ShaderNode::get_feature() | KERNEL_FEATURE_NODE_EMISSION;
  }

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float, surface_mix_weight)
};

class HoldoutNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(HoldoutNode)
  ClosureType get_closure_type() override
  {
    return CLOSURE_HOLDOUT_ID;
  }
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float, surface_mix_weight)
  NODE_SOCKET_API(float, volume_mix_weight)
};

class AmbientOcclusionNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(AmbientOcclusionNode)

  bool has_spatial_varying() override
  {
    return true;
  }
  uint get_feature() override
  {
    return KERNEL_FEATURE_NODE_RAYTRACE;
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
  bool is_linear_operation() override
  {
    return true;
  }

  void compile(SVMCompiler &compiler,
               ShaderInput *density,
               ShaderInput *param1 = nullptr,
               ShaderInput *param2 = nullptr);
  uint get_feature() override
  {
    return ShaderNode::get_feature() | KERNEL_FEATURE_NODE_VOLUME;
  }
  ClosureType get_closure_type() override
  {
    return closure;
  }
  bool has_volume_support() override
  {
    return true;
  }

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, density)
  NODE_SOCKET_API(float, volume_mix_weight)

 protected:
  ClosureType closure;

 public:
  bool equals(const ShaderNode & /*other*/) override
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
  ScatterVolumeNode(const NodeType *node_type);
  SHADER_NODE_CLASS(ScatterVolumeNode)

  NODE_SOCKET_API(float, anisotropy)
  NODE_SOCKET_API(float, IOR)
  NODE_SOCKET_API(float, backscatter)
  NODE_SOCKET_API(float, alpha)
  NODE_SOCKET_API(float, diameter)
  NODE_SOCKET_API(ClosureType, phase)
};

class VolumeCoefficientsNode : public ScatterVolumeNode {
 public:
  SHADER_NODE_CLASS(VolumeCoefficientsNode)

  NODE_SOCKET_API(float3, scatter_coeffs)
  NODE_SOCKET_API(float3, absorption_coeffs)
  NODE_SOCKET_API(float3, emission_coeffs)
};

class PrincipledVolumeNode : public VolumeNode {
 public:
  SHADER_NODE_CLASS(PrincipledVolumeNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
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
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;

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

  /* Aspect Ratio. */
  NODE_SOCKET_API(float, aspect_ratio)

  /* Optional modulation factors for the lobes. */
  NODE_SOCKET_API(float, R)
  NODE_SOCKET_API(float, TT)
  NODE_SOCKET_API(float, TRT)

  /* Weight for mix shader. */
  NODE_SOCKET_API(float, surface_mix_weight)
  /* If linked, here will be the given random number. */
  NODE_SOCKET_API(float, random)
  /* Selected coloring parametrization. */
  NODE_SOCKET_API(NodePrincipledHairParametrization, parametrization)
  /* Selected scattering model (chiang/huang). */
  NODE_SOCKET_API(NodePrincipledHairModel, model)

  uint get_feature() override
  {
    return ccl::BsdfBaseNode::get_feature() | KERNEL_FEATURE_NODE_PRINCIPLED_HAIR;
  }

  bool has_surface_transparent() override;
};

class HairBsdfNode : public BsdfNode {
 public:
  SHADER_NODE_CLASS(HairBsdfNode)
  ClosureType get_closure_type() override
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
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }
  int get_group();
};

class TextureCoordinateNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(TextureCoordinateNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(bool, from_dupli)
  NODE_SOCKET_API(bool, use_transform)
  NODE_SOCKET_API(Transform, ob_tfm)
};

class UVMapNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(UVMapNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(ustring, attribute)
  NODE_SOCKET_API(bool, from_dupli)
};

class LightPathNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(LightPathNode)
};

class LightFalloffNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(LightFalloffNode)
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float, smooth)
};

class ObjectInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ObjectInfoNode)
};

class ParticleInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ParticleInfoNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
};

class HairInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(HairInfoNode)

  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }
};

class PointInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(PointInfoNode)

  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }
};

class VolumeInfoNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VolumeInfoNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }
  void expand(ShaderGraph *graph) override;
};

class VertexColorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VertexColorNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(ustring, layer_name)
};

class ValueNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ValueNode)

  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float, value)
};

class ColorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(ColorNode)

  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float3, value)
};

class AddClosureNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(AddClosureNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }
};

class MixClosureNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixClosureNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float, fac)
};

class MixClosureWeightNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixClosureWeightNode)
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float, weight)
  NODE_SOCKET_API(float, fac)
};

class InvertNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(InvertNode)
  void constant_fold(const ConstantFolder &folder) override;

  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(float3, color)

  bool is_linear_operation() override
  {
    return true;
  }
};

class MixNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override;

  NODE_SOCKET_API(NodeMix, mix_type)
  NODE_SOCKET_API(bool, use_clamp)
  NODE_SOCKET_API(float3, color1)
  NODE_SOCKET_API(float3, color2)
  NODE_SOCKET_API(float, fac)
};

class MixColorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixColorNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override;

  NODE_SOCKET_API(float3, a)
  NODE_SOCKET_API(float3, b)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(bool, use_clamp)
  NODE_SOCKET_API(bool, use_clamp_result)
  NODE_SOCKET_API(NodeMix, blend_type)
};

class MixFloatNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixFloatNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override;

  NODE_SOCKET_API(float, a)
  NODE_SOCKET_API(float, b)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(bool, use_clamp)
};

class MixVectorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixVectorNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override;

  NODE_SOCKET_API(float3, a)
  NODE_SOCKET_API(float3, b)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(bool, use_clamp)
};

class MixVectorNonUniformNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MixVectorNonUniformNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override;

  NODE_SOCKET_API(float3, a)
  NODE_SOCKET_API(float3, b)
  NODE_SOCKET_API(float3, fac)
  NODE_SOCKET_API(bool, use_clamp)
};

class CombineColorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CombineColorNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(NodeCombSepColorType, color_type)
  NODE_SOCKET_API(float, r)
  NODE_SOCKET_API(float, g)
  NODE_SOCKET_API(float, b)
};

class CombineXYZNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CombineXYZNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float, x)
  NODE_SOCKET_API(float, y)
  NODE_SOCKET_API(float, z)
};

class GammaNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(GammaNode)
  void constant_fold(const ConstantFolder &folder) override;

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, gamma)
};

class BrightContrastNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BrightContrastNode)
  void constant_fold(const ConstantFolder &folder) override;

  NODE_SOCKET_API(float3, color)
  NODE_SOCKET_API(float, bright)
  NODE_SOCKET_API(float, contrast)
};

class SeparateColorNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SeparateColorNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(NodeCombSepColorType, color_type)
  NODE_SOCKET_API(float3, color)
};

class SeparateXYZNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SeparateXYZNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override
  {
    return true;
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
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(ustring, attribute)

  bool stochastic_sample = true;
};

class CameraNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(CameraNode)
  bool has_spatial_varying() override
  {
    return true;
  }
};

class FresnelNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(FresnelNode)
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, IOR)
};

class LayerWeightNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(LayerWeightNode)
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(float, blend)
};

class WireframeNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(WireframeNode)
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(float, size)
  NODE_SOCKET_API(bool, use_pixel_size)
};

class WavelengthNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(WavelengthNode)

  NODE_SOCKET_API(float, wavelength)
};

class BlackbodyNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BlackbodyNode)
  void constant_fold(const ConstantFolder &folder) override;

  NODE_SOCKET_API(float, temperature)
};

class VectorMapRangeNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorMapRangeNode)
  void expand(ShaderGraph *graph) override;
  bool is_linear_operation() override;

  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float3, from_min)
  NODE_SOCKET_API(float3, from_max)
  NODE_SOCKET_API(float3, to_min)
  NODE_SOCKET_API(float3, to_max)
  NODE_SOCKET_API(float3, steps)
  NODE_SOCKET_API(NodeMapRangeType, range_type)
  NODE_SOCKET_API(bool, use_clamp)
};

class MapRangeNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MapRangeNode)
  void expand(ShaderGraph *graph) override;
  bool is_linear_operation() override;

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
  void constant_fold(const ConstantFolder &folder) override;
  NODE_SOCKET_API(float, value)
  NODE_SOCKET_API(float, min)
  NODE_SOCKET_API(float, max)
  NODE_SOCKET_API(NodeClampType, clamp_type)
};

class MathNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(MathNode)
  void expand(ShaderGraph *graph) override;
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override;

  NODE_SOCKET_API(float, value1)
  NODE_SOCKET_API(float, value2)
  NODE_SOCKET_API(float, value3)
  NODE_SOCKET_API(NodeMathType, math_type)
  NODE_SOCKET_API(bool, use_clamp)
};

class NormalNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(NormalNode)
  bool is_linear_operation() override
  {
    return true;
  }

  NODE_SOCKET_API(float3, direction)
  NODE_SOCKET_API(float3, normal)
};

class VectorMathNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorMathNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool is_linear_operation() override;

  NODE_SOCKET_API(float3, vector1)
  NODE_SOCKET_API(float3, vector2)
  NODE_SOCKET_API(float3, vector3)
  NODE_SOCKET_API(float, scale)
  NODE_SOCKET_API(NodeVectorMathType, math_type)
};

class VectorRotateNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(VectorRotateNode)

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

  NODE_SOCKET_API(NodeVectorTransformType, transform_type)
  NODE_SOCKET_API(NodeVectorTransformConvertSpace, convert_from)
  NODE_SOCKET_API(NodeVectorTransformConvertSpace, convert_to)
  NODE_SOCKET_API(float3, vector)
};

class BumpNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BumpNode)
  void constant_fold(const ConstantFolder &folder) override;
  bool has_spatial_varying() override
  {
    return true;
  }
  uint get_feature() override
  {
    return KERNEL_FEATURE_NODE_BUMP;
  }

  NODE_SOCKET_API(bool, invert)
  NODE_SOCKET_API(bool, use_object_space)
  NODE_SOCKET_API(float, height)
  NODE_SOCKET_API(float, filter_width)
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

  NODE_SOCKET_API_ARRAY(array<float3>, curves)
  NODE_SOCKET_API(float, min_x)
  NODE_SOCKET_API(float, max_x)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(float3, value)
  NODE_SOCKET_API(bool, extrapolate)

 protected:
  using ShaderNode::constant_fold;
  void constant_fold(const ConstantFolder &folder, ShaderInput *value_in);
  void compile(SVMCompiler &compiler,
               const int type,
               ShaderInput *value_in,
               ShaderOutput *value_out);
  void compile(OSLCompiler &compiler, const char *name);
};

class RGBCurvesNode : public CurvesNode {
 public:
  SHADER_NODE_CLASS(RGBCurvesNode)
  void constant_fold(const ConstantFolder &folder) override;
};

class VectorCurvesNode : public CurvesNode {
 public:
  SHADER_NODE_CLASS(VectorCurvesNode)
  void constant_fold(const ConstantFolder &folder) override;
};

class FloatCurveNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(FloatCurveNode)
  void constant_fold(const ConstantFolder &folder) override;

  NODE_SOCKET_API_ARRAY(array<float>, curve)
  NODE_SOCKET_API(float, min_x)
  NODE_SOCKET_API(float, max_x)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(float, value)
  NODE_SOCKET_API(bool, extrapolate)
};

class RGBRampNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(RGBRampNode)
  void constant_fold(const ConstantFolder &folder) override;

  NODE_SOCKET_API_ARRAY(array<float3>, ramp)
  NODE_SOCKET_API_ARRAY(array<float>, ramp_alpha)
  NODE_SOCKET_API(float, fac)
  NODE_SOCKET_API(bool, interpolate)
};

class SetNormalNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(SetNormalNode)
  NODE_SOCKET_API(float3, direction)
};

class OSLNode final : public ShaderNode {
 public:
  static OSLNode *create(ShaderGraph *graph,
                         const size_t num_inputs,
                         const OSLNode *from = nullptr);
  ~OSLNode() override;

  static void operator delete(void *ptr)
  {
    /* Override delete operator to silence new-delete-type-mismatch ASAN warnings
     * regarding size mismatch in the destructor. This is intentional as we allocate
     * extra space at the end of the node. */
    ::operator delete(ptr);
  }
  static void operator delete(void * /*unused*/, void * /*unused*/)
  {
    /* Deliberately empty placement delete operator, to avoid MSVC warning C4291. */
  }

  ShaderNode *clone(ShaderGraph *graph) const override;

  void attributes(Shader *shader, AttributeRequestSet *attributes) override;

  char *input_default_value();
  void add_input(ustring name, SocketType::Type type, const int flags = 0);
  void add_output(ustring name, SocketType::Type type);

  SHADER_NODE_NO_CLONE_CLASS(OSLNode)

  bool has_surface_emission() override
  {
    return has_emission;
  }

  /* Ideally we could better detect this, but we can't query this now. */
  bool has_spatial_varying() override
  {
    return true;
  }
  bool has_volume_support() override
  {
    return true;
  }
  uint get_feature() override
  {
    return ShaderNode::get_feature() | KERNEL_FEATURE_NODE_RAYTRACE;
  }

  bool equals(const ShaderNode & /*other*/) override
  {
    return false;
  }

  string filepath;
  string bytecode_hash;
  bool has_emission;
};

class NormalMapNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(NormalMapNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(NodeNormalMapSpace, space)
  NODE_SOCKET_API(ustring, attribute)
  NODE_SOCKET_API(float, strength)
  NODE_SOCKET_API(float3, color)
};

class RadialTilingNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(RadialTilingNode)

  NODE_SOCKET_API(bool, use_normalize)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float, r_gon_sides)
  NODE_SOCKET_API(float, r_gon_roundness)
};

class TangentNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(TangentNode)
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  bool has_spatial_varying() override
  {
    return true;
  }

  NODE_SOCKET_API(NodeTangentDirectionType, direction_type)
  NODE_SOCKET_API(NodeTangentAxis, axis)
  NODE_SOCKET_API(ustring, attribute)
};

class BevelNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(BevelNode)
  bool has_spatial_varying() override
  {
    return true;
  }
  uint get_feature() override
  {
    return KERNEL_FEATURE_NODE_RAYTRACE;
  }

  NODE_SOCKET_API(float, radius)
  NODE_SOCKET_API(float3, normal)
  NODE_SOCKET_API(int, samples)
};

class DisplacementNode : public ShaderNode {
 public:
  SHADER_NODE_CLASS(DisplacementNode)
  void constant_fold(const ConstantFolder &folder) override;
  uint get_feature() override
  {
    return KERNEL_FEATURE_NODE_BUMP;
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
  void attributes(Shader *shader, AttributeRequestSet *attributes) override;
  bool has_attribute_dependency() override
  {
    return true;
  }
  void constant_fold(const ConstantFolder &folder) override;
  uint get_feature() override
  {
    return KERNEL_FEATURE_NODE_BUMP;
  }

  NODE_SOCKET_API(NodeNormalMapSpace, space)
  NODE_SOCKET_API(ustring, attribute)
  NODE_SOCKET_API(float3, vector)
  NODE_SOCKET_API(float, midlevel)
  NODE_SOCKET_API(float, scale)
};

CCL_NAMESPACE_END
