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
#include "BlenderTextureManager.h"

#include "../application/AppConfig.h"
#include "../stroke/Canvas.h"

#include "BKE_global.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h" /* free_libblock */
#include "BKE_main.h" /* struct Main */
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "RE_pipeline.h"
}

#include <limits.h>

namespace Freestyle {

BlenderStrokeRenderer::BlenderStrokeRenderer(Render *re, int render_count) : StrokeRenderer()
{
	freestyle_bmain = &re->freestyle_bmain;

	// TEMPORARY - need a  texture manager
	_textureManager = new BlenderTextureManager;
	_textureManager->load();

	// for stroke mesh generation
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
		printf("%s: %d threads\n", __func__, freestyle_scene->r.threads);
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
}

BlenderStrokeRenderer::~BlenderStrokeRenderer()
{
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

	if (0 != _textureManager) {
		delete _textureManager;
		_textureManager = NULL;
	}

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

void BlenderStrokeRenderer::RenderStrokeRep(StrokeRep *iStrokeRep) const
{
	bool has_mat = false;
	int a = 0;

	// Look for a good existing material
	for (Link *lnk = (Link *)freestyle_bmain->mat.first; lnk; lnk = lnk->next) {
		Material *ma = (Material*) lnk;
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
			ma->mtex[a] = (MTex *) iStrokeRep->getMTex(a);

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

	RenderStrokeRepBasic(iStrokeRep);
}

void BlenderStrokeRenderer::RenderStrokeRepBasic(StrokeRep *iStrokeRep) const
{
	////////////////////
	//  Build up scene
	////////////////////

	vector<Strip*>& strips = iStrokeRep->getStrips();
	Strip::vertex_container::iterator v[3];
	StrokeVertexRep *svRep[3];
	/* Vec3r color[3]; */ /* UNUSED */
	unsigned int vertex_index, edge_index, loop_index;
	Vec2r p;

	for (vector<Strip*>::iterator s = strips.begin(), send = strips.end(); s != send; ++s) {
		Strip::vertex_container& strip_vertices = (*s)->vertices();
		int strip_vertex_count = (*s)->sizeStrip();
		int xl, xu, yl, yu, n, visible_faces, visible_segments;
		bool visible;

		// iterate over all vertices and count visible faces and strip segments
		// (note: a strip segment is a series of visible faces, while two strip
		// segments are separated by one or more invisible faces)
		v[0] = strip_vertices.begin();
		v[1] = v[0] + 1;
		v[2] = v[0] + 2;
		visible_faces = visible_segments = 0;
		visible = false;
		for (n = 2; n < strip_vertex_count; n++, v[0]++, v[1]++, v[2]++) {
			svRep[0] = *(v[0]);
			svRep[1] = *(v[1]);
			svRep[2] = *(v[2]);
			xl = xu = yl = yu = 0;
			for (int j = 0; j < 3; j++) {
				p = svRep[j]->point2d();
				if (p[0] < 0.0)
					xl++;
				else if (p[0] > _width)
					xu++;
				if (p[1] < 0.0)
					yl++;
				else if (p[1] > _height)
					yu++;
			}
			if (xl == 3 || xu == 3 || yl == 3 || yu == 3) {
				visible = false;
			}
			else {
				visible_faces++;
				if (!visible)
					visible_segments++;
				visible = true;
			}
		}
		if (visible_faces == 0)
			continue;

		//me = Mesh.New()
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
		mesh->totvert = visible_faces + visible_segments * 2;
		mesh->mvert = (MVert *)CustomData_add_layer(&mesh->vdata, CD_MVERT, CD_CALLOC, NULL, mesh->totvert);

		// edges allocation
		mesh->totedge = visible_faces * 2 + visible_segments;
		mesh->medge = (MEdge *)CustomData_add_layer(&mesh->edata, CD_MEDGE, CD_CALLOC, NULL, mesh->totedge);

		// faces allocation
		mesh->totpoly = visible_faces;
		mesh->mpoly = (MPoly *)CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_CALLOC, NULL, mesh->totpoly);

		// loops allocation
		mesh->totloop = visible_faces * 3;
		mesh->mloop = (MLoop *)CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_CALLOC, NULL, mesh->totloop);

		// colors allocation
		mesh->mloopcol = (MLoopCol *)CustomData_add_layer(&mesh->ldata, CD_MLOOPCOL, CD_CALLOC, NULL, mesh->totloop);

		////////////////////
		//  Data copy
		////////////////////

		MVert *vertices = mesh->mvert;
		MEdge *edges = mesh->medge;
		MPoly *polys = mesh->mpoly;
		MLoop *loops = mesh->mloop;
		MLoopCol *colors = mesh->mloopcol;
		MLoopUV *loopsuv[2];

		v[0] = strip_vertices.begin();
		v[1] = v[0] + 1;
		v[2] = v[0] + 2;

		vertex_index = edge_index = loop_index = 0;
		visible = false;

		// First UV layer
		CustomData_add_layer_named(&mesh->pdata, CD_MTEXPOLY, CD_CALLOC, NULL, mesh->totpoly, "along_stroke");
		CustomData_add_layer_named(&mesh->ldata, CD_MLOOPUV, CD_CALLOC, NULL, mesh->totloop, "along_stroke");
		CustomData_set_layer_active(&mesh->pdata, CD_MTEXPOLY, 0);
		CustomData_set_layer_active(&mesh->ldata, CD_MLOOPUV, 0);
		BKE_mesh_update_customdata_pointers(mesh, true);

		loopsuv[0] = mesh->mloopuv;

		// Second UV layer
		CustomData_add_layer_named(&mesh->pdata, CD_MTEXPOLY, CD_CALLOC, NULL, mesh->totpoly, "along_stroke_tips");
		CustomData_add_layer_named(&mesh->ldata, CD_MLOOPUV, CD_CALLOC, NULL, mesh->totloop, "along_stroke_tips");
		CustomData_set_layer_active(&mesh->pdata, CD_MTEXPOLY, 1);
		CustomData_set_layer_active(&mesh->ldata, CD_MLOOPUV, 1);
		BKE_mesh_update_customdata_pointers(mesh, true);

		loopsuv[1] = mesh->mloopuv;

		// Note: Mesh generation in the following loop assumes stroke strips
		// to be triangle strips.
		for (n = 2; n < strip_vertex_count; n++, v[0]++, v[1]++, v[2]++) {
			svRep[0] = *(v[0]);
			svRep[1] = *(v[1]);
			svRep[2] = *(v[2]);
			xl = xu = yl = yu = 0;
			for (int j = 0; j < 3; j++) {
				p = svRep[j]->point2d();
				if (p[0] < 0.0)
					xl++;
				else if (p[0] > _width)
					xu++;
				if (p[1] < 0.0)
					yl++;
				else if (p[1] > _height)
					yu++;
			}
			if (xl == 3 || xu == 3 || yl == 3 || yu == 3) {
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
				if (iStrokeRep->getMTex(0)) {
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
						/* freestyle tex-origin is upside-down */
						for (int i = 0; i < 3; i++) {
							loopsuv[L][i].uv[1] *= -1;
						}
						loopsuv[L] += 3;
					}
				}

				// colors
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
				colors += 3;
			}
		} // loop over strip vertices
#if 0
		BKE_mesh_validate(mesh, true);
#endif
	} // loop over strips
}

// A replacement of BKE_object_add() for better performance.
Object *BlenderStrokeRenderer::NewMesh() const
{
	Object *ob;
	Base *base;
	char name[MAX_ID_NAME];
	unsigned int mesh_id = get_stroke_mesh_id();

	/* XXX this is for later review, for now we start names with 27 (DEL) 
	   to allow ignoring them in DAG_ids_check_recalc() */
	BLI_snprintf(name, MAX_ID_NAME, "%c0%08xOB", 27, mesh_id);
	ob = BKE_object_add_only_object(freestyle_bmain, OB_MESH, name);
	BLI_snprintf(name, MAX_ID_NAME, "%c0%08xME", 27, mesh_id);
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
