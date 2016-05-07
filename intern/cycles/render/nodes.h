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

#include "graph.h"

#include "util_string.h"

CCL_NAMESPACE_BEGIN

class ImageManager;
class Scene;
class Shader;

/* Texture Mapping */

class TextureMapping {
public:
	TextureMapping();
	Transform compute_transform();
	bool skip();
	void compile(SVMCompiler& compiler, int offset_in, int offset_out);
	void compile(OSLCompiler &compiler);

	int compile_begin(SVMCompiler& compiler, ShaderInput *vector_in);
	void compile_end(SVMCompiler& compiler, ShaderInput *vector_in, int vector_offset);

	float3 translation;
	float3 rotation;
	float3 scale;

	float3 min, max;
	bool use_minmax;

	enum Type { POINT = 0, TEXTURE = 1, VECTOR = 2, NORMAL = 3 };
	Type type;
	static NodeEnum type_enum;

	enum Mapping { NONE = 0, X = 1, Y = 2, Z = 3 };
	Mapping x_mapping, y_mapping, z_mapping;
	static NodeEnum mapping_enum;

	enum Projection { FLAT, CUBE, TUBE, SPHERE };
	Projection projection;
	static NodeEnum projection_enum;

	bool equals(const TextureMapping& other) {
		return translation == other.translation &&
		       rotation == other.rotation &&
		       scale == other.scale &&
		       use_minmax == other.use_minmax &&
		       min == other.min &&
		       max == other.max &&
		       type == other.type &&
		       x_mapping == other.x_mapping &&
		       y_mapping == other.y_mapping &&
		       z_mapping == other.z_mapping &&
		       projection == other.projection;
	}
};

/* Nodes */

class TextureNode : public ShaderNode {
public:
	explicit TextureNode(const char *name_) : ShaderNode(name_) {}
	TextureMapping tex_mapping;

	virtual bool equals(const ShaderNode *other) {
		return ShaderNode::equals(other) &&
		       tex_mapping.equals(((const TextureNode*)other)->tex_mapping);
	}
};

/* Any node which uses image manager's slot should be a subclass of this one. */
class ImageSlotTextureNode : public TextureNode {
public:
	explicit ImageSlotTextureNode(const char *name_) : TextureNode(name_) {
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

	ImageManager *image_manager;
	int is_float;
	bool is_linear;
	bool use_alpha;
	string filename;
	void *builtin_data;
	ustring color_space;
	ustring projection;
	InterpolationType interpolation;
	ExtensionType extension;
	float projection_blend;
	bool animated;

	static NodeEnum color_space_enum;
	static NodeEnum projection_enum;

	virtual bool equals(const ShaderNode *other) {
		const ImageTextureNode *image_node = (const ImageTextureNode*)other;
		return ImageSlotTextureNode::equals(other) &&
		       use_alpha == image_node->use_alpha &&
		       filename == image_node->filename &&
		       builtin_data == image_node->builtin_data &&
		       color_space == image_node->color_space &&
		       projection == image_node->projection &&
		       interpolation == image_node->interpolation &&
		       extension == image_node->extension &&
		       projection_blend == image_node->projection_blend &&
		       animated == image_node->animated;
	}
};

class EnvironmentTextureNode : public ImageSlotTextureNode {
public:
	SHADER_NODE_NO_CLONE_CLASS(EnvironmentTextureNode)
	~EnvironmentTextureNode();
	ShaderNode *clone() const;
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	ImageManager *image_manager;
	int is_float;
	bool is_linear;
	bool use_alpha;
	string filename;
	void *builtin_data;
	ustring color_space;
	ustring projection;
	InterpolationType interpolation;
	bool animated;

	static NodeEnum color_space_enum;
	static NodeEnum projection_enum;

	virtual bool equals(const ShaderNode *other) {
		const EnvironmentTextureNode *env_node = (const EnvironmentTextureNode*)other;
		return ImageSlotTextureNode::equals(other) &&
		       use_alpha == env_node->use_alpha &&
		       filename == env_node->filename &&
		       builtin_data == env_node->builtin_data &&
		       color_space == env_node->color_space &&
		       projection == env_node->projection &&
		       interpolation == env_node->interpolation &&
		       animated == env_node->animated;
	}
};

class SkyTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(SkyTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	float3 sun_direction;
	float turbidity;
	float ground_albedo;

	ustring type;
	static NodeEnum type_enum;

	virtual bool equals(const ShaderNode *other) {
		const SkyTextureNode *sky_node = (const SkyTextureNode*)other;
		return TextureNode::equals(other) &&
		       sun_direction == sky_node->sun_direction &&
		       turbidity == sky_node->turbidity &&
		       ground_albedo == sky_node->ground_albedo &&
		       type == sky_node->type;
	}
};

class OutputNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(OutputNode)

	/* Don't allow output node de-duplication. */
	virtual bool equals(const ShaderNode * /*other*/) { return false; }
};

class GradientTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(GradientTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	ustring type;
	static NodeEnum type_enum;

	virtual bool equals(const ShaderNode *other) {
		const GradientTextureNode *gradient_node = (const GradientTextureNode*)other;
		return TextureNode::equals(other) &&
		       type == gradient_node->type;
	}
};

class NoiseTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(NoiseTextureNode)
};

class VoronoiTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(VoronoiTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	ustring coloring;

	static NodeEnum coloring_enum;

	virtual bool equals(const ShaderNode *other) {
		const VoronoiTextureNode *voronoi_node = (const VoronoiTextureNode*)other;
		return TextureNode::equals(other) &&
		       coloring == voronoi_node->coloring;
	}
};

class MusgraveTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(MusgraveTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	ustring type;

	static NodeEnum type_enum;

	virtual bool equals(const ShaderNode *other) {
		const MusgraveTextureNode *musgrave_node = (const MusgraveTextureNode*)other;
		return TextureNode::equals(other) &&
		       type == musgrave_node->type;
	}
};

class WaveTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(WaveTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	ustring type;
	ustring profile;
	static NodeEnum type_enum;
	static NodeEnum profile_enum;

	virtual bool equals(const ShaderNode *other) {
		const WaveTextureNode *wave_node = (const WaveTextureNode*)other;
		return TextureNode::equals(other) &&
		       type == wave_node->type &&
		       profile == wave_node->profile;
	}
};

class MagicTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(MagicTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	int depth;

	virtual bool equals(const ShaderNode *other) {
		const MagicTextureNode *magic_node = (const MagicTextureNode*)other;
		return TextureNode::equals(other) &&
		       depth == magic_node->depth;
	}
};

class CheckerTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(CheckerTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }
};

class BrickTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(BrickTextureNode)

	float offset, squash;
	int offset_frequency, squash_frequency;

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	virtual bool equals(const ShaderNode *other) {
		const BrickTextureNode *brick_node = (const BrickTextureNode*)other;
		return TextureNode::equals(other) &&
		       offset == brick_node->offset &&
		       squash == brick_node->squash &&
		       offset_frequency == brick_node->offset_frequency &&
		       squash_frequency == brick_node->squash_frequency;
	}
};

class PointDensityTextureNode : public ShaderNode {
public:
	SHADER_NODE_NO_CLONE_CLASS(PointDensityTextureNode)

	~PointDensityTextureNode();
	ShaderNode *clone() const;
	void attributes(Shader *shader, AttributeRequestSet *attributes);

	bool has_spatial_varying() { return true; }
	bool has_object_dependency() { return true; }

	ImageManager *image_manager;
	int slot;
	string filename;
	ustring space;
	void *builtin_data;
	InterpolationType interpolation;

	Transform tfm;

	static NodeEnum space_enum;

	virtual bool equals(const ShaderNode *other) {
		const PointDensityTextureNode *point_dendity_node = (const PointDensityTextureNode*)other;
		return ShaderNode::equals(other) &&
		       filename == point_dendity_node->filename &&
		       space == point_dendity_node->space &&
		       builtin_data == point_dendity_node->builtin_data &&
		       interpolation == point_dendity_node->interpolation &&
		       tfm == point_dendity_node->tfm;
	}
};

class MappingNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MappingNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	TextureMapping tex_mapping;

	virtual bool equals(const ShaderNode *other) {
		const MappingNode *mapping_node = (const MappingNode*)other;
		return ShaderNode::equals(other) &&
		       tex_mapping.equals(mapping_node->tex_mapping);
	}
};

class ConvertNode : public ShaderNode {
public:
	ConvertNode(SocketType::Type from, SocketType::Type to, bool autoconvert = false);
	SHADER_NODE_BASE_CLASS(ConvertNode)

	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);

	SocketType::Type from, to;

	virtual bool equals(const ShaderNode *other)
	{
		const ConvertNode *convert_node = (const ConvertNode*)other;
		return ShaderNode::equals(other) &&
		       from == convert_node->from &&
		       to == convert_node->to;
	}
};

class BsdfNode : public ShaderNode {
public:
	explicit BsdfNode(bool scattering = false);
	SHADER_NODE_BASE_CLASS(BsdfNode);

	bool has_spatial_varying() { return true; }
	void compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2, ShaderInput *param3 = NULL, ShaderInput *param4 = NULL);
	virtual ClosureType get_closure_type() { return closure; }

	ClosureType closure;
	bool scattering;

	virtual bool equals(const ShaderNode * /*other*/)
	{
		/* TODO(sergey): With some care BSDF nodes can be de-duplicated. */
		return false;
	}
};

class AnisotropicBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(AnisotropicBsdfNode)

	ustring distribution;
	static NodeEnum distribution_enum;

	void attributes(Shader *shader, AttributeRequestSet *attributes);
};

class DiffuseBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(DiffuseBsdfNode)
};

class TranslucentBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(TranslucentBsdfNode)
};

class TransparentBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(TransparentBsdfNode)

	bool has_surface_transparent() { return true; }
};

class VelvetBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(VelvetBsdfNode)
};

class GlossyBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(GlossyBsdfNode)

	void simplify_settings(Scene *scene);
	bool has_integrator_dependency();

	ustring distribution, distribution_orig;
	static NodeEnum distribution_enum;
};

class GlassBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(GlassBsdfNode)

	void simplify_settings(Scene *scene);
	bool has_integrator_dependency();

	ustring distribution, distribution_orig;
	static NodeEnum distribution_enum;
};

class RefractionBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(RefractionBsdfNode)

	void simplify_settings(Scene *scene);
	bool has_integrator_dependency();

	ustring distribution, distribution_orig;
	static NodeEnum distribution_enum;
};

class ToonBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(ToonBsdfNode)

	ustring component;
	static NodeEnum component_enum;
};

class SubsurfaceScatteringNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(SubsurfaceScatteringNode)
	bool has_surface_bssrdf() { return true; }
	bool has_bssrdf_bump();

	static NodeEnum falloff_enum;
};

class EmissionNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(EmissionNode)
	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);
	virtual ClosureType get_closure_type() { return CLOSURE_EMISSION_ID; }

	bool has_surface_emission() { return true; }
};

class BackgroundNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BackgroundNode)
	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);
	virtual ClosureType get_closure_type() { return CLOSURE_BACKGROUND_ID; }
};

class HoldoutNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(HoldoutNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	virtual ClosureType get_closure_type() { return CLOSURE_HOLDOUT_ID; }
};

class AmbientOcclusionNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(AmbientOcclusionNode)

	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	virtual ClosureType get_closure_type() { return CLOSURE_AMBIENT_OCCLUSION_ID; }
};

class VolumeNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VolumeNode)

	void compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2);
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	virtual int get_feature() {
		return ShaderNode::get_feature() | NODE_FEATURE_VOLUME;
	}
	virtual ClosureType get_closure_type() { return closure; }

	ClosureType closure;

	virtual bool equals(const ShaderNode * /*other*/)
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
};

class HairBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(HairBsdfNode)

	ustring component;
	static NodeEnum component_enum;

};

class GeometryNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(GeometryNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
};

class TextureCoordinateNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(TextureCoordinateNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
	bool has_object_dependency() { return use_transform; }

	bool from_dupli;
	bool use_transform;
	Transform ob_tfm;

	virtual bool equals(const ShaderNode *other) {
		const TextureCoordinateNode *texco_node = (const TextureCoordinateNode*)other;
		return ShaderNode::equals(other) &&
		       from_dupli == texco_node->from_dupli &&
		       use_transform == texco_node->use_transform &&
		       ob_tfm == texco_node->ob_tfm;
	}
};

class UVMapNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(UVMapNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }

	ustring attribute;
	bool from_dupli;

	virtual bool equals(const ShaderNode *other) {
		const UVMapNode *uv_map_node = (const UVMapNode*)other;
		return ShaderNode::equals(other) &&
		       attribute == uv_map_node->attribute &&
		       from_dupli == uv_map_node->from_dupli;
	}
};

class LightPathNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(LightPathNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class LightFalloffNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(LightFalloffNode)
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_2; }
};

class ObjectInfoNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(ObjectInfoNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class ParticleInfoNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(ParticleInfoNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class HairInfoNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(HairInfoNode)

	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	virtual int get_feature() {
		return ShaderNode::get_feature() | NODE_FEATURE_HAIR;
	}
};

class ValueNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(ValueNode)

	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);

	float value;

	virtual bool equals(const ShaderNode *other) {
		const ValueNode *value_node = (const ValueNode*)other;
		return ShaderNode::equals(other) &&
		       value == value_node->value;
	}
};

class ColorNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(ColorNode)

	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);

	float3 value;

	virtual bool equals(const ShaderNode *other) {
		const ColorNode *color_node = (const ColorNode*)other;
		return ShaderNode::equals(other) &&
		       value == color_node->value;
	}
};

class AddClosureNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(AddClosureNode)
};

class MixClosureNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MixClosureNode)
	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);
};

class MixClosureWeightNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MixClosureWeightNode);
};

class InvertNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(InvertNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class MixNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MixNode)
	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	bool use_clamp;

	ustring type;
	static NodeEnum type_enum;

	virtual bool equals(const ShaderNode *other)
	{
		const MixNode *mix_node = (const MixNode*)other;
		return ShaderNode::equals(other) &&
		       use_clamp == mix_node->use_clamp &&
		       type == mix_node->type;
	}
};

class CombineRGBNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(CombineRGBNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class CombineHSVNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(CombineHSVNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class CombineXYZNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(CombineXYZNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class GammaNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(GammaNode)

	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);

	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class BrightContrastNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BrightContrastNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class SeparateRGBNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(SeparateRGBNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class SeparateHSVNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(SeparateHSVNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class SeparateXYZNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(SeparateXYZNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class HSVNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(HSVNode)
};

class AttributeNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(AttributeNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }

	ustring attribute;

	virtual bool equals(const ShaderNode *other) {
		const AttributeNode *color_node = (const AttributeNode*)other;
		return ShaderNode::equals(other) &&
		       attribute == color_node->attribute;
	}
};

class CameraNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(CameraNode)
	bool has_spatial_varying() { return true; }
};

class FresnelNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(FresnelNode)
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class LayerWeightNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(LayerWeightNode)
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class WireframeNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(WireframeNode)
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	bool use_pixel_size;
};

class WavelengthNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(WavelengthNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class BlackbodyNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BlackbodyNode)
	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class MathNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MathNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);

	bool use_clamp;

	ustring type;
	static NodeEnum type_enum;

	virtual bool equals(const ShaderNode *other)
	{
		const MathNode *math_node = (const MathNode*)other;
		return ShaderNode::equals(other) &&
		       use_clamp == math_node->use_clamp &&
		       type == math_node->type;
	}
};

class NormalNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(NormalNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	float3 direction;

	virtual bool equals(const ShaderNode *other)
	{
		const NormalNode *normal_node = (const NormalNode*)other;
		return ShaderNode::equals(other) &&
		       direction == normal_node->direction;
	}
};

class VectorMathNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VectorMathNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);

	ustring type;
	static NodeEnum type_enum;

	virtual bool equals(const ShaderNode *other)
	{
		const MathNode *math_node = (const MathNode*)other;
		return ShaderNode::equals(other) &&
		       type == math_node->type;
	}
};

class VectorTransformNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VectorTransformNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	ustring type;
	ustring convert_from;
	ustring convert_to;

	static NodeEnum type_enum;
	static NodeEnum convert_space_enum;

	virtual bool equals(const ShaderNode *other) {
		const VectorTransformNode *vector_transform_node = (const VectorTransformNode*)other;
		return ShaderNode::equals(other) &&
		       type == vector_transform_node->type &&
		       convert_from == vector_transform_node->convert_from &&
		       convert_to == vector_transform_node->convert_to;
	}
};

class BumpNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BumpNode)
	bool constant_fold(ShaderGraph *graph, ShaderOutput *socket, float3 *optimized_value);
	bool has_spatial_varying() { return true; }
	virtual int get_feature() {
		return NODE_FEATURE_BUMP;
	}

	bool invert;

	virtual bool equals(const ShaderNode *other) {
		const BumpNode *bump_node = (const BumpNode*)other;
		return ShaderNode::equals(other) &&
		       invert == bump_node->invert;
	}
};

class RGBCurvesNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(RGBCurvesNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
	virtual bool equals(const ShaderNode * /*other*/) { return false; }

	array<float3> curves;
	float min_x, max_x;
};

class VectorCurvesNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VectorCurvesNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
	virtual bool equals(const ShaderNode * /*other*/) { return false; }

	array<float3> curves;
	float min_x, max_x;
};

class RGBRampNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(RGBRampNode)
	array<float3> ramp;
	array<float> ramp_alpha;
	bool interpolate;
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	virtual bool equals(const ShaderNode * /*other*/) { return false; }
};

class SetNormalNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(SetNormalNode)
};

class OSLScriptNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(OSLScriptNode)

	/* ideally we could beter detect this, but we can't query this now */
	bool has_spatial_varying() { return true; }

	string filepath;
	string bytecode_hash;

	virtual bool equals(const ShaderNode * /*other*/) { return false; }
};

class NormalMapNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(NormalMapNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	ustring space;
	static NodeEnum space_enum;

	ustring attribute;

	virtual bool equals(const ShaderNode *other)
	{
		const NormalMapNode *normal_map_node = (const NormalMapNode*)other;
		return ShaderNode::equals(other) &&
		       space == normal_map_node->space &&
		       attribute == normal_map_node->attribute;
	}
};

class TangentNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(TangentNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	ustring direction_type;
	static NodeEnum direction_type_enum;

	ustring axis;
	static NodeEnum axis_enum;

	ustring attribute;

	virtual bool equals(const ShaderNode *other)
	{
		const TangentNode *tangent_node = (const TangentNode*)other;
		return ShaderNode::equals(other) &&
		       direction_type == tangent_node->direction_type &&
		       axis == tangent_node->axis &&
		       attribute == tangent_node->attribute;
	}
};

CCL_NAMESPACE_END

#endif /* __NODES_H__ */

