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

/* Any node which uses image manager's slot should be a subclass of this one. */
class ImageSlotNode : public ShaderNode {
public:
	ImageSlotNode(const char *name_) : ShaderNode(name_) {
		special_type = SHADER_SPECIAL_TYPE_IMAGE_SLOT;
	}
	int slot;
};

class TextureNode : public ShaderNode {
public:
	TextureNode(const char *name_) : ShaderNode(name_) {}
	TextureMapping tex_mapping;
};

class ImageSlotTextureNode : public ImageSlotNode {
public:
	ImageSlotTextureNode(const char *name_) : ImageSlotNode(name_) {}
	TextureMapping tex_mapping;
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

	static ShaderEnum color_space_enum;
	static ShaderEnum projection_enum;
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

	static ShaderEnum color_space_enum;
	static ShaderEnum projection_enum;
};

class SkyTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(SkyTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	float3 sun_direction;
	float turbidity;
	float ground_albedo;
	
	ustring type;
	static ShaderEnum type_enum;
};

class OutputNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(OutputNode)
};

class GradientTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(GradientTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	ustring type;
	static ShaderEnum type_enum;
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

	static ShaderEnum coloring_enum;
};

class MusgraveTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(MusgraveTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	ustring type;

	static ShaderEnum type_enum;
};

class WaveTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(WaveTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	ustring type;
	static ShaderEnum type_enum;
};

class MagicTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(MagicTextureNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	int depth;
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

	static ShaderEnum space_enum;
};

class MappingNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MappingNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	TextureMapping tex_mapping;
};

class ConvertNode : public ShaderNode {
public:
	ConvertNode(ShaderSocketType from, ShaderSocketType to, bool autoconvert = false);
	SHADER_NODE_BASE_CLASS(ConvertNode)

	bool constant_fold(ShaderOutput *socket, float3 *optimized_value);

	ShaderSocketType from, to;
};

class ProxyNode : public ShaderNode {
public:
	ProxyNode(ShaderSocketType type);
	SHADER_NODE_BASE_CLASS(ProxyNode)

	ShaderSocketType type;
};

class BsdfNode : public ShaderNode {
public:
	BsdfNode(bool scattering = false);
	SHADER_NODE_BASE_CLASS(BsdfNode);

	bool has_spatial_varying() { return true; }
	void compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2, ShaderInput *param3 = NULL, ShaderInput *param4 = NULL);

	ClosureType closure;
	bool scattering;
};

class AnisotropicBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(AnisotropicBsdfNode)

	ustring distribution;
	static ShaderEnum distribution_enum;

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
	static ShaderEnum distribution_enum;
};

class GlassBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(GlassBsdfNode)

	void simplify_settings(Scene *scene);
	bool has_integrator_dependency();

	ustring distribution, distribution_orig;
	static ShaderEnum distribution_enum;
};

class RefractionBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(RefractionBsdfNode)

	void simplify_settings(Scene *scene);
	bool has_integrator_dependency();

	ustring distribution, distribution_orig;
	static ShaderEnum distribution_enum;
};

class ToonBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(ToonBsdfNode)

	ustring component;
	static ShaderEnum component_enum;
};

class SubsurfaceScatteringNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(SubsurfaceScatteringNode)
	bool has_surface_bssrdf() { return true; }
	bool has_bssrdf_bump();
	bool has_spatial_varying() { return true; }

	static ShaderEnum falloff_enum;
};

class EmissionNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(EmissionNode)

	bool has_surface_emission() { return true; }
	bool has_spatial_varying() { return true; }
};

class BackgroundNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BackgroundNode)
};

class HoldoutNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(HoldoutNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class AmbientOcclusionNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(AmbientOcclusionNode)

	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
};

class VolumeNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VolumeNode)

	void compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2);
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }

	ClosureType closure;
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
	static ShaderEnum component_enum;

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
};

class UVMapNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(UVMapNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }

	ustring attribute;
	bool from_dupli;
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

	bool constant_fold(ShaderOutput *socket, float3 *optimized_value);

	float value;
};

class ColorNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(ColorNode)

	bool constant_fold(ShaderOutput *socket, float3 *optimized_value);

	float3 value;
};

class AddClosureNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(AddClosureNode)
};

class MixClosureNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MixClosureNode)
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

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	bool use_clamp;

	ustring type;
	static ShaderEnum type_enum;
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

	bool constant_fold(ShaderOutput *socket, float3 *optimized_value);

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
	bool constant_fold(ShaderOutput *socket, float3 *optimized_value);

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }
};

class MathNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MathNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	bool constant_fold(ShaderOutput *socket, float3 *optimized_value);

	bool use_clamp;

	ustring type;
	static ShaderEnum type_enum;
};

class NormalNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(NormalNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_2; }

	float3 direction;
};

class VectorMathNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VectorMathNode)
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
	bool constant_fold(ShaderOutput *socket, float3 *optimized_value);

	ustring type;
	static ShaderEnum type_enum;
};

class VectorTransformNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VectorTransformNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	ustring type;
	ustring convert_from;
	ustring convert_to;
	
	static ShaderEnum type_enum;
	static ShaderEnum convert_space_enum;
};

class BumpNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BumpNode)
	bool has_spatial_varying() { return true; }
	virtual int get_feature() {
		return NODE_FEATURE_BUMP;
	}

	bool invert;
};

class RGBCurvesNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(RGBCurvesNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	float4 curves[RAMP_TABLE_SIZE];
	float min_x, max_x;
};

class VectorCurvesNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VectorCurvesNode)

	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	float4 curves[RAMP_TABLE_SIZE];
};

class RGBRampNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(RGBRampNode)
	float4 ramp[RAMP_TABLE_SIZE];
	bool interpolate;
	virtual int get_group() { return NODE_GROUP_LEVEL_1; }
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
	
	/* ShaderInput/ShaderOutput only stores a shallow string copy (const char *)!
	 * The actual socket names have to be stored externally to avoid memory errors. */
	vector<ustring> input_names;
	vector<ustring> output_names;
};

class NormalMapNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(NormalMapNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	ustring space;
	static ShaderEnum space_enum;

	ustring attribute;
};

class TangentNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(TangentNode)
	void attributes(Shader *shader, AttributeRequestSet *attributes);
	bool has_spatial_varying() { return true; }
	virtual int get_group() { return NODE_GROUP_LEVEL_3; }

	ustring direction_type;
	static ShaderEnum direction_type_enum;

	ustring axis;
	static ShaderEnum axis_enum;

	ustring attribute;
};

CCL_NAMESPACE_END

#endif /* __NODES_H__ */

