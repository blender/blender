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

/** \file blender/draw/modes/object_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_userdef_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_force.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_anim.h"
#include "BKE_camera.h"
#include "BKE_curve.h"
#include "BKE_global.h"

#include "ED_view3d.h"
#include "ED_view3d.h"

#include "GPU_shader.h"

#include "UI_resources.h"

#include "draw_mode_engines.h"
#include "draw_common.h"

extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern GlobalsUboStorage ts;

extern char datatoc_object_outline_resolve_frag_glsl[];
extern char datatoc_object_outline_detect_frag_glsl[];
extern char datatoc_object_outline_expand_frag_glsl[];
extern char datatoc_object_grid_frag_glsl[];
extern char datatoc_object_grid_vert_glsl[];
extern char datatoc_common_globals_lib_glsl[];

/* *********** LISTS *********** */
typedef struct OBJECT_PassList {
	struct DRWPass *non_meshes;
	struct DRWPass *ob_center;
	struct DRWPass *outlines;
	struct DRWPass *outlines_search;
	struct DRWPass *outlines_expand;
	struct DRWPass *outlines_fade1;
	struct DRWPass *outlines_fade2;
	struct DRWPass *outlines_fade3;
	struct DRWPass *outlines_fade4;
	struct DRWPass *outlines_fade5;
	struct DRWPass *outlines_resolve;
	struct DRWPass *grid;
	struct DRWPass *bone_solid;
	struct DRWPass *bone_wire;
} OBJECT_PassList;

typedef struct OBJECT_FramebufferList {
	struct GPUFrameBuffer *outlines;
	struct GPUFrameBuffer *blur;
} OBJECT_FramebufferList;

typedef struct OBJECT_TextureList {
	struct GPUTexture *outlines_depth_tx;
	struct GPUTexture *outlines_color_tx;
	struct GPUTexture *outlines_blur_tx;
} OBJECT_TextureList;

typedef struct OBJECT_StorageList {
	struct g_data *g_data;
} OBJECT_StorageList;

typedef struct OBJECT_Data {
	void *engine_type;
	OBJECT_FramebufferList *fbl;
	OBJECT_TextureList *txl;
	OBJECT_PassList *psl;
	OBJECT_StorageList *stl;
} OBJECT_Data;

/* *********** STATIC *********** */

typedef struct g_data{
	/* Empties */
	DRWShadingGroup *plain_axes;
	DRWShadingGroup *cube;
	DRWShadingGroup *circle;
	DRWShadingGroup *sphere;
	DRWShadingGroup *cone;
	DRWShadingGroup *single_arrow;
	DRWShadingGroup *single_arrow_line;
	DRWShadingGroup *arrows;
	DRWShadingGroup *axis_names;

	/* Force Field */
	DRWShadingGroup *field_wind;
	DRWShadingGroup *field_force;
	DRWShadingGroup *field_vortex;
	DRWShadingGroup *field_curve_sta;
	DRWShadingGroup *field_curve_end;
	DRWShadingGroup *field_tube_limit;
	DRWShadingGroup *field_cone_limit;

	/* Speaker */
	DRWShadingGroup *speaker;

	/* Lamps */
	DRWShadingGroup *lamp_center;
	DRWShadingGroup *lamp_center_group;
	DRWShadingGroup *lamp_groundpoint;
	DRWShadingGroup *lamp_groundline;
	DRWShadingGroup *lamp_circle;
	DRWShadingGroup *lamp_circle_shadow;
	DRWShadingGroup *lamp_sunrays;
	DRWShadingGroup *lamp_distance;
	DRWShadingGroup *lamp_buflimit;
	DRWShadingGroup *lamp_buflimit_points;
	DRWShadingGroup *lamp_area;
	DRWShadingGroup *lamp_hemi;
	DRWShadingGroup *lamp_spot_cone;
	DRWShadingGroup *lamp_spot_blend;
	DRWShadingGroup *lamp_spot_pyramid;
	DRWShadingGroup *lamp_spot_blend_rect;

	/* Helpers */
	DRWShadingGroup *relationship_lines;

	/* Objects Centers */
	DRWShadingGroup *center_active;
	DRWShadingGroup *center_selected;
	DRWShadingGroup *center_deselected;
	DRWShadingGroup *center_selected_lib;
	DRWShadingGroup *center_deselected_lib;

	/* Camera */
	DRWShadingGroup *camera;
	DRWShadingGroup *camera_tria;
	DRWShadingGroup *camera_focus;
	DRWShadingGroup *camera_clip;
	DRWShadingGroup *camera_clip_points;
	DRWShadingGroup *camera_mist;
	DRWShadingGroup *camera_mist_points;

	/* Outlines */
	DRWShadingGroup *outlines_active;
	DRWShadingGroup *outlines_active_group;
	DRWShadingGroup *outlines_select;
	DRWShadingGroup *outlines_select_group;
	DRWShadingGroup *outlines_transform;

} g_data; /* Transient data */

static struct {
	struct GPUShader *outline_resolve_sh;
	struct GPUShader *outline_detect_sh;
	struct GPUShader *outline_fade_sh;
	struct GPUShader *grid_sh;
	float camera_pos[3];
	float grid_settings[5];
	float grid_mat[4][4];
	int grid_flag;
	int zpos_flag;
	int zneg_flag;
} e_data = {NULL}; /* Engine data */


enum {
	SHOW_AXIS_X  = (1 << 0),
	SHOW_AXIS_Y  = (1 << 1),
	SHOW_AXIS_Z  = (1 << 2),
	SHOW_GRID    = (1 << 3),
	PLANE_XY     = (1 << 4),
	PLANE_XZ     = (1 << 5),
	PLANE_YZ     = (1 << 6),
	CLIP_ZPOS    = (1 << 7),
	CLIP_ZNEG    = (1 << 8),
};

/* *********** FUNCTIONS *********** */

static void OBJECT_engine_init(void *vedata)
{
	OBJECT_TextureList *txl = ((OBJECT_Data *)vedata)->txl;
	OBJECT_FramebufferList *fbl = ((OBJECT_Data *)vedata)->fbl;

	const float *viewport_size = DRW_viewport_size_get();

	DRWFboTexture tex[2] = {{&txl->outlines_depth_tx, DRW_BUF_DEPTH_24, 0},
	                        {&txl->outlines_color_tx, DRW_BUF_RGBA_8, DRW_TEX_FILTER}};
	DRW_framebuffer_init(&fbl->outlines,
	                     (int)viewport_size[0], (int)viewport_size[1],
	                     tex, 2);

	DRWFboTexture blur_tex = {&txl->outlines_blur_tx, DRW_BUF_RGBA_8, DRW_TEX_FILTER};
	DRW_framebuffer_init(&fbl->blur,
	                     (int)viewport_size[0], (int)viewport_size[1],
	                     &blur_tex, 1);

	if (!e_data.outline_resolve_sh) {
		e_data.outline_resolve_sh = DRW_shader_create_fullscreen(datatoc_object_outline_resolve_frag_glsl, NULL);
	}

	if (!e_data.outline_detect_sh) {
		e_data.outline_detect_sh = DRW_shader_create_fullscreen(datatoc_object_outline_detect_frag_glsl, NULL);
	}

	if (!e_data.outline_fade_sh) {
		e_data.outline_fade_sh = DRW_shader_create_fullscreen(datatoc_object_outline_expand_frag_glsl, NULL);
	}

	if (!e_data.grid_sh) {
		e_data.grid_sh = DRW_shader_create_with_lib(datatoc_object_grid_vert_glsl, NULL,
		                                            datatoc_object_grid_frag_glsl,
		                                            datatoc_common_globals_lib_glsl, NULL);
	}

	{
		/* Grid precompute */
		float viewinvmat[4][4], winmat[4][4], invwinmat[4][4], viewmat[4][4];
		const bContext *C = DRW_get_context();
		View3D *v3d = CTX_wm_view3d(C);
		Scene *scene = CTX_data_scene(C);
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		float grid_scale = ED_view3d_grid_scale(scene, v3d, NULL);
		float grid_res, offs;

		const bool show_axis_x = v3d->gridflag & V3D_SHOW_X;
		const bool show_axis_y = v3d->gridflag & V3D_SHOW_Y;
		const bool show_axis_z = v3d->gridflag & V3D_SHOW_Z;
		const bool show_floor = (v3d->gridflag & V3D_SHOW_FLOOR);

		DRW_viewport_matrix_get(winmat, DRW_MAT_WIN);
		DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
		DRW_viewport_matrix_get(viewinvmat, DRW_MAT_VIEWINV);

		/* Setup camera pos */
		copy_v3_v3(e_data.camera_pos, viewinvmat[3]);

		/* if perps */
		if (winmat[3][3] == 0.0f) {
			float fov;
			float viewvecs[2][4] = {
			    {1.0f, -1.0f, -1.0f, 1.0f},
			    {-1.0f, 1.0f, -1.0f, 1.0f}
			};
			/* invert the proj matrix */
			invert_m4_m4(invwinmat, winmat);

			/* convert the view vectors to view space */
			for (int i = 0; i < 2; i++) {
				mul_m4_v4(invwinmat, viewvecs[i]);
				mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]); /* normalize */
			}

			fov = angle_v3v3(viewvecs[0], viewvecs[1]) / 2.0f;
			grid_res = fabsf(tanf(fov)) / grid_scale;

			/* Grid matrix polygon offset (fix depth fighting) */
			/* see ED_view3d_polygon_offset */
			offs = winmat[3][2] * -0.0025f;

			e_data.grid_flag = (1 << 4); /* XY plane */
			if (show_axis_x)
				e_data.grid_flag |= SHOW_AXIS_X;
			if (show_axis_y)
				e_data.grid_flag |= SHOW_AXIS_Y;
			if (show_floor)
				e_data.grid_flag |= SHOW_GRID;
		}
		else {
			float viewdist = 1.0f / max_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
			grid_res = viewdist / grid_scale;

			/* Grid matrix polygon offset (fix depth fighting) */
			/* see ED_view3d_polygon_offset */
			offs = 0.00001f * viewdist;
			if (ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT)) {
				e_data.grid_flag = PLANE_YZ;
				e_data.grid_flag |= SHOW_AXIS_Y;
				e_data.grid_flag |= SHOW_AXIS_Z;
				e_data.grid_flag |= SHOW_GRID;
			}
			else if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
				e_data.grid_flag = PLANE_XY;
				e_data.grid_flag |= SHOW_AXIS_X;
				e_data.grid_flag |= SHOW_AXIS_Y;
				e_data.grid_flag |= SHOW_GRID;
			}
			else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
				e_data.grid_flag = PLANE_XZ;
				e_data.grid_flag |= SHOW_AXIS_X;
				e_data.grid_flag |= SHOW_AXIS_Z;
				e_data.grid_flag |= SHOW_GRID;
			}
			else { /* RV3D_VIEW_USER */
				e_data.grid_flag = PLANE_XY;
				if (show_axis_x)
					e_data.grid_flag |= SHOW_AXIS_X;
				if (show_axis_y)
					e_data.grid_flag |= SHOW_AXIS_Y;
				if (show_floor)
					e_data.grid_flag |= SHOW_GRID;
			}
		}

		/* Z axis if needed */
		if (((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) && show_axis_z) {
			e_data.zpos_flag = SHOW_AXIS_Z;

			float zvec[4] = {0.0f, 0.0f, -1.0f, 0.0f};
			mul_m4_v4(viewinvmat, zvec);

			/* z axis : chose the most facing plane */
			if (fabsf(zvec[0]) < fabsf(zvec[1])) {
				e_data.zpos_flag |= PLANE_XZ;
			}
			else {
				e_data.zpos_flag |= PLANE_YZ;
			}

			e_data.zneg_flag = e_data.zpos_flag;

			/* Persp : If camera is below floor plane, we switch clipping
			 * Ortho : If eye vector is looking up, we switch clipping */
			if (((winmat[3][3] == 0.0f) && (e_data.camera_pos[2] > 0.0f)) ||
				((winmat[3][3] != 0.0f) && (zvec[2] < 0.0f))) {
				e_data.zpos_flag |= CLIP_ZPOS;
				e_data.zneg_flag |= CLIP_ZNEG;
			}
			else {
				e_data.zpos_flag |= CLIP_ZNEG;
				e_data.zneg_flag |= CLIP_ZPOS;
			}
		}
		else {
			e_data.zneg_flag = e_data.zpos_flag = CLIP_ZNEG | CLIP_ZPOS;
		}

		winmat[3][2] -= offs;
		mul_m4_m4m4(e_data.grid_mat, winmat, viewmat);

		e_data.grid_settings[0] = v3d->far / 2.0f; /* gridDistance */
		e_data.grid_settings[1] = grid_res; /* gridResolution */
		e_data.grid_settings[2] = grid_scale; /* gridScale */
		e_data.grid_settings[3] = v3d->gridsubdiv; /* gridSubdiv */
		e_data.grid_settings[4] = (v3d->gridsubdiv > 1) ? 1.0f / log(v3d->gridsubdiv) : 0.0; /* 1/log(gridSubdiv) */
	}
}

static void OBJECT_engine_free(void)
{
	if (e_data.outline_resolve_sh)
		DRW_shader_free(e_data.outline_resolve_sh);
	if (e_data.outline_detect_sh)
		DRW_shader_free(e_data.outline_detect_sh);
	if (e_data.outline_fade_sh)
		DRW_shader_free(e_data.outline_fade_sh);
	if (e_data.grid_sh)
		DRW_shader_free(e_data.grid_sh);
}

static DRWShadingGroup *shgroup_outline(DRWPass *pass, const float col[4], struct GPUShader *sh)
{
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", col, 1);

	return grp;
}

static void OBJECT_cache_init(void *vedata)
{
	OBJECT_PassList *psl = ((OBJECT_Data *)vedata)->psl;
	OBJECT_TextureList *txl = ((OBJECT_Data *)vedata)->txl;
	OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(g_data), "g_data");
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_WIRE;
		psl->outlines = DRW_pass_create("Outlines Pass", state);

		struct GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);

		/* Select */
		stl->g_data->outlines_select = shgroup_outline(psl->outlines, ts.colorSelect, sh);
		stl->g_data->outlines_select_group = shgroup_outline(psl->outlines, ts.colorGroupActive, sh);

		/* Transform */
		stl->g_data->outlines_transform = shgroup_outline(psl->outlines, ts.colorTransform, sh);

		/* Active */
		stl->g_data->outlines_active = shgroup_outline(psl->outlines, ts.colorActive, sh);
		stl->g_data->outlines_active_group = shgroup_outline(psl->outlines, ts.colorGroupActive, sh);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR;
		struct Batch *quad = DRW_cache_fullscreen_quad_get();
		static float alphaOcclu = 0.35f;
		static float one = 1.0f;
		static float alpha1 = 5.0f / 6.0f;
		static float alpha2 = 4.0f / 5.0f;
		static float alpha3 = 3.0f / 4.0f;
		static float alpha4 = 2.0f / 3.0f;
		static float alpha5 = 1.0f / 2.0f;
		static bool bTrue = true;
		static bool bFalse = false;

		psl->outlines_search = DRW_pass_create("Outlines Expand Pass", state);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.outline_detect_sh, psl->outlines_search);
		DRW_shgroup_uniform_buffer(grp, "outlineColor", &txl->outlines_color_tx, 0);
		DRW_shgroup_uniform_buffer(grp, "outlineDepth", &txl->outlines_depth_tx, 1);
		DRW_shgroup_uniform_buffer(grp, "sceneDepth", &dtxl->depth, 2);
		DRW_shgroup_uniform_float(grp, "alphaOcclu", &alphaOcclu, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->outlines_expand = DRW_pass_create("Outlines Expand Pass", state);

		grp = DRW_shgroup_create(e_data.outline_fade_sh, psl->outlines_expand);
		DRW_shgroup_uniform_buffer(grp, "outlineColor", &txl->outlines_blur_tx, 0);
		DRW_shgroup_uniform_buffer(grp, "outlineDepth", &txl->outlines_depth_tx, 1);
		DRW_shgroup_uniform_float(grp, "alpha", &one, 1);
		DRW_shgroup_uniform_bool(grp, "doExpand", &bTrue, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->outlines_fade1 = DRW_pass_create("Outlines Fade 1 Pass", state);

		grp = DRW_shgroup_create(e_data.outline_fade_sh, psl->outlines_fade1);
		DRW_shgroup_uniform_buffer(grp, "outlineColor", &txl->outlines_color_tx, 0);
		DRW_shgroup_uniform_buffer(grp, "outlineDepth", &txl->outlines_depth_tx, 1);
		DRW_shgroup_uniform_float(grp, "alpha", &alpha1, 1);
		DRW_shgroup_uniform_bool(grp, "doExpand", &bFalse, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->outlines_fade2 = DRW_pass_create("Outlines Fade 2 Pass", state);

		grp = DRW_shgroup_create(e_data.outline_fade_sh, psl->outlines_fade2);
		DRW_shgroup_uniform_buffer(grp, "outlineColor", &txl->outlines_blur_tx, 0);
		DRW_shgroup_uniform_buffer(grp, "outlineDepth", &txl->outlines_depth_tx, 1);
		DRW_shgroup_uniform_float(grp, "alpha", &alpha2, 1);
		DRW_shgroup_uniform_bool(grp, "doExpand", &bFalse, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->outlines_fade3 = DRW_pass_create("Outlines Fade 3 Pass", state);

		grp = DRW_shgroup_create(e_data.outline_fade_sh, psl->outlines_fade3);
		DRW_shgroup_uniform_buffer(grp, "outlineColor", &txl->outlines_color_tx, 0);
		DRW_shgroup_uniform_buffer(grp, "outlineDepth", &txl->outlines_depth_tx, 1);
		DRW_shgroup_uniform_float(grp, "alpha", &alpha3, 1);
		DRW_shgroup_uniform_bool(grp, "doExpand", &bFalse, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->outlines_fade4 = DRW_pass_create("Outlines Fade 4 Pass", state);

		grp = DRW_shgroup_create(e_data.outline_fade_sh, psl->outlines_fade4);
		DRW_shgroup_uniform_buffer(grp, "outlineColor", &txl->outlines_blur_tx, 0);
		DRW_shgroup_uniform_buffer(grp, "outlineDepth", &txl->outlines_depth_tx, 1);
		DRW_shgroup_uniform_float(grp, "alpha", &alpha4, 1);
		DRW_shgroup_uniform_bool(grp, "doExpand", &bFalse, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->outlines_fade5 = DRW_pass_create("Outlines Fade 5 Pass", state);

		grp = DRW_shgroup_create(e_data.outline_fade_sh, psl->outlines_fade5);
		DRW_shgroup_uniform_buffer(grp, "outlineColor", &txl->outlines_color_tx, 0);
		DRW_shgroup_uniform_buffer(grp, "outlineDepth", &txl->outlines_depth_tx, 1);
		DRW_shgroup_uniform_float(grp, "alpha", &alpha5, 1);
		DRW_shgroup_uniform_bool(grp, "doExpand", &bFalse, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND;
		psl->outlines_resolve = DRW_pass_create("Outlines Resolve Pass", state);

		struct Batch *quad = DRW_cache_fullscreen_quad_get();

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.outline_resolve_sh, psl->outlines_resolve);
		DRW_shgroup_uniform_buffer(grp, "outlineBluredColor", &txl->outlines_blur_tx, 0);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		/* Grid pass */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		psl->grid = DRW_pass_create("Infinite Grid Pass", state);

		struct Batch *quad = DRW_cache_fullscreen_quad_get();
		static float mat[4][4];
		unit_m4(mat);

		/* Create 3 quads to render ordered transparency Z axis */
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.grid_sh, psl->grid);
		DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.zneg_flag, 1);
		DRW_shgroup_uniform_mat4(grp, "ViewProjectionOffsetMatrix", (float *)e_data.grid_mat);
		DRW_shgroup_uniform_vec3(grp, "cameraPos", e_data.camera_pos, 1);
		DRW_shgroup_uniform_vec4(grp, "gridSettings", e_data.grid_settings, 1);
		DRW_shgroup_uniform_float(grp, "gridOneOverLogSubdiv", &e_data.grid_settings[4], 1);
		DRW_shgroup_uniform_block(grp, "globalsBlock", globals_ubo, 0);
		DRW_shgroup_call_add(grp, quad, mat);

		grp = DRW_shgroup_create(e_data.grid_sh, psl->grid);
		DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.grid_flag, 1);
		DRW_shgroup_call_add(grp, quad, mat);

		grp = DRW_shgroup_create(e_data.grid_sh, psl->grid);
		DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.zpos_flag, 1);
		DRW_shgroup_call_add(grp, quad, mat);
	}

	{
		/* Solid bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->bone_solid = DRW_pass_create("Bone Solid Pass", state);
	}

	{
		/* Wire bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		psl->bone_wire = DRW_pass_create("Bone Wire Pass", state);
	}

	{
		/* Non Meshes Pass (Camera, empties, lamps ...) */
		struct Batch *geom;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND | DRW_STATE_POINT;
		state |= DRW_STATE_WIRE;
		psl->non_meshes = DRW_pass_create("Non Meshes Pass", state);

		/* Empties */
		geom = DRW_cache_plain_axes_get();
		stl->g_data->plain_axes = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_cube_get();
		stl->g_data->cube = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_circle_get();
		stl->g_data->circle = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_empty_sphere_get();
		stl->g_data->sphere = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_empty_cone_get();
		stl->g_data->cone = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_arrow_get();
		stl->g_data->single_arrow = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		stl->g_data->single_arrow_line = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_arrows_get();
		stl->g_data->arrows = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_axis_names_get();
		stl->g_data->axis_names = shgroup_instance_axis_names(psl->non_meshes, geom);

		/* Force Field */
		geom = DRW_cache_field_wind_get();
		stl->g_data->field_wind = shgroup_instance_scaled(psl->non_meshes, geom);

		geom = DRW_cache_field_force_get();
		stl->g_data->field_force = shgroup_instance_screen_aligned(psl->non_meshes, geom);

		geom = DRW_cache_field_vortex_get();
		stl->g_data->field_vortex = shgroup_instance_scaled(psl->non_meshes, geom);

		geom = DRW_cache_screenspace_circle_get();
		stl->g_data->field_curve_sta = shgroup_instance_screen_aligned(psl->non_meshes, geom);

		/* Speaker */
		geom = DRW_cache_speaker_get();
		stl->g_data->speaker = shgroup_instance(psl->non_meshes, geom);

		/* Camera */
		geom = DRW_cache_camera_get();
		stl->g_data->camera = shgroup_camera_instance(psl->non_meshes, geom);

		geom = DRW_cache_camera_tria_get();
		stl->g_data->camera_tria = shgroup_camera_instance(psl->non_meshes, geom);

		geom = DRW_cache_plain_axes_get();
		stl->g_data->camera_focus = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		stl->g_data->camera_clip = shgroup_distance_lines_instance(psl->non_meshes, geom);
		stl->g_data->camera_mist = shgroup_distance_lines_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_endpoints_get();
		stl->g_data->camera_clip_points = shgroup_distance_lines_instance(psl->non_meshes, geom);
		stl->g_data->camera_mist_points = shgroup_distance_lines_instance(psl->non_meshes, geom);

		/* Lamps */
		/* TODO
		 * for now we create multiple times the same VBO with only lamp center coordinates
		 * but ideally we would only create it once */

		/* start with buflimit because we don't want stipples */
		geom = DRW_cache_single_line_get();
		stl->g_data->lamp_buflimit = shgroup_distance_lines_instance(psl->non_meshes, geom);

		stl->g_data->lamp_center = shgroup_dynpoints_uniform_color(psl->non_meshes, ts.colorLampNoAlpha, &ts.sizeLampCenter);
		stl->g_data->lamp_center_group = shgroup_dynpoints_uniform_color(psl->non_meshes, ts.colorGroup, &ts.sizeLampCenter);

		geom = DRW_cache_lamp_get();
		stl->g_data->lamp_circle = shgroup_instance_screenspace(psl->non_meshes, geom, &ts.sizeLampCircle);
		stl->g_data->lamp_circle_shadow = shgroup_instance_screenspace(psl->non_meshes, geom, &ts.sizeLampCircleShadow);

		geom = DRW_cache_lamp_sunrays_get();
		stl->g_data->lamp_sunrays = shgroup_instance_screenspace(psl->non_meshes, geom, &ts.sizeLampCircle);

		stl->g_data->lamp_groundline = shgroup_groundlines_uniform_color(psl->non_meshes, ts.colorLamp);
		stl->g_data->lamp_groundpoint = shgroup_groundpoints_uniform_color(psl->non_meshes, ts.colorLamp);

		geom = DRW_cache_lamp_area_get();
		stl->g_data->lamp_area = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_lamp_hemi_get();
		stl->g_data->lamp_hemi = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		stl->g_data->lamp_distance = shgroup_distance_lines_instance(psl->non_meshes, geom);

		geom = DRW_cache_single_line_endpoints_get();
		stl->g_data->lamp_buflimit_points = shgroup_distance_lines_instance(psl->non_meshes, geom);

		geom = DRW_cache_lamp_spot_get();
		stl->g_data->lamp_spot_cone = shgroup_spot_instance(psl->non_meshes, geom);

		geom = DRW_cache_circle_get();
		stl->g_data->lamp_spot_blend = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_lamp_spot_square_get();
		stl->g_data->lamp_spot_pyramid = shgroup_instance(psl->non_meshes, geom);

		geom = DRW_cache_square_get();
		stl->g_data->lamp_spot_blend_rect = shgroup_instance(psl->non_meshes, geom);

		/* -------- STIPPLES ------- */
		/* TODO port to shader stipple */

		/* Relationship Lines */
		stl->g_data->relationship_lines = shgroup_dynlines_uniform_color(psl->non_meshes, ts.colorWire);
		DRW_shgroup_state_set(stl->g_data->relationship_lines, DRW_STATE_STIPPLE_3);

		/* Force Field Curve Guide End (here because of stipple) */
		geom = DRW_cache_screenspace_circle_get();
		stl->g_data->field_curve_end = shgroup_instance_screen_aligned(psl->non_meshes, geom);

		/* Force Field Limits */
		geom = DRW_cache_field_tube_limit_get();
		stl->g_data->field_tube_limit = shgroup_instance_scaled(psl->non_meshes, geom);

		geom = DRW_cache_field_cone_limit_get();
		stl->g_data->field_cone_limit = shgroup_instance_scaled(psl->non_meshes, geom);
	}

	{
		/* Object Center pass grouped by State */
		DRWShadingGroup *grp;
		static float outlineWidth, size;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_POINT;
		psl->ob_center = DRW_pass_create("Obj Center Pass", state);

		outlineWidth = 1.0f * U.pixelsize;
		size = U.obcenter_dia * U.pixelsize + outlineWidth;

		struct GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);

		/* Active */
		grp = DRW_shgroup_point_batch_create(sh, psl->ob_center);
		DRW_shgroup_uniform_float(grp, "size", &size, 1);
		DRW_shgroup_uniform_float(grp, "outlineWidth", &outlineWidth, 1);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorActive, 1);
		DRW_shgroup_uniform_vec4(grp, "outlineColor", ts.colorOutline, 1);
		stl->g_data->center_active = grp;

		/* Select */
		grp = DRW_shgroup_point_batch_create(sh, psl->ob_center);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorSelect, 1);
		stl->g_data->center_selected = grp;

		/* Deselect */
		grp = DRW_shgroup_point_batch_create(sh, psl->ob_center);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorDeselect, 1);
		stl->g_data->center_deselected = grp;

		/* Select (library) */
		grp = DRW_shgroup_point_batch_create(sh, psl->ob_center);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorLibrarySelect, 1);
		stl->g_data->center_selected_lib = grp;

		/* Deselect (library) */
		grp = DRW_shgroup_point_batch_create(sh, psl->ob_center);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorLibrary, 1);
		stl->g_data->center_deselected_lib = grp;
	}
}

static void DRW_shgroup_lamp(OBJECT_StorageList *stl, Object *ob, SceneLayer *sl)
{
	Lamp *la = ob->data;
	float *color;
	int theme_id = DRW_object_wire_theme_get(ob, sl, &color);
	static float zero = 0.0f;

	float **la_mats = (float **)DRW_object_engine_data_get(ob, &draw_engine_object_type);
	if (*la_mats == NULL) {
		/* we need 2 matrices */
		*la_mats = MEM_mallocN(sizeof(float) * 16 * 2, "Lamp Object Mode Matrices");
	}

	float (*shapemat)[4], (*spotblendmat)[4];
	shapemat = (float (*)[4])(*la_mats);
	spotblendmat = (float (*)[4])(*la_mats + 16);

	/* Don't draw the center if it's selected or active */
	if (theme_id == TH_GROUP)
		DRW_shgroup_dynamic_call_add(stl->g_data->lamp_center_group, ob->obmat[3]);
	else if (theme_id == TH_LAMP)
		DRW_shgroup_dynamic_call_add(stl->g_data->lamp_center, ob->obmat[3]);

	/* First circle */
	DRW_shgroup_dynamic_call_add(stl->g_data->lamp_circle, ob->obmat[3], color);

	/* draw dashed outer circle if shadow is on. remember some lamps can't have certain shadows! */
	if (la->type != LA_HEMI) {
		if ((la->mode & LA_SHAD_RAY) || ((la->mode & LA_SHAD_BUF) && (la->type == LA_SPOT))) {
			DRW_shgroup_dynamic_call_add(stl->g_data->lamp_circle_shadow, ob->obmat[3], color);
		}
	}

	/* Distance */
	if (ELEM(la->type, LA_HEMI, LA_SUN, LA_AREA)) {
		DRW_shgroup_dynamic_call_add(stl->g_data->lamp_distance, color, &zero, &la->dist, ob->obmat);
	}

	copy_m4_m4(shapemat, ob->obmat);

	if (la->type == LA_SUN) {
		DRW_shgroup_dynamic_call_add(stl->g_data->lamp_sunrays, ob->obmat[3], color);
	}
	else if (la->type == LA_SPOT) {
		float size[3], sizemat[4][4];
		static float one = 1.0f;
		float blend = 1.0f - pow2f(la->spotblend);

		size[0] = size[1] = sinf(la->spotsize * 0.5f) * la->dist;
		size[2] = cosf(la->spotsize * 0.5f) * la->dist;

		size_to_mat4(sizemat, size);
		mul_m4_m4m4(shapemat, ob->obmat, sizemat);

		size[0] = size[1] = blend; size[2] = 1.0f;
		size_to_mat4(sizemat, size);
		translate_m4(sizemat, 0.0f, 0.0f, -1.0f);
		rotate_m4(sizemat, 'X', M_PI / 2.0f);
		mul_m4_m4m4(spotblendmat, shapemat, sizemat);

		if (la->mode & LA_SQUARE) {
			DRW_shgroup_dynamic_call_add(stl->g_data->lamp_spot_pyramid,    color, &one, shapemat);

			/* hide line if it is zero size or overlaps with outer border,
			 * previously it adjusted to always to show it but that seems
			 * confusing because it doesn't show the actual blend size */
			if (blend != 0.0f && blend != 1.0f) {
				DRW_shgroup_dynamic_call_add(stl->g_data->lamp_spot_blend_rect, color, &one, spotblendmat);
			}
		}
		else {
			DRW_shgroup_dynamic_call_add(stl->g_data->lamp_spot_cone,  color, shapemat);

			/* hide line if it is zero size or overlaps with outer border,
			 * previously it adjusted to always to show it but that seems
			 * confusing because it doesn't show the actual blend size */
			if (blend != 0.0f && blend != 1.0f) {
				DRW_shgroup_dynamic_call_add(stl->g_data->lamp_spot_blend, color, &one, spotblendmat);
			}
		}

		DRW_shgroup_dynamic_call_add(stl->g_data->lamp_buflimit,        color, &la->clipsta, &la->clipend, ob->obmat);
		DRW_shgroup_dynamic_call_add(stl->g_data->lamp_buflimit_points, color, &la->clipsta, &la->clipend, ob->obmat);
	}
	else if (la->type == LA_HEMI) {
		static float hemisize = 2.0f;
		DRW_shgroup_dynamic_call_add(stl->g_data->lamp_hemi, color, &hemisize, shapemat);
	}
	else if (la->type == LA_AREA) {
		float size[3] = {1.0f, 1.0f, 1.0f}, sizemat[4][4];

		if (la->area_shape == LA_AREA_RECT) {
			size[1] = la->area_sizey / la->area_size;
			size_to_mat4(sizemat, size);
			mul_m4_m4m4(shapemat, shapemat, sizemat);
		}

		DRW_shgroup_dynamic_call_add(stl->g_data->lamp_area, color, &la->area_size, shapemat);
	}

	/* Line and point going to the ground */
	DRW_shgroup_dynamic_call_add(stl->g_data->lamp_groundline, ob->obmat[3]);
	DRW_shgroup_dynamic_call_add(stl->g_data->lamp_groundpoint, ob->obmat[3]);
}

static void DRW_shgroup_camera(OBJECT_StorageList *stl, Object *ob, SceneLayer *sl)
{
	const struct bContext *C = DRW_get_context();
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);

	Camera *cam = ob->data;
	const bool is_active = (ob == v3d->camera);
	float *color;
	DRW_object_wire_theme_get(ob, sl, &color);

	float vec[4][3], asp[2], shift[2], scale[3], drawsize;

	scale[0] = 1.0f / len_v3(ob->obmat[0]);
	scale[1] = 1.0f / len_v3(ob->obmat[1]);
	scale[2] = 1.0f / len_v3(ob->obmat[2]);

	BKE_camera_view_frame_ex(scene, cam, cam->drawsize, false, scale,
	                         asp, shift, &drawsize, vec);

	// /* Frame coords */
	copy_v2_v2(cam->drwcorners[0], vec[0]);
	copy_v2_v2(cam->drwcorners[1], vec[1]);
	copy_v2_v2(cam->drwcorners[2], vec[2]);
	copy_v2_v2(cam->drwcorners[3], vec[3]);

	/* depth */
	cam->drwdepth = vec[0][2];

	/* tria */
	cam->drwtria[0][0] = shift[0] + ((0.7f * drawsize) * scale[0]);
	cam->drwtria[0][1] = shift[1] + ((drawsize * (asp[1] + 0.1f)) * scale[1]);
	cam->drwtria[1][0] = shift[0];
	cam->drwtria[1][1] = shift[1] + ((1.1f * drawsize * (asp[1] + 0.7f)) * scale[1]);

	DRW_shgroup_dynamic_call_add(stl->g_data->camera, color, cam->drwcorners, &cam->drwdepth, cam->drwtria, ob->obmat);

	/* Active cam */
	if (is_active) {
		DRW_shgroup_dynamic_call_add(stl->g_data->camera_tria, color, cam->drwcorners, &cam->drwdepth, cam->drwtria, ob->obmat);
	}

	/* draw the rest in normalize object space */
	copy_m4_m4(cam->drwnormalmat, ob->obmat);
	normalize_m4(cam->drwnormalmat);

	if (cam->flag & CAM_SHOWLIMITS) {
		static float col[3] = {0.5f, 0.5f, 0.25f}, col_hi[3] = {1.0f, 1.0f, 0.5f};
		float sizemat[4][4], size[3] = {1.0f, 1.0f, 0.0f};
		float focusdist = BKE_camera_object_dof_distance(ob);

		copy_m4_m4(cam->drwfocusmat, cam->drwnormalmat);
		translate_m4(cam->drwfocusmat, 0.0f, 0.0f, -focusdist);
		size_to_mat4(sizemat, size);
		mul_m4_m4m4(cam->drwfocusmat, cam->drwfocusmat, sizemat);

		DRW_shgroup_dynamic_call_add(stl->g_data->camera_focus, (is_active ? col_hi : col), &cam->drawsize, cam->drwfocusmat);

		DRW_shgroup_dynamic_call_add(stl->g_data->camera_clip, color, &cam->clipsta, &cam->clipend, cam->drwnormalmat);
		DRW_shgroup_dynamic_call_add(stl->g_data->camera_clip_points, (is_active ? col_hi : col), &cam->clipsta, &cam->clipend, cam->drwnormalmat);
	}

	if (cam->flag & CAM_SHOWMIST) {
		World *world = scene->world;

		if (world) {
			static float col[3] = {0.5f, 0.5f, 0.5f}, col_hi[3] = {1.0f, 1.0f, 1.0f};
			world->mistend = world->miststa + world->mistdist;
			DRW_shgroup_dynamic_call_add(stl->g_data->camera_mist,        color, &world->miststa, &world->mistend, cam->drwnormalmat);
			DRW_shgroup_dynamic_call_add(stl->g_data->camera_mist_points, (is_active ? col_hi : col), &world->miststa, &world->mistend, cam->drwnormalmat);
		}
	}
}

static void DRW_shgroup_empty(OBJECT_StorageList *stl, Object *ob, SceneLayer *sl)
{
	float *color;
	DRW_object_wire_theme_get(ob, sl, &color);

	switch (ob->empty_drawtype) {
		case OB_PLAINAXES:
			DRW_shgroup_dynamic_call_add(stl->g_data->plain_axes, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_SINGLE_ARROW:
			DRW_shgroup_dynamic_call_add(stl->g_data->single_arrow, color, &ob->empty_drawsize, ob->obmat);
			DRW_shgroup_dynamic_call_add(stl->g_data->single_arrow_line, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_CUBE:
			DRW_shgroup_dynamic_call_add(stl->g_data->cube, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_CIRCLE:
			DRW_shgroup_dynamic_call_add(stl->g_data->circle, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_EMPTY_SPHERE:
			DRW_shgroup_dynamic_call_add(stl->g_data->sphere, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_EMPTY_CONE:
			DRW_shgroup_dynamic_call_add(stl->g_data->cone, color, &ob->empty_drawsize, ob->obmat);
			break;
		case OB_ARROWS:
			DRW_shgroup_dynamic_call_add(stl->g_data->arrows, color, &ob->empty_drawsize, ob->obmat);
			DRW_shgroup_dynamic_call_add(stl->g_data->axis_names, color, &ob->empty_drawsize, ob->obmat);
			break;
	}
}

static void DRW_shgroup_forcefield(OBJECT_StorageList *stl, Object *ob, SceneLayer *sl)
{
	int theme_id = DRW_object_wire_theme_get(ob, sl, NULL);
	float *color = DRW_color_background_blend_get(theme_id);
	PartDeflect *pd = ob->pd;
	Curve *cu = (ob->type == OB_CURVE) ? ob->data : NULL;

	/* TODO Move this to depsgraph */
	float tmp[3];
	copy_v3_fl(pd->drawvec1, ob->empty_drawsize);

	switch (pd->forcefield) {
		case PFIELD_WIND:
			pd->drawvec1[2] = pd->f_strength;
			break;
		case PFIELD_VORTEX:
			if (pd->f_strength < 0.0f) {
				pd->drawvec1[1] = -pd->drawvec1[1];
			}
			break;
		case PFIELD_GUIDE:
			if (cu && (cu->flag & CU_PATH) && ob->curve_cache->path && ob->curve_cache->path->data) {
				where_on_path(ob, 0.0f, pd->drawvec1, tmp, NULL, NULL, NULL);
				where_on_path(ob, 1.0f, pd->drawvec2, tmp, NULL, NULL, NULL);
			}
			break;
	}

	if (pd->falloff == PFIELD_FALL_TUBE) {
		pd->drawvec_falloff_max[0] = pd->drawvec_falloff_max[1] = (pd->flag & PFIELD_USEMAXR) ? pd->maxrad : 1.0f;
		pd->drawvec_falloff_max[2] = (pd->flag & PFIELD_USEMAX) ? pd->maxdist : 0.0f;

		pd->drawvec_falloff_min[0] = pd->drawvec_falloff_min[1] = (pd->flag & PFIELD_USEMINR) ? pd->minrad : 1.0f;
		pd->drawvec_falloff_min[2] = (pd->flag & PFIELD_USEMIN) ? pd->mindist : 0.0f;
	}
	else if (pd->falloff == PFIELD_FALL_CONE) {
		float radius, distance;

		radius = DEG2RADF((pd->flag & PFIELD_USEMAXR) ? pd->maxrad : 1.0f);
		distance = (pd->flag & PFIELD_USEMAX) ? pd->maxdist : 0.0f;
		pd->drawvec_falloff_max[0] = pd->drawvec_falloff_max[1] = distance * sinf(radius);
		pd->drawvec_falloff_max[2] = distance * cosf(radius);

		radius = DEG2RADF((pd->flag & PFIELD_USEMINR) ? pd->minrad : 1.0f);
		distance = (pd->flag & PFIELD_USEMIN) ? pd->mindist : 0.0f;

		pd->drawvec_falloff_min[0] = pd->drawvec_falloff_min[1] = distance * sinf(radius);
		pd->drawvec_falloff_min[2] = distance * cosf(radius);
	}
	/* End of things that should go to depthgraph */

	switch (pd->forcefield) {
		case PFIELD_WIND:
			DRW_shgroup_dynamic_call_add(stl->g_data->field_wind, color, &pd->drawvec1, ob->obmat);
			break;
		case PFIELD_FORCE:
			DRW_shgroup_dynamic_call_add(stl->g_data->field_force, color, &pd->drawvec1, ob->obmat);
			break;
		case PFIELD_VORTEX:
			DRW_shgroup_dynamic_call_add(stl->g_data->field_vortex, color, &pd->drawvec1, ob->obmat);
			break;
		case PFIELD_GUIDE:
			if (cu && (cu->flag & CU_PATH) && ob->curve_cache->path && ob->curve_cache->path->data) {
				DRW_shgroup_dynamic_call_add(stl->g_data->field_curve_sta, color, &pd->f_strength, ob->obmat);
				DRW_shgroup_dynamic_call_add(stl->g_data->field_curve_end, color, &pd->f_strength, ob->obmat);
			}
			break;
	}

	if (pd->falloff == PFIELD_FALL_SPHERE) {
		/* as last, guide curve alters it */
		if ((pd->flag & PFIELD_USEMAX) != 0) {
			DRW_shgroup_dynamic_call_add(stl->g_data->field_curve_end, color, &pd->maxdist, ob->obmat);
		}

		if ((pd->flag & PFIELD_USEMIN) != 0) {
			DRW_shgroup_dynamic_call_add(stl->g_data->field_curve_end, color, &pd->mindist, ob->obmat);
		}
	}
	else if (pd->falloff == PFIELD_FALL_TUBE) {
		if (pd->flag & (PFIELD_USEMAX | PFIELD_USEMAXR)) {
			DRW_shgroup_dynamic_call_add(stl->g_data->field_tube_limit, color, &pd->drawvec_falloff_max, ob->obmat);
		}

		if (pd->flag & (PFIELD_USEMIN | PFIELD_USEMINR)) {
			DRW_shgroup_dynamic_call_add(stl->g_data->field_tube_limit, color, &pd->drawvec_falloff_min, ob->obmat);
		}
	}
	else if (pd->falloff == PFIELD_FALL_CONE) {
		if (pd->flag & (PFIELD_USEMAX | PFIELD_USEMAXR)) {
			DRW_shgroup_dynamic_call_add(stl->g_data->field_cone_limit, color, &pd->drawvec_falloff_max, ob->obmat);
		}

		if (pd->flag & (PFIELD_USEMIN | PFIELD_USEMINR)) {
			DRW_shgroup_dynamic_call_add(stl->g_data->field_cone_limit, color, &pd->drawvec_falloff_min, ob->obmat);
		}
	}
}

static void DRW_shgroup_speaker(OBJECT_StorageList *stl, Object *ob, SceneLayer *sl)
{
	float *color;
	static float one = 1.0f;
	DRW_object_wire_theme_get(ob, sl, &color);

	DRW_shgroup_dynamic_call_add(stl->g_data->speaker, color, &one, ob->obmat);
}

static void DRW_shgroup_relationship_lines(OBJECT_StorageList *stl, Object *ob)
{
	if (ob->parent) {
		DRW_shgroup_dynamic_call_add(stl->g_data->relationship_lines, ob->obmat[3]);
		DRW_shgroup_dynamic_call_add(stl->g_data->relationship_lines, ob->parent->obmat[3]);
	}
}

static void DRW_shgroup_object_center(OBJECT_StorageList *stl, Object *ob)
{
	const bool is_library = ob->id.us > 1 || ID_IS_LINKED_DATABLOCK(ob);
	DRWShadingGroup *shgroup;

	if ((ob->base_flag & BASE_SELECTED) != 0) {
		if (is_library) {
			shgroup = stl->g_data->center_selected_lib;
		}
		else {
			shgroup = stl->g_data->center_selected;
		}
	}
	else {
		if (is_library) {
			shgroup = stl->g_data->center_deselected_lib;
		}
		else {
			shgroup = stl->g_data->center_deselected;
		}
	}

	DRW_shgroup_dynamic_call_add(shgroup, ob->obmat[3]);
}

static void OBJECT_cache_populate(void *vedata, Object *ob)
{
	OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;
	const struct bContext *C = DRW_get_context();
	Scene *scene = CTX_data_scene(C);
	SceneLayer *sl = CTX_data_scene_layer(C);

	//CollectionEngineSettings *ces_mode_ob = BKE_object_collection_engine_get(ob, COLLECTION_MODE_OBJECT, "");

	//bool do_wire = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_wire");
	bool do_outlines = ((ob->base_flag & BASE_SELECTED) != 0);

	switch (ob->type) {
		case OB_MESH:
			{
				Object *obedit = scene->obedit;
				int theme_id = DRW_object_wire_theme_get(ob, sl, NULL);
				if (ob != obedit) {
					if (do_outlines) {
						struct Batch *geom = DRW_cache_mesh_surface_get(ob);
						switch (theme_id) {
							case TH_ACTIVE:
								DRW_shgroup_call_add(stl->g_data->outlines_active, geom, ob->obmat);
								break;
							case TH_SELECT:
								DRW_shgroup_call_add(stl->g_data->outlines_select, geom, ob->obmat);
								break;
							case TH_GROUP_ACTIVE:
								DRW_shgroup_call_add(stl->g_data->outlines_select_group, geom, ob->obmat);
								break;
							case TH_TRANSFORM:
								DRW_shgroup_call_add(stl->g_data->outlines_transform, geom, ob->obmat);
								break;
						}
					}
				}
			}
			break;
		case OB_LAMP:
			DRW_shgroup_lamp(stl, ob, sl);
			break;
		case OB_CAMERA:
			DRW_shgroup_camera(stl, ob, sl);
			break;
		case OB_EMPTY:
			DRW_shgroup_empty(stl, ob, sl);
			break;
		case OB_SPEAKER:
			DRW_shgroup_speaker(stl, ob, sl);
			break;
		case OB_ARMATURE:
			{
				bArmature *arm = ob->data;
				if (arm->edbo == NULL) {
					DRW_shgroup_armature_object(ob, sl, ((OBJECT_Data *)vedata)->psl->bone_solid,
					                                    ((OBJECT_Data *)vedata)->psl->bone_wire,
					                                    stl->g_data->relationship_lines);
				}
			}
			break;
		default:
			break;
	}

	if (ob->pd && ob->pd->forcefield) {
		DRW_shgroup_forcefield(stl, ob, sl);
	}

	DRW_shgroup_object_center(stl, ob);
	DRW_shgroup_relationship_lines(stl, ob);
}

static void OBJECT_draw_scene(void *vedata)
{

	OBJECT_PassList *psl = ((OBJECT_Data *)vedata)->psl;
	OBJECT_FramebufferList *fbl = ((OBJECT_Data *)vedata)->fbl;
	OBJECT_TextureList *txl = ((OBJECT_Data *)vedata)->txl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	float clearcol[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	/* Render filled polygon on a separate framebuffer */
	DRW_framebuffer_bind(fbl->outlines);
	DRW_framebuffer_clear(true, true, false, clearcol, 1.0f);
	DRW_draw_pass(psl->outlines);

	/* detach textures */
	DRW_framebuffer_texture_detach(txl->outlines_depth_tx);

	/* Search outline pixels */
	DRW_framebuffer_bind(fbl->blur);
	DRW_draw_pass(psl->outlines_search);

	/* Expand and fade gradually */
	DRW_framebuffer_bind(fbl->outlines);
	DRW_draw_pass(psl->outlines_expand);

	DRW_framebuffer_bind(fbl->blur);
	DRW_draw_pass(psl->outlines_fade1);

	DRW_framebuffer_bind(fbl->outlines);
	DRW_draw_pass(psl->outlines_fade2);

	DRW_framebuffer_bind(fbl->blur);
	DRW_draw_pass(psl->outlines_fade3);

	DRW_framebuffer_bind(fbl->outlines);
	DRW_draw_pass(psl->outlines_fade4);

	DRW_framebuffer_bind(fbl->blur);
	DRW_draw_pass(psl->outlines_fade5);

	/* reattach */
	DRW_framebuffer_texture_attach(fbl->outlines, txl->outlines_depth_tx, 0);
	DRW_framebuffer_bind(dfbl->default_fb);

	/* This needs to be drawn after the oultine */
	DRW_draw_pass(psl->bone_wire);
	DRW_draw_pass(psl->bone_solid);
	DRW_draw_pass(psl->non_meshes);
	DRW_draw_pass(psl->ob_center);
	DRW_draw_pass(psl->grid);

	/* Combine with scene buffer last */
	DRW_draw_pass(psl->outlines_resolve);
}

void OBJECT_collection_settings_create(IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_MODE_OBJECT);
	BKE_collection_engine_property_add_int(props, "show_wire", false);
	BKE_collection_engine_property_add_int(props, "show_backface_culling", false);
}

static const DrawEngineDataSize OBJECT_data_size = DRW_VIEWPORT_DATA_SIZE(OBJECT_Data);

DrawEngineType draw_engine_object_type = {
	NULL, NULL,
	N_("ObjectMode"),
	&OBJECT_data_size,
	&OBJECT_engine_init,
	&OBJECT_engine_free,
	&OBJECT_cache_init,
	&OBJECT_cache_populate,
	NULL,
	NULL,
	&OBJECT_draw_scene
};
