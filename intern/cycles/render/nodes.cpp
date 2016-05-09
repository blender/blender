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

#include "image.h"
#include "integrator.h"
#include "nodes.h"
#include "scene.h"
#include "svm.h"
#include "svm_math_util.h"
#include "osl.h"

#include "util_sky_model.h"
#include "util_foreach.h"
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

	type = TEXTURE;

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
	
	float3 scale_clamped = scale;

	if(type == TEXTURE || type == NORMAL) {
		/* keep matrix invertible */
		if(fabsf(scale.x) < 1e-5f)
			scale_clamped.x = signf(scale.x)*1e-5f;
		if(fabsf(scale.y) < 1e-5f)
			scale_clamped.y = signf(scale.y)*1e-5f;
		if(fabsf(scale.z) < 1e-5f)
			scale_clamped.z = signf(scale.z)*1e-5f;
	}
	
	Transform smat = transform_scale(scale_clamped);
	Transform rmat = transform_euler(rotation);
	Transform tmat = transform_translate(translation);

	Transform mat;

	switch(type) {
		case TEXTURE:
			/* inverse transform on texture coordinate gives
			 * forward transform on texture */
			mat = tmat*rmat*smat;
			mat = transform_inverse(mat);
			break;
		case POINT:
			/* full transform */
			mat = tmat*rmat*smat;
			break;
		case VECTOR:
			/* no translation for vectors */
			mat = rmat*smat;
			break;
		case NORMAL:
			/* no translation for normals, and inverse transpose */
			mat = rmat*smat;
			mat = transform_inverse(mat);
			mat = transform_transpose(mat);
			break;
	}

	/* projection last */
	mat = mat*mmat;

	return mat;
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

	if(type == NORMAL) {
		compiler.add_node(NODE_VECTOR_MATH, NODE_VECTOR_MATH_NORMALIZE, offset_out, offset_out);
		compiler.add_node(NODE_VECTOR_MATH, SVM_STACK_INVALID, offset_out);
	}
}

/* Convenience function for texture nodes, allocating stack space to output
 * a modified vector and returning its offset */
int TextureMapping::compile_begin(SVMCompiler& compiler, ShaderInput *vector_in)
{
	if(!skip()) {
		int offset_in = compiler.stack_assign(vector_in);
		int offset_out = compiler.stack_find_offset(SHADER_SOCKET_VECTOR);

		compile(compiler, offset_in, offset_out);

		return offset_out;
	}

	return compiler.stack_assign(vector_in);
}

void TextureMapping::compile_end(SVMCompiler& compiler, ShaderInput *vector_in, int vector_offset)
{
	if(!skip()) {
		compiler.stack_clear_offset(vector_in->type, vector_offset);
	}
}

void TextureMapping::compile(OSLCompiler &compiler)
{
	if(!skip()) {
		Transform tfm = transform_transpose(compute_transform());

		compiler.parameter("mapping", tfm);
		compiler.parameter("use_mapping", 1);
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

static ShaderEnum image_projection_init()
{
	ShaderEnum enm;

	enm.insert("Flat", NODE_IMAGE_PROJ_FLAT);
	enm.insert("Box", NODE_IMAGE_PROJ_BOX);
	enm.insert("Sphere", NODE_IMAGE_PROJ_SPHERE);
	enm.insert("Tube", NODE_IMAGE_PROJ_TUBE);

	return enm;
}

static const char* get_osl_interpolation_parameter(InterpolationType interpolation)
{
	switch(interpolation) {
		case INTERPOLATION_CLOSEST:
			return "closest";
		case INTERPOLATION_CUBIC:
			return "cubic";
		case INTERPOLATION_SMART:
			return "smart";
		case INTERPOLATION_LINEAR:
		default:
			return "linear";
	}
}

ShaderEnum ImageTextureNode::color_space_enum = color_space_init();
ShaderEnum ImageTextureNode::projection_enum = image_projection_init();

ImageTextureNode::ImageTextureNode()
: ImageSlotTextureNode("image_texture")
{
	image_manager = NULL;
	slot = -1;
	is_float = -1;
	is_linear = false;
	use_alpha = true;
	filename = "";
	builtin_data = NULL;
	color_space = ustring("Color");
	projection = ustring("Flat");
	interpolation = INTERPOLATION_LINEAR;
	extension = EXTENSION_REPEAT;
	projection_blend = 0.0f;
	animated = false;

	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_UV);
	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Alpha", SHADER_SOCKET_FLOAT);
}

ImageTextureNode::~ImageTextureNode()
{
	if(image_manager) {
		image_manager->remove_image(filename,
		                            builtin_data,
		                            interpolation,
		                            extension);
	}
}

ShaderNode *ImageTextureNode::clone() const
{
	ImageTextureNode *node = new ImageTextureNode(*this);
	node->image_manager = NULL;
	node->slot = -1;
	node->is_float = -1;
	node->is_linear = false;
	return node;
}

void ImageTextureNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
#ifdef WITH_PTEX
	/* todo: avoid loading other texture coordinates when using ptex,
	 * and hide texture coordinate socket in the UI */
	if(shader->has_surface && string_endswith(filename, ".ptx")) {
		/* ptex */
		attributes->add(ATTR_STD_PTEX_FACE_ID);
		attributes->add(ATTR_STD_PTEX_UV);
	}
#endif

	ShaderNode::attributes(shader, attributes);
}

void ImageTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *alpha_out = output("Alpha");

	image_manager = compiler.image_manager;
	if(is_float == -1) {
		bool is_float_bool;
		slot = image_manager->add_image(filename,
		                                builtin_data,
		                                animated,
		                                0,
		                                is_float_bool,
		                                is_linear,
		                                interpolation,
		                                extension,
		                                use_alpha);
		is_float = (int)is_float_bool;
	}

	if(slot != -1) {
		int srgb = (is_linear || color_space != "Color")? 0: 1;
		int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

		if(projection != "Box") {
			compiler.add_node(NODE_TEX_IMAGE,
				slot,
				compiler.encode_uchar4(
					vector_offset,
					compiler.stack_assign_if_linked(color_out),
					compiler.stack_assign_if_linked(alpha_out),
					srgb),
				projection_enum[projection]);
		}
		else {
			compiler.add_node(NODE_TEX_IMAGE_BOX,
				slot,
				compiler.encode_uchar4(
					vector_offset,
					compiler.stack_assign_if_linked(color_out),
					compiler.stack_assign_if_linked(alpha_out),
					srgb),
				__float_as_int(projection_blend));
		}

		tex_mapping.compile_end(compiler, vector_in, vector_offset);
	}
	else {
		/* image not found */
		if(!color_out->links.empty()) {
			compiler.add_node(NODE_VALUE_V, compiler.stack_assign(color_out));
			compiler.add_node(NODE_VALUE_V, make_float3(TEX_IMAGE_MISSING_R,
			                                            TEX_IMAGE_MISSING_G,
			                                            TEX_IMAGE_MISSING_B));
		}
		if(!alpha_out->links.empty())
			compiler.add_node(NODE_VALUE_F, __float_as_int(TEX_IMAGE_MISSING_A), compiler.stack_assign(alpha_out));
	}
}

void ImageTextureNode::compile(OSLCompiler& compiler)
{
	ShaderOutput *alpha_out = output("Alpha");

	tex_mapping.compile(compiler);

	image_manager = compiler.image_manager;
	if(is_float == -1) {
		if(builtin_data == NULL) {
			ImageManager::ImageDataType type;
			type = image_manager->get_image_metadata(filename, NULL, is_linear);
			if(type == ImageManager::IMAGE_DATA_TYPE_FLOAT || type == ImageManager::IMAGE_DATA_TYPE_FLOAT4)
				is_float = 1;
		}
		else {
			bool is_float_bool;
			slot = image_manager->add_image(filename,
			                                builtin_data,
			                                animated,
			                                0,
			                                is_float_bool,
			                                is_linear,
			                                interpolation,
			                                extension,
			                                use_alpha);
			is_float = (int)is_float_bool;
		}
	}

	if(slot == -1) {
		compiler.parameter("filename", filename.c_str());
	}
	else {
		/* TODO(sergey): It's not so simple to pass custom attribute
		 * to the texture() function in order to make builtin images
		 * support more clear. So we use special file name which is
		 * "@<slot_number>" and check whether file name matches this
		 * mask in the OSLRenderServices::texture().
		 */
		compiler.parameter("filename", string_printf("@%d", slot).c_str());
	}
	if(is_linear || color_space != "Color")
		compiler.parameter("color_space", "Linear");
	else
		compiler.parameter("color_space", "sRGB");
	compiler.parameter("projection", projection);
	compiler.parameter("projection_blend", projection_blend);
	compiler.parameter("is_float", is_float);
	compiler.parameter("use_alpha", !alpha_out->links.empty());
	compiler.parameter("interpolation", get_osl_interpolation_parameter(interpolation));

	switch(extension) {
		case EXTENSION_EXTEND:
			compiler.parameter("wrap", "clamp");
			break;
		case EXTENSION_CLIP:
			compiler.parameter("wrap", "black");
			break;
		case EXTENSION_REPEAT:
		default:
			compiler.parameter("wrap", "periodic");
			break;
	}

	compiler.add(this, "node_image_texture");
}

/* Environment Texture */

static ShaderEnum env_projection_init()
{
	ShaderEnum enm;

	enm.insert("Equirectangular", 0);
	enm.insert("Mirror Ball", 1);

	return enm;
}

ShaderEnum EnvironmentTextureNode::color_space_enum = color_space_init();
ShaderEnum EnvironmentTextureNode::projection_enum = env_projection_init();

EnvironmentTextureNode::EnvironmentTextureNode()
: ImageSlotTextureNode("environment_texture")
{
	image_manager = NULL;
	slot = -1;
	is_float = -1;
	is_linear = false;
	use_alpha = true;
	filename = "";
	builtin_data = NULL;
	color_space = ustring("Color");
	interpolation = INTERPOLATION_LINEAR;
	projection = ustring("Equirectangular");
	animated = false;

	add_input("Vector", SHADER_SOCKET_VECTOR, ShaderInput::POSITION);
	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Alpha", SHADER_SOCKET_FLOAT);
}

EnvironmentTextureNode::~EnvironmentTextureNode()
{
	if(image_manager) {
		image_manager->remove_image(filename,
		                            builtin_data,
		                            interpolation,
		                            EXTENSION_REPEAT);
	}
}

ShaderNode *EnvironmentTextureNode::clone() const
{
	EnvironmentTextureNode *node = new EnvironmentTextureNode(*this);
	node->image_manager = NULL;
	node->slot = -1;
	node->is_float = -1;
	node->is_linear = false;
	return node;
}

void EnvironmentTextureNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
#ifdef WITH_PTEX
	if(shader->has_surface && string_endswith(filename, ".ptx")) {
		/* ptex */
		attributes->add(ATTR_STD_PTEX_FACE_ID);
		attributes->add(ATTR_STD_PTEX_UV);
	}
#endif

	ShaderNode::attributes(shader, attributes);
}

void EnvironmentTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *alpha_out = output("Alpha");

	image_manager = compiler.image_manager;
	if(slot == -1) {
		bool is_float_bool;
		slot = image_manager->add_image(filename,
		                                builtin_data,
		                                animated,
		                                0,
		                                is_float_bool,
		                                is_linear,
		                                interpolation,
		                                EXTENSION_REPEAT,
		                                use_alpha);
		is_float = (int)is_float_bool;
	}

	if(slot != -1) {
		int srgb = (is_linear || color_space != "Color")? 0: 1;
		int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

		compiler.add_node(NODE_TEX_ENVIRONMENT,
			slot,
			compiler.encode_uchar4(
				vector_offset,
				compiler.stack_assign_if_linked(color_out),
				compiler.stack_assign_if_linked(alpha_out),
				srgb),
			projection_enum[projection]);
	
		tex_mapping.compile_end(compiler, vector_in, vector_offset);
	}
	else {
		/* image not found */
		if(!color_out->links.empty()) {
			compiler.add_node(NODE_VALUE_V, compiler.stack_assign(color_out));
			compiler.add_node(NODE_VALUE_V, make_float3(TEX_IMAGE_MISSING_R,
			                                            TEX_IMAGE_MISSING_G,
			                                            TEX_IMAGE_MISSING_B));
		}
		if(!alpha_out->links.empty())
			compiler.add_node(NODE_VALUE_F, __float_as_int(TEX_IMAGE_MISSING_A), compiler.stack_assign(alpha_out));
	}
}

void EnvironmentTextureNode::compile(OSLCompiler& compiler)
{
	ShaderOutput *alpha_out = output("Alpha");

	tex_mapping.compile(compiler);

	/* See comments in ImageTextureNode::compile about support
	 * of builtin images.
	 */
	image_manager = compiler.image_manager;
	if(is_float == -1) {
		if(builtin_data == NULL) {
			ImageManager::ImageDataType type;
			type = image_manager->get_image_metadata(filename, NULL, is_linear);
			if(type == ImageManager::IMAGE_DATA_TYPE_FLOAT || type == ImageManager::IMAGE_DATA_TYPE_FLOAT4)
				is_float = 1;
		}
		else {
			bool is_float_bool;
			slot = image_manager->add_image(filename,
			                                builtin_data,
			                                animated,
			                                0,
			                                is_float_bool,
			                                is_linear,
			                                interpolation,
			                                EXTENSION_REPEAT,
			                                use_alpha);
			is_float = (int)is_float_bool;
		}
	}

	if(slot == -1) {
		compiler.parameter("filename", filename.c_str());
	}
	else {
		compiler.parameter("filename", string_printf("@%d", slot).c_str());
	}
	compiler.parameter("projection", projection);
	if(is_linear || color_space != "Color")
		compiler.parameter("color_space", "Linear");
	else
		compiler.parameter("color_space", "sRGB");

	compiler.parameter("interpolation", get_osl_interpolation_parameter(interpolation));

	compiler.parameter("is_float", is_float);
	compiler.parameter("use_alpha", !alpha_out->links.empty());
	compiler.add(this, "node_environment_texture");
}

/* Sky Texture */

static float2 sky_spherical_coordinates(float3 dir)
{
	return make_float2(acosf(dir.z), atan2f(dir.x, dir.y));
}

typedef struct SunSky {
	/* sun direction in spherical and cartesian */
	float theta, phi;

	/* Parameter */
	float radiance_x, radiance_y, radiance_z;
	float config_x[9], config_y[9], config_z[9];
} SunSky;

/* Preetham model */
static float sky_perez_function(float lam[6], float theta, float gamma)
{
	return (1.0f + lam[0]*expf(lam[1]/cosf(theta))) * (1.0f + lam[2]*expf(lam[3]*gamma)  + lam[4]*cosf(gamma)*cosf(gamma));
}

static void sky_texture_precompute_old(SunSky *sunsky, float3 dir, float turbidity)
{
	/*
	* We re-use the SunSky struct of the new model, to avoid extra variables
	* zenith_Y/x/y is now radiance_x/y/z
	* perez_Y/x/y is now config_x/y/z
	*/
	
	float2 spherical = sky_spherical_coordinates(dir);
	float theta = spherical.x;
	float phi = spherical.y;

	sunsky->theta = theta;
	sunsky->phi = phi;

	float theta2 = theta*theta;
	float theta3 = theta2*theta;
	float T = turbidity;
	float T2 = T * T;

	float chi = (4.0f / 9.0f - T / 120.0f) * (M_PI_F - 2.0f * theta);
	sunsky->radiance_x = (4.0453f * T - 4.9710f) * tanf(chi) - 0.2155f * T + 2.4192f;
	sunsky->radiance_x *= 0.06f;

	sunsky->radiance_y =
	(0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * theta) * T2 +
	(-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * theta + 0.00394f) * T +
	(0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * theta + 0.25886f);

	sunsky->radiance_z =
	(0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * theta) * T2 +
	(-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * theta  + 0.00516f) * T +
	(0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * theta  + 0.26688f);

	sunsky->config_x[0] = (0.1787f * T  - 1.4630f);
	sunsky->config_x[1] = (-0.3554f * T  + 0.4275f);
	sunsky->config_x[2] = (-0.0227f * T  + 5.3251f);
	sunsky->config_x[3] = (0.1206f * T  - 2.5771f);
	sunsky->config_x[4] = (-0.0670f * T  + 0.3703f);

	sunsky->config_y[0] = (-0.0193f * T  - 0.2592f);
	sunsky->config_y[1] = (-0.0665f * T  + 0.0008f);
	sunsky->config_y[2] = (-0.0004f * T  + 0.2125f);
	sunsky->config_y[3] = (-0.0641f * T  - 0.8989f);
	sunsky->config_y[4] = (-0.0033f * T  + 0.0452f);

	sunsky->config_z[0] = (-0.0167f * T  - 0.2608f);
	sunsky->config_z[1] = (-0.0950f * T  + 0.0092f);
	sunsky->config_z[2] = (-0.0079f * T  + 0.2102f);
	sunsky->config_z[3] = (-0.0441f * T  - 1.6537f);
	sunsky->config_z[4] = (-0.0109f * T  + 0.0529f);

	/* unused for old sky model */
	for(int i = 5; i < 9; i++) {
		sunsky->config_x[i] = 0.0f;
		sunsky->config_y[i] = 0.0f;
		sunsky->config_z[i] = 0.0f;
	}

	sunsky->radiance_x /= sky_perez_function(sunsky->config_x, 0, theta);
	sunsky->radiance_y /= sky_perez_function(sunsky->config_y, 0, theta);
	sunsky->radiance_z /= sky_perez_function(sunsky->config_z, 0, theta);
}

/* Hosek / Wilkie */
static void sky_texture_precompute_new(SunSky *sunsky, float3 dir, float turbidity, float ground_albedo)
{
	/* Calculate Sun Direction and save coordinates */
	float2 spherical = sky_spherical_coordinates(dir);
	float theta = spherical.x;
	float phi = spherical.y;
	
	/* Clamp Turbidity */
	turbidity = clamp(turbidity, 0.0f, 10.0f); 
	
	/* Clamp to Horizon */
	theta = clamp(theta, 0.0f, M_PI_2_F); 

	sunsky->theta = theta;
	sunsky->phi = phi;

	double solarElevation = M_PI_2_F - theta;

	/* Initialize Sky Model */
	ArHosekSkyModelState *sky_state;
	sky_state = arhosek_xyz_skymodelstate_alloc_init(turbidity, ground_albedo, solarElevation);

	/* Copy values from sky_state to SunSky */
	for(int i = 0; i < 9; ++i) {
		sunsky->config_x[i] = (float)sky_state->configs[0][i];
		sunsky->config_y[i] = (float)sky_state->configs[1][i];
		sunsky->config_z[i] = (float)sky_state->configs[2][i];
	}
	sunsky->radiance_x = (float)sky_state->radiances[0];
	sunsky->radiance_y = (float)sky_state->radiances[1];
	sunsky->radiance_z = (float)sky_state->radiances[2];

	/* Free sky_state */
	arhosekskymodelstate_free(sky_state);
}

static ShaderEnum sky_type_init()
{
	ShaderEnum enm;

	enm.insert("Preetham", NODE_SKY_OLD);
	enm.insert("Hosek / Wilkie", NODE_SKY_NEW);

	return enm;
}

ShaderEnum SkyTextureNode::type_enum = sky_type_init();

SkyTextureNode::SkyTextureNode()
: TextureNode("sky_texture")
{
	type = ustring("Hosek / Wilkie");
	
	sun_direction = make_float3(0.0f, 0.0f, 1.0f);
	turbidity = 2.2f;
	ground_albedo = 0.3f;

	add_input("Vector", SHADER_SOCKET_VECTOR, ShaderInput::POSITION);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void SkyTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *color_out = output("Color");

	SunSky sunsky;
	if(type_enum[type] == NODE_SKY_OLD)
		sky_texture_precompute_old(&sunsky, sun_direction, turbidity);
	else if(type_enum[type] == NODE_SKY_NEW)
		sky_texture_precompute_new(&sunsky, sun_direction, turbidity, ground_albedo);
	else
		assert(false);

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);
	int sky_model = type_enum[type];

	compiler.stack_assign(color_out);
	compiler.add_node(NODE_TEX_SKY, vector_offset, compiler.stack_assign(color_out), sky_model);
	compiler.add_node(__float_as_uint(sunsky.phi), __float_as_uint(sunsky.theta), __float_as_uint(sunsky.radiance_x), __float_as_uint(sunsky.radiance_y));
	compiler.add_node(__float_as_uint(sunsky.radiance_z), __float_as_uint(sunsky.config_x[0]), __float_as_uint(sunsky.config_x[1]), __float_as_uint(sunsky.config_x[2]));
	compiler.add_node(__float_as_uint(sunsky.config_x[3]), __float_as_uint(sunsky.config_x[4]), __float_as_uint(sunsky.config_x[5]), __float_as_uint(sunsky.config_x[6]));
	compiler.add_node(__float_as_uint(sunsky.config_x[7]), __float_as_uint(sunsky.config_x[8]), __float_as_uint(sunsky.config_y[0]), __float_as_uint(sunsky.config_y[1]));
	compiler.add_node(__float_as_uint(sunsky.config_y[2]), __float_as_uint(sunsky.config_y[3]), __float_as_uint(sunsky.config_y[4]), __float_as_uint(sunsky.config_y[5]));
	compiler.add_node(__float_as_uint(sunsky.config_y[6]), __float_as_uint(sunsky.config_y[7]), __float_as_uint(sunsky.config_y[8]), __float_as_uint(sunsky.config_z[0]));
	compiler.add_node(__float_as_uint(sunsky.config_z[1]), __float_as_uint(sunsky.config_z[2]), __float_as_uint(sunsky.config_z[3]), __float_as_uint(sunsky.config_z[4]));
	compiler.add_node(__float_as_uint(sunsky.config_z[5]), __float_as_uint(sunsky.config_z[6]), __float_as_uint(sunsky.config_z[7]), __float_as_uint(sunsky.config_z[8]));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void SkyTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

	SunSky sunsky;

	if(type_enum[type] == NODE_SKY_OLD)
		sky_texture_precompute_old(&sunsky, sun_direction, turbidity);
	else if(type_enum[type] == NODE_SKY_NEW)
		sky_texture_precompute_new(&sunsky, sun_direction, turbidity, ground_albedo);
	else
		assert(false);
		
	compiler.parameter("sky_model", type);
	compiler.parameter("theta", sunsky.theta);
	compiler.parameter("phi", sunsky.phi);
	compiler.parameter_color("radiance", make_float3(sunsky.radiance_x, sunsky.radiance_y, sunsky.radiance_z));
	compiler.parameter_array("config_x", sunsky.config_x, 9);
	compiler.parameter_array("config_y", sunsky.config_y, 9);
	compiler.parameter_array("config_z", sunsky.config_z, 9);
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

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

	compiler.add_node(NODE_TEX_GRADIENT,
		compiler.encode_uchar4(
			type_enum[type],
			vector_offset,
			compiler.stack_assign_if_linked(fac_out),
			compiler.stack_assign_if_linked(color_out)));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void GradientTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

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

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

	compiler.add_node(NODE_TEX_NOISE,
		compiler.encode_uchar4(
			vector_offset,
			compiler.stack_assign_if_linked(scale_in),
			compiler.stack_assign_if_linked(detail_in),
			compiler.stack_assign_if_linked(distortion_in)),
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(color_out),
			compiler.stack_assign_if_linked(fac_out)));
	compiler.add_node(
		__float_as_int(scale_in->value.x),
		__float_as_int(detail_in->value.x),
		__float_as_int(distortion_in->value.x));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void NoiseTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

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

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

	compiler.add_node(NODE_TEX_VORONOI,
		coloring_enum[coloring],
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(scale_in),
			vector_offset,
			compiler.stack_assign(fac_out),
			compiler.stack_assign(color_out)),
		__float_as_int(scale_in->value.x));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void VoronoiTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

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

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

	compiler.add_node(NODE_TEX_MUSGRAVE,
		compiler.encode_uchar4(
			type_enum[type],
			vector_offset,
			compiler.stack_assign_if_linked(color_out),
			compiler.stack_assign_if_linked(fac_out)),
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(dimension_in),
			compiler.stack_assign_if_linked(lacunarity_in),
			compiler.stack_assign_if_linked(detail_in),
			compiler.stack_assign_if_linked(offset_in)),
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(gain_in),
			compiler.stack_assign_if_linked(scale_in)));
	compiler.add_node(__float_as_int(dimension_in->value.x),
		__float_as_int(lacunarity_in->value.x),
		__float_as_int(detail_in->value.x),
		__float_as_int(offset_in->value.x));
	compiler.add_node(__float_as_int(gain_in->value.x),
		__float_as_int(scale_in->value.x));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void MusgraveTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

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

static ShaderEnum wave_profile_init()
{
	ShaderEnum enm;

	enm.insert("Sine", NODE_WAVE_PROFILE_SIN);
	enm.insert("Saw", NODE_WAVE_PROFILE_SAW);

	return enm;
}

ShaderEnum WaveTextureNode::type_enum = wave_type_init();
ShaderEnum WaveTextureNode::profile_enum = wave_profile_init();

WaveTextureNode::WaveTextureNode()
: TextureNode("wave_texture")
{
	type = ustring("Bands");
	profile = ustring("Sine");

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

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

	compiler.add_node(NODE_TEX_WAVE,
		compiler.encode_uchar4(
			type_enum[type],
			compiler.stack_assign_if_linked(color_out),
			compiler.stack_assign_if_linked(fac_out),
			compiler.stack_assign_if_linked(dscale_in)),
		compiler.encode_uchar4(
			vector_offset,
			compiler.stack_assign_if_linked(scale_in),
			compiler.stack_assign_if_linked(detail_in),
			compiler.stack_assign_if_linked(distortion_in)),
		profile_enum[profile]);

	compiler.add_node(
		__float_as_int(scale_in->value.x),
		__float_as_int(detail_in->value.x),
		__float_as_int(distortion_in->value.x),
		__float_as_int(dscale_in->value.x));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void WaveTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

	compiler.parameter("Type", type);
	compiler.parameter("Profile", profile);

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

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

	compiler.add_node(NODE_TEX_MAGIC,
		compiler.encode_uchar4(
			depth,
			compiler.stack_assign_if_linked(color_out),
			compiler.stack_assign_if_linked(fac_out)),
		compiler.encode_uchar4(
			vector_offset,
			compiler.stack_assign_if_linked(scale_in),
			compiler.stack_assign_if_linked(distortion_in)));
	compiler.add_node(
		__float_as_int(scale_in->value.x),
		__float_as_int(distortion_in->value.x));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void MagicTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

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

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

	compiler.add_node(NODE_TEX_CHECKER,
		compiler.encode_uchar4(
			vector_offset,
			compiler.stack_assign(color1_in),
			compiler.stack_assign(color2_in),
			compiler.stack_assign_if_linked(scale_in)),
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(color_out),
			compiler.stack_assign_if_linked(fac_out)),
		__float_as_int(scale_in->value.x));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void CheckerTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

	compiler.add(this, "node_checker_texture");
}

/* Brick Texture */

BrickTextureNode::BrickTextureNode()
: TextureNode("brick_texture")
{
	offset = 0.5f;
	offset_frequency = 2;
	squash = 1.0f;
	squash_frequency = 2;
	
	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::TEXTURE_GENERATED);
	add_input("Color1", SHADER_SOCKET_COLOR);
	add_input("Color2", SHADER_SOCKET_COLOR);
	add_input("Mortar", SHADER_SOCKET_COLOR);
	add_input("Scale", SHADER_SOCKET_FLOAT, 5.0f);
	add_input("Mortar Size", SHADER_SOCKET_FLOAT, 0.02f);
	add_input("Bias", SHADER_SOCKET_FLOAT, 0.0f);
	add_input("Brick Width", SHADER_SOCKET_FLOAT, 0.5f);
	add_input("Row Height", SHADER_SOCKET_FLOAT, 0.25f);

	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void BrickTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderInput *color1_in = input("Color1");
	ShaderInput *color2_in = input("Color2");
	ShaderInput *mortar_in = input("Mortar");
	ShaderInput *scale_in = input("Scale");
	ShaderInput *mortar_size_in = input("Mortar Size");
	ShaderInput *bias_in = input("Bias");
	ShaderInput *brick_width_in = input("Brick Width");
	ShaderInput *row_height_in = input("Row Height");
	
	ShaderOutput *color_out = output("Color");
	ShaderOutput *fac_out = output("Fac");

	int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

	compiler.add_node(NODE_TEX_BRICK,
		compiler.encode_uchar4(
			vector_offset,
			compiler.stack_assign(color1_in),
			compiler.stack_assign(color2_in),
			compiler.stack_assign(mortar_in)),
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(scale_in),
			compiler.stack_assign_if_linked(mortar_size_in),
			compiler.stack_assign_if_linked(bias_in),
			compiler.stack_assign_if_linked(brick_width_in)),
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(row_height_in),
			compiler.stack_assign_if_linked(color_out),
			compiler.stack_assign_if_linked(fac_out)));
			
	compiler.add_node(compiler.encode_uchar4(offset_frequency, squash_frequency),
		__float_as_int(scale_in->value.x),
		__float_as_int(mortar_size_in->value.x),
		__float_as_int(bias_in->value.x));

	compiler.add_node(__float_as_int(brick_width_in->value.x),
		__float_as_int(row_height_in->value.x),
		__float_as_int(offset),
		__float_as_int(squash));

	tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void BrickTextureNode::compile(OSLCompiler& compiler)
{
	tex_mapping.compile(compiler);

	compiler.parameter("Offset", offset);
	compiler.parameter("OffsetFrequency", offset_frequency);
	compiler.parameter("Squash", squash);
	compiler.parameter("SquashFrequency", squash_frequency);
	compiler.add(this, "node_brick_texture");
}

/* Point Density Texture */

static ShaderEnum point_density_space_init()
{
	ShaderEnum enm;

	enm.insert("Object", NODE_TEX_VOXEL_SPACE_OBJECT);
	enm.insert("World", NODE_TEX_VOXEL_SPACE_WORLD);

	return enm;
}

ShaderEnum PointDensityTextureNode::space_enum = point_density_space_init();

PointDensityTextureNode::PointDensityTextureNode()
: ShaderNode("point_density")
{
	image_manager = NULL;
	slot = -1;
	filename = "";
	space = ustring("Object");
	builtin_data = NULL;
	interpolation = INTERPOLATION_LINEAR;

	tfm = transform_identity();

	add_input("Vector", SHADER_SOCKET_POINT, ShaderInput::POSITION);
	add_output("Density", SHADER_SOCKET_FLOAT);
	add_output("Color", SHADER_SOCKET_COLOR);
}

PointDensityTextureNode::~PointDensityTextureNode()
{
	if(image_manager) {
		image_manager->remove_image(filename,
		                            builtin_data,
		                            interpolation,
		                            EXTENSION_CLIP);
	}
}

ShaderNode *PointDensityTextureNode::clone() const
{
	PointDensityTextureNode *node = new PointDensityTextureNode(*this);
	node->image_manager = NULL;
	node->slot = -1;
	return node;
}

void PointDensityTextureNode::attributes(Shader *shader,
                                         AttributeRequestSet *attributes)
{
	if(shader->has_volume)
		attributes->add(ATTR_STD_GENERATED_TRANSFORM);

	ShaderNode::attributes(shader, attributes);
}

void PointDensityTextureNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *density_out = output("Density");
	ShaderOutput *color_out = output("Color");

	const bool use_density = !density_out->links.empty();
	const bool use_color = !color_out->links.empty();

	image_manager = compiler.image_manager;

	if(use_density || use_color) {
		if(slot == -1) {
			bool is_float, is_linear;
			slot = image_manager->add_image(filename, builtin_data,
			                                false, 0,
			                                is_float, is_linear,
			                                interpolation,
			                                EXTENSION_CLIP,
			                                true);
		}

		if(slot != -1) {
			compiler.stack_assign(vector_in);
			compiler.add_node(NODE_TEX_VOXEL,
			                  slot,
			                  compiler.encode_uchar4(compiler.stack_assign(vector_in),
			                                         compiler.stack_assign_if_linked(density_out),
			                                         compiler.stack_assign_if_linked(color_out),
			                                         space_enum[space]));
			if(space == "World") {
				compiler.add_node(tfm.x);
				compiler.add_node(tfm.y);
				compiler.add_node(tfm.z);
				compiler.add_node(tfm.w);
			}
		}
		else {
			if(use_density) {
				compiler.add_node(NODE_VALUE_F,
								  __float_as_int(0.0f),
								  compiler.stack_assign(density_out));
			}
			if (use_color) {
				compiler.add_node(NODE_VALUE_V, compiler.stack_assign(color_out));
				compiler.add_node(NODE_VALUE_V, make_float3(TEX_IMAGE_MISSING_R,
															TEX_IMAGE_MISSING_G,
															TEX_IMAGE_MISSING_B));
			}
		}
	}
}

void PointDensityTextureNode::compile(OSLCompiler& compiler)
{
	ShaderOutput *density_out = output("Density");
	ShaderOutput *color_out = output("Color");

	const bool use_density = !density_out->links.empty();
	const bool use_color = !color_out->links.empty();

	image_manager = compiler.image_manager;

	if(use_density || use_color) {
		if(slot == -1) {
			bool is_float, is_linear;
			slot = image_manager->add_image(filename, builtin_data,
			                                false, 0,
			                                is_float, is_linear,
			                                interpolation,
			                                EXTENSION_CLIP,
			                                true);
		}

		if(slot != -1) {
			compiler.parameter("filename", string_printf("@%d", slot).c_str());
		}
		if(space == "World") {
			compiler.parameter("mapping", transform_transpose(tfm));
			compiler.parameter("use_mapping", 1);
		}
		switch(interpolation) {
			case INTERPOLATION_CLOSEST:
				compiler.parameter("interpolation", "closest");
				break;
			case INTERPOLATION_CUBIC:
				compiler.parameter("interpolation", "cubic");
				break;
			case INTERPOLATION_LINEAR:
			default:
				compiler.parameter("interpolation", "linear");
				break;
		}

		compiler.add(this, "node_voxel_texture");
	}
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

	compiler.add_node(NODE_NORMAL,
		compiler.stack_assign(normal_in),
		compiler.stack_assign(normal_out),
		compiler.stack_assign(dot_out));
	compiler.add_node(
		__float_as_int(direction.x),
		__float_as_int(direction.y),
		__float_as_int(direction.z));
}

void NormalNode::compile(OSLCompiler& compiler)
{
	compiler.parameter_normal("Direction", direction);
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

	tex_mapping.compile(compiler, compiler.stack_assign(vector_in), compiler.stack_assign(vector_out));
}

void MappingNode::compile(OSLCompiler& compiler)
{
	Transform tfm = transform_transpose(tex_mapping.compute_transform());
	compiler.parameter("Matrix", tfm);
	compiler.parameter_point("mapping_min", tex_mapping.min);
	compiler.parameter_point("mapping_max", tex_mapping.max);
	compiler.parameter("use_minmax", tex_mapping.use_minmax);

	compiler.add(this, "node_mapping");
}

/* Convert */

ConvertNode::ConvertNode(ShaderSocketType from_, ShaderSocketType to_, bool autoconvert)
: ShaderNode("convert")
{
	from = from_;
	to = to_;

	if(autoconvert) {
		if(from == to)
			special_type = SHADER_SPECIAL_TYPE_PROXY;
		else
			special_type = SHADER_SPECIAL_TYPE_AUTOCONVERT;
	}

	if(from == SHADER_SOCKET_FLOAT)
		add_input("Val", SHADER_SOCKET_FLOAT);
	else if(from == SHADER_SOCKET_INT)
		add_input("ValInt", SHADER_SOCKET_INT);
	else if(from == SHADER_SOCKET_COLOR)
		add_input("Color", SHADER_SOCKET_COLOR);
	else if(from == SHADER_SOCKET_VECTOR)
		add_input("Vector", SHADER_SOCKET_VECTOR);
	else if(from == SHADER_SOCKET_POINT)
		add_input("Point", SHADER_SOCKET_POINT);
	else if(from == SHADER_SOCKET_NORMAL)
		add_input("Normal", SHADER_SOCKET_NORMAL);
	else if(from == SHADER_SOCKET_STRING)
		add_input("String", SHADER_SOCKET_STRING);
	else if(from == SHADER_SOCKET_CLOSURE)
		add_input("Closure", SHADER_SOCKET_CLOSURE);
	else
		assert(0);

	if(to == SHADER_SOCKET_FLOAT)
		add_output("Val", SHADER_SOCKET_FLOAT);
	else if(to == SHADER_SOCKET_INT)
		add_output("ValInt", SHADER_SOCKET_INT);
	else if(to == SHADER_SOCKET_COLOR)
		add_output("Color", SHADER_SOCKET_COLOR);
	else if(to == SHADER_SOCKET_VECTOR)
		add_output("Vector", SHADER_SOCKET_VECTOR);
	else if(to == SHADER_SOCKET_POINT)
		add_output("Point", SHADER_SOCKET_POINT);
	else if(to == SHADER_SOCKET_NORMAL)
		add_output("Normal", SHADER_SOCKET_NORMAL);
	else if(to == SHADER_SOCKET_STRING)
		add_output("String", SHADER_SOCKET_STRING);
	else if(to == SHADER_SOCKET_CLOSURE)
		add_output("Closure", SHADER_SOCKET_CLOSURE);
	else
		assert(0);
}

bool ConvertNode::constant_fold(ShaderGraph * /*graph*/,
                                ShaderOutput * /*socket*/,
                                float3 *optimized_value)
{
	ShaderInput *in = inputs[0];
	float3 value = in->value;

	/* TODO(DingTo): conversion from/to int is not supported yet, don't fold in that case */

	if(in->link == NULL) {
		if(from == SHADER_SOCKET_FLOAT) {
			if(to == SHADER_SOCKET_INT)
				/* float to int */
				return false;
			else
				/* float to float3 */
				*optimized_value = make_float3(value.x, value.x, value.x);
		}
		else if(from == SHADER_SOCKET_INT) {
			if(to == SHADER_SOCKET_FLOAT)
				/* int to float */
				return false;
			else
				/* int to vector/point/normal */
				return false;
		}
		else if(to == SHADER_SOCKET_FLOAT) {
			if(from == SHADER_SOCKET_COLOR)
				/* color to float */
				optimized_value->x = linear_rgb_to_gray(value);
			else
				/* vector/point/normal to float */
				optimized_value->x = average(value);
		}
		else if(to == SHADER_SOCKET_INT) {
			if(from == SHADER_SOCKET_COLOR)
				/* color to int */
				return false;
			else
				/* vector/point/normal to int */
				return false;
		}
		else {
			*optimized_value = value;
		}

		return true;
	}

	return false;
}

void ConvertNode::compile(SVMCompiler& compiler)
{
	/* constant folding should eliminate proxy nodes */
	assert(from != to);

	ShaderInput *in = inputs[0];
	ShaderOutput *out = outputs[0];

	if(from == SHADER_SOCKET_FLOAT) {
		if(to == SHADER_SOCKET_INT)
			/* float to int */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_FI, compiler.stack_assign(in), compiler.stack_assign(out));
		else
			/* float to float3 */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_FV, compiler.stack_assign(in), compiler.stack_assign(out));
	}
	else if(from == SHADER_SOCKET_INT) {
		if(to == SHADER_SOCKET_FLOAT)
			/* int to float */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_IF, compiler.stack_assign(in), compiler.stack_assign(out));
		else
			/* int to vector/point/normal */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_IV, compiler.stack_assign(in), compiler.stack_assign(out));
	}
	else if(to == SHADER_SOCKET_FLOAT) {
		if(from == SHADER_SOCKET_COLOR)
			/* color to float */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_CF, compiler.stack_assign(in), compiler.stack_assign(out));
		else
			/* vector/point/normal to float */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_VF, compiler.stack_assign(in), compiler.stack_assign(out));
	}
	else if(to == SHADER_SOCKET_INT) {
		if(from == SHADER_SOCKET_COLOR)
			/* color to int */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_CI, compiler.stack_assign(in), compiler.stack_assign(out));
		else
			/* vector/point/normal to int */
			compiler.add_node(NODE_CONVERT, NODE_CONVERT_VI, compiler.stack_assign(in), compiler.stack_assign(out));
	}
	else {
		/* float3 to float3 */
		if(in->link) {
			/* no op in SVM */
			compiler.stack_link(in, out);
		}
		else {
			/* set 0,0,0 value */
			compiler.add_node(NODE_VALUE_V, compiler.stack_assign(out));
			compiler.add_node(NODE_VALUE_V, in->value);
		}
	}
}

void ConvertNode::compile(OSLCompiler& compiler)
{
	/* constant folding should eliminate proxy nodes */
	assert(from != to);

	if(from == SHADER_SOCKET_FLOAT)
		compiler.add(this, "node_convert_from_float");
	else if(from == SHADER_SOCKET_INT)
		compiler.add(this, "node_convert_from_int");
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

/* BSDF Closure */

BsdfNode::BsdfNode(bool scattering_)
: ShaderNode("bsdf"), scattering(scattering_)
{
	special_type = SHADER_SPECIAL_TYPE_CLOSURE;

	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("Normal", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL);
	add_input("SurfaceMixWeight", SHADER_SOCKET_FLOAT, 0.0f, ShaderInput::USE_SVM);

	if(scattering) {
		closure = CLOSURE_BSSRDF_CUBIC_ID;
		add_output("BSSRDF", SHADER_SOCKET_CLOSURE);
	}
	else {
		closure = CLOSURE_BSDF_DIFFUSE_ID;
		add_output("BSDF", SHADER_SOCKET_CLOSURE);
	}
}

void BsdfNode::compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2, ShaderInput *param3, ShaderInput *param4)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *normal_in = input("Normal");
	ShaderInput *tangent_in = input("Tangent");

	if(color_in->link)
		compiler.add_node(NODE_CLOSURE_WEIGHT, compiler.stack_assign(color_in));
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value);

	int normal_offset = compiler.stack_assign_if_linked(normal_in);
	int tangent_offset = (tangent_in) ? compiler.stack_assign_if_linked(tangent_in) : SVM_STACK_INVALID;
	int param3_offset = (param3) ? compiler.stack_assign(param3) : SVM_STACK_INVALID;
	int param4_offset = (param4) ? compiler.stack_assign(param4) : SVM_STACK_INVALID;

	compiler.add_node(NODE_CLOSURE_BSDF,
		compiler.encode_uchar4(closure,
			(param1)? compiler.stack_assign(param1): SVM_STACK_INVALID,
			(param2)? compiler.stack_assign(param2): SVM_STACK_INVALID,
			compiler.closure_mix_weight_offset()),
		__float_as_int((param1)? param1->value.x: 0.0f),
		__float_as_int((param2)? param2->value.x: 0.0f));

	compiler.add_node(normal_offset, tangent_offset, param3_offset, param4_offset);
}

void BsdfNode::compile(SVMCompiler& compiler)
{
	compile(compiler, NULL, NULL);
}

void BsdfNode::compile(OSLCompiler& /*compiler*/)
{
	assert(0);
}

/* Anisotropic BSDF Closure */

static ShaderEnum aniso_distribution_init()
{
	ShaderEnum enm;

	enm.insert("Beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID);
	enm.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID);
	enm.insert("Ashikhmin-Shirley", CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID);

	return enm;
}

ShaderEnum AnisotropicBsdfNode::distribution_enum = aniso_distribution_init();

AnisotropicBsdfNode::AnisotropicBsdfNode()
{
	closure = CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID;
	distribution = ustring("GGX");

	add_input("Tangent", SHADER_SOCKET_VECTOR, ShaderInput::TANGENT);

	add_input("Roughness", SHADER_SOCKET_FLOAT, 0.2f);
	add_input("Anisotropy", SHADER_SOCKET_FLOAT, 0.5f);
	add_input("Rotation", SHADER_SOCKET_FLOAT, 0.0f);
}

void AnisotropicBsdfNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	if(shader->has_surface) {
		ShaderInput *tangent_in = input("Tangent");

		if(!tangent_in->link)
			attributes->add(ATTR_STD_GENERATED);
	}

	ShaderNode::attributes(shader, attributes);
}

void AnisotropicBsdfNode::compile(SVMCompiler& compiler)
{
	closure = (ClosureType)distribution_enum[distribution];

	BsdfNode::compile(compiler, input("Roughness"), input("Anisotropy"), input("Rotation"));
}

void AnisotropicBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("distribution", distribution);
	compiler.add(this, "node_anisotropic_bsdf");
}

/* Glossy BSDF Closure */

static ShaderEnum glossy_distribution_init()
{
	ShaderEnum enm;

	enm.insert("Sharp", CLOSURE_BSDF_REFLECTION_ID);
	enm.insert("Beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
	enm.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_ID);
	enm.insert("Ashikhmin-Shirley", CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID);

	return enm;
}

ShaderEnum GlossyBsdfNode::distribution_enum = glossy_distribution_init();

GlossyBsdfNode::GlossyBsdfNode()
{
	closure = CLOSURE_BSDF_MICROFACET_GGX_ID;
	distribution = ustring("GGX");
	distribution_orig = ustring("");

	add_input("Roughness", SHADER_SOCKET_FLOAT, 0.2f);
}

void GlossyBsdfNode::simplify_settings(Scene *scene)
{
	if(distribution_orig == "") {
		distribution_orig = distribution;
	}
	Integrator *integrator = scene->integrator;
	if(integrator->filter_glossy == 0.0f) {
		/* Fallback to Sharp closure for Roughness close to 0.
		 * Note: Keep the epsilon in sync with kernel!
		 */
		ShaderInput *roughness_input = input("Roughness");
		if(!roughness_input->link && roughness_input->value.x <= 1e-4f) {
			distribution = ustring("Sharp");
		}
	}
	else {
		/* Rollback to original distribution when filter glossy is used. */
		distribution = distribution_orig;
	}
	closure = (ClosureType)distribution_enum[distribution];
}

bool GlossyBsdfNode::has_integrator_dependency()
{
	ShaderInput *roughness_input = input("Roughness");
	return !roughness_input->link && roughness_input->value.x <= 1e-4f;
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

	enm.insert("Sharp", CLOSURE_BSDF_SHARP_GLASS_ID);
	enm.insert("Beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID);
	enm.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);

	return enm;
}

ShaderEnum GlassBsdfNode::distribution_enum = glass_distribution_init();

GlassBsdfNode::GlassBsdfNode()
{
	closure = CLOSURE_BSDF_SHARP_GLASS_ID;
	distribution = ustring("Sharp");
	distribution_orig = ustring("");

	add_input("Roughness", SHADER_SOCKET_FLOAT, 0.0f);
	add_input("IOR", SHADER_SOCKET_FLOAT, 0.3f);
}

void GlassBsdfNode::simplify_settings(Scene *scene)
{
	if(distribution_orig == "") {
		distribution_orig = distribution;
	}
	Integrator *integrator = scene->integrator;
	if(integrator->filter_glossy == 0.0f) {
		/* Fallback to Sharp closure for Roughness close to 0.
		 * Note: Keep the epsilon in sync with kernel!
		 */
		ShaderInput *roughness_input = input("Roughness");
		if(!roughness_input->link && roughness_input->value.x <= 1e-4f) {
			distribution = ustring("Sharp");
		}
	}
	else {
		/* Rollback to original distribution when filter glossy is used. */
		distribution = distribution_orig;
	}
	closure = (ClosureType)distribution_enum[distribution];
}

bool GlassBsdfNode::has_integrator_dependency()
{
	ShaderInput *roughness_input = input("Roughness");
	return !roughness_input->link && roughness_input->value.x <= 1e-4f;
}

void GlassBsdfNode::compile(SVMCompiler& compiler)
{
	closure = (ClosureType)distribution_enum[distribution];

	if(closure == CLOSURE_BSDF_SHARP_GLASS_ID)
		BsdfNode::compile(compiler, NULL, input("IOR"));
	else
		BsdfNode::compile(compiler, input("Roughness"), input("IOR"));
}

void GlassBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("distribution", distribution);
	compiler.add(this, "node_glass_bsdf");
}

/* Refraction BSDF Closure */

static ShaderEnum refraction_distribution_init()
{
	ShaderEnum enm;

	enm.insert("Sharp", CLOSURE_BSDF_REFRACTION_ID);
	enm.insert("Beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID);
	enm.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);

	return enm;
}

ShaderEnum RefractionBsdfNode::distribution_enum = refraction_distribution_init();

RefractionBsdfNode::RefractionBsdfNode()
{
	closure = CLOSURE_BSDF_REFRACTION_ID;
	distribution = ustring("Sharp");
	distribution_orig = ustring("");

	add_input("Roughness", SHADER_SOCKET_FLOAT, 0.0f);
	add_input("IOR", SHADER_SOCKET_FLOAT, 0.3f);
}

void RefractionBsdfNode::simplify_settings(Scene *scene)
{
	if(distribution_orig == "") {
		distribution_orig = distribution;
	}
	Integrator *integrator = scene->integrator;
	if(integrator->filter_glossy == 0.0f) {
		/* Fallback to Sharp closure for Roughness close to 0.
		 * Note: Keep the epsilon in sync with kernel!
		 */
		ShaderInput *roughness_input = input("Roughness");
		if(!roughness_input->link && roughness_input->value.x <= 1e-4f) {
			distribution = ustring("Sharp");
		}
	}
	else {
		/* Rollback to original distribution when filter glossy is used. */
		distribution = distribution_orig;
	}
	closure = (ClosureType)distribution_enum[distribution];
}

bool RefractionBsdfNode::has_integrator_dependency()
{
	ShaderInput *roughness_input = input("Roughness");
	return !roughness_input->link && roughness_input->value.x <= 1e-4f;
}

void RefractionBsdfNode::compile(SVMCompiler& compiler)
{
	closure = (ClosureType)distribution_enum[distribution];

	if(closure == CLOSURE_BSDF_REFRACTION_ID)
		BsdfNode::compile(compiler, NULL, input("IOR"));
	else
		BsdfNode::compile(compiler, input("Roughness"), input("IOR"));
}

void RefractionBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("distribution", distribution);
	compiler.add(this, "node_refraction_bsdf");
}

/* Toon BSDF Closure */

static ShaderEnum toon_component_init()
{
	ShaderEnum enm;

	enm.insert("Diffuse", CLOSURE_BSDF_DIFFUSE_TOON_ID);
	enm.insert("Glossy", CLOSURE_BSDF_GLOSSY_TOON_ID);

	return enm;
}

ShaderEnum ToonBsdfNode::component_enum = toon_component_init();

ToonBsdfNode::ToonBsdfNode()
{
	closure = CLOSURE_BSDF_DIFFUSE_TOON_ID;
	component = ustring("Diffuse");

	add_input("Size", SHADER_SOCKET_FLOAT, 0.5f);
	add_input("Smooth", SHADER_SOCKET_FLOAT, 0.0f);
}

void ToonBsdfNode::compile(SVMCompiler& compiler)
{
	closure = (ClosureType)component_enum[component];
	
	BsdfNode::compile(compiler, input("Size"), input("Smooth"));
}

void ToonBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("component", component);
	compiler.add(this, "node_toon_bsdf");
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

/* Subsurface Scattering Closure */

static ShaderEnum subsurface_falloff_init()
{
	ShaderEnum enm;

	enm.insert("Cubic", CLOSURE_BSSRDF_CUBIC_ID);
	enm.insert("Gaussian", CLOSURE_BSSRDF_GAUSSIAN_ID);
	enm.insert("Burley", CLOSURE_BSSRDF_BURLEY_ID);

	return enm;
}

ShaderEnum SubsurfaceScatteringNode::falloff_enum = subsurface_falloff_init();

SubsurfaceScatteringNode::SubsurfaceScatteringNode()
: BsdfNode(true)
{
	name = "subsurface_scattering";
	closure = CLOSURE_BSSRDF_CUBIC_ID;

	add_input("Scale", SHADER_SOCKET_FLOAT, 0.01f);
	add_input("Radius", SHADER_SOCKET_VECTOR, make_float3(0.1f, 0.1f, 0.1f));
	add_input("Sharpness", SHADER_SOCKET_FLOAT, 0.0f);
	add_input("Texture Blur", SHADER_SOCKET_FLOAT, 1.0f);
}

void SubsurfaceScatteringNode::compile(SVMCompiler& compiler)
{
	BsdfNode::compile(compiler, input("Scale"), input("Texture Blur"), input("Radius"), input("Sharpness"));
}

void SubsurfaceScatteringNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("Falloff", falloff_enum[closure]);
	compiler.add(this, "node_subsurface_scattering");
}

bool SubsurfaceScatteringNode::has_bssrdf_bump()
{
	/* detect if anything is plugged into the normal input besides the default */
	ShaderInput *normal_in = input("Normal");
	return (normal_in->link && normal_in->link->parent->special_type != SHADER_SPECIAL_TYPE_GEOMETRY);
}

/* Emissive Closure */

EmissionNode::EmissionNode()
: ShaderNode("emission")
{
	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("Strength", SHADER_SOCKET_FLOAT, 10.0f);
	add_input("SurfaceMixWeight", SHADER_SOCKET_FLOAT, 0.0f, ShaderInput::USE_SVM);

	add_output("Emission", SHADER_SOCKET_CLOSURE);
}

void EmissionNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *strength_in = input("Strength");

	if(color_in->link || strength_in->link) {
		compiler.add_node(NODE_EMISSION_WEIGHT,
		                  compiler.stack_assign(color_in),
						  compiler.stack_assign(strength_in));
	}
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value * strength_in->value.x);

	compiler.add_node(NODE_CLOSURE_EMISSION, compiler.closure_mix_weight_offset());
}

void EmissionNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_emission");
}

bool EmissionNode::constant_fold(ShaderGraph * /*graph*/, ShaderOutput * /*socket*/, float3 * /*optimized_value*/)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *strength_in = input("Strength");

	return ((!color_in->link && color_in->value == make_float3(0.0f, 0.0f, 0.0f)) ||
	        (!strength_in->link && strength_in->value.x == 0.0f));
}

/* Background Closure */

BackgroundNode::BackgroundNode()
: ShaderNode("background")
{
	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("Strength", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("SurfaceMixWeight", SHADER_SOCKET_FLOAT, 0.0f, ShaderInput::USE_SVM);

	add_output("Background", SHADER_SOCKET_CLOSURE);
}

void BackgroundNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *strength_in = input("Strength");

	if(color_in->link || strength_in->link) {
		compiler.add_node(NODE_EMISSION_WEIGHT,
		                  compiler.stack_assign(color_in),
						  compiler.stack_assign(strength_in));
	}
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value*strength_in->value.x);

	compiler.add_node(NODE_CLOSURE_BACKGROUND, compiler.closure_mix_weight_offset());
}

void BackgroundNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_background");
}

bool BackgroundNode::constant_fold(ShaderGraph * /*graph*/, ShaderOutput * /*socket*/, float3 * /*optimized_value*/)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *strength_in = input("Strength");

	return ((!color_in->link && color_in->value == make_float3(0.0f, 0.0f, 0.0f)) ||
	        (!strength_in->link && strength_in->value.x == 0.0f));
}

/* Holdout Closure */

HoldoutNode::HoldoutNode()
: ShaderNode("holdout")
{
	add_input("SurfaceMixWeight", SHADER_SOCKET_FLOAT, 0.0f, ShaderInput::USE_SVM);
	add_input("VolumeMixWeight", SHADER_SOCKET_FLOAT, 0.0f, ShaderInput::USE_SVM);

	add_output("Holdout", SHADER_SOCKET_CLOSURE);
}

void HoldoutNode::compile(SVMCompiler& compiler)
{
	float3 value = make_float3(1.0f, 1.0f, 1.0f);

	compiler.add_node(NODE_CLOSURE_SET_WEIGHT, value);
	compiler.add_node(NODE_CLOSURE_HOLDOUT, compiler.closure_mix_weight_offset());
}

void HoldoutNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_holdout");
}

/* Ambient Occlusion */

AmbientOcclusionNode::AmbientOcclusionNode()
: ShaderNode("ambient_occlusion")
{
	add_input("NormalIn", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, ShaderInput::USE_OSL);
	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("SurfaceMixWeight", SHADER_SOCKET_FLOAT, 0.0f, ShaderInput::USE_SVM);

	add_output("AO", SHADER_SOCKET_CLOSURE);
}

void AmbientOcclusionNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");

	if(color_in->link)
		compiler.add_node(NODE_CLOSURE_WEIGHT, compiler.stack_assign(color_in));
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value);

	compiler.add_node(NODE_CLOSURE_AMBIENT_OCCLUSION, compiler.closure_mix_weight_offset());
}

void AmbientOcclusionNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_ambient_occlusion");
}

/* Volume Closure */

VolumeNode::VolumeNode()
: ShaderNode("volume")
{
	closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;

	add_input("Color", SHADER_SOCKET_COLOR, make_float3(0.8f, 0.8f, 0.8f));
	add_input("Density", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("VolumeMixWeight", SHADER_SOCKET_FLOAT, 0.0f, ShaderInput::USE_SVM);

	add_output("Volume", SHADER_SOCKET_CLOSURE);
}

void VolumeNode::compile(SVMCompiler& compiler, ShaderInput *param1, ShaderInput *param2)
{
	ShaderInput *color_in = input("Color");

	if(color_in->link)
		compiler.add_node(NODE_CLOSURE_WEIGHT, compiler.stack_assign(color_in));
	else
		compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color_in->value);
	
	compiler.add_node(NODE_CLOSURE_VOLUME,
		compiler.encode_uchar4(closure,
			(param1)? compiler.stack_assign(param1): SVM_STACK_INVALID,
			(param2)? compiler.stack_assign(param2): SVM_STACK_INVALID,
			compiler.closure_mix_weight_offset()),
		__float_as_int((param1)? param1->value.x: 0.0f),
		__float_as_int((param2)? param2->value.x: 0.0f));
}

void VolumeNode::compile(SVMCompiler& compiler)
{
	compile(compiler, NULL, NULL);
}

void VolumeNode::compile(OSLCompiler& /*compiler*/)
{
	assert(0);
}

/* Absorption Volume Closure */

AbsorptionVolumeNode::AbsorptionVolumeNode()
{
	closure = CLOSURE_VOLUME_ABSORPTION_ID;
}

void AbsorptionVolumeNode::compile(SVMCompiler& compiler)
{
	VolumeNode::compile(compiler, input("Density"), NULL);
}

void AbsorptionVolumeNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_absorption_volume");
}

/* Scatter Volume Closure */

ScatterVolumeNode::ScatterVolumeNode()
{
	closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
	
	add_input("Anisotropy", SHADER_SOCKET_FLOAT, 0.0f);
}

void ScatterVolumeNode::compile(SVMCompiler& compiler)
{
	VolumeNode::compile(compiler, input("Density"), input("Anisotropy"));
}

void ScatterVolumeNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_scatter_volume");
}

/* Hair BSDF Closure */

static ShaderEnum hair_component_init()
{
	ShaderEnum enm;

	enm.insert("Reflection", CLOSURE_BSDF_HAIR_REFLECTION_ID);
	enm.insert("Transmission", CLOSURE_BSDF_HAIR_TRANSMISSION_ID);

	return enm;
}

ShaderEnum HairBsdfNode::component_enum = hair_component_init();

HairBsdfNode::HairBsdfNode()
{
	closure = CLOSURE_BSDF_HAIR_REFLECTION_ID;
	component = ustring("Reflection");

	add_input("Offset", SHADER_SOCKET_FLOAT);
	add_input("RoughnessU", SHADER_SOCKET_FLOAT);
	add_input("RoughnessV", SHADER_SOCKET_FLOAT);
	add_input("Tangent", SHADER_SOCKET_VECTOR);
}

void HairBsdfNode::compile(SVMCompiler& compiler)
{
	closure = (ClosureType)component_enum[component];

	BsdfNode::compile(compiler, input("RoughnessU"), input("RoughnessV"), input("Offset"));
}

void HairBsdfNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("component", component);

	compiler.add(this, "node_hair_bsdf");
}

/* Geometry */

GeometryNode::GeometryNode()
: ShaderNode("geometry")
{
	special_type = SHADER_SPECIAL_TYPE_GEOMETRY;

	add_input("NormalIn", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, ShaderInput::USE_OSL);
	add_output("Position", SHADER_SOCKET_POINT);
	add_output("Normal", SHADER_SOCKET_NORMAL);
	add_output("Tangent", SHADER_SOCKET_NORMAL);
	add_output("True Normal", SHADER_SOCKET_NORMAL);
	add_output("Incoming", SHADER_SOCKET_VECTOR);
	add_output("Parametric", SHADER_SOCKET_POINT);
	add_output("Backfacing", SHADER_SOCKET_FLOAT);
	add_output("Pointiness", SHADER_SOCKET_FLOAT);
}

void GeometryNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	if(shader->has_surface) {
		if(!output("Tangent")->links.empty()) {
			attributes->add(ATTR_STD_GENERATED);
		}
		if(!output("Pointiness")->links.empty()) {
			attributes->add(ATTR_STD_POINTINESS);
		}
	}

	ShaderNode::attributes(shader, attributes);
}

void GeometryNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;
	NodeType geom_node = NODE_GEOMETRY;
	NodeType attr_node = NODE_ATTR;

	if(bump == SHADER_BUMP_DX) {
		geom_node = NODE_GEOMETRY_BUMP_DX;
		attr_node = NODE_ATTR_BUMP_DX;
	}
	else if(bump == SHADER_BUMP_DY) {
		geom_node = NODE_GEOMETRY_BUMP_DY;
		attr_node = NODE_ATTR_BUMP_DY;
	}
	
	out = output("Position");
	if(!out->links.empty()) {
		compiler.add_node(geom_node, NODE_GEOM_P, compiler.stack_assign(out));
	}

	out = output("Normal");
	if(!out->links.empty()) {
		compiler.add_node(geom_node, NODE_GEOM_N, compiler.stack_assign(out));
	}

	out = output("Tangent");
	if(!out->links.empty()) {
		compiler.add_node(geom_node, NODE_GEOM_T, compiler.stack_assign(out));
	}

	out = output("True Normal");
	if(!out->links.empty()) {
		compiler.add_node(geom_node, NODE_GEOM_Ng, compiler.stack_assign(out));
	}

	out = output("Incoming");
	if(!out->links.empty()) {
		compiler.add_node(geom_node, NODE_GEOM_I, compiler.stack_assign(out));
	}

	out = output("Parametric");
	if(!out->links.empty()) {
		compiler.add_node(geom_node, NODE_GEOM_uv, compiler.stack_assign(out));
	}

	out = output("Backfacing");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_backfacing, compiler.stack_assign(out));
	}

	out = output("Pointiness");
	if(!out->links.empty()) {
		if(compiler.output_type() != SHADER_TYPE_VOLUME) {
			compiler.add_node(attr_node,
			                  ATTR_STD_POINTINESS,
			                  compiler.stack_assign(out),
			                  NODE_ATTR_FLOAT);
		}
		else {
			compiler.add_node(NODE_VALUE_F, __float_as_int(0.0f), compiler.stack_assign(out));
		}
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
	add_input("NormalIn", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, ShaderInput::USE_OSL);
	add_output("Generated", SHADER_SOCKET_POINT);
	add_output("Normal", SHADER_SOCKET_NORMAL);
	add_output("UV", SHADER_SOCKET_POINT);
	add_output("Object", SHADER_SOCKET_POINT);
	add_output("Camera", SHADER_SOCKET_POINT);
	add_output("Window", SHADER_SOCKET_POINT);
	add_output("Reflection", SHADER_SOCKET_NORMAL);

	from_dupli = false;
	use_transform = false;
	ob_tfm = transform_identity();
}

void TextureCoordinateNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	if(shader->has_surface) {
		if(!from_dupli) {
			if(!output("Generated")->links.empty())
				attributes->add(ATTR_STD_GENERATED);
			if(!output("UV")->links.empty())
				attributes->add(ATTR_STD_UV);
		}
	}

	if(shader->has_volume) {
		if(!from_dupli) {
			if(!output("Generated")->links.empty()) {
				attributes->add(ATTR_STD_GENERATED_TRANSFORM);
			}
		}
	}

	ShaderNode::attributes(shader, attributes);
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
			compiler.add_node(geom_node, NODE_GEOM_P, compiler.stack_assign(out));
		}
		else {
			if(from_dupli) {
				compiler.add_node(texco_node, NODE_TEXCO_DUPLI_GENERATED, compiler.stack_assign(out));
			}
			else if(compiler.output_type() == SHADER_TYPE_VOLUME) {
				compiler.add_node(texco_node, NODE_TEXCO_VOLUME_GENERATED, compiler.stack_assign(out));
			}
			else {
				int attr = compiler.attribute(ATTR_STD_GENERATED);
				compiler.add_node(attr_node, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT3);
			}
		}
	}

	out = output("Normal");
	if(!out->links.empty()) {
		compiler.add_node(texco_node, NODE_TEXCO_NORMAL, compiler.stack_assign(out));
	}

	out = output("UV");
	if(!out->links.empty()) {
		if(from_dupli) {
			compiler.add_node(texco_node, NODE_TEXCO_DUPLI_UV, compiler.stack_assign(out));
		}
		else {
			int attr = compiler.attribute(ATTR_STD_UV);
			compiler.add_node(attr_node, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT3);
		}
	}

	out = output("Object");
	if(!out->links.empty()) {
		compiler.add_node(texco_node, NODE_TEXCO_OBJECT, compiler.stack_assign(out), use_transform);
		if(use_transform) {
			Transform ob_itfm = transform_inverse(ob_tfm);
			compiler.add_node(ob_itfm.x);
			compiler.add_node(ob_itfm.y);
			compiler.add_node(ob_itfm.z);
			compiler.add_node(ob_itfm.w);
		}
	}

	out = output("Camera");
	if(!out->links.empty()) {
		compiler.add_node(texco_node, NODE_TEXCO_CAMERA, compiler.stack_assign(out));
	}

	out = output("Window");
	if(!out->links.empty()) {
		compiler.add_node(texco_node, NODE_TEXCO_WINDOW, compiler.stack_assign(out));
	}

	out = output("Reflection");
	if(!out->links.empty()) {
		if(compiler.background) {
			compiler.add_node(geom_node, NODE_GEOM_I, compiler.stack_assign(out));
		}
		else {
			compiler.add_node(texco_node, NODE_TEXCO_REFLECTION, compiler.stack_assign(out));
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
	if(compiler.output_type() == SHADER_TYPE_VOLUME)
		compiler.parameter("is_volume", true);
	compiler.parameter("use_transform", use_transform);
	Transform ob_itfm = transform_transpose(transform_inverse(ob_tfm));
	compiler.parameter("object_itfm", ob_itfm);

	compiler.parameter("from_dupli", from_dupli);

	compiler.add(this, "node_texture_coordinate");
}

UVMapNode::UVMapNode()
: ShaderNode("uvmap")
{
	attribute = "";
	from_dupli = false;

	add_output("UV", SHADER_SOCKET_POINT);
}

void UVMapNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	if(shader->has_surface) {
		if(!from_dupli) {
			if(!output("UV")->links.empty()) {
				if(attribute != "")
					attributes->add(attribute);
				else
					attributes->add(ATTR_STD_UV);
			}
		}
	}

	ShaderNode::attributes(shader, attributes);
}

void UVMapNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out = output("UV");
	NodeType texco_node = NODE_TEX_COORD;
	NodeType attr_node = NODE_ATTR;
	int attr;

	if(bump == SHADER_BUMP_DX) {
		texco_node = NODE_TEX_COORD_BUMP_DX;
		attr_node = NODE_ATTR_BUMP_DX;
	}
	else if(bump == SHADER_BUMP_DY) {
		texco_node = NODE_TEX_COORD_BUMP_DY;
		attr_node = NODE_ATTR_BUMP_DY;
	}

	if(!out->links.empty()) {
		if(from_dupli) {
			compiler.add_node(texco_node, NODE_TEXCO_DUPLI_UV, compiler.stack_assign(out));
		}
		else {
			if(attribute != "")
				attr = compiler.attribute(attribute);
			else
				attr = compiler.attribute(ATTR_STD_UV);

			compiler.add_node(attr_node, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT3);
		}
	}
}

void UVMapNode::compile(OSLCompiler& compiler)
{
	if(bump == SHADER_BUMP_DX)
		compiler.parameter("bump_offset", "dx");
	else if(bump == SHADER_BUMP_DY)
		compiler.parameter("bump_offset", "dy");
	else
		compiler.parameter("bump_offset", "center");

	compiler.parameter("from_dupli", from_dupli);
	compiler.parameter("name", attribute.c_str());
	compiler.add(this, "node_uv_map");
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
	add_output("Is Volume Scatter Ray", SHADER_SOCKET_FLOAT);
	add_output("Ray Length", SHADER_SOCKET_FLOAT);
	add_output("Ray Depth", SHADER_SOCKET_FLOAT);
	add_output("Transparent Depth", SHADER_SOCKET_FLOAT);
	add_output("Transmission Depth", SHADER_SOCKET_FLOAT);
}

void LightPathNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;

	out = output("Is Camera Ray");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_camera, compiler.stack_assign(out));
	}

	out = output("Is Shadow Ray");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_shadow, compiler.stack_assign(out));
	}

	out = output("Is Diffuse Ray");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_diffuse, compiler.stack_assign(out));
	}

	out = output("Is Glossy Ray");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_glossy, compiler.stack_assign(out));
	}

	out = output("Is Singular Ray");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_singular, compiler.stack_assign(out));
	}

	out = output("Is Reflection Ray");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_reflection, compiler.stack_assign(out));
	}


	out = output("Is Transmission Ray");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_transmission, compiler.stack_assign(out));
	}
	
	out = output("Is Volume Scatter Ray");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_volume_scatter, compiler.stack_assign(out));
	}

	out = output("Ray Length");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_length, compiler.stack_assign(out));
	}
	
	out = output("Ray Depth");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_depth, compiler.stack_assign(out));
	}

	out = output("Transparent Depth");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_transparent, compiler.stack_assign(out));
	}

	out = output("Transmission Depth");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_transmission, compiler.stack_assign(out));
	}
}

void LightPathNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_light_path");
}

/* Light Falloff */

LightFalloffNode::LightFalloffNode()
: ShaderNode("light_fallof")
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

	ShaderOutput *out = output("Quadratic");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_FALLOFF, NODE_LIGHT_FALLOFF_QUADRATIC,
			compiler.encode_uchar4(
				compiler.stack_assign(strength_in),
				compiler.stack_assign(smooth_in),
				compiler.stack_assign(out)));
	}

	out = output("Linear");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_FALLOFF, NODE_LIGHT_FALLOFF_LINEAR,
			compiler.encode_uchar4(
				compiler.stack_assign(strength_in),
				compiler.stack_assign(smooth_in),
				compiler.stack_assign(out)));
	}

	out = output("Constant");
	if(!out->links.empty()) {
		compiler.add_node(NODE_LIGHT_FALLOFF, NODE_LIGHT_FALLOFF_CONSTANT,
			compiler.encode_uchar4(
				compiler.stack_assign(strength_in),
				compiler.stack_assign(smooth_in),
				compiler.stack_assign(out)));
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
		compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_LOCATION, compiler.stack_assign(out));
	}

	out = output("Object Index");
	if(!out->links.empty()) {
		compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_INDEX, compiler.stack_assign(out));
	}

	out = output("Material Index");
	if(!out->links.empty()) {
		compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_MAT_INDEX, compiler.stack_assign(out));
	}

	out = output("Random");
	if(!out->links.empty()) {
		compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_RANDOM, compiler.stack_assign(out));
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
	add_output("Index", SHADER_SOCKET_FLOAT);
	add_output("Age", SHADER_SOCKET_FLOAT);
	add_output("Lifetime", SHADER_SOCKET_FLOAT);
	add_output("Location", SHADER_SOCKET_POINT);
#if 0	/* not yet supported */
	add_output("Rotation", SHADER_SOCKET_QUATERNION);
#endif
	add_output("Size", SHADER_SOCKET_FLOAT);
	add_output("Velocity", SHADER_SOCKET_VECTOR);
	add_output("Angular Velocity", SHADER_SOCKET_VECTOR);
}

void ParticleInfoNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	if(!output("Index")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);
	if(!output("Age")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);
	if(!output("Lifetime")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);
	if(!output("Location")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);
#if 0	/* not yet supported */
	if(!output("Rotation")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);
#endif
	if(!output("Size")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);
	if(!output("Velocity")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);
	if(!output("Angular Velocity")->links.empty())
		attributes->add(ATTR_STD_PARTICLE);

	ShaderNode::attributes(shader, attributes);
}

void ParticleInfoNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;
	
	out = output("Index");
	if(!out->links.empty()) {
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_INDEX, compiler.stack_assign(out));
	}
	
	out = output("Age");
	if(!out->links.empty()) {
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_AGE, compiler.stack_assign(out));
	}
	
	out = output("Lifetime");
	if(!out->links.empty()) {
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_LIFETIME, compiler.stack_assign(out));
	}
	
	out = output("Location");
	if(!out->links.empty()) {
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_LOCATION, compiler.stack_assign(out));
	}
	
	/* quaternion data is not yet supported by Cycles */
#if 0
	out = output("Rotation");
	if(!out->links.empty()) {
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_ROTATION, compiler.stack_assign(out));
	}
#endif
	
	out = output("Size");
	if(!out->links.empty()) {
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_SIZE, compiler.stack_assign(out));
	}
	
	out = output("Velocity");
	if(!out->links.empty()) {
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_VELOCITY, compiler.stack_assign(out));
	}
	
	out = output("Angular Velocity");
	if(!out->links.empty()) {
		compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_ANGULAR_VELOCITY, compiler.stack_assign(out));
	}
}

void ParticleInfoNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_particle_info");
}

/* Hair Info */

HairInfoNode::HairInfoNode()
: ShaderNode("hair_info")
{
	add_output("Is Strand", SHADER_SOCKET_FLOAT);
	add_output("Intercept", SHADER_SOCKET_FLOAT);
	add_output("Thickness", SHADER_SOCKET_FLOAT);
	add_output("Tangent Normal", SHADER_SOCKET_NORMAL);
	/*output for minimum hair width transparency - deactivated*/
	/*add_output("Fade", SHADER_SOCKET_FLOAT);*/
}

void HairInfoNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	if(shader->has_surface) {
		ShaderOutput *intercept_out = output("Intercept");

		if(!intercept_out->links.empty())
			attributes->add(ATTR_STD_CURVE_INTERCEPT);
	}

	ShaderNode::attributes(shader, attributes);
}

void HairInfoNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *out;
	
	out = output("Is Strand");
	if(!out->links.empty()) {
		compiler.add_node(NODE_HAIR_INFO, NODE_INFO_CURVE_IS_STRAND, compiler.stack_assign(out));
	}

	out = output("Intercept");
	if(!out->links.empty()) {
		int attr = compiler.attribute(ATTR_STD_CURVE_INTERCEPT);
		compiler.add_node(NODE_ATTR, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT);
	}

	out = output("Thickness");
	if(!out->links.empty()) {
		compiler.add_node(NODE_HAIR_INFO, NODE_INFO_CURVE_THICKNESS, compiler.stack_assign(out));
	}

	out = output("Tangent Normal");
	if(!out->links.empty()) {
		compiler.add_node(NODE_HAIR_INFO, NODE_INFO_CURVE_TANGENT_NORMAL, compiler.stack_assign(out));
	}

	/*out = output("Fade");
	if(!out->links.empty()) {
		compiler.add_node(NODE_HAIR_INFO, NODE_INFO_CURVE_FADE, compiler.stack_assign(out));
	}*/

}

void HairInfoNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_hair_info");
}

/* Value */

ValueNode::ValueNode()
: ShaderNode("value")
{
	value = 0.0f;

	add_output("Value", SHADER_SOCKET_FLOAT);
}

bool ValueNode::constant_fold(ShaderGraph * /*graph*/, ShaderOutput * /*socket*/,
                              float3 *optimized_value)
{
	*optimized_value = make_float3(value, value, value);
	return true;
}

void ValueNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *val_out = output("Value");

	compiler.add_node(NODE_VALUE_F, __float_as_int(value), compiler.stack_assign(val_out));
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

bool ColorNode::constant_fold(ShaderGraph * /*graph*/, ShaderOutput * /*socket*/,
                              float3 *optimized_value)
{
	*optimized_value = value;
	return true;
}

void ColorNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *color_out = output("Color");

	if(!color_out->links.empty()) {
		compiler.add_node(NODE_VALUE_V, compiler.stack_assign(color_out));
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
	special_type = SHADER_SPECIAL_TYPE_COMBINE_CLOSURE;

	add_input("Closure1", SHADER_SOCKET_CLOSURE);
	add_input("Closure2", SHADER_SOCKET_CLOSURE);
	add_output("Closure",  SHADER_SOCKET_CLOSURE);
}

void AddClosureNode::compile(SVMCompiler& /*compiler*/)
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
	special_type = SHADER_SPECIAL_TYPE_COMBINE_CLOSURE;

	add_input("Fac", SHADER_SOCKET_FLOAT, 0.5f);
	add_input("Closure1", SHADER_SOCKET_CLOSURE);
	add_input("Closure2", SHADER_SOCKET_CLOSURE);
	add_output("Closure",  SHADER_SOCKET_CLOSURE);
}

void MixClosureNode::compile(SVMCompiler& /*compiler*/)
{
	/* handled in the SVM compiler */
}

void MixClosureNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_mix_closure");
}

bool MixClosureNode::constant_fold(ShaderGraph *graph, ShaderOutput * /*socket*/, float3 * /*optimized_value*/)
{
	ShaderInput *fac_in = input("Fac");
	ShaderInput *closure1_in = input("Closure1");
	ShaderInput *closure2_in = input("Closure2");
	ShaderOutput *closure_out = output("Closure");

	/* remove useless mix closures nodes */
	if(closure1_in->link == closure2_in->link) {
		graph->relink(this, closure_out, closure1_in->link);
		return true;
	}

	/* remove unused mix closure input when factor is 0.0 or 1.0 */
	/* check for closure links and make sure factor link is disconnected */
	if(closure1_in->link && closure2_in->link && !fac_in->link) {
		/* factor 0.0 */
		if(fac_in->value.x == 0.0f) {
			graph->relink(this, closure_out, closure1_in->link);
			return true;
		}
		/* factor 1.0 */
		else if(fac_in->value.x == 1.0f) {
			graph->relink(this, closure_out, closure2_in->link);
			return true;
		}
	}

	return false;
}

/* Mix Closure */

MixClosureWeightNode::MixClosureWeightNode()
: ShaderNode("mix_closure_weight")
{
	add_input("Weight", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Fac", SHADER_SOCKET_FLOAT, 1.0f);
	add_output("Weight1", SHADER_SOCKET_FLOAT);
	add_output("Weight2", SHADER_SOCKET_FLOAT);
}

void MixClosureWeightNode::compile(SVMCompiler& compiler)
{
	ShaderInput *weight_in = input("Weight");
	ShaderInput *fac_in = input("Fac");
	ShaderOutput *weight1_out = output("Weight1");
	ShaderOutput *weight2_out = output("Weight2");

	compiler.add_node(NODE_MIX_CLOSURE,
		compiler.encode_uchar4(
			compiler.stack_assign(fac_in),
			compiler.stack_assign(weight_in),
			compiler.stack_assign(weight1_out),
			compiler.stack_assign(weight2_out)));
}

void MixClosureWeightNode::compile(OSLCompiler& /*compiler*/)
{
	assert(0);
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

	compiler.add_node(NODE_INVERT,
		compiler.stack_assign(fac_in),
		compiler.stack_assign(color_in),
		compiler.stack_assign(color_out));
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

	use_clamp = false;

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
	enm.insert("Value", NODE_MIX_VAL);
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

	compiler.add_node(NODE_MIX,
		compiler.stack_assign(fac_in),
		compiler.stack_assign(color1_in),
		compiler.stack_assign(color2_in));
	compiler.add_node(NODE_MIX, type_enum[type], compiler.stack_assign(color_out));

	if(use_clamp) {
		compiler.add_node(NODE_MIX, 0, compiler.stack_assign(color_out));
		compiler.add_node(NODE_MIX, NODE_MIX_CLAMP, compiler.stack_assign(color_out));
	}
}

void MixNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("type", type);
	compiler.parameter("Clamp", use_clamp);
	compiler.add(this, "node_mix");
}

bool MixNode::constant_fold(ShaderGraph *graph, ShaderOutput * /*socket*/, float3 * optimized_value)
{
	if(type != ustring("Mix")) {
		return false;
	}

	ShaderInput *fac_in = input("Fac");
	ShaderInput *color1_in = input("Color1");
	ShaderInput *color2_in = input("Color2");
	ShaderOutput *color_out = output("Color");

	/* remove useless mix colors nodes */
	if(color1_in->link && color1_in->link == color2_in->link) {
		graph->relink(this, color_out, color1_in->link);
		return true;
	}

	/* remove unused mix color input when factor is 0.0 or 1.0 */
	if(!fac_in->link) {
		/* factor 0.0 */
		if(fac_in->value.x == 0.0f) {
			if (color1_in->link)
				graph->relink(this, color_out, color1_in->link);
			else
				*optimized_value = color1_in->value;
			return true;
		}
		/* factor 1.0 */
		else if(fac_in->value.x == 1.0f) {
			if (color2_in->link)
				graph->relink(this, color_out, color2_in->link);
			else
				*optimized_value = color2_in->value;
			return true;
		}
	}

	return false;
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

	compiler.add_node(NODE_COMBINE_VECTOR,
		compiler.stack_assign(red_in), 0,
		compiler.stack_assign(color_out));

	compiler.add_node(NODE_COMBINE_VECTOR,
		compiler.stack_assign(green_in), 1,
		compiler.stack_assign(color_out));

	compiler.add_node(NODE_COMBINE_VECTOR,
		compiler.stack_assign(blue_in), 2,
		compiler.stack_assign(color_out));
}

void CombineRGBNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_combine_rgb");
}

/* Combine XYZ */
CombineXYZNode::CombineXYZNode()
: ShaderNode("combine_xyz")
{
	add_input("X", SHADER_SOCKET_FLOAT);
	add_input("Y", SHADER_SOCKET_FLOAT);
	add_input("Z", SHADER_SOCKET_FLOAT);
	add_output("Vector", SHADER_SOCKET_VECTOR);
}

void CombineXYZNode::compile(SVMCompiler& compiler)
{
	ShaderInput *x_in = input("X");
	ShaderInput *y_in = input("Y");
	ShaderInput *z_in = input("Z");
	ShaderOutput *vector_out = output("Vector");

	compiler.add_node(NODE_COMBINE_VECTOR,
		compiler.stack_assign(x_in), 0,
		compiler.stack_assign(vector_out));

	compiler.add_node(NODE_COMBINE_VECTOR,
		compiler.stack_assign(y_in), 1,
		compiler.stack_assign(vector_out));

	compiler.add_node(NODE_COMBINE_VECTOR,
		compiler.stack_assign(z_in), 2,
		compiler.stack_assign(vector_out));
}

void CombineXYZNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_combine_xyz");
}

/* Combine HSV */
CombineHSVNode::CombineHSVNode()
: ShaderNode("combine_hsv")
{
	add_input("H", SHADER_SOCKET_FLOAT);
	add_input("S", SHADER_SOCKET_FLOAT);
	add_input("V", SHADER_SOCKET_FLOAT);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void CombineHSVNode::compile(SVMCompiler& compiler)
{
	ShaderInput *hue_in = input("H");
	ShaderInput *saturation_in = input("S");
	ShaderInput *value_in = input("V");
	ShaderOutput *color_out = output("Color");

	compiler.add_node(NODE_COMBINE_HSV,
		compiler.stack_assign(hue_in),
		compiler.stack_assign(saturation_in),
		compiler.stack_assign(value_in));
	compiler.add_node(NODE_COMBINE_HSV,
		compiler.stack_assign(color_out));
}

void CombineHSVNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_combine_hsv");
}

/* Gamma */
GammaNode::GammaNode()
: ShaderNode("gamma")
{
	add_input("Color", SHADER_SOCKET_COLOR);
	add_input("Gamma", SHADER_SOCKET_FLOAT);
	add_output("Color", SHADER_SOCKET_COLOR);
}

bool GammaNode::constant_fold(ShaderGraph * /*graph*/, ShaderOutput *socket, float3 *optimized_value)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *gamma_in = input("Gamma");

	if(socket == output("Color")) {
		if(color_in->link == NULL && gamma_in->link == NULL) {
			*optimized_value = svm_math_gamma_color(color_in->value,
			                                        gamma_in->value.x);
			return true;
		}
	}

	return false;
}

void GammaNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");
	ShaderInput *gamma_in = input("Gamma");
	ShaderOutput *color_out = output("Color");

	compiler.add_node(NODE_GAMMA,
		compiler.stack_assign(gamma_in),
		compiler.stack_assign(color_in),
		compiler.stack_assign(color_out));
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

	compiler.add_node(NODE_BRIGHTCONTRAST,
		compiler.stack_assign(color_in),
		compiler.stack_assign(color_out),
		compiler.encode_uchar4(
			compiler.stack_assign(bright_in),
			compiler.stack_assign(contrast_in)));
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

	compiler.add_node(NODE_SEPARATE_VECTOR,
		compiler.stack_assign(color_in), 0,
		compiler.stack_assign(red_out));

	compiler.add_node(NODE_SEPARATE_VECTOR,
		compiler.stack_assign(color_in), 1,
		compiler.stack_assign(green_out));

	compiler.add_node(NODE_SEPARATE_VECTOR,
		compiler.stack_assign(color_in), 2,
		compiler.stack_assign(blue_out));
}

void SeparateRGBNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_separate_rgb");
}

/* Separate XYZ */
SeparateXYZNode::SeparateXYZNode()
: ShaderNode("separate_xyz")
{
	add_input("Vector", SHADER_SOCKET_VECTOR);
	add_output("X", SHADER_SOCKET_FLOAT);
	add_output("Y", SHADER_SOCKET_FLOAT);
	add_output("Z", SHADER_SOCKET_FLOAT);
}

void SeparateXYZNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *x_out = output("X");
	ShaderOutput *y_out = output("Y");
	ShaderOutput *z_out = output("Z");

	compiler.add_node(NODE_SEPARATE_VECTOR,
		compiler.stack_assign(vector_in), 0,
		compiler.stack_assign(x_out));

	compiler.add_node(NODE_SEPARATE_VECTOR,
		compiler.stack_assign(vector_in), 1,
		compiler.stack_assign(y_out));

	compiler.add_node(NODE_SEPARATE_VECTOR,
		compiler.stack_assign(vector_in), 2,
		compiler.stack_assign(z_out));
}

void SeparateXYZNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_separate_xyz");
}

/* Separate HSV */
SeparateHSVNode::SeparateHSVNode()
: ShaderNode("separate_hsv")
{
	add_input("Color", SHADER_SOCKET_COLOR);
	add_output("H", SHADER_SOCKET_FLOAT);
	add_output("S", SHADER_SOCKET_FLOAT);
	add_output("V", SHADER_SOCKET_FLOAT);
}

void SeparateHSVNode::compile(SVMCompiler& compiler)
{
	ShaderInput *color_in = input("Color");
	ShaderOutput *hue_out = output("H");
	ShaderOutput *saturation_out = output("S");
	ShaderOutput *value_out = output("V");

	compiler.add_node(NODE_SEPARATE_HSV,
		compiler.stack_assign(color_in),
		compiler.stack_assign(hue_out),
		compiler.stack_assign(saturation_out));
	compiler.add_node(NODE_SEPARATE_HSV,
		compiler.stack_assign(value_out));
}

void SeparateHSVNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_separate_hsv");
}

/* Hue Saturation Value */
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

	compiler.add_node(NODE_HSV,
		compiler.encode_uchar4(
			compiler.stack_assign(color_in),
			compiler.stack_assign(fac_in),
			compiler.stack_assign(color_out)),
		compiler.encode_uchar4(
			compiler.stack_assign(hue_in),
			compiler.stack_assign(saturation_in),
			compiler.stack_assign(value_in)));
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

void AttributeNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	ShaderOutput *color_out = output("Color");
	ShaderOutput *vector_out = output("Vector");
	ShaderOutput *fac_out = output("Fac");

	if(!color_out->links.empty() || !vector_out->links.empty() || !fac_out->links.empty()) {
		AttributeStandard std = Attribute::name_standard(attribute.c_str());

		if(std != ATTR_STD_NONE)
			attributes->add(std);
		else
			attributes->add(attribute);
	}

	if(shader->has_volume)
		attributes->add(ATTR_STD_GENERATED_TRANSFORM);

	ShaderNode::attributes(shader, attributes);
}

void AttributeNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *color_out = output("Color");
	ShaderOutput *vector_out = output("Vector");
	ShaderOutput *fac_out = output("Fac");
	NodeType attr_node = NODE_ATTR;
	AttributeStandard std = Attribute::name_standard(attribute.c_str());
	int attr;

	if(std != ATTR_STD_NONE)
		attr = compiler.attribute(std);
	else
		attr = compiler.attribute(attribute);

	if(bump == SHADER_BUMP_DX)
		attr_node = NODE_ATTR_BUMP_DX;
	else if(bump == SHADER_BUMP_DY)
		attr_node = NODE_ATTR_BUMP_DY;

	if(!color_out->links.empty() || !vector_out->links.empty()) {
		if(!color_out->links.empty()) {
			compiler.add_node(attr_node, attr, compiler.stack_assign(color_out), NODE_ATTR_FLOAT3);
		}
		if(!vector_out->links.empty()) {
			compiler.add_node(attr_node, attr, compiler.stack_assign(vector_out), NODE_ATTR_FLOAT3);
		}
	}

	if(!fac_out->links.empty()) {
		compiler.add_node(attr_node, attr, compiler.stack_assign(fac_out), NODE_ATTR_FLOAT);
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
	
	if(Attribute::name_standard(attribute.c_str()) != ATTR_STD_NONE)
		compiler.parameter("name", (string("geom:") + attribute.c_str()).c_str());
	else
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

	compiler.add_node(NODE_CAMERA,
		compiler.stack_assign(vector_out),
		compiler.stack_assign(z_depth_out),
		compiler.stack_assign(distance_out));
}

void CameraNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_camera");
}

/* Fresnel */

FresnelNode::FresnelNode()
: ShaderNode("fresnel")
{
	add_input("Normal", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, ShaderInput::USE_OSL);
	add_input("IOR", SHADER_SOCKET_FLOAT, 1.45f);
	add_output("Fac", SHADER_SOCKET_FLOAT);
}

void FresnelNode::compile(SVMCompiler& compiler)
{
	ShaderInput *normal_in = input("Normal");
	ShaderInput *IOR_in = input("IOR");
	ShaderOutput *fac_out = output("Fac");

	compiler.add_node(NODE_FRESNEL,
		compiler.stack_assign(IOR_in),
		__float_as_int(IOR_in->value.x),
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(normal_in),
			compiler.stack_assign(fac_out)));
}

void FresnelNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_fresnel");
}

/* Layer Weight */

LayerWeightNode::LayerWeightNode()
: ShaderNode("layer_weight")
{
	add_input("Normal", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, ShaderInput::USE_OSL);
	add_input("Blend", SHADER_SOCKET_FLOAT, 0.5f);

	add_output("Fresnel", SHADER_SOCKET_FLOAT);
	add_output("Facing", SHADER_SOCKET_FLOAT);
}

void LayerWeightNode::compile(SVMCompiler& compiler)
{
	ShaderInput *normal_in = input("Normal");
	ShaderInput *blend_in = input("Blend");
	ShaderOutput *fresnel_out = output("Fresnel");
	ShaderOutput *facing_out = output("Facing");

	if(!fresnel_out->links.empty()) {
		compiler.add_node(NODE_LAYER_WEIGHT,
			compiler.stack_assign_if_linked(blend_in),
			__float_as_int(blend_in->value.x),
			compiler.encode_uchar4(NODE_LAYER_WEIGHT_FRESNEL,
			                       compiler.stack_assign_if_linked(normal_in),
			                       compiler.stack_assign(fresnel_out)));
	}

	if(!facing_out->links.empty()) {
		compiler.add_node(NODE_LAYER_WEIGHT,
			compiler.stack_assign_if_linked(blend_in),
			__float_as_int(blend_in->value.x),
			compiler.encode_uchar4(NODE_LAYER_WEIGHT_FACING,
			                       compiler.stack_assign_if_linked(normal_in),
			                       compiler.stack_assign(facing_out)));
	}
}

void LayerWeightNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_layer_weight");
}

/* Wireframe */

WireframeNode::WireframeNode()
: ShaderNode("wireframe")
{
	add_input("Size", SHADER_SOCKET_FLOAT, 0.01f);
	add_output("Fac", SHADER_SOCKET_FLOAT);
	
	use_pixel_size = false;
}

void WireframeNode::compile(SVMCompiler& compiler)
{
	ShaderInput *size_in = input("Size");
	ShaderOutput *fac_out = output("Fac");
	NodeBumpOffset bump_offset = NODE_BUMP_OFFSET_CENTER;
	if(bump == SHADER_BUMP_DX) {
		bump_offset = NODE_BUMP_OFFSET_DX;
	}
	else if(bump == SHADER_BUMP_DY) {
		bump_offset = NODE_BUMP_OFFSET_DY;
	}
	compiler.add_node(NODE_WIREFRAME,
	                  compiler.stack_assign(size_in),
	                  compiler.stack_assign(fac_out),
	                  compiler.encode_uchar4(use_pixel_size,
	                                         bump_offset,
	                                         0, 0));
}

void WireframeNode::compile(OSLCompiler& compiler)
{
	if(bump == SHADER_BUMP_DX) {
		compiler.parameter("bump_offset", "dx");
	}
	else if(bump == SHADER_BUMP_DY) {
		compiler.parameter("bump_offset", "dy");
	}
	else {
		compiler.parameter("bump_offset", "center");
	}
	compiler.parameter("use_pixel_size", use_pixel_size);
	compiler.add(this, "node_wireframe");
}

/* Wavelength */

WavelengthNode::WavelengthNode()
: ShaderNode("wavelength")
{
	add_input("Wavelength", SHADER_SOCKET_FLOAT, 500.0f);
	add_output("Color", SHADER_SOCKET_COLOR);
}

void WavelengthNode::compile(SVMCompiler& compiler)
{
	ShaderInput *wavelength_in = input("Wavelength");
	ShaderOutput *color_out = output("Color");

	compiler.add_node(NODE_WAVELENGTH,
		compiler.stack_assign(wavelength_in),
		compiler.stack_assign(color_out));
}

void WavelengthNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_wavelength");
}

/* Blackbody */

BlackbodyNode::BlackbodyNode()
: ShaderNode("blackbody")
{
	add_input("Temperature", SHADER_SOCKET_FLOAT, 1200.0f);
	add_output("Color", SHADER_SOCKET_COLOR);
}

bool BlackbodyNode::constant_fold(ShaderGraph * /*graph*/, ShaderOutput *socket, float3 *optimized_value)
{
	ShaderInput *temperature_in = input("Temperature");

	if(socket == output("Color")) {
		if(temperature_in->link == NULL) {
			*optimized_value = svm_math_blackbody_color(temperature_in->value.x);
			return true;
		}
	}

	return false;
}

void BlackbodyNode::compile(SVMCompiler& compiler)
{
	ShaderInput *temperature_in = input("Temperature");
	ShaderOutput *color_out = output("Color");

	compiler.add_node(NODE_BLACKBODY,
		compiler.stack_assign(temperature_in),
		compiler.stack_assign(color_out));
}

void BlackbodyNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_blackbody");
}

/* Output */

OutputNode::OutputNode()
: ShaderNode("output")
{
	special_type = SHADER_SPECIAL_TYPE_OUTPUT;

	add_input("Surface", SHADER_SOCKET_CLOSURE);
	add_input("Volume", SHADER_SOCKET_CLOSURE);
	add_input("Displacement", SHADER_SOCKET_FLOAT);
	add_input("Normal", SHADER_SOCKET_NORMAL);
}

void OutputNode::compile(SVMCompiler& compiler)
{
	if(compiler.output_type() == SHADER_TYPE_DISPLACEMENT) {
		ShaderInput *displacement_in = input("Displacement");

		if(displacement_in->link) {
			compiler.add_node(NODE_SET_DISPLACEMENT, compiler.stack_assign(displacement_in));
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

	use_clamp = false;

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
	enm.insert("Modulo", NODE_MATH_MODULO);
	enm.insert("Absolute", NODE_MATH_ABSOLUTE);

	return enm;
}

ShaderEnum MathNode::type_enum = math_type_init();

bool MathNode::constant_fold(ShaderGraph * /*graph*/, ShaderOutput *socket, float3 *optimized_value)
{
	ShaderInput *value1_in = input("Value1");
	ShaderInput *value2_in = input("Value2");

	if(socket == output("Value")) {
		if(value1_in->link == NULL && value2_in->link == NULL) {
			optimized_value->x = svm_math((NodeMath)type_enum[type],
			                              value1_in->value.x,
			                              value2_in->value.x);

			if(use_clamp) {
				optimized_value->x = saturate(optimized_value->x);
			}

			return true;
		}
	}

	return false;
}

void MathNode::compile(SVMCompiler& compiler)
{
	ShaderInput *value1_in = input("Value1");
	ShaderInput *value2_in = input("Value2");
	ShaderOutput *value_out = output("Value");

	compiler.add_node(NODE_MATH,
		type_enum[type],
		compiler.stack_assign(value1_in),
		compiler.stack_assign(value2_in));
	compiler.add_node(NODE_MATH, compiler.stack_assign(value_out));

	if(use_clamp) {
		compiler.add_node(NODE_MATH, NODE_MATH_CLAMP, compiler.stack_assign(value_out));
		compiler.add_node(NODE_MATH, compiler.stack_assign(value_out));
	}
}

void MathNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("type", type);
	compiler.parameter("Clamp", use_clamp);
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

bool VectorMathNode::constant_fold(ShaderGraph * /*graph*/, ShaderOutput *socket, float3 *optimized_value)
{
	ShaderInput *vector1_in = input("Vector1");
	ShaderInput *vector2_in = input("Vector2");

	float value;
	float3 vector;

	if(vector1_in->link == NULL && vector2_in->link == NULL) {
		svm_vector_math(&value,
		                &vector,
		                (NodeVectorMath)type_enum[type],
		                vector1_in->value,
		                vector2_in->value);

		if(socket == output("Value")) {
			optimized_value->x = value;
			return true;
		}
		else if(socket == output("Vector")) {
			*optimized_value = vector;
			return true;
		}
	}

	return false;
}

void VectorMathNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector1_in = input("Vector1");
	ShaderInput *vector2_in = input("Vector2");
	ShaderOutput *value_out = output("Value");
	ShaderOutput *vector_out = output("Vector");

	compiler.add_node(NODE_VECTOR_MATH,
		type_enum[type],
		compiler.stack_assign(vector1_in),
		compiler.stack_assign(vector2_in));
	compiler.add_node(NODE_VECTOR_MATH,
		compiler.stack_assign(value_out),
		compiler.stack_assign(vector_out));
}

void VectorMathNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("type", type);
	compiler.add(this, "node_vector_math");
}

/* VectorTransform */

VectorTransformNode::VectorTransformNode()
: ShaderNode("vector_transform")
{
	type = ustring("Vector");
	convert_from = ustring("world");
	convert_to = ustring("object");

	add_input("Vector", SHADER_SOCKET_VECTOR);
	add_output("Vector",  SHADER_SOCKET_VECTOR);
}

static ShaderEnum vector_transform_type_init()
{
	ShaderEnum enm;

	enm.insert("Vector", NODE_VECTOR_TRANSFORM_TYPE_VECTOR);
	enm.insert("Point", NODE_VECTOR_TRANSFORM_TYPE_POINT);
	enm.insert("Normal", NODE_VECTOR_TRANSFORM_TYPE_NORMAL);

	return enm;
}

static ShaderEnum vector_transform_convert_space_init()
{
	ShaderEnum enm;

	enm.insert("world", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD);
	enm.insert("object", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT);
	enm.insert("camera", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA);

	return enm;
}

ShaderEnum VectorTransformNode::type_enum = vector_transform_type_init();
ShaderEnum VectorTransformNode::convert_space_enum = vector_transform_convert_space_init();

void VectorTransformNode::compile(SVMCompiler& compiler)
{
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *vector_out = output("Vector");

	compiler.add_node(NODE_VECTOR_TRANSFORM,
		compiler.encode_uchar4(type_enum[type],
		                       convert_space_enum[convert_from],
		                       convert_space_enum[convert_to]),
		compiler.encode_uchar4(compiler.stack_assign(vector_in),
		                       compiler.stack_assign(vector_out)));
}

void VectorTransformNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("type", type);
	compiler.parameter("convert_from", convert_from);
	compiler.parameter("convert_to", convert_to);
	compiler.add(this, "node_vector_transform");
}

/* BumpNode */

BumpNode::BumpNode()
: ShaderNode("bump")
{
	invert = false;

	special_type = SHADER_SPECIAL_TYPE_BUMP;

	/* this input is used by the user, but after graph transform it is no longer
	 * used and moved to sampler center/x/y instead */
	add_input("Height", SHADER_SOCKET_FLOAT);

	add_input("SampleCenter", SHADER_SOCKET_FLOAT);
	add_input("SampleX", SHADER_SOCKET_FLOAT);
	add_input("SampleY", SHADER_SOCKET_FLOAT);
	add_input("Normal", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL);
	add_input("Strength", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Distance", SHADER_SOCKET_FLOAT, 0.1f);

	add_output("Normal", SHADER_SOCKET_NORMAL);
}

void BumpNode::compile(SVMCompiler& compiler)
{
	ShaderInput *center_in = input("SampleCenter");
	ShaderInput *dx_in = input("SampleX");
	ShaderInput *dy_in = input("SampleY");
	ShaderInput *normal_in = input("Normal");
	ShaderInput *strength_in = input("Strength");
	ShaderInput *distance_in = input("Distance");
	ShaderOutput *normal_out = output("Normal");

	/* pack all parameters in the node */
	compiler.add_node(NODE_SET_BUMP,
		compiler.encode_uchar4(
			compiler.stack_assign_if_linked(normal_in),
			compiler.stack_assign(distance_in),
			invert),
		compiler.encode_uchar4(
			compiler.stack_assign(center_in),
			compiler.stack_assign(dx_in),
			compiler.stack_assign(dy_in),
			compiler.stack_assign(strength_in)),
		compiler.stack_assign(normal_out));
}

void BumpNode::compile(OSLCompiler& compiler)
{
	compiler.parameter("invert", invert);
	compiler.add(this, "node_bump");
}

bool BumpNode::constant_fold(ShaderGraph *graph,
                             ShaderOutput * /*socket*/,
                             float3 * /*optimized_value*/)
{
	ShaderInput *height_in = input("Height");
	ShaderInput *normal_in = input("Normal");

	if(height_in->link == NULL) {
		if(normal_in->link == NULL) {
			GeometryNode *geom = new GeometryNode();
			graph->add(geom);
			graph->relink(this, outputs[0], geom->output("Normal"));
		}
		else {
			graph->relink(this, outputs[0], normal_in->link);
		}
		return true;
	}

	/* TODO(sergey): Ignore bump with zero strength. */

	return false;
}

/* RGBCurvesNode */

RGBCurvesNode::RGBCurvesNode()
: ShaderNode("rgb_curves")
{
	add_input("Fac", SHADER_SOCKET_FLOAT);
	add_input("Color", SHADER_SOCKET_COLOR);
	add_output("Color", SHADER_SOCKET_COLOR);

	min_x = 0.0f;
	max_x = 1.0f;
}

void RGBCurvesNode::compile(SVMCompiler& compiler)
{
	ShaderInput *fac_in = input("Fac");
	ShaderInput *color_in = input("Color");
	ShaderOutput *color_out = output("Color");

	compiler.add_node(NODE_RGB_CURVES,
	                  compiler.encode_uchar4(compiler.stack_assign(fac_in),
	                                         compiler.stack_assign(color_in),
	                                         compiler.stack_assign(color_out)),
	                  __float_as_int(min_x),
	                  __float_as_int(max_x));
	compiler.add_array(curves, RAMP_TABLE_SIZE);
}

void RGBCurvesNode::compile(OSLCompiler& compiler)
{
	float ramp[RAMP_TABLE_SIZE][3];

	for(int i = 0; i < RAMP_TABLE_SIZE; ++i) {
		ramp[i][0] = curves[i].x;
		ramp[i][1] = curves[i].y;
		ramp[i][2] = curves[i].z;
	}

	compiler.parameter_color_array("ramp", ramp, RAMP_TABLE_SIZE);
	compiler.parameter("min_x", min_x);
	compiler.parameter("max_x", max_x);
	compiler.add(this, "node_rgb_curves");
}

/* VectorCurvesNode */

VectorCurvesNode::VectorCurvesNode()
: ShaderNode("vector_curves")
{
	add_input("Fac", SHADER_SOCKET_FLOAT);
	add_input("Vector", SHADER_SOCKET_VECTOR);
	add_output("Vector", SHADER_SOCKET_VECTOR);

	min_x = 0.0f;
	max_x = 1.0f;
}

void VectorCurvesNode::compile(SVMCompiler& compiler)
{
	ShaderInput *fac_in = input("Fac");
	ShaderInput *vector_in = input("Vector");
	ShaderOutput *vector_out = output("Vector");

	compiler.add_node(NODE_VECTOR_CURVES,
	                  compiler.encode_uchar4(compiler.stack_assign(fac_in),
	                                         compiler.stack_assign(vector_in),
	                                         compiler.stack_assign(vector_out)),
	                  __float_as_int(min_x),
	                  __float_as_int(max_x));
	compiler.add_array(curves, RAMP_TABLE_SIZE);
}

void VectorCurvesNode::compile(OSLCompiler& compiler)
{
	float ramp[RAMP_TABLE_SIZE][3];

	for(int i = 0; i < RAMP_TABLE_SIZE; ++i) {
		ramp[i][0] = curves[i].x;
		ramp[i][1] = curves[i].y;
		ramp[i][2] = curves[i].z;
	}

	compiler.parameter_color_array("ramp", ramp, RAMP_TABLE_SIZE);
	compiler.parameter("min_x", min_x);
	compiler.parameter("max_x", max_x);
	compiler.add(this, "node_vector_curves");
}

/* RGBRampNode */

RGBRampNode::RGBRampNode()
: ShaderNode("rgb_ramp")
{
	add_input("Fac", SHADER_SOCKET_FLOAT);
	add_output("Color", SHADER_SOCKET_COLOR);
	add_output("Alpha", SHADER_SOCKET_FLOAT);

	interpolate = true;
}

void RGBRampNode::compile(SVMCompiler& compiler)
{
	ShaderInput *fac_in = input("Fac");
	ShaderOutput *color_out = output("Color");
	ShaderOutput *alpha_out = output("Alpha");

	compiler.add_node(NODE_RGB_RAMP,
		compiler.encode_uchar4(
			compiler.stack_assign(fac_in),
			compiler.stack_assign_if_linked(color_out),
			compiler.stack_assign_if_linked(alpha_out)),
		interpolate);
	compiler.add_array(ramp, RAMP_TABLE_SIZE);
}

void RGBRampNode::compile(OSLCompiler& compiler)
{
	/* OSL shader only takes separate RGB and A array, split the RGBA base array */
	/* NB: cycles float3 type is actually 4 floats! need to use an explicit array */
	float ramp_color[RAMP_TABLE_SIZE][3];
	float ramp_alpha[RAMP_TABLE_SIZE];

	for(int i = 0; i < RAMP_TABLE_SIZE; ++i) {
		ramp_color[i][0] = ramp[i].x;
		ramp_color[i][1] = ramp[i].y;
		ramp_color[i][2] = ramp[i].z;
		ramp_alpha[i] = ramp[i].w;
	}

	compiler.parameter_color_array("ramp_color", ramp_color, RAMP_TABLE_SIZE);
	compiler.parameter_array("ramp_alpha", ramp_alpha, RAMP_TABLE_SIZE);
	compiler.parameter("ramp_interpolate", interpolate);
	
	compiler.add(this, "node_rgb_ramp");
}

/* Set Normal Node */

SetNormalNode::SetNormalNode()
: ShaderNode("set_normal")
{
	add_input("Direction", SHADER_SOCKET_VECTOR);
	add_output("Normal", SHADER_SOCKET_NORMAL);
}

void SetNormalNode::compile(SVMCompiler& compiler)
{
	ShaderInput  *direction_in = input("Direction");
	ShaderOutput *normal_out = output("Normal");

	compiler.add_node(NODE_CLOSURE_SET_NORMAL,
		compiler.stack_assign(direction_in),
		compiler.stack_assign(normal_out));
}

void SetNormalNode::compile(OSLCompiler& compiler)
{
	compiler.add(this, "node_set_normal"); 
}

/* OSLScriptNode */

OSLScriptNode::OSLScriptNode()
: ShaderNode("osl_script")
{
	special_type = SHADER_SPECIAL_TYPE_SCRIPT;
}

void OSLScriptNode::compile(SVMCompiler& /*compiler*/)
{
	/* doesn't work for SVM, obviously ... */
}

void OSLScriptNode::compile(OSLCompiler& compiler)
{
	if(!filepath.empty())
		compiler.add(this, filepath.c_str(), true);
	else
		compiler.add(this, bytecode_hash.c_str(), false);
}

/* Normal Map */

static ShaderEnum normal_map_space_init()
{
	ShaderEnum enm;

	enm.insert("Tangent", NODE_NORMAL_MAP_TANGENT);
	enm.insert("Object", NODE_NORMAL_MAP_OBJECT);
	enm.insert("World", NODE_NORMAL_MAP_WORLD);
	enm.insert("Blender Object", NODE_NORMAL_MAP_BLENDER_OBJECT);
	enm.insert("Blender World", NODE_NORMAL_MAP_BLENDER_WORLD);

	return enm;
}

ShaderEnum NormalMapNode::space_enum = normal_map_space_init();

NormalMapNode::NormalMapNode()
: ShaderNode("normal_map")
{
	space = ustring("Tangent");
	attribute = ustring("");

	add_input("NormalIn", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, ShaderInput::USE_OSL);
	add_input("Strength", SHADER_SOCKET_FLOAT, 1.0f);
	add_input("Color", SHADER_SOCKET_COLOR);

	add_output("Normal", SHADER_SOCKET_NORMAL);
}

void NormalMapNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	if(shader->has_surface && space == ustring("Tangent")) {
		if(attribute == ustring("")) {
			attributes->add(ATTR_STD_UV_TANGENT);
			attributes->add(ATTR_STD_UV_TANGENT_SIGN);
		}
		else {
			attributes->add(ustring((string(attribute.c_str()) + ".tangent").c_str()));
			attributes->add(ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
		}

		attributes->add(ATTR_STD_VERTEX_NORMAL);
	}
	
	ShaderNode::attributes(shader, attributes);
}

void NormalMapNode::compile(SVMCompiler& compiler)
{
	ShaderInput  *color_in = input("Color");
	ShaderInput  *strength_in = input("Strength");
	ShaderOutput *normal_out = output("Normal");
	int attr = 0, attr_sign = 0;

	if(space == ustring("Tangent")) {
		if(attribute == ustring("")) {
			attr = compiler.attribute(ATTR_STD_UV_TANGENT);
			attr_sign = compiler.attribute(ATTR_STD_UV_TANGENT_SIGN);
		}
		else {
			attr = compiler.attribute(ustring((string(attribute.c_str()) + ".tangent").c_str()));
			attr_sign = compiler.attribute(ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
		}
	}

	compiler.add_node(NODE_NORMAL_MAP,
		compiler.encode_uchar4(
			compiler.stack_assign(color_in),
			compiler.stack_assign(strength_in),
			compiler.stack_assign(normal_out),
			space_enum[space]),
		attr, attr_sign);
}

void NormalMapNode::compile(OSLCompiler& compiler)
{
	if(space == ustring("Tangent")) {
		if(attribute == ustring("")) {
			compiler.parameter("attr_name", ustring("geom:tangent"));
			compiler.parameter("attr_sign_name", ustring("geom:tangent_sign"));
		}
		else {
			compiler.parameter("attr_name", ustring((string(attribute.c_str()) + ".tangent").c_str()));
			compiler.parameter("attr_sign_name", ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
		}
	}

	compiler.parameter("space", space);

	compiler.add(this, "node_normal_map"); 
}

/* Tangent */

static ShaderEnum tangent_direction_type_init()
{
	ShaderEnum enm;

	enm.insert("Radial", NODE_TANGENT_RADIAL);
	enm.insert("UV Map", NODE_TANGENT_UVMAP);

	return enm;
}

static ShaderEnum tangent_axis_init()
{
	ShaderEnum enm;

	enm.insert("X", NODE_TANGENT_AXIS_X);
	enm.insert("Y", NODE_TANGENT_AXIS_Y);
	enm.insert("Z", NODE_TANGENT_AXIS_Z);

	return enm;
}

ShaderEnum TangentNode::direction_type_enum = tangent_direction_type_init();
ShaderEnum TangentNode::axis_enum = tangent_axis_init();

TangentNode::TangentNode()
: ShaderNode("tangent")
{
	direction_type = ustring("Radial");
	axis = ustring("X");
	attribute = ustring("");

	add_input("NormalIn", SHADER_SOCKET_NORMAL, ShaderInput::NORMAL, ShaderInput::USE_OSL);
	add_output("Tangent", SHADER_SOCKET_NORMAL);
}

void TangentNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
	if(shader->has_surface) {
		if(direction_type == ustring("UV Map")) {
			if(attribute == ustring(""))
				attributes->add(ATTR_STD_UV_TANGENT);
			else
				attributes->add(ustring((string(attribute.c_str()) + ".tangent").c_str()));
		}
		else
			attributes->add(ATTR_STD_GENERATED);
	}
	
	ShaderNode::attributes(shader, attributes);
}

void TangentNode::compile(SVMCompiler& compiler)
{
	ShaderOutput *tangent_out = output("Tangent");
	int attr;

	if(direction_type == ustring("UV Map")) {
		if(attribute == ustring(""))
			attr = compiler.attribute(ATTR_STD_UV_TANGENT);
		else
			attr = compiler.attribute(ustring((string(attribute.c_str()) + ".tangent").c_str()));
	}
	else
		attr = compiler.attribute(ATTR_STD_GENERATED);

	compiler.add_node(NODE_TANGENT,
		compiler.encode_uchar4(
			compiler.stack_assign(tangent_out),
			direction_type_enum[direction_type],
			axis_enum[axis]), attr);
}

void TangentNode::compile(OSLCompiler& compiler)
{
	if(direction_type == ustring("UV Map")) {
		if(attribute == ustring(""))
			compiler.parameter("attr_name", ustring("geom:tangent"));
		else
			compiler.parameter("attr_name", ustring((string(attribute.c_str()) + ".tangent").c_str()));
	}

	compiler.parameter("direction_type", direction_type);
	compiler.parameter("axis", axis);
	compiler.add(this, "node_tangent"); 
}

CCL_NAMESPACE_END
