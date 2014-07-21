/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/blender_interface/BlenderStrokeRenderer.cpp
 *  \ingroup freestyle
 */

#include "BlenderStrokeRenderer.h"

#include "../application/AppConfig.h"
#include "../stroke/Canvas.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "DNA_camera_types.h"
#include "DNA_listBase.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h" /* free_libblock */
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "RE_pipeline.h"

#include "render_types.h"
}

#include <limits.h>

namespace Freestyle {

BlenderStrokeRenderer::BlenderStrokeRenderer(bContext *C, Render *re, int render_count) : StrokeRenderer()
{
	freestyle_bmain = re->freestyle_bmain;

	// for stroke mesh generation
	_context = C;
	_width = re->winx;
	_height = re->winy;

	old_scene = re->scene;

	char name[MAX_ID_NAME - 2];
	BLI_snprintf(name, sizeof(name), "FRS%d_%s", render_count, re->scene->id.name + 2);
	freestyle_scene = BKE_scene_add(freestyle_bmain, name);
	freestyle_scene->r.cfra = old_scene->r.cfra;
	freestyle_scene->r.mode = old_scene->r.mode &
	                          ~(R_EDGE_FRS | R_SHADOW | R_SSS | R_PANORAMA | R_ENVMAP | R_MBLUR | R_BORDER);
	freestyle_scene->r.xsch = re->rectx; // old_scene->r.xsch
	freestyle_scene->r.ysch = re->recty; // old_scene->r.ysch
	freestyle_scene->r.xasp = 1.0f; // old_scene->r.xasp;
	freestyle_scene->r.yasp = 1.0f; // old_scene->r.yasp;
	freestyle_scene->r.tilex = old_scene->r.tilex;
	freestyle_scene->r.tiley = old_scene->r.tiley;
	freestyle_scene->r.size = 100; // old_scene->r.size
	//freestyle_scene->r.maximsize = old_scene->r.maximsize; /* DEPRECATED */
	freestyle_scene->r.ocres = old_scene->r.ocres;
	freestyle_scene->r.color_mgt_flag = 0; // old_scene->r.color_mgt_flag;
	freestyle_scene->r.scemode = old_scene->r.scemode & ~(R_SINGLE_LAYER | R_NO_FRAME_UPDATE);
	freestyle_scene->r.flag = old_scene->r.flag;
	freestyle_scene->r.threads = old_scene->r.threads;
	freestyle_scene->r.border.xmin = old_scene->r.border.xmin;
	freestyle_scene->r.border.ymin = old_scene->r.border.ymin;
	freestyle_scene->r.border.xmax = old_scene->r.border.xmax;
	freestyle_scene->r.border.ymax = old_scene->r.border.ymax;
	strcpy(freestyle_scene->r.pic, old_scene->r.pic);
	freestyle_scene->r.safety.xmin = old_scene->r.safety.xmin;
	freestyle_scene->r.safety.ymin = old_scene->r.safety.ymin;
	freestyle_scene->r.safety.xmax = old_scene->r.safety.xmax;
	freestyle_scene->r.safety.ymax = old_scene->r.safety.ymax;
	freestyle_scene->r.osa = old_scene->r.osa;
	freestyle_scene->r.filtertype = old_scene->r.filtertype;
	freestyle_scene->r.gauss = old_scene->r.gauss;
	freestyle_scene->r.dither_intensity = old_scene->r.dither_intensity;
	BLI_strncpy(freestyle_scene->r.engine, old_scene->r.engine, sizeof(freestyle_scene->r.engine));
	freestyle_scene->r.im_format.planes = R_IMF_PLANES_RGBA; 
	freestyle_scene->r.im_format.imtype = R_IMF_IMTYPE_PNG;
	BKE_scene_disable_color_management(freestyle_scene);

	if (G.debug & G_DEBUG_FREESTYLE) {
		printf("%s: %d thread(s)\n", __func__, BKE_render_num_threads(&freestyle_scene->r));
	}

	// Render layer
	SceneRenderLayer *srl = (SceneRenderLayer *)freestyle_scene->r.layers.first;
	srl->layflag = SCE_LAY_SOLID | SCE_LAY_ZTRA;

	BKE_scene_set_background(freestyle_bmain, freestyle_scene);

	// Camera
	Object *object_camera = BKE_object_add(freestyle_bmain, freestyle_scene, OB_CAMERA);

	Camera *camera = (Camera *)object_camera->data;
	camera->type = CAM_ORTHO;
	camera->ortho_scale = max(re->rectx, re->recty);
	camera->clipsta = 0.1f;
	camera->clipend = 100.0f;

	_z_delta = 0.00001f;
	_z = camera->clipsta + _z_delta;

	object_camera->loc[0] = re->disprect.xmin + 0.5f * re->rectx;
	object_camera->loc[1] = re->disprect.ymin + 0.5f * re->recty;
	object_camera->loc[2] = 1.0f;

	freestyle_scene->camera = object_camera;

	// Reset serial mesh ID (used for BlenderStrokeRenderer::NewMesh())
	_mesh_id = 0xffffffff;

	// Check if the rendering engine uses new shading nodes
	_use_shading_nodes = BKE_scene_use_new_shading_nodes(freestyle_scene);

	// Create a bNodeTree-to-Material hash table
	if (_use_shading_nodes)
		_nodetree_hash = BLI_ghash_ptr_new("BlenderStrokeRenderer::_nodetree_hash");
	else
		_nodetree_hash = NULL;
}

BlenderStrokeRenderer::~BlenderStrokeRenderer()
{
#if 0
	return; // XXX
#endif

	// The freestyle_scene object is not released here.  Instead,
	// the scene is released in free_all_freestyle_renders() in
	// source/blender/render/intern/source/pipeline.c, after the
	// compositor has finished.

	// release objects and data blocks
	for (Base *b = (Base *)freestyle_scene->base.first; b; b = b->next) {
		Object *ob = b->object;
		void *data = ob->data;
		char *name = ob->id.name;
#if 0
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "removing " << name[0] << name[1] << ":" << (name+2) << endl;
		}
#endif
		switch (ob->type) {
		case OB_MESH:
			BKE_libblock_free(freestyle_bmain, ob);
			BKE_libblock_free(freestyle_bmain, data);
			break;
		case OB_CAMERA:
			BKE_libblock_free(freestyle_bmain, ob);
			BKE_libblock_free(freestyle_bmain, data);
			freestyle_scene->camera = NULL;
			break;
		default:
			cerr << "Warning: unexpected object in the scene: " << name[0] << name[1] << ":" << (name + 2) << endl;
		}
	}
	BLI_freelistN(&freestyle_scene->base);

	// release materials
	Link *lnk = (Link *)freestyle_bmain->mat.first;

	while (lnk)
	{
		Material *ma = (Material*)lnk;
		// We want to retain the linestyle mtexs, so let's detach them first
		for (int a = 0; a < MAX_MTEX; a++) {
			if (ma->mtex[a]) {
				ma->mtex[a] = NULL;
			}
			else {
				break; // Textures are ordered, no empty slots between two textures
			}
		}
		lnk = lnk->next;
		BKE_libblock_free(freestyle_bmain, ma);
	}

	if (_use_shading_nodes)
		BLI_ghash_free(_nodetree_hash, NULL, NULL);
}

float BlenderStrokeRenderer::get_stroke_vertex_z(void) const
{
	float z = _z;
	BlenderStrokeRenderer *self = const_cast<BlenderStrokeRenderer *>(this);
	if (!(_z < _z_delta * 100000.0f))
		self->_z_delta *= 10.0f;
	self->_z += _z_delta;
	return -z;
}

unsigned int BlenderStrokeRenderer::get_stroke_mesh_id(void) const
{
	unsigned mesh_id = _mesh_id;
	BlenderStrokeRenderer *self = const_cast<BlenderStrokeRenderer *>(this);
	self->_mesh_id--;
	return mesh_id;
}

Material* BlenderStrokeRenderer::GetStrokeShader(bContext *C, Main *bmain, bNodeTree *iNodeTree, bool do_id_user)
{
	Material *ma = BKE_material_add(bmain, "stroke_shader");
	bNodeTree *ntree;
	bNode *output_linestyle = NULL;
	bNodeSocket *fromsock, *tosock;
	PointerRNA fromptr, toptr;
	NodeShaderAttribute *storage;

	if (iNodeTree) {
		// make a copy of linestyle->nodetree
		ntree = ntreeCopyTree_ex(iNodeTree, bmain, do_id_user);

		// find the active Output Line Style node
		for (bNode *node = (bNode *)ntree->nodes.first; node; node = node->next) {
			if (node->type == SH_NODE_OUTPUT_LINESTYLE && (node->flag & NODE_DO_OUTPUT)) {
				output_linestyle = node;
				break;
			}
		}
	}
	else {
		ntree = ntreeAddTree(NULL, "stroke_shader", "ShaderNodeTree");
	}
	ma->nodetree = ntree;
	ma->use_nodes = 1;

	bNode *input_attr_color = nodeAddStaticNode(C, ntree, SH_NODE_ATTRIBUTE);
	input_attr_color->locx = 0.0f;
	input_attr_color->locy = -200.0f;
	storage = (NodeShaderAttribute *)input_attr_color->storage;
	BLI_strncpy(storage->name, "color", sizeof(storage->name));

	bNode *mix_rgb_color = nodeAddStaticNode(C, ntree, SH_NODE_MIX_RGB);
	mix_rgb_color->custom1 = MA_RAMP_BLEND; // Mix
	mix_rgb_color->locx = 200.0f;
	mix_rgb_color->locy = -200.0f;
	tosock = (bNodeSocket *)BLI_findlink(&mix_rgb_color->inputs, 0); // Fac
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, tosock, &toptr);
	RNA_float_set(&toptr, "default_value", 0.0f);

	bNode *input_attr_alpha = nodeAddStaticNode(C, ntree, SH_NODE_ATTRIBUTE);
	input_attr_alpha->locx = 400.0f;
	input_attr_alpha->locy = 300.0f;
	storage = (NodeShaderAttribute *)input_attr_alpha->storage;
	BLI_strncpy(storage->name, "alpha", sizeof(storage->name));

	bNode *mix_rgb_alpha = nodeAddStaticNode(C, ntree, SH_NODE_MIX_RGB);
	mix_rgb_alpha->custom1 = MA_RAMP_BLEND; // Mix
	mix_rgb_alpha->locx = 600.0f;
	mix_rgb_alpha->locy = 300.0f;
	tosock = (bNodeSocket *)BLI_findlink(&mix_rgb_alpha->inputs, 0); // Fac
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, tosock, &toptr);
	RNA_float_set(&toptr, "default_value", 0.0f);

	bNode *shader_emission = nodeAddStaticNode(C, ntree, SH_NODE_EMISSION);
	shader_emission->locx = 400.0f;
	shader_emission->locy = -200.0f;

	bNode *input_light_path = nodeAddStaticNode(C, ntree, SH_NODE_LIGHT_PATH);
	input_light_path->locx = 400.0f;
	input_light_path->locy = 100.0f;

	bNode *mix_shader_color = nodeAddStaticNode(C, ntree, SH_NODE_MIX_SHADER);
	mix_shader_color->locx = 600.0f;
	mix_shader_color->locy = -100.0f;

	bNode *shader_transparent = nodeAddStaticNode(C, ntree, SH_NODE_BSDF_TRANSPARENT);
	shader_transparent->locx = 600.0f;
	shader_transparent->locy = 100.0f;

	bNode *mix_shader_alpha = nodeAddStaticNode(C, ntree, SH_NODE_MIX_SHADER);
	mix_shader_alpha->locx = 800.0f;
	mix_shader_alpha->locy = 100.0f;

	bNode *output_material = nodeAddStaticNode(C, ntree, SH_NODE_OUTPUT_MATERIAL);
	output_material->locx = 1000.0f;
	output_material->locy = 100.0f;

	fromsock = (bNodeSocket *)BLI_findlink(&input_attr_color->outputs, 0); // Color
	tosock = (bNodeSocket *)BLI_findlink(&mix_rgb_color->inputs, 1); // Color1
	nodeAddLink(ntree, input_attr_color, fromsock, mix_rgb_color, tosock);

	fromsock = (bNodeSocket *)BLI_findlink(&mix_rgb_color->outputs, 0); // Color
	tosock = (bNodeSocket *)BLI_findlink(&shader_emission->inputs, 0); // Color
	nodeAddLink(ntree, mix_rgb_color, fromsock, shader_emission, tosock);

	fromsock = (bNodeSocket *)BLI_findlink(&shader_emission->outputs, 0); // Emission
	tosock = (bNodeSocket *)BLI_findlink(&mix_shader_color->inputs, 2); // Shader (second)
	nodeAddLink(ntree, shader_emission, fromsock, mix_shader_color, tosock);

	fromsock = (bNodeSocket *)BLI_findlink(&input_light_path->outputs, 0); // In Camera Ray
	tosock = (bNodeSocket *)BLI_findlink(&mix_shader_color->inputs, 0); // Fac
	nodeAddLink(ntree, input_light_path, fromsock, mix_shader_color, tosock);

	fromsock = (bNodeSocket *)BLI_findlink(&mix_rgb_alpha->outputs, 0); // Color
	tosock = (bNodeSocket *)BLI_findlink(&mix_shader_alpha->inputs, 0); // Fac
	nodeAddLink(ntree, mix_rgb_alpha, fromsock, mix_shader_alpha, tosock);

	fromsock = (bNodeSocket *)BLI_findlink(&input_attr_alpha->outputs, 0); // Color
	tosock = (bNodeSocket *)BLI_findlink(&mix_rgb_alpha->inputs, 1); // Color1
	nodeAddLink(ntree, input_attr_alpha, fromsock, mix_rgb_alpha, tosock);

	fromsock = (bNodeSocket *)BLI_findlink(&shader_transparent->outputs, 0); // BSDF
	tosock = (bNodeSocket *)BLI_findlink(&mix_shader_alpha->inputs, 1); // Shader (first)
	nodeAddLink(ntree, shader_transparent, fromsock, mix_shader_alpha, tosock);

	fromsock = (bNodeSocket *)BLI_findlink(&mix_shader_color->outputs, 0); // Shader
	tosock = (bNodeSocket *)BLI_findlink(&mix_shader_alpha->inputs, 2); // Shader (second)
	nodeAddLink(ntree, mix_shader_color, fromsock, mix_shader_alpha, tosock);

	fromsock = (bNodeSocket *)BLI_findlink(&mix_shader_alpha->outputs, 0); // Shader
	tosock = (bNodeSocket *)BLI_findlink(&output_material->inputs, 0); // Surface
	nodeAddLink(ntree, mix_shader_alpha, fromsock, output_material, tosock);

	if (output_linestyle) {
		bNodeSocket *outsock;
		bNodeLink *link;

		mix_rgb_color->custom1 = output_linestyle->custom1; // blend_type
		mix_rgb_color->custom2 = output_linestyle->custom2; // use_clamp

		outsock = (bNodeSocket *)BLI_findlink(&output_linestyle->inputs, 0); // Color
		tosock = (bNodeSocket *)BLI_findlink(&mix_rgb_color->inputs, 2); // Color2
		link = (bNodeLink *)BLI_findptr(&ntree->links, outsock, offsetof(bNodeLink, tosock));
		if (link) {
			nodeAddLink(ntree, link->fromnode, link->fromsock, mix_rgb_color, tosock);
		}
		else {
			float color[4];
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, outsock, &fromptr);
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, tosock, &toptr);
			RNA_float_get_array(&fromptr, "default_value", color);
			RNA_float_set_array(&toptr, "default_value", color);
		}

		outsock = (bNodeSocket *)BLI_findlink(&output_linestyle->inputs, 1); // Color Fac
		tosock = (bNodeSocket *)BLI_findlink(&mix_rgb_color->inputs, 0); // Fac
		link = (bNodeLink *)BLI_findptr(&ntree->links, outsock, offsetof(bNodeLink, tosock));
		if (link) {
			nodeAddLink(ntree, link->fromnode, link->fromsock, mix_rgb_color, tosock);
		}
		else {
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, outsock, &fromptr);
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, tosock, &toptr);
			RNA_float_set(&toptr, "default_value", RNA_float_get(&fromptr, "default_value"));
		}

		outsock = (bNodeSocket *)BLI_findlink(&output_linestyle->inputs, 2); // Alpha
		tosock = (bNodeSocket *)BLI_findlink(&mix_rgb_alpha->inputs, 2); // Color2
		link = (bNodeLink *)BLI_findptr(&ntree->links, outsock, offsetof(bNodeLink, tosock));
		if (link) {
			nodeAddLink(ntree, link->fromnode, link->fromsock, mix_rgb_alpha, tosock);
		}
		else {
			float color[4];
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, outsock, &fromptr);
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, tosock, &toptr);
			color[0] = color[1] = color[2] = RNA_float_get(&fromptr, "default_value");
			color[3] = 1.0f;
			RNA_float_set_array(&toptr, "default_value", color);
		}

		outsock = (bNodeSocket *)BLI_findlink(&output_linestyle->inputs, 3); // Alpha Fac
		tosock = (bNodeSocket *)BLI_findlink(&mix_rgb_alpha->inputs, 0); // Fac
		link = (bNodeLink *)BLI_findptr(&ntree->links, outsock, offsetof(bNodeLink, tosock));
		if (link) {
			nodeAddLink(ntree, link->fromnode, link->fromsock, mix_rgb_alpha, tosock);
		}
		else {
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, outsock, &fromptr);
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, tosock, &toptr);
			RNA_float_set(&toptr, "default_value", RNA_float_get(&fromptr, "default_value"));
		}

		for (bNode *node = (bNode *)ntree->nodes.first; node; node = node->next) {
			if (node->type == SH_NODE_UVALONGSTROKE) {
				// UV output of the UV Along Stroke node
				bNodeSocket *sock = (bNodeSocket *)BLI_findlink(&node->outputs, 0);

				// add new UV Map node
				bNode *input_uvmap = nodeAddStaticNode(C, ntree, SH_NODE_UVMAP);
				input_uvmap->locx = node->locx - 200.0f;
				input_uvmap->locy = node->locy;
				NodeShaderUVMap *storage = (NodeShaderUVMap *)input_uvmap->storage;
				if (node->custom1 & 1) { // use_tips
					BLI_strncpy(storage->uv_map, "along_stroke_tips", sizeof(storage->uv_map));
				}
				else {
					BLI_strncpy(storage->uv_map, "along_stroke", sizeof(storage->uv_map));
				}
				fromsock = (bNodeSocket *)BLI_findlink(&input_uvmap->outputs, 0); // UV

				// replace links from the UV Along Stroke node by links from the UV Map node
				for (bNodeLink *link = (bNodeLink *)ntree->links.first; link; link = link->next) {
					if (link->fromnode == node && link->fromsock == sock) {
						nodeAddLink(ntree, input_uvmap, fromsock, link->tonode, link->tosock);
					}
				}
				nodeRemSocketLinks(ntree, sock);
			}
		}
	}

	nodeSetActive(ntree, output_material);
	ntreeUpdateTree(bmain, ntree);

	return ma;
}

void BlenderStrokeRenderer::RenderStrokeRep(StrokeRep *iStrokeRep) const
{
	if (_use_shading_nodes) {
		bNodeTree *nt = iStrokeRep->getNodeTree();
		Material *ma = (Material *)BLI_ghash_lookup(_nodetree_hash, nt);
		if (!ma) {
			ma = BlenderStrokeRenderer::GetStrokeShader(_context, freestyle_bmain, nt, false);
			BLI_ghash_insert(_nodetree_hash, nt, ma);
#if 0
			if (G.debug & G_DEBUG_FREESTYLE) {
				printf("Material '%s' created using ShaderNodeTree '%s'\n", ma->id.name+2, nt->id.name+2);
			}
#endif
		}

		if (strcmp(freestyle_scene->r.engine, "CYCLES") == 0) {
			PointerRNA scene_ptr;
			RNA_pointer_create(NULL, &RNA_Scene, freestyle_scene, &scene_ptr);
			PointerRNA cycles_ptr = RNA_pointer_get(&scene_ptr, "cycles");
			RNA_boolean_set(&cycles_ptr, "film_transparent", 1);
		}

		iStrokeRep->setMaterial(ma);
	}
	else {
		bool has_mat = false;
		int a = 0;

		// Look for a good existing material
		for (Link *lnk = (Link *)freestyle_bmain->mat.first; lnk; lnk = lnk->next) {
			Material *ma = (Material*)lnk;
			bool texs_are_good = true;
			// as soon as textures differ it's not the right one
			for (int a = 0; a < MAX_MTEX; a++) {
				if (ma->mtex[a] != iStrokeRep->getMTex(a)) {
					texs_are_good = false;
					break;
				}
			}

			if (texs_are_good) {
				iStrokeRep->setMaterial(ma);
				has_mat = true;
				break; // if textures are good, no need to search anymore
			}
		}

		// If still no material, create one
		if (!has_mat) {
			Material *ma = BKE_material_add(freestyle_bmain, "stroke_material");
			ma->mode |= MA_VERTEXCOLP;
			ma->mode |= MA_TRANSP;
			ma->mode |= MA_SHLESS;
			ma->vcol_alpha = 1;

			// Textures
			//for (int a = 0; a < MAX_MTEX; a++) {
			while (iStrokeRep->getMTex(a)) {
				ma->mtex[a] = (MTex *)iStrokeRep->getMTex(a);

				// We'll generate both with tips and without tips
				// coordinates, on two different UV layers.
				if (ma->mtex[a]->texflag & MTEX_TIPS)  {
					BLI_strncpy(ma->mtex[a]->uvname, "along_stroke_tips", sizeof(ma->mtex[a]->uvname));
				}
				else {
					BLI_strncpy(ma->mtex[a]->uvname, "along_stroke", sizeof(ma->mtex[a]->uvname));
				}
				a++;
			}

			iStrokeRep->setMaterial(ma);
		}
	}

	RenderStrokeRepBasic(iStrokeRep);
}

// Check if the triangle is visible (i.e., within the render image boundary)
bool BlenderStrokeRenderer::test_triangle_visibility(StrokeVertexRep *svRep[3]) const
{
	int xl, xu, yl, yu;
	Vec2r p;

	xl = xu = yl = yu = 0;
	for (int i = 0; i < 3; i++) {
		p = svRep[i]->point2d();
		if (p[0] < 0.0)
			xl++;
		else if (p[0] > _width)
			xu++;
		if (p[1] < 0.0)
			yl++;
		else if (p[1] > _height)
			yu++;
	}
	return !(xl == 3 || xu == 3 || yl == 3 || yu == 3);
}

// Check the visibility of faces and strip segments.
void BlenderStrokeRenderer::test_strip_visibility(Strip::vertex_container& strip_vertices,
	int *visible_faces, int *visible_segments) const
{
	const int strip_vertex_count = strip_vertices.size();
	Strip::vertex_container::iterator v[3];
	StrokeVertexRep *svRep[3];
	bool visible;

	// iterate over all vertices and count visible faces and strip segments
	// (note: a strip segment is a series of visible faces, while two strip
	// segments are separated by one or more invisible faces)
	v[0] = strip_vertices.begin();
	v[1] = v[0] + 1;
	v[2] = v[0] + 2;
	*visible_faces = *visible_segments = 0;
	visible = false;
	for (int n = 2; n < strip_vertex_count; n++, v[0]++, v[1]++, v[2]++) {
		svRep[0] = *(v[0]);
		svRep[1] = *(v[1]);
		svRep[2] = *(v[2]);
		if (test_triangle_visibility(svRep)) {
			(*visible_faces)++;
			if (!visible)
				(*visible_segments)++;
			visible = true;
		}
		else {
			visible = false;
		}
	}
}

// Build a mesh object representing a stroke
void BlenderStrokeRenderer::RenderStrokeRepBasic(StrokeRep *iStrokeRep) const
{
	vector<Strip*>& strips = iStrokeRep->getStrips();
	const bool hasTex = iStrokeRep->hasTex();
	Strip::vertex_container::iterator v[3];
	StrokeVertexRep *svRep[3];
	unsigned int vertex_index, edge_index, loop_index;
	Vec2r p;

	int totvert = 0, totedge = 0, totpoly = 0, totloop = 0;
	int visible_faces, visible_segments;

	bool visible;
	for (vector<Strip*>::iterator s = strips.begin(), send = strips.end(); s != send; ++s) {
		Strip::vertex_container& strip_vertices = (*s)->vertices();

		// count visible faces and strip segments
		test_strip_visibility(strip_vertices, &visible_faces, &visible_segments);
		if (visible_faces == 0)
			continue;

		totvert += visible_faces + visible_segments * 2;
		totedge += visible_faces * 2 + visible_segments;
		totpoly += visible_faces;
		totloop += visible_faces * 3;
	}

#if 0
	Object *object_mesh = BKE_object_add(freestyle_bmain, freestyle_scene, OB_MESH);
#else
	Object *object_mesh = NewMesh();
#endif
	Mesh *mesh = (Mesh *)object_mesh->data;
	mesh->mat = (Material **)MEM_mallocN(1 * sizeof(Material *), "MaterialList");
	mesh->mat[0] = iStrokeRep->getMaterial();
	mesh->totcol = 1;
	test_object_materials(freestyle_bmain, (ID *)mesh);

	// vertices allocation
	mesh->totvert = totvert; // visible_faces + visible_segments * 2;
	mesh->mvert = (MVert *)CustomData_add_layer(&mesh->vdata, CD_MVERT, CD_CALLOC, NULL, mesh->totvert);

	// edges allocation
	mesh->totedge = totedge; // visible_faces * 2 + visible_segments;
	mesh->medge = (MEdge *)CustomData_add_layer(&mesh->edata, CD_MEDGE, CD_CALLOC, NULL, mesh->totedge);

	// faces allocation
	mesh->totpoly = totpoly; // visible_faces;
	mesh->mpoly = (MPoly *)CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_CALLOC, NULL, mesh->totpoly);

	// loops allocation
	mesh->totloop = totloop; // visible_faces * 3;
	mesh->mloop = (MLoop *)CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_CALLOC, NULL, mesh->totloop);

	// colors and alpha transparency (the latter represented by grayscale colors)
	MLoopCol *colors = (MLoopCol *)CustomData_add_layer_named(&mesh->ldata, CD_MLOOPCOL, CD_CALLOC, NULL, mesh->totloop, "color");
	MLoopCol *transp = (MLoopCol *)CustomData_add_layer_named(&mesh->ldata, CD_MLOOPCOL, CD_CALLOC, NULL, mesh->totloop, "alpha");

	////////////////////
	//  Data copy
	////////////////////

	MVert *vertices = mesh->mvert;
	MEdge *edges = mesh->medge;
	MPoly *polys = mesh->mpoly;
	MLoop *loops = mesh->mloop;
	MLoopUV *loopsuv[2] = {NULL};

	if (hasTex) {
		// First UV layer
		CustomData_add_layer_named(&mesh->pdata, CD_MTEXPOLY, CD_CALLOC, NULL, mesh->totpoly, "along_stroke");
		CustomData_add_layer_named(&mesh->ldata, CD_MLOOPUV, CD_CALLOC, NULL, mesh->totloop, "along_stroke");
		CustomData_set_layer_active(&mesh->pdata, CD_MTEXPOLY, 0);
		CustomData_set_layer_active(&mesh->ldata, CD_MLOOPUV, 0);
		BKE_mesh_update_customdata_pointers(mesh, false);

		loopsuv[0] = mesh->mloopuv;

		// Second UV layer
		CustomData_add_layer_named(&mesh->pdata, CD_MTEXPOLY, CD_CALLOC, NULL, mesh->totpoly, "along_stroke_tips");
		CustomData_add_layer_named(&mesh->ldata, CD_MLOOPUV, CD_CALLOC, NULL, mesh->totloop, "along_stroke_tips");
		CustomData_set_layer_active(&mesh->pdata, CD_MTEXPOLY, 1);
		CustomData_set_layer_active(&mesh->ldata, CD_MLOOPUV, 1);
		BKE_mesh_update_customdata_pointers(mesh, false);

		loopsuv[1] = mesh->mloopuv;
	}

	vertex_index = edge_index = loop_index = 0;

	for (vector<Strip*>::iterator s = strips.begin(), send = strips.end(); s != send; ++s) {
		Strip::vertex_container& strip_vertices = (*s)->vertices();
		int strip_vertex_count = strip_vertices.size();

		// count visible faces and strip segments
		test_strip_visibility(strip_vertices, &visible_faces, &visible_segments);
		if (visible_faces == 0)
			continue;

		v[0] = strip_vertices.begin();
		v[1] = v[0] + 1;
		v[2] = v[0] + 2;

		visible = false;

		// Note: Mesh generation in the following loop assumes stroke strips
		// to be triangle strips.
		for (int n = 2; n < strip_vertex_count; n++, v[0]++, v[1]++, v[2]++) {
			svRep[0] = *(v[0]);
			svRep[1] = *(v[1]);
			svRep[2] = *(v[2]);
			if (!test_triangle_visibility(svRep)) {
				visible = false;
			}
			else {
				if (!visible) {
					// first vertex
					vertices->co[0] = svRep[0]->point2d()[0];
					vertices->co[1] = svRep[0]->point2d()[1];
					vertices->co[2] = get_stroke_vertex_z();
					vertices->no[0] = 0;
					vertices->no[1] = 0;
					vertices->no[2] = SHRT_MAX;
					++vertices;
					++vertex_index;

					// second vertex
					vertices->co[0] = svRep[1]->point2d()[0];
					vertices->co[1] = svRep[1]->point2d()[1];
					vertices->co[2] = get_stroke_vertex_z();
					vertices->no[0] = 0;
					vertices->no[1] = 0;
					vertices->no[2] = SHRT_MAX;
					++vertices;
					++vertex_index;

					// first edge
					edges->v1 = vertex_index - 2;
					edges->v2 = vertex_index - 1;
					++edges;
					++edge_index;
				}
				visible = true;

				// vertex
				vertices->co[0] = svRep[2]->point2d()[0];
				vertices->co[1] = svRep[2]->point2d()[1];
				vertices->co[2] = get_stroke_vertex_z();
				vertices->no[0] = 0;
				vertices->no[1] = 0;
				vertices->no[2] = SHRT_MAX;
				++vertices;
				++vertex_index;

				// edges
				edges->v1 = vertex_index - 1;
				edges->v2 = vertex_index - 3;
				++edges;
				++edge_index;

				edges->v1 = vertex_index - 1;
				edges->v2 = vertex_index - 2;
				++edges;
				++edge_index;

				// poly
				polys->loopstart = loop_index;
				polys->totloop = 3;
				++polys;

				// Even and odd loops connect triangles vertices differently
				bool is_odd = n % 2;
				// loops
				if (is_odd) {
					loops[0].v = vertex_index - 1;
					loops[0].e = edge_index - 2;

					loops[1].v = vertex_index - 3;
					loops[1].e = edge_index - 3;

					loops[2].v = vertex_index - 2;
					loops[2].e = edge_index - 1;
				}
				else {
					loops[0].v = vertex_index - 1;
					loops[0].e = edge_index - 1;

					loops[1].v = vertex_index - 2;
					loops[1].e = edge_index - 3;

					loops[2].v = vertex_index - 3;
					loops[2].e = edge_index - 2;
				}
				loops += 3;
				loop_index += 3;

				// UV
				if (hasTex) {
					// First UV layer (loopsuv[0]) has no tips (texCoord(0)).
					// Second UV layer (loopsuv[1]) has tips:  (texCoord(1)).
					for (int L = 0; L < 2; L++) {
						if (is_odd) {
							loopsuv[L][0].uv[0] = svRep[2]->texCoord(L).x();
							loopsuv[L][0].uv[1] = svRep[2]->texCoord(L).y();

							loopsuv[L][1].uv[0] = svRep[0]->texCoord(L).x();
							loopsuv[L][1].uv[1] = svRep[0]->texCoord(L).y();

							loopsuv[L][2].uv[0] = svRep[1]->texCoord(L).x();
							loopsuv[L][2].uv[1] = svRep[1]->texCoord(L).y();
						}
						else {
							loopsuv[L][0].uv[0] = svRep[2]->texCoord(L).x();
							loopsuv[L][0].uv[1] = svRep[2]->texCoord(L).y();

							loopsuv[L][1].uv[0] = svRep[1]->texCoord(L).x();
							loopsuv[L][1].uv[1] = svRep[1]->texCoord(L).y();

							loopsuv[L][2].uv[0] = svRep[0]->texCoord(L).x();
							loopsuv[L][2].uv[1] = svRep[0]->texCoord(L).y();
						}
						loopsuv[L] += 3;
					}
				}

				// colors and alpha transparency
				if (is_odd) {
					colors[0].r = (short)(255.0f * svRep[2]->color()[0]);
					colors[0].g = (short)(255.0f * svRep[2]->color()[1]);
					colors[0].b = (short)(255.0f * svRep[2]->color()[2]);
					colors[0].a = (short)(255.0f * svRep[2]->alpha());

					colors[1].r = (short)(255.0f * svRep[0]->color()[0]);
					colors[1].g = (short)(255.0f * svRep[0]->color()[1]);
					colors[1].b = (short)(255.0f * svRep[0]->color()[2]);
					colors[1].a = (short)(255.0f * svRep[0]->alpha());

					colors[2].r = (short)(255.0f * svRep[1]->color()[0]);
					colors[2].g = (short)(255.0f * svRep[1]->color()[1]);
					colors[2].b = (short)(255.0f * svRep[1]->color()[2]);
					colors[2].a = (short)(255.0f * svRep[1]->alpha());
				}
				else {
					colors[0].r = (short)(255.0f * svRep[2]->color()[0]);
					colors[0].g = (short)(255.0f * svRep[2]->color()[1]);
					colors[0].b = (short)(255.0f * svRep[2]->color()[2]);
					colors[0].a = (short)(255.0f * svRep[2]->alpha());

					colors[1].r = (short)(255.0f * svRep[1]->color()[0]);
					colors[1].g = (short)(255.0f * svRep[1]->color()[1]);
					colors[1].b = (short)(255.0f * svRep[1]->color()[2]);
					colors[1].a = (short)(255.0f * svRep[1]->alpha());

					colors[2].r = (short)(255.0f * svRep[0]->color()[0]);
					colors[2].g = (short)(255.0f * svRep[0]->color()[1]);
					colors[2].b = (short)(255.0f * svRep[0]->color()[2]);
					colors[2].a = (short)(255.0f * svRep[0]->alpha());
				}
				transp[0].r = transp[0].g = transp[0].b = colors[0].a;
				transp[1].r = transp[1].g = transp[1].b = colors[1].a;
				transp[2].r = transp[2].g = transp[2].b = colors[2].a;
				colors += 3;
				transp += 3;
			}
		} // loop over strip vertices
	} // loop over strips
#if 0
	BLI_assert(totvert == vertex_index);
	BLI_assert(totedge == edge_index);
	BLI_assert(totloop == loop_index);
	BKE_mesh_validate(mesh, true);
#endif
}

// A replacement of BKE_object_add() for better performance.
Object *BlenderStrokeRenderer::NewMesh() const
{
	Object *ob;
	Base *base;
	char name[MAX_ID_NAME];
	unsigned int mesh_id = get_stroke_mesh_id();

	BLI_snprintf(name, MAX_ID_NAME, "0%08xOB", mesh_id);
	ob = BKE_object_add_only_object(freestyle_bmain, OB_MESH, name);
	BLI_snprintf(name, MAX_ID_NAME, "0%08xME", mesh_id);
	ob->data = BKE_mesh_add(freestyle_bmain, name);
	ob->lay = 1;

	base = BKE_scene_base_add(freestyle_scene, ob);
#if 0
	BKE_scene_base_deselect_all(scene);
	BKE_scene_base_select(scene, base);
#else
	(void)base;
#endif

	DAG_id_tag_update_ex(freestyle_bmain, &ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	return ob;
}

Render *BlenderStrokeRenderer::RenderScene(Render *re, bool render)
{
	Camera *camera = (Camera *)freestyle_scene->camera->data;
	if (camera->clipend < _z)
		camera->clipend = _z + _z_delta * 100.0f;
#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "clipsta " << camera->clipsta << ", clipend " << camera->clipend << endl;
	}
#endif

	Render *freestyle_render = RE_NewRender(freestyle_scene->id.name);

	RE_RenderFreestyleStrokes(freestyle_render, freestyle_bmain, freestyle_scene, render);

	return freestyle_render;
}

} /* namespace Freestyle */
