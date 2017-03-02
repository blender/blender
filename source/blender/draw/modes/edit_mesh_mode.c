/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/modes/edit_mesh_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "GPU_shader.h"
#include "DNA_view3d_types.h"

#include "draw_mode_pass.h"

#include "edit_mesh_mode.h"

/* keep it under MAX_PASSES */
typedef struct EDIT_MESH_PassList {
	struct DRWPass *non_meshes_pass;
	struct DRWPass *ob_center_pass;
	struct DRWPass *wire_outline_pass;
	struct DRWPass *depth_pass_hidden_wire;
	struct DRWPass *edit_face_overlay_pass;
} EDIT_MESH_PassList;

static DRWShadingGroup *depth_shgrp_hidden_wire;
static DRWShadingGroup *face_overlay_shgrp;
static DRWShadingGroup *ledges_overlay_shgrp;
static DRWShadingGroup *lverts_overlay_shgrp;
static DRWShadingGroup *facedot_overlay_shgrp;

extern struct GPUUniformBuffer *globals_ubo; /* draw_mode_pass.c */

static struct GPUShader *overlay_tri_sh = NULL;
static struct GPUShader *overlay_tri_fast_sh = NULL;
static struct GPUShader *overlay_tri_vcol_sh = NULL;
static struct GPUShader *overlay_tri_vcol_fast_sh = NULL;
static struct GPUShader *overlay_edge_sh = NULL;
static struct GPUShader *overlay_edge_vcol_sh = NULL;
static struct GPUShader *overlay_vert_sh = NULL;
static struct GPUShader *overlay_facedot_sh = NULL;

extern char datatoc_edit_overlay_frag_glsl[];
extern char datatoc_edit_overlay_vert_glsl[];
extern char datatoc_edit_overlay_geom_tri_glsl[];
extern char datatoc_edit_overlay_geom_edge_glsl[];
extern char datatoc_edit_overlay_loosevert_vert_glsl[];
extern char datatoc_edit_overlay_facedot_frag_glsl[];
extern char datatoc_edit_overlay_facedot_vert_glsl[];

void EDIT_MESH_cache_init(void)
{
	EDIT_MESH_PassList *psl = DRW_mode_pass_list_get();
	static struct GPUShader *depth_sh, *tri_sh, *ledge_sh;

	const struct bContext *C = DRW_get_context();
	struct RegionView3D *rv3d = CTX_wm_region_view3d(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;

	if (!depth_sh)
		depth_sh = DRW_shader_create_3D_depth_only();

	if (!overlay_tri_sh) {
		overlay_tri_sh = DRW_shader_create(datatoc_edit_overlay_vert_glsl,
		                                   datatoc_edit_overlay_geom_tri_glsl,
		                                   datatoc_edit_overlay_frag_glsl, "#define EDGE_FIX\n");
	}
	if (!overlay_tri_fast_sh) {
		overlay_tri_fast_sh = DRW_shader_create(datatoc_edit_overlay_vert_glsl,
		                                       datatoc_edit_overlay_geom_tri_glsl,
		                                       datatoc_edit_overlay_frag_glsl, NULL);
	}
	if (!overlay_tri_vcol_sh) {
		overlay_tri_vcol_sh = DRW_shader_create(datatoc_edit_overlay_vert_glsl,
		                                        datatoc_edit_overlay_geom_tri_glsl,
		                                        datatoc_edit_overlay_frag_glsl, "#define EDGE_FIX\n"
		                                                                        "#define VERTEX_SELECTION\n");
	}
	if (!overlay_tri_vcol_fast_sh) {
		overlay_tri_vcol_fast_sh = DRW_shader_create(datatoc_edit_overlay_vert_glsl,
		                                             datatoc_edit_overlay_geom_tri_glsl,
		                                             datatoc_edit_overlay_frag_glsl, "#define VERTEX_SELECTION\n");
	}
	if (!overlay_edge_sh) {
		overlay_edge_sh = DRW_shader_create(datatoc_edit_overlay_vert_glsl,
		                                    datatoc_edit_overlay_geom_edge_glsl,
		                                    datatoc_edit_overlay_frag_glsl, NULL);
	}
	if (!overlay_edge_vcol_sh) {
		overlay_edge_vcol_sh = DRW_shader_create(datatoc_edit_overlay_vert_glsl,
		                                         datatoc_edit_overlay_geom_edge_glsl,
		                                         datatoc_edit_overlay_frag_glsl, "#define VERTEX_SELECTION\n");
	}
	if (!overlay_vert_sh) {
		overlay_vert_sh = DRW_shader_create(datatoc_edit_overlay_loosevert_vert_glsl, NULL,
		                                    datatoc_edit_overlay_frag_glsl, "#define VERTEX_SELECTION\n");
	}
	if (!overlay_facedot_sh) {
		overlay_facedot_sh = DRW_shader_create(datatoc_edit_overlay_facedot_vert_glsl, NULL,
		                                       datatoc_edit_overlay_facedot_frag_glsl, NULL);
	}

	if ((ts->selectmode & SCE_SELECT_VERTEX) != 0) {
		ledge_sh = overlay_edge_vcol_sh;

		if ((rv3d->rflag & RV3D_NAVIGATING) != 0)
			tri_sh = overlay_tri_vcol_fast_sh;
		else
			tri_sh = overlay_tri_vcol_sh;
	}
	else {
		ledge_sh = overlay_edge_sh;

		if ((rv3d->rflag & RV3D_NAVIGATING) != 0)
			tri_sh = overlay_tri_fast_sh;
		else
			tri_sh = overlay_tri_sh;
	}

	psl->depth_pass_hidden_wire = DRW_pass_create("Depth Pass Hidden Wire", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK);
	depth_shgrp_hidden_wire = DRW_shgroup_create(depth_sh, psl->depth_pass_hidden_wire);

	psl->edit_face_overlay_pass = DRW_pass_create("Edit Mesh Face Overlay Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND | DRW_STATE_POINT);

	face_overlay_shgrp = DRW_shgroup_create(tri_sh, psl->edit_face_overlay_pass);
	DRW_shgroup_uniform_block(face_overlay_shgrp, "globalsBlock", globals_ubo, 0);
	DRW_shgroup_uniform_vec2(face_overlay_shgrp, "viewportSize", DRW_viewport_size_get(), 1);

	ledges_overlay_shgrp = DRW_shgroup_create(ledge_sh, psl->edit_face_overlay_pass);
	DRW_shgroup_uniform_vec2(ledges_overlay_shgrp, "viewportSize", DRW_viewport_size_get(), 1);

	if ((ts->selectmode & (SCE_SELECT_VERTEX)) != 0) {
		lverts_overlay_shgrp = DRW_shgroup_create(overlay_vert_sh, psl->edit_face_overlay_pass);
		DRW_shgroup_uniform_vec2(lverts_overlay_shgrp, "viewportSize", DRW_viewport_size_get(), 1);
	}

	if ((ts->selectmode & (SCE_SELECT_FACE)) != 0) {
		facedot_overlay_shgrp = DRW_shgroup_create(overlay_facedot_sh, psl->edit_face_overlay_pass);
	}

	DRW_mode_passes_setup(NULL,
	                      NULL,
	                      &psl->wire_outline_pass,
	                      &psl->non_meshes_pass,
	                      &psl->ob_center_pass,
	                      NULL,
	                      NULL);
}

void EDIT_MESH_cache_populate(Object *ob)
{
	struct Batch *geom;
	struct Batch *geo_ovl_tris, *geo_ovl_ledges, *geo_ovl_lverts, *geo_ovl_fcenter;
	const struct bContext *C = DRW_get_context();
	Scene *scene = CTX_data_scene(C);
	Object *obedit = scene->obedit;
	ToolSettings *ts = scene->toolsettings;

	CollectionEngineSettings *ces_mode_ed = BKE_object_collection_engine_get(ob, COLLECTION_MODE_EDIT, "");
	bool do_occlude_wire = BKE_collection_engine_property_value_get_bool(ces_mode_ed, "show_occlude_wire");

	switch (ob->type) {
		case OB_MESH:
			if (ob == obedit) {
				DRW_cache_wire_overlay_get(ob, &geo_ovl_tris, &geo_ovl_ledges, &geo_ovl_lverts);
				DRW_shgroup_call_add(face_overlay_shgrp, geo_ovl_tris, ob->obmat);
				DRW_shgroup_call_add(ledges_overlay_shgrp, geo_ovl_ledges, ob->obmat);

				if ((ts->selectmode & SCE_SELECT_VERTEX) != 0)
					DRW_shgroup_call_add(lverts_overlay_shgrp, geo_ovl_lverts, ob->obmat);

				if ((ts->selectmode & SCE_SELECT_FACE) != 0) {
					geo_ovl_fcenter = DRW_cache_face_centers_get(ob);
					DRW_shgroup_call_add(facedot_overlay_shgrp, geo_ovl_fcenter, ob->obmat);
				}

				if (do_occlude_wire) {
					geom = DRW_cache_surface_get(ob);
					DRW_shgroup_call_add(depth_shgrp_hidden_wire, geom, ob->obmat);
				}
			}
			break;
		case OB_LAMP:
			DRW_shgroup_lamp(ob);
			break;
		case OB_CAMERA:
		case OB_EMPTY:
			DRW_shgroup_empty(ob);
			break;
		case OB_SPEAKER:
			DRW_shgroup_speaker(ob);
			break;
		case OB_ARMATURE:
			DRW_shgroup_armature_object(ob);
			break;
		default:
			break;
	}

	DRW_shgroup_object_center(ob);
	DRW_shgroup_relationship_lines(ob);
}

void EDIT_MESH_cache_finish(void)
{
	/* Do nothing */
}

void EDIT_MESH_draw(void)
{
	EDIT_MESH_PassList *psl = DRW_mode_pass_list_get();

	DRW_draw_pass(psl->depth_pass_hidden_wire);
	DRW_draw_pass(psl->edit_face_overlay_pass);
	DRW_draw_pass(psl->wire_outline_pass);
	DRW_draw_pass(psl->non_meshes_pass);
	DRW_draw_pass(psl->ob_center_pass);
}

void EDIT_MESH_collection_settings_create(CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	BKE_collection_engine_property_add_int(ces, "show_occlude_wire", false);
}

void EDIT_MESH_engine_free(void)
{
	if (overlay_tri_sh)
		DRW_shader_free(overlay_tri_sh);
	if (overlay_tri_fast_sh)
		DRW_shader_free(overlay_tri_fast_sh);
	if (overlay_tri_vcol_sh)
		DRW_shader_free(overlay_tri_vcol_sh);
	if (overlay_tri_vcol_fast_sh)
		DRW_shader_free(overlay_tri_vcol_fast_sh);
	if (overlay_edge_sh)
		DRW_shader_free(overlay_edge_sh);
	if (overlay_edge_vcol_sh)
		DRW_shader_free(overlay_edge_vcol_sh);
	if (overlay_vert_sh)
		DRW_shader_free(overlay_vert_sh);
	if (overlay_facedot_sh)
		DRW_shader_free(overlay_facedot_sh);
}