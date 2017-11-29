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

/** \file eevee_private.h
 *  \ingroup DNA
 */

#ifndef __EEVEE_PRIVATE_H__
#define __EEVEE_PRIVATE_H__

struct Object;

extern struct DrawEngineType draw_engine_eevee_type;

/* Minimum UBO is 16384 bytes */
#define MAX_PROBE 128 /* TODO : find size by dividing UBO max size by probe data size */
#define MAX_GRID 64 /* TODO : find size by dividing UBO max size by grid data size */
#define MAX_PLANAR 16 /* TODO : find size by dividing UBO max size by grid data size */
#define MAX_LIGHT 128 /* TODO : find size by dividing UBO max size by light data size */
#define MAX_CASCADE_NUM 4
#define MAX_SHADOW 256 /* TODO : Make this depends on GL_MAX_ARRAY_TEXTURE_LAYERS */
#define MAX_SHADOW_CASCADE 8
#define MAX_SHADOW_CUBE (MAX_SHADOW - MAX_CASCADE_NUM * MAX_SHADOW_CASCADE)
#define MAX_BLOOM_STEP 16

/* Only define one of these. */
// #define IRRADIANCE_SH_L2
// #define IRRADIANCE_CUBEMAP
#define IRRADIANCE_HL2

#if defined(IRRADIANCE_SH_L2)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_SH_L2\n"
#elif defined(IRRADIANCE_CUBEMAP)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_CUBEMAP\n"
#elif defined(IRRADIANCE_HL2)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_HL2\n"
#endif

#define SHADER_DEFINES \
	"#define EEVEE_ENGINE\n" \
	"#define MAX_PROBE " STRINGIFY(MAX_PROBE) "\n" \
	"#define MAX_GRID " STRINGIFY(MAX_GRID) "\n" \
	"#define MAX_PLANAR " STRINGIFY(MAX_PLANAR) "\n" \
	"#define MAX_LIGHT " STRINGIFY(MAX_LIGHT) "\n" \
	"#define MAX_SHADOW " STRINGIFY(MAX_SHADOW) "\n" \
	"#define MAX_SHADOW_CUBE " STRINGIFY(MAX_SHADOW_CUBE) "\n" \
	"#define MAX_SHADOW_CASCADE " STRINGIFY(MAX_SHADOW_CASCADE) "\n" \
	"#define MAX_CASCADE_NUM " STRINGIFY(MAX_CASCADE_NUM) "\n" \
	SHADER_IRRADIANCE

#define SWAP_DOUBLE_BUFFERS() {                                       \
	if (effects->swap_double_buffer) {                                \
		SWAP(struct GPUFrameBuffer *, fbl->main, fbl->double_buffer); \
		SWAP(GPUTexture *, txl->color, txl->color_double_buffer);     \
		effects->swap_double_buffer = false;                          \
	}                                                                 \
} ((void)0)

#define SWAP_BUFFERS() {                           \
	if (effects->target_buffer != fbl->main) {     \
		SWAP_DOUBLE_BUFFERS();                     \
		effects->source_buffer = txl->color_post;  \
		effects->target_buffer = fbl->main;        \
	}                                              \
	else {                                         \
		SWAP_DOUBLE_BUFFERS();                     \
		effects->source_buffer = txl->color;       \
		effects->target_buffer = fbl->effect_fb;   \
	}                                              \
} ((void)0)

/* World shader variations */
enum {
	VAR_WORLD_BACKGROUND    = 0,
	VAR_WORLD_PROBE         = 1,
	VAR_WORLD_VOLUME        = 2,
};

/* Material shader variations */
enum {
	VAR_MAT_MESH     = (1 << 0),
	VAR_MAT_PROBE    = (1 << 1),
	VAR_MAT_HAIR     = (1 << 2),
	VAR_MAT_FLAT     = (1 << 3),
	VAR_MAT_BLEND    = (1 << 4),
	VAR_MAT_VSM      = (1 << 5),
	VAR_MAT_ESM      = (1 << 6),
	/* Max number of variation */
	/* IMPORTANT : Leave it last and set
	 * it's value accordingly. */
	VAR_MAT_MAX      = (1 << 7),
	/* These are options that are not counted in VAR_MAT_MAX
	 * because they are not cumulative with the others above. */
	VAR_MAT_CLIP     = (1 << 8),
	VAR_MAT_HASH     = (1 << 9),
	VAR_MAT_MULT     = (1 << 10),
	VAR_MAT_SHADOW   = (1 << 11),
	VAR_MAT_REFRACT  = (1 << 12),
	VAR_MAT_VOLUME   = (1 << 13),
	VAR_MAT_SSS      = (1 << 14),
	VAR_MAT_TRANSLUC = (1 << 15),
	VAR_MAT_SSSALBED = (1 << 16),
};

/* Shadow Technique */
enum {
	SHADOW_ESM = 1,
	SHADOW_VSM = 2,
	SHADOW_METHOD_MAX = 3,
};

typedef struct EEVEE_PassList {
	/* Shadows */
	struct DRWPass *shadow_pass;
	struct DRWPass *shadow_cube_pass;
	struct DRWPass *shadow_cube_copy_pass;
	struct DRWPass *shadow_cube_store_pass;
	struct DRWPass *shadow_cascade_pass;
	struct DRWPass *shadow_cascade_copy_pass;
	struct DRWPass *shadow_cascade_store_pass;

	/* Probes */
	struct DRWPass *probe_background;
	struct DRWPass *probe_glossy_compute;
	struct DRWPass *probe_diffuse_compute;
	struct DRWPass *probe_grid_fill;
	struct DRWPass *probe_display;
	struct DRWPass *probe_planar_downsample_ps;

	/* Effects */
	struct DRWPass *ao_horizon_search;
	struct DRWPass *ao_horizon_debug;
	struct DRWPass *motion_blur;
	struct DRWPass *bloom_blit;
	struct DRWPass *bloom_downsample_first;
	struct DRWPass *bloom_downsample;
	struct DRWPass *bloom_upsample;
	struct DRWPass *bloom_resolve;
	struct DRWPass *dof_down;
	struct DRWPass *dof_scatter;
	struct DRWPass *dof_resolve;
	struct DRWPass *volumetric_world_ps;
	struct DRWPass *volumetric_objects_ps;
	struct DRWPass *volumetric_scatter_ps;
	struct DRWPass *volumetric_integration_ps;
	struct DRWPass *volumetric_resolve_ps;
	struct DRWPass *ssr_raytrace;
	struct DRWPass *ssr_resolve;
	struct DRWPass *sss_blur_ps;
	struct DRWPass *sss_resolve_ps;
	struct DRWPass *color_downsample_ps;
	struct DRWPass *color_downsample_cube_ps;
	struct DRWPass *taa_resolve;

	/* HiZ */
	struct DRWPass *minz_downlevel_ps;
	struct DRWPass *maxz_downlevel_ps;
	struct DRWPass *minz_downdepth_ps;
	struct DRWPass *maxz_downdepth_ps;
	struct DRWPass *minz_downdepth_layer_ps;
	struct DRWPass *maxz_downdepth_layer_ps;
	struct DRWPass *minz_copydepth_ps;
	struct DRWPass *maxz_copydepth_ps;

	struct DRWPass *depth_pass;
	struct DRWPass *depth_pass_cull;
	struct DRWPass *depth_pass_clip;
	struct DRWPass *depth_pass_clip_cull;
	struct DRWPass *refract_depth_pass;
	struct DRWPass *refract_depth_pass_cull;
	struct DRWPass *refract_depth_pass_clip;
	struct DRWPass *refract_depth_pass_clip_cull;
	struct DRWPass *default_pass[VAR_MAT_MAX];
	struct DRWPass *sss_pass;
	struct DRWPass *material_pass;
	struct DRWPass *refract_pass;
	struct DRWPass *transparent_pass;
	struct DRWPass *background_pass;
} EEVEE_PassList;

typedef struct EEVEE_FramebufferList {
	/* Effects */
	struct GPUFrameBuffer *gtao_fb;
	struct GPUFrameBuffer *gtao_debug_fb;
	struct GPUFrameBuffer *downsample_fb;
	struct GPUFrameBuffer *effect_fb;
	struct GPUFrameBuffer *bloom_blit_fb;
	struct GPUFrameBuffer *bloom_down_fb[MAX_BLOOM_STEP];
	struct GPUFrameBuffer *bloom_accum_fb[MAX_BLOOM_STEP - 1];
	struct GPUFrameBuffer *sss_blur_fb;
	struct GPUFrameBuffer *sss_clear_fb;
	struct GPUFrameBuffer *dof_down_fb;
	struct GPUFrameBuffer *dof_scatter_far_fb;
	struct GPUFrameBuffer *dof_scatter_near_fb;
	struct GPUFrameBuffer *volumetric_fb;
	struct GPUFrameBuffer *volumetric_scat_fb;
	struct GPUFrameBuffer *volumetric_integ_fb;
	struct GPUFrameBuffer *screen_tracing_fb;
	struct GPUFrameBuffer *refract_fb;

	struct GPUFrameBuffer *planarref_fb;

	struct GPUFrameBuffer *main;
	struct GPUFrameBuffer *double_buffer;
	struct GPUFrameBuffer *depth_double_buffer_fb;
} EEVEE_FramebufferList;

typedef struct EEVEE_TextureList {
	/* Effects */
	struct GPUTexture *color_post; /* R16_G16_B16 */
	struct GPUTexture *dof_down_near; /* R16_G16_B16_A16 */
	struct GPUTexture *dof_down_far; /* R16_G16_B16_A16 */
	struct GPUTexture *dof_coc; /* R16_G16 */
	struct GPUTexture *dof_near_blur; /* R16_G16_B16_A16 */
	struct GPUTexture *dof_far_blur; /* R16_G16_B16_A16 */
	struct GPUTexture *bloom_blit; /* R16_G16_B16 */
	struct GPUTexture *bloom_downsample[MAX_BLOOM_STEP]; /* R16_G16_B16 */
	struct GPUTexture *bloom_upsample[MAX_BLOOM_STEP - 1]; /* R16_G16_B16 */
	struct GPUTexture *ssr_normal_input;
	struct GPUTexture *ssr_specrough_input;
	struct GPUTexture *refract_color;

	struct GPUTexture *volume_prop_scattering;
	struct GPUTexture *volume_prop_extinction;
	struct GPUTexture *volume_prop_emission;
	struct GPUTexture *volume_prop_phase;
	struct GPUTexture *volume_scatter;
	struct GPUTexture *volume_transmittance;
	struct GPUTexture *volume_scatter_history;
	struct GPUTexture *volume_transmittance_history;

	struct GPUTexture *planar_pool;
	struct GPUTexture *planar_depth;

	struct GPUTexture *gtao_horizons;

	struct GPUTexture *sss_data;
	struct GPUTexture *sss_albedo;
	struct GPUTexture *sss_blur;
	struct GPUTexture *sss_stencil;

	struct GPUTexture *maxzbuffer;

	struct GPUTexture *color; /* R16_G16_B16 */
	struct GPUTexture *color_double_buffer;
	struct GPUTexture *depth_double_buffer;
} EEVEE_TextureList;

typedef struct EEVEE_StorageList {
	/* Effects */
	struct EEVEE_EffectsInfo *effects;

	struct EEVEE_PrivateData *g_data;
} EEVEE_StorageList;

/* ************ LIGHT UBO ************* */
typedef struct EEVEE_Light {
	float position[3], dist;
	float color[3], spec;
	float spotsize, spotblend, radius, shadowid;
	float rightvec[3], sizex;
	float upvec[3], sizey;
	float forwardvec[3], lamptype;
} EEVEE_Light;

typedef struct EEVEE_Shadow {
	float near, far, bias, exp;
	float shadow_start, data_start, multi_shadow_count, shadow_blur;
	float contact_dist, contact_bias, contact_spread, contact_thickness;
} EEVEE_Shadow;

typedef struct EEVEE_ShadowCube {
	float position[3], pad;
} EEVEE_ShadowCube;

typedef struct EEVEE_ShadowCascade {
	float shadowmat[MAX_CASCADE_NUM][4][4]; /* World->Lamp->NDC->Tex : used for sampling the shadow map. */
	float split_start[4];
	float split_end[4];
} EEVEE_ShadowCascade;

typedef struct EEVEE_ShadowRender {
	float shadowmat[6][4][4]; /* World->Lamp->NDC : used to render the shadow map. 6 frustum for cubemap shadow */
	float viewmat[6][4][4]; /* World->Lamp : used to render the shadow map. 6 viewmat for cubemap shadow */
	float position[3], pad;
	float cube_texel_size;
	float stored_texel_size;
	float clip_near;
	float clip_far;
	int shadow_samples_ct;
	float shadow_inv_samples_ct;
} EEVEE_ShadowRender;

/* ************ VOLUME DATA ************ */
typedef struct EEVEE_VolumetricsInfo {
	float integration_step_count, shadow_step_count, sample_distribution, light_clamp;
	float integration_start, integration_end;
	float depth_param[3], history_alpha;
	bool use_lights, use_volume_shadows;
	int froxel_tex_size[3];
	float inv_tex_size[3];
	float volume_coord_scale[2];
	float jitter[3];
} EEVEE_VolumetricsInfo;

/* ************ LIGHT DATA ************* */
typedef struct EEVEE_LampsInfo {
	int num_light, cache_num_light;
	int num_layer, cache_num_layer;
	int gpu_cube_ct, gpu_cascade_ct, gpu_shadow_ct;
	int cpu_cube_ct, cpu_cascade_ct;
	int update_flag;
	int shadow_size, shadow_method;
	bool shadow_high_bitdepth;
	int shadow_cube_target_size;
	int current_shadow_cascade;
	int current_shadow_face;
	float filter_size;
	/* List of lights in the scene. */
	/* XXX This is fragile, can get out of sync quickly. */
	struct Object *light_ref[MAX_LIGHT];
	struct Object *shadow_cube_ref[MAX_SHADOW_CUBE];
	struct Object *shadow_cascade_ref[MAX_SHADOW_CASCADE];
	/* UBO Storage : data used by UBO */
	struct EEVEE_Light         light_data[MAX_LIGHT];
	struct EEVEE_ShadowRender  shadow_render_data;
	struct EEVEE_Shadow        shadow_data[MAX_SHADOW];
	struct EEVEE_ShadowCube    shadow_cube_data[MAX_SHADOW_CUBE];
	struct EEVEE_ShadowCascade shadow_cascade_data[MAX_SHADOW_CASCADE];
} EEVEE_LampsInfo;

/* EEVEE_LampsInfo->update_flag */
enum {
	LIGHT_UPDATE_SHADOW_CUBE = (1 << 0),
};

/* ************ PROBE UBO ************* */
typedef struct EEVEE_LightProbe {
	float position[3], parallax_type;
	float attenuation_fac;
	float attenuation_type;
	float pad3[2];
	float attenuationmat[4][4];
	float parallaxmat[4][4];
} EEVEE_LightProbe;

typedef struct EEVEE_LightGrid {
	float mat[4][4];
	int resolution[3], offset;
	float corner[3], attenuation_scale;
	float increment_x[3], attenuation_bias; /* world space vector between 2 opposite cells */
	float increment_y[3], level_bias;
	float increment_z[3], pad4;
} EEVEE_LightGrid;

typedef struct EEVEE_PlanarReflection {
	float plane_equation[4];
	float clip_vec_x[3], attenuation_scale;
	float clip_vec_y[3], attenuation_bias;
	float clip_edge_x_pos, clip_edge_x_neg;
	float clip_edge_y_pos, clip_edge_y_neg;
	float facing_scale, facing_bias, pad[2];
	float reflectionmat[4][4];
} EEVEE_PlanarReflection;

/* ************ PROBE DATA ************* */
typedef struct EEVEE_LightProbesInfo {
	int num_cube, cache_num_cube;
	int num_grid, cache_num_grid;
	int num_planar, cache_num_planar;
	int update_flag;
	int updated_bounce;
	int num_bounce;
	int cubemap_res;
	int target_size;
	int grid_initialized;
	/* Actual number of probes that have datas. */
	int num_render_cube;
	int num_render_grid;
	/* For rendering probes */
	float probemat[6][4][4];
	int layer;
	float texel_size;
	float padding_size;
	float samples_ct;
	float invsamples_ct;
	float roughness;
	float lodfactor;
	float lod_rt_max, lod_cube_max, lod_planar_max;
	int shres;
	int shnbr;
	bool specular_toggle;
	bool ssr_toggle;
	bool sss_toggle;
	/* List of probes in the scene. */
	/* XXX This is fragile, can get out of sync quickly. */
	struct Object *probes_cube_ref[MAX_PROBE];
	struct Object *probes_grid_ref[MAX_GRID];
	struct Object *probes_planar_ref[MAX_PLANAR];
	/* UBO Storage : data used by UBO */
	struct EEVEE_LightProbe probe_data[MAX_PROBE];
	struct EEVEE_LightGrid grid_data[MAX_GRID];
	struct EEVEE_PlanarReflection planar_data[MAX_PLANAR];
} EEVEE_LightProbesInfo;

/* EEVEE_LightProbesInfo->update_flag */
enum {
	PROBE_UPDATE_CUBE = (1 << 0),
	PROBE_UPDATE_GRID = (1 << 1),
	PROBE_UPDATE_ALL  = 0xFFFFFF,
};

/* ************ EFFECTS DATA ************* */
typedef struct EEVEE_EffectsInfo {
	int enabled_effects;
	bool swap_double_buffer;

	/* SSSS */
	int sss_sample_count;
	float sss_jitter_threshold;
	bool sss_separate_albedo;

	/* Volumetrics */
	bool use_volumetrics;
	int volume_current_sample;

	/* SSR */
	bool use_ssr;
	bool reflection_trace_full;
	bool ssr_use_normalization;
	int ssr_ray_count;
	float ssr_firefly_fac;
	float ssr_border_fac;
	float ssr_max_roughness;
	float ssr_quality;
	float ssr_thickness;
	float ssr_pixelsize[2];

	/* Temporal Anti Aliasing */
	int taa_current_sample;
	int taa_total_sample;
	float taa_alpha;
	bool prev_drw_support;
	float prev_drw_persmat[4][4];
	float overide_persmat[4][4];
	float overide_persinv[4][4];
	float overide_winmat[4][4];
	float overide_wininv[4][4];

	/* Ambient Occlusion */
	bool use_ao, use_bent_normals;
	float ao_dist, ao_samples, ao_factor, ao_samples_inv;
	float ao_offset, ao_bounce_fac, ao_quality, ao_settings;
	float ao_sample_nbr;
	int ao_texsize[2], hori_tex_layers;

	/* Motion Blur */
	float current_ndc_to_world[4][4];
	float past_world_to_ndc[4][4];
	float tmp_mat[4][4];
	int motion_blur_samples;

	/* Depth Of Field */
	float dof_near_far[2];
	float dof_params[3];
	float dof_bokeh[4];
	float dof_layer_select[2];
	int dof_target_size[2];

	/* Bloom */
	int bloom_iteration_ct;
	float source_texel_size[2];
	float blit_texel_size[2];
	float downsamp_texel_size[MAX_BLOOM_STEP][2];
	float bloom_color[3];
	float bloom_clamp;
	float bloom_sample_scale;
	float bloom_curve_threshold[4];
	float unf_source_texel_size[2];
	struct GPUTexture *unf_source_buffer; /* pointer copy */
	struct GPUTexture *unf_base_buffer; /* pointer copy */

	/* Not alloced, just a copy of a *GPUtexture in EEVEE_TextureList. */
	struct GPUTexture *source_buffer;       /* latest updated texture */
	struct GPUFrameBuffer *target_buffer;   /* next target to render to */
} EEVEE_EffectsInfo;

enum {
	EFFECT_MOTION_BLUR         = (1 << 0),
	EFFECT_BLOOM               = (1 << 1),
	EFFECT_DOF                 = (1 << 2),
	EFFECT_VOLUMETRIC          = (1 << 3),
	EFFECT_SSR                 = (1 << 4),
	EFFECT_DOUBLE_BUFFER       = (1 << 5), /* Not really an effect but a feature */
	EFFECT_REFRACT             = (1 << 6),
	EFFECT_GTAO                = (1 << 7),
	EFFECT_TAA                 = (1 << 8),
	EFFECT_POST_BUFFER         = (1 << 9), /* Not really an effect but a feature */
	EFFECT_NORMAL_BUFFER       = (1 << 10), /* Not really an effect but a feature */
	EFFECT_SSS                 = (1 << 11),
};

/* ************** SCENE LAYER DATA ************** */
typedef struct EEVEE_ViewLayerData {
	/* Lamps */
	struct EEVEE_LampsInfo *lamps;

	struct GPUUniformBuffer *light_ubo;
	struct GPUUniformBuffer *shadow_ubo;
	struct GPUUniformBuffer *shadow_render_ubo;
	struct GPUUniformBuffer *shadow_samples_ubo;

	struct GPUFrameBuffer *shadow_target_fb;
	struct GPUFrameBuffer *shadow_store_fb;

	struct GPUTexture *shadow_cube_target;
	struct GPUTexture *shadow_cube_blur;
	struct GPUTexture *shadow_cascade_target;
	struct GPUTexture *shadow_cascade_blur;
	struct GPUTexture *shadow_pool;

	struct ListBase shadow_casters; /* Shadow casters gathered during cache iteration */

	/* Probes */
	struct EEVEE_LightProbesInfo *probes;

	struct GPUUniformBuffer *probe_ubo;
	struct GPUUniformBuffer *grid_ubo;
	struct GPUUniformBuffer *planar_ubo;

	struct GPUFrameBuffer *probe_fb;
	struct GPUFrameBuffer *probe_filter_fb;

	struct GPUTexture *probe_rt;
	struct GPUTexture *probe_pool;
	struct GPUTexture *irradiance_pool;
	struct GPUTexture *irradiance_rt;

	struct ListBase probe_queue; /* List of probes to update */

	/* Volumetrics */
	struct EEVEE_VolumetricsInfo *volumetrics;
} EEVEE_ViewLayerData;

/* ************ OBJECT DATA ************ */
typedef struct EEVEE_LampEngineData {
	bool need_update;
	struct ListBase shadow_caster_list;
	void *storage; /* either EEVEE_LightData, EEVEE_ShadowCubeData, EEVEE_ShadowCascadeData */
} EEVEE_LampEngineData;

typedef struct EEVEE_LightProbeEngineData {
	/* NOTE: need_full_update is set by dependency graph when the probe or it's
	 * object is updated. This triggers full probe update, including it's
	 * "progressive" GI refresh.
	 *
	 * need_update is always set to truth when need_full_update is tagged, but
	 * might also be forced to be kept truth during GI refresh stages.
	 *
	 * TODO(sergey): Is there a way to avoid two flags here, or at least make
	 * it more clear what's going on here?
	 */
	bool need_full_update;
	bool need_update;

	bool ready_to_shade;
	int updated_cells;
	int updated_lvl;
	int num_cell;
	int max_lvl;
	int probe_id; /* Only used for display data */
	float probe_size; /* Only used for display data */
	/* For planar reflection rendering */
	float viewmat[4][4];
	float persmat[4][4];
	float planer_eq_offset[4];
	struct ListBase captured_object_list;
} EEVEE_LightProbeEngineData;

typedef struct EEVEE_ObjectEngineData {
	bool need_update;
} EEVEE_ObjectEngineData;

/* *********************************** */

typedef struct EEVEE_Data {
	void *engine_type;
	EEVEE_FramebufferList *fbl;
	EEVEE_TextureList *txl;
	EEVEE_PassList *psl;
	EEVEE_StorageList *stl;
} EEVEE_Data;

typedef struct EEVEE_PrivateData {
	struct DRWShadingGroup *shadow_shgrp;
	struct DRWShadingGroup *depth_shgrp;
	struct DRWShadingGroup *depth_shgrp_cull;
	struct DRWShadingGroup *depth_shgrp_clip;
	struct DRWShadingGroup *depth_shgrp_clip_cull;
	struct DRWShadingGroup *refract_depth_shgrp;
	struct DRWShadingGroup *refract_depth_shgrp_cull;
	struct DRWShadingGroup *refract_depth_shgrp_clip;
	struct DRWShadingGroup *refract_depth_shgrp_clip_cull;
	struct DRWShadingGroup *cube_display_shgrp;
	struct DRWShadingGroup *planar_display_shgrp;
	struct DRWShadingGroup *planar_downsample;
	struct GHash *material_hash;
	struct GHash *hair_material_hash;
	struct GPUTexture *minzbuffer;
	struct GPUTexture *ssr_hit_output[4];
	struct GPUTexture *gtao_horizons_debug;
	float background_alpha; /* TODO find a better place for this. */
	float viewvecs[2][4];
	/* For planar probes */
	float texel_size[2];
	/* To correct mip level texel mis-alignement */
	float mip_ratio[10][2]; /* TODO put in a UBO */
	/* For double buffering */
	bool view_updated;
	bool valid_double_buffer;
	float prev_persmat[4][4];
} EEVEE_PrivateData; /* Transient data */

/* eevee_data.c */
EEVEE_ViewLayerData *EEVEE_view_layer_data_get(void);
EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure(void);
EEVEE_ObjectEngineData *EEVEE_object_data_get(Object *ob);
EEVEE_ObjectEngineData *EEVEE_object_data_ensure(Object *ob);
EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_get(Object *ob);
EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_ensure(Object *ob);
EEVEE_LampEngineData *EEVEE_lamp_data_get(Object *ob);
EEVEE_LampEngineData *EEVEE_lamp_data_ensure(Object *ob);

/* eevee_materials.c */
struct GPUTexture *EEVEE_materials_get_util_tex(void); /* XXX */
void EEVEE_materials_init(EEVEE_StorageList *stl);
void EEVEE_materials_cache_init(EEVEE_Data *vedata);
void EEVEE_materials_cache_populate(EEVEE_Data *vedata, EEVEE_ViewLayerData *sldata, Object *ob);
void EEVEE_materials_cache_finish(EEVEE_Data *vedata);
struct GPUMaterial *EEVEE_material_world_lightprobe_get(struct Scene *scene, struct World *wo);
struct GPUMaterial *EEVEE_material_world_background_get(struct Scene *scene, struct World *wo);
struct GPUMaterial *EEVEE_material_world_volume_get(struct Scene *scene, struct World *wo);
struct GPUMaterial *EEVEE_material_mesh_get(
        struct Scene *scene, Material *ma, EEVEE_Data *vedata,
        bool use_blend, bool use_multiply, bool use_refract, bool use_sss, bool use_translucency, int shadow_method);
struct GPUMaterial *EEVEE_material_mesh_volume_get(struct Scene *scene, Material *ma);
struct GPUMaterial *EEVEE_material_mesh_depth_get(struct Scene *scene, Material *ma, bool use_hashed_alpha, bool is_shadow);
struct GPUMaterial *EEVEE_material_hair_get(struct Scene *scene, Material *ma, int shadow_method);
void EEVEE_materials_free(void);
void EEVEE_draw_default_passes(EEVEE_PassList *psl);
void EEVEE_update_util_texture(float offset);

/* eevee_lights.c */
void EEVEE_lights_init(EEVEE_ViewLayerData *sldata);
void EEVEE_lights_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_PassList *psl);
void EEVEE_lights_cache_add(EEVEE_ViewLayerData *sldata, struct Object *ob);
void EEVEE_lights_cache_shcaster_add(
        EEVEE_ViewLayerData *sldata, EEVEE_PassList *psl, struct Gwn_Batch *geom, float (*obmat)[4]);
void EEVEE_lights_cache_shcaster_material_add(
        EEVEE_ViewLayerData *sldata, EEVEE_PassList *psl,
        struct GPUMaterial *gpumat, struct Gwn_Batch *geom, struct Object *ob,
        float (*obmat)[4], float *alpha_threshold);
void EEVEE_lights_cache_finish(EEVEE_ViewLayerData *sldata);
void EEVEE_lights_update(EEVEE_ViewLayerData *sldata);
void EEVEE_draw_shadows(EEVEE_ViewLayerData *sldata, EEVEE_PassList *psl);
void EEVEE_lights_free(void);

/* eevee_lightprobes.c */
void EEVEE_lightprobes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_cache_add(EEVEE_ViewLayerData *sldata, Object *ob);
void EEVEE_lightprobes_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_refresh(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_free(void);

/* eevee_depth_of_field.c */
int EEVEE_depth_of_field_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_depth_of_field_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_depth_of_field_draw(EEVEE_Data *vedata);
void EEVEE_depth_of_field_free(void);

/* eevee_bloom.c */
int EEVEE_bloom_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_draw(EEVEE_Data *vedata);
void EEVEE_bloom_free(void);

/* eevee_occlusion.c */
int EEVEE_occlusion_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_draw_debug(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_free(void);

/* eevee_screen_raytrace.c */
int EEVEE_screen_raytrace_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_screen_raytrace_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_refraction_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_reflection_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_screen_raytrace_free(void);

/* eevee_subsurface.c */
int EEVEE_subsurface_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_add_pass(EEVEE_Data *vedata, unsigned int sss_id, struct GPUUniformBuffer *sss_profile);
void EEVEE_subsurface_data_render(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_free(void);

/* eevee_motion_blur.c */
int EEVEE_motion_blur_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_motion_blur_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_motion_blur_draw(EEVEE_Data *vedata);
void EEVEE_motion_blur_free(void);

/* eevee_temporal_sampling.c */
int EEVEE_temporal_sampling_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_temporal_sampling_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_temporal_sampling_draw(EEVEE_Data *vedata);
void EEVEE_temporal_sampling_free(void);

/* eevee_volumes.c */
int EEVEE_volumes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_cache_object_add(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, struct Scene *scene, Object *ob);
void EEVEE_volumes_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_resolve(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_free_smoke_textures(void);
void EEVEE_volumes_free(void);

/* eevee_effects.c */
void EEVEE_effects_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_effects_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_create_minmax_buffer(EEVEE_Data *vedata, struct GPUTexture *depth_src, int layer);
void EEVEE_downsample_buffer(EEVEE_Data *vedata, struct GPUFrameBuffer *fb_src, struct GPUTexture *texture_src, int level);
void EEVEE_downsample_cube_buffer(EEVEE_Data *vedata, struct GPUFrameBuffer *fb_src, struct GPUTexture *texture_src, int level);
void EEVEE_effects_do_gtao(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_draw_effects(EEVEE_Data *vedata);
void EEVEE_effects_free(void);

/* Shadow Matrix */
static const float texcomat[4][4] = { /* From NDC to TexCo */
	{0.5f, 0.0f, 0.0f, 0.0f},
	{0.0f, 0.5f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.5f, 0.0f},
	{0.5f, 0.5f, 0.5f, 1.0f}
};

/* Cubemap Matrices */
static const float cubefacemat[6][4][4] = {
	/* Pos X */
	{{0.0f, 0.0f, -1.0f, 0.0f},
	 {0.0f, -1.0f, 0.0f, 0.0f},
	 {-1.0f, 0.0f, 0.0f, 0.0f},
	 {0.0f, 0.0f, 0.0f, 1.0f}},
	/* Neg X */
	{{0.0f, 0.0f, 1.0f, 0.0f},
	 {0.0f, -1.0f, 0.0f, 0.0f},
	 {1.0f, 0.0f, 0.0f, 0.0f},
	 {0.0f, 0.0f, 0.0f, 1.0f}},
	/* Pos Y */
	{{1.0f, 0.0f, 0.0f, 0.0f},
	 {0.0f, 0.0f, -1.0f, 0.0f},
	 {0.0f, 1.0f, 0.0f, 0.0f},
	 {0.0f, 0.0f, 0.0f, 1.0f}},
	/* Neg Y */
	{{1.0f, 0.0f, 0.0f, 0.0f},
	 {0.0f, 0.0f, 1.0f, 0.0f},
	 {0.0f, -1.0f, 0.0f, 0.0f},
	 {0.0f, 0.0f, 0.0f, 1.0f}},
	/* Pos Z */
	{{1.0f, 0.0f, 0.0f, 0.0f},
	 {0.0f, -1.0f, 0.0f, 0.0f},
	 {0.0f, 0.0f, -1.0f, 0.0f},
	 {0.0f, 0.0f, 0.0f, 1.0f}},
	/* Neg Z */
	{{-1.0f, 0.0f, 0.0f, 0.0f},
	 {0.0f, -1.0f, 0.0f, 0.0f},
	 {0.0f, 0.0f, 1.0f, 0.0f},
	 {0.0f, 0.0f, 0.0f, 1.0f}},
};

#endif /* __EEVEE_PRIVATE_H__ */
