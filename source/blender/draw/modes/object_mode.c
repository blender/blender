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
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_smoke_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BIF_gl.h"

#include "BLI_string_utils.h"

#include "BKE_anim.h"
#include "BKE_camera.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_movieclip.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_tracking.h"

#include "ED_view3d.h"

#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "MEM_guardedalloc.h"

#include "UI_resources.h"

#include "draw_mode_engines.h"
#include "draw_manager_text.h"
#include "draw_common.h"

#include "DEG_depsgraph_query.h"

extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GPUTexture *globals_ramp; /* draw_common.c */
extern GlobalsUboStorage ts;

extern char datatoc_object_outline_prepass_vert_glsl[];
extern char datatoc_object_outline_prepass_geom_glsl[];
extern char datatoc_object_outline_prepass_frag_glsl[];
extern char datatoc_object_outline_resolve_frag_glsl[];
extern char datatoc_object_outline_detect_frag_glsl[];
extern char datatoc_object_outline_expand_frag_glsl[];
extern char datatoc_object_grid_frag_glsl[];
extern char datatoc_object_grid_vert_glsl[];
extern char datatoc_object_empty_image_frag_glsl[];
extern char datatoc_object_empty_image_vert_glsl[];
extern char datatoc_object_lightprobe_grid_vert_glsl[];
extern char datatoc_object_particle_prim_vert_glsl[];
extern char datatoc_object_particle_dot_vert_glsl[];
extern char datatoc_object_particle_dot_frag_glsl[];
extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_fxaa_lib_glsl[];
extern char datatoc_gpu_shader_flat_color_frag_glsl[];
extern char datatoc_gpu_shader_flat_id_frag_glsl[];
extern char datatoc_common_fullscreen_vert_glsl[];
extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

/* *********** LISTS *********** */
typedef struct OBJECT_PassList {
	struct DRWPass *non_meshes[2];
	struct DRWPass *ob_center;
	struct DRWPass *outlines;
	struct DRWPass *outlines_search;
	struct DRWPass *outlines_expand;
	struct DRWPass *outlines_bleed;
	struct DRWPass *outlines_resolve;
	struct DRWPass *grid;
	struct DRWPass *bone_solid[2];
	struct DRWPass *bone_outline[2];
	struct DRWPass *bone_wire[2];
	struct DRWPass *bone_envelope[2];
	struct DRWPass *bone_axes[2];
	struct DRWPass *particle;
	struct DRWPass *lightprobes;
} OBJECT_PassList;

typedef struct OBJECT_FramebufferList {
	struct GPUFrameBuffer *outlines_fb;
	struct GPUFrameBuffer *blur_fb;
	struct GPUFrameBuffer *expand_fb;
	struct GPUFrameBuffer *ghost_fb;
} OBJECT_FramebufferList;

typedef struct OBJECT_StorageList {
	struct OBJECT_PrivateData *g_data;
} OBJECT_StorageList;

typedef struct OBJECT_Data {
	void *engine_type;
	OBJECT_FramebufferList *fbl;
	DRWViewportEmptyList *txl;
	OBJECT_PassList *psl;
	OBJECT_StorageList *stl;
} OBJECT_Data;

/* *********** STATIC *********** */

typedef struct OBJECT_ShadingGroupList {
	/* Reference only */
	struct DRWPass *non_meshes;
	struct DRWPass *bone_solid;
	struct DRWPass *bone_outline;
	struct DRWPass *bone_wire;
	struct DRWPass *bone_envelope;
	struct DRWPass *bone_axes;

	/* Empties */
	DRWShadingGroup *plain_axes;
	DRWShadingGroup *cube;
	DRWShadingGroup *circle;
	DRWShadingGroup *sphere;
	DRWShadingGroup *sphere_solid;
	DRWShadingGroup *cylinder;
	DRWShadingGroup *capsule_cap;
	DRWShadingGroup *capsule_body;
	DRWShadingGroup *cone;
	DRWShadingGroup *single_arrow;
	DRWShadingGroup *single_arrow_line;
	DRWShadingGroup *empty_axes;
	/* GPUTexture -> EmptyImageShadingGroupData */
	GHash *image_plane_map;

	/* Force Field */
	DRWShadingGroup *field_wind;
	DRWShadingGroup *field_force;
	DRWShadingGroup *field_vortex;
	DRWShadingGroup *field_curve_sta;
	DRWShadingGroup *field_curve_end;
	DRWShadingGroup *field_tube_limit;
	DRWShadingGroup *field_cone_limit;

	/* Grease Pencil */
	DRWShadingGroup *gpencil_axes;

	/* Speaker */
	DRWShadingGroup *speaker;

	/* Probe */
	DRWShadingGroup *probe_cube;
	DRWShadingGroup *probe_planar;
	DRWShadingGroup *probe_grid;

	/* MetaBalls */
	DRWShadingGroup *mball_handle;

	/* Lamps */
	DRWShadingGroup *lamp_center;
	DRWShadingGroup *lamp_groundpoint;
	DRWShadingGroup *lamp_groundline;
	DRWShadingGroup *lamp_circle;
	DRWShadingGroup *lamp_circle_shadow;
	DRWShadingGroup *lamp_sunrays;
	DRWShadingGroup *lamp_distance;
	DRWShadingGroup *lamp_buflimit;
	DRWShadingGroup *lamp_buflimit_points;
	DRWShadingGroup *lamp_area_sphere;
	DRWShadingGroup *lamp_area_square;
	DRWShadingGroup *lamp_area_disk;
	DRWShadingGroup *lamp_hemi;
	DRWShadingGroup *lamp_spot_cone;
	DRWShadingGroup *lamp_spot_blend;
	DRWShadingGroup *lamp_spot_pyramid;
	DRWShadingGroup *lamp_spot_blend_rect;

	/* Helpers */
	DRWShadingGroup *relationship_lines;

	/* Camera */
	DRWShadingGroup *camera;
	DRWShadingGroup *camera_frame;
	DRWShadingGroup *camera_tria;
	DRWShadingGroup *camera_focus;
	DRWShadingGroup *camera_clip;
	DRWShadingGroup *camera_clip_points;
	DRWShadingGroup *camera_mist;
	DRWShadingGroup *camera_mist_points;
	ListBase camera_path;

	/* Wire */
	DRWShadingGroup *wire;
	DRWShadingGroup *wire_active;
	DRWShadingGroup *wire_select;
	DRWShadingGroup *wire_transform;

	/* Points */
	DRWShadingGroup *points;
	DRWShadingGroup *points_active;
	DRWShadingGroup *points_select;
	DRWShadingGroup *points_transform;

	/* Texture Space */
	DRWShadingGroup *texspace;
} OBJECT_ShadingGroupList;

typedef struct OBJECT_PrivateData {
	OBJECT_ShadingGroupList sgl;
	OBJECT_ShadingGroupList sgl_ghost;

	/* Outlines */
	DRWShadingGroup *outlines_active;
	DRWShadingGroup *outlines_select;
	DRWShadingGroup *outlines_transform;

	/* Lightprobes */
	DRWShadingGroup *lightprobes_cube_select;
	DRWShadingGroup *lightprobes_cube_active;
	DRWShadingGroup *lightprobes_cube_transform;

	DRWShadingGroup *lightprobes_planar_select;
	DRWShadingGroup *lightprobes_planar_active;
	DRWShadingGroup *lightprobes_planar_transform;

	/* Objects Centers */
	DRWShadingGroup *center_active;
	DRWShadingGroup *center_selected;
	DRWShadingGroup *center_deselected;
	DRWShadingGroup *center_selected_lib;
	DRWShadingGroup *center_deselected_lib;

	/* Outlines id offset */
	int id_ofs_active;
	int id_ofs_select;
	int id_ofs_transform;
	int id_ofs_prb_active;
	int id_ofs_prb_select;
	int id_ofs_prb_transform;
} OBJECT_PrivateData; /* Transient data */

static struct {
	/* Instance Data format */
	struct GPUVertFormat *particle_format;
	struct GPUVertFormat *empty_image_format;
	struct GPUVertFormat *empty_image_wire_format;

	/* fullscreen shaders */
	GPUShader *outline_prepass_sh;
	GPUShader *outline_prepass_wire_sh;
	GPUShader *outline_resolve_sh;
	GPUShader *outline_resolve_aa_sh;
	GPUShader *outline_detect_sh;
	GPUShader *outline_detect_wire_sh;
	GPUShader *outline_fade_sh;

	/* regular shaders */
	GPUShader *object_empty_image_sh;
	GPUShader *object_empty_image_wire_sh;
	GPUShader *grid_sh;
	GPUShader *part_dot_sh;
	GPUShader *part_prim_sh;
	GPUShader *part_axis_sh;
	GPUShader *lightprobe_grid_sh;
	float camera_pos[3];
	float screenvecs[3][4];
	float grid_settings[5];
	int grid_flag;
	float grid_normal[3];
	float grid_axes[3];
	int zpos_flag;
	int zneg_flag;
	float zplane_normal[3];
	float zplane_axes[3];
	float inv_viewport_size[2];
	bool draw_grid;
	/* Temp buffer textures */
	struct GPUTexture *outlines_depth_tx;
	struct GPUTexture *outlines_id_tx;
	struct GPUTexture *outlines_color_tx;
	struct GPUTexture *outlines_blur_tx;

	ListBase smoke_domains;
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
	GRID_BACK    = (1 << 9),
};

/* Prototypes. */
static void DRW_shgroup_empty_ex(OBJECT_ShadingGroupList *sgl, float mat[4][4], float *draw_size, char draw_type, float *color);

/* *********** FUNCTIONS *********** */

static void OBJECT_engine_init(void *vedata)
{
	OBJECT_FramebufferList *fbl = ((OBJECT_Data *)vedata)->fbl;

	const float *viewport_size = DRW_viewport_size_get();
	const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

	if (DRW_state_is_fbo()) {
		e_data.outlines_depth_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_DEPTH_COMPONENT24,
		                                                     &draw_engine_object_type);
		/* XXX TODO GPU_R16UI can overflow, it would cause no harm
		 * (only bad colored or missing outlines) but we should
		 * use 32bits only if the scene have that many objects */
		e_data.outlines_id_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_R16UI,
		                                                  &draw_engine_object_type);

		GPU_framebuffer_ensure_config(&fbl->outlines_fb, {
			GPU_ATTACHMENT_TEXTURE(e_data.outlines_depth_tx),
			GPU_ATTACHMENT_TEXTURE(e_data.outlines_id_tx)
		});

		e_data.outlines_color_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA8,
		                                                     &draw_engine_object_type);

		GPU_framebuffer_ensure_config(&fbl->expand_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE(e_data.outlines_color_tx)
		});

		e_data.outlines_blur_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA8,
		                                                    &draw_engine_object_type);

		GPU_framebuffer_ensure_config(&fbl->blur_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE(e_data.outlines_blur_tx)
		});
	}

	/* Shaders */
	if (!e_data.outline_resolve_sh) {
		/* Outline */
		e_data.outline_prepass_sh = DRW_shader_create_3D(datatoc_object_outline_prepass_frag_glsl, NULL);

		e_data.outline_prepass_wire_sh = DRW_shader_create(
		            datatoc_object_outline_prepass_vert_glsl,
		            datatoc_object_outline_prepass_geom_glsl,
		            datatoc_object_outline_prepass_frag_glsl, NULL);

		e_data.outline_resolve_sh = DRW_shader_create_fullscreen(datatoc_object_outline_resolve_frag_glsl, NULL);

		e_data.outline_resolve_aa_sh = DRW_shader_create_with_lib(
		            datatoc_common_fullscreen_vert_glsl, NULL,
		            datatoc_object_outline_resolve_frag_glsl,
		            datatoc_common_fxaa_lib_glsl,
		            "#define FXAA_ALPHA\n"
		            "#define USE_FXAA\n");

		e_data.outline_detect_sh = DRW_shader_create_with_lib(
		            datatoc_common_fullscreen_vert_glsl, NULL,
		            datatoc_object_outline_detect_frag_glsl,
		            datatoc_common_globals_lib_glsl,
		            NULL);

		e_data.outline_detect_wire_sh = DRW_shader_create_with_lib(
		            datatoc_common_fullscreen_vert_glsl, NULL,
		            datatoc_object_outline_detect_frag_glsl,
		            datatoc_common_globals_lib_glsl,
		            "#define WIRE\n");


		e_data.outline_fade_sh = DRW_shader_create_fullscreen(datatoc_object_outline_expand_frag_glsl, NULL);

		/* Empty images */
		e_data.object_empty_image_sh = DRW_shader_create(
		           datatoc_object_empty_image_vert_glsl, NULL,
		           datatoc_object_empty_image_frag_glsl, NULL);

		e_data.object_empty_image_wire_sh = DRW_shader_create(
		           datatoc_object_empty_image_vert_glsl, NULL,
		           datatoc_object_empty_image_frag_glsl,
		           "#define USE_WIRE\n");

		/* Grid */
		e_data.grid_sh = DRW_shader_create_with_lib(
		        datatoc_object_grid_vert_glsl, NULL,
		        datatoc_object_grid_frag_glsl,
		        datatoc_common_globals_lib_glsl, NULL);

		/* Particles */
		e_data.part_prim_sh = DRW_shader_create(
		        datatoc_object_particle_prim_vert_glsl, NULL, datatoc_gpu_shader_flat_color_frag_glsl, NULL);

		e_data.part_axis_sh = DRW_shader_create(
		        datatoc_object_particle_prim_vert_glsl, NULL, datatoc_gpu_shader_flat_color_frag_glsl,
		        "#define USE_AXIS\n");

		e_data.part_dot_sh = DRW_shader_create(
		        datatoc_object_particle_dot_vert_glsl, NULL, datatoc_object_particle_dot_frag_glsl, NULL);

		/* Lightprobes */
		e_data.lightprobe_grid_sh = DRW_shader_create(
		        datatoc_object_lightprobe_grid_vert_glsl, NULL, datatoc_gpu_shader_flat_id_frag_glsl, NULL);
	}

	{
		/* Grid precompute */
		float invviewmat[4][4], invwinmat[4][4];
		float viewmat[4][4], winmat[4][4];
		const DRWContextState *draw_ctx = DRW_context_state_get();
		View3D *v3d = draw_ctx->v3d;
		Scene *scene = draw_ctx->scene;
		RegionView3D *rv3d = draw_ctx->rv3d;
		float grid_scale = ED_view3d_grid_scale(scene, v3d, NULL);
		float grid_res;

		const bool show_axis_x = (v3d->gridflag & V3D_SHOW_X) != 0;
		const bool show_axis_y = (v3d->gridflag & V3D_SHOW_Y) != 0;
		const bool show_axis_z = (v3d->gridflag & V3D_SHOW_Z) != 0;
		const bool show_floor = (v3d->gridflag & V3D_SHOW_FLOOR) != 0;
		e_data.draw_grid = show_axis_x || show_axis_y || show_axis_z || show_floor;

		DRW_viewport_matrix_get(winmat, DRW_MAT_WIN);
		DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
		DRW_viewport_matrix_get(invwinmat, DRW_MAT_WININV);
		DRW_viewport_matrix_get(invviewmat, DRW_MAT_VIEWINV);

		/* Setup camera pos */
		copy_v3_v3(e_data.camera_pos, invviewmat[3]);

		/* if perps */
		if (winmat[3][3] == 0.0f) {
			float fov;
			float viewvecs[2][4] = {
			    {1.0f, -1.0f, -1.0f, 1.0f},
			    {-1.0f, 1.0f, -1.0f, 1.0f}
			};

			/* convert the view vectors to view space */
			for (int i = 0; i < 2; i++) {
				mul_m4_v4(invwinmat, viewvecs[i]);
				mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]); /* perspective divide */
			}

			fov = angle_v3v3(viewvecs[0], viewvecs[1]) / 2.0f;
			grid_res = fabsf(tanf(fov)) / grid_scale;

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

			if (ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT)) {
				e_data.grid_flag = PLANE_YZ;
				if (show_axis_y) {
					e_data.grid_flag |= SHOW_AXIS_Y;
				}
				if (show_axis_z) {
					e_data.grid_flag |= SHOW_AXIS_Z;
				}
				if (show_floor) {
					e_data.grid_flag |= SHOW_GRID;
					e_data.grid_flag |= GRID_BACK;
				}
			}
			else if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
				e_data.grid_flag = PLANE_XY;
				if (show_axis_x) {
					e_data.grid_flag |= SHOW_AXIS_X;
				}
				if (show_axis_y) {
					e_data.grid_flag |= SHOW_AXIS_Y;
				}
				if (show_floor) {
					e_data.grid_flag |= SHOW_GRID;
					e_data.grid_flag |= GRID_BACK;
				}
			}
			else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
				e_data.grid_flag = PLANE_XZ;
				if (show_axis_x) {
					e_data.grid_flag |= SHOW_AXIS_X;
				}
				if (show_axis_z) {
					e_data.grid_flag |= SHOW_AXIS_Z;
				}
				if (show_floor) {
					e_data.grid_flag |= SHOW_GRID;
					e_data.grid_flag |= GRID_BACK;
				}
			}
			else { /* RV3D_VIEW_USER */
				e_data.grid_flag = PLANE_XY;
				if (show_axis_x) {
					e_data.grid_flag |= SHOW_AXIS_X;
				}
				if (show_axis_y) {
					e_data.grid_flag |= SHOW_AXIS_Y;
				}
				if (show_floor) {
					e_data.grid_flag |= SHOW_GRID;
				}
			}
		}

		e_data.grid_normal[0] = (float)((e_data.grid_flag & PLANE_YZ) != 0);
		e_data.grid_normal[1] = (float)((e_data.grid_flag & PLANE_XZ) != 0);
		e_data.grid_normal[2] = (float)((e_data.grid_flag & PLANE_XY) != 0);

		e_data.grid_axes[0] = (float)((e_data.grid_flag & (PLANE_XZ | PLANE_XY)) != 0);
		e_data.grid_axes[1] = (float)((e_data.grid_flag & (PLANE_YZ | PLANE_XY)) != 0);
		e_data.grid_axes[2] = (float)((e_data.grid_flag & (PLANE_YZ | PLANE_XZ)) != 0);

		/* Vectors to recover pixel world position. Fix grid precision issue. */
		/* Using pixel at z = 0.0f in ndc space : gives average precision between
		 * near and far plane. Note that it might not be the best choice. */
		copy_v4_fl4(e_data.screenvecs[0],  1.0f, -1.0f, 0.0f, 1.0f);
		copy_v4_fl4(e_data.screenvecs[1], -1.0f,  1.0f, 0.0f, 1.0f);
		copy_v4_fl4(e_data.screenvecs[2], -1.0f, -1.0f, 0.0f, 1.0f);

		for (int i = 0; i < 3; i++) {
			/* Doing 2 steps to recover world position of the corners of the frustum.
			 * Using the inverse perspective matrix is giving very low precision output. */
			mul_m4_v4(invwinmat, e_data.screenvecs[i]);
			e_data.screenvecs[i][0] /= e_data.screenvecs[i][3]; /* perspective divide */
			e_data.screenvecs[i][1] /= e_data.screenvecs[i][3]; /* perspective divide */
			e_data.screenvecs[i][2] /= e_data.screenvecs[i][3]; /* perspective divide */
			e_data.screenvecs[i][3] = 1.0f;
			/* main instability come from this one */
			/* TODO : to make things even more stable, don't use
			 * invviewmat and derive vectors from camera properties */
			mul_m4_v4(invviewmat, e_data.screenvecs[i]);
		}

		sub_v3_v3(e_data.screenvecs[0], e_data.screenvecs[2]);
		sub_v3_v3(e_data.screenvecs[1], e_data.screenvecs[2]);

		/* Z axis if needed */
		if (((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) && show_axis_z) {
			e_data.zpos_flag = SHOW_AXIS_Z;

			float zvec[4] = {0.0f, 0.0f, -1.0f, 0.0f};
			mul_m4_v4(invviewmat, zvec);

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
			    ((winmat[3][3] != 0.0f) && (zvec[2] < 0.0f)))
			{
				e_data.zpos_flag |= CLIP_ZPOS;
				e_data.zneg_flag |= CLIP_ZNEG;
			}
			else {
				e_data.zpos_flag |= CLIP_ZNEG;
				e_data.zneg_flag |= CLIP_ZPOS;
			}

			e_data.zplane_normal[0] = (float)((e_data.zpos_flag & PLANE_YZ) != 0);
			e_data.zplane_normal[1] = (float)((e_data.zpos_flag & PLANE_XZ) != 0);
			e_data.zplane_normal[2] = (float)((e_data.zpos_flag & PLANE_XY) != 0);

			e_data.zplane_axes[0] = (float)((e_data.zpos_flag & (PLANE_XZ | PLANE_XY)) != 0);
			e_data.zplane_axes[1] = (float)((e_data.zpos_flag & (PLANE_YZ | PLANE_XY)) != 0);
			e_data.zplane_axes[2] = (float)((e_data.zpos_flag & (PLANE_YZ | PLANE_XZ)) != 0);

		}
		else {
			e_data.zneg_flag = e_data.zpos_flag = CLIP_ZNEG | CLIP_ZPOS;
		}

		float dist;
		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			Object *camera_object = DEG_get_evaluated_object(draw_ctx->depsgraph, v3d->camera);
			dist = ((Camera *)camera_object)->clipend;
		}
		else {
			dist = v3d->far;
		}

		e_data.grid_settings[0] = dist / 2.0f; /* gridDistance */
		e_data.grid_settings[1] = grid_res; /* gridResolution */
		e_data.grid_settings[2] = grid_scale; /* gridScale */
		e_data.grid_settings[3] = v3d->gridsubdiv; /* gridSubdiv */
		e_data.grid_settings[4] = (v3d->gridsubdiv > 1) ? 1.0f / logf(v3d->gridsubdiv) : 0.0f; /* 1/log(gridSubdiv) */
	}

	copy_v2_v2(e_data.inv_viewport_size, DRW_viewport_size_get());
	invert_v2(e_data.inv_viewport_size);
}

static void OBJECT_engine_free(void)
{
	MEM_SAFE_FREE(e_data.particle_format);
	MEM_SAFE_FREE(e_data.empty_image_format);
	MEM_SAFE_FREE(e_data.empty_image_wire_format);
	DRW_SHADER_FREE_SAFE(e_data.outline_prepass_sh);
	DRW_SHADER_FREE_SAFE(e_data.outline_prepass_wire_sh);
	DRW_SHADER_FREE_SAFE(e_data.outline_resolve_sh);
	DRW_SHADER_FREE_SAFE(e_data.outline_resolve_aa_sh);
	DRW_SHADER_FREE_SAFE(e_data.outline_detect_sh);
	DRW_SHADER_FREE_SAFE(e_data.outline_detect_wire_sh);
	DRW_SHADER_FREE_SAFE(e_data.outline_fade_sh);
	DRW_SHADER_FREE_SAFE(e_data.object_empty_image_sh);
	DRW_SHADER_FREE_SAFE(e_data.object_empty_image_wire_sh);
	DRW_SHADER_FREE_SAFE(e_data.grid_sh);
	DRW_SHADER_FREE_SAFE(e_data.part_prim_sh);
	DRW_SHADER_FREE_SAFE(e_data.part_axis_sh);
	DRW_SHADER_FREE_SAFE(e_data.part_dot_sh);
	DRW_SHADER_FREE_SAFE(e_data.lightprobe_grid_sh);
}

static DRWShadingGroup *shgroup_outline(DRWPass *pass, const int *ofs, GPUShader *sh)
{
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_int(grp, "baseId", ofs, 1);

	return grp;
}

/* currently same as 'shgroup_outline', new function to avoid confustion */
static DRWShadingGroup *shgroup_wire(DRWPass *pass, const float col[4], GPUShader *sh)
{
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", col, 1);

	return grp;
}

/* currently same as 'shgroup_outline', new function to avoid confustion */
static DRWShadingGroup *shgroup_points(DRWPass *pass, const float col[4], GPUShader *sh)
{
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_vec4(grp, "color", col, 1);

	return grp;
}

static int *shgroup_theme_id_to_probe_outline_counter(
        OBJECT_StorageList *stl, int theme_id)
{
	switch (theme_id) {
		case TH_ACTIVE:
			return &stl->g_data->id_ofs_prb_active;
		case TH_SELECT:
			return &stl->g_data->id_ofs_prb_select;
		case TH_TRANSFORM:
		default:
			return &stl->g_data->id_ofs_prb_transform;
	}
}

static int *shgroup_theme_id_to_outline_counter(
        OBJECT_StorageList *stl, int theme_id)
{
	switch (theme_id) {
		case TH_ACTIVE:
			return &stl->g_data->id_ofs_active;
		case TH_SELECT:
			return &stl->g_data->id_ofs_select;
		case TH_TRANSFORM:
		default:
			return &stl->g_data->id_ofs_transform;
	}
}

static DRWShadingGroup *shgroup_theme_id_to_probe_planar_outline_shgrp(
        OBJECT_StorageList *stl, int theme_id)
{
	/* does not increment counter */
	switch (theme_id) {
		case TH_ACTIVE:
			return stl->g_data->lightprobes_planar_active;
		case TH_SELECT:
			return stl->g_data->lightprobes_planar_select;
		case TH_TRANSFORM:
		default:
			return stl->g_data->lightprobes_planar_transform;
	}
}

static DRWShadingGroup *shgroup_theme_id_to_probe_cube_outline_shgrp(
        OBJECT_StorageList *stl, int theme_id)
{
	/* does not increment counter */
	switch (theme_id) {
		case TH_ACTIVE:
			return stl->g_data->lightprobes_cube_active;
		case TH_SELECT:
			return stl->g_data->lightprobes_cube_select;
		case TH_TRANSFORM:
		default:
			return stl->g_data->lightprobes_cube_transform;
	}
}

static DRWShadingGroup *shgroup_theme_id_to_outline_or(
        OBJECT_StorageList *stl, int theme_id, DRWShadingGroup *fallback)
{
	int *counter = shgroup_theme_id_to_outline_counter(stl, theme_id);
	*counter += 1;

	switch (theme_id) {
		case TH_ACTIVE:
			return stl->g_data->outlines_active;
		case TH_SELECT:
			return stl->g_data->outlines_select;
		case TH_TRANSFORM:
			return stl->g_data->outlines_transform;
		default:
			return fallback;
	}
}

static DRWShadingGroup *shgroup_theme_id_to_wire_or(
        OBJECT_ShadingGroupList *sgl, int theme_id, DRWShadingGroup *fallback)
{
	switch (theme_id) {
		case TH_ACTIVE:
			return sgl->wire_active;
		case TH_SELECT:
			return sgl->wire_select;
		case TH_TRANSFORM:
			return sgl->wire_transform;
		default:
			return fallback;
	}
}

static DRWShadingGroup *shgroup_theme_id_to_point_or(
        OBJECT_ShadingGroupList *sgl, int theme_id, DRWShadingGroup *fallback)
{
	switch (theme_id) {
		case TH_ACTIVE:
			return sgl->points_active;
		case TH_SELECT:
			return sgl->points_select;
		case TH_TRANSFORM:
			return sgl->points_transform;
		default:
			return fallback;
	}
}

static void image_calc_aspect(Image *ima, ImageUser *iuser, float r_image_aspect[2])
{
	float ima_x, ima_y;
	if (ima) {
		int w, h;
		BKE_image_get_size(ima, iuser, &w, &h);
		ima_x = w;
		ima_y = h;
	}
	else {
		/* if no image, make it a 1x1 empty square, honor scale & offset */
		ima_x = ima_y = 1.0f;
	}
	/* Get the image aspect even if the buffer is invalid */
	float sca_x = 1.0f, sca_y = 1.0f;
	if (ima) {
		if (ima->aspx > ima->aspy) {
			sca_y = ima->aspy / ima->aspx;
		}
		else if (ima->aspx < ima->aspy) {
			sca_x = ima->aspx / ima->aspy;
		}
	}

	const float scale_x_inv = ima_x * sca_x;
	const float scale_y_inv = ima_y * sca_y;
	if (scale_x_inv > scale_y_inv) {
		r_image_aspect[0] = 1.0f;
		r_image_aspect[1] = scale_y_inv / scale_x_inv;
	}
	else {
		r_image_aspect[0] = scale_x_inv / scale_y_inv;
		r_image_aspect[1] = 1.0f;
	}
}

/* per-image shading groups for image-type empty objects */
struct EmptyImageShadingGroupData {
	DRWShadingGroup *shgrp_image;
	DRWShadingGroup *shgrp_wire;
	float image_aspect[2];
};

static void DRW_shgroup_empty_image(
        OBJECT_ShadingGroupList *sgl, Object *ob, const float color[3])
{
	/* TODO: 'StereoViews', see draw_empty_image. */

	if (sgl->image_plane_map == NULL) {
		sgl->image_plane_map = BLI_ghash_ptr_new(__func__);
	}

	struct EmptyImageShadingGroupData *empty_image_data;

	GPUTexture *tex = ob->data ?
	        GPU_texture_from_blender(ob->data, ob->iuser, GL_TEXTURE_2D, false, 0.0f) : NULL;
	void **val_p;

	/* Create on demand, 'tex' may be NULL. */
	if (BLI_ghash_ensure_p(sgl->image_plane_map, tex, &val_p)) {
		empty_image_data = *val_p;
	}
	else {
		empty_image_data = MEM_mallocN(sizeof(*empty_image_data), __func__);

		image_calc_aspect(ob->data, ob->iuser, empty_image_data->image_aspect);

		if (tex) {
			DRW_shgroup_instance_format(e_data.empty_image_format, {
				{"objectColor",         DRW_ATTRIB_FLOAT, 4},
				{"size",                DRW_ATTRIB_FLOAT, 1},
				{"offset",              DRW_ATTRIB_FLOAT, 2},
				{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16},
			});

			struct GPUBatch *geom = DRW_cache_image_plane_get();
			DRWShadingGroup *grp = DRW_shgroup_instance_create(
			        e_data.object_empty_image_sh, sgl->non_meshes, geom, e_data.empty_image_format);
			DRW_shgroup_uniform_texture(grp, "image", tex);
			DRW_shgroup_uniform_vec2(grp, "aspect", empty_image_data->image_aspect, 1);

			empty_image_data->shgrp_image = grp;
		}
		else {
			empty_image_data->shgrp_image = NULL;
		}

		{
			DRW_shgroup_instance_format(e_data.empty_image_wire_format, {
				{"color",               DRW_ATTRIB_FLOAT, 4},
				{"size",                DRW_ATTRIB_FLOAT, 1},
				{"offset",              DRW_ATTRIB_FLOAT, 2},
				{"InstanceModelMatrix", DRW_ATTRIB_FLOAT, 16}
			});

			struct GPUBatch *geom = DRW_cache_image_plane_wire_get();
			DRWShadingGroup *grp = DRW_shgroup_instance_create(
			        e_data.object_empty_image_wire_sh, sgl->non_meshes, geom, e_data.empty_image_wire_format);
			DRW_shgroup_uniform_vec2(grp, "aspect", empty_image_data->image_aspect, 1);

			empty_image_data->shgrp_wire = grp;
		}

		*val_p = empty_image_data;
	}

	if (empty_image_data->shgrp_image != NULL) {
		DRW_shgroup_call_dynamic_add(
		        empty_image_data->shgrp_image,
		        ob->col,
		        &ob->empty_drawsize,
		        ob->ima_ofs,
		        ob->obmat);
	}

	DRW_shgroup_call_dynamic_add(
	        empty_image_data->shgrp_wire,
	        color,
	        &ob->empty_drawsize,
	        ob->ima_ofs,
	        ob->obmat);
}

static void OBJECT_cache_init(void *vedata)
{
	OBJECT_PassList *psl = ((OBJECT_Data *)vedata)->psl;
	OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
	OBJECT_PrivateData *g_data;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const bool xray_enabled = ((draw_ctx->v3d->shading.flag & V3D_SHADING_XRAY) != 0) &&
	                           (draw_ctx->v3d->shading.type < OB_MATERIAL);
	/* TODO : use dpi setting for enabling the second pass */
	const bool do_outline_expand = false;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	g_data = stl->g_data;

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WIRE;
		psl->outlines = DRW_pass_create("Outlines Depth Pass", state);

		GPUShader *sh = e_data.outline_prepass_sh;

		if (xray_enabled) {
			sh = e_data.outline_prepass_wire_sh;
		}

		g_data->outlines_select = shgroup_outline(psl->outlines, &g_data->id_ofs_select, sh);
		g_data->outlines_transform = shgroup_outline(psl->outlines, &g_data->id_ofs_transform, sh);
		g_data->outlines_active = shgroup_outline(psl->outlines, &g_data->id_ofs_active, sh);

		g_data->id_ofs_select = 0;
		g_data->id_ofs_active = 0;
		g_data->id_ofs_transform = 0;
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_POINT;
		DRWPass *pass = psl->lightprobes = DRW_pass_create("Object Probe Pass", state);
		struct GPUBatch *sphere = DRW_cache_sphere_get();
		struct GPUBatch *quad = DRW_cache_quad_get();

		/* Cubemap */
		g_data->lightprobes_cube_select       = shgroup_instance_outline(pass, sphere, &g_data->id_ofs_prb_select);
		g_data->lightprobes_cube_active       = shgroup_instance_outline(pass, sphere, &g_data->id_ofs_prb_active);
		g_data->lightprobes_cube_transform    = shgroup_instance_outline(pass, sphere, &g_data->id_ofs_prb_transform);

		/* Planar */
		g_data->lightprobes_planar_select       = shgroup_instance_outline(pass, quad, &g_data->id_ofs_prb_select);
		g_data->lightprobes_planar_active       = shgroup_instance_outline(pass, quad, &g_data->id_ofs_prb_active);
		g_data->lightprobes_planar_transform    = shgroup_instance_outline(pass, quad, &g_data->id_ofs_prb_transform);

		g_data->id_ofs_prb_select = 0;
		g_data->id_ofs_prb_active = 0;
		g_data->id_ofs_prb_transform = 0;
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR;
		struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();
		/* Don't occlude the "outline" detection pass if in xray mode (too much flickering). */
		float alphaOcclu = (xray_enabled) ? 1.0f : 0.35f;
		/* Reminder : bool uniforms need to be 4 bytes. */
		static const int bTrue = true;
		static const int bFalse = false;

		psl->outlines_search = DRW_pass_create("Outlines Detect Pass", state);

		GPUShader *sh = (xray_enabled) ? e_data.outline_detect_wire_sh : e_data.outline_detect_sh;
		DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->outlines_search);
		DRW_shgroup_uniform_texture_ref(grp, "outlineId", &e_data.outlines_id_tx);
		DRW_shgroup_uniform_texture_ref(grp, "outlineDepth", &e_data.outlines_depth_tx);
		DRW_shgroup_uniform_texture_ref(grp, "sceneDepth", &dtxl->depth);
		DRW_shgroup_uniform_block(grp, "globalsBlock", globals_ubo);
		DRW_shgroup_uniform_float_copy(grp, "alphaOcclu", alphaOcclu);
		DRW_shgroup_uniform_int(grp, "idOffsets", &stl->g_data->id_ofs_active, 3);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->outlines_expand = DRW_pass_create("Outlines Expand Pass", state);

		grp = DRW_shgroup_create(e_data.outline_fade_sh, psl->outlines_expand);
		DRW_shgroup_uniform_texture_ref(grp, "outlineColor", &e_data.outlines_blur_tx);
		DRW_shgroup_uniform_bool(grp, "doExpand", (do_outline_expand) ? &bTrue : &bFalse, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->outlines_bleed = DRW_pass_create("Outlines Bleed Pass", state);

		if (do_outline_expand) {
			grp = DRW_shgroup_create(e_data.outline_fade_sh, psl->outlines_bleed);
			DRW_shgroup_uniform_texture_ref(grp, "outlineColor", &e_data.outlines_color_tx);
			DRW_shgroup_uniform_bool(grp, "doExpand", &bFalse, 1);
			DRW_shgroup_call_add(grp, quad, NULL);
		}
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND;
		psl->outlines_resolve = DRW_pass_create("Outlines Resolve Pass", state);

		struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();
		GPUTexture **outline_tx = (do_outline_expand) ? &e_data.outlines_blur_tx : &e_data.outlines_color_tx;

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.outline_resolve_aa_sh, psl->outlines_resolve);
		DRW_shgroup_uniform_texture_ref(grp, "outlineBluredColor", outline_tx);
		DRW_shgroup_uniform_vec2(grp, "rcpDimensions", e_data.inv_viewport_size, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		/* Grid pass */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND;
		psl->grid = DRW_pass_create("Infinite Grid Pass", state);

		struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();
		static float mat[4][4];
		unit_m4(mat);

		/* Create 3 quads to render ordered transparency Z axis */
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.grid_sh, psl->grid);
		DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.zneg_flag, 1);
		DRW_shgroup_uniform_vec3(grp, "planeNormal", e_data.zplane_normal, 1);
		DRW_shgroup_uniform_vec3(grp, "planeAxes", e_data.zplane_axes, 1);
		DRW_shgroup_uniform_vec3(grp, "cameraPos", e_data.camera_pos, 1);
		DRW_shgroup_uniform_vec4(grp, "screenvecs[0]", e_data.screenvecs[0], 3);
		DRW_shgroup_uniform_vec4(grp, "gridSettings", e_data.grid_settings, 1);
		DRW_shgroup_uniform_float(grp, "gridOneOverLogSubdiv", &e_data.grid_settings[4], 1);
		DRW_shgroup_uniform_block(grp, "globalsBlock", globals_ubo);
		DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
		DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
		DRW_shgroup_call_add(grp, quad, mat);

		grp = DRW_shgroup_create(e_data.grid_sh, psl->grid);
		DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.grid_flag, 1);
		DRW_shgroup_uniform_vec3(grp, "planeNormal", e_data.grid_normal, 1);
		DRW_shgroup_uniform_vec3(grp, "planeAxes", e_data.grid_axes, 1);
		DRW_shgroup_uniform_block(grp, "globalsBlock", globals_ubo);
		DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
		DRW_shgroup_call_add(grp, quad, mat);

		grp = DRW_shgroup_create(e_data.grid_sh, psl->grid);
		DRW_shgroup_uniform_int(grp, "gridFlag", &e_data.zpos_flag, 1);
		DRW_shgroup_uniform_vec3(grp, "planeNormal", e_data.zplane_normal, 1);
		DRW_shgroup_uniform_vec3(grp, "planeAxes", e_data.zplane_axes, 1);
		DRW_shgroup_uniform_block(grp, "globalsBlock", globals_ubo);
		DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
		DRW_shgroup_call_add(grp, quad, mat);
	}

	for (int i = 0; i < 2; ++i) {
		OBJECT_ShadingGroupList *sgl = (i == 1) ? &stl->g_data->sgl_ghost : &stl->g_data->sgl;

		/* Solid bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
		sgl->bone_solid = psl->bone_solid[i] = DRW_pass_create("Bone Solid Pass", state);
		sgl->bone_outline = psl->bone_outline[i] = DRW_pass_create("Bone Outline Pass", state);

		/* Wire bones */
		state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND;
		sgl->bone_wire = psl->bone_wire[i] = DRW_pass_create("Bone Wire Pass", state);

		/* distance outline around envelope bones */
		state = DRW_STATE_ADDITIVE | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_FRONT;
		sgl->bone_envelope = psl->bone_envelope[i] = DRW_pass_create("Bone Envelope Outline Pass", state);

		state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WIRE;
		sgl->bone_axes = psl->bone_axes[i] = DRW_pass_create("Bone Axes Pass", state);
	}

	for (int i = 0; i < 2; ++i) {
		OBJECT_ShadingGroupList *sgl = (i == 1) ? &stl->g_data->sgl_ghost : &stl->g_data->sgl;

		/* Non Meshes Pass (Camera, empties, lamps ...) */
		struct GPUBatch *geom;
		struct GPUShader *sh;

		DRWState state =
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
		        DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND | DRW_STATE_POINT;
		state |= DRW_STATE_WIRE;
		sgl->non_meshes = psl->non_meshes[i] = DRW_pass_create("Non Meshes Pass", state);

		/* Empties */
		geom = DRW_cache_plain_axes_get();
		sgl->plain_axes = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_empty_cube_get();
		sgl->cube = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_circle_get();
		sgl->circle = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_empty_sphere_get();
		sgl->sphere = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_sphere_get();
		sgl->sphere_solid = shgroup_instance_solid(sgl->non_meshes, geom);

		geom = DRW_cache_empty_cylinder_get();
		sgl->cylinder = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_empty_capsule_cap_get();
		sgl->capsule_cap = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_empty_capsule_body_get();
		sgl->capsule_body = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_empty_cone_get();
		sgl->cone = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_single_arrow_get();
		sgl->single_arrow = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		sgl->single_arrow_line = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_bone_arrows_get();
		sgl->empty_axes = shgroup_instance_empty_axes(sgl->non_meshes, geom);

		/* initialize on first use */
		sgl->image_plane_map = NULL;

		/* Force Field */
		geom = DRW_cache_field_wind_get();
		sgl->field_wind = shgroup_instance_scaled(sgl->non_meshes, geom);

		geom = DRW_cache_field_force_get();
		sgl->field_force = shgroup_instance_screen_aligned(sgl->non_meshes, geom);

		geom = DRW_cache_field_vortex_get();
		sgl->field_vortex = shgroup_instance_scaled(sgl->non_meshes, geom);

		geom = DRW_cache_screenspace_circle_get();
		sgl->field_curve_sta = shgroup_instance_screen_aligned(sgl->non_meshes, geom);

		/* Grease Pencil */
		geom = DRW_cache_gpencil_axes_get();
		sgl->gpencil_axes = shgroup_instance(sgl->non_meshes, geom);

		/* Speaker */
		geom = DRW_cache_speaker_get();
		sgl->speaker = shgroup_instance(sgl->non_meshes, geom);

		/* Probe */
		static float probeSize = 14.0f;
		geom = DRW_cache_lightprobe_cube_get();
		sgl->probe_cube = shgroup_instance_screenspace(sgl->non_meshes, geom, &probeSize);

		geom = DRW_cache_lightprobe_grid_get();
		sgl->probe_grid = shgroup_instance_screenspace(sgl->non_meshes, geom, &probeSize);

		static float probePlanarSize = 20.0f;
		geom = DRW_cache_lightprobe_planar_get();
		sgl->probe_planar = shgroup_instance_screenspace(sgl->non_meshes, geom, &probePlanarSize);

		/* Camera */
		geom = DRW_cache_camera_get();
		sgl->camera = shgroup_camera_instance(sgl->non_meshes, geom);

		geom = DRW_cache_camera_frame_get();
		sgl->camera_frame = shgroup_camera_instance(sgl->non_meshes, geom);

		geom = DRW_cache_camera_tria_get();
		sgl->camera_tria = shgroup_camera_instance(sgl->non_meshes, geom);

		geom = DRW_cache_plain_axes_get();
		sgl->camera_focus = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		sgl->camera_clip = shgroup_distance_lines_instance(sgl->non_meshes, geom);
		sgl->camera_mist = shgroup_distance_lines_instance(sgl->non_meshes, geom);

		geom = DRW_cache_single_line_endpoints_get();
		sgl->camera_clip_points = shgroup_distance_lines_instance(sgl->non_meshes, geom);
		sgl->camera_mist_points = shgroup_distance_lines_instance(sgl->non_meshes, geom);

		BLI_listbase_clear(&sgl->camera_path);

		/* Texture Space */
		geom = DRW_cache_empty_cube_get();
		sgl->texspace = shgroup_instance(sgl->non_meshes, geom);

		/* Wires (for loose edges) */
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
		sgl->wire = shgroup_wire(sgl->non_meshes, ts.colorWire, sh);
		sgl->wire_select = shgroup_wire(sgl->non_meshes, ts.colorSelect, sh);
		sgl->wire_transform = shgroup_wire(sgl->non_meshes, ts.colorTransform, sh);
		sgl->wire_active = shgroup_wire(sgl->non_meshes, ts.colorActive, sh);

		/* Points (loose points) */
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_FIXED_SIZE_UNIFORM_COLOR);
		sgl->points = shgroup_points(sgl->non_meshes, ts.colorWire, sh);
		sgl->points_select = shgroup_points(sgl->non_meshes, ts.colorSelect, sh);
		sgl->points_transform = shgroup_points(sgl->non_meshes, ts.colorTransform, sh);
		sgl->points_active = shgroup_points(sgl->non_meshes, ts.colorActive, sh);

		/* Metaballs Handles */
		sgl->mball_handle = shgroup_instance_mball_handles(sgl->non_meshes);

		/* Lamps */
		/* TODO
		 * for now we create multiple times the same VBO with only lamp center coordinates
		 * but ideally we would only create it once */

		/* start with buflimit because we don't want stipples */
		geom = DRW_cache_single_line_get();
		sgl->lamp_buflimit = shgroup_distance_lines_instance(sgl->non_meshes, geom);

		sgl->lamp_center = shgroup_dynpoints_uniform_color(sgl->non_meshes, ts.colorLampNoAlpha, &ts.sizeLampCenter);

		geom = DRW_cache_lamp_get();
		sgl->lamp_circle = shgroup_instance_screenspace(sgl->non_meshes, geom, &ts.sizeLampCircle);
		geom = DRW_cache_lamp_shadows_get();
		sgl->lamp_circle_shadow = shgroup_instance_screenspace(sgl->non_meshes, geom, &ts.sizeLampCircleShadow);

		geom = DRW_cache_lamp_sunrays_get();
		sgl->lamp_sunrays = shgroup_instance_screenspace(sgl->non_meshes, geom, &ts.sizeLampCircle);

		sgl->lamp_groundline = shgroup_groundlines_uniform_color(sgl->non_meshes, ts.colorLamp);
		sgl->lamp_groundpoint = shgroup_groundpoints_uniform_color(sgl->non_meshes, ts.colorLamp);

		geom = DRW_cache_screenspace_circle_get();
		sgl->lamp_area_sphere = shgroup_instance_screen_aligned(sgl->non_meshes, geom);

		geom = DRW_cache_lamp_area_square_get();
		sgl->lamp_area_square = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_lamp_area_disk_get();
		sgl->lamp_area_disk = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_lamp_hemi_get();
		sgl->lamp_hemi = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_single_line_get();
		sgl->lamp_distance = shgroup_distance_lines_instance(sgl->non_meshes, geom);

		geom = DRW_cache_single_line_endpoints_get();
		sgl->lamp_buflimit_points = shgroup_distance_lines_instance(sgl->non_meshes, geom);

		geom = DRW_cache_lamp_spot_get();
		sgl->lamp_spot_cone = shgroup_spot_instance(sgl->non_meshes, geom);

		geom = DRW_cache_circle_get();
		sgl->lamp_spot_blend = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_lamp_spot_square_get();
		sgl->lamp_spot_pyramid = shgroup_instance(sgl->non_meshes, geom);

		geom = DRW_cache_square_get();
		sgl->lamp_spot_blend_rect = shgroup_instance(sgl->non_meshes, geom);

		/* -------- STIPPLES ------- */

		/* Relationship Lines */
		sgl->relationship_lines = shgroup_dynlines_dashed_uniform_color(sgl->non_meshes, ts.colorWire);

		/* Force Field Curve Guide End (here because of stipple) */
		/* TODO port to shader stipple */
		geom = DRW_cache_screenspace_circle_get();
		sgl->field_curve_end = shgroup_instance_screen_aligned(sgl->non_meshes, geom);

		/* Force Field Limits */
		/* TODO port to shader stipple */
		geom = DRW_cache_field_tube_limit_get();
		sgl->field_tube_limit = shgroup_instance_scaled(sgl->non_meshes, geom);

		/* TODO port to shader stipple */
		geom = DRW_cache_field_cone_limit_get();
		sgl->field_cone_limit = shgroup_instance_scaled(sgl->non_meshes, geom);
	}

	{
		/* Object Center pass grouped by State */
		DRWShadingGroup *grp;
		static float outlineWidth, size;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_POINT;
		psl->ob_center = DRW_pass_create("Obj Center Pass", state);

		outlineWidth = 1.0f * U.pixelsize;
		size = U.obcenter_dia * U.pixelsize + outlineWidth;

		GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);

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

	{
		/* Particle Pass */
		psl->particle = DRW_pass_create(
		        "Particle Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
		        DRW_STATE_POINT | DRW_STATE_BLEND);
	}
}

static void DRW_shgroup_mball_handles(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
	MetaBall *mb = ob->data;

	float *color;
	DRW_object_wire_theme_get(ob, view_layer, &color);

	float draw_scale_xform[3][4]; /* Matrix of Scale and Translation */
	{
		float scamat[3][3];
		copy_m3_m4(scamat, ob->obmat);
		/* Get the normalized inverse matrix to extract only
		* the scale of Scamat */
		float iscamat[3][3];
		invert_m3_m3(iscamat, scamat);
		normalize_m3(iscamat);
		mul_m3_m3_post(scamat, iscamat);

		copy_v3_v3(draw_scale_xform[0], scamat[0]);
		copy_v3_v3(draw_scale_xform[1], scamat[1]);
		copy_v3_v3(draw_scale_xform[2], scamat[2]);
	}

	for (MetaElem *ml = mb->elems.first; ml != NULL; ml = ml->next) {
		/* draw radius */
		float world_pos[3];
		mul_v3_m4v3(world_pos, ob->obmat, &ml->x);
		draw_scale_xform[0][3] = world_pos[0];
		draw_scale_xform[1][3] = world_pos[1];
		draw_scale_xform[2][3] = world_pos[2];

		DRW_shgroup_call_dynamic_add(sgl->mball_handle, draw_scale_xform, &ml->rad, color);
	}
}

static void DRW_shgroup_lamp(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
	Lamp *la = ob->data;
	float *color;
	int theme_id = DRW_object_wire_theme_get(ob, view_layer, &color);
	static float zero = 0.0f;

	typedef struct LampEngineData {
		DrawData dd;
		float shape_mat[4][4];
		float spot_blend_mat[4][4];
	} LampEngineData;

	LampEngineData *lamp_engine_data =
	        (LampEngineData *)DRW_drawdata_ensure(
	                &ob->id,
	                &draw_engine_object_type,
	                sizeof(LampEngineData),
	                NULL,
	                NULL);

	float (*shapemat)[4] = lamp_engine_data->shape_mat;
	float (*spotblendmat)[4] = lamp_engine_data->spot_blend_mat;

	/* Don't draw the center if it's selected or active */
	if (theme_id == TH_LAMP)
		DRW_shgroup_call_dynamic_add(sgl->lamp_center, ob->obmat[3]);

	/* First circle */
	DRW_shgroup_call_dynamic_add(sgl->lamp_circle, ob->obmat[3], color);

	/* draw dashed outer circle for shadow */
	if (la->type != LA_HEMI) {
		DRW_shgroup_call_dynamic_add(sgl->lamp_circle_shadow, ob->obmat[3], color);
	}

	/* Distance */
	if (ELEM(la->type, LA_HEMI, LA_SUN, LA_AREA)) {
		DRW_shgroup_call_dynamic_add(sgl->lamp_distance, color, &zero, &la->dist, ob->obmat);
	}

	copy_m4_m4(shapemat, ob->obmat);

	if (la->type == LA_SUN) {
		DRW_shgroup_call_dynamic_add(sgl->lamp_sunrays, ob->obmat[3], color);
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
		rotate_m4(sizemat, 'X', (float)(M_PI / 2));
		mul_m4_m4m4(spotblendmat, shapemat, sizemat);

		if (la->mode & LA_SQUARE) {
			DRW_shgroup_call_dynamic_add(sgl->lamp_spot_pyramid, color, &one, shapemat);

			/* hide line if it is zero size or overlaps with outer border,
			 * previously it adjusted to always to show it but that seems
			 * confusing because it doesn't show the actual blend size */
			if (blend != 0.0f && blend != 1.0f) {
				DRW_shgroup_call_dynamic_add(sgl->lamp_spot_blend_rect, color, &one, spotblendmat);
			}
		}
		else {
			DRW_shgroup_call_dynamic_add(sgl->lamp_spot_cone, color, shapemat);

			/* hide line if it is zero size or overlaps with outer border,
			 * previously it adjusted to always to show it but that seems
			 * confusing because it doesn't show the actual blend size */
			if (blend != 0.0f && blend != 1.0f) {
				DRW_shgroup_call_dynamic_add(sgl->lamp_spot_blend, color, &one, spotblendmat);
			}
		}

		DRW_shgroup_call_dynamic_add(sgl->lamp_buflimit, color, &la->clipsta, &la->clipend, ob->obmat);
		DRW_shgroup_call_dynamic_add(sgl->lamp_buflimit_points, color, &la->clipsta, &la->clipend, ob->obmat);
	}
	else if (la->type == LA_HEMI) {
		static float hemisize = 2.0f;
		DRW_shgroup_call_dynamic_add(sgl->lamp_hemi, color, &hemisize, shapemat);
	}
	else if (la->type == LA_AREA) {
		float size[3] = {1.0f, 1.0f, 1.0f}, sizemat[4][4];

		if (ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE)) {
			size[1] = la->area_sizey / la->area_size;
			size_to_mat4(sizemat, size);
			mul_m4_m4m4(shapemat, shapemat, sizemat);
		}

		if (ELEM(la->area_shape, LA_AREA_DISK, LA_AREA_ELLIPSE)) {
			DRW_shgroup_call_dynamic_add(sgl->lamp_area_disk, color, &la->area_size, shapemat);
		}
		else {
			DRW_shgroup_call_dynamic_add(sgl->lamp_area_square, color, &la->area_size, shapemat);
		}
	}

	if (ELEM(la->type, LA_LOCAL, LA_SPOT)) {
		/* We only want position not scale. */
		shapemat[0][0] = shapemat[1][1] = shapemat[2][2] = 1.0f;
		shapemat[0][1] = shapemat[0][2] = 0.0f;
		shapemat[1][0] = shapemat[1][2] = 0.0f;
		shapemat[2][0] = shapemat[2][1] = 0.0f;
		DRW_shgroup_call_dynamic_add(sgl->lamp_area_sphere, color, &la->area_size, shapemat);
	}

	/* Line and point going to the ground */
	DRW_shgroup_call_dynamic_add(sgl->lamp_groundline, ob->obmat[3]);
	DRW_shgroup_call_dynamic_add(sgl->lamp_groundpoint, ob->obmat[3]);
}

static GPUBatch *batch_camera_path_get(
        ListBase *camera_paths, const MovieTrackingReconstruction *reconstruction)
{
	GPUBatch *geom;
	static GPUVertFormat format = { 0 };
	static struct { uint pos; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	}
	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, reconstruction->camnr);

	MovieReconstructedCamera *camera = reconstruction->cameras;
	for (int a = 0; a < reconstruction->camnr; a++, camera++) {
		GPU_vertbuf_attr_set(vbo, attr_id.pos, a, camera->mat[3]);
	}

	geom = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);

	/* Store the batch to do cleanup after drawing. */
	BLI_addtail(camera_paths, BLI_genericNodeN(geom));
	return geom;
}

static void batch_camera_path_free(ListBase *camera_paths)
{
	LinkData *link;
	while ((link = BLI_pophead(camera_paths))) {
		GPUBatch *camera_path = link->data;
		GPU_batch_discard(camera_path);
		MEM_freeN(link);
	}
}

static void DRW_shgroup_camera(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;
	Scene *scene = draw_ctx->scene;
	RegionView3D *rv3d = draw_ctx->rv3d;

	Camera *cam = ob->data;
	const  Object *camera_object = DEG_get_evaluated_object(draw_ctx->depsgraph, v3d->camera);
	const bool is_active = (ob == camera_object);
	const bool look_through = (is_active && (rv3d->persp == RV3D_CAMOB));
	float *color;
	DRW_object_wire_theme_get(ob, view_layer, &color);

	float vec[4][3], asp[2], shift[2], scale[3], drawsize;

	scale[0] = 1.0f / len_v3(ob->obmat[0]);
	scale[1] = 1.0f / len_v3(ob->obmat[1]);
	scale[2] = 1.0f / len_v3(ob->obmat[2]);

	BKE_camera_view_frame_ex(scene, cam, cam->drawsize, false, scale,
	                         asp, shift, &drawsize, vec);

	/* Frame coords */
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

	if (look_through) {
		/* Only draw the frame. */
		DRW_shgroup_call_dynamic_add(
		        sgl->camera_frame, color, cam->drwcorners,
		        &cam->drwdepth, cam->drwtria, ob->obmat);
	}
	else {
		DRW_shgroup_call_dynamic_add(
		        sgl->camera, color, cam->drwcorners,
		        &cam->drwdepth, cam->drwtria, ob->obmat);

		/* Active cam */
		if (is_active) {
			DRW_shgroup_call_dynamic_add(
			        sgl->camera_tria, color,
			        cam->drwcorners, &cam->drwdepth, cam->drwtria, ob->obmat);
		}
	}

	/* draw the rest in normalize object space */
	normalize_m4_m4(cam->drwnormalmat, ob->obmat);

	if (cam->flag & CAM_SHOWLIMITS) {
		static float col[3] = {0.5f, 0.5f, 0.25f}, col_hi[3] = {1.0f, 1.0f, 0.5f};
		float sizemat[4][4], size[3] = {1.0f, 1.0f, 0.0f};
		float focusdist = BKE_camera_object_dof_distance(ob);

		copy_m4_m4(cam->drwfocusmat, cam->drwnormalmat);
		translate_m4(cam->drwfocusmat, 0.0f, 0.0f, -focusdist);
		size_to_mat4(sizemat, size);
		mul_m4_m4m4(cam->drwfocusmat, cam->drwfocusmat, sizemat);

		DRW_shgroup_call_dynamic_add(
		        sgl->camera_focus, (is_active ? col_hi : col),
		        &cam->drawsize, cam->drwfocusmat);

		DRW_shgroup_call_dynamic_add(
		        sgl->camera_clip, color,
		        &cam->clipsta, &cam->clipend, cam->drwnormalmat);
		DRW_shgroup_call_dynamic_add(
		        sgl->camera_clip_points, (is_active ? col_hi : col),
		        &cam->clipsta, &cam->clipend, cam->drwnormalmat);
	}

	if (cam->flag & CAM_SHOWMIST) {
		World *world = scene->world;

		if (world) {
			static float col[3] = {0.5f, 0.5f, 0.5f}, col_hi[3] = {1.0f, 1.0f, 1.0f};
			world->mistend = world->miststa + world->mistdist;
			DRW_shgroup_call_dynamic_add(
			        sgl->camera_mist, color,
			        &world->miststa, &world->mistend, cam->drwnormalmat);
			DRW_shgroup_call_dynamic_add(
			        sgl->camera_mist_points, (is_active ? col_hi : col),
			        &world->miststa, &world->mistend, cam->drwnormalmat);
		}
	}

	/* Motion Tracking. */
	MovieClip *clip = BKE_object_movieclip_get(scene, ob, false);
	if ((v3d->flag2 & V3D_SHOW_RECONSTRUCTION) && (clip != NULL)){
		BLI_assert(BLI_listbase_is_empty(&sgl->camera_path));
		const bool is_select = DRW_state_is_select();
		const bool is_solid_bundle = (v3d->bundle_drawtype == OB_EMPTY_SPHERE) &&
		                             ((v3d->shading.type != OB_SOLID) ||
		                              ((v3d->shading.flag & V3D_SHADING_XRAY) == 0));

		MovieTracking *tracking = &clip->tracking;
		/* Index must start in 1, to mimic BKE_tracking_track_get_indexed. */
		int track_index = 1;

		uchar text_color_selected[4], text_color_unselected[4];
		float bundle_color_unselected[4], bundle_color_solid[4];

		UI_GetThemeColor4ubv(TH_SELECT, text_color_selected);
		UI_GetThemeColor4ubv(TH_TEXT, text_color_unselected);
		UI_GetThemeColor4fv(TH_WIRE, bundle_color_unselected);
		UI_GetThemeColor4fv(TH_BUNDLE_SOLID, bundle_color_solid);

		float camera_mat[4][4];
		BKE_tracking_get_camera_object_matrix(draw_ctx->depsgraph, scene, ob, camera_mat);

		float bundle_scale_mat[4][4];
		if (is_solid_bundle) {
			scale_m4_fl(bundle_scale_mat, v3d->bundle_size);
		}

		for (MovieTrackingObject *tracking_object = tracking->objects.first;
		     tracking_object != NULL;
		     tracking_object = tracking_object->next)
		{
			float tracking_object_mat[4][4];

			if (tracking_object->flag & TRACKING_OBJECT_CAMERA){
				copy_m4_m4(tracking_object_mat, camera_mat);
			}
			else {
				const int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, DEG_get_ctime(draw_ctx->depsgraph));
				float object_mat[4][4];
				BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object, framenr, object_mat);

				invert_m4(object_mat);
				mul_m4_m4m4(tracking_object_mat, cam->drwnormalmat, object_mat);
			}

			ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
			for (MovieTrackingTrack *track = tracksbase->first; track; track = track->next) {

				if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
					continue;
				}

				bool is_selected = TRACK_SELECTED(track);

				float bundle_mat[4][4];
				copy_m4_m4(bundle_mat, tracking_object_mat);
				translate_m4(bundle_mat, track->bundle_pos[0], track->bundle_pos[1], track->bundle_pos[2]);

				float *bundle_color;
				if (track->flag & TRACK_CUSTOMCOLOR) {
					bundle_color = track->color;
				}
				else if (is_solid_bundle) {
					bundle_color = bundle_color_solid;
				}
				else if (is_selected) {
					bundle_color = color;
				}
				else {
					bundle_color = bundle_color_unselected;
				}

				if (is_select) {
					DRW_select_load_id(camera_object->select_color | (track_index << 16));
					track_index++;
				}

				if (is_solid_bundle) {

					if (is_selected) {
						DRW_shgroup_empty_ex(sgl,
						                     bundle_mat,
						                     &v3d->bundle_size,
						                     v3d->bundle_drawtype,
						                     color);
					}

					float bundle_color_v4[4] = {
					    bundle_color[0],
					    bundle_color[1],
					    bundle_color[2],
					    1.0f,
					};

					mul_m4_m4m4(bundle_mat, bundle_mat, bundle_scale_mat);
					DRW_shgroup_call_dynamic_add(sgl->sphere_solid,
					                             bundle_mat,
					                             bundle_color_v4);
				}
				else {
					DRW_shgroup_empty_ex(sgl,
					                     bundle_mat,
					                     &v3d->bundle_size,
					                     v3d->bundle_drawtype,
					                     bundle_color);
				}

				if ((v3d->flag2 & V3D_SHOW_BUNDLENAME) && !is_select) {
					struct DRWTextStore *dt = DRW_text_cache_ensure();

					DRW_text_cache_add(dt,
					                   bundle_mat[3],
					                   track->name,
					                   strlen(track->name),
					                   10,
					                   DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
					                   is_selected ? text_color_selected : text_color_unselected);
				}
			}

			if ((v3d->flag2 & V3D_SHOW_CAMERAPATH) && (tracking_object->flag & TRACKING_OBJECT_CAMERA) && !is_select) {
				MovieTrackingReconstruction *reconstruction;
				reconstruction = BKE_tracking_object_get_reconstruction(tracking, tracking_object);

				if (reconstruction->camnr) {
					static float camera_path_color[4];
					UI_GetThemeColor4fv(TH_CAMERA_PATH, camera_path_color);

					GPUBatch *geom = batch_camera_path_get(&sgl->camera_path, reconstruction);
					GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
					DRWShadingGroup *shading_group = DRW_shgroup_create(shader, sgl->non_meshes);
					DRW_shgroup_uniform_vec4(shading_group, "color", camera_path_color, 1);
					DRW_shgroup_call_add(shading_group, geom, camera_mat);
				}
			}
		}
	}
}

static void DRW_shgroup_empty_ex(
        OBJECT_ShadingGroupList *sgl, float mat[4][4], float *draw_size, char draw_type, float *color)
{
	switch (draw_type) {
		case OB_PLAINAXES:
			DRW_shgroup_call_dynamic_add(sgl->plain_axes, color, draw_size, mat);
			break;
		case OB_SINGLE_ARROW:
			DRW_shgroup_call_dynamic_add(sgl->single_arrow, color, draw_size, mat);
			DRW_shgroup_call_dynamic_add(sgl->single_arrow_line, color, draw_size, mat);
			break;
		case OB_CUBE:
			DRW_shgroup_call_dynamic_add(sgl->cube, color, draw_size, mat);
			break;
		case OB_CIRCLE:
			DRW_shgroup_call_dynamic_add(sgl->circle, color, draw_size, mat);
			break;
		case OB_EMPTY_SPHERE:
			DRW_shgroup_call_dynamic_add(sgl->sphere, color, draw_size, mat);
			break;
		case OB_EMPTY_CONE:
			DRW_shgroup_call_dynamic_add(sgl->cone, color, draw_size, mat);
			break;
		case OB_ARROWS:
			DRW_shgroup_call_dynamic_add(sgl->empty_axes, color, draw_size, mat);
			break;
		case OB_EMPTY_IMAGE:
			BLI_assert(!"Should never happen, use DRW_shgroup_empty instead.");
			break;
	}
}

static void DRW_shgroup_empty(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
	float *color;
	DRW_object_wire_theme_get(ob, view_layer, &color);

	switch (ob->empty_drawtype) {
		case OB_PLAINAXES:
		case OB_SINGLE_ARROW:
		case OB_CUBE:
		case OB_CIRCLE:
		case OB_EMPTY_SPHERE:
		case OB_EMPTY_CONE:
		case OB_ARROWS:
			DRW_shgroup_empty_ex(sgl, ob->obmat, &ob->empty_drawsize, ob->empty_drawtype, color);
			break;
		case OB_EMPTY_IMAGE:
			DRW_shgroup_empty_image(sgl, ob, color);
			break;
	}
}

static void DRW_shgroup_forcefield(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
	int theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
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
			if (cu && (cu->flag & CU_PATH) && ob->runtime.curve_cache->path && ob->runtime.curve_cache->path->data) {
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
			DRW_shgroup_call_dynamic_add(sgl->field_wind, color, &pd->drawvec1, ob->obmat);
			break;
		case PFIELD_FORCE:
			DRW_shgroup_call_dynamic_add(sgl->field_force, color, &pd->drawvec1, ob->obmat);
			break;
		case PFIELD_VORTEX:
			DRW_shgroup_call_dynamic_add(sgl->field_vortex, color, &pd->drawvec1, ob->obmat);
			break;
		case PFIELD_GUIDE:
			if (cu && (cu->flag & CU_PATH) && ob->runtime.curve_cache->path && ob->runtime.curve_cache->path->data) {
				DRW_shgroup_call_dynamic_add(sgl->field_curve_sta, color, &pd->f_strength, ob->obmat);
				DRW_shgroup_call_dynamic_add(sgl->field_curve_end, color, &pd->f_strength, ob->obmat);
			}
			break;
	}

	if (pd->falloff == PFIELD_FALL_SPHERE) {
		/* as last, guide curve alters it */
		if ((pd->flag & PFIELD_USEMAX) != 0) {
			DRW_shgroup_call_dynamic_add(sgl->field_curve_end, color, &pd->maxdist, ob->obmat);
		}

		if ((pd->flag & PFIELD_USEMIN) != 0) {
			DRW_shgroup_call_dynamic_add(sgl->field_curve_end, color, &pd->mindist, ob->obmat);
		}
	}
	else if (pd->falloff == PFIELD_FALL_TUBE) {
		if (pd->flag & (PFIELD_USEMAX | PFIELD_USEMAXR)) {
			DRW_shgroup_call_dynamic_add(sgl->field_tube_limit, color, &pd->drawvec_falloff_max, ob->obmat);
		}

		if (pd->flag & (PFIELD_USEMIN | PFIELD_USEMINR)) {
			DRW_shgroup_call_dynamic_add(sgl->field_tube_limit, color, &pd->drawvec_falloff_min, ob->obmat);
		}
	}
	else if (pd->falloff == PFIELD_FALL_CONE) {
		if (pd->flag & (PFIELD_USEMAX | PFIELD_USEMAXR)) {
			DRW_shgroup_call_dynamic_add(sgl->field_cone_limit, color, &pd->drawvec_falloff_max, ob->obmat);
		}

		if (pd->flag & (PFIELD_USEMIN | PFIELD_USEMINR)) {
			DRW_shgroup_call_dynamic_add(sgl->field_cone_limit, color, &pd->drawvec_falloff_min, ob->obmat);
		}
	}
}

static void DRW_shgroup_volume_extra(
        OBJECT_ShadingGroupList *sgl,
        Object *ob, ViewLayer *view_layer, Scene *scene, ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData *)md;
	SmokeDomainSettings *sds = smd->domain;
	float *color;
	float one = 1.0f;

	if (sds == NULL) {
		return;
	}

	DRW_object_wire_theme_get(ob, view_layer, &color);

	/* Small cube showing voxel size. */
	float voxel_cubemat[4][4] = {{0.0f}};
	voxel_cubemat[0][0] = 1.0f / (float)sds->res[0];
	voxel_cubemat[1][1] = 1.0f / (float)sds->res[1];
	voxel_cubemat[2][2] = 1.0f / (float)sds->res[2];
	voxel_cubemat[3][0] = voxel_cubemat[3][1] = voxel_cubemat[3][2] = -1.0f;
	voxel_cubemat[3][3] = 1.0f;
	translate_m4(voxel_cubemat, 1.0f, 1.0f, 1.0f);
	mul_m4_m4m4(voxel_cubemat, ob->obmat, voxel_cubemat);

	DRW_shgroup_call_dynamic_add(sgl->cube, color, &one, voxel_cubemat);

	/* Don't show smoke before simulation starts, this could be made an option in the future. */
	if (!sds->draw_velocity || !sds->fluid || CFRA < sds->point_cache[0]->startframe) {
		return;
	}

	const bool use_needle = (sds->vector_draw_type == VECTOR_DRAW_NEEDLE);
	int line_count = (use_needle) ? 6 : 1;
	int slice_axis = -1;
	line_count *= sds->res[0] * sds->res[1] * sds->res[2];

	if (sds->slice_method == MOD_SMOKE_SLICE_AXIS_ALIGNED &&
	    sds->axis_slice_method == AXIS_SLICE_SINGLE)
	{
		float invviewmat[4][4];
		DRW_viewport_matrix_get(invviewmat, DRW_MAT_VIEWINV);

		const int axis = (sds->slice_axis == SLICE_AXIS_AUTO)
		                  ? axis_dominant_v3_single(invviewmat[2])
		                  : sds->slice_axis - 1;
		slice_axis = axis;
		line_count /= sds->res[axis];
	}

	GPU_create_smoke_velocity(smd);

	DRWShadingGroup *grp = DRW_shgroup_create(volume_velocity_shader_get(use_needle), sgl->non_meshes);
	DRW_shgroup_uniform_texture(grp, "velocityX", sds->tex_velocity_x);
	DRW_shgroup_uniform_texture(grp, "velocityY", sds->tex_velocity_y);
	DRW_shgroup_uniform_texture(grp, "velocityZ", sds->tex_velocity_z);
	DRW_shgroup_uniform_float_copy(grp, "displaySize", sds->vector_scale);
	DRW_shgroup_uniform_float_copy(grp, "slicePosition", sds->slice_depth);
	DRW_shgroup_uniform_int_copy(grp, "sliceAxis", slice_axis);
	DRW_shgroup_call_procedural_lines_add(grp, line_count, ob->obmat);

	BLI_addtail(&e_data.smoke_domains, BLI_genericNodeN(smd));
}

static void volumes_free_smoke_textures(void)
{
	/* Free Smoke Textures after rendering */
	/* XXX This is a waste of processing and GPU bandwidth if nothing
	 * is updated. But the problem is since Textures are stored in the
	 * modifier we don't want them to take precious VRAM if the
	 * modifier is not used for display. We should share them for
	 * all viewport in a redraw at least. */
	for (LinkData *link = e_data.smoke_domains.first; link; link = link->next) {
		SmokeModifierData *smd = (SmokeModifierData *)link->data;
		GPU_free_smoke(smd);
	}
	BLI_freelistN(&e_data.smoke_domains);
}

static void DRW_shgroup_gpencil(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
	float *color;
	DRW_object_wire_theme_get(ob, view_layer, &color);

	DRW_shgroup_call_dynamic_add(sgl->gpencil_axes, color, &ob->empty_drawsize, ob->obmat);
}

static void DRW_shgroup_speaker(OBJECT_ShadingGroupList *sgl, Object *ob, ViewLayer *view_layer)
{
	float *color;
	static float one = 1.0f;
	DRW_object_wire_theme_get(ob, view_layer, &color);

	DRW_shgroup_call_dynamic_add(sgl->speaker, color, &one, ob->obmat);
}

typedef struct OBJECT_LightProbeEngineData {
	DrawData dd;

	float prb_mats[6][4][4];
	float probe_cube_mat[4][4];
	float draw_size;
	float increment_x[3];
	float increment_y[3];
	float increment_z[3];
	float corner[3];
	uint cell_count;
} OBJECT_LightProbeEngineData;

static void DRW_shgroup_lightprobe(OBJECT_StorageList *stl, OBJECT_PassList *psl, Object *ob, ViewLayer *view_layer)
{
	float *color;
	static float one = 1.0f;
	LightProbe *prb = (LightProbe *)ob->data;
	bool do_outlines = ((ob->base_flag & BASE_SELECTED) != 0);
	int theme_id = DRW_object_wire_theme_get(ob, view_layer, &color);

	OBJECT_ShadingGroupList *sgl = (ob->dtx & OB_DRAWXRAY) ? &stl->g_data->sgl_ghost : &stl->g_data->sgl;

	OBJECT_LightProbeEngineData *prb_data =
	        (OBJECT_LightProbeEngineData *)DRW_drawdata_ensure(
	                &ob->id,
	                &draw_engine_object_type,
	                sizeof(OBJECT_LightProbeEngineData),
	                NULL,
	                NULL);

	if ((DRW_state_is_select() || do_outlines) && ((prb->flag & LIGHTPROBE_FLAG_SHOW_DATA) != 0)) {
		int *call_id = shgroup_theme_id_to_probe_outline_counter(stl, theme_id);

		if (prb->type == LIGHTPROBE_TYPE_GRID) {
			/* Update transforms */
			float cell_dim[3], half_cell_dim[3];
			cell_dim[0] = 2.0f / (float)(prb->grid_resolution_x);
			cell_dim[1] = 2.0f / (float)(prb->grid_resolution_y);
			cell_dim[2] = 2.0f / (float)(prb->grid_resolution_z);

			mul_v3_v3fl(half_cell_dim, cell_dim, 0.5f);

			/* First cell. */
			copy_v3_fl(prb_data->corner, -1.0f);
			add_v3_v3(prb_data->corner, half_cell_dim);
			mul_m4_v3(ob->obmat, prb_data->corner);

			/* Opposite neighbor cell. */
			copy_v3_fl3(prb_data->increment_x, cell_dim[0], 0.0f, 0.0f);
			add_v3_v3(prb_data->increment_x, half_cell_dim);
			add_v3_fl(prb_data->increment_x, -1.0f);
			mul_m4_v3(ob->obmat, prb_data->increment_x);
			sub_v3_v3(prb_data->increment_x, prb_data->corner);

			copy_v3_fl3(prb_data->increment_y, 0.0f, cell_dim[1], 0.0f);
			add_v3_v3(prb_data->increment_y, half_cell_dim);
			add_v3_fl(prb_data->increment_y, -1.0f);
			mul_m4_v3(ob->obmat, prb_data->increment_y);
			sub_v3_v3(prb_data->increment_y, prb_data->corner);

			copy_v3_fl3(prb_data->increment_z, 0.0f, 0.0f, cell_dim[2]);
			add_v3_v3(prb_data->increment_z, half_cell_dim);
			add_v3_fl(prb_data->increment_z, -1.0f);
			mul_m4_v3(ob->obmat, prb_data->increment_z);
			sub_v3_v3(prb_data->increment_z, prb_data->corner);

			prb_data->cell_count = prb->grid_resolution_x * prb->grid_resolution_y * prb->grid_resolution_z;
			DRWShadingGroup *grp = DRW_shgroup_create(e_data.lightprobe_grid_sh, psl->lightprobes);
			DRW_shgroup_uniform_int_copy(grp, "call_id", *call_id);
			DRW_shgroup_uniform_int(grp, "baseId", call_id, 1); /* that's correct */
			DRW_shgroup_uniform_vec3(grp, "corner", prb_data->corner, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_x", prb_data->increment_x, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_y", prb_data->increment_y, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_z", prb_data->increment_z, 1);
			DRW_shgroup_uniform_ivec3(grp, "grid_resolution", &prb->grid_resolution_x, 1);
			DRW_shgroup_call_procedural_points_add(grp, prb_data->cell_count, NULL);
		}
		else if (prb->type == LIGHTPROBE_TYPE_CUBE) {
			// prb_data->draw_size = prb->data_draw_size * 0.1f;
			// unit_m4(prb_data->probe_cube_mat);
			// copy_v3_v3(prb_data->probe_cube_mat[3], ob->obmat[3]);

			DRWShadingGroup *grp = shgroup_theme_id_to_probe_cube_outline_shgrp(stl, theme_id);
			/* TODO remove or change the drawing of the cube probes. Theses line draws nothing on purpose
			 * to keep the call ids correct. */
			zero_m4(prb_data->probe_cube_mat);
			DRW_shgroup_call_dynamic_add(grp, call_id, &prb_data->draw_size, prb_data->probe_cube_mat);
		}
		else {
			prb_data->draw_size = 1.0f;

			DRWShadingGroup *grp = shgroup_theme_id_to_probe_planar_outline_shgrp(stl, theme_id);
			DRW_shgroup_call_dynamic_add(grp, call_id, &prb_data->draw_size, ob->obmat);
		}

		*call_id += 1;
	}

	switch (prb->type) {
		case LIGHTPROBE_TYPE_PLANAR:
			DRW_shgroup_call_dynamic_add(sgl->probe_planar, ob->obmat[3], color);
			break;
		case LIGHTPROBE_TYPE_GRID:
			DRW_shgroup_call_dynamic_add(sgl->probe_grid, ob->obmat[3], color);
			break;
		case LIGHTPROBE_TYPE_CUBE:
		default:
			DRW_shgroup_call_dynamic_add(sgl->probe_cube, ob->obmat[3], color);
			break;
	}



	if (prb->type == LIGHTPROBE_TYPE_PLANAR) {
		float (*mat)[4];
		mat = (float (*)[4])(prb_data->prb_mats[0]);
		copy_m4_m4(mat, ob->obmat);
		normalize_m4(mat);

		DRW_shgroup_call_dynamic_add(sgl->single_arrow, color, &ob->empty_drawsize, mat);
		DRW_shgroup_call_dynamic_add(sgl->single_arrow_line, color, &ob->empty_drawsize, mat);

		mat = (float (*)[4])(prb_data->prb_mats[1]);
		copy_m4_m4(mat, ob->obmat);
		zero_v3(mat[2]);

		DRW_shgroup_call_dynamic_add(sgl->cube, color, &one, mat);
	}

	if ((prb->flag & LIGHTPROBE_FLAG_SHOW_INFLUENCE) != 0) {

		prb->distfalloff = (1.0f - prb->falloff) * prb->distinf;
		prb->distgridinf = prb->distinf;

		if (prb->type == LIGHTPROBE_TYPE_GRID) {
			prb->distfalloff += 1.0f;
			prb->distgridinf += 1.0f;
		}

		if (prb->type == LIGHTPROBE_TYPE_GRID ||
		    prb->attenuation_type == LIGHTPROBE_SHAPE_BOX)
		{
			DRW_shgroup_call_dynamic_add(sgl->cube, color, &prb->distgridinf, ob->obmat);
			DRW_shgroup_call_dynamic_add(sgl->cube, color, &prb->distfalloff, ob->obmat);
		}
		else if (prb->type == LIGHTPROBE_TYPE_PLANAR) {
			float (*rangemat)[4];
			rangemat = (float (*)[4])(prb_data->prb_mats[2]);
			copy_m4_m4(rangemat, ob->obmat);
			normalize_v3(rangemat[2]);
			mul_v3_fl(rangemat[2], prb->distinf);

			DRW_shgroup_call_dynamic_add(sgl->cube, color, &one, rangemat);

			rangemat = (float (*)[4])(prb_data->prb_mats[3]);
			copy_m4_m4(rangemat, ob->obmat);
			normalize_v3(rangemat[2]);
			mul_v3_fl(rangemat[2], prb->distfalloff);

			DRW_shgroup_call_dynamic_add(sgl->cube, color, &one, rangemat);
		}
		else {
			DRW_shgroup_call_dynamic_add(sgl->sphere, color, &prb->distgridinf, ob->obmat);
			DRW_shgroup_call_dynamic_add(sgl->sphere, color, &prb->distfalloff, ob->obmat);
		}
	}

	if ((prb->flag & LIGHTPROBE_FLAG_SHOW_PARALLAX) != 0) {
		if (prb->type != LIGHTPROBE_TYPE_PLANAR) {
			float (*obmat)[4], *dist;

			if ((prb->flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0) {
				dist = &prb->distpar;
				/* TODO object parallax */
				obmat = ob->obmat;
			}
			else {
				dist = &prb->distinf;
				obmat = ob->obmat;
			}

			if (prb->parallax_type == LIGHTPROBE_SHAPE_BOX) {
				DRW_shgroup_call_dynamic_add(sgl->cube, color, dist, obmat);
			}
			else {
				DRW_shgroup_call_dynamic_add(sgl->sphere, color, dist, obmat);
			}
		}
	}

	if ((prb->flag & LIGHTPROBE_FLAG_SHOW_CLIP_DIST) != 0) {
		if (prb->type != LIGHTPROBE_TYPE_PLANAR) {
			static const float cubefacemat[6][4][4] = {
				{{0.0, 0.0, -1.0, 0.0}, {0.0, -1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}},
				{{0.0, 0.0, 1.0, 0.0}, {0.0, -1.0, 0.0, 0.0}, {1.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}},
				{{1.0, 0.0, 0.0, 0.0}, {0.0, 0.0, -1.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}},
				{{1.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {0.0, -1.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}},
				{{1.0, 0.0, 0.0, 0.0}, {0.0, -1.0, 0.0, 0.0}, {0.0, 0.0, -1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}},
				{{-1.0, 0.0, 0.0, 0.0}, {0.0, -1.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}},
			};

			for (int i = 0; i < 6; ++i) {
				float (*clipmat)[4];
				clipmat = (float (*)[4])(prb_data->prb_mats[i]);

				normalize_m4_m4(clipmat, ob->obmat);
				mul_m4_m4m4(clipmat, clipmat, cubefacemat[i]);

				DRW_shgroup_call_dynamic_add(sgl->lamp_buflimit, color, &prb->clipsta, &prb->clipend, clipmat);
				DRW_shgroup_call_dynamic_add(sgl->lamp_buflimit_points, color, &prb->clipsta, &prb->clipend, clipmat);
			}
		}
	}

	/* Line and point going to the ground */
	if (prb->type == LIGHTPROBE_TYPE_CUBE) {
		DRW_shgroup_call_dynamic_add(sgl->lamp_groundline, ob->obmat[3]);
		DRW_shgroup_call_dynamic_add(sgl->lamp_groundpoint, ob->obmat[3]);
	}
}

static void DRW_shgroup_relationship_lines(OBJECT_ShadingGroupList *sgl, Object *ob)
{
	if (ob->parent && DRW_check_object_visible_within_active_context(ob->parent)) {
		DRW_shgroup_call_dynamic_add(sgl->relationship_lines, ob->parent->obmat[3]);
		DRW_shgroup_call_dynamic_add(sgl->relationship_lines, ob->obmat[3]);
	}

	if (ob->rigidbody_constraint) {
		Object *rbc_ob1 = ob->rigidbody_constraint->ob1;
		Object *rbc_ob2 = ob->rigidbody_constraint->ob2;
		if (rbc_ob1 && DRW_check_object_visible_within_active_context(rbc_ob1)) {
			DRW_shgroup_call_dynamic_add(sgl->relationship_lines, rbc_ob1->obmat[3]);
			DRW_shgroup_call_dynamic_add(sgl->relationship_lines, ob->obmat[3]);
		}
		if (rbc_ob2 && DRW_check_object_visible_within_active_context(rbc_ob2)) {
			DRW_shgroup_call_dynamic_add(sgl->relationship_lines, rbc_ob2->obmat[3]);
			DRW_shgroup_call_dynamic_add(sgl->relationship_lines, ob->obmat[3]);
		}
	}
}

static void DRW_shgroup_object_center(OBJECT_StorageList *stl, Object *ob, ViewLayer *view_layer, View3D *v3d)
{
	if (v3d->overlay.flag & V3D_OVERLAY_HIDE_OBJECT_ORIGINS) {
		return;
	}

	const bool is_library = ob->id.us > 1 || ID_IS_LINKED(ob);
	DRWShadingGroup *shgroup;

	if (ob == OBACT(view_layer)) {
		shgroup = stl->g_data->center_active;
	}
	else if (ob->base_flag & BASE_SELECTED) {
		if (is_library) {
			shgroup = stl->g_data->center_selected_lib;
		}
		else {
			shgroup = stl->g_data->center_selected;
		}
	}
	else if (v3d->flag & V3D_DRAW_CENTERS) {
		if (is_library) {
			shgroup = stl->g_data->center_deselected_lib;
		}
		else {
			shgroup = stl->g_data->center_deselected;
		}
	}
	else {
		return;
	}

	DRW_shgroup_call_dynamic_add(shgroup, ob->obmat[3]);
}

static void DRW_shgroup_texture_space(OBJECT_ShadingGroupList *sgl, Object *ob, int theme_id)
{
	if (ob->data == NULL) {
		return;
	}

	ID *ob_data = ob->data;
	float *texcoloc = NULL;
	float *texcosize = NULL;
	if (ob->data != NULL) {
		switch (GS(ob_data->name)) {
			case ID_ME:
				BKE_mesh_texspace_get_reference((Mesh *)ob_data, NULL, &texcoloc, NULL, &texcosize);
				break;
			case ID_CU:
			{
				Curve *cu = (Curve *)ob_data;
				if (cu->bb == NULL || (cu->bb->flag & BOUNDBOX_DIRTY)) {
					BKE_curve_texspace_calc(cu);
				}
				texcoloc = cu->loc;
				texcosize = cu->size;
				break;
			}
			case ID_MB:
			{
				MetaBall *mb = (MetaBall *)ob_data;
				texcoloc = mb->loc;
				texcosize = mb->size;
				break;
			}
			default:
				BLI_assert(0);
		}
	}

	float tmp[4][4] = {{0.0f}}, one = 1.0f;
	tmp[0][0] = texcosize[0];
	tmp[1][1] = texcosize[1];
	tmp[2][2] = texcosize[2];
	tmp[3][0] = texcoloc[0];
	tmp[3][1] = texcoloc[1];
	tmp[3][2] = texcoloc[2];
	tmp[3][3] = 1.0f;

	mul_m4_m4m4(tmp, ob->obmat, tmp);

	float color[4];
	UI_GetThemeColor4fv(theme_id, color);

	DRW_shgroup_call_dynamic_add(sgl->texspace, color, &one, tmp);
}

static void DRW_shgroup_bounds(OBJECT_ShadingGroupList *sgl, Object *ob, int theme_id)
{
	float color[4], center[3], size[3], tmp[4][4], final_mat[4][4], one = 1.0f;
	BoundBox  bb_local;

	if (ob->type == OB_MBALL && !BKE_mball_is_basis(ob)) {
		return;
	}

	BoundBox *bb = BKE_object_boundbox_get(ob);

	if (!ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT,
	                    OB_MBALL, OB_ARMATURE, OB_LATTICE))
	{
		const float min[3] = {-1.0f, -1.0f, -1.0f}, max[3] = {1.0f, 1.0f, 1.0f};
		bb = &bb_local;
		BKE_boundbox_init_from_minmax(bb, min, max);
	}

	UI_GetThemeColor4fv(theme_id, color);
	BKE_boundbox_calc_center_aabb(bb, center);
	BKE_boundbox_calc_size_aabb(bb, size);

	switch (ob->boundtype) {
		case OB_BOUND_BOX:
			size_to_mat4(tmp, size);
			copy_v3_v3(tmp[3], center);
			mul_m4_m4m4(tmp, ob->obmat, tmp);
			DRW_shgroup_call_dynamic_add(sgl->cube, color, &one, tmp);
			break;
		case OB_BOUND_SPHERE:
			size[0] = max_fff(size[0], size[1], size[2]);
			size[1] = size[2] = size[0];
			size_to_mat4(tmp, size);
			copy_v3_v3(tmp[3], center);
			mul_m4_m4m4(tmp, ob->obmat, tmp);
			DRW_shgroup_call_dynamic_add(sgl->sphere, color, &one, tmp);
			break;
		case OB_BOUND_CYLINDER:
			size[0] = max_ff(size[0], size[1]);
			size[1] = size[0];
			size_to_mat4(tmp, size);
			copy_v3_v3(tmp[3], center);
			mul_m4_m4m4(tmp, ob->obmat, tmp);
			DRW_shgroup_call_dynamic_add(sgl->cylinder, color, &one, tmp);
			break;
		case OB_BOUND_CONE:
			size[0] = max_ff(size[0], size[1]);
			size[1] = size[0];
			size_to_mat4(tmp, size);
			copy_v3_v3(tmp[3], center);
			/* Cone batch has base at 0 and is pointing towards +Y. */
			swap_v3_v3(tmp[1], tmp[2]);
			tmp[3][2] -= size[2];
			mul_m4_m4m4(tmp, ob->obmat, tmp);
			DRW_shgroup_call_dynamic_add(sgl->cone, color, &one, tmp);
			break;
		case OB_BOUND_CAPSULE:
			size[0] = max_ff(size[0], size[1]);
			size[1] = size[0];
			scale_m4_fl(tmp, size[0]);
			copy_v2_v2(tmp[3], center);
			tmp[3][2] = center[2] + max_ff(0.0f, size[2] - size[0]);
			mul_m4_m4m4(final_mat, ob->obmat, tmp);
			DRW_shgroup_call_dynamic_add(sgl->capsule_cap, color, &one, final_mat);
			negate_v3(tmp[2]);
			tmp[3][2] = center[2] - max_ff(0.0f, size[2] - size[0]);
			mul_m4_m4m4(final_mat, ob->obmat, tmp);
			DRW_shgroup_call_dynamic_add(sgl->capsule_cap, color, &one, final_mat);
			tmp[2][2] = max_ff(0.0f, size[2] * 2.0f - size[0] * 2.0f);
			mul_m4_m4m4(final_mat, ob->obmat, tmp);
			DRW_shgroup_call_dynamic_add(sgl->capsule_body, color, &one, final_mat);
			break;
	}
}

static void OBJECT_cache_populate_particles(Object *ob,
                                            OBJECT_PassList *psl)
{
	for (ParticleSystem *psys = ob->particlesystem.first; psys; psys = psys->next) {
		if (!psys_check_enabled(ob, psys, false)) {
			continue;
		}
		if (!DRW_check_psys_visible_within_active_context(ob, psys)) {
			continue;
		}

		ParticleSettings *part = psys->part;
		int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

		static float mat[4][4];
		unit_m4(mat);

		if (draw_as != PART_DRAW_PATH) {
			struct GPUBatch *geom = DRW_cache_particles_get_dots(ob, psys);
			DRWShadingGroup *shgrp = NULL;
			static int screen_space[2] = {0, 1};
			static float def_prim_col[3] = {0.5f, 0.5f, 0.5f};
			static float def_sec_col[3] = {1.0f, 1.0f, 1.0f};

			/* Dummy particle format for instancing to work. */
			DRW_shgroup_instance_format(e_data.particle_format, {{"dummy", DRW_ATTRIB_FLOAT, 1}});

			Material *ma = give_current_material(ob, part->omat);

			switch (draw_as) {
				case PART_DRAW_DOT:
					shgrp = DRW_shgroup_create(e_data.part_dot_sh, psl->particle);
					DRW_shgroup_uniform_vec3(shgrp, "color", ma ? &ma->r : def_prim_col, 1);
					DRW_shgroup_uniform_vec3(shgrp, "outlineColor", ma ? &ma->specr : def_sec_col, 1);
					DRW_shgroup_uniform_float(shgrp, "pixel_size", DRW_viewport_pixelsize_get(), 1);
					DRW_shgroup_uniform_float(shgrp, "size", &part->draw_size, 1);
					DRW_shgroup_uniform_texture(shgrp, "ramp", globals_ramp);
					DRW_shgroup_call_add(shgrp, geom, mat);
					break;
				case PART_DRAW_CROSS:
					shgrp = DRW_shgroup_instance_create(
					        e_data.part_prim_sh, psl->particle, DRW_cache_particles_get_prim(PART_DRAW_CROSS),
					        e_data.particle_format);
					DRW_shgroup_uniform_texture(shgrp, "ramp", globals_ramp);
					DRW_shgroup_uniform_vec3(shgrp, "color", ma ? &ma->r : def_prim_col, 1);
					DRW_shgroup_uniform_int(shgrp, "screen_space", &screen_space[0], 1);
					break;
				case PART_DRAW_CIRC:
					shgrp = DRW_shgroup_instance_create(
					        e_data.part_prim_sh, psl->particle, DRW_cache_particles_get_prim(PART_DRAW_CIRC),
					        e_data.particle_format);
					DRW_shgroup_uniform_texture(shgrp, "ramp", globals_ramp);
					DRW_shgroup_uniform_vec3(shgrp, "color", ma ? &ma->r : def_prim_col, 1);
					DRW_shgroup_uniform_int(shgrp, "screen_space", &screen_space[1], 1);
					break;
				case PART_DRAW_AXIS:
					shgrp = DRW_shgroup_instance_create(
					        e_data.part_axis_sh, psl->particle, DRW_cache_particles_get_prim(PART_DRAW_AXIS),
					        e_data.particle_format);
					DRW_shgroup_uniform_int(shgrp, "screen_space", &screen_space[0], 1);
					break;
				default:
					break;
			}

			if (shgrp) {
				if (draw_as != PART_DRAW_DOT) {
					DRW_shgroup_uniform_float(shgrp, "draw_size", &part->draw_size, 1);
					DRW_shgroup_instance_batch(shgrp, geom);
				}
			}
		}
	}
}

static void OBJECT_cache_populate(void *vedata, Object *ob)
{
	OBJECT_PassList *psl = ((OBJECT_Data *)vedata)->psl;
	OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;
	OBJECT_ShadingGroupList *sgl = (ob->dtx & OB_DRAWXRAY) ? &stl->g_data->sgl_ghost : &stl->g_data->sgl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	Scene *scene = draw_ctx->scene;
	View3D *v3d = draw_ctx->v3d;
	ModifierData *md = NULL;
	int theme_id = TH_UNDEFINED;

	/* Handle particles first in case the emitter itself shouldn't be rendered. */
	if (ob->type == OB_MESH) {
		OBJECT_cache_populate_particles(ob, psl);
	}

	if (DRW_check_object_visible_within_active_context(ob) == false) {
		return;
	}

	const bool do_outlines = (draw_ctx->v3d->flag & V3D_SELECT_OUTLINE) && ((ob->base_flag & BASE_SELECTED) != 0);
	const bool show_relations = ((draw_ctx->v3d->flag & V3D_HIDE_HELPLINES) == 0);
	const bool hide_object_extra = (v3d->overlay.flag & V3D_OVERLAY_HIDE_OBJECT_XTRAS) != 0;

	if (do_outlines) {
		if (!BKE_object_is_in_editmode(ob) && !((ob == draw_ctx->obact) && (draw_ctx->object_mode & OB_MODE_ALL_PAINT))) {
			struct GPUBatch *geom;
			const bool xray_enabled = ((v3d->shading.flag & V3D_SHADING_XRAY) != 0) &&
			                           (v3d->shading.type < OB_MATERIAL);
			if (xray_enabled) {
				geom = DRW_cache_object_edge_detection_get(ob, NULL);
			}
			else {
				geom = DRW_cache_object_surface_get(ob);
			}
			if (geom) {
				theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
				DRWShadingGroup *shgroup = shgroup_theme_id_to_outline_or(stl, theme_id, NULL);
				if (shgroup != NULL) {
					DRW_shgroup_call_object_add(shgroup, geom, ob);
				}
			}
		}
	}

	switch (ob->type) {
		case OB_MESH:
		{
			if (hide_object_extra) {
				break;
			}
			if (ob != draw_ctx->object_edit) {
				Mesh *me = ob->data;
				if (me->totedge == 0) {
					struct GPUBatch *geom = DRW_cache_mesh_verts_get(ob);
					if (geom) {
						if (theme_id == TH_UNDEFINED) {
							theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
						}

						DRWShadingGroup *shgroup = shgroup_theme_id_to_point_or(sgl, theme_id, sgl->points);
						DRW_shgroup_call_object_add(shgroup, geom, ob);
					}
				}
				else {
					struct GPUBatch *geom = DRW_cache_mesh_loose_edges_get(ob);
					if (geom) {
						if (theme_id == TH_UNDEFINED) {
							theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
						}

						DRWShadingGroup *shgroup = shgroup_theme_id_to_wire_or(sgl, theme_id, sgl->wire);
						DRW_shgroup_call_object_add(shgroup, geom, ob);
					}
				}
			}
			break;
		}
		case OB_SURF:
			break;
		case OB_LATTICE:
		{
			if (ob != draw_ctx->object_edit) {
				if (hide_object_extra) {
					break;
				}
				struct GPUBatch *geom = DRW_cache_lattice_wire_get(ob, false);
				if (theme_id == TH_UNDEFINED) {
					theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
				}

				DRWShadingGroup *shgroup = shgroup_theme_id_to_wire_or(sgl, theme_id, sgl->wire);
				DRW_shgroup_call_object_add(shgroup, geom, ob);
			}
			break;
		}
		case OB_CURVE:
		{
			if (ob != draw_ctx->object_edit) {
				if (hide_object_extra) {
					break;
				}
				struct GPUBatch *geom = DRW_cache_curve_edge_wire_get(ob);
				if (theme_id == TH_UNDEFINED) {
					theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
				}
				DRWShadingGroup *shgroup = shgroup_theme_id_to_wire_or(sgl, theme_id, sgl->wire);
				DRW_shgroup_call_object_add(shgroup, geom, ob);
			}
			break;
		}
		case OB_MBALL:
		{
			if (ob != draw_ctx->object_edit) {
				DRW_shgroup_mball_handles(sgl, ob, view_layer);
			}
			break;
		}
		case OB_LAMP:
			if (hide_object_extra) {
				break;
			}
			DRW_shgroup_lamp(sgl, ob, view_layer);
			break;
		case OB_CAMERA:
			if (hide_object_extra) {
				break;
			}
			 DRW_shgroup_camera(sgl, ob, view_layer);
			break;
		case OB_EMPTY:
			if (hide_object_extra) {
				break;
			}
			DRW_shgroup_empty(sgl, ob, view_layer);
			break;
		case OB_GPENCIL:
			DRW_shgroup_gpencil(sgl, ob, view_layer);
			break;
		case OB_SPEAKER:
			if (hide_object_extra) {
				break;
			}
			DRW_shgroup_speaker(sgl, ob, view_layer);
			break;
		case OB_LIGHTPROBE:
			if (hide_object_extra) {
				break;
			}
			DRW_shgroup_lightprobe(stl, psl, ob, view_layer);
			break;
		case OB_ARMATURE:
		{
			if (v3d->overlay.flag & V3D_OVERLAY_HIDE_BONES) {
				break;
			}
			bArmature *arm = ob->data;
			if (arm->edbo == NULL) {
				if (DRW_state_is_select() || !DRW_pose_mode_armature(ob, draw_ctx->obact)) {
					DRWArmaturePasses passes = {
					    .bone_solid = sgl->bone_solid,
					    .bone_outline = sgl->bone_outline,
					    .bone_wire = sgl->bone_wire,
					    .bone_envelope = sgl->bone_envelope,
					    .bone_axes = sgl->bone_axes,
					    .relationship_lines = NULL, /* Don't draw relationship lines */
					};
					DRW_shgroup_armature_object(ob, view_layer, passes);
				}
			}
			break;
		}
		default:
			break;
	}

	if (ob->pd && ob->pd->forcefield) {
		DRW_shgroup_forcefield(sgl, ob, view_layer);
	}

	/* don't show object extras in set's */
	if ((ob->base_flag & (BASE_FROM_SET | BASE_FROMDUPLI)) == 0) {
		if ((draw_ctx->object_mode & OB_MODE_ALL_PAINT) == 0) {
			DRW_shgroup_object_center(stl, ob, view_layer, v3d);
		}

		if (show_relations) {
			DRW_shgroup_relationship_lines(sgl, ob);
		}

		if ((ob->dtx != 0) && theme_id == TH_UNDEFINED) {
			theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
		}

		if ((ob->dtx & OB_DRAWNAME) && DRW_state_show_text()) {
			struct DRWTextStore *dt = DRW_text_cache_ensure();

			uchar color[4];
			UI_GetThemeColor4ubv(theme_id, color);

			DRW_text_cache_add(
			        dt, ob->obmat[3],
			        ob->id.name + 2, strlen(ob->id.name + 2),
			        10, DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR, color);
		}

		if ((ob->dtx & OB_TEXSPACE) && ELEM(ob->type, OB_MESH, OB_CURVE, OB_MBALL)) {
			DRW_shgroup_texture_space(sgl, ob, theme_id);
		}

		if (ob->dtx & OB_DRAWBOUNDOX) {
			DRW_shgroup_bounds(sgl, ob, theme_id);
		}

		if (ob->dtx & OB_AXIS) {
			float *color, axes_size = 1.0f;
			DRW_object_wire_theme_get(ob, view_layer, &color);

			DRW_shgroup_call_dynamic_add(sgl->empty_axes, color, &axes_size, ob->obmat);
		}

		if ((md = modifiers_findByType(ob, eModifierType_Smoke)) &&
		    (modifier_isEnabled(scene, md, eModifierMode_Realtime)) &&
		    (((SmokeModifierData *)md)->domain != NULL))
		{
			DRW_shgroup_volume_extra(sgl, ob, view_layer, scene, md);
		}
	}
}

static void OBJECT_draw_scene(void *vedata)
{

	OBJECT_PassList *psl = ((OBJECT_Data *)vedata)->psl;
	OBJECT_StorageList *stl = ((OBJECT_Data *)vedata)->stl;
	OBJECT_FramebufferList *fbl = ((OBJECT_Data *)vedata)->fbl;
	OBJECT_PrivateData *g_data = stl->g_data;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	int id_ct_select =       g_data->id_ofs_select;
	int id_ct_active =       g_data->id_ofs_active;
	int id_ct_transform =    g_data->id_ofs_transform;

	int id_ct_prb_select =       g_data->id_ofs_prb_select;
	int id_ct_prb_active =       g_data->id_ofs_prb_active;
	int id_ct_prb_transform =    g_data->id_ofs_prb_transform;

	int outline_calls = id_ct_select + id_ct_active + id_ct_transform;
	outline_calls += id_ct_prb_select + id_ct_prb_active + id_ct_prb_transform;

	float clearcol[4] = {0.0f, 0.0f, 0.0f, 0.0f};

//	DRW_draw_pass(psl->bone_envelope);  /* Never drawn in Object mode currently. */

	MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl)

	/* This needs to be drawn after the oultine */
	DRW_draw_pass(stl->g_data->sgl.bone_solid);
	DRW_draw_pass(stl->g_data->sgl.bone_wire);
	DRW_draw_pass(stl->g_data->sgl.bone_outline);
	DRW_draw_pass(stl->g_data->sgl.non_meshes);
	DRW_draw_pass(psl->particle);
	DRW_draw_pass(stl->g_data->sgl.bone_axes);

	MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl)

	if (DRW_state_is_fbo() && outline_calls > 0) {
		DRW_stats_group_start("Outlines");

		g_data->id_ofs_active = 1;
		g_data->id_ofs_select =    g_data->id_ofs_active + id_ct_active + id_ct_prb_active + 1;
		g_data->id_ofs_transform = g_data->id_ofs_select + id_ct_select + id_ct_prb_select + 1;

		g_data->id_ofs_prb_active =       g_data->id_ofs_active       + id_ct_active;
		g_data->id_ofs_prb_select =       g_data->id_ofs_select       + id_ct_select;
		g_data->id_ofs_prb_transform =    g_data->id_ofs_transform    + id_ct_transform;

		/* Render filled polygon on a separate framebuffer */
		GPU_framebuffer_bind(fbl->outlines_fb);
		GPU_framebuffer_clear_color_depth(fbl->outlines_fb, clearcol, 1.0f);
		DRW_draw_pass(psl->outlines);
		DRW_draw_pass(psl->lightprobes);

		/* Search outline pixels */
		GPU_framebuffer_bind(fbl->blur_fb);
		DRW_draw_pass(psl->outlines_search);

		/* Expand outline to form a 3px wide line */
		GPU_framebuffer_bind(fbl->expand_fb);
		DRW_draw_pass(psl->outlines_expand);

		/* Bleed color so the AA can do it's stuff */
		GPU_framebuffer_bind(fbl->blur_fb);
		DRW_draw_pass(psl->outlines_bleed);

		/* restore main framebuffer */
		GPU_framebuffer_bind(dfbl->default_fb);
		DRW_stats_group_end();
	}
	else if (DRW_state_is_select()) {
		/* Render probes spheres/planes so we can select them. */
		DRW_draw_pass(psl->lightprobes);
	}

	DRW_draw_pass(psl->ob_center);

	if (DRW_state_is_fbo()) {
		if (e_data.draw_grid) {
			GPU_framebuffer_bind(dfbl->color_only_fb);
			DRW_draw_pass(psl->grid);
		}

		/* Combine with scene buffer last */
		if (outline_calls > 0) {
			DRW_draw_pass(psl->outlines_resolve);
		}
	}

	/* This has to be freed only after drawing empties! */
	if (stl->g_data->sgl.image_plane_map) {
		BLI_ghash_free(stl->g_data->sgl.image_plane_map, NULL, MEM_freeN);
		stl->g_data->sgl.image_plane_map = NULL;
	}

	volumes_free_smoke_textures();
	batch_camera_path_free(&stl->g_data->sgl.camera_path);

	if (!DRW_pass_is_empty(stl->g_data->sgl_ghost.bone_solid) ||
		!DRW_pass_is_empty(stl->g_data->sgl_ghost.bone_wire) ||
		!DRW_pass_is_empty(stl->g_data->sgl_ghost.bone_outline) ||
		!DRW_pass_is_empty(stl->g_data->sgl_ghost.non_meshes) ||
		!DRW_pass_is_empty(stl->g_data->sgl_ghost.bone_axes))
	{
		if (DRW_state_is_fbo()) {
			/* meh, late init to not request a depth buffer we won't use. */
			const float *viewport_size = DRW_viewport_size_get();
			const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

			GPUTexture *ghost_depth_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_DEPTH_COMPONENT24, &draw_engine_object_type);
			GPU_framebuffer_ensure_config(&fbl->ghost_fb, {
				GPU_ATTACHMENT_TEXTURE(ghost_depth_tx),
				GPU_ATTACHMENT_TEXTURE(dtxl->color),
			});

			GPU_framebuffer_bind(fbl->ghost_fb);
			GPU_framebuffer_clear_depth(fbl->ghost_fb, 1.0f);
		}

		DRW_draw_pass(stl->g_data->sgl_ghost.bone_solid);
		DRW_draw_pass(stl->g_data->sgl_ghost.bone_wire);
		DRW_draw_pass(stl->g_data->sgl_ghost.bone_outline);
		DRW_draw_pass(stl->g_data->sgl_ghost.non_meshes);
		DRW_draw_pass(stl->g_data->sgl_ghost.bone_axes);
	}

	batch_camera_path_free(&stl->g_data->sgl_ghost.camera_path);
	/* This has to be freed only after drawing empties! */
	if (stl->g_data->sgl_ghost.image_plane_map) {
		BLI_ghash_free(stl->g_data->sgl_ghost.image_plane_map, NULL, MEM_freeN);
		stl->g_data->sgl_ghost.image_plane_map = NULL;
	}
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
	&OBJECT_draw_scene,
	NULL,
	NULL,
};
