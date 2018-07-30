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

/** \file workbench_private.h
 *  \ingroup draw_engine
 */

#ifndef __WORKBENCH_PRIVATE_H__
#define __WORKBENCH_PRIVATE_H__


#include "BKE_studiolight.h"

#include "DNA_image_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_userdef_types.h"

#include "DRW_render.h"

#include "workbench_engine.h"

#define WORKBENCH_ENGINE "BLENDER_WORKBENCH"
#define M_GOLDEN_RATION_CONJUGATE 0.618033988749895
#define MAX_SHADERS (1 << 10)

#define TEXTURE_DRAWING_ENABLED(wpd) (wpd->shading.color_type & V3D_SHADING_TEXTURE_COLOR)
#define FLAT_ENABLED(wpd) (wpd->shading.light == V3D_LIGHTING_FLAT)
#define STUDIOLIGHT_ENABLED(wpd) (wpd->shading.light == V3D_LIGHTING_STUDIO)
#define MATCAP_ENABLED(wpd) (wpd->shading.light == V3D_LIGHTING_MATCAP)
#define STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd) (STUDIOLIGHT_ENABLED(wpd) && (wpd->studio_light->flag & STUDIOLIGHT_ORIENTATION_WORLD))
#define STUDIOLIGHT_ORIENTATION_CAMERA_ENABLED(wpd) (STUDIOLIGHT_ENABLED(wpd) && (wpd->studio_light->flag & STUDIOLIGHT_ORIENTATION_CAMERA))
#define STUDIOLIGHT_ORIENTATION_VIEWNORMAL_ENABLED(wpd) (MATCAP_ENABLED(wpd) && (wpd->studio_light->flag & STUDIOLIGHT_ORIENTATION_VIEWNORMAL))
#define CAVITY_ENABLED(wpd) (wpd->shading.flag & V3D_SHADING_CAVITY)
#define SHADOW_ENABLED(wpd) (wpd->shading.flag & V3D_SHADING_SHADOW)

#define IS_NAVIGATING(wpd) ((DRW_context_state_get()->rv3d) && (DRW_context_state_get()->rv3d->rflag & RV3D_NAVIGATING))
#define FXAA_ENABLED(wpd) ((!DRW_state_is_opengl_render()) && \
                            (IN_RANGE(wpd->user_preferences->gpu_viewport_quality, GPU_VIEWPORT_QUALITY_FXAA, GPU_VIEWPORT_QUALITY_TAA8) || \
                             ((IS_NAVIGATING(wpd) || wpd->is_playback) && (wpd->user_preferences->gpu_viewport_quality >= GPU_VIEWPORT_QUALITY_TAA8))))
#define TAA_ENABLED(wpd) (wpd->user_preferences->gpu_viewport_quality >= GPU_VIEWPORT_QUALITY_TAA8 && !IS_NAVIGATING(wpd) && !wpd->is_playback)
#define SPECULAR_HIGHLIGHT_ENABLED(wpd) ((wpd->shading.flag & V3D_SHADING_SPECULAR_HIGHLIGHT) && (!STUDIOLIGHT_ORIENTATION_VIEWNORMAL_ENABLED(wpd)))
#define OBJECT_ID_PASS_ENABLED(wpd) (wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE)
#define NORMAL_VIEWPORT_COMP_PASS_ENABLED(wpd) (MATCAP_ENABLED(wpd) || STUDIOLIGHT_ENABLED(wpd) || SHADOW_ENABLED(wpd) || SPECULAR_HIGHLIGHT_ENABLED(wpd))
#define NORMAL_VIEWPORT_PASS_ENABLED(wpd) (NORMAL_VIEWPORT_COMP_PASS_ENABLED(wpd) || CAVITY_ENABLED(wpd))
#define NORMAL_ENCODING_ENABLED() (true)


struct RenderEngine;
struct RenderLayer;
struct rcti;


typedef struct WORKBENCH_FramebufferList {
	/* Deferred render buffers */
	struct GPUFrameBuffer *prepass_fb;
	struct GPUFrameBuffer *cavity_fb;
	struct GPUFrameBuffer *composite_fb;

	struct GPUFrameBuffer *effect_fb;
	struct GPUFrameBuffer *effect_taa_fb;
	struct GPUFrameBuffer *depth_buffer_fb;
	struct GPUFrameBuffer *volume_fb;

	/* Forward render buffers */
	struct GPUFrameBuffer *object_outline_fb;
	struct GPUFrameBuffer *transparent_accum_fb;
	struct GPUFrameBuffer *transparent_revealage_fb;
} WORKBENCH_FramebufferList;

typedef struct WORKBENCH_TextureList {
	struct GPUTexture *history_buffer_tx;
	struct GPUTexture *depth_buffer_tx;
} WORKBENCH_TextureList;

typedef struct WORKBENCH_StorageList {
	struct WORKBENCH_PrivateData *g_data;
	struct WORKBENCH_EffectInfo *effects;
} WORKBENCH_StorageList;

typedef struct WORKBENCH_PassList {
	/* deferred rendering */
	struct DRWPass *prepass_pass;
	struct DRWPass *prepass_hair_pass;
	struct DRWPass *cavity_pass;
	struct DRWPass *shadow_depth_pass_pass;
	struct DRWPass *shadow_depth_pass_mani_pass;
	struct DRWPass *shadow_depth_fail_pass;
	struct DRWPass *shadow_depth_fail_mani_pass;
	struct DRWPass *shadow_depth_fail_caps_pass;
	struct DRWPass *shadow_depth_fail_caps_mani_pass;
	struct DRWPass *composite_pass;
	struct DRWPass *composite_shadow_pass;
	struct DRWPass *effect_aa_pass;
	struct DRWPass *volume_pass;

	/* forward rendering */
	struct DRWPass *transparent_accum_pass;
	struct DRWPass *object_outline_pass;
	struct DRWPass *depth_pass;
	struct DRWPass *checker_depth_pass;
} WORKBENCH_PassList;

typedef struct WORKBENCH_Data {
	void *engine_type;
	WORKBENCH_FramebufferList *fbl;
	WORKBENCH_TextureList *txl;
	WORKBENCH_PassList *psl;
	WORKBENCH_StorageList *stl;
} WORKBENCH_Data;

typedef struct WORKBENCH_UBO_Light {
	float light_direction_vs[4];
	float specular_color[3];
	float energy;
} WORKBENCH_UBO_Light;

typedef struct WORKBENCH_UBO_World {
	float spherical_harmonics_coefs[STUDIOLIGHT_SPHERICAL_HARMONICS_MAX_COMPONENTS][4];
	float background_color_low[4];
	float background_color_high[4];
	float object_outline_color[4];
	float shadow_direction_vs[4];
	WORKBENCH_UBO_Light lights[3];
	int num_lights;
	int matcap_orientation;
	float background_alpha;
	int pad[1];
} WORKBENCH_UBO_World;
BLI_STATIC_ASSERT_ALIGN(WORKBENCH_UBO_World, 16)


typedef struct WORKBENCH_PrivateData {
	struct GHash *material_hash;
	struct GPUShader *prepass_solid_sh;
	struct GPUShader *prepass_solid_hair_sh;
	struct GPUShader *prepass_texture_sh;
	struct GPUShader *prepass_texture_hair_sh;
	struct GPUShader *composite_sh;
	struct GPUShader *transparent_accum_sh;
	struct GPUShader *transparent_accum_hair_sh;
	struct GPUShader *transparent_accum_texture_sh;
	struct GPUShader *transparent_accum_texture_hair_sh;
	View3DShading shading;
	StudioLight *studio_light;
	UserDef *user_preferences;
	struct GPUUniformBuffer *world_ubo;
	struct DRWShadingGroup *shadow_shgrp;
	struct DRWShadingGroup *depth_shgrp;
	WORKBENCH_UBO_World world_data;
	float shadow_multiplier;
	float cached_shadow_direction[3];
	float shadow_mat[4][4];
	float shadow_inv[4][4];
	float shadow_far_plane[4]; /* Far plane of the view frustum. */
	float shadow_near_corners[4][3]; /* Near plane corners in shadow space. */
	float shadow_near_min[3]; /* min and max of shadow_near_corners. allow fast test */
	float shadow_near_max[3];
	float shadow_near_sides[2][4]; /* This is a parallelogram, so only 2 normal and distance to the edges. */
	bool shadow_changed;
	bool is_playback;

	/* Volumes */
	bool volumes_do;
	ListBase smoke_domains;

	/* Ssao */
	float winmat[4][4];
	float viewvecs[3][4];
	float ssao_params[4];
	float ssao_settings[4];
} WORKBENCH_PrivateData; /* Transient data */

typedef struct WORKBENCH_EffectInfo {
	float override_persmat[4][4];
	float override_persinv[4][4];
	float override_winmat[4][4];
	float override_wininv[4][4];
	float last_mat[4][4];
	float curr_mat[4][4];
	int jitter_index;
	float taa_mix_factor;
	bool view_updated;
} WORKBENCH_EffectInfo;

typedef struct WORKBENCH_MaterialData {
	float diffuse_color[4];
	float specular_color[4];
	float roughness;
	int object_id;
	int color_type;
	Image *ima;

	/* Linked shgroup for drawing */
	DRWShadingGroup *shgrp;
	/* forward rendering */
	DRWShadingGroup *shgrp_object_outline;
} WORKBENCH_MaterialData;

typedef struct WORKBENCH_ObjectData {
	DrawData dd;

	/* Shadow direction in local object space. */
	float shadow_dir[3], shadow_depth;
	float shadow_min[3], shadow_max[3]; /* Min, max in shadow space */
	BoundBox shadow_bbox;
	bool shadow_bbox_dirty;

	int object_id;
} WORKBENCH_ObjectData;

/* workbench_engine.c */
void workbench_solid_materials_init(WORKBENCH_Data *vedata);
void workbench_solid_materials_cache_init(WORKBENCH_Data *vedata);
void workbench_solid_materials_cache_populate(WORKBENCH_Data *vedata, Object *ob);
void workbench_solid_materials_cache_finish(WORKBENCH_Data *vedata);
void workbench_solid_materials_draw_scene(WORKBENCH_Data *vedata);
void workbench_solid_materials_free(void);

/* workbench_deferred.c */
void workbench_deferred_engine_init(WORKBENCH_Data *vedata);
void workbench_deferred_engine_free(void);
void workbench_deferred_draw_background(WORKBENCH_Data *vedata);
void workbench_deferred_draw_scene(WORKBENCH_Data *vedata);
void workbench_deferred_draw_finish(WORKBENCH_Data *vedata);
void workbench_deferred_cache_init(WORKBENCH_Data *vedata);
void workbench_deferred_solid_cache_populate(WORKBENCH_Data *vedata, Object *ob);
void workbench_deferred_cache_finish(WORKBENCH_Data *vedata);

/* workbench_forward.c */
void workbench_forward_engine_init(WORKBENCH_Data *vedata);
void workbench_forward_engine_free(void);
void workbench_forward_draw_background(WORKBENCH_Data *vedata);
void workbench_forward_draw_scene(WORKBENCH_Data *vedata);
void workbench_forward_cache_init(WORKBENCH_Data *vedata);
void workbench_forward_cache_populate(WORKBENCH_Data *vedata, Object *ob);
void workbench_forward_cache_finish(WORKBENCH_Data *vedata);

/* workbench_effect_aa.c */
void workbench_aa_create_pass(WORKBENCH_Data *vedata, GPUTexture **tx);
void workbench_aa_draw_pass(WORKBENCH_Data *vedata, GPUTexture *tx);

/* workbench_effect_fxaa.c */
void workbench_fxaa_engine_init(void);
void workbench_fxaa_engine_free(void);
DRWPass *workbench_fxaa_create_pass(GPUTexture **color_buffer_tx);

/* workbench_effect_taa.c */
void workbench_taa_engine_init(WORKBENCH_Data *vedata);
void workbench_taa_engine_free(void);
DRWPass *workbench_taa_create_pass(WORKBENCH_Data *vedata, GPUTexture **color_buffer_tx);
void workbench_taa_draw_scene_start(WORKBENCH_Data *vedata);
void workbench_taa_draw_scene_end(WORKBENCH_Data *vedata);
void workbench_taa_view_updated(WORKBENCH_Data *vedata);
int workbench_taa_calculate_num_iterations(WORKBENCH_Data *vedata);

/* workbench_materials.c */
int workbench_material_determine_color_type(WORKBENCH_PrivateData *wpd, Image *ima);
char *workbench_material_build_defines(WORKBENCH_PrivateData *wpd, bool use_textures, bool is_hair);
void workbench_material_update_data(WORKBENCH_PrivateData *wpd, Object *ob, Material *mat, WORKBENCH_MaterialData *data);
uint workbench_material_get_hash(WORKBENCH_MaterialData *material_template);
int workbench_material_get_shader_index(WORKBENCH_PrivateData *wpd, bool use_textures, bool is_hair);
void workbench_material_set_normal_world_matrix(
        DRWShadingGroup *grp, WORKBENCH_PrivateData *wpd, float persistent_matrix[3][3]);
void workbench_material_shgroup_uniform(WORKBENCH_PrivateData *wpd, DRWShadingGroup *grp, WORKBENCH_MaterialData *material);
void workbench_material_copy(WORKBENCH_MaterialData *dest_material, const WORKBENCH_MaterialData *source_material);

/* workbench_studiolight.c */
void studiolight_update_world(StudioLight *sl, WORKBENCH_UBO_World *wd);
void studiolight_update_light(WORKBENCH_PrivateData *wpd, const float light_direction[3]);
bool studiolight_object_cast_visible_shadow(WORKBENCH_PrivateData *wpd, Object *ob, WORKBENCH_ObjectData *oed);
float studiolight_object_shadow_distance(WORKBENCH_PrivateData *wpd, Object *ob, WORKBENCH_ObjectData *oed);
bool studiolight_camera_in_object_shadow(WORKBENCH_PrivateData *wpd, Object *ob, WORKBENCH_ObjectData *oed);

/* workbench_data.c */
void workbench_effect_info_init(WORKBENCH_EffectInfo *effect_info);
void workbench_private_data_init(WORKBENCH_PrivateData *wpd);
void workbench_private_data_free(WORKBENCH_PrivateData *wpd);
void workbench_private_data_get_light_direction(WORKBENCH_PrivateData *wpd, float r_light_direction[3]);

/* workbench_volume.c */
void workbench_volume_engine_init(void);
void workbench_volume_engine_free(void);
void workbench_volume_cache_init(WORKBENCH_Data *vedata);
void workbench_volume_cache_populate(WORKBENCH_Data *vedata, Scene *scene, Object *ob, struct ModifierData *md);
void workbench_volume_smoke_textures_free(WORKBENCH_PrivateData *wpd);

/* workbench_render.c */
void workbench_render(WORKBENCH_Data *vedata, struct RenderEngine *engine, struct RenderLayer *render_layer, const struct rcti *rect);
void workbench_render_update_passes(struct RenderEngine *engine, struct Scene *scene, struct ViewLayer *view_layer);

#endif
