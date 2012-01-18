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

#include "background.h"
#include "graph.h"
#include "light.h"
#include "nodes.h"
#include "scene.h"
#include "shader.h"

#include "blender_sync.h"
#include "blender_util.h"

#include "util_debug.h"

CCL_NAMESPACE_BEGIN

typedef map<void*, ShaderNode*> PtrNodeMap;
typedef pair<ShaderNode*, std::string> SocketPair;
typedef map<void*, SocketPair> PtrSockMap;

/* Find */

void BlenderSync::find_shader(BL::ID id, vector<uint>& used_shaders, int default_shader)
{
	Shader *shader = (id)? shader_map.find(id): scene->shaders[default_shader];

	for(size_t i = 0; i < scene->shaders.size(); i++) {
		if(scene->shaders[i] == shader) {
			used_shaders.push_back(i);
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
	BL::NodeSocketRGBA sock(get_node_output(b_node, name));
	return get_float3(sock.default_value());
}

static float get_node_output_value(BL::Node b_node, const string& name)
{
	BL::NodeSocketFloatNone sock(get_node_output(b_node, name));
	return sock.default_value();
}

static void get_tex_mapping(TextureMapping *mapping, BL::TexMapping b_mapping)
{
	if(!b_mapping)
		return;

	mapping->translation = get_float3(b_mapping.location());
	mapping->rotation = get_float3(b_mapping.rotation());
	mapping->scale = get_float3(b_mapping.scale());

	mapping->x_mapping = (TextureMapping::Mapping)b_mapping.mapping_x();
	mapping->y_mapping = (TextureMapping::Mapping)b_mapping.mapping_y();
	mapping->z_mapping = (TextureMapping::Mapping)b_mapping.mapping_z();
}

static void get_tex_mapping(TextureMapping *mapping, BL::ShaderNodeMapping b_mapping)
{
	if(!b_mapping)
		return;

	mapping->translation = get_float3(b_mapping.location());
	mapping->rotation = get_float3(b_mapping.rotation());
	mapping->scale = get_float3(b_mapping.scale());
}

static ShaderNode *add_node(BL::BlendData b_data, ShaderGraph *graph, BL::ShaderNode b_node)
{
	ShaderNode *node = NULL;

	switch(b_node.type()) {
		/* not supported */
		case BL::ShaderNode::type_CURVE_RGB: break;
		case BL::ShaderNode::type_CURVE_VEC: break;
		case BL::ShaderNode::type_GEOMETRY: break;
		case BL::ShaderNode::type_MATERIAL: break;
		case BL::ShaderNode::type_MATERIAL_EXT: break;
		case BL::ShaderNode::type_OUTPUT: break;
		case BL::ShaderNode::type_SCRIPT: break;
		case BL::ShaderNode::type_SQUEEZE: break;
		case BL::ShaderNode::type_TEXTURE: break;
		case BL::ShaderNode::type_VALTORGB: break;
		/* handled outside this function */
		case BL::ShaderNode::type_GROUP: break;
		/* existing blender nodes */
		case BL::ShaderNode::type_RGB: {
			ColorNode *color = new ColorNode();
			color->value = get_node_output_rgba(b_node, "Color");
			node = color;
			break;
		}
		case BL::ShaderNode::type_VALUE: {
			ValueNode *value = new ValueNode();
			value->value = get_node_output_value(b_node, "Value");
			node = value;
			break;
		}
		case BL::ShaderNode::type_CAMERA: {
			node = new CameraNode();
			break;
		}
		case BL::ShaderNode::type_INVERT: {
			node = new InvertNode();
			break;
		}
		case BL::ShaderNode::type_GAMMA: {
			node = new GammaNode();
			break;
		}
		case BL::ShaderNode::type_MIX_RGB: {
			BL::ShaderNodeMixRGB b_mix_node(b_node);
			MixNode *mix = new MixNode();
			mix->type = MixNode::type_enum[b_mix_node.blend_type()];
			node = mix;
			break;
		}
		case BL::ShaderNode::type_SEPRGB: {
			node = new SeparateRGBNode();
			break;
		}
		case BL::ShaderNode::type_COMBRGB: {
			node = new CombineRGBNode();
			break;
		}
		case BL::ShaderNode::type_HUE_SAT: {
			node = new HSVNode();
			break;
		}
		case BL::ShaderNode::type_RGBTOBW: {
			node = new ConvertNode(SHADER_SOCKET_COLOR, SHADER_SOCKET_FLOAT);
			break;
		}
		case BL::ShaderNode::type_MATH: {
			BL::ShaderNodeMath b_math_node(b_node);
			MathNode *math = new MathNode();
			math->type = MathNode::type_enum[b_math_node.operation()];
			node = math;
			break;
		}
		case BL::ShaderNode::type_VECT_MATH: {
			BL::ShaderNodeVectorMath b_vector_math_node(b_node);
			VectorMathNode *vmath = new VectorMathNode();
			vmath->type = VectorMathNode::type_enum[b_vector_math_node.operation()];
			node = vmath;
			break;
		}
		case BL::ShaderNode::type_NORMAL: {
			BL::Node::outputs_iterator out_it;
			b_node.outputs.begin(out_it);
			BL::NodeSocketVectorNone vec_sock(*out_it);

			NormalNode *norm = new NormalNode();
			norm->direction = get_float3(vec_sock.default_value());

			node = norm;
			break;
		}
		case BL::ShaderNode::type_MAPPING: {
			BL::ShaderNodeMapping b_mapping_node(b_node);
			MappingNode *mapping = new MappingNode();

			get_tex_mapping(&mapping->tex_mapping, b_mapping_node);

			node = mapping;
			break;
		}

		/* new nodes */
		case BL::ShaderNode::type_OUTPUT_MATERIAL:
		case BL::ShaderNode::type_OUTPUT_WORLD:
		case BL::ShaderNode::type_OUTPUT_LAMP: {
			node = graph->output();
			break;
		}
		case BL::ShaderNode::type_FRESNEL: {
			node = new FresnelNode();
			break;
		}
		case BL::ShaderNode::type_LAYER_WEIGHT: {
			node = new LayerWeightNode();
			break;
		}
		case BL::ShaderNode::type_ADD_SHADER: {
			node = new AddClosureNode();
			break;
		}
		case BL::ShaderNode::type_MIX_SHADER: {
			node = new MixClosureNode();
			break;
		}
		case BL::ShaderNode::type_ATTRIBUTE: {
			BL::ShaderNodeAttribute b_attr_node(b_node);
			AttributeNode *attr = new AttributeNode();
			attr->attribute = b_attr_node.attribute_name();
			node = attr;
			break;
		}
		case BL::ShaderNode::type_BACKGROUND: {
			node = new BackgroundNode();
			break;
		}
		case BL::ShaderNode::type_HOLDOUT: {
			node = new HoldoutNode();
			break;
		}
		case BL::ShaderNode::type_BSDF_DIFFUSE: {
			node = new DiffuseBsdfNode();
			break;
		}
		case BL::ShaderNode::type_BSDF_GLOSSY: {
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
			break;
		}
		case BL::ShaderNode::type_BSDF_GLASS: {
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
			break;
		}
		case BL::ShaderNode::type_BSDF_TRANSLUCENT: {
			node = new TranslucentBsdfNode();
			break;
		}
		case BL::ShaderNode::type_BSDF_TRANSPARENT: {
			node = new TransparentBsdfNode();
			break;
		}
		case BL::ShaderNode::type_BSDF_VELVET: {
			node = new VelvetBsdfNode();
			break;
		}
		case BL::ShaderNode::type_EMISSION: {
			node = new EmissionNode();
			break;
		}
		case BL::ShaderNode::type_VOLUME_ISOTROPIC: {
			node = new IsotropicVolumeNode();
			break;
		}
		case BL::ShaderNode::type_VOLUME_TRANSPARENT: {
			node = new TransparentVolumeNode();
			break;
		}
		case BL::ShaderNode::type_NEW_GEOMETRY: {
			node = new GeometryNode();
			break;
		}
		case BL::ShaderNode::type_LIGHT_PATH: {
			node = new LightPathNode();
			break;
		}
		case BL::ShaderNode::type_TEX_IMAGE: {
			BL::ShaderNodeTexImage b_image_node(b_node);
			BL::Image b_image(b_image_node.image());
			ImageTextureNode *image = new ImageTextureNode();
			/* todo: handle generated/builtin images */
			if(b_image)
				image->filename = blender_absolute_path(b_data, b_image, b_image.filepath());
			image->color_space = ImageTextureNode::color_space_enum[(int)b_image_node.color_space()];
			get_tex_mapping(&image->tex_mapping, b_image_node.texture_mapping());
			node = image;
			break;
		}
		case BL::ShaderNode::type_TEX_ENVIRONMENT: {
			BL::ShaderNodeTexEnvironment b_env_node(b_node);
			BL::Image b_image(b_env_node.image());
			EnvironmentTextureNode *env = new EnvironmentTextureNode();
			if(b_image)
				env->filename = blender_absolute_path(b_data, b_image, b_image.filepath());
			env->color_space = EnvironmentTextureNode::color_space_enum[(int)b_env_node.color_space()];
			get_tex_mapping(&env->tex_mapping, b_env_node.texture_mapping());
			node = env;
			break;
		}
		case BL::ShaderNode::type_TEX_GRADIENT: {
			BL::ShaderNodeTexGradient b_gradient_node(b_node);
			GradientTextureNode *gradient = new GradientTextureNode();
			gradient->type = GradientTextureNode::type_enum[(int)b_gradient_node.gradient_type()];
			get_tex_mapping(&gradient->tex_mapping, b_gradient_node.texture_mapping());
			node = gradient;
			break;
		}
		case BL::ShaderNode::type_TEX_VORONOI: {
			BL::ShaderNodeTexVoronoi b_voronoi_node(b_node);
			VoronoiTextureNode *voronoi = new VoronoiTextureNode();
			voronoi->coloring = VoronoiTextureNode::coloring_enum[(int)b_voronoi_node.coloring()];
			get_tex_mapping(&voronoi->tex_mapping, b_voronoi_node.texture_mapping());
			node = voronoi;
			break;
		}
		case BL::ShaderNode::type_TEX_MAGIC: {
			BL::ShaderNodeTexMagic b_magic_node(b_node);
			MagicTextureNode *magic = new MagicTextureNode();
			magic->depth = b_magic_node.turbulence_depth();
			get_tex_mapping(&magic->tex_mapping, b_magic_node.texture_mapping());
			node = magic;
			break;
		}
		case BL::ShaderNode::type_TEX_WAVE: {
			BL::ShaderNodeTexWave b_wave_node(b_node);
			WaveTextureNode *wave = new WaveTextureNode();
			wave->type = WaveTextureNode::type_enum[(int)b_wave_node.wave_type()];
			get_tex_mapping(&wave->tex_mapping, b_wave_node.texture_mapping());
			node = wave;
			break;
		}
		case BL::ShaderNode::type_TEX_CHECKER: {
			BL::ShaderNodeTexChecker b_checker_node(b_node);
			CheckerTextureNode *checker = new CheckerTextureNode();
			get_tex_mapping(&checker->tex_mapping, b_checker_node.texture_mapping());
			node = checker;
			break;
		}
		case BL::ShaderNode::type_TEX_NOISE: {
			BL::ShaderNodeTexNoise b_noise_node(b_node);
			NoiseTextureNode *noise = new NoiseTextureNode();
			get_tex_mapping(&noise->tex_mapping, b_noise_node.texture_mapping());
			node = noise;
			break;
		}
		case BL::ShaderNode::type_TEX_MUSGRAVE: {
			BL::ShaderNodeTexMusgrave b_musgrave_node(b_node);
			MusgraveTextureNode *musgrave = new MusgraveTextureNode();
			musgrave->type = MusgraveTextureNode::type_enum[(int)b_musgrave_node.musgrave_type()];
			get_tex_mapping(&musgrave->tex_mapping, b_musgrave_node.texture_mapping());
			node = musgrave;
			break;
		}
		case BL::ShaderNode::type_TEX_COORD: {
			node = new TextureCoordinateNode();;
			break;
		}
		case BL::ShaderNode::type_TEX_SKY: {
			BL::ShaderNodeTexSky b_sky_node(b_node);
			SkyTextureNode *sky = new SkyTextureNode();
			sky->sun_direction = get_float3(b_sky_node.sun_direction());
			sky->turbidity = b_sky_node.turbidity();
			get_tex_mapping(&sky->tex_mapping, b_sky_node.texture_mapping());
			node = sky;
			break;
		}
	}

	if(node && node != graph->output())
		graph->add(node);

	return node;
}

static SocketPair node_socket_map_pair(PtrNodeMap& node_map, BL::Node b_node, BL::NodeSocket b_socket)
{
	BL::Node::inputs_iterator b_input;
	BL::Node::outputs_iterator b_output;
	string name = b_socket.name();
	bool found = false;
	int counter = 0, total = 0;

	/* find in inputs */
	for(b_node.inputs.begin(b_input); b_input != b_node.inputs.end(); ++b_input) {
		if(b_input->name() == name) {
			if(!found)
				counter++;
			total++;
		}

		if(b_input->ptr.data == b_socket.ptr.data)
			found = true;
	}

	if(!found) {
		/* find in outputs */
		found = false;
		counter = 0;
		total = 0;

		for(b_node.outputs.begin(b_output); b_output != b_node.outputs.end(); ++b_output) {
			if(b_output->name() == name) {
				if(!found)
					counter++;
				total++;
			}

			if(b_output->ptr.data == b_socket.ptr.data)
				found = true;
		}
	}

	/* rename if needed */
	if(name == "Shader")
		name = "Closure";

	if(total > 1)
		name = string_printf("%s%d", name.c_str(), counter);

	return SocketPair(node_map[b_node.ptr.data], name);
}

static ShaderSocketType convert_socket_type(BL::NodeSocket::type_enum b_type)
{
	switch (b_type) {
	case BL::NodeSocket::type_VALUE:
		return SHADER_SOCKET_FLOAT;
	case BL::NodeSocket::type_VECTOR:
		return SHADER_SOCKET_VECTOR;
	case BL::NodeSocket::type_RGBA:
		return SHADER_SOCKET_COLOR;
	case BL::NodeSocket::type_SHADER:
		return SHADER_SOCKET_CLOSURE;
	
	case BL::NodeSocket::type_BOOLEAN:
	case BL::NodeSocket::type_MESH:
	case BL::NodeSocket::type_INT:
	default:
		return SHADER_SOCKET_FLOAT;
	}
}

static void set_default_value(ShaderInput *input, BL::NodeSocket sock)
{
	/* copy values for non linked inputs */
	switch(input->type) {
	case SHADER_SOCKET_FLOAT: {
		BL::NodeSocketFloatNone value_sock(sock);
		input->set(value_sock.default_value());
		break;
	}
	case SHADER_SOCKET_COLOR: {
		BL::NodeSocketRGBA rgba_sock(sock);
		input->set(get_float3(rgba_sock.default_value()));
		break;
	}
	case SHADER_SOCKET_NORMAL:
	case SHADER_SOCKET_POINT:
	case SHADER_SOCKET_VECTOR: {
		BL::NodeSocketVectorNone vec_sock(sock);
		input->set(get_float3(vec_sock.default_value()));
		break;
	}
	case SHADER_SOCKET_CLOSURE:
		break;
	}
}

static void add_nodes(BL::BlendData b_data, ShaderGraph *graph, BL::ShaderNodeTree b_ntree, PtrSockMap& sockets_map)
{
	/* add nodes */
	BL::ShaderNodeTree::nodes_iterator b_node;
	PtrNodeMap node_map;
	PtrSockMap proxy_map;

	for(b_ntree.nodes.begin(b_node); b_node != b_ntree.nodes.end(); ++b_node) {
		if(b_node->is_a(&RNA_NodeGroup)) {
			/* add proxy converter nodes for inputs and outputs */
			BL::NodeGroup b_gnode(*b_node);
			BL::ShaderNodeTree b_group_ntree(b_gnode.node_tree());
			BL::Node::inputs_iterator b_input;
			BL::Node::outputs_iterator b_output;
			
			PtrSockMap group_sockmap;
			
			for(b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
				ShaderSocketType extern_type = convert_socket_type(b_input->type());
				ShaderSocketType intern_type = convert_socket_type(b_input->group_socket().type());
				ShaderNode *proxy = graph->add(new ProxyNode(extern_type, intern_type));
				
				/* map the external node socket to the proxy node socket */
				proxy_map[b_input->ptr.data] = SocketPair(proxy, proxy->inputs[0]->name);
				/* map the internal group socket to the proxy node socket */
				group_sockmap[b_input->group_socket().ptr.data] = SocketPair(proxy, proxy->outputs[0]->name);
				
				/* default input values of the group node */
				set_default_value(proxy->inputs[0], *b_input);
			}
			
			for(b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
				ShaderSocketType extern_type = convert_socket_type(b_output->type());
				ShaderSocketType intern_type = convert_socket_type(b_output->group_socket().type());
				ShaderNode *proxy = graph->add(new ProxyNode(intern_type, extern_type));
				
				/* map the external node socket to the proxy node socket */
				proxy_map[b_output->ptr.data] = SocketPair(proxy, proxy->outputs[0]->name);
				/* map the internal group socket to the proxy node socket */
				group_sockmap[b_output->group_socket().ptr.data] = SocketPair(proxy, proxy->inputs[0]->name);
				
				/* default input values of internal, unlinked group outputs */
				set_default_value(proxy->inputs[0], b_output->group_socket());
			}
			
			add_nodes(b_data, graph, b_group_ntree, group_sockmap);
		}
		else {
			ShaderNode *node = add_node(b_data, graph, BL::ShaderNode(*b_node));
			
			if(node) {
				BL::Node::inputs_iterator b_input;
				
				node_map[b_node->ptr.data] = node;
				
				for(b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
					SocketPair pair = node_socket_map_pair(node_map, *b_node, *b_input);
					ShaderInput *input = pair.first->input(pair.second.c_str());
					
					assert(input);
					
					/* copy values for non linked inputs */
					set_default_value(input, *b_input);
				}
			}
		}
	}

	/* connect nodes */
	BL::NodeTree::links_iterator b_link;

	for(b_ntree.links.begin(b_link); b_link != b_ntree.links.end(); ++b_link) {
		/* get blender link data */
		BL::Node b_from_node = b_link->from_node();
		BL::Node b_to_node = b_link->to_node();

		BL::NodeSocket b_from_sock = b_link->from_socket();
		BL::NodeSocket b_to_sock = b_link->to_socket();

		SocketPair from_pair, to_pair;

		/* links without a node pointer are connections to group inputs/outputs */

		/* from sock */
		if(b_from_node) {
			if (b_from_node.is_a(&RNA_NodeGroup))
				from_pair = proxy_map[b_from_sock.ptr.data];
			else
				from_pair = node_socket_map_pair(node_map, b_from_node, b_from_sock);
		}
		else
			from_pair = sockets_map[b_from_sock.ptr.data];

		/* to sock */
		if(b_to_node) {
			if (b_to_node.is_a(&RNA_NodeGroup))
				to_pair = proxy_map[b_to_sock.ptr.data];
			else
				to_pair = node_socket_map_pair(node_map, b_to_node, b_to_sock);
		}
		else
			to_pair = sockets_map[b_to_sock.ptr.data];

		/* either node may be NULL when the node was not exported, typically
		   because the node type is not supported */
		if(from_pair.first && to_pair.first) {
			ShaderOutput *output = from_pair.first->output(from_pair.second.c_str());
			ShaderInput *input = to_pair.first->input(to_pair.second.c_str());

			graph->connect(output, input);
		}
	}
}

/* Sync Materials */

void BlenderSync::sync_materials()
{
	shader_map.set_default(scene->shaders[scene->default_surface]);

	/* material loop */
	BL::BlendData::materials_iterator b_mat;

	for(b_data.materials.begin(b_mat); b_mat != b_data.materials.end(); ++b_mat) {
		Shader *shader;
		
		/* test if we need to sync */
		if(shader_map.sync(&shader, *b_mat)) {
			ShaderGraph *graph = new ShaderGraph();

			shader->name = b_mat->name().c_str();

			/* create nodes */
			if(b_mat->use_nodes() && b_mat->node_tree()) {
				PtrSockMap sock_to_node;
				BL::ShaderNodeTree b_ntree(b_mat->node_tree());

				add_nodes(b_data, graph, b_ntree, sock_to_node);
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
			shader->sample_as_light = get_boolean(cmat, "sample_as_light");
			shader->homogeneous_volume = get_boolean(cmat, "homogeneous_volume");

			shader->set_graph(graph);
			shader->tag_update(scene);
		}
	}
}

/* Sync World */

void BlenderSync::sync_world()
{
	Background *background = scene->background;
	Background prevbackground = *background;

	BL::World b_world = b_scene.world();

	if(world_recalc || b_world.ptr.data != world_map) {
		Shader *shader = scene->shaders[scene->default_background];
		ShaderGraph *graph = new ShaderGraph();

		/* create nodes */
		if(b_world && b_world.use_nodes() && b_world.node_tree()) {
			PtrSockMap sock_to_node;
			BL::ShaderNodeTree b_ntree(b_world.node_tree());

			add_nodes(b_data, graph, b_ntree, sock_to_node);
		}
		else if(b_world) {
			ShaderNode *closure, *out;

			closure = graph->add(new BackgroundNode());
			closure->input("Color")->value = get_float3(b_world.horizon_color());
			out = graph->output();

			graph->connect(closure->output("Background"), out->input("Surface"));
		}

		shader->set_graph(graph);
		shader->tag_update(scene);
	}

	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	background->transparent = get_boolean(cscene, "film_transparent");

	if(background->modified(prevbackground))
		background->tag_update(scene);

	world_map = b_world.ptr.data;
	world_recalc = false;
}

/* Sync Lamps */

void BlenderSync::sync_lamps()
{
	shader_map.set_default(scene->shaders[scene->default_light]);

	/* lamp loop */
	BL::BlendData::lamps_iterator b_lamp;

	for(b_data.lamps.begin(b_lamp); b_lamp != b_data.lamps.end(); ++b_lamp) {
		Shader *shader;
		
		/* test if we need to sync */
		if(shader_map.sync(&shader, *b_lamp)) {
			ShaderGraph *graph = new ShaderGraph();

			/* create nodes */
			if(b_lamp->use_nodes() && b_lamp->node_tree()) {
				shader->name = b_lamp->name().c_str();

				PtrSockMap sock_to_node;
				BL::ShaderNodeTree b_ntree(b_lamp->node_tree());

				add_nodes(b_data, graph, b_ntree, sock_to_node);
			}
			else {
				ShaderNode *closure, *out;
				float strength = 1.0f;

				if(b_lamp->type() == BL::Lamp::type_POINT ||
				   b_lamp->type() == BL::Lamp::type_SPOT ||
				   b_lamp->type() == BL::Lamp::type_AREA)
					strength = 100.0f;

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
	shader_map.pre_sync();

	sync_world();
	sync_lamps();
	sync_materials();

	/* false = don't delete unused shaders, not supported */
	shader_map.post_sync(false);
}

CCL_NAMESPACE_END

