/*
 * Copyright 2011, Blender Foundation.
 *
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
 */

#ifndef __NODES_H__
#define __NODES_H__

#include "graph.h"

#include "util_string.h"

CCL_NAMESPACE_BEGIN

class ImageManager;
class Shadr;

/* Texture Mapping */

class TextureMapping {
public:
	TextureMapping();
	Transform compute_transform();
	bool skip();
	void compile(SVMCompiler& compiler, int offset_in, int offset_out);

	float3 translation;
	float3 rotation;
	float3 scale;

	enum Mapping { NONE=0, X=1, Y=2, Z=3 };
	Mapping x_mapping, y_mapping, z_mapping;

	enum Projection { FLAT, CUBE, TUBE, SPHERE };
	Projection projection;
};

/* Nodes */

class TextureNode : public ShaderNode {
public:
	TextureNode(const char *name) : ShaderNode(name) {}
	TextureMapping tex_mapping;
};

class ImageTextureNode : public TextureNode {
public:
	SHADER_NODE_NO_CLONE_CLASS(ImageTextureNode)
	~ImageTextureNode();
	ShaderNode *clone() const;

	ImageManager *image_manager;
	int slot;
	string filename;
	ustring color_space;

	static ShaderEnum color_space_enum;
};

class EnvironmentTextureNode : public TextureNode {
public:
	SHADER_NODE_NO_CLONE_CLASS(EnvironmentTextureNode)
	~EnvironmentTextureNode();
	ShaderNode *clone() const;

	ImageManager *image_manager;
	int slot;
	string filename;
	ustring color_space;

	static ShaderEnum color_space_enum;
};

class SkyTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(SkyTextureNode)

	float3 sun_direction;
	float turbidity;
};

class OutputNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(OutputNode)
};

class GradientTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(GradientTextureNode)

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

	ustring coloring;

	static ShaderEnum coloring_enum;
};

class MusgraveTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(MusgraveTextureNode)

	ustring type;

	static ShaderEnum type_enum;
};

class WaveTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(WaveTextureNode)

	ustring type;
	static ShaderEnum type_enum;
};

class MagicTextureNode : public TextureNode {
public:
	SHADER_NODE_CLASS(MagicTextureNode)

	int depth;
};

class MappingNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MappingNode)

	TextureMapping tex_mapping;
};

class ConvertNode : public ShaderNode {
public:
	ConvertNode(ShaderSocketType from, ShaderSocketType to);
	SHADER_NODE_BASE_CLASS(ConvertNode)

	ShaderSocketType from, to;
};

class ProxyNode : public ShaderNode {
public:
	ProxyNode(ShaderSocketType from, ShaderSocketType to);
	SHADER_NODE_BASE_CLASS(ProxyNode)

	ShaderSocketType from, to;
};

class BsdfNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BsdfNode)

	void compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2);

	ClosureType closure;
};

class WardBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(WardBsdfNode)
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
};

class VelvetBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(VelvetBsdfNode)
};

class GlossyBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(GlossyBsdfNode)

	ustring distribution;
	static ShaderEnum distribution_enum;
};

class GlassBsdfNode : public BsdfNode {
public:
	SHADER_NODE_CLASS(GlassBsdfNode)

	ustring distribution;
	static ShaderEnum distribution_enum;
};

class EmissionNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(EmissionNode)

	bool total_power;
};

class BackgroundNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BackgroundNode)
};

class HoldoutNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(HoldoutNode)
};

class VolumeNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VolumeNode)

	void compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2);

	ClosureType closure;
};

class TransparentVolumeNode : public VolumeNode {
public:
	SHADER_NODE_CLASS(TransparentVolumeNode)
};

class IsotropicVolumeNode : public VolumeNode {
public:
	SHADER_NODE_CLASS(IsotropicVolumeNode)
};

class GeometryNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(GeometryNode)
};

class TextureCoordinateNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(TextureCoordinateNode)
	void attributes(AttributeRequestSet *attributes);
};

class LightPathNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(LightPathNode)
};

class ValueNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(ValueNode)

	float value;
};

class ColorNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(ColorNode)

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

class InvertNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(InvertNode)
};

class MixNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MixNode)

	ustring type;
	static ShaderEnum type_enum;
};

class CombineRGBNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(CombineRGBNode)
};

class GammaNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(GammaNode)
};

class SeparateRGBNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(SeparateRGBNode)
};

class HSVNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(HSVNode)
};

class AttributeNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(AttributeNode)
	void attributes(AttributeRequestSet *attributes);

	ustring attribute;
};

class CameraNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(CameraNode)
};

class FresnelNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(FresnelNode)
};

class LayerWeightNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(LayerWeightNode)
};

class MathNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(MathNode)

	ustring type;
	static ShaderEnum type_enum;
};

class NormalNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(NormalNode)

	float3 direction;
};

class VectorMathNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(VectorMathNode)

	ustring type;
	static ShaderEnum type_enum;
};

class BumpNode : public ShaderNode {
public:
	SHADER_NODE_CLASS(BumpNode)
};

CCL_NAMESPACE_END

#endif /* __NODES_H__ */

