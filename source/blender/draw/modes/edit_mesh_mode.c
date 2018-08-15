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

#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

#include "edit_mesh_mode_intern.h" /* own include */

#include "BKE_object.h"

#include "BLI_dynstr.h"


extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GlobalsUboStorage ts; /* draw_common.c */

extern char datatoc_edit_mesh_overlay_common_lib_glsl[];
extern char datatoc_edit_mesh_overlay_frag_glsl[];
extern char datatoc_edit_mesh_overlay_vert_glsl[];
extern char datatoc_edit_mesh_overlay_geom_tri_glsl[];
extern char datatoc_edit_mesh_overlay_geom_edge_glsl[];
extern char datatoc_edit_mesh_overlay_loosevert_vert_glsl[];
extern char datatoc_edit_mesh_overlay_facedot_frag_glsl[];
extern char datatoc_edit_mesh_overlay_facedot_vert_glsl[];
extern char datatoc_edit_mesh_overlay_ghost_clear_vert_glsl[];
extern char datatoc_edit_mesh_overlay_mix_frag_glsl[];
extern char datatoc_edit_mesh_overlay_facefill_vert_glsl[];
extern char datatoc_edit_mesh_overlay_facefill_frag_glsl[];
extern char datatoc_edit_normals_vert_glsl[];
extern char datatoc_edit_normals_geom_glsl[];
extern char datatoc_common_globals_lib_glsl[];

extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

/* *********** LISTS *********** */
typedef struct EDIT_MESH_PassList {
	struct DRWPass *vcolor_faces;
	struct DRWPass *depth_hidden_wire;
	struct DRWPass *ghost_clear_depth;
	struct DRWPass *edit_face_overlay;
	struct DRWPass *edit_face_occluded;
	struct DRWPass *mix_occlude;
	struct DRWPass *facefill_occlude;
	struct DRWPass *normals;
} EDIT_MESH_PassList;

typedef struct EDIT_MESH_FramebufferList {
	struct GPUFrameBuffer *occlude_wire_fb;
	struct GPUFrameBuffer *ghost_wire_fb;
} EDIT_MESH_FramebufferList;

typedef struct EDIT_MESH_StorageList {
	struct EDIT_MESH_PrivateData *g_data;
} EDIT_MESH_StorageList;

typedef struct EDIT_MESH_Data {
	void *engine_type;
	EDIT_MESH_FramebufferList *fbl;
	DRWViewportEmptyList *txl;
	EDIT_MESH_PassList *psl;
	EDIT_MESH_StorageList *stl;
} EDIT_MESH_Data;

/* *********** STATIC *********** */
#define MAX_SHADERS 16

static struct {
	/* weight/vert-color */
	GPUShader *vcolor_face_shader;

	/* Geometry */
	GPUShader *overlay_tri_sh_cache[MAX_SHADERS];
	GPUShader *overlay_loose_edge_sh_cache[MAX_SHADERS];

	GPUShader *overlay_vert_sh;
	GPUShader *overlay_facedot_sh;
	GPUShader *overlay_mix_sh;
	GPUShader *overlay_facefill_sh;
	GPUShader *normals_face_sh;
	GPUShader *normals_loop_sh;
	GPUShader *normals_sh;
	GPUShader *depth_sh;
	GPUShader *ghost_clear_depth_sh;
	/* temp buffer texture */
	struct GPUTexture *occlude_wire_depth_tx;
	struct GPUTexture *occlude_wire_color_tx;
} e_data = {NULL}; /* Engine data */

typedef struct EDIT_MESH_PrivateData {
	/* weight/vert-color */
	DRWShadingGroup *fvcolor_shgrp;
	DRWShadingGroup *depth_shgrp_hidden_wire;

	DRWShadingGroup *fnormals_shgrp;
	DRWShadingGroup *vnormals_shgrp;
	DRWShadingGroup *lnormals_shgrp;

	DRWShadingGroup *face_overlay_shgrp;
	DRWShadingGroup *ledges_overlay_shgrp;
	DRWShadingGroup *lverts_overlay_shgrp;
	DRWShadingGroup *facedot_overlay_shgrp;

	DRWShadingGroup *face_occluded_shgrp;
	DRWShadingGroup *ledges_occluded_shgrp;
	DRWShadingGroup *lverts_occluded_shgrp;
	DRWShadingGroup *facedot_occluded_shgrp;
	DRWShadingGroup *facefill_occluded_shgrp;

	int ghost_ob;
	int edit_ob;
	bool do_zbufclip;
} EDIT_MESH_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */
static int EDIT_MESH_sh_index(ToolSettings *tsettings, RegionView3D *rv3d, bool supports_fast_mode)
{
	int result = tsettings->selectmode << 1;
	if (supports_fast_mode) {
		SET_FLAG_FROM_TEST(result, (rv3d->rflag & RV3D_NAVIGATING), 1 << 0);
	}
	return result;
}

static char *EDIT_MESH_sh_defines(ToolSettings *tsettings, RegionView3D *rv3d, bool anti_alias, bool looseedge)
{
	const int selectmode = tsettings->selectmode;
	const int fast_mode = rv3d->rflag & RV3D_NAVIGATING;

	char *str = NULL;
	DynStr *ds = BLI_dynstr_new();

	if (selectmode & SCE_SELECT_VERTEX) {
		BLI_dynstr_append(ds, "#define VERTEX_SELECTION\n");
	}

	if (selectmode & SCE_SELECT_EDGE) {
		BLI_dynstr_append(ds, "#define EDGE_SELECTION\n");
	}

	if (selectmode & SCE_SELECT_FACE) {
		BLI_dynstr_append(ds, "#define FACE_SELECTION\n");
	}

	if (!fast_mode) {
		BLI_dynstr_append(ds, "#define EDGE_FIX\n");
	}

	if (anti_alias) {
		BLI_dynstr_append(ds, "#define ANTI_ALIASING\n");
	}

	if (!looseedge) {
		BLI_dynstr_append(ds, "#define VERTEX_FACING\n");
	}

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return str;
}
static char *EDIT_MESH_sh_lib(void)
{
	char *str = NULL;
	DynStr *ds = BLI_dynstr_new();

	BLI_dynstr_append(ds, datatoc_common_globals_lib_glsl);
	BLI_dynstr_append(ds, datatoc_edit_mesh_overlay_common_lib_glsl);

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return str;
}

static GPUShader *EDIT_MESH_ensure_shader(ToolSettings *tsettings, RegionView3D *rv3d, bool fast_mode, bool looseedge)
{
	const int index = EDIT_MESH_sh_index(tsettings, rv3d, fast_mode);
	if (looseedge) {
		if (!e_data.overlay_loose_edge_sh_cache[index]) {
			char *defines = EDIT_MESH_sh_defines(tsettings, rv3d, true, true);
			char *lib = EDIT_MESH_sh_lib();
			e_data.overlay_loose_edge_sh_cache[index] = DRW_shader_create_with_lib(
			        datatoc_edit_mesh_overlay_vert_glsl,
			        datatoc_edit_mesh_overlay_geom_edge_glsl,
			        datatoc_edit_mesh_overlay_frag_glsl,
			        lib,
			        defines);
			MEM_freeN(lib);
			MEM_freeN(defines);
		}
		return e_data.overlay_loose_edge_sh_cache[index];
	}
	else {
		if (!e_data.overlay_tri_sh_cache[index]) {
			char *defines = EDIT_MESH_sh_defines(tsettings, rv3d, true, false);
			char *lib = EDIT_MESH_sh_lib();
			e_data.overlay_tri_sh_cache[index] = DRW_shader_create_with_lib(
			        datatoc_edit_mesh_overlay_vert_glsl,
			        datatoc_edit_mesh_overlay_geom_tri_glsl,
			        datatoc_edit_mesh_overlay_frag_glsl,
			        lib,
			        defines);
			MEM_freeN(lib);
			MEM_freeN(defines);
		}
		return e_data.overlay_tri_sh_cache[index];
	}
}

static void EDIT_MESH_engine_init(void *vedata)
{
	EDIT_MESH_FramebufferList *fbl = ((EDIT_MESH_Data *)vedata)->fbl;

	const float *viewport_size = DRW_viewport_size_get();
	const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

	e_data.occlude_wire_depth_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_DEPTH_COMPONENT24,
	                                                         &draw_engine_edit_mesh_type);
	e_data.occlude_wire_color_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA8,
	                                                         &draw_engine_edit_mesh_type);

	GPU_framebuffer_ensure_config(&fbl->occlude_wire_fb, {
		GPU_ATTACHMENT_TEXTURE(e_data.occlude_wire_depth_tx),
		GPU_ATTACHMENT_TEXTURE(e_data.occlude_wire_color_tx)
	});

	if (!e_data.vcolor_face_shader) {
		e_data.vcolor_face_shader = GPU_shader_get_builtin_shader(GPU_SHADER_SIMPLE_LIGHTING_SMOOTH_COLOR_ALPHA);
	}

	if (!e_data.overlay_vert_sh) {
		char *lib = EDIT_MESH_sh_lib();
		e_data.overlay_vert_sh = DRW_shader_create_with_lib(
		        datatoc_edit_mesh_overlay_loosevert_vert_glsl, NULL,
		        datatoc_edit_mesh_overlay_frag_glsl,
		        lib,
		        "#define VERTEX_SELECTION\n");
		MEM_freeN(lib);
	}
	if (!e_data.overlay_facedot_sh) {
		e_data.overlay_facedot_sh = DRW_shader_create_with_lib(
		        datatoc_edit_mesh_overlay_facedot_vert_glsl, NULL,
		        datatoc_edit_mesh_overlay_facedot_frag_glsl,
		        datatoc_common_globals_lib_glsl,
		        "#define VERTEX_FACING\n");
	}
	if (!e_data.overlay_mix_sh) {
		e_data.overlay_mix_sh = DRW_shader_create_fullscreen(datatoc_edit_mesh_overlay_mix_frag_glsl, NULL);
	}
	if (!e_data.overlay_facefill_sh) {
		e_data.overlay_facefill_sh = DRW_shader_create_with_lib(
		        datatoc_edit_mesh_overlay_facefill_vert_glsl, NULL,
		        datatoc_edit_mesh_overlay_facefill_frag_glsl,
		        datatoc_common_globals_lib_glsl, NULL);
	}
	if (!e_data.normals_face_sh) {
		e_data.normals_face_sh = DRW_shader_create(
		        datatoc_edit_normals_vert_glsl,
		        datatoc_edit_normals_geom_glsl,
		        datatoc_gpu_shader_uniform_color_frag_glsl,
		        "#define FACE_NORMALS\n");
	}
	if (!e_data.normals_loop_sh) {
		e_data.normals_loop_sh = DRW_shader_create(
		        datatoc_edit_normals_vert_glsl,
		        datatoc_edit_normals_geom_glsl,
		        datatoc_gpu_shader_uniform_color_frag_glsl,
		        "#define LOOP_NORMALS\n");
	}
	if (!e_data.normals_sh) {
		e_data.normals_sh = DRW_shader_create(
		        datatoc_edit_normals_vert_glsl,
		        datatoc_edit_normals_geom_glsl,
		        datatoc_gpu_shader_uniform_color_frag_glsl, NULL);
	}
	if (!e_data.depth_sh) {
		e_data.depth_sh = DRW_shader_create_3D_depth_only();
	}
	if (!e_data.ghost_clear_depth_sh) {
		e_data.ghost_clear_depth_sh = DRW_shader_create(datatoc_edit_mesh_overlay_ghost_clear_vert_glsl,
		                                                NULL, NULL, NULL);
	}
}

static DRWPass *edit_mesh_create_overlay_pass(
        float *faceAlpha, DRWState statemod,
        DRWShadingGroup **r_face_shgrp, DRWShadingGroup **r_ledges_shgrp,
        DRWShadingGroup **r_lverts_shgrp, DRWShadingGroup **r_facedot_shgrp)
{
	GPUShader *tri_sh, *ledge_sh;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	RegionView3D *rv3d = draw_ctx->rv3d;
	Scene *scene = draw_ctx->scene;
	ToolSettings *tsettings = scene->toolsettings;

	ledge_sh = EDIT_MESH_ensure_shader(tsettings, rv3d, false, true);
	tri_sh = EDIT_MESH_ensure_shader(tsettings, rv3d, true, false);

	DRWPass *pass = DRW_pass_create(
	        "Edit Mesh Face Overlay Pass",
	        DRW_STATE_WRITE_COLOR | DRW_STATE_POINT | statemod);

	*r_face_shgrp = DRW_shgroup_create(tri_sh, pass);
	DRW_shgroup_uniform_block(*r_face_shgrp, "globalsBlock", globals_ubo);
	DRW_shgroup_uniform_vec2(*r_face_shgrp, "viewportSize", DRW_viewport_size_get(), 1);
	DRW_shgroup_uniform_float(*r_face_shgrp, "faceAlphaMod", faceAlpha, 1);

	*r_ledges_shgrp = DRW_shgroup_create(ledge_sh, pass);
	DRW_shgroup_uniform_block(*r_ledges_shgrp, "globalsBlock", globals_ubo);
	DRW_shgroup_uniform_vec2(*r_ledges_shgrp, "viewportSize", DRW_viewport_size_get(), 1);

	if ((tsettings->selectmode & (SCE_SELECT_VERTEX)) != 0) {
		*r_lverts_shgrp = DRW_shgroup_create(e_data.overlay_vert_sh, pass);
		DRW_shgroup_uniform_block(*r_lverts_shgrp, "globalsBlock", globals_ubo);
		DRW_shgroup_uniform_vec2(*r_lverts_shgrp, "viewportSize", DRW_viewport_size_get(), 1);
	}

	if ((tsettings->selectmode & (SCE_SELECT_FACE)) != 0) {
		*r_facedot_shgrp = DRW_shgroup_create(e_data.overlay_facedot_sh, pass);
		DRW_shgroup_uniform_block(*r_facedot_shgrp, "globalsBlock", globals_ubo);
	}

	return pass;
}

static float backwire_opacity;
static float face_mod;
static float size_normal;

static void EDIT_MESH_cache_init(void *vedata)
{
	EDIT_MESH_PassList *psl = ((EDIT_MESH_Data *)vedata)->psl;
	EDIT_MESH_StorageList *stl = ((EDIT_MESH_Data *)vedata)->stl;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;

	static float zero = 0.0f;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}
	stl->g_data->ghost_ob = 0;
	stl->g_data->edit_ob = 0;

	const bool xray_enabled = ((draw_ctx->v3d->shading.flag & V3D_SHADING_XRAY) != 0) &&
	                           (draw_ctx->v3d->shading.type < OB_MATERIAL);
	stl->g_data->do_zbufclip = ((v3d->flag & V3D_ZBUF_SELECT) == 0) || xray_enabled;

	{
		psl->vcolor_faces = DRW_pass_create(
		        "Vert Color Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);

		stl->g_data->fvcolor_shgrp = DRW_shgroup_create(e_data.vcolor_face_shader, psl->vcolor_faces);

		static float light[3] = {-0.3f, 0.5f, 1.0f};
		static float alpha = 1.0f;
		static float world_light = 1.0f;  /* XXX, see: paint_vertex_mode.c */
		DRW_shgroup_uniform_vec3(stl->g_data->fvcolor_shgrp, "light", light, 1);
		DRW_shgroup_uniform_float(stl->g_data->fvcolor_shgrp, "alpha", &alpha, 1);
		DRW_shgroup_uniform_float(stl->g_data->fvcolor_shgrp, "global", &world_light, 1);
	}

	{
		/* Complementary Depth Pass */
		psl->depth_hidden_wire = DRW_pass_create(
		        "Depth Pass Hidden Wire",
		        DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK);
		stl->g_data->depth_shgrp_hidden_wire = DRW_shgroup_create(e_data.depth_sh, psl->depth_hidden_wire);
	}

	{
		/* Depth clearing for ghosting. */
		psl->ghost_clear_depth = DRW_pass_create(
		        "Ghost Depth Clear",
		        DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_STENCIL_NEQUAL);

		DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.ghost_clear_depth_sh, psl->ghost_clear_depth);
		DRW_shgroup_stencil_mask(shgrp, 0x00);
		DRW_shgroup_call_add(shgrp, DRW_cache_fullscreen_quad_get(), NULL);
	}

	{
		/* Normals */
		psl->normals = DRW_pass_create(
		        "Edit Mesh Normals Pass",
		        DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL);

		stl->g_data->fnormals_shgrp = DRW_shgroup_create(e_data.normals_face_sh, psl->normals);
		DRW_shgroup_uniform_float(stl->g_data->fnormals_shgrp, "normalSize", &size_normal, 1);
		DRW_shgroup_uniform_vec4(stl->g_data->fnormals_shgrp, "color", ts.colorNormal, 1);

		stl->g_data->vnormals_shgrp = DRW_shgroup_create(e_data.normals_sh, psl->normals);
		DRW_shgroup_uniform_float(stl->g_data->vnormals_shgrp, "normalSize", &size_normal, 1);
		DRW_shgroup_uniform_vec4(stl->g_data->vnormals_shgrp, "color", ts.colorVNormal, 1);

		stl->g_data->lnormals_shgrp = DRW_shgroup_create(e_data.normals_loop_sh, psl->normals);
		DRW_shgroup_uniform_float(stl->g_data->lnormals_shgrp, "normalSize", &size_normal, 1);
		DRW_shgroup_uniform_vec4(stl->g_data->lnormals_shgrp, "color", ts.colorLNormal, 1);
	}

	if (!stl->g_data->do_zbufclip) {
		psl->edit_face_overlay = edit_mesh_create_overlay_pass(
		        &face_mod, DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND,
		        &stl->g_data->face_overlay_shgrp, &stl->g_data->ledges_overlay_shgrp,
		        &stl->g_data->lverts_overlay_shgrp, &stl->g_data->facedot_overlay_shgrp);
	}
	else {
		/* We render all wires with depth and opaque to a new fbo and blend the result based on depth values */
		psl->edit_face_occluded = edit_mesh_create_overlay_pass(
		        &zero, DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH,
		        &stl->g_data->face_occluded_shgrp, &stl->g_data->ledges_occluded_shgrp,
		        &stl->g_data->lverts_occluded_shgrp, &stl->g_data->facedot_occluded_shgrp);

		/* however we loose the front faces value (because we need the depth of occluded wires and
		 * faces are alpha blended ) so we recover them in a new pass. */
		psl->facefill_occlude = DRW_pass_create(
		        "Front Face Color",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND);
		stl->g_data->facefill_occluded_shgrp = DRW_shgroup_create(e_data.overlay_facefill_sh, psl->facefill_occlude);
		DRW_shgroup_uniform_block(stl->g_data->facefill_occluded_shgrp, "globalsBlock", globals_ubo);

		/* we need a full screen pass to combine the result */
		struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

		psl->mix_occlude = DRW_pass_create(
		        "Mix Occluded Wires",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);
		DRWShadingGroup *mix_shgrp = DRW_shgroup_create(e_data.overlay_mix_sh, psl->mix_occlude);
		DRW_shgroup_call_add(mix_shgrp, quad, NULL);
		DRW_shgroup_uniform_float(mix_shgrp, "alpha", &backwire_opacity, 1);
		DRW_shgroup_uniform_texture_ref(mix_shgrp, "wireColor", &e_data.occlude_wire_color_tx);
		DRW_shgroup_uniform_texture_ref(mix_shgrp, "wireDepth", &e_data.occlude_wire_depth_tx);
		DRW_shgroup_uniform_texture_ref(mix_shgrp, "sceneDepth", &dtxl->depth);
	}
}

static void edit_mesh_add_ob_to_pass(
        Scene *scene, Object *ob, DRWShadingGroup *face_shgrp, DRWShadingGroup *ledges_shgrp,
        DRWShadingGroup *lverts_shgrp, DRWShadingGroup *facedot_shgrp, DRWShadingGroup *facefill_shgrp)
{
	struct GPUBatch *geo_ovl_tris, *geo_ovl_ledges, *geo_ovl_lverts, *geo_ovl_fcenter;
	ToolSettings *tsettings = scene->toolsettings;

	DRW_cache_mesh_wire_overlay_get(ob, &geo_ovl_tris, &geo_ovl_ledges, &geo_ovl_lverts);
	DRW_shgroup_call_add(face_shgrp, geo_ovl_tris, ob->obmat);
	DRW_shgroup_call_add(ledges_shgrp, geo_ovl_ledges, ob->obmat);

	if (facefill_shgrp) {
		DRW_shgroup_call_add(facefill_shgrp, geo_ovl_tris, ob->obmat);
	}

	if ((tsettings->selectmode & SCE_SELECT_VERTEX) != 0) {
		DRW_shgroup_call_add(lverts_shgrp, geo_ovl_lverts, ob->obmat);
	}

	if (facedot_shgrp && (tsettings->selectmode & SCE_SELECT_FACE) != 0 ) {
		geo_ovl_fcenter = DRW_cache_face_centers_get(ob);
		DRW_shgroup_call_add(facedot_shgrp, geo_ovl_fcenter, ob->obmat);
	}
}

static void EDIT_MESH_cache_populate(void *vedata, Object *ob)
{
	EDIT_MESH_StorageList *stl = ((EDIT_MESH_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;
	Scene *scene = draw_ctx->scene;
	struct GPUBatch *geom;

	if (ob->type == OB_MESH) {
		if ((ob == draw_ctx->object_edit) || BKE_object_is_in_editmode(ob)) {
			const Mesh *me = ob->data;
			bool do_occlude_wire = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_OCCLUDE_WIRE) != 0;
			bool do_show_weight = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_WEIGHT) != 0;

			/* Updating uniform */
			backwire_opacity = v3d->overlay.backwire_opacity;

			bool fnormals_do = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_FACE_NORMALS) != 0;
			bool vnormals_do = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_VERT_NORMALS) != 0;
			bool lnormals_do = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_LOOP_NORMALS) != 0;
			/* Updating uniform */
			size_normal = v3d->overlay.normals_length;

			face_mod = (do_occlude_wire) ? 0.0f : 1.0f;

			if (do_show_weight) {
				geom = DRW_cache_mesh_surface_weights_get(ob);
				DRW_shgroup_call_add(stl->g_data->fvcolor_shgrp, geom, ob->obmat);
			}

			if (do_occlude_wire) {
				geom = DRW_cache_mesh_surface_get(ob);
				DRW_shgroup_call_add(stl->g_data->depth_shgrp_hidden_wire, geom, ob->obmat);
			}

			if (fnormals_do) {
				geom = DRW_cache_face_centers_get(ob);
				DRW_shgroup_call_add(stl->g_data->fnormals_shgrp, geom, ob->obmat);
			}

			if (vnormals_do || lnormals_do) {
				struct GPUBatch *geo_ovl_tris, *geo_ovl_ledges, *geo_ovl_lverts;
				DRW_cache_mesh_normals_overlay_get(ob, &geo_ovl_tris, &geo_ovl_ledges, &geo_ovl_lverts);

				if (vnormals_do) {
					DRW_shgroup_call_add(stl->g_data->vnormals_shgrp, geo_ovl_tris, ob->obmat);
					DRW_shgroup_call_add(stl->g_data->vnormals_shgrp, geo_ovl_ledges, ob->obmat);
					DRW_shgroup_call_add(stl->g_data->vnormals_shgrp, geo_ovl_lverts, ob->obmat);
				}

				if (lnormals_do) {
					DRW_shgroup_call_add(stl->g_data->lnormals_shgrp, geo_ovl_tris, ob->obmat);
				}
			}

			if (stl->g_data->do_zbufclip) {
				edit_mesh_add_ob_to_pass(
				        scene, ob, stl->g_data->face_occluded_shgrp, stl->g_data->ledges_occluded_shgrp,
				        stl->g_data->lverts_occluded_shgrp, stl->g_data->facedot_occluded_shgrp,
				        stl->g_data->facefill_occluded_shgrp);
			}
			else {
				edit_mesh_add_ob_to_pass(
				        scene, ob, stl->g_data->face_overlay_shgrp, stl->g_data->ledges_overlay_shgrp,
				        stl->g_data->lverts_overlay_shgrp, NULL, NULL);
			}

			stl->g_data->ghost_ob += (ob->dtx & OB_DRAWXRAY) ? 1 : 0;
			stl->g_data->edit_ob += 1;

			/* 3D text overlay */
			if (me->drawflag & (ME_DRAWEXTRA_EDGELEN |
			                    ME_DRAWEXTRA_FACEAREA |
			                    ME_DRAWEXTRA_FACEANG |
			                    ME_DRAWEXTRA_EDGEANG |
			                    ME_DRAWEXTRA_INDICES))
			{
				if (DRW_state_show_text()) {
					DRW_edit_mesh_mode_text_measure_stats(
					       draw_ctx->ar, v3d, ob, &scene->unit);
				}
			}
		}
	}
}

static void EDIT_MESH_draw_scene(void *vedata)
{
	EDIT_MESH_PassList *psl = ((EDIT_MESH_Data *)vedata)->psl;
	EDIT_MESH_StorageList *stl = ((EDIT_MESH_Data *)vedata)->stl;
	EDIT_MESH_FramebufferList *fbl = ((EDIT_MESH_Data *)vedata)->fbl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	DRW_draw_pass(psl->vcolor_faces);

	DRW_draw_pass(psl->depth_hidden_wire);

	if (stl->g_data->do_zbufclip) {
		float clearcol[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		/* render facefill */
		DRW_draw_pass(psl->facefill_occlude);

		/* Render wires on a separate framebuffer */
		GPU_framebuffer_bind(fbl->occlude_wire_fb);
		GPU_framebuffer_clear_color_depth(fbl->occlude_wire_fb, clearcol, 1.0f);
		DRW_draw_pass(psl->normals);
		DRW_draw_pass(psl->edit_face_occluded);

		/* Combine with scene buffer */
		GPU_framebuffer_bind(dfbl->color_only_fb);
		DRW_draw_pass(psl->mix_occlude);
	}
	else {
		DRW_draw_pass(psl->normals);

		const DRWContextState *draw_ctx = DRW_context_state_get();
		View3D *v3d = draw_ctx->v3d;

		if (v3d->shading.type == OB_SOLID && (v3d->shading.flag & V3D_SHADING_XRAY) == 0) {
			if (stl->g_data->ghost_ob == 1 && stl->g_data->edit_ob == 1) {
				/* In the case of single ghost object edit (common case for retopology):
				 * we duplicate the depht+stencil buffer and clear all depth to 1.0f where
				 * the stencil buffer is no 0x00. */
				const float *viewport_size = DRW_viewport_size_get();
				const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};
				struct GPUTexture *ghost_depth_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_DEPTH24_STENCIL8, &draw_engine_edit_mesh_type);
				GPU_framebuffer_ensure_config(&fbl->ghost_wire_fb, {
					GPU_ATTACHMENT_TEXTURE(ghost_depth_tx),
					GPU_ATTACHMENT_TEXTURE(dtxl->color),
				});

				GPU_framebuffer_blit(dfbl->depth_only_fb, 0, fbl->ghost_wire_fb, 0, GPU_DEPTH_BIT | GPU_STENCIL_BIT);
				GPU_framebuffer_bind(fbl->ghost_wire_fb);

				DRW_draw_pass(psl->ghost_clear_depth);
			}
		}

		DRW_draw_pass(psl->edit_face_overlay);
	}
}

static void EDIT_MESH_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.overlay_vert_sh);
	DRW_SHADER_FREE_SAFE(e_data.overlay_facedot_sh);
	DRW_SHADER_FREE_SAFE(e_data.overlay_mix_sh);
	DRW_SHADER_FREE_SAFE(e_data.overlay_facefill_sh);
	DRW_SHADER_FREE_SAFE(e_data.normals_loop_sh);
	DRW_SHADER_FREE_SAFE(e_data.normals_face_sh);
	DRW_SHADER_FREE_SAFE(e_data.normals_sh);

	for (int i = 0; i < MAX_SHADERS; i++) {
		DRW_SHADER_FREE_SAFE(e_data.overlay_tri_sh_cache[i]);
		DRW_SHADER_FREE_SAFE(e_data.overlay_loose_edge_sh_cache[i]);
	}
}

static const DrawEngineDataSize EDIT_MESH_data_size = DRW_VIEWPORT_DATA_SIZE(EDIT_MESH_Data);

DrawEngineType draw_engine_edit_mesh_type = {
	NULL, NULL,
	N_("EditMeshMode"),
	&EDIT_MESH_data_size,
	&EDIT_MESH_engine_init,
	&EDIT_MESH_engine_free,
	&EDIT_MESH_cache_init,
	&EDIT_MESH_cache_populate,
	NULL,
	NULL,
	&EDIT_MESH_draw_scene,
	NULL,
	NULL,
};
