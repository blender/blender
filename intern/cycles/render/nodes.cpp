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

#include "image.h"
#include "nodes.h"
#include "svm.h"
#include "osl.h"

#include "util_transform.h"

CCL_NAMESPACE_BEGIN

/* Texture Mapping */

TextureMapping::TextureMapping()
{
	translation = make_float3(0.0f, 0.0f, 0.0f);
	rotation = make_float3(0.0f, 0.0f, 0.0f);
	scale = make_float3(1.0f, 1.0f, 1.0f);

	min = make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	max = make_float3(FLT_MAX, FLT_MAX, FLT_MAX);

	use_minmax = false;

	x_mapping = X;
	y_mapping = Y;
	z_mapping = Z;

	projection = FLAT;
}

Transform TextureMapping::compute_transform()
{
	Transform mmat = transform_scale(make_float3(0.0f, 0.0f, 0.0f));

	if(x_mapping != NONE)
		mmat[0][x_mapping-1] = 1.0f;
	if(y_mapping != NONE)
		mmat[1][y_mapping-1] = 1.0f;
	if(z_mapping != NONE)
		mmat[2][z_mapping-1] = 1.0f;

	Transform smat = transform_scale(scale);
	Transform rmat = transform_euler(rotation);
	Transform tmat = transform_translate(translation);

	return tmat*rmat*smat*mmat;
}

bool TextureMapping::skip()
{
	if(translation != make_float3(0.0f, 0.0f, 0.0f))
		return false;
	if(rotation != make_float3(0.0f, 0.0f, 0.0f))
		return false;
	if(scale != make_float3(1.0f, 1.0f, 1.0f))
		return false;
	
	if(x_mapping != X || y_mapping != Y || z_mapping != Z)
		return false;
	if(use_minmax)
		return false;
	
	return true;
}

void TextureMapping::compile(SVMCompiler& compiler, int offset_in, int offset_out)
{
	if(offset_in == SVM_STACK_INVALID || offset_out == SVM_STACK_INVALID)
		return;

	compiler.add_node(NODE_MAPPING, offset_in, offset_out);

	Transform tfm = compute_transform();
	compiler.add_node(tfm.x);
	compiler.add_node(tfm.y);
	compiler.add_node(tfm.z);
	compiler.add_node(tfm.w);

	if(use_minmax) {
		compiler.add_node(NODE_MIN_MAX, offset_out, offset_out);
		compiler.add_node(float3_to_float4(min));
		compiler.add_node(float3_to_float4(max));
	}
}

/* Image Texture */

static ShaderEnum color_space_init()
{
	ShaderEnum enm;

	enm.insert("None", 0);
	enm.insert("Color", 1);

	return enm;
}

ShaderEnum ImageTextureNode::color_space_enum = color_space_init();

ImageTextureNode::ImageTextureNode()
: TextureNode("image_texture")
{
	image_manager = NULL;
	slot = -1;
	is_float = false;
	filename = "";
	color_space = ustring("Color");

	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_UV);
	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Alpha", SHADER_SOCKET_FLOAT);
}

ImageTextureNode::~ImageTextureNode()
{
	if(image_manager)
		image_manager->remove_image(filename);
}

ShaderNode *ImageTextureNode::clone() const
{
	ImageTextureNode *node = new ImageTextureNode(*this);
	node->image_manager = NULL;
	node->slot = -1;
	node->is_float = false;
	return node;
}

void ImageTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *alpha_out = output("Alpha");

	image_manager = compiler.image_manager;
	if(slot == -1)
		slot = image_manager->add_image(filename, is_float);

	if(!color_out->links.empty())
		compiler.stack_assign(color_out);
	if(!alpha_out->links.empty())
		compiler.stack_assign(alpha_out);

	if(slot != -1) {
		compiler.stack_assign(vector_in);

		int srgb = (is_float || color_space != "Color")? 0: 1;
		int vector_offset = vector_in->stack_offset;

		if(!tex_mapping.skip()) {
			vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
			tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
		}

		compiler.add_node(NODE_TEX_IMAGE,
			slot,
			compiler.encode_uchar4(
				vector_offset,
				color_out->stack_offset,
				alpha_out->stack_offset,
				srgb));
	
		if(vector_offset != vector_in->stack_offset)
			compiler.stack_clear_offset(vector_in->type, vector_offset);
	}
	else {
		/* image not found */
		if(!color_out->links.empty()) {
			compiler.add_node(NODE_VALUE_V, color_out->stack_offset);
			compiler.add_node(NODE_VALUE_V, make_float3(0, 0, 0));
		}
		if(!alpha_out->links.empty())
			compiler.add_node(NODE_VALUE_F, __float_as_int(0.0f), alpha_out->stack_offset);
	}
}

void ImageTextureNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("filename", filename.c_str());
	if(is_float || color_space != "Color")
		compiler.parameter("color_space", "Linear");
	else
		compiler.parameter("color_space", "sRGB");
	compiler.add(this, "node_image_texture");
}

/* Environment Texture */

static ShaderEnum projection_init()
{
	ShaderEnum enm;

	enm.insert("Equirectangular", 0);
	enm.insert("Mirror Ball", 1);

	return enm;
}

ShaderEnum EnvironmentTextureNode::color_space_enum = color_space_init();
ShaderEnum EnvironmentTextureNode::projection_enum = projection_init();

EnvironmentTextureNode::EnvironmentTextureNode()
: TextureNode("environment_texture")
{
	image_manager = NULL;
	slot = -1;
	is_float = false;
	filename = "";
	color_space = ustring("Color");
	projection = ustring("Equirectangular");

	add_input("Vector", SHADER_SOCKET_VECTOR, ShaderInput::POSITION);
	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Alpha", SHADER_SOCKET_FLOAT);
}

EnvironmentTextureNode::~EnvironmentTextureNode()
{
	if(image_manager)
		image_manager->remove_image(filename);
}

ShaderNode *EnvironmentTextureNode::clone() const
{
	EnvironmentTextureNode *node = new EnvironmentTextureNode(*this);
	node->image_manager = NULL;
	node->slot = -1;
	node->is_float = false;
	return node;
}

void EnvironmentTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *alpha_out = output("Alpha");

	image_manager = compiler.image_manager;
	if(slot == -1)
		slot = image_manager->add_image(filename, is_float);

	if(!color_out->links.empty())
		compiler.stack_assign(color_out);
	if(!alpha_out->links.empty())
		compiler.stack_assign(alpha_out);
	
	if(slot != -1) {
		compiler.stack_assign(vector_in);

		int srgb = (is_float || color_space != "Color")? 0: 1;
		int vector_offset = vector_in->stack_offset;

		if(!tex_mapping.skip()) {
			vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
			tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
		}

		compiler.add_node(NODE_TEX_ENVIRONMENT,
			slot,
			compiler.encode_uchar4(
				vector_offset,
				color_out->stack_offset,
				alpha_out->stack_offset,
				srgb),
			projection_enum[projection]);
	
		if(vector_offset != vector_in->stack_offset)
			compiler.stack_clear_offset(vector_in->type, vector_offset);
	}
	else {
		/* image not found */
		if(!color_out->links.empty()) {
			compiler.add_node(NODE_VALUE_V, color_out->stack_offset);
			compiler.add_node(NODE_VALUE_V, make_float3(0, 0, 0));
		}
		if(!alpha_out->links.empty())
			compiler.add_node(NODE_VALUE_F, __float_as_int(0.0f), alpha_out->stack_offset);
	}
}

void EnvironmentTextureNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("filename", filename.c_str());
	if(is_float || color_space != "Color")
		compiler.parameter("color_space", "Linear");
	else
		compiler.parameter("color_space", "sRGB");
	compiler.add(this, "node_environment_texture");
}

/* Sky Texture */

static float2 sky_spherical_coordinates(float3 dir)
{
	return make_float2(acosf(dir.z), atan2f(dir.x, dir.y));
}

static float sky_perez_function(float lam[6], float theta, float gamma)
{
	return (1.f + lam[0]*expf(lam[1]/cosf(theta))) * (1.f + lam[2]*expf(lam[3]*gamma)  + lam[4]*cosf(gamma)*cosf(gamma));
}

static void sky_texture_precompute(KernelSunSky *ksunsky, float3 dir, float turbidity)
{
	float2 spherical = sky_spherical_coordinates(dir);
	float theta = spherical.x;
	float phi = spherical.y;

	ksunsky->theta = theta;
	ksunsky->phi = phi;

	float theta2 = theta*theta;
	float theta3 = theta*theta*theta;
	float T = turbidity;
	float T2 = T * T;

	float chi = (4.0f / 9.0f - T / 120.0f) * (M_PI_F - 2.0f * theta);
	ksunsky->zenith_Y = (4.0453f * T - 4.9710f) * tan(chi) - 0.2155f * T + 2.4192f;
	ksunsky->zenith_Y *= 0.06f;

	ksunsky->zenith_x =
	(0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * theta) * T2 +
	(-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * theta + 0.00394f) * T +
	(0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * theta + 0.25886f);

	ksunsky->zenith_y =
	(0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * theta) * T2 +
	(-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * theta  + 0.00516f) * T +
	(0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * theta  + 0.26688f);

	ksunsky->perez_Y[0] = (0.1787f * T  - 1.4630f);
	ksunsky->perez_Y[1] = (-0.3554f * T  + 0.4275f);
	ksunsky->perez_Y[2] = (-0.0227f * T  + 5.3251f);
	ksunsky->perez_Y[3] = (0.1206f * T  - 2.5771f);
	ksunsky->perez_Y[4] = (-0.0670f * T  + 0.3703f);

	ksunsky->perez_x[0] = (-0.0193f * T  - 0.2592f);
	ksunsky->perez_x[1] = (-0.0665f * T  + 0.0008f);
	ksunsky->perez_x[2] = (-0.0004f * T  + 0.2125f);
	ksunsky->perez_x[3] = (-0.0641f * T  - 0.8989f);
	ksunsky->perez_x[4] = (-0.0033f * T  + 0.0452f);

	ksunsky->perez_y[0] = (-0.0167f * T  - 0.2608f);
	ksunsky->perez_y[1] = (-0.0950f * T  + 0.0092f);
	ksunsky->perez_y[2] = (-0.0079f * T  + 0.2102f);
	ksunsky->perez_y[3] = (-0.0441f * T  - 1.6537f);
	ksunsky->perez_y[4] = (-0.0109f * T  + 0.0529f);

	ksunsky->zenith_Y /= sky_perez_function(ksunsky->perez_Y, 0, theta);
	ksunsky->zenith_x /= sky_perez_function(ksunsky->perez_x, 0, theta);
	ksunsky->zenith_y /= sky_perez_function(ksunsky->perez_y, 0, theta);
}

SkyTextureNode::SkyTextureNode()
: TextureNode("sky_texture")
{
	sun_direction = make_float3(0.0f, 0.0f, 1.0f);
	turbidity = 2.2f;

	add_input("Vector", SHADER_SOCKET_VECTOR, ShaderInput::POSITION);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void SkyTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");

	if(compiler.sunsky) {
		sky_texture_precompute(compiler.sunsky, sun_direction, turbidity);
		compiler.sunsky = NULL;
	}

	if(vector_in->link)
		compiler.stack_assign(vector_in);

	int vector_offset = vector_in->stack_offset;

	if(!tex_mapping.skip()) {
		vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
		tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
	}

	compiler.stack_assign(color_out);
	compiler.add_node(NODE_TEX_SKY, vector_offset, color_out->stack_offset);

	if(vector_offset != vector_in->stack_offset)
		compiler.stack_clear_offset(vector_in->type, vector_offset);
}

void SkyTextureNode::compile(OSLCompiler& compiler)
{
	compiler.parameter_vector("sun_direction", sun_direction);
	compiler.parameter("turbidity", turbidity);
	compiler.add(this, "node_sky_texture");
}

/* Gradient Texture */

static ShaderEnum gradient_type_init()
{
	ShaderEnum enm;

	enm.insert("Linear", NODE_BLEND_LINEAR);
	enm.insert("Quadratic", NODE_BLEND_QUADRATIC);
	enm.insert("Easing", NODE_BLEND_EASING);
	enm.insert("Diagonal", NODE_BLEND_DIAGONAL);
	enm.insert("Radial", NODE_BLEND_RADIAL);
	enm.insert("Quadratic Sphere", NODE_BLEND_QUADRATIC_SPHERE);
	enm.insert("Spherical", NODE_BLEND_SPHERICAL);

	return enm;
}

ShaderEnum GradientTextureNode::type_enum = gradient_type_init();

GradientTextureNode::GradientTextureNode()
: TextureNode("gradient_texture")
{
	type = ustring("Linear");

	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_GENERATED);
	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void GradientTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *fac_out = output("Fac");

	if(vector_in->link) compiler.stack_assign(vector_in);

	int vector_offset = vector_in->stack_offset;

	if(!tex_mapping.skip()) {
		vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
		tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
	}

	if(!fac_out->links.empty())
		compiler.stack_assign(fac_out);
	if(!color_out->links.empty())
		compiler.stack_assign(color_out);

	compiler.add_node(NODE_TEX_GRADIENT,
		compiler.encode_uchar4(type_enum[type], vector_offset, fac_out->stack_offset, color_out->stack_offset));

	if(vector_offset != vector_in->stack_offset)
		compiler.stack_clear_offset(vector_in->type, vector_offset);
}

void GradientTextureNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("Type", type);
	compiler.add(this, "node_gradient_texture");
}

/* Noise Texture */

NoiseTextureNode::NoiseTextureNode()
: TextureNode("noise_texture")
{
	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_GENERATED);
	add_input("Scale", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Detail", SHADER_SOCKET_FLOAT, 2.0f);
	add_input("Distortion", SHADER_SOCKET_FLOAT, 0.0f);

	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void NoiseTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *distortion_in = input("Distortion");
	ShaderInput *detail_in = input("Detail");
	ShaderInput *scale_in = input("Scale");
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *fac_out = output("Fac");

	if(vector_in->link) compiler.stack_assign(vector_in);
	if(scale_in->link) compiler.stack_assign(scale_in);
	if(detail_in->link) compiler.stack_assign(detail_in);
	if(distortion_in->link) compiler.stack_assign(distortion_in);

	int vector_offset = vector_in->stack_offset;

	if(!tex_mapping.skip()) {
		vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
		tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
	}

	if(!fac_out->links.empty())
		compiler.stack_assign(fac_out);
	if(!color_out->links.empty())
		compiler.stack_assign(color_out);

	compiler.add_node(NODE_TEX_NOISE,
		compiler.encode_uchar4(vector_offset, scale_in->stack_offset, detail_in->stack_offset, distortion_in->stack_offset),
		compiler.encode_uchar4(color_out->stack_offset, fac_out->stack_offset));
	compiler.add_node(
		__float_as_int(scale_in->value.x),
		__float_as_int(detail_in->value.x),
		__float_as_int(distortion_in->value.x));

	if(vector_offset != vector_in->stack_offset)
		compiler.stack_clear_offset(vector_in->type, vector_offset);
}

void NoiseTextureNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_noise_texture");
}

/* Voronoi Texture */

static ShaderEnum voronoi_coloring_init()
{
	ShaderEnum enm;

	enm.insert("Intensity", NODE_VORONOI_INTENSITY);
	enm.insert("Cells", NODE_VORONOI_CELLS);

	return enm;
}

ShaderEnum VoronoiTextureNode::coloring_enum  = voronoi_coloring_init();

VoronoiTextureNode::VoronoiTextureNode()
: TextureNode("voronoi_texture")
{
	coloring = ustring("Intensity");

	add_input("Scale", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_GENERATED);

	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void VoronoiTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *scale_in = input("Scale");
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *fac_out = output("Fac");

	if(vector_in->link) compiler.stack_assign(vector_in);
	if(scale_in->link) compiler.stack_assign(scale_in);

	int vector_offset = vector_in->stack_offset;

	if(!tex_mapping.skip()) {
		vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
		tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
	}

	compiler.stack_assign(color_out);
	compiler.stack_assign(fac_out);

	compiler.add_node(NODE_TEX_VORONOI,
		coloring_enum[coloring],
		compiler.encode_uchar4(scale_in->stack_offset, vector_offset, fac_out->stack_offset, color_out->stack_offset),
		__float_as_int(scale_in->value.x));

	if(vector_offset != vector_in->stack_offset)
		compiler.stack_clear_offset(vector_in->type, vector_offset);
}

void VoronoiTextureNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("Coloring", coloring);
	compiler.add(this, "node_voronoi_texture");
}

/* Musgrave Texture */

static ShaderEnum musgrave_type_init()
{
	ShaderEnum enm;

	enm.insert("Multifractal", NODE_MUSGRAVE_MULTIFRACTAL);
	enm.insert("fBM", NODE_MUSGRAVE_FBM);
	enm.insert("Hybrid Multifractal", NODE_MUSGRAVE_HYBRID_MULTIFRACTAL);
	enm.insert("Ridged Multifractal", NODE_MUSGRAVE_RIDGED_MULTIFRACTAL);
	enm.insert("Hetero Terrain", NODE_MUSGRAVE_HETERO_TERRAIN);

	return enm;
}

ShaderEnum MusgraveTextureNode::type_enum = musgrave_type_init();

MusgraveTextureNode::MusgraveTextureNode()
: TextureNode("musgrave_texture")
{
	type = ustring("fBM");

	add_input("Scale", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Detail", SHADER_SOCKET_FLOAT, 2.0f);
	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_GENERATED);
	add_input("Dimension", SHADER_SOCKET_FLOAT, 2.0f);
	add_input("Lacunarity", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Offset", SHADER_SOCKET_FLOAT, 0.0f);
	add_input("Gain", SHADER_SOCKET_FLOAT, 1.0f);

	add_output("Fac", SHADER_SOCKET_FLOAT);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void MusgraveTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderInput *scale_in = input("Scale");
	ShaderInput *dimension_in = input("Dimension");
	ShaderInput *lacunarity_in = input("Lacunarity");
	ShaderInput *detail_in = input("Detail");
	ShaderInput *offset_in = input("Offset");
	ShaderInput *gain_in = input("Gain");
	ShaderOutput *fac_out = output("Fac");
	ShaderOutput *color_out = output("Color");

	if(vector_in->link) compiler.stack_assign(vector_in);
	if(dimension_in->link) compiler.stack_assign(dimension_in);
	if(lacunarity_in->link) compiler.stack_assign(lacunarity_in);
	if(detail_in->link) compiler.stack_assign(detail_in);
	if(offset_in->link) compiler.stack_assign(offset_in);
	if(gain_in->link) compiler.stack_assign(gain_in);
	if(scale_in->link) compiler.stack_assign(scale_in);

	int vector_offset = vector_in->stack_offset;

	if(!tex_mapping.skip()) {
		vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
		tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
	}

	if(!fac_out->links.empty())
		compiler.stack_assign(fac_out);
	if(!color_out->links.empty())
		compiler.stack_assign(color_out);

	compiler.add_node(NODE_TEX_MUSGRAVE,
		compiler.encode_uchar4(type_enum[type], vector_offset, color_out->stack_offset, fac_out->stack_offset),
		compiler.encode_uchar4(dimension_in->stack_offset, lacunarity_in->stack_offset, detail_in->stack_offset, offset_in->stack_offset),
		compiler.encode_uchar4(gain_in->stack_offset, scale_in->stack_offset));
	compiler.add_node(__float_as_int(dimension_in->value.x),
		__float_as_int(lacunarity_in->value.x),
		__float_as_int(detail_in->value.x),
		__float_as_int(offset_in->value.x));
	compiler.add_node(__float_as_int(gain_in->value.x),
		__float_as_int(scale_in->value.x));

	if(vector_offset != vector_in->stack_offset)
		compiler.stack_clear_offset(vector_in->type, vector_offset);
}

void MusgraveTextureNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("Type", type);

	compiler.add(this, "node_musgrave_texture");
}

/* Wave Texture */

static ShaderEnum wave_type_init()
{
	ShaderEnum enm;

	enm.insert("Bands", NODE_WAVE_BANDS);
	enm.insert("Rings", NODE_WAVE_RINGS);

	return enm;
}

ShaderEnum WaveTextureNode::type_enum = wave_type_init();

WaveTextureNode::WaveTextureNode()
: TextureNode("wave_texture")
{
	type = ustring("Bands");

	add_input("Scale", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Distortion", SHADER_SOCKET_FLOAT, 0.0f);
	add_input("Detail", SHADER_SOCKET_FLOAT, 2.0f);
	add_input("Detail Scale", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_GENERATED);

	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void WaveTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *scale_in = input("Scale");
	ShaderInput *distortion_in = input("Distortion");
	ShaderInput *dscale_in = input("Detail Scale");
	ShaderInput *detail_in = input("Detail");
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *fac_out = output("Fac");
	ShaderOutput *color_out = output("Color");

	if(scale_in->link) compiler.stack_assign(scale_in);
	if(detail_in->link) compiler.stack_assign(detail_in);
	if(distortion_in->link) compiler.stack_assign(distortion_in);
	if(dscale_in->link) compiler.stack_assign(dscale_in);
	if(vector_in->link) compiler.stack_assign(vector_in);

	int vector_offset = vector_in->stack_offset;

	if(!tex_mapping.skip()) {
		vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
		tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
	}

	if(!fac_out->links.empty())
		compiler.stack_assign(fac_out);
	if(!color_out->links.empty())
		compiler.stack_assign(color_out);

	compiler.add_node(NODE_TEX_WAVE,
		compiler.encode_uchar4(type_enum[type], color_out->stack_offset, fac_out->stack_offset, dscale_in->stack_offset),
		compiler.encode_uchar4(vector_offset, scale_in->stack_offset, detail_in->stack_offset, distortion_in->stack_offset));

	compiler.add_node(
		__float_as_int(scale_in->value.x),
		__float_as_int(detail_in->value.x),
		__float_as_int(distortion_in->value.x),
		__float_as_int(dscale_in->value.x));

	if(vector_offset != vector_in->stack_offset)
		compiler.stack_clear_offset(vector_in->type, vector_offset);
}

void WaveTextureNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("Type", type);

	compiler.add(this, "node_wave_texture");
}

/* Magic Texture */

MagicTextureNode::MagicTextureNode()
: TextureNode("magic_texture")
{
	depth = 2;

	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_GENERATED);
	add_input("Scale", SHADER_SOCKET_FLOAT, 5.0f);
	add_input("Distortion", SHADER_SOCKET_FLOAT, 1.0f);

	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void MagicTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderInput *scale_in = input("Scale");
	ShaderInput *distortion_in = input("Distortion");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *fac_out = output("Fac");

	if(vector_in->link) compiler.stack_assign(vector_in);
	if(distortion_in->link) compiler.stack_assign(distortion_in);
	if(scale_in->link) compiler.stack_assign(scale_in);

	int vector_offset = vector_in->stack_offset;

	if(!tex_mapping.skip()) {
		vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
		tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
	}

	if(!fac_out->links.empty())
		compiler.stack_assign(fac_out);
	if(!color_out->links.empty())
		compiler.stack_assign(color_out);

	compiler.add_node(NODE_TEX_MAGIC,
		compiler.encode_uchar4(depth, color_out->stack_offset, fac_out->stack_offset),
		compiler.encode_uchar4(vector_offset, scale_in->stack_offset, distortion_in->stack_offset));
	compiler.add_node(
		__float_as_int(scale_in->value.x),
		__float_as_int(distortion_in->value.x));

	if(vector_offset != vector_in->stack_offset)
		compiler.stack_clear_offset(vector_in->type, vector_offset);
}

void MagicTextureNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("Depth", depth);
	compiler.add(this, "node_magic_texture");
}

/* Checker Texture */

CheckerTextureNode::CheckerTextureNode()
: TextureNode("checker_texture")
{
	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_GENERATED);
	add_input("Color1", SHADER_SOCKET_COLOR);
	add_input("Color2", SHADER_SOCKET_COLOR);
	add_input("Scale", SHADER_SOCKET_FLOAT, 1.0f);

	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void CheckerTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderInput *color1_in = input("Color1");
	ShaderInput *color2_in = input("Color2");
	ShaderInput *scale_in = input("Scale");
	
	ShaderOutput *color_out = output("Color");
	ShaderOutput *fac_out = output("Fac");

	compiler.stack_assign(vector_in);
	compiler.stack_assign(color1_in);
	compiler.stack_assign(color2_in);
	if(scale_in->link) compiler.stack_assign(scale_in);

	int vector_offset = vector_in->stack_offset;

	if(!tex_mapping.skip()) {
		vector_offset = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);
		tex_mapping.compile(compiler, vector_in->stack_offset, vector_offset);
	}

	if(!color_out->links.empty())
		compiler.stack_assign(color_out);
	if(!fac_out->links.empty())
		compiler.stack_assign(fac_out);

	compiler.add_node(NODE_TEX_CHECKER,
		compiler.encode_uchar4(vector_offset, color1_in->stack_offset, color2_in->stack_offset, scale_in->stack_offset),
		compiler.encode_uchar4(color_out->stack_offset, fac_out->stack_offset),
		__float_as_int(scale_in->value.x));

	if(vector_offset != vector_in->stack_offset)
		compiler.stack_clear_offset(vector_in->type, vector_offset);
}

void CheckerTextureNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_checker_texture");
}

/* Normal */

NormalNode::NormalNode()
: ShaderNode("normal")
{
	direction = make_float3(0.0f, 0.0f, 1.0f);

	add_input("Normal", SHADER_SOCKET_NORMAL);
	add_output("Normal", SHADER_SOCKET_NORMAL);
	add_output("Dot",  SHADER_SOCKET_FLOAT);
}

void NormalNode::compile(SVMCompiler& compiler)
{
	ShaderInput *normal_in = input("Normal");
	ShaderOutput *normal_out = output("Normal");
	ShaderOutput *dot_out = output("Dot");

	compiler.stack_assign(normal_in);
	compiler.stack_assign(normal_out);
	compiler.stack_assign(dot_out);

	compiler.add_node(NODE_NORMAL, normal_in->stack_offset, normal_out->stack_offset, dot_out->stack_offset);
	compiler.add_node(
		__float_as_int(direction.x),
		__float_as_int(direction.y),
		__float_as_int(direction.z));
}

void NormalNode::compile(OSLCompiler& compiler)
{
	compiler.parameter_vector("Direction", direction);
	compiler.add(this, "node_normal");
}

/* Mapping */

MappingNode::MappingNode()
: ShaderNode("mapping")
{
	add_input("Vector", SHADER_SOCKET_POINT);
	add_output("Vector", SHADER_SOCKET_POINT);
}

void MappingNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *vector_out = output("Vector");

	compiler.stack_assign(vector_in);
	compiler.stack_assign(vector_out);

	tex_mapping.compile(compiler, vector_in->stack_offset, vector_out->stack_offset);
}

void MappingNode::compile(OSLCompiler& compiler)
{
	Transform tfm = transform_transpose(tex_mapping.compute_transform());
	compiler.parameter("Matrix", tfm);

	compiler.add(this, "node_mapping");
}

/* Convert */

ConvertNode::ConvertNode(ShaderSocketType from_, ShaderSocketType to_)
: ShaderNode("convert")
{
	from = from_;
	to = to_;

	assert(from != to);

	if(from == SHADER_SOCKET_FLOAT)
		add_input("Val", SHADER_SOCKET_FLOAT);
	else if(from == SHADER_SOCKET_COLOR)
		add_input("Color", SHADER_SOCKET_COLOR);
	else if(from == SHADER_SOCKET_VECTOR)
		add_input("Vector", SHADER_SOCKET_VECTOR);
	else if(from == SHADER_SOCKET_POINT)
		add_input("Point", SHADER_SOCKET_POINT);
	else if(from == SHADER_SOCKET_NORMAL)
		add_input("Normal", SHADER_SOCKET_NORMAL);
	else
		assert(0);

	if(to == SHADER_SOCKET_FLOAT)
		add_output("Val", SHADER_SOCKET_FLOAT);
	else if(to == SHADER_SOCKET_COLOR)
		add_output("Color", SHADER_SOCKET_COLOR);
	else if(to == SHADER_SOCKET_VECTOR)
		add_output("Vector", SHADER_SOCKET_VECTOR);
	else if(to == SHADER_SOCKET_POINT)
		add_output("Point", SHADER_SOCKET_POINT);
	else if(to == SHADER_SOCKET_NORMAL)
		add_output("Normal", SHADER_SOCKET_NORMAL);
	else
		assert(0);
}

void ConvertNode::compile(SVMCompiler& compiler)
{
	ShaderInput *in = inputs[0];
	ShaderOutput *out = outputs[0];

	if(to == SHADER_SOCKET_FLOAT) {
		compiler.stack_assign(in);
		compiler.stack_assign(out);

		if(from == SHADER_SOCKET_COLOR)
			/* color to float */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_CF, in->stack_offset, out->stack_offset);
		else
			/* vector/point/normal to float */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_VF, in->stack_offset, out->stack_offset);
	}
	else if(from == SHADER_SOCKET_FLOAT) {
		compiler.stack_assign(in);
		compiler.stack_assign(out);

		/* float to float3 */
		compiler.add_node(NODE_CONVERT, NODE_CONVERT_FV, in->stack_offset, out->stack_offset);
	}
	else {
		/* float3 to float3 */
		if(in->link) {
			/* no op in SVM */
			compiler.stack_link(in, out);
		}
		else {
			/* set 0,0,0 value */
			compiler.stack_assign(in);
			compiler.stack_assign(out);

			compiler.add_node(NODE_VALUE_V, in->stack_offset);
			compiler.add_node(NODE_VALUE_V, in->value);
		}
	}
}

void ConvertNode::compile(OSLCompiler& compiler)
{
	if(from == SHADER_SOCKET_FLOAT)
		compiler.add(this, "node_convert_from_float");
	else if(from == SHADER_SOCKET_COLOR)
		compiler.add(this, "node_convert_from_color");
	else if(from == SHADER_SOCKET_VECTOR)
		compiler.add(this, "node_convert_from_vector");
	else if(from == SHADER_SOCKET_POINT)
		compiler.add(this, "node_convert_from_point");
	else if(from == SHADER_SOCKET_NORMAL)
		compiler.add(this, "node_convert_from_normal");
	else
		assert(0);
}

/* Proxy */

ProxyNode::ProxyNode(ShaderSocketType from_, ShaderSocketType to_)
: ShaderNode("proxy")
{
	from = from_;
	to = to_;

	add_input("Input", from);
	add_output("Output", to);
}

void ProxyNode::compile(SVMCompiler& compiler)
{
}

void ProxyNode::compile(OSLCompiler& compiler)
{
}

/* BSDF Closure */

BsdfNode::BsdfNode()
: ShaderNode("bsdf")
{
	closure = ccl::CLOSURE_BSDF_DIFFUSE_ID;

	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("Normal", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, true);

	add_output("BSDF", SHADER_SOCKET_CLOSURE);
}

void BsdfNode::compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2)
{
	ShaderInput *color_in = input("Color");

	if(color_in->link) {
		compiler.stack_assign(color_in);
		compiler.add_node(NODE_CLOSURE_WEIGHT, color_in->stack_offset);
	}
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value);
	
	if(param1)
		compiler.stack_assign(param1);
	if(param2)
		compiler.stack_assign(param2);

	compiler.add_node(NODE_CLOSURE_BSDF,
		compiler.encode_uchar4(closure,
			(param1)? param1->stack_offset: SVM_STACK_INVALID,
			(param2)? param2->stack_offset: SVM_STACK_INVALID,
			compiler.closure_mix_weight_offset()),
		__float_as_int((param1)? param1->value.x: 0.0f),
		__float_as_int((param2)? param2->value.x: 0.0f));
}

void BsdfNode::compile(SVMCompiler& compiler)
{
	compile(compiler, NULL, NULL);
}

void BsdfNode::compile(OSLCompiler& compiler)
{
	assert(0);
}

/* Ward BSDF Closure */

WardBsdfNode::WardBsdfNode()
{
	closure = CLOSURE_BSDF_WARD_ID;

	add_input("Roughness U", SHADER_SOCKET_FLOAT, 0.2f);
	add_input("Roughness V", SHADER_SOCKET_FLOAT, 0.2f);
}

void WardBsdfNode::compile(SVMCompiler& compiler)
{
	BsdfNode::compile(compiler, input("Roughness U"), input("Roughness V"));
}

void WardBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_ward_bsdf");
}

/* Glossy BSDF Closure */

static ShaderEnum glossy_distribution_init()
{
	ShaderEnum enm;

	enm.insert("Sharp", CLOSURE_BSDF_REFLECTION_ID);
	enm.insert("Beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
	enm.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_ID);

	return enm;
}

ShaderEnum GlossyBsdfNode::distribution_enum = glossy_distribution_init();

GlossyBsdfNode::GlossyBsdfNode()
{
	distribution = ustring("Beckmann");

	add_input("Roughness", SHADER_SOCKET_FLOAT, 0.2f);
}

void GlossyBsdfNode::compile(SVMCompiler& compiler)
{
	closure = (ClosureType)distribution_enum[distribution];

	if(closure == CLOSURE_BSDF_REFLECTION_ID)
		BsdfNode::compile(compiler, NULL, NULL);
	else
		BsdfNode::compile(compiler, input("Roughness"), NULL);
}

void GlossyBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("distribution", distribution);
	compiler.add(this, "node_glossy_bsdf");
}

/* Glass BSDF Closure */

static ShaderEnum glass_distribution_init()
{
	ShaderEnum enm;

	enm.insert("Sharp", CLOSURE_BSDF_REFRACTION_ID);
	enm.insert("Beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID);
	enm.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);

	return enm;
}

ShaderEnum GlassBsdfNode::distribution_enum = glass_distribution_init();

GlassBsdfNode::GlassBsdfNode()
{
	distribution = ustring("Sharp");

	add_input("Roughness", SHADER_SOCKET_FLOAT, 0.0f);
	add_input("IOR", SHADER_SOCKET_FLOAT, 0.3f);
}

void GlassBsdfNode::compile(SVMCompiler& compiler)
{
	closure = (ClosureType)distribution_enum[distribution];

	if(closure == CLOSURE_BSDF_REFRACTION_ID)
		BsdfNode::compile(compiler, NULL, input("IOR"));
	else
		BsdfNode::compile(compiler, input("Roughness"), input("IOR"));
}

void GlassBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("distribution", distribution);
	compiler.add(this, "node_glass_bsdf");
}

/* Velvet BSDF Closure */

VelvetBsdfNode::VelvetBsdfNode()
{
	closure = CLOSURE_BSDF_ASHIKHMIN_VELVET_ID;

	add_input("Sigma", SHADER_SOCKET_FLOAT, 1.0f);
}

void VelvetBsdfNode::compile(SVMCompiler& compiler)
{
	BsdfNode::compile(compiler, input("Sigma"), NULL);
}

void VelvetBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_velvet_bsdf");
}

/* Diffuse BSDF Closure */

DiffuseBsdfNode::DiffuseBsdfNode()
{
	closure = CLOSURE_BSDF_DIFFUSE_ID;
	add_input("Roughness", SHADER_SOCKET_FLOAT, 0.0f);
}

void DiffuseBsdfNode::compile(SVMCompiler& compiler)
{
	BsdfNode::compile(compiler, input("Roughness"), NULL);
}

void DiffuseBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_diffuse_bsdf");
}

/* Translucent BSDF Closure */

TranslucentBsdfNode::TranslucentBsdfNode()
{
	closure = CLOSURE_BSDF_TRANSLUCENT_ID;
}

void TranslucentBsdfNode::compile(SVMCompiler& compiler)
{
	BsdfNode::compile(compiler, NULL, NULL);
}

void TranslucentBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_translucent_bsdf");
}

/* Transparent BSDF Closure */

TransparentBsdfNode::TransparentBsdfNode()
{
	name = "transparent";
	closure = CLOSURE_BSDF_TRANSPARENT_ID;
}

void TransparentBsdfNode::compile(SVMCompiler& compiler)
{
	BsdfNode::compile(compiler, NULL, NULL);
}

void TransparentBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_transparent_bsdf");
}

/* Emissive Closure */

EmissionNode::EmissionNode()
: ShaderNode("emission")
{
	total_power = false;

	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("Strength", SHADER_SOCKET_FLOAT, 10.0f);
	add_output("Emission", SHADER_SOCKET_CLOSURE);
}

void EmissionNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *strength_in = input("Strength");

	if(color_in->link || strength_in->link) {
		compiler.stack_assign(color_in);
		compiler.stack_assign(strength_in);
		compiler.add_node(NODE_EMISSION_WEIGHT, color_in->stack_offset, strength_in->stack_offset, total_power? 1: 0);
	}
	else if(total_power)
		compiler.add_node(NODE_EMISSION_SET_WEIGHT_TOTAL, color_in->value * strength_in->value.x);
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value * strength_in->value.x);

	compiler.add_node(NODE_CLOSURE_EMISSION, compiler.closure_mix_weight_offset());
}

void EmissionNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("TotalPower", (total_power)? 1: 0);
	compiler.add(this, "node_emission");
}

/* Background Closure */

BackgroundNode::BackgroundNode()
: ShaderNode("background")
{
	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("Strength", SHADER_SOCKET_FLOAT, 1.0f);
	add_output("Background", SHADER_SOCKET_CLOSURE);
}

void BackgroundNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *strength_in = input("Strength");

	if(color_in->link || strength_in->link) {
		compiler.stack_assign(color_in);
		compiler.stack_assign(strength_in);
		compiler.add_node(NODE_EMISSION_WEIGHT, color_in->stack_offset, strength_in->stack_offset);
	}
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value*strength_in->value.x);

	compiler.add_node(NODE_CLOSURE_BACKGROUND, compiler.closure_mix_weight_offset());
}

void BackgroundNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_background");
}

/* Holdout Closure */

HoldoutNode::HoldoutNode()
: ShaderNode("holdout")
{
	add_output("Holdout", SHADER_SOCKET_CLOSURE);
}

void HoldoutNode::compile(SVMCompiler& compiler)
{
	compiler.add_node(NODE_CLOSURE_HOLDOUT, compiler.closure_mix_weight_offset());
}

void HoldoutNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_holdout");
}

/* Volume Closure */

VolumeNode::VolumeNode()
: ShaderNode("volume")
{
	closure = ccl::CLOSURE_VOLUME_ISOTROPIC_ID;

	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("Density", SHADER_SOCKET_FLOAT, 1.0f);

	add_output("Volume", SHADER_SOCKET_CLOSURE);
}

void VolumeNode::compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2)
{
	ShaderInput *color_in = input("Color");

	if(color_in->link) {
		compiler.stack_assign(color_in);
		compiler.add_node(NODE_CLOSURE_WEIGHT, color_in->stack_offset);
	}
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value);
	
	if(param1)
		compiler.stack_assign(param1);
	if(param2)
		compiler.stack_assign(param2);

	compiler.add_node(NODE_CLOSURE_VOLUME,
		compiler.encode_uchar4(closure,
			(param1)? param1->stack_offset: SVM_STACK_INVALID,
			(param2)? param2->stack_offset: SVM_STACK_INVALID,
			compiler.closure_mix_weight_offset()),
		__float_as_int((param1)? param1->value.x: 0.0f),
		__float_as_int((param2)? param2->value.x: 0.0f));
}

void VolumeNode::compile(SVMCompiler& compiler)
{
	compile(compiler, NULL, NULL);
}

void VolumeNode::compile(OSLCompiler& compiler)
{
	assert(0);
}

/* Transparent Volume Closure */

TransparentVolumeNode::TransparentVolumeNode()
{
	closure = CLOSURE_VOLUME_TRANSPARENT_ID;
}

void TransparentVolumeNode::compile(SVMCompiler& compiler)
{
	VolumeNode::compile(compiler, input("Density"), NULL);
}

void TransparentVolumeNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_isotropic_volume");
}

/* Isotropic Volume Closure */

IsotropicVolumeNode::IsotropicVolumeNode()
{
	closure = CLOSURE_VOLUME_ISOTROPIC_ID;
}

void IsotropicVolumeNode::compile(SVMCompiler& compiler)
{
	VolumeNode::compile(compiler, input("Density"), NULL);
}

void IsotropicVolumeNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_isotropic_volume");
}

/* Geometry */

GeometryNode::GeometryNode()
: ShaderNode("geometry")
{
	add_input("NormalIn", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, true);
	add_output("Position", SHADER_SOCKET_POINT);
	add_output("Normal", SHADER_SOCKET_NORMAL);
	add_output("Tangent", SHADER_SOCKET_NORMAL);
	add_output("True Normal", SHADER_SOCKET_NORMAL);
	add_output("Incoming", SHADER_SOCKET_VECTOR);
	add_output("Parametric", SHADER_SOCKET_POINT);
	add_output("Backfacing", SHADER_SOCKET_FLOAT);
}

void GeometryNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;
	NodeType geom_node = NODE_GEOMETRY;

	if(bump == SHADER_BUMP_DX)
		geom_node = NODE_GEOMETRY_BUMP_DX;
	else if(bump == SHADER_BUMP_DY)
		geom_node = NODE_GEOMETRY_BUMP_DY;
	
	out = output("Position");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(geom_node, NODE_GEOM_P, out->stack_offset);
	}

	out = output("Normal");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(geom_node, NODE_GEOM_N, out->stack_offset);
	}

	out = output("Tangent");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(geom_node, NODE_GEOM_T, out->stack_offset);
	}

	out = output("True Normal");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(geom_node, NODE_GEOM_Ng, out->stack_offset);
	}

	out = output("Incoming");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(geom_node, NODE_GEOM_I, out->stack_offset);
	}

	out = output("Parametric");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(geom_node, NODE_GEOM_uv, out->stack_offset);
	}

	out = output("Backfacing");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_backfacing, out->stack_offset);
	}
}

void GeometryNode::compile(OSLCompiler& compiler)
{
	if(bump == SHADER_BUMP_DX)
		compiler.parameter("bump_offset", "dx");
	else if(bump == SHADER_BUMP_DY)
		compiler.parameter("bump_offset", "dy");
	else
		compiler.parameter("bump_offset", "center");

	compiler.add(this, "node_geometry");
}

/* TextureCoordinate */

TextureCoordinateNode::TextureCoordinateNode()
: ShaderNode("texture_coordinate")
{
	add_input("Normal", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, true);
	add_output("Generated", SHADER_SOCKET_POINT);
	add_output("Normal", SHADER_SOCKET_NORMAL);
	add_output("UV", SHADER_SOCKET_POINT);
	add_output("Object", SHADER_SOCKET_POINT);
	add_output("Camera", SHADER_SOCKET_POINT);
	add_output("Window", SHADER_SOCKET_POINT);
	add_output("Reflection", SHADER_SOCKET_NORMAL);
}

void TextureCoordinateNode::attributes(AttributeRequestSet *attributes)
{
	if(!output("Generated")->links.empty())
		attributes->add(ATTR_STD_GENERATED);
	if(!output("UV")->links.empty())
		attributes->add(ATTR_STD_UV);

	ShaderNode::attributes(attributes);
}

void TextureCoordinateNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;
	NodeType texco_node = NODE_TEX_COORD;
	NodeType attr_node = NODE_ATTR;
	NodeType geom_node = NODE_GEOMETRY;

	if(bump == SHADER_BUMP_DX) {
		texco_node = NODE_TEX_COORD_BUMP_DX;
		attr_node = NODE_ATTR_BUMP_DX;
		geom_node = NODE_GEOMETRY_BUMP_DX;
	}
	else if(bump == SHADER_BUMP_DY) {
		texco_node = NODE_TEX_COORD_BUMP_DY;
		attr_node = NODE_ATTR_BUMP_DY;
		geom_node = NODE_GEOMETRY_BUMP_DY;
	}
	
	out = output("Generated");
	if(!out->links.empty()) {
		if(compiler.background) {
			compiler.stack_assign(out);
			compiler.add_node(geom_node, NODE_GEOM_P, out->stack_offset);
		}
		else {
			int attr = compiler.attribute(ATTR_STD_GENERATED);
			compiler.stack_assign(out);
			compiler.add_node(attr_node, attr, out->stack_offset, NODE_ATTR_FLOAT3);
		}
	}

	out = output("Normal");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(texco_node, NODE_TEXCO_NORMAL, out->stack_offset);
	}

	out = output("UV");
	if(!out->links.empty()) {
		int attr = compiler.attribute(ATTR_STD_UV);
		compiler.stack_assign(out);
		compiler.add_node(attr_node, attr, out->stack_offset, NODE_ATTR_FLOAT3);
	}

	out = output("Object");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(texco_node, NODE_TEXCO_OBJECT, out->stack_offset);
	}

	out = output("Camera");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(texco_node, NODE_TEXCO_CAMERA, out->stack_offset);
	}

	out = output("Window");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(texco_node, NODE_TEXCO_WINDOW, out->stack_offset);
	}

	out = output("Reflection");
	if(!out->links.empty()) {
		if(compiler.background) {
			compiler.stack_assign(out);
			compiler.add_node(geom_node, NODE_GEOM_I, out->stack_offset);
		}
		else {
			compiler.stack_assign(out);
			compiler.add_node(texco_node, NODE_TEXCO_REFLECTION, out->stack_offset);
		}
	}
}

void TextureCoordinateNode::compile(OSLCompiler& compiler)
{
	if(bump == SHADER_BUMP_DX)
		compiler.parameter("bump_offset", "dx");
	else if(bump == SHADER_BUMP_DY)
		compiler.parameter("bump_offset", "dy");
	else
		compiler.parameter("bump_offset", "center");
	
	if(compiler.background)
		compiler.parameter("is_background", true);

	compiler.add(this, "node_texture_coordinate");
}

/* Light Path */

LightPathNode::LightPathNode()
: ShaderNode("light_path")
{
	add_output("Is Camera Ray", SHADER_SOCKET_FLOAT);
	add_output("Is Shadow Ray", SHADER_SOCKET_FLOAT);
	add_output("Is Diffuse Ray", SHADER_SOCKET_FLOAT);
	add_output("Is Glossy Ray", SHADER_SOCKET_FLOAT);
	add_output("Is Singular Ray", SHADER_SOCKET_FLOAT);
	add_output("Is Reflection Ray", SHADER_SOCKET_FLOAT);
	add_output("Is Transmission Ray", SHADER_SOCKET_FLOAT);
	add_output("Ray Length", SHADER_SOCKET_FLOAT);
}

void LightPathNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;

	out = output("Is Camera Ray");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_camera, out->stack_offset);
	}

	out = output("Is Shadow Ray");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_shadow, out->stack_offset);
	}

	out = output("Is Diffuse Ray");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_diffuse, out->stack_offset);
	}

	out = output("Is Glossy Ray");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_glossy, out->stack_offset);
	}

	out = output("Is Singular Ray");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_singular, out->stack_offset);
	}

	out = output("Is Reflection Ray");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_reflection, out->stack_offset);
	}


	out = output("Is Transmission Ray");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_transmission, out->stack_offset);
	}
	
	out = output("Ray Length");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_length, out->stack_offset);
	}

}

void LightPathNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_light_path");
}

/* Light Falloff */

LightFalloffNode::LightFalloffNode()
: ShaderNode("light_path")
{
	add_input("Strength", SHADER_SOCKET_FLOAT, 100.0f);
	add_input("Smooth", SHADER_SOCKET_FLOAT, 0.0f);
	add_output("Quadratic", SHADER_SOCKET_FLOAT);
	add_output("Linear", SHADER_SOCKET_FLOAT);
	add_output("Constant", SHADER_SOCKET_FLOAT);
}

void LightFalloffNode::compile(SVMCompiler& compiler)
{
	ShaderInput *strength_in = input("Strength");
	ShaderInput *smooth_in = input("Smooth");

	compiler.stack_assign(strength_in);
	compiler.stack_assign(smooth_in);

	ShaderOutput *out = output("Quadratic");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_FALLOFF, NODE_LIGHT_FALLOFF_QUADRATIC,
			compiler.encode_uchar4(strength_in->stack_offset, smooth_in->stack_offset, out->stack_offset));
	}

	out = output("Linear");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_FALLOFF, NODE_LIGHT_FALLOFF_LINEAR,
			compiler.encode_uchar4(strength_in->stack_offset, smooth_in->stack_offset, out->stack_offset));
	}

	out = output("Constant");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_LIGHT_FALLOFF, NODE_LIGHT_FALLOFF_CONSTANT,
			compiler.encode_uchar4(strength_in->stack_offset, smooth_in->stack_offset, out->stack_offset));
	}
}

void LightFalloffNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_light_falloff");
}

/* Object Info */

ObjectInfoNode::ObjectInfoNode()
: ShaderNode("object_info")
{
	add_output("Location", SHADER_SOCKET_VECTOR);
	add_output("Object Index", SHADER_SOCKET_FLOAT);
	add_output("Material Index", SHADER_SOCKET_FLOAT);
	add_output("Random", SHADER_SOCKET_FLOAT);
}

void ObjectInfoNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out = output("Location");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_LOCATION, out->stack_offset);
	}

	out = output("Object Index");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_INDEX, out->stack_offset);
	}

	out = output("Material Index");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_MAT_INDEX, out->stack_offset);
	}

	out = output("Random");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_RANDOM, out->stack_offset);
	}
}

void ObjectInfoNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_object_info");
}

/* Particle Info */

ParticleInfoNode::ParticleInfoNode()
: ShaderNode("particle_info")
{
	add_output("Age", SHADER_SOCKET_FLOAT);
	add_output("Lifetime", SHADER_SOCKET_FLOAT);
}

void ParticleInfoNode::attributes(AttributeRequestSet *attributes)
{
	if(!output("Age")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);
	if(!output("Lifetime")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);

	ShaderNode::attributes(attributes);
}

void ParticleInfoNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;
	
	out = output("Age");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_AGE, out->stack_offset);
	}
	
	out = output("Lifetime");
	if(!out->links.empty()) {
		compiler.stack_assign(out);
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_LIFETIME, out->stack_offset);
	}
}

void ParticleInfoNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_particle_info");
}

/* Value */

ValueNode::ValueNode()
: ShaderNode("value")
{
	value = 0.0f;

	add_output("Value", SHADER_SOCKET_FLOAT);
}

void ValueNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *val_out = output("Value");

	compiler.stack_assign(val_out);
	compiler.add_node(NODE_VALUE_F, __float_as_int(value), val_out->stack_offset);
}

void ValueNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("value_value", value);
	compiler.add(this, "node_value");
}

/* Color */

ColorNode::ColorNode()
: ShaderNode("color")
{
	value = make_float3(0.0f, 0.0f, 0.0f);

	add_output("Color", SHADER_SOCKET_COLOR);
}

void ColorNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *color_out = output("Color");

	if(color_out && !color_out->links.empty()) {
		compiler.stack_assign(color_out);
		compiler.add_node(NODE_VALUE_V, color_out->stack_offset);
		compiler.add_node(NODE_VALUE_V, value);
	}
}

void ColorNode::compile(OSLCompiler& compiler)
{
	compiler.parameter_color("color_value", value);

	compiler.add(this, "node_value");
}

/* Add Closure */

AddClosureNode::AddClosureNode()
: ShaderNode("add_closure")
{
	add_input("Closure1", SHADER_SOCKET_CLOSURE);
	add_input("Closure2", SHADER_SOCKET_CLOSURE);
	add_output("Closure",  SHADER_SOCKET_CLOSURE);
}

void AddClosureNode::compile(SVMCompiler& compiler)
{
	/* handled in the SVM compiler */
}

void AddClosureNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_add_closure");
}

/* Mix Closure */

MixClosureNode::MixClosureNode()
: ShaderNode("mix_closure")
{
	add_input("Fac", SHADER_SOCKET_FLOAT, 0.5f);
	add_input("Closure1", SHADER_SOCKET_CLOSURE);
	add_input("Closure2", SHADER_SOCKET_CLOSURE);
	add_output("Closure",  SHADER_SOCKET_CLOSURE);
}

void MixClosureNode::compile(SVMCompiler& compiler)
{
	/* handled in the SVM compiler */
}

void MixClosureNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_mix_closure");
}

/* Invert */

InvertNode::InvertNode()
: ShaderNode("invert")
{
	add_input("Fac", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Color", SHADER_SOCKET_COLOR);
	add_output("Color",  SHADER_SOCKET_COLOR);
}

void InvertNode::compile(SVMCompiler& compiler)
{
	ShaderInput *fac_in = input("Fac");
	ShaderInput *color_in = input("Color");
	ShaderOutput *color_out = output("Color");

	compiler.stack_assign(fac_in);
	compiler.stack_assign(color_in);
	compiler.stack_assign(color_out);

	compiler.add_node(NODE_INVERT, fac_in->stack_offset, color_in->stack_offset, color_out->stack_offset);
}

void InvertNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_invert");
}

/* Mix */

MixNode::MixNode()
: ShaderNode("mix")
{
	type = ustring("Mix");

	add_input("Fac", SHADER_SOCKET_FLOAT, 0.5f);
	add_input("Color1", SHADER_SOCKET_COLOR);
	add_input("Color2", SHADER_SOCKET_COLOR);
	add_output("Color",  SHADER_SOCKET_COLOR);
}

static ShaderEnum mix_type_init()
{
	ShaderEnum enm;

	enm.insert("Mix", NODE_MIX_BLEND);
	enm.insert("Add", NODE_MIX_ADD);
	enm.insert("Multiply", NODE_MIX_MUL);
	enm.insert("Screen", NODE_MIX_SCREEN);
	enm.insert("Overlay", NODE_MIX_OVERLAY);
	enm.insert("Subtract", NODE_MIX_SUB);
	enm.insert("Divide", NODE_MIX_DIV);
	enm.insert("Difference", NODE_MIX_DIFF);
	enm.insert("Darken", NODE_MIX_DARK);
	enm.insert("Lighten", NODE_MIX_LIGHT);
	enm.insert("Dodge", NODE_MIX_DODGE);
	enm.insert("Burn", NODE_MIX_BURN);
	enm.insert("Hue", NODE_MIX_HUE);
	enm.insert("Saturation", NODE_MIX_SAT);
	enm.insert("Value", NODE_MIX_VAL );
	enm.insert("Color", NODE_MIX_COLOR);
	enm.insert("Soft Light", NODE_MIX_SOFT);
	enm.insert("Linear Light", NODE_MIX_LINEAR);

	return enm;
}

ShaderEnum MixNode::type_enum = mix_type_init();

void MixNode::compile(SVMCompiler& compiler)
{
	ShaderInput *fac_in = input("Fac");
	ShaderInput *color1_in = input("Color1");
	ShaderInput *color2_in = input("Color2");
	ShaderOutput *color_out = output("Color");

	compiler.stack_assign(fac_in);
	compiler.stack_assign(color1_in);
	compiler.stack_assign(color2_in);
	compiler.stack_assign(color_out);

	compiler.add_node(NODE_MIX, fac_in->stack_offset, color1_in->stack_offset, color2_in->stack_offset);
	compiler.add_node(NODE_MIX, type_enum[type], color_out->stack_offset);
}

void MixNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("type", type);
	compiler.add(this, "node_mix");
}

/* Combine RGB */
CombineRGBNode::CombineRGBNode()
: ShaderNode("combine_rgb")
{
	add_input("R", SHADER_SOCKET_FLOAT);
	add_input("G", SHADER_SOCKET_FLOAT);
	add_input("B", SHADER_SOCKET_FLOAT);
	add_output("Image", SHADER_SOCKET_COLOR);
}

void CombineRGBNode::compile(SVMCompiler& compiler)
{
	ShaderInput *red_in = input("R");
	ShaderInput *green_in = input("G");
	ShaderInput *blue_in = input("B");
	ShaderOutput *color_out = output("Image");

	compiler.stack_assign(color_out);

	compiler.stack_assign(red_in);
	compiler.add_node(NODE_COMBINE_RGB, red_in->stack_offset, 0, color_out->stack_offset);

	compiler.stack_assign(green_in);
	compiler.add_node(NODE_COMBINE_RGB, green_in->stack_offset, 1, color_out->stack_offset);

	compiler.stack_assign(blue_in);
	compiler.add_node(NODE_COMBINE_RGB, blue_in->stack_offset, 2, color_out->stack_offset);
}

void CombineRGBNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_combine_rgb");
}

/* Gamma */
GammaNode::GammaNode()
: ShaderNode("gamma")
{
	add_input("Color", SHADER_SOCKET_COLOR);
	add_input("Gamma", SHADER_SOCKET_FLOAT);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void GammaNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *gamma_in = input("Gamma");
	ShaderOutput *color_out = output("Color");

	compiler.stack_assign(color_in);
	compiler.stack_assign(gamma_in);
	compiler.stack_assign(color_out);

	compiler.add_node(NODE_GAMMA, gamma_in->stack_offset, color_in->stack_offset, color_out->stack_offset);
}

void GammaNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_gamma");
}

/* Bright Contrast */
BrightContrastNode::BrightContrastNode()
: ShaderNode("brightness")
{
	add_input("Color", SHADER_SOCKET_COLOR);
	add_input("Bright", SHADER_SOCKET_FLOAT);
	add_input("Contrast", SHADER_SOCKET_FLOAT);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void BrightContrastNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *bright_in = input("Bright");
	ShaderInput *contrast_in = input("Contrast");
	ShaderOutput *color_out = output("Color");

	compiler.stack_assign(color_in);
	compiler.stack_assign(bright_in);
	compiler.stack_assign(contrast_in);
	compiler.stack_assign(color_out);

	compiler.add_node(NODE_BRIGHTCONTRAST,
		color_in->stack_offset, color_out->stack_offset,
		compiler.encode_uchar4(bright_in->stack_offset, contrast_in->stack_offset));
}

void BrightContrastNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_brightness");
}

/* Separate RGB */
SeparateRGBNode::SeparateRGBNode()
: ShaderNode("separate_rgb")
{
	add_input("Image", SHADER_SOCKET_COLOR);
	add_output("R", SHADER_SOCKET_FLOAT);
	add_output("G", SHADER_SOCKET_FLOAT);
	add_output("B", SHADER_SOCKET_FLOAT);
}

void SeparateRGBNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Image");
	ShaderOutput *red_out = output("R");
	ShaderOutput *green_out = output("G");
	ShaderOutput *blue_out = output("B");

	compiler.stack_assign(color_in);

	compiler.stack_assign(red_out);
	compiler.add_node(NODE_SEPARATE_RGB, color_in->stack_offset, 0, red_out->stack_offset);

	compiler.stack_assign(green_out);
	compiler.add_node(NODE_SEPARATE_RGB, color_in->stack_offset, 1, green_out->stack_offset);

	compiler.stack_assign(blue_out);
	compiler.add_node(NODE_SEPARATE_RGB, color_in->stack_offset, 2, blue_out->stack_offset);
}

void SeparateRGBNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_separate_rgb");
}

/* Separate RGB */
HSVNode::HSVNode()
: ShaderNode("hsv")
{
	add_input("Hue", SHADER_SOCKET_FLOAT);
	add_input("Saturation", SHADER_SOCKET_FLOAT);
	add_input("Value", SHADER_SOCKET_FLOAT);
	add_input("Fac", SHADER_SOCKET_FLOAT);
	add_input("Color", SHADER_SOCKET_COLOR);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void HSVNode::compile(SVMCompiler& compiler)
{
	ShaderInput *hue_in = input("Hue");
	ShaderInput *saturation_in = input("Saturation");
	ShaderInput *value_in = input("Value");
	ShaderInput *fac_in = input("Fac");
	ShaderInput *color_in = input("Color");
	ShaderOutput *color_out = output("Color");

	compiler.stack_assign(hue_in);
	compiler.stack_assign(saturation_in);
	compiler.stack_assign(value_in);
	compiler.stack_assign(fac_in);
	compiler.stack_assign(color_in);
	compiler.stack_assign(color_out);

	compiler.add_node(NODE_HSV, color_in->stack_offset, fac_in->stack_offset, color_out->stack_offset);
	compiler.add_node(NODE_HSV, hue_in->stack_offset, saturation_in->stack_offset, value_in->stack_offset);
}

void HSVNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_hsv");
}

/* Attribute */

AttributeNode::AttributeNode()
: ShaderNode("attribute")
{
	attribute = "";

	add_output("Color",  SHADER_SOCKET_COLOR);
	add_output("Vector",  SHADER_SOCKET_VECTOR);
	add_output("Fac",  SHADER_SOCKET_FLOAT);
}

void AttributeNode::attributes(AttributeRequestSet *attributes)
{
	ShaderOutput *color_out = output("Color");
	ShaderOutput *vector_out = output("Vector");
	ShaderOutput *fac_out = output("Fac");

	if(!color_out->links.empty() || !vector_out->links.empty() || !fac_out->links.empty())
		attributes->add(attribute);
	
	ShaderNode::attributes(attributes);
}

void AttributeNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *color_out = output("Color");
	ShaderOutput *vector_out = output("Vector");
	ShaderOutput *fac_out = output("Fac");
	NodeType attr_node = NODE_ATTR;

	if(bump == SHADER_BUMP_DX)
		attr_node = NODE_ATTR_BUMP_DX;
	else if(bump == SHADER_BUMP_DY)
		attr_node = NODE_ATTR_BUMP_DY;

	if(!color_out->links.empty() || !vector_out->links.empty()) {
		int attr = compiler.attribute(attribute);

		if(!color_out->links.empty()) {
			compiler.stack_assign(color_out);
			compiler.add_node(attr_node, attr, color_out->stack_offset, NODE_ATTR_FLOAT3);
		}
		if(!vector_out->links.empty()) {
			compiler.stack_assign(vector_out);
			compiler.add_node(attr_node, attr, vector_out->stack_offset, NODE_ATTR_FLOAT3);
		}
	}

	if(!fac_out->links.empty()) {
		int attr = compiler.attribute(attribute);

		compiler.stack_assign(fac_out);
		compiler.add_node(attr_node, attr, fac_out->stack_offset, NODE_ATTR_FLOAT);
	}
}

void AttributeNode::compile(OSLCompiler& compiler)
{
	if(bump == SHADER_BUMP_DX)
		compiler.parameter("bump_offset", "dx");
	else if(bump == SHADER_BUMP_DY)
		compiler.parameter("bump_offset", "dy");
	else
		compiler.parameter("bump_offset", "center");

	compiler.parameter("name", attribute.c_str());
	compiler.add(this, "node_attribute");
}

/* Camera */

CameraNode::CameraNode()
: ShaderNode("camera")
{
	add_output("View Vector",  SHADER_SOCKET_VECTOR);
	add_output("View Z Depth",  SHADER_SOCKET_FLOAT);
	add_output("View Distance",  SHADER_SOCKET_FLOAT);
}

void CameraNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *vector_out = output("View Vector");
	ShaderOutput *z_depth_out = output("View Z Depth");
	ShaderOutput *distance_out = output("View Distance");

	compiler.stack_assign(vector_out);
	compiler.stack_assign(z_depth_out);
	compiler.stack_assign(distance_out);
	compiler.add_node(NODE_CAMERA, vector_out->stack_offset, z_depth_out->stack_offset, distance_out->stack_offset);
}

void CameraNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_camera");
}

/* Fresnel */

FresnelNode::FresnelNode()
: ShaderNode("Fresnel")
{
	add_input("Normal", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, true);
	add_input("IOR", SHADER_SOCKET_FLOAT, 1.45f);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void FresnelNode::compile(SVMCompiler& compiler)
{
	ShaderInput *ior_in = input("IOR");
	ShaderOutput *fac_out = output("Fac");

	compiler.stack_assign(ior_in);
	compiler.stack_assign(fac_out);
	compiler.add_node(NODE_FRESNEL, ior_in->stack_offset, __float_as_int(ior_in->value.x), fac_out->stack_offset);
}

void FresnelNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_fresnel");
}

/* Blend Weight */

LayerWeightNode::LayerWeightNode()
: ShaderNode("LayerWeight")
{
	add_input("Normal", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, true);
	add_input("Blend", SHADER_SOCKET_FLOAT, 0.5f);

	add_output("Fresnel", SHADER_SOCKET_FLOAT);
	add_output("Facing", SHADER_SOCKET_FLOAT);
}

void LayerWeightNode::compile(SVMCompiler& compiler)
{
	ShaderInput *blend_in = input("Blend");

	if(blend_in->link)
		compiler.stack_assign(blend_in);

	ShaderOutput *fresnel_out = output("Fresnel");
	if(!fresnel_out->links.empty()) {
		compiler.stack_assign(fresnel_out);
		compiler.add_node(NODE_LAYER_WEIGHT, blend_in->stack_offset, __float_as_int(blend_in->value.x),
			compiler.encode_uchar4(NODE_LAYER_WEIGHT_FRESNEL, fresnel_out->stack_offset));
	}

	ShaderOutput *facing_out = output("Facing");
	if(!facing_out->links.empty()) {
		compiler.stack_assign(facing_out);
		compiler.add_node(NODE_LAYER_WEIGHT, blend_in->stack_offset, __float_as_int(blend_in->value.x),
			compiler.encode_uchar4(NODE_LAYER_WEIGHT_FACING, facing_out->stack_offset));
	}
}

void LayerWeightNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_layer_height");
}

/* Output */

OutputNode::OutputNode()
: ShaderNode("output")
{
	add_input("Surface", SHADER_SOCKET_CLOSURE);
	add_input("Volume", SHADER_SOCKET_CLOSURE);
	add_input("Displacement", SHADER_SOCKET_FLOAT);
}

void OutputNode::compile(SVMCompiler& compiler)
{
	if(compiler.output_type() == SHADER_TYPE_DISPLACEMENT) {
		ShaderInput *displacement_in = input("Displacement");

		if(displacement_in->link) {
			compiler.stack_assign(displacement_in);
			compiler.add_node(NODE_SET_DISPLACEMENT, displacement_in->stack_offset);
		}
	}
}

void OutputNode::compile(OSLCompiler& compiler)
{
	if(compiler.output_type() == SHADER_TYPE_SURFACE)
		compiler.add(this, "node_output_surface");
	else if(compiler.output_type() == SHADER_TYPE_VOLUME)
		compiler.add(this, "node_output_volume");
	else if(compiler.output_type() == SHADER_TYPE_DISPLACEMENT)
		compiler.add(this, "node_output_displacement");
}

/* Math */

MathNode::MathNode()
: ShaderNode("math")
{
	type = ustring("Add");

	add_input("Value1", SHADER_SOCKET_FLOAT);
	add_input("Value2", SHADER_SOCKET_FLOAT);
	add_output("Value",  SHADER_SOCKET_FLOAT);
}

static ShaderEnum math_type_init()
{
	ShaderEnum enm;

	enm.insert("Add", NODE_MATH_ADD);
	enm.insert("Subtract", NODE_MATH_SUBTRACT);
	enm.insert("Multiply", NODE_MATH_MULTIPLY);
	enm.insert("Divide", NODE_MATH_DIVIDE);
	enm.insert("Sine", NODE_MATH_SINE);
	enm.insert("Cosine", NODE_MATH_COSINE);
	enm.insert("Tangent", NODE_MATH_TANGENT);
	enm.insert("Arcsine", NODE_MATH_ARCSINE);
	enm.insert("Arccosine", NODE_MATH_ARCCOSINE);
	enm.insert("Arctangent", NODE_MATH_ARCTANGENT);
	enm.insert("Power", NODE_MATH_POWER);
	enm.insert("Logarithm", NODE_MATH_LOGARITHM);
	enm.insert("Minimum", NODE_MATH_MINIMUM);
	enm.insert("Maximum", NODE_MATH_MAXIMUM);
	enm.insert("Round", NODE_MATH_ROUND);
	enm.insert("Less Than", NODE_MATH_LESS_THAN);
	enm.insert("Greater Than", NODE_MATH_GREATER_THAN);

	return enm;
}

ShaderEnum MathNode::type_enum = math_type_init();

void MathNode::compile(SVMCompiler& compiler)
{
	ShaderInput *value1_in = input("Value1");
	ShaderInput *value2_in = input("Value2");
	ShaderOutput *value_out = output("Value");

	compiler.stack_assign(value1_in);
	compiler.stack_assign(value2_in);
	compiler.stack_assign(value_out);

	compiler.add_node(NODE_MATH, type_enum[type], value1_in->stack_offset, value2_in->stack_offset);
	compiler.add_node(NODE_MATH, value_out->stack_offset);
}

void MathNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("type", type);
	compiler.add(this, "node_math");
}

/* VectorMath */

VectorMathNode::VectorMathNode()
: ShaderNode("vector_math")
{
	type = ustring("Add");

	add_input("Vector1", SHADER_SOCKET_VECTOR);
	add_input("Vector2", SHADER_SOCKET_VECTOR);
	add_output("Value",  SHADER_SOCKET_FLOAT);
	add_output("Vector",  SHADER_SOCKET_VECTOR);
}

static ShaderEnum vector_math_type_init()
{
	ShaderEnum enm;

	enm.insert("Add", NODE_VECTOR_MATH_ADD);
	enm.insert("Subtract", NODE_VECTOR_MATH_SUBTRACT);
	enm.insert("Average", NODE_VECTOR_MATH_AVERAGE);
	enm.insert("Dot Product", NODE_VECTOR_MATH_DOT_PRODUCT);
	enm.insert("Cross Product", NODE_VECTOR_MATH_CROSS_PRODUCT);
	enm.insert("Normalize", NODE_VECTOR_MATH_NORMALIZE);

	return enm;
}

ShaderEnum VectorMathNode::type_enum = vector_math_type_init();

void VectorMathNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector1_in = input("Vector1");
	ShaderInput *vector2_in = input("Vector2");
	ShaderOutput *value_out = output("Value");
	ShaderOutput *vector_out = output("Vector");

	compiler.stack_assign(vector1_in);
	compiler.stack_assign(vector2_in);
	compiler.stack_assign(value_out);
	compiler.stack_assign(vector_out);

	compiler.add_node(NODE_VECTOR_MATH, type_enum[type], vector1_in->stack_offset, vector2_in->stack_offset);
	compiler.add_node(NODE_VECTOR_MATH, value_out->stack_offset, vector_out->stack_offset);
}

void VectorMathNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("type", type);
	compiler.add(this, "node_vector_math");
}

/* BumpNode */

BumpNode::BumpNode()
: ShaderNode("bump")
{
	add_input("SampleCenter", SHADER_SOCKET_FLOAT);
	add_input("SampleX", SHADER_SOCKET_FLOAT);
	add_input("SampleY", SHADER_SOCKET_FLOAT);

	add_output("Normal", SHADER_SOCKET_NORMAL);
}

void BumpNode::compile(SVMCompiler& compiler)
{
	ShaderInput *center_in = input("SampleCenter");
	ShaderInput *dx_in = input("SampleX");
	ShaderInput *dy_in = input("SampleY");

	compiler.stack_assign(center_in);
	compiler.stack_assign(dx_in);
	compiler.stack_assign(dy_in);

	compiler.add_node(NODE_SET_BUMP, center_in->stack_offset, dx_in->stack_offset, dy_in->stack_offset);
}

void BumpNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_bump");
}

/* RGBCurvesNode */

RGBCurvesNode::RGBCurvesNode()
: ShaderNode("rgb_curves")
{
	add_input("Fac", SHADER_SOCKET_FLOAT);
	add_input("Color", SHADER_SOCKET_COLOR);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void RGBCurvesNode::compile(SVMCompiler& compiler)
{
	ShaderInput *fac_in = input("Fac");
	ShaderInput *color_in = input("Color");
	ShaderOutput *color_out = output("Color");

	compiler.stack_assign(fac_in);
	compiler.stack_assign(color_in);
	compiler.stack_assign(color_out);

	compiler.add_node(NODE_RGB_CURVES, fac_in->stack_offset, color_in->stack_offset, color_out->stack_offset);
	compiler.add_array(curves, RAMP_TABLE_SIZE);
}

void RGBCurvesNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_rgb_curves");
}

/* RGBRampNode */

RGBRampNode::RGBRampNode()
: ShaderNode("rgb_ramp")
{
	add_input("Fac", SHADER_SOCKET_FLOAT);
	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Alpha", SHADER_SOCKET_FLOAT);
}

void RGBRampNode::compile(SVMCompiler& compiler)
{
	ShaderInput *fac_in = input("Fac");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *alpha_out = output("Alpha");

	compiler.stack_assign(fac_in);
	if(!color_out->links.empty())
		compiler.stack_assign(color_out);
	if(!alpha_out->links.empty())
		compiler.stack_assign(alpha_out);

	compiler.add_node(NODE_RGB_RAMP, fac_in->stack_offset, color_out->stack_offset, alpha_out->stack_offset);
	compiler.add_array(ramp, RAMP_TABLE_SIZE);
}

void RGBRampNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_rgb_ramp");
}

CCL_NAMESPACE_END

