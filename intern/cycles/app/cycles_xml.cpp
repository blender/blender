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

#include <stdio.h>

#include <sstream>
#include <algorithm>
#include <iterator>

#include "camera.h"
#include "film.h"
#include "graph.h"
#include "integrator.h"
#include "light.h"
#include "mesh.h"
#include "nodes.h"
#include "object.h"
#include "shader.h"
#include "scene.h"

#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_split.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_path.h"
#include "util_transform.h"
#include "util_xml.h"

#include "cycles_xml.h"

CCL_NAMESPACE_BEGIN

/* XML reading state */

struct XMLReadState {
	Scene *scene;		/* scene pointer */
	Transform tfm;		/* current transform state */
	bool smooth;		/* smooth normal state */
	int shader;			/* current shader */
	string base;		/* base path to current file*/
	float dicing_rate;	/* current dicing rate */
	Mesh::DisplacementMethod displacement_method;
};

/* Attribute Reading */

static bool xml_read_bool(bool *value, pugi::xml_node node, const char *name)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr) {
		*value = (string_iequals(attr.value(), "true")) || (atoi(attr.value()) != 0);
		return true;
	}

	return false;
}

static bool xml_read_int(int *value, pugi::xml_node node, const char *name)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr) {
		*value = atoi(attr.value());
		return true;
	}

	return false;
}

static bool xml_read_int_array(vector<int>& value, pugi::xml_node node, const char *name)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr) {
		vector<string> tokens;
		string_split(tokens, attr.value());

		foreach(const string& token, tokens)
			value.push_back(atoi(token.c_str()));

		return true;
	}

	return false;
}

static bool xml_read_float(float *value, pugi::xml_node node, const char *name)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr) {
		*value = atof(attr.value());
		return true;
	}

	return false;
}

static bool xml_read_float_array(vector<float>& value, pugi::xml_node node, const char *name)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr) {
		vector<string> tokens;
		string_split(tokens, attr.value());

		foreach(const string& token, tokens)
			value.push_back(atof(token.c_str()));

		return true;
	}

	return false;
}

static bool xml_read_float3(float3 *value, pugi::xml_node node, const char *name)
{
	vector<float> array;

	if(xml_read_float_array(array, node, name) && array.size() == 3) {
		*value = make_float3(array[0], array[1], array[2]);
		return true;
	}

	return false;
}

static bool xml_read_float3_array(vector<float3>& value, pugi::xml_node node, const char *name)
{
	vector<float> array;

	if(xml_read_float_array(array, node, name)) {
		for(size_t i = 0; i < array.size(); i += 3)
			value.push_back(make_float3(array[i+0], array[i+1], array[i+2]));

		return true;
	}

	return false;
}

static bool xml_read_float4(float4 *value, pugi::xml_node node, const char *name)
{
	vector<float> array;

	if(xml_read_float_array(array, node, name) && array.size() == 4) {
		*value = make_float4(array[0], array[1], array[2], array[3]);
		return true;
	}

	return false;
}

static bool xml_read_string(string *str, pugi::xml_node node, const char *name)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr) {
		*str = attr.value();
		return true;
	}

	return false;
}

static bool xml_read_ustring(ustring *str, pugi::xml_node node, const char *name)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr) {
		*str = ustring(attr.value());
		return true;
	}

	return false;
}

static bool xml_equal_string(pugi::xml_node node, const char *name, const char *value)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr)
		return string_iequals(attr.value(), value);
	
	return false;
}

static bool xml_read_enum(ustring *str, ShaderEnum& enm, pugi::xml_node node, const char *name)
{
	pugi::xml_attribute attr = node.attribute(name);

	if(attr) {
		ustring ustr(attr.value());

		if(enm.exists(ustr)) {
			*str = ustr;
			return true;
		}
		else
			fprintf(stderr, "Unknown value \"%s\" for attribute \"%s\".\n", ustr.c_str(), name);
	}

	return false;
}

/* Film */

static void xml_read_film(const XMLReadState& state, pugi::xml_node node)
{
	Camera *cam = state.scene->camera;

	xml_read_int(&cam->width, node, "width");
	xml_read_int(&cam->height, node, "height");

	float aspect = (float)cam->width/(float)cam->height;

	if(cam->width >= cam->height) {
		cam->left = -aspect;
		cam->right = aspect;
		cam->bottom = -1.0f;
		cam->top = 1.0f;
	}
	else {
		cam->left = -1.0f;
		cam->right = 1.0f;
		cam->bottom = -1.0f/aspect;
		cam->top = 1.0f/aspect;
	}

	cam->need_update = true;
	cam->update();
}

/* Integrator */

static void xml_read_integrator(const XMLReadState& state, pugi::xml_node node)
{
	Integrator *integrator = state.scene->integrator;

	xml_read_int(&integrator->min_bounce, node, "min_bounce");
	xml_read_int(&integrator->max_bounce, node, "max_bounce");
	
	xml_read_int(&integrator->max_diffuse_bounce, node, "max_diffuse_bounce");
	xml_read_int(&integrator->max_glossy_bounce, node, "max_glossy_bounce");
	xml_read_int(&integrator->max_transmission_bounce, node, "max_transmission_bounce");
	
	xml_read_int(&integrator->transparent_min_bounce, node, "transparent_min_bounce");
	xml_read_int(&integrator->transparent_max_bounce, node, "transparent_max_bounce");
	
	xml_read_bool(&integrator->transparent_shadows, node, "transparent_shadows");
	xml_read_bool(&integrator->no_caustics, node, "no_caustics");
	
	xml_read_int(&integrator->seed, node, "seed");
}

/* Camera */

static void xml_read_camera(const XMLReadState& state, pugi::xml_node node)
{
	Camera *cam = state.scene->camera;

	if(xml_read_float(&cam->fov, node, "fov"))
		cam->fov *= M_PI/180.0f;

	xml_read_float(&cam->nearclip, node, "nearclip");
	xml_read_float(&cam->farclip, node, "farclip");
	xml_read_float(&cam->aperturesize, node, "aperturesize"); // 0.5*focallength/fstop
	xml_read_float(&cam->focaldistance, node, "focaldistance");
	xml_read_float(&cam->shuttertime, node, "shuttertime");

	if(xml_equal_string(node, "type", "orthographic"))
		cam->type = CAMERA_ORTHOGRAPHIC;
	else if(xml_equal_string(node, "type", "perspective"))
		cam->type = CAMERA_PERSPECTIVE;
	else if(xml_equal_string(node, "type", "environment"))
		cam->type = CAMERA_ENVIRONMENT;

	cam->matrix = state.tfm;

	cam->need_update = true;
	cam->update();
}

/* Shader */

static string xml_socket_name(const char *name)
{
	string sname = name;
	size_t i;

	while((i = sname.find(" ")) != string::npos)
		sname.replace(i, 1, "");
	
	return sname;
}

static void xml_read_shader_graph(const XMLReadState& state, Shader *shader, pugi::xml_node graph_node)
{
	ShaderGraph *graph = new ShaderGraph();

	map<string, ShaderNode*> nodemap;

	nodemap["output"] = graph->output();

	for(pugi::xml_node node = graph_node.first_child(); node; node = node.next_sibling()) {
		ShaderNode *snode = NULL;

		if(string_iequals(node.name(), "image_texture")) {
			ImageTextureNode *img = new ImageTextureNode();

			xml_read_string(&img->filename, node, "src");
			img->filename = path_join(state.base, img->filename);

			snode = img;
		}
		else if(string_iequals(node.name(), "environment_texture")) {
			EnvironmentTextureNode *env = new EnvironmentTextureNode();

			xml_read_string(&env->filename, node, "src");
			env->filename = path_join(state.base, env->filename);

			snode = env;
		}
		else if(string_iequals(node.name(), "sky_texture")) {
			SkyTextureNode *sky = new SkyTextureNode();

			xml_read_float3(&sky->sun_direction, node, "sun_direction");
			xml_read_float(&sky->turbidity, node, "turbidity");
			
			snode = sky;
		}
		else if(string_iequals(node.name(), "noise_texture")) {
			snode = new NoiseTextureNode();
		}
		else if(string_iequals(node.name(), "checker_texture")) {
			snode = new CheckerTextureNode();
		}
		else if(string_iequals(node.name(), "gradient_texture")) {
			GradientTextureNode *blend = new GradientTextureNode();
			xml_read_enum(&blend->type, GradientTextureNode::type_enum, node, "type");
			snode = blend;
		}
		else if(string_iequals(node.name(), "voronoi_texture")) {
			VoronoiTextureNode *voronoi = new VoronoiTextureNode();
			xml_read_enum(&voronoi->coloring, VoronoiTextureNode::coloring_enum, node, "coloring");
			snode = voronoi;
		}
		else if(string_iequals(node.name(), "musgrave_texture")) {
			MusgraveTextureNode *musgrave = new MusgraveTextureNode();
			xml_read_enum(&musgrave->type, MusgraveTextureNode::type_enum, node, "type");
			snode = musgrave;
		}
		else if(string_iequals(node.name(), "magic_texture")) {
			MagicTextureNode *magic = new MagicTextureNode();
			xml_read_int(&magic->depth, node, "depth");
			snode = magic;
		}
		else if(string_iequals(node.name(), "noise_texture")) {
			NoiseTextureNode *dist = new NoiseTextureNode();
			snode = dist;
		}
		else if(string_iequals(node.name(), "wave_texture")) {
			WaveTextureNode *wood = new WaveTextureNode();
			xml_read_enum(&wood->type, WaveTextureNode::type_enum, node, "type");
			snode = wood;
		}
		else if(string_iequals(node.name(), "normal")) {
			snode = new NormalNode();
		}
		else if(string_iequals(node.name(), "mapping")) {
			snode = new MappingNode();
		}
		else if(string_iequals(node.name(), "ward_bsdf")) {
			snode = new WardBsdfNode();
		}
		else if(string_iequals(node.name(), "diffuse_bsdf")) {
			snode = new DiffuseBsdfNode();
		}
		else if(string_iequals(node.name(), "translucent_bsdf")) {
			snode = new TranslucentBsdfNode();
		}
		else if(string_iequals(node.name(), "transparent_bsdf")) {
			snode = new TransparentBsdfNode();
		}
		else if(string_iequals(node.name(), "velvet_bsdf")) {
			snode = new VelvetBsdfNode();
		}
		else if(string_iequals(node.name(), "glossy_bsdf")) {
			GlossyBsdfNode *glossy = new GlossyBsdfNode();
			xml_read_enum(&glossy->distribution, GlossyBsdfNode::distribution_enum, node, "distribution");
			snode = glossy;
		}
		else if(string_iequals(node.name(), "glass_bsdf")) {
			GlassBsdfNode *diel = new GlassBsdfNode();
			xml_read_enum(&diel->distribution, GlassBsdfNode::distribution_enum, node, "distribution");
			snode = diel;
		}
		else if(string_iequals(node.name(), "emission")) {
			EmissionNode *emission = new EmissionNode();
			xml_read_bool(&emission->total_power, node, "total_power");
			snode = emission;
		}
		else if(string_iequals(node.name(), "background")) {
			snode = new BackgroundNode();
		}
		else if(string_iequals(node.name(), "transparent_volume")) {
			snode = new TransparentVolumeNode();
		}
		else if(string_iequals(node.name(), "isotropic_volume")) {
			snode = new IsotropicVolumeNode();
		}
		else if(string_iequals(node.name(), "geometry")) {
			snode = new GeometryNode();
		}
		else if(string_iequals(node.name(), "texture_coordinate")) {
			snode = new TextureCoordinateNode();
		}
		else if(string_iequals(node.name(), "lightPath")) {
			snode = new LightPathNode();
		}
		else if(string_iequals(node.name(), "value")) {
			ValueNode *value = new ValueNode();
			xml_read_float(&value->value, node, "value");
			snode = value;
		}
		else if(string_iequals(node.name(), "color")) {
			ColorNode *color = new ColorNode();
			xml_read_float3(&color->value, node, "value");
			snode = color;
		}
		else if(string_iequals(node.name(), "mix_closure")) {
			snode = new MixClosureNode();
		}
		else if(string_iequals(node.name(), "add_closure")) {
			snode = new AddClosureNode();
		}
		else if(string_iequals(node.name(), "invert")) {
			snode = new InvertNode();
		}
		else if(string_iequals(node.name(), "mix")) {
			MixNode *mix = new MixNode();
			xml_read_enum(&mix->type, MixNode::type_enum, node, "type");
			snode = mix;
		}
		else if(string_iequals(node.name(), "gamma")) {
			snode = new GammaNode();
		}
		else if(string_iequals(node.name(), "brightness")) {
			snode = new BrightContrastNode();
		}
		else if(string_iequals(node.name(), "combine_rgb")) {
			snode = new CombineRGBNode();
		}
		else if(string_iequals(node.name(), "separate_rgb")) {
			snode = new SeparateRGBNode();
		}
		else if(string_iequals(node.name(), "hsv")) {
			snode = new HSVNode();
		}
		else if(string_iequals(node.name(), "attribute")) {
			AttributeNode *attr = new AttributeNode();
			xml_read_ustring(&attr->attribute, node, "attribute");
			snode = attr;
		}
		else if(string_iequals(node.name(), "camera")) {
			snode = new CameraNode();
		}
		else if(string_iequals(node.name(), "fresnel")) {
			snode = new FresnelNode();
		}
		else if(string_iequals(node.name(), "math")) {
			MathNode *math = new MathNode();
			xml_read_enum(&math->type, MathNode::type_enum, node, "type");
			snode = math;
		}
		else if(string_iequals(node.name(), "vector_math")) {
			VectorMathNode *vmath = new VectorMathNode();
			xml_read_enum(&vmath->type, VectorMathNode::type_enum, node, "type");
			snode = vmath;
		}
		else if(string_iequals(node.name(), "connect")) {
			/* connect nodes */
			vector<string> from_tokens, to_tokens;

			string_split(from_tokens, node.attribute("from").value());
			string_split(to_tokens, node.attribute("to").value());

			if(from_tokens.size() == 2 && to_tokens.size() == 2) {
				/* find nodes and sockets */
				ShaderOutput *output = NULL;
				ShaderInput *input = NULL;

				if(nodemap.find(from_tokens[0]) != nodemap.end()) {
					ShaderNode *fromnode = nodemap[from_tokens[0]];

					foreach(ShaderOutput *out, fromnode->outputs)
						if(string_iequals(xml_socket_name(out->name), from_tokens[1]))
							output = out;

					if(!output)
						fprintf(stderr, "Unknown output socket name \"%s\" on \"%s\".\n", from_tokens[1].c_str(), from_tokens[0].c_str());
				}
				else
					fprintf(stderr, "Unknown shader node name \"%s\".\n", from_tokens[0].c_str());

				if(nodemap.find(to_tokens[0]) != nodemap.end()) {
					ShaderNode *tonode = nodemap[to_tokens[0]];

					foreach(ShaderInput *in, tonode->inputs)
						if(string_iequals(xml_socket_name(in->name), to_tokens[1]))
							input = in;

					if(!input)
						fprintf(stderr, "Unknown input socket name \"%s\" on \"%s\".\n", to_tokens[1].c_str(), to_tokens[0].c_str());
				}
				else
					fprintf(stderr, "Unknown shader node name \"%s\".\n", to_tokens[0].c_str());

				/* connect */
				if(output && input)
					graph->connect(output, input);
			}
			else
				fprintf(stderr, "Invalid from or to value for connect node.\n");
		}
		else
			fprintf(stderr, "Unknown shader node \"%s\".\n", node.name());

		if(snode) {
			/* add to graph */
			graph->add(snode);

			/* add to map for name lookups */
			string name = "";
			xml_read_string(&name, node, "name");

			nodemap[name] = snode;

			/* read input values */
			for(pugi::xml_attribute attr = node.first_attribute(); attr; attr = attr.next_attribute()) {
				foreach(ShaderInput *in, snode->inputs) {
					if(string_iequals(in->name, attr.name())) {
						switch(in->type) {
							case SHADER_SOCKET_FLOAT:
								xml_read_float(&in->value.x, node, attr.name());
								break;
							case SHADER_SOCKET_COLOR:
							case SHADER_SOCKET_VECTOR:
							case SHADER_SOCKET_POINT:
							case SHADER_SOCKET_NORMAL:
								xml_read_float3(&in->value, node, attr.name());
								break;
							default:
								break;
						}
					}
				}
			}
		}
	}

	shader->set_graph(graph);
	shader->tag_update(state.scene);
}

static void xml_read_shader(const XMLReadState& state, pugi::xml_node node)
{
	Shader *shader = new Shader();
	xml_read_string(&shader->name, node, "name");
	xml_read_shader_graph(state, shader, node);
	state.scene->shaders.push_back(shader);
}

/* Background */

static void xml_read_background(const XMLReadState& state, pugi::xml_node node)
{
	Shader *shader = state.scene->shaders[state.scene->default_background];

	xml_read_shader_graph(state, shader, node);
}

/* Mesh */

static Mesh *xml_add_mesh(Scene *scene, const Transform& tfm)
{
	/* create mesh */
	Mesh *mesh = new Mesh();
	scene->meshes.push_back(mesh);

	/* create object*/
	Object *object = new Object();
	object->mesh = mesh;
	object->tfm = tfm;
	scene->objects.push_back(object);

	return mesh;
}

static void xml_read_mesh(const XMLReadState& state, pugi::xml_node node)
{
	/* add mesh */
	Mesh *mesh = xml_add_mesh(state.scene, state.tfm);
	mesh->used_shaders.push_back(state.shader);

	/* read state */
	int shader = state.shader;
	bool smooth = state.smooth;

	mesh->displacement_method = state.displacement_method;

	/* read vertices and polygons, RIB style */
	vector<float3> P;
	vector<int> verts, nverts;

	xml_read_float3_array(P, node, "P");
	xml_read_int_array(verts, node, "verts");
	xml_read_int_array(nverts, node, "nverts");

	if(xml_equal_string(node, "subdivision", "catmull-clark")) {
		/* create subd mesh */
		SubdMesh sdmesh;

		/* create subd vertices */
		for(size_t i = 0; i < P.size(); i++)
			sdmesh.add_vert(P[i]);

		/* create subd faces */
		int index_offset = 0;

		for(size_t i = 0; i < nverts.size(); i++) {
			if(nverts[i] == 4) {
				int v0 = verts[index_offset + 0];
				int v1 = verts[index_offset + 1];
				int v2 = verts[index_offset + 2];
				int v3 = verts[index_offset + 3];

				sdmesh.add_face(v0, v1, v2, v3);
			}
			else {
				for(int j = 0; j < nverts[i]-2; j++) {
					int v0 = verts[index_offset];
					int v1 = verts[index_offset + j + 1];
					int v2 = verts[index_offset + j + 2];;

					sdmesh.add_face(v0, v1, v2);
				}
			}

			index_offset += nverts[i];
		}

		/* finalize subd mesh */
		sdmesh.link_boundary();

		/* subdivide */
		DiagSplit dsplit;
		//dsplit.camera = state.scene->camera;
		//dsplit.dicing_rate = 5.0f;
		dsplit.dicing_rate = state.dicing_rate;
		xml_read_float(&dsplit.dicing_rate, node, "dicing_rate");
		sdmesh.tessellate(&dsplit, false, mesh, shader, smooth);
	}
	else {
		/* create vertices */
		mesh->verts = P;

		/* create triangles */
		int index_offset = 0;

		for(size_t i = 0; i < nverts.size(); i++) {
			for(int j = 0; j < nverts[i]-2; j++) {
				int v0 = verts[index_offset];
				int v1 = verts[index_offset + j + 1];
				int v2 = verts[index_offset + j + 2];

				assert(v0 < (int)P.size());
				assert(v1 < (int)P.size());
				assert(v2 < (int)P.size());

				mesh->add_triangle(v0, v1, v2, shader, smooth);
			}

			index_offset += nverts[i];
		}
	}

	/* temporary for test compatibility */
	mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
}

/* Patch */

static void xml_read_patch(const XMLReadState& state, pugi::xml_node node)
{
	/* read patch */
	Patch *patch = NULL;

	vector<float3> P;
	xml_read_float3_array(P, node, "P");

	if(xml_equal_string(node, "type", "bilinear")) {
		/* bilinear patch */
		if(P.size() == 4) {
			LinearQuadPatch *bpatch = new LinearQuadPatch();

			for(int i = 0; i < 4; i++)
				P[i] = transform_point(&state.tfm, P[i]);
			memcpy(bpatch->hull, &P[0], sizeof(bpatch->hull));

			patch = bpatch;
		}
		else
			fprintf(stderr, "Invalid number of control points for bilinear patch.\n");
	}
	else if(xml_equal_string(node, "type", "bicubic")) {
		/* bicubic patch */
		if(P.size() == 16) {
			BicubicPatch *bpatch = new BicubicPatch();

			for(int i = 0; i < 16; i++)
				P[i] = transform_point(&state.tfm, P[i]);
			memcpy(bpatch->hull, &P[0], sizeof(bpatch->hull));

			patch = bpatch;
		}
		else
			fprintf(stderr, "Invalid number of control points for bicubic patch.\n");
	}
	else
		fprintf(stderr, "Unknown patch type.\n");

	if(patch) {
		/* add mesh */
		Mesh *mesh = xml_add_mesh(state.scene, transform_identity());

		mesh->used_shaders.push_back(state.shader);

		/* split */
		DiagSplit dsplit;
		//dsplit.camera = state.scene->camera;
		//dsplit.dicing_rate = 5.0f;
		dsplit.dicing_rate = state.dicing_rate;
		xml_read_float(&dsplit.dicing_rate, node, "dicing_rate");
		dsplit.split_quad(mesh, patch, state.shader, state.smooth);

		delete patch;

		/* temporary for test compatibility */
		mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
	}
}

/* Light */

static void xml_read_light(const XMLReadState& state, pugi::xml_node node)
{
	Light *light = new Light();
	light->shader = state.shader;
	xml_read_float3(&light->co, node, "P");
	light->co = transform_point(&state.tfm, light->co);

	state.scene->lights.push_back(light);
}

/* Transform */

static void xml_read_transform(pugi::xml_node node, Transform& tfm)
{
	if(node.attribute("matrix")) {
		vector<float> matrix;
		if(xml_read_float_array(matrix, node, "matrix") && matrix.size() == 16)
			tfm = tfm * transform_transpose((*(Transform*)&matrix[0]));
	}

	if(node.attribute("translate")) {
		float3 translate = make_float3(0.0f, 0.0f, 0.0f);
		xml_read_float3(&translate, node, "translate");
		tfm = tfm * transform_translate(translate);
	}

	if(node.attribute("rotate")) {
		float4 rotate = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		xml_read_float4(&rotate, node, "rotate");
		tfm = tfm * transform_rotate(rotate.x*M_PI/180.0f, make_float3(rotate.y, rotate.z, rotate.w));
	}

	if(node.attribute("scale")) {
		float3 scale = make_float3(0.0f, 0.0f, 0.0f);
		xml_read_float3(&scale, node, "scale");
		tfm = tfm * transform_scale(scale);
	}
}

/* State */

static void xml_read_state(XMLReadState& state, pugi::xml_node node)
{
	/* read shader */
	string shadername;

	if(xml_read_string(&shadername, node, "shader")) {
		int i = 0;
		bool found = false;

		foreach(Shader *shader, state.scene->shaders) {
			if(shader->name == shadername) {
				state.shader = i;
				found = true;
				break;
			}

			i++;
		}

		if(!found)
			fprintf(stderr, "Unknown shader \"%s\".\n", shadername.c_str());
	}

	xml_read_float(&state.dicing_rate, node, "dicing_rate");

	/* read smooth/flat */
	if(xml_equal_string(node, "interpolation", "smooth"))
		state.smooth = true;
	else if(xml_equal_string(node, "interpolation", "flat"))
		state.smooth = false;

	/* read displacement method */
	if(xml_equal_string(node, "displacement_method", "true"))
		state.displacement_method = Mesh::DISPLACE_TRUE;
	else if(xml_equal_string(node, "displacement_method", "bump"))
		state.displacement_method = Mesh::DISPLACE_BUMP;
	else if(xml_equal_string(node, "displacement_method", "both"))
		state.displacement_method = Mesh::DISPLACE_BOTH;
}

/* Scene */

static void xml_read_include(const XMLReadState& state, const string& src);

static void xml_read_scene(const XMLReadState& state, pugi::xml_node scene_node)
{
	for(pugi::xml_node node = scene_node.first_child(); node; node = node.next_sibling()) {
		if(string_iequals(node.name(), "film")) {
			xml_read_film(state, node);
		}
		else if(string_iequals(node.name(), "integrator")) {
			xml_read_integrator(state, node);
		}
		else if(string_iequals(node.name(), "camera")) {
			xml_read_camera(state, node);
		}
		else if(string_iequals(node.name(), "shader")) {
			xml_read_shader(state, node);
		}
		else if(string_iequals(node.name(), "background")) {
			xml_read_background(state, node);
		}
		else if(string_iequals(node.name(), "mesh")) {
			xml_read_mesh(state, node);
		}
		else if(string_iequals(node.name(), "patch")) {
			xml_read_patch(state, node);
		}
		else if(string_iequals(node.name(), "light")) {
			xml_read_light(state, node);
		}
		else if(string_iequals(node.name(), "transform")) {
			XMLReadState substate = state;

			xml_read_transform(node, substate.tfm);
			xml_read_scene(substate, node);
		}
		else if(string_iequals(node.name(), "state")) {
			XMLReadState substate = state;

			xml_read_state(substate, node);
			xml_read_scene(substate, node);
		}
		else if(string_iequals(node.name(), "include")) {
			string src;

			if(xml_read_string(&src, node, "src"))
				xml_read_include(state, src);
		}
		else
			fprintf(stderr, "Unknown node \"%s\".\n", node.name());
	}
}

/* Include */

static void xml_read_include(const XMLReadState& state, const string& src)
{
	/* open XML document */
	pugi::xml_document doc;
	pugi::xml_parse_result parse_result;

	string path = path_join(state.base, src);
	parse_result = doc.load_file(path.c_str());

	if(parse_result) {
		XMLReadState substate = state;
		substate.base = path_dirname(path);

		xml_read_scene(substate, doc);
	}
	else
		fprintf(stderr, "%s read error: %s\n", src.c_str(), parse_result.description());
}

/* File */

void xml_read_file(Scene *scene, const char *filepath)
{
	XMLReadState state;

	state.scene = scene;
	state.tfm = transform_identity();
	state.shader = scene->default_surface;
	state.smooth = false;
	state.dicing_rate = 0.1f;
	state.base = path_dirname(filepath);

	xml_read_include(state, path_filename(filepath));

	scene->params.bvh_type = SceneParams::BVH_STATIC;
}

CCL_NAMESPACE_END

