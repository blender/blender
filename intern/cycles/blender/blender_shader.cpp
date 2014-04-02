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
 * limitations under the License
 */

#include "background.h"
#include "graph.h"
#include "light.h"
#include "nodes.h"
#include "osl.h"
#include "scene.h"
#include "shader.h"

#include "blender_sync.h"
#include "blender_util.h"

#include "util_debug.h"

CCL_NAMESPACE_BEGIN

typedef map<void*, ShaderInput*> PtrInputMap;
typedef map<void*, ShaderOutput*> PtrOutputMap;
typedef map<std::string, ProxyNode*> ProxyMap;

/* Find */

void BlenderSync::find_shader(BL::ID id, vector<uint>& used_shaders, int default_shader)
{
	Shader *shader = (id)? shader_map.find(id): scene->shaders[default_shader];

	for(size_t i = 0; i < scene->shaders.size(); i++) {
		if(scene->shaders[i] == shader) {
			used_shaders.push_back(i);
			scene->shaders[i]->tag_used(scene);
			break;
		}
	}
}

/* Graph */

static BL::NodeSocket get_node_output(BL::Node b_node, const string& name)
{
	BL::Node::outputs_iterator b_out;
	
	for(b_node.outputs.begin(b_out); b_out != b_node.outputs.end(); ++b_out)
		if(b_out->name() == name)
			return *b_out;
	
	assert(0);
	
	return *b_out;
}

static float3 get_node_output_rgba(BL::Node b_node, const string& name)
{
	BL::NodeSocket b_sock = get_node_output(b_node, name);
	float value[4];
	RNA_float_get_array(&b_sock.ptr, "default_value", value);
	return make_float3(value[0], value[1], value[2]);
}

static float get_node_output_value(BL::Node b_node, const string& name)
{
	BL::NodeSocket b_sock = get_node_output(b_node, name);
	return RNA_float_get(&b_sock.ptr, "default_value");
}

static float3 get_node_output_vector(BL::Node b_node, const string& name)
{
	BL::NodeSocket b_sock = get_node_output(b_node, name);
	float value[3];
	RNA_float_get_array(&b_sock.ptr, "default_value", value);
	return make_float3(value[0], value[1], value[2]);
}

static ShaderSocketType convert_socket_type(BL::NodeSocket b_socket)
{
	switch (b_socket.type()) {
		case BL::NodeSocket::type_VALUE:
			return SHADER_SOCKET_FLOAT;
		case BL::NodeSocket::type_INT:
			return SHADER_SOCKET_INT;
		case BL::NodeSocket::type_VECTOR:
			return SHADER_SOCKET_VECTOR;
		case BL::NodeSocket::type_RGBA:
			return SHADER_SOCKET_COLOR;
		case BL::NodeSocket::type_STRING:
			return SHADER_SOCKET_STRING;
		case BL::NodeSocket::type_SHADER:
			return SHADER_SOCKET_CLOSURE;
		
		default:
			return SHADER_SOCKET_UNDEFINED;
	}
}

static void set_default_value(ShaderInput *input, BL::Node b_node, BL::NodeSocket b_sock, BL::BlendData b_data, BL::ID b_id)
{
	/* copy values for non linked inputs */
	switch(input->type) {
	case SHADER_SOCKET_FLOAT: {
		input->set(get_float(b_sock.ptr, "default_value"));
		break;
	}
	case SHADER_SOCKET_INT: {
		input->set((float)get_int(b_sock.ptr, "default_value"));
		break;
	}
	case SHADER_SOCKET_COLOR: {
		input->set(float4_to_float3(get_float4(b_sock.ptr, "default_value")));
		break;
	}
	case SHADER_SOCKET_NORMAL:
	case SHADER_SOCKET_POINT:
	case SHADER_SOCKET_VECTOR: {
		input->set(get_float3(b_sock.ptr, "default_value"));
		break;
	}
	case SHADER_SOCKET_STRING: {
		input->set((ustring)blender_absolute_path(b_data, b_id, get_string(b_sock.ptr, "default_value")));
		break;
	}
	
	case SHADER_SOCKET_CLOSURE:
	case SHADER_SOCKET_UNDEFINED:
		break;
	}
}

static void get_tex_mapping(TextureMapping *mapping, BL::TexMapping b_mapping)
{
	if(!b_mapping)
		return;

	mapping->translation = get_float3(b_mapping.translation());
	mapping->rotation = get_float3(b_mapping.rotation());
	mapping->scale = get_float3(b_mapping.scale());
	mapping->type = (TextureMapping::Type)b_mapping.vector_type();

	mapping->x_mapping = (TextureMapping::Mapping)b_mapping.mapping_x();
	mapping->y_mapping = (TextureMapping::Mapping)b_mapping.mapping_y();
	mapping->z_mapping = (TextureMapping::Mapping)b_mapping.mapping_z();
}

static void get_tex_mapping(TextureMapping *mapping, BL::ShaderNodeMapping b_mapping)
{
	if(!b_mapping)
		return;

	mapping->translation = get_float3(b_mapping.translation());
	mapping->rotation = get_float3(b_mapping.rotation());
	mapping->scale = get_float3(b_mapping.scale());
	mapping->type = (TextureMapping::Type)b_mapping.vector_type();

	mapping->use_minmax = b_mapping.use_min() || b_mapping.use_max();

	if(b_mapping.use_min())
		mapping->min = get_float3(b_mapping.min());
	if(b_mapping.use_max())
		mapping->max = get_float3(b_mapping.max());
}

static bool is_output_node(BL::Node b_node)
{
	return (b_node.is_a(&RNA_ShaderNodeOutputMaterial)
		    || b_node.is_a(&RNA_ShaderNodeOutputWorld)
		    || b_node.is_a(&RNA_ShaderNodeOutputLamp));
}

static ShaderNode *add_node(Scene *scene, BL::BlendData b_data, BL::Scene b_scene, ShaderGraph *graph, BL::ShaderNodeTree b_ntree, BL::ShaderNode b_node)
{
	ShaderNode *node = NULL;

	/* existing blender nodes */
	if (b_node.is_a(&RNA_ShaderNodeRGBCurve)) {
		BL::ShaderNodeRGBCurve b_curve_node(b_node);
		RGBCurvesNode *curves = new RGBCurvesNode();
		curvemapping_color_to_array(b_curve_node.mapping(), curves->curves, RAMP_TABLE_SIZE, true);
		node = curves;
	}
	if (b_node.is_a(&RNA_ShaderNodeVectorCurve)) {
		BL::ShaderNodeVectorCurve b_curve_node(b_node);
		VectorCurvesNode *curves = new VectorCurvesNode();
		curvemapping_color_to_array(b_curve_node.mapping(), curves->curves, RAMP_TABLE_SIZE, false);
		node = curves;
	}
	else if (b_node.is_a(&RNA_ShaderNodeValToRGB)) {
		RGBRampNode *ramp = new RGBRampNode();
		BL::ShaderNodeValToRGB b_ramp_node(b_node);
		colorramp_to_array(b_ramp_node.color_ramp(), ramp->ramp, RAMP_TABLE_SIZE);
		ramp->interpolate = b_ramp_node.color_ramp().interpolation() != BL::ColorRamp::interpolation_CONSTANT;
		node = ramp;
	}
	else if (b_node.is_a(&RNA_ShaderNodeRGB)) {
		ColorNode *color = new ColorNode();
		color->value = get_node_output_rgba(b_node, "Color");
		node = color;
	}
	else if (b_node.is_a(&RNA_ShaderNodeValue)) {
		ValueNode *value = new ValueNode();
		value->value = get_node_output_value(b_node, "Value");
		node = value;
	}
	else if (b_node.is_a(&RNA_ShaderNodeCameraData)) {
		node = new CameraNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeInvert)) {
		node = new InvertNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeGamma)) {
		node = new GammaNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeBrightContrast)) {
		node = new BrightContrastNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeMixRGB)) {
		BL::ShaderNodeMixRGB b_mix_node(b_node);
		MixNode *mix = new MixNode();
		mix->type = MixNode::type_enum[b_mix_node.blend_type()];
			mix->use_clamp = b_mix_node.use_clamp();
		node = mix;
	}
	else if (b_node.is_a(&RNA_ShaderNodeSeparateRGB)) {
		node = new SeparateRGBNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeCombineRGB)) {
		node = new CombineRGBNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeSeparateHSV)) {
		node = new SeparateHSVNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeCombineHSV)) {
		node = new CombineHSVNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeHueSaturation)) {
		node = new HSVNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeRGBToBW)) {
		node = new ConvertNode(SHADER_SOCKET_COLOR, SHADER_SOCKET_FLOAT);
	}
	else if (b_node.is_a(&RNA_ShaderNodeMath)) {
		BL::ShaderNodeMath b_math_node(b_node);
		MathNode *math = new MathNode();
		math->type = MathNode::type_enum[b_math_node.operation()];
			math->use_clamp = b_math_node.use_clamp();
		node = math;
	}
	else if (b_node.is_a(&RNA_ShaderNodeVectorMath)) {
		BL::ShaderNodeVectorMath b_vector_math_node(b_node);
		VectorMathNode *vmath = new VectorMathNode();
		vmath->type = VectorMathNode::type_enum[b_vector_math_node.operation()];
		node = vmath;
	}
	else if (b_node.is_a(&RNA_ShaderNodeVectorTransform)) {
		BL::ShaderNodeVectorTransform b_vector_transform_node(b_node);
		VectorTransformNode *vtransform = new VectorTransformNode();
		vtransform->type = VectorTransformNode::type_enum[b_vector_transform_node.type()];
		vtransform->convert_from = VectorTransformNode::convert_space_enum[b_vector_transform_node.convert_from()];
		vtransform->convert_to = VectorTransformNode::convert_space_enum[b_vector_transform_node.convert_to()];
		node = vtransform;
	}
	else if (b_node.is_a(&RNA_ShaderNodeNormal)) {
		BL::Node::outputs_iterator out_it;
		b_node.outputs.begin(out_it);
		
		NormalNode *norm = new NormalNode();
		norm->direction = get_node_output_vector(b_node, "Normal");
		node = norm;
	}
	else if (b_node.is_a(&RNA_ShaderNodeMapping)) {
		BL::ShaderNodeMapping b_mapping_node(b_node);
		MappingNode *mapping = new MappingNode();
		
		get_tex_mapping(&mapping->tex_mapping, b_mapping_node);
		
		node = mapping;
	}
	else if (b_node.is_a(&RNA_ShaderNodeFresnel)) {
		node = new FresnelNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeLayerWeight)) {
		node = new LayerWeightNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeAddShader)) {
		node = new AddClosureNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeMixShader)) {
		node = new MixClosureNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeAttribute)) {
		BL::ShaderNodeAttribute b_attr_node(b_node);
		AttributeNode *attr = new AttributeNode();
		attr->attribute = b_attr_node.attribute_name();
		node = attr;
	}
	else if (b_node.is_a(&RNA_ShaderNodeBackground)) {
		node = new BackgroundNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeHoldout)) {
		node = new HoldoutNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfAnisotropic)) {
		node = new WardBsdfNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfDiffuse)) {
		node = new DiffuseBsdfNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeSubsurfaceScattering)) {
		BL::ShaderNodeSubsurfaceScattering b_subsurface_node(b_node);

		SubsurfaceScatteringNode *subsurface = new SubsurfaceScatteringNode();

		switch(b_subsurface_node.falloff()) {
		case BL::ShaderNodeSubsurfaceScattering::falloff_CUBIC:
			subsurface->closure = CLOSURE_BSSRDF_CUBIC_ID;
			break;
		case BL::ShaderNodeSubsurfaceScattering::falloff_GAUSSIAN:
			subsurface->closure = CLOSURE_BSSRDF_GAUSSIAN_ID;
			break;
		}

		node = subsurface;
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfGlossy)) {
		BL::ShaderNodeBsdfGlossy b_glossy_node(b_node);
		GlossyBsdfNode *glossy = new GlossyBsdfNode();
		
		switch(b_glossy_node.distribution()) {
		case BL::ShaderNodeBsdfGlossy::distribution_SHARP:
			glossy->distribution = ustring("Sharp");
			break;
		case BL::ShaderNodeBsdfGlossy::distribution_BECKMANN:
			glossy->distribution = ustring("Beckmann");
			break;
		case BL::ShaderNodeBsdfGlossy::distribution_GGX:
			glossy->distribution = ustring("GGX");
			break;
		}
		node = glossy;
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfGlass)) {
		BL::ShaderNodeBsdfGlass b_glass_node(b_node);
		GlassBsdfNode *glass = new GlassBsdfNode();
		switch(b_glass_node.distribution()) {
		case BL::ShaderNodeBsdfGlass::distribution_SHARP:
			glass->distribution = ustring("Sharp");
			break;
		case BL::ShaderNodeBsdfGlass::distribution_BECKMANN:
			glass->distribution = ustring("Beckmann");
			break;
		case BL::ShaderNodeBsdfGlass::distribution_GGX:
			glass->distribution = ustring("GGX");
			break;
		}
		node = glass;
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfRefraction)) {
		BL::ShaderNodeBsdfRefraction b_refraction_node(b_node);
		RefractionBsdfNode *refraction = new RefractionBsdfNode();
		switch(b_refraction_node.distribution()) {
			case BL::ShaderNodeBsdfRefraction::distribution_SHARP:
				refraction->distribution = ustring("Sharp");
				break;
			case BL::ShaderNodeBsdfRefraction::distribution_BECKMANN:
				refraction->distribution = ustring("Beckmann");
				break;
			case BL::ShaderNodeBsdfRefraction::distribution_GGX:
				refraction->distribution = ustring("GGX");
				break;
		}
		node = refraction;
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfToon)) {
		BL::ShaderNodeBsdfToon b_toon_node(b_node);
		ToonBsdfNode *toon = new ToonBsdfNode();
		switch(b_toon_node.component()) {
			case BL::ShaderNodeBsdfToon::component_DIFFUSE:
				toon->component = ustring("Diffuse");
				break;
			case BL::ShaderNodeBsdfToon::component_GLOSSY:
				toon->component = ustring("Glossy");
				break;
		}
		node = toon;
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfHair)) {
		BL::ShaderNodeBsdfHair b_hair_node(b_node);
		HairBsdfNode *hair = new HairBsdfNode();
		switch(b_hair_node.component()) {
			case BL::ShaderNodeBsdfHair::component_Reflection:
				hair->component = ustring("Reflection");
				break;
			case BL::ShaderNodeBsdfHair::component_Transmission:
				hair->component = ustring("Transmission");
				break;
		}
		node = hair;
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfTranslucent)) {
		node = new TranslucentBsdfNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfTransparent)) {
		node = new TransparentBsdfNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeBsdfVelvet)) {
		node = new VelvetBsdfNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeEmission)) {
		node = new EmissionNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeAmbientOcclusion)) {
		node = new AmbientOcclusionNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeVolumeScatter)) {
		node = new ScatterVolumeNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeVolumeAbsorption)) {
		node = new AbsorptionVolumeNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeNewGeometry)) {
		node = new GeometryNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeWireframe)) {
		BL::ShaderNodeWireframe b_wireframe_node(b_node);
		WireframeNode *wire = new WireframeNode();
		wire->use_pixel_size = b_wireframe_node.use_pixel_size();
		node = wire;
	}
	else if (b_node.is_a(&RNA_ShaderNodeWavelength)) {
		node = new WavelengthNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeBlackbody)) {
		node = new BlackbodyNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeLightPath)) {
		node = new LightPathNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeLightFalloff)) {
		node = new LightFalloffNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeObjectInfo)) {
		node = new ObjectInfoNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeParticleInfo)) {
		node = new ParticleInfoNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeHairInfo)) {
		node = new HairInfoNode();
	}
	else if (b_node.is_a(&RNA_ShaderNodeBump)) {
		BL::ShaderNodeBump b_bump_node(b_node);
		BumpNode *bump = new BumpNode();
		bump->invert = b_bump_node.invert();
		node = bump;
	}
	else if (b_node.is_a(&RNA_ShaderNodeScript)) {
#ifdef WITH_OSL
		if(scene->shader_manager->use_osl()) {
			/* create script node */
			BL::ShaderNodeScript b_script_node(b_node);
			OSLScriptNode *script_node = new OSLScriptNode();
			
			/* Generate inputs/outputs from node sockets
			 *
			 * Note: the node sockets are generated from OSL parameters,
			 * so the names match those of the corresponding parameters exactly.
			 *
			 * Note 2: ShaderInput/ShaderOutput store shallow string copies only!
			 * Socket names must be stored in the extra lists instead. */
			BL::Node::inputs_iterator b_input;
			
			for (b_script_node.inputs.begin(b_input); b_input != b_script_node.inputs.end(); ++b_input) {
				script_node->input_names.push_back(ustring(b_input->name()));
				ShaderInput *input = script_node->add_input(script_node->input_names.back().c_str(),
				                                            convert_socket_type(*b_input));
				set_default_value(input, b_node, *b_input, b_data, b_ntree);
			}
			
			BL::Node::outputs_iterator b_output;
			
			for (b_script_node.outputs.begin(b_output); b_output != b_script_node.outputs.end(); ++b_output) {
				script_node->output_names.push_back(ustring(b_output->name()));
				script_node->add_output(script_node->output_names.back().c_str(),
				                        convert_socket_type(*b_output));
			}
			
			/* load bytecode or filepath */
			OSLShaderManager *manager = (OSLShaderManager*)scene->shader_manager;
			string bytecode_hash = b_script_node.bytecode_hash();
			
			if(!bytecode_hash.empty()) {
				/* loaded bytecode if not already done */
				if(!manager->shader_test_loaded(bytecode_hash))
					manager->shader_load_bytecode(bytecode_hash, b_script_node.bytecode());
				
				script_node->bytecode_hash = bytecode_hash;
			}
			else {
				/* set filepath */
				script_node->filepath = blender_absolute_path(b_data, b_ntree, b_script_node.filepath());
			}
			
			node = script_node;
		}
#endif
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexImage)) {
		BL::ShaderNodeTexImage b_image_node(b_node);
		BL::Image b_image(b_image_node.image());
		ImageTextureNode *image = new ImageTextureNode();
		if(b_image) {
			/* builtin images will use callback-based reading because
			 * they could only be loaded correct from blender side
			 */
			bool is_builtin = b_image.packed_file() ||
			                  b_image.source() == BL::Image::source_GENERATED ||
			                  b_image.source() == BL::Image::source_MOVIE;

			if(is_builtin) {
				/* for builtin images we're using image datablock name to find an image to
				 * read pixels from later
				 *
				 * also store frame number as well, so there's no differences in handling
				 * builtin names for packed images and movies
				 */
				int scene_frame = b_scene.frame_current();
				int image_frame = image_user_frame_number(b_image_node.image_user(), scene_frame);
				image->filename = b_image.name() + "@" + string_printf("%d", image_frame);
				image->builtin_data = b_image.ptr.data;
			}
			else {
				image->filename = image_user_file_path(b_image_node.image_user(), b_image, b_scene.frame_current());
				image->builtin_data = NULL;
			}

			image->animated = b_image_node.image_user().use_auto_refresh();
		}
		image->color_space = ImageTextureNode::color_space_enum[(int)b_image_node.color_space()];
		image->projection = ImageTextureNode::projection_enum[(int)b_image_node.projection()];
		image->interpolation = (InterpolationType)b_image_node.interpolation();
		image->projection_blend = b_image_node.projection_blend();
		get_tex_mapping(&image->tex_mapping, b_image_node.texture_mapping());
		node = image;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexEnvironment)) {
		BL::ShaderNodeTexEnvironment b_env_node(b_node);
		BL::Image b_image(b_env_node.image());
		EnvironmentTextureNode *env = new EnvironmentTextureNode();
		if(b_image) {
			bool is_builtin = b_image.packed_file() ||
			                  b_image.source() == BL::Image::source_GENERATED ||
			                  b_image.source() == BL::Image::source_MOVIE;

			if(is_builtin) {
				int scene_frame = b_scene.frame_current();
				int image_frame = image_user_frame_number(b_env_node.image_user(), scene_frame);
				env->filename = b_image.name() + "@" + string_printf("%d", image_frame);
				env->builtin_data = b_image.ptr.data;
			}
			else {
				env->filename = image_user_file_path(b_env_node.image_user(), b_image, b_scene.frame_current());
				env->animated = b_env_node.image_user().use_auto_refresh();
				env->builtin_data = NULL;
			}
		}
		env->color_space = EnvironmentTextureNode::color_space_enum[(int)b_env_node.color_space()];
		env->projection = EnvironmentTextureNode::projection_enum[(int)b_env_node.projection()];
		get_tex_mapping(&env->tex_mapping, b_env_node.texture_mapping());
		node = env;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexGradient)) {
		BL::ShaderNodeTexGradient b_gradient_node(b_node);
		GradientTextureNode *gradient = new GradientTextureNode();
		gradient->type = GradientTextureNode::type_enum[(int)b_gradient_node.gradient_type()];
		get_tex_mapping(&gradient->tex_mapping, b_gradient_node.texture_mapping());
		node = gradient;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexVoronoi)) {
		BL::ShaderNodeTexVoronoi b_voronoi_node(b_node);
		VoronoiTextureNode *voronoi = new VoronoiTextureNode();
		voronoi->coloring = VoronoiTextureNode::coloring_enum[(int)b_voronoi_node.coloring()];
		get_tex_mapping(&voronoi->tex_mapping, b_voronoi_node.texture_mapping());
		node = voronoi;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexMagic)) {
		BL::ShaderNodeTexMagic b_magic_node(b_node);
		MagicTextureNode *magic = new MagicTextureNode();
		magic->depth = b_magic_node.turbulence_depth();
		get_tex_mapping(&magic->tex_mapping, b_magic_node.texture_mapping());
		node = magic;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexWave)) {
		BL::ShaderNodeTexWave b_wave_node(b_node);
		WaveTextureNode *wave = new WaveTextureNode();
		wave->type = WaveTextureNode::type_enum[(int)b_wave_node.wave_type()];
		get_tex_mapping(&wave->tex_mapping, b_wave_node.texture_mapping());
		node = wave;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexChecker)) {
		BL::ShaderNodeTexChecker b_checker_node(b_node);
		CheckerTextureNode *checker = new CheckerTextureNode();
		get_tex_mapping(&checker->tex_mapping, b_checker_node.texture_mapping());
		node = checker;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexBrick)) {
		BL::ShaderNodeTexBrick b_brick_node(b_node);
		BrickTextureNode *brick = new BrickTextureNode();
		brick->offset = b_brick_node.offset();
		brick->offset_frequency = b_brick_node.offset_frequency();
		brick->squash = b_brick_node.squash();
		brick->squash_frequency = b_brick_node.squash_frequency();
		get_tex_mapping(&brick->tex_mapping, b_brick_node.texture_mapping());
		node = brick;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexNoise)) {
		BL::ShaderNodeTexNoise b_noise_node(b_node);
		NoiseTextureNode *noise = new NoiseTextureNode();
		get_tex_mapping(&noise->tex_mapping, b_noise_node.texture_mapping());
		node = noise;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexMusgrave)) {
		BL::ShaderNodeTexMusgrave b_musgrave_node(b_node);
		MusgraveTextureNode *musgrave = new MusgraveTextureNode();
		musgrave->type = MusgraveTextureNode::type_enum[(int)b_musgrave_node.musgrave_type()];
		get_tex_mapping(&musgrave->tex_mapping, b_musgrave_node.texture_mapping());
		node = musgrave;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexCoord)) {
		BL::ShaderNodeTexCoord b_tex_coord_node(b_node);
		TextureCoordinateNode *tex_coord = new TextureCoordinateNode();
		tex_coord->from_dupli = b_tex_coord_node.from_dupli();
		node = tex_coord;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTexSky)) {
		BL::ShaderNodeTexSky b_sky_node(b_node);
		SkyTextureNode *sky = new SkyTextureNode();
		sky->type = SkyTextureNode::type_enum[(int)b_sky_node.sky_type()];
		sky->sun_direction = normalize(get_float3(b_sky_node.sun_direction()));
		sky->turbidity = b_sky_node.turbidity();
		sky->ground_albedo = b_sky_node.ground_albedo();
		get_tex_mapping(&sky->tex_mapping, b_sky_node.texture_mapping());
		node = sky;
	}
	else if (b_node.is_a(&RNA_ShaderNodeNormalMap)) {
		BL::ShaderNodeNormalMap b_normal_map_node(b_node);
		NormalMapNode *nmap = new NormalMapNode();
		nmap->space = NormalMapNode::space_enum[(int)b_normal_map_node.space()];
		nmap->attribute = b_normal_map_node.uv_map();
		node = nmap;
	}
	else if (b_node.is_a(&RNA_ShaderNodeTangent)) {
		BL::ShaderNodeTangent b_tangent_node(b_node);
		TangentNode *tangent = new TangentNode();
		tangent->direction_type = TangentNode::direction_type_enum[(int)b_tangent_node.direction_type()];
		tangent->axis = TangentNode::axis_enum[(int)b_tangent_node.axis()];
		tangent->attribute = b_tangent_node.uv_map();
		node = tangent;
	}
	else if (b_node.is_a(&RNA_ShaderNodeUVMap)) {
		BL::ShaderNodeUVMap b_uvmap_node(b_node);
		UVMapNode *uvm = new UVMapNode();
		uvm->attribute = b_uvmap_node.uv_map();
		uvm->from_dupli = b_uvmap_node.from_dupli();
		node = uvm;
	}

	if(node)
		graph->add(node);

	return node;
}

static bool node_use_modified_socket_name(ShaderNode *node)
{
	if (node->special_type == SHADER_SPECIAL_TYPE_SCRIPT)
		return false;
	
	return true;
}

static ShaderInput *node_find_input_by_name(ShaderNode *node, BL::Node b_node, BL::NodeSocket b_socket)
{
	string name = b_socket.name();
	
	if (node_use_modified_socket_name(node)) {
		BL::Node::inputs_iterator b_input;
		bool found = false;
		int counter = 0, total = 0;
		
		for (b_node.inputs.begin(b_input); b_input != b_node.inputs.end(); ++b_input) {
			if (b_input->name() == name) {
				if (!found)
					counter++;
				total++;
			}
			
			if(b_input->ptr.data == b_socket.ptr.data)
				found = true;
		}
		
		/* rename if needed */
		if (name == "Shader")
			name = "Closure";
		
		if (total > 1)
			name = string_printf("%s%d", name.c_str(), counter);
	}
	
	return node->input(name.c_str());
}

static ShaderOutput *node_find_output_by_name(ShaderNode *node, BL::Node b_node, BL::NodeSocket b_socket)
{
	string name = b_socket.name();
	
	if (node_use_modified_socket_name(node)) {
		BL::Node::outputs_iterator b_output;
		bool found = false;
		int counter = 0, total = 0;
		
		for (b_node.outputs.begin(b_output); b_output != b_node.outputs.end(); ++b_output) {
			if (b_output->name() == name) {
				if (!found)
					counter++;
				total++;
			}
			
			if(b_output->ptr.data == b_socket.ptr.data)
				found = true;
		}
		
		/* rename if needed */
		if (name == "Shader")
			name = "Closure";
		
		if (total > 1)
			name = string_printf("%s%d", name.c_str(), counter);
	}
	
	return node->output(name.c_str());
}

static void add_nodes(Scene *scene, BL::BlendData b_data, BL::Scene b_scene, ShaderGraph *graph, BL::ShaderNodeTree b_ntree,
                      const ProxyMap &proxy_input_map, const ProxyMap &proxy_output_map)
{
	/* add nodes */
	BL::ShaderNodeTree::nodes_iterator b_node;
	PtrInputMap input_map;
	PtrOutputMap output_map;
	
	BL::Node::inputs_iterator b_input;
	BL::Node::outputs_iterator b_output;

	/* find the node to use for output if there are multiple */
	bool found_active_output = false;
	BL::ShaderNode output_node(PointerRNA_NULL);

	for(b_ntree.nodes.begin(b_node); b_node != b_ntree.nodes.end(); ++b_node) {
		if (is_output_node(*b_node)) {
			BL::ShaderNodeOutputMaterial b_output_node(*b_node);

			if(b_output_node.is_active_output()) {
				output_node = b_output_node;
				found_active_output = true;
				break;
			}
			else if(!output_node.ptr.data && !found_active_output) {
				output_node = b_output_node;
			}
		}
	}

	/* add nodes */
	for(b_ntree.nodes.begin(b_node); b_node != b_ntree.nodes.end(); ++b_node) {
		if (b_node->mute() || b_node->is_a(&RNA_NodeReroute)) {
			/* replace muted node with internal links */
			BL::Node::internal_links_iterator b_link;
			for (b_node->internal_links.begin(b_link); b_link != b_node->internal_links.end(); ++b_link) {
				ProxyNode *proxy = new ProxyNode(convert_socket_type(b_link->to_socket()));
				
				input_map[b_link->from_socket().ptr.data] = proxy->inputs[0];
				output_map[b_link->to_socket().ptr.data] = proxy->outputs[0];
				
				graph->add(proxy);
			}
		}
		else if (b_node->is_a(&RNA_ShaderNodeGroup) || b_node->is_a(&RNA_NodeCustomGroup)) {
			
			BL::ShaderNodeTree b_group_ntree(PointerRNA_NULL);
			if (b_node->is_a(&RNA_ShaderNodeGroup))
				b_group_ntree = BL::ShaderNodeTree(((BL::NodeGroup)(*b_node)).node_tree());
			else
				b_group_ntree = BL::ShaderNodeTree(((BL::NodeCustomGroup)(*b_node)).node_tree());
			ProxyMap group_proxy_input_map, group_proxy_output_map;
			
			/* Add a proxy node for each socket
			 * Do this even if the node group has no internal tree,
			 * so that links have something to connect to and assert won't fail.
			 */
			for(b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
				ProxyNode *proxy = new ProxyNode(convert_socket_type(*b_input));
				graph->add(proxy);
				
				/* register the proxy node for internal binding */
				group_proxy_input_map[b_input->identifier()] = proxy;
				
				input_map[b_input->ptr.data] = proxy->inputs[0];
				
				set_default_value(proxy->inputs[0], *b_node, *b_input, b_data, b_ntree);
			}
			for(b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
				ProxyNode *proxy = new ProxyNode(convert_socket_type(*b_output));
				graph->add(proxy);
				
				/* register the proxy node for internal binding */
				group_proxy_output_map[b_output->identifier()] = proxy;
				
				output_map[b_output->ptr.data] = proxy->outputs[0];
			}
			
			if (b_group_ntree)
				add_nodes(scene, b_data, b_scene, graph, b_group_ntree, group_proxy_input_map, group_proxy_output_map);
		}
		else if (b_node->is_a(&RNA_NodeGroupInput)) {
			/* map each socket to a proxy node */
			for(b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
				ProxyMap::const_iterator proxy_it = proxy_input_map.find(b_output->identifier());
				if (proxy_it != proxy_input_map.end()) {
					ProxyNode *proxy = proxy_it->second;
					
					output_map[b_output->ptr.data] = proxy->outputs[0];
				}
			}
		}
		else if (b_node->is_a(&RNA_NodeGroupOutput)) {
			BL::NodeGroupOutput b_output_node(*b_node);
			/* only the active group output is used */
			if (b_output_node.is_active_output()) {
				/* map each socket to a proxy node */
				for(b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
					ProxyMap::const_iterator proxy_it = proxy_output_map.find(b_input->identifier());
					if (proxy_it != proxy_output_map.end()) {
						ProxyNode *proxy = proxy_it->second;
						
						input_map[b_input->ptr.data] = proxy->inputs[0];
						
						set_default_value(proxy->inputs[0], *b_node, *b_input, b_data, b_ntree);
					}
				}
			}
		}
		else {
			ShaderNode *node = NULL;

			if (is_output_node(*b_node)) {
				if (b_node->ptr.data == output_node.ptr.data) {
					node = graph->output();
				}
			}
			else {
				node = add_node(scene, b_data, b_scene, graph, b_ntree, BL::ShaderNode(*b_node));
			}
			
			if(node) {
				/* map node sockets for linking */
				for(b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
					ShaderInput *input = node_find_input_by_name(node, *b_node, *b_input);
					input_map[b_input->ptr.data] = input;
					
					set_default_value(input, *b_node, *b_input, b_data, b_ntree);
				}
				for(b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
					ShaderOutput *output = node_find_output_by_name(node, *b_node, *b_output);
					output_map[b_output->ptr.data] = output;
				}
			}
		}
	}

	/* connect nodes */
	BL::NodeTree::links_iterator b_link;

	for(b_ntree.links.begin(b_link); b_link != b_ntree.links.end(); ++b_link) {
		/* get blender link data */
		BL::NodeSocket b_from_sock = b_link->from_socket();
		BL::NodeSocket b_to_sock = b_link->to_socket();

		ShaderOutput *output = 0;
		ShaderInput *input = 0;
		
		PtrOutputMap::iterator output_it = output_map.find(b_from_sock.ptr.data);
		if (output_it != output_map.end())
			output = output_it->second;
		PtrInputMap::iterator input_it = input_map.find(b_to_sock.ptr.data);
		if (input_it != input_map.end())
			input = input_it->second;

		/* either node may be NULL when the node was not exported, typically
		 * because the node type is not supported */
		if(output && input)
			graph->connect(output, input);
	}
}

static void add_nodes(Scene *scene, BL::BlendData b_data, BL::Scene b_scene, ShaderGraph *graph, BL::ShaderNodeTree b_ntree)
{
	static const ProxyMap empty_proxy_map;
	add_nodes(scene, b_data, b_scene, graph, b_ntree, empty_proxy_map, empty_proxy_map);
}

/* Sync Materials */

void BlenderSync::sync_materials(bool update_all)
{
	shader_map.set_default(scene->shaders[scene->default_surface]);

	/* material loop */
	BL::BlendData::materials_iterator b_mat;

	for(b_data.materials.begin(b_mat); b_mat != b_data.materials.end(); ++b_mat) {
		Shader *shader;
		
		/* test if we need to sync */
		if(shader_map.sync(&shader, *b_mat) || update_all) {
			ShaderGraph *graph = new ShaderGraph();

			shader->name = b_mat->name().c_str();
			shader->pass_id = b_mat->pass_index();

			/* create nodes */
			if(b_mat->use_nodes() && b_mat->node_tree()) {
				BL::ShaderNodeTree b_ntree(b_mat->node_tree());

				add_nodes(scene, b_data, b_scene, graph, b_ntree);
			}
			else {
				ShaderNode *closure, *out;

				closure = graph->add(new DiffuseBsdfNode());
				closure->input("Color")->value = get_float3(b_mat->diffuse_color());
				out = graph->output();

				graph->connect(closure->output("BSDF"), out->input("Surface"));
			}

			/* settings */
			PointerRNA cmat = RNA_pointer_get(&b_mat->ptr, "cycles");
			shader->use_mis = get_boolean(cmat, "sample_as_light");
			shader->use_transparent_shadow = get_boolean(cmat, "use_transparent_shadow");
			shader->heterogeneous_volume = !get_boolean(cmat, "homogeneous_volume");

			shader->set_graph(graph);
			shader->tag_update(scene);
		}
	}
}

/* Sync World */

void BlenderSync::sync_world(bool update_all)
{
	Background *background = scene->background;
	Background prevbackground = *background;

	BL::World b_world = b_scene.world();

	if(world_recalc || update_all || b_world.ptr.data != world_map) {
		Shader *shader = scene->shaders[scene->default_background];
		ShaderGraph *graph = new ShaderGraph();

		/* create nodes */
		if(b_world && b_world.use_nodes() && b_world.node_tree()) {
			BL::ShaderNodeTree b_ntree(b_world.node_tree());

			add_nodes(scene, b_data, b_scene, graph, b_ntree);
			
			/* volume */
			PointerRNA cworld = RNA_pointer_get(&b_world.ptr, "cycles");
			shader->heterogeneous_volume = !get_boolean(cworld, "homogeneous_volume");
		}
		else if(b_world) {
			ShaderNode *closure, *out;

			closure = graph->add(new BackgroundNode());
			closure->input("Color")->value = get_float3(b_world.horizon_color());
			out = graph->output();

			graph->connect(closure->output("Background"), out->input("Surface"));
		}

		if(b_world) {
			/* AO */
			BL::WorldLighting b_light = b_world.light_settings();

			if(b_light.use_ambient_occlusion())
				background->ao_factor = b_light.ao_factor();
			else
				background->ao_factor = 0.0f;

			background->ao_distance = b_light.distance();

			/* visibility */
			PointerRNA cvisibility = RNA_pointer_get(&b_world.ptr, "cycles_visibility");
			uint visibility = 0;

			visibility |= get_boolean(cvisibility, "camera")? PATH_RAY_CAMERA: 0;
			visibility |= get_boolean(cvisibility, "diffuse")? PATH_RAY_DIFFUSE: 0;
			visibility |= get_boolean(cvisibility, "glossy")? PATH_RAY_GLOSSY: 0;
			visibility |= get_boolean(cvisibility, "transmission")? PATH_RAY_TRANSMIT: 0;

			background->visibility = visibility;
		}

		shader->set_graph(graph);
		shader->tag_update(scene);
		background->tag_update(scene);
	}

	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	/* when doing preview render check for BI's transparency settings,
	 * this is so because bledner's preview render routines are not able
	 * to tweak all cycles's settings depending on different circumstances
	 */
	if(b_engine.is_preview() == false)
		background->transparent = get_boolean(cscene, "film_transparent");
	else
		background->transparent = b_scene.render().alpha_mode() == BL::RenderSettings::alpha_mode_TRANSPARENT;

	background->use = render_layer.use_background;

	if(background->modified(prevbackground))
		background->tag_update(scene);
}

/* Sync Lamps */

void BlenderSync::sync_lamps(bool update_all)
{
	shader_map.set_default(scene->shaders[scene->default_light]);

	/* lamp loop */
	BL::BlendData::lamps_iterator b_lamp;

	for(b_data.lamps.begin(b_lamp); b_lamp != b_data.lamps.end(); ++b_lamp) {
		Shader *shader;
		
		/* test if we need to sync */
		if(shader_map.sync(&shader, *b_lamp) || update_all) {
			ShaderGraph *graph = new ShaderGraph();

			/* create nodes */
			if(b_lamp->use_nodes() && b_lamp->node_tree()) {
				shader->name = b_lamp->name().c_str();

				BL::ShaderNodeTree b_ntree(b_lamp->node_tree());

				add_nodes(scene, b_data, b_scene, graph, b_ntree);
			}
			else {
				ShaderNode *closure, *out;
				float strength = 1.0f;

				if(b_lamp->type() == BL::Lamp::type_POINT ||
				   b_lamp->type() == BL::Lamp::type_SPOT ||
				   b_lamp->type() == BL::Lamp::type_AREA)
				{
					strength = 100.0f;
				}

				closure = graph->add(new EmissionNode());
				closure->input("Color")->value = get_float3(b_lamp->color());
				closure->input("Strength")->value.x = strength;
				out = graph->output();

				graph->connect(closure->output("Emission"), out->input("Surface"));
			}

			shader->set_graph(graph);
			shader->tag_update(scene);
		}
	}
}

void BlenderSync::sync_shaders()
{
	/* for auto refresh images */
	bool auto_refresh_update = false;

	if(preview) {
		ImageManager *image_manager = scene->image_manager;
		int frame = b_scene.frame_current();
		auto_refresh_update = image_manager->set_animation_frame_update(frame);
	}

	shader_map.pre_sync();

	sync_world(auto_refresh_update);
	sync_lamps(auto_refresh_update);
	sync_materials(auto_refresh_update);

	/* false = don't delete unused shaders, not supported */
	shader_map.post_sync(false);
}

CCL_NAMESPACE_END

