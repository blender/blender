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

/** \file eevee_materials.c
 *  \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_world_types.h"
#include "DNA_modifier_types.h"

#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_alloca.h"

#include "BKE_particle.h"

#include "GPU_material.h"

#include "eevee_engine.h"
#include "eevee_lut.h"
#include "eevee_private.h"

#if defined(IRRADIANCE_SH_L2)
#define SHADER_IRRADIANCE "#define IRRADIANCE_SH_L2\n"
#elif defined(IRRADIANCE_CUBEMAP)
#define SHADER_IRRADIANCE "#define IRRADIANCE_CUBEMAP\n"
#elif defined(IRRADIANCE_HL2)
#define SHADER_IRRADIANCE "#define IRRADIANCE_HL2\n"
#endif

#define SHADER_DEFINES \
	"#define EEVEE_ENGINE\n" \
	"#define MAX_PROBE " STRINGIFY(MAX_PROBE) "\n" \
	"#define MAX_GRID " STRINGIFY(MAX_GRID) "\n" \
	"#define MAX_PLANAR " STRINGIFY(MAX_PLANAR) "\n" \
	"#define MAX_LIGHT " STRINGIFY(MAX_LIGHT) "\n" \
	"#define MAX_SHADOW_CUBE " STRINGIFY(MAX_SHADOW_CUBE) "\n" \
	"#define MAX_SHADOW_MAP " STRINGIFY(MAX_SHADOW_MAP) "\n" \
	"#define MAX_SHADOW_CASCADE " STRINGIFY(MAX_SHADOW_CASCADE) "\n" \
	"#define MAX_CASCADE_NUM " STRINGIFY(MAX_CASCADE_NUM) "\n" \
	SHADER_IRRADIANCE

/* *********** STATIC *********** */
static struct {
	char *frag_shader_lib;

	struct GPUShader *default_prepass_sh;
	struct GPUShader *default_prepass_clip_sh;
	struct GPUShader *default_lit[VAR_MAT_MAX];

	struct GPUShader *default_background;

	/* 64*64 array texture containing all LUTs and other utilitarian arrays.
	 * Packing enables us to same precious textures slots. */
	struct GPUTexture *util_tex;

	float viewvecs[2][4];
} e_data = {NULL}; /* Engine data */

extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_prepass_frag_glsl[];
extern char datatoc_prepass_vert_glsl[];
extern char datatoc_default_frag_glsl[];
extern char datatoc_default_world_frag_glsl[];
extern char datatoc_ltc_lib_glsl[];
extern char datatoc_bsdf_lut_frag_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_direct_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_irradiance_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_lit_surface_frag_glsl[];
extern char datatoc_lit_surface_vert_glsl[];
extern char datatoc_shadow_frag_glsl[];
extern char datatoc_shadow_geom_glsl[];
extern char datatoc_shadow_vert_glsl[];
extern char datatoc_lightprobe_geom_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_background_vert_glsl[];

extern Material defmaterial;
extern GlobalsUboStorage ts;

/* *********** FUNCTIONS *********** */

#if 0 /* Used only to generate the LUT values */
static struct GPUTexture *create_ggx_lut_texture(int UNUSED(w), int UNUSED(h))
{
	struct GPUTexture *tex;
	struct GPUFrameBuffer *fb = NULL;
	static float samples_ct = 8192.0f;
	static float inv_samples_ct = 1.0f / 8192.0f;

	char *lib_str = NULL;

	DynStr *ds_vert = BLI_dynstr_new();
	BLI_dynstr_append(ds_vert, datatoc_bsdf_common_lib_glsl);
	BLI_dynstr_append(ds_vert, datatoc_bsdf_sampling_lib_glsl);
	lib_str = BLI_dynstr_get_cstring(ds_vert);
	BLI_dynstr_free(ds_vert);

	struct GPUShader *sh = DRW_shader_create_with_lib(
	        datatoc_lightprobe_vert_glsl, datatoc_lightprobe_geom_glsl, datatoc_bsdf_lut_frag_glsl, lib_str,
	        "#define HAMMERSLEY_SIZE 8192\n"
	        "#define BRDF_LUT_SIZE 64\n"
	        "#define NOISE_SIZE 64\n");

	DRWPass *pass = DRW_pass_create("LightProbe Filtering", DRW_STATE_WRITE_COLOR);
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_float(grp, "sampleCount", &samples_ct, 1);
	DRW_shgroup_uniform_float(grp, "invSampleCount", &inv_samples_ct, 1);
	DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
	DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);

	struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
	DRW_shgroup_call_add(grp, geom, NULL);

	float *texels = MEM_mallocN(sizeof(float[2]) * w * h, "lut");

	tex = DRW_texture_create_2D(w, h, DRW_TEX_RG_16, DRW_TEX_FILTER, (float *)texels);

	DRWFboTexture tex_filter = {&tex, DRW_TEX_RG_16, DRW_TEX_FILTER};
	DRW_framebuffer_init(&fb, &draw_engine_eevee_type, w, h, &tex_filter, 1);

	DRW_framebuffer_bind(fb);
	DRW_draw_pass(pass);

	float *data = MEM_mallocN(sizeof(float[3]) * w * h, "lut");
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, data);

	printf("{");
	for (int i = 0; i < w*h * 3; i+=3) {
		printf("%ff, %ff, ", data[i],  data[i+1]); i+=3;
		printf("%ff, %ff, ", data[i],  data[i+1]); i+=3;
		printf("%ff, %ff, ", data[i],  data[i+1]); i+=3;
		printf("%ff, %ff, \n", data[i],  data[i+1]);
	}
	printf("}");

	MEM_freeN(texels);
	MEM_freeN(data);

	return tex;
}
#endif

static char *eevee_get_defines(int options)
{
	char *str = NULL;

	BLI_assert(options < VAR_MAT_MAX);

	DynStr *ds = BLI_dynstr_new();
	BLI_dynstr_appendf(ds, SHADER_DEFINES);

	if ((options & VAR_MAT_MESH) != 0) {
		BLI_dynstr_appendf(ds, "#define MESH_SHADER\n");
	}
	if ((options & VAR_MAT_HAIR) != 0) {
		BLI_dynstr_appendf(ds, "#define HAIR_SHADER\n");
	}
	if ((options & VAR_MAT_PROBE) != 0) {
		BLI_dynstr_appendf(ds, "#define PROBE_CAPTURE\n");
	}
	if ((options & VAR_MAT_AO) != 0) {
		BLI_dynstr_appendf(ds, "#define USE_AO\n");
	}
	if ((options & VAR_MAT_FLAT) != 0) {
		BLI_dynstr_appendf(ds, "#define USE_FLAT_NORMAL\n");
	}
	if ((options & VAR_MAT_BENT) != 0) {
		BLI_dynstr_appendf(ds, "#define USE_BENT_NORMAL\n");
	}

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	return str;
}

static void add_standard_uniforms(DRWShadingGroup *shgrp, EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	DRW_shgroup_uniform_block(shgrp, "probe_block", sldata->probe_ubo);
	DRW_shgroup_uniform_block(shgrp, "grid_block", sldata->grid_ubo);
	DRW_shgroup_uniform_block(shgrp, "planar_block", sldata->planar_ubo);
	DRW_shgroup_uniform_block(shgrp, "light_block", sldata->light_ubo);
	DRW_shgroup_uniform_block(shgrp, "shadow_block", sldata->shadow_ubo);
	DRW_shgroup_uniform_int(shgrp, "light_count", &sldata->lamps->num_light, 1);
	DRW_shgroup_uniform_int(shgrp, "probe_count", &sldata->probes->num_render_cube, 1);
	DRW_shgroup_uniform_int(shgrp, "grid_count", &sldata->probes->num_render_grid, 1);
	DRW_shgroup_uniform_int(shgrp, "planar_count", &sldata->probes->num_planar, 1);
	DRW_shgroup_uniform_bool(shgrp, "specToggle", &sldata->probes->specular_toggle, 1);
	DRW_shgroup_uniform_float(shgrp, "lodMax", &sldata->probes->lodmax, 1);
	DRW_shgroup_uniform_texture(shgrp, "utilTex", e_data.util_tex);
	DRW_shgroup_uniform_buffer(shgrp, "probeCubes", &sldata->probe_pool);
	DRW_shgroup_uniform_buffer(shgrp, "probePlanars", &vedata->txl->planar_pool);
	DRW_shgroup_uniform_buffer(shgrp, "irradianceGrid", &sldata->irradiance_pool);
	DRW_shgroup_uniform_buffer(shgrp, "shadowCubes", &sldata->shadow_depth_cube_pool);
	DRW_shgroup_uniform_buffer(shgrp, "shadowCascades", &sldata->shadow_depth_cascade_pool);
	if (vedata->stl->effects->use_ao) {
		DRW_shgroup_uniform_vec4(shgrp, "viewvecs[0]", (float *)e_data.viewvecs, 2);
		DRW_shgroup_uniform_buffer(shgrp, "minMaxDepthTex", &vedata->stl->g_data->minmaxz);
		DRW_shgroup_uniform_vec3(shgrp, "aoParameters", &vedata->stl->effects->ao_dist, 1);
	}
}

static void create_default_shader(int options)
{
	DynStr *ds_frag = BLI_dynstr_new();
	BLI_dynstr_append(ds_frag, e_data.frag_shader_lib);
	BLI_dynstr_append(ds_frag, datatoc_default_frag_glsl);
	char *frag_str = BLI_dynstr_get_cstring(ds_frag);
	BLI_dynstr_free(ds_frag);

	char *defines = eevee_get_defines(options);

	e_data.default_lit[options] = DRW_shader_create(datatoc_lit_surface_vert_glsl, NULL, frag_str, defines);

	MEM_freeN(defines);
	MEM_freeN(frag_str);
}

void EEVEE_materials_init(void)
{
	if (!e_data.frag_shader_lib) {
		char *frag_str = NULL;

		/* Shaders */
		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_ambient_occlusion_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_octahedron_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_irradiance_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_ltc_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_bsdf_direct_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lit_surface_frag_glsl);
		e_data.frag_shader_lib = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, e_data.frag_shader_lib);
		BLI_dynstr_append(ds_frag, datatoc_default_frag_glsl);
		frag_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.default_background = DRW_shader_create_fullscreen(
		        datatoc_default_world_frag_glsl, NULL);

		e_data.default_prepass_sh = DRW_shader_create(
		        datatoc_prepass_vert_glsl, NULL, datatoc_prepass_frag_glsl,
		        NULL);

		e_data.default_prepass_clip_sh = DRW_shader_create(
		        datatoc_prepass_vert_glsl, NULL, datatoc_prepass_frag_glsl,
		        "#define CLIP_PLANES\n");

		MEM_freeN(frag_str);

		/* Textures */
		const int layers = 3;
		float (*texels)[4] = MEM_mallocN(sizeof(float[4]) * 64 * 64 * layers, "utils texels");
		float (*texels_layer)[4] = texels;

		/* Copy ltc_mat_ggx into 1st layer */
		memcpy(texels_layer, ltc_mat_ggx, sizeof(float[4]) * 64 * 64);
		texels_layer += 64 * 64;

		/* Copy bsdf_split_sum_ggx into 2nd layer red and green channels.
		   Copy ltc_mag_ggx into 2nd layer blue channel. */
		for (int i = 0; i < 64 * 64; i++) {
			texels_layer[i][0] = bsdf_split_sum_ggx[i*2 + 0];
			texels_layer[i][1] = bsdf_split_sum_ggx[i*2 + 1];
			texels_layer[i][2] = ltc_mag_ggx[i];
		}
		texels_layer += 64 * 64;

		for (int i = 0; i < 64 * 64; i++) {
			texels_layer[i][0] = blue_noise[i][0];
			texels_layer[i][1] = blue_noise[i][1] * 0.5 + 0.5;
			texels_layer[i][2] = blue_noise[i][2];
			texels_layer[i][3] = blue_noise[i][3];
		}

		e_data.util_tex = DRW_texture_create_2D_array(64, 64, layers, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_WRAP, (float *)texels);
		MEM_freeN(texels);
	}

	{
		/* Update viewvecs */
		const bool is_persp = DRW_viewport_is_persp_get();
		float invproj[4][4], winmat[4][4];
		/* view vectors for the corners of the view frustum.
		 * Can be used to recreate the world space position easily */
		float viewvecs[3][4] = {
		    {-1.0f, -1.0f, -1.0f, 1.0f},
		    {1.0f, -1.0f, -1.0f, 1.0f},
		    {-1.0f, 1.0f, -1.0f, 1.0f}
		};

		/* invert the view matrix */
		DRW_viewport_matrix_get(winmat, DRW_MAT_WIN);
		invert_m4_m4(invproj, winmat);

		/* convert the view vectors to view space */
		for (int i = 0; i < 3; i++) {
			mul_m4_v4(invproj, viewvecs[i]);
			/* normalized trick see:
			 * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
			mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
			if (is_persp)
				mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
			viewvecs[i][3] = 1.0;
		}

		copy_v4_v4(e_data.viewvecs[0], viewvecs[0]);
		copy_v4_v4(e_data.viewvecs[1], viewvecs[1]);

		/* we need to store the differences */
		e_data.viewvecs[1][0] -= viewvecs[0][0];
		e_data.viewvecs[1][1] = viewvecs[2][1] - viewvecs[0][1];

		/* calculate a depth offset as well */
		if (!is_persp) {
			float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
			mul_m4_v4(invproj, vec_far);
			mul_v3_fl(vec_far, 1.0f / vec_far[3]);
			e_data.viewvecs[1][2] = vec_far[2] - viewvecs[0][2];
		}
	}
}

struct GPUMaterial *EEVEE_material_world_lightprobe_get(struct Scene *scene, World *wo)
{
	const void *engine = &DRW_engine_viewport_eevee_type;
	const int options = VAR_WORLD_PROBE;

	GPUMaterial *mat = GPU_material_from_nodetree_find(&wo->gpumaterial, engine, options);
	if (mat != NULL) {
		return mat;
	}
	return GPU_material_from_nodetree(
	        scene, wo->nodetree, &wo->gpumaterial, engine, options,
	        datatoc_lightprobe_vert_glsl, datatoc_lightprobe_geom_glsl, e_data.frag_shader_lib,
	        SHADER_DEFINES "#define PROBE_CAPTURE\n");
}

struct GPUMaterial *EEVEE_material_world_background_get(struct Scene *scene, World *wo)
{
	const void *engine = &DRW_engine_viewport_eevee_type;
	int options = VAR_WORLD_BACKGROUND;

	GPUMaterial *mat = GPU_material_from_nodetree_find(&wo->gpumaterial, engine, options);
	if (mat != NULL) {
		return mat;
	}
	return GPU_material_from_nodetree(
	        scene, wo->nodetree, &wo->gpumaterial, engine, options,
	        datatoc_background_vert_glsl, NULL, e_data.frag_shader_lib,
	        SHADER_DEFINES "#define WORLD_BACKGROUND\n");
}

struct GPUMaterial *EEVEE_material_mesh_get(
        struct Scene *scene, Material *ma,
        bool use_ao, bool use_bent_normals)
{
	const void *engine = &DRW_engine_viewport_eevee_type;
	int options = VAR_MAT_MESH;

	if (use_ao) options |= VAR_MAT_AO;
	if (use_bent_normals) options |= VAR_MAT_BENT;

	GPUMaterial *mat = GPU_material_from_nodetree_find(&ma->gpumaterial, engine, options);
	if (mat) {
		return mat;
	}

	char *defines = eevee_get_defines(options);

	mat = GPU_material_from_nodetree(
	        scene, ma->nodetree, &ma->gpumaterial, engine, options,
	        datatoc_lit_surface_vert_glsl, NULL, e_data.frag_shader_lib,
	        defines);

	MEM_freeN(defines);

	return mat;
}

struct GPUMaterial *EEVEE_material_hair_get(
        struct Scene *scene, Material *ma,
        bool use_ao, bool use_bent_normals)
{
	const void *engine = &DRW_engine_viewport_eevee_type;
	int options = VAR_MAT_MESH | VAR_MAT_HAIR;

	if (use_ao) options |= VAR_MAT_AO;
	if (use_bent_normals) options |= VAR_MAT_BENT;

	GPUMaterial *mat = GPU_material_from_nodetree_find(&ma->gpumaterial, engine, options);
	if (mat) {
		return mat;
	}

	char *defines = eevee_get_defines(options);

	mat = GPU_material_from_nodetree(
	        scene, ma->nodetree, &ma->gpumaterial, engine, options,
	        datatoc_lit_surface_vert_glsl, NULL, e_data.frag_shader_lib,
	        defines);

	MEM_freeN(defines);

	return mat;
}

static struct DRWShadingGroup *EEVEE_default_shading_group_get(
        EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata,
        bool is_hair, bool is_flat_normal, bool use_ao, bool use_bent_normals)
{
	int options = VAR_MAT_MESH;

	if (is_hair) options |= VAR_MAT_HAIR;
	if (use_ao) options |= VAR_MAT_AO;
	if (use_bent_normals) options |= VAR_MAT_BENT;
	if (is_flat_normal) options |= VAR_MAT_FLAT;

	if (e_data.default_lit[options] == NULL) {
		create_default_shader(options);
	}

	if (vedata->psl->default_pass[options] == NULL) {
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES | DRW_STATE_WIRE;
		vedata->psl->default_pass[options] = DRW_pass_create("Default Lit Pass", state);

		DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.default_lit[options], vedata->psl->default_pass[options]);
		add_standard_uniforms(shgrp, sldata, vedata);
	}

	return DRW_shgroup_create(e_data.default_lit[options], vedata->psl->default_pass[options]);
}

void EEVEE_materials_cache_init(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	{
		/* Global AO Switch*/
		const DRWContextState *draw_ctx = DRW_context_state_get();
		SceneLayer *scene_layer = draw_ctx->sl;
		IDProperty *props = BKE_scene_layer_engine_evaluated_get(scene_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);
		stl->effects->use_ao = BKE_collection_engine_property_value_get_bool(props, "gtao_enable");
		stl->effects->use_bent_normals = BKE_collection_engine_property_value_get_bool(props, "gtao_use_bent_normals");
	}

	/* Create Material Ghash */
	{
		stl->g_data->material_hash = BLI_ghash_ptr_new("Eevee_material ghash");
		stl->g_data->hair_material_hash = BLI_ghash_ptr_new("Eevee_hair_material ghash");
	}

	{
		psl->background_pass = DRW_pass_create("Background Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
		DRWShadingGroup *grp = NULL;

		const DRWContextState *draw_ctx = DRW_context_state_get();
		Scene *scene = draw_ctx->scene;
		World *wo = scene->world;

		float *col = ts.colorBackground;

		if (wo) {
			col = &wo->horr;

			if (wo->use_nodes && wo->nodetree) {
				struct GPUMaterial *gpumat = EEVEE_material_world_background_get(scene, wo);
				grp = DRW_shgroup_material_create(gpumat, psl->background_pass);

				if (grp) {
					DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
					DRW_shgroup_call_add(grp, geom, NULL);
				}
				else {
					/* Shader failed : pink background */
					static float pink[3] = {1.0f, 0.0f, 1.0f};
					col = pink;
				}
			}
		}

		/* Fallback if shader fails or if not using nodetree. */
		if (grp == NULL) {
			grp = DRW_shgroup_create(e_data.default_background, psl->background_pass);
			DRW_shgroup_uniform_vec3(grp, "color", col, 1);
			DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
			DRW_shgroup_call_add(grp, geom, NULL);
		}
	}

	{
		DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_WIRE;
		psl->depth_pass = DRW_pass_create("Depth Pass", state);
		stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.default_prepass_sh, psl->depth_pass);

		state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK;
		psl->depth_pass_cull = DRW_pass_create("Depth Pass Cull", state);
		stl->g_data->depth_shgrp_cull = DRW_shgroup_create(e_data.default_prepass_sh, psl->depth_pass_cull);

		state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CLIP_PLANES | DRW_STATE_WIRE;
		psl->depth_pass_clip = DRW_pass_create("Depth Pass Clip", state);
		stl->g_data->depth_shgrp_clip = DRW_shgroup_create(e_data.default_prepass_clip_sh, psl->depth_pass_clip);

		state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CLIP_PLANES | DRW_STATE_CULL_BACK;
		psl->depth_pass_clip_cull = DRW_pass_create("Depth Pass Cull Clip", state);
		stl->g_data->depth_shgrp_clip_cull = DRW_shgroup_create(e_data.default_prepass_clip_sh, psl->depth_pass_clip_cull);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES | DRW_STATE_WIRE;
		psl->material_pass = DRW_pass_create("Material Shader Pass", state);
	}
}

#define ADD_SHGROUP_CALL(shgrp, ob, geom) do { \
	if (is_sculpt_mode) { \
		DRW_shgroup_call_sculpt_add(shgrp, ob, ob->obmat); \
	} \
	else { \
		DRW_shgroup_call_object_add(shgrp, geom, ob); \
	} \
} while (0)

void EEVEE_materials_cache_populate(EEVEE_Data *vedata, EEVEE_SceneLayerData *sldata, Object *ob, struct Gwn_Batch *geom)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	GHash *material_hash = stl->g_data->material_hash;

	IDProperty *ces_mode_ob = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_OBJECT, "");
	const bool do_cull = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_backface_culling");
	const bool is_active = (ob == draw_ctx->obact);
	const bool is_sculpt_mode = is_active && (ob->mode & OB_MODE_SCULPT) != 0;
	const bool is_default_mode_shader = is_sculpt_mode;

	/* Depth Prepass */
	DRWShadingGroup *depth_shgrp = do_cull ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp;
	DRWShadingGroup *depth_clip_shgrp = do_cull ? stl->g_data->depth_shgrp_clip_cull : stl->g_data->depth_shgrp_clip;
	ADD_SHGROUP_CALL(depth_shgrp, ob, geom);
	ADD_SHGROUP_CALL(depth_clip_shgrp, ob, geom);

	/* First get materials for this mesh. */
	if (ELEM(ob->type, OB_MESH)) {
		const int materials_len = MAX2(1, (is_sculpt_mode ? 1 : ob->totcol));
		struct DRWShadingGroup **shgrp_array = BLI_array_alloca(shgrp_array, materials_len);
		struct GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);

		bool use_flat_nor = false;

		if (is_default_mode_shader) {
			if (is_sculpt_mode) {
				use_flat_nor = DRW_object_is_flat_normal(ob);
			}
		}

		for (int i = 0; i < materials_len; ++i) {
			DRWShadingGroup *shgrp = NULL;
			Material *ma = give_current_material(ob, i + 1);

			if (ma == NULL)
				ma = &defmaterial;

			float *color_p = &ma->r;
			float *metal_p = &ma->ray_mirror;
			float *spec_p = &ma->spec;
			float *rough_p = &ma->gloss_mir;

			const bool use_gpumat = (ma->use_nodes && ma->nodetree);

			shgrp = BLI_ghash_lookup(material_hash, (const void *)ma);
			if (shgrp) {
				shgrp_array[i] = shgrp;  /* ADD_SHGROUP_CALL below */
				/* This will have been created already, just perform a lookup. */
				gpumat_array[i] = (use_gpumat) ? EEVEE_material_mesh_get(
				        draw_ctx->scene, ma,stl->effects->use_ao, stl->effects->use_bent_normals) : NULL;
				continue;
			}

			/* May not be set below. */
			gpumat_array[i] = NULL;

			if (use_gpumat) {
				Scene *scene = draw_ctx->scene;
				struct GPUMaterial *gpumat = EEVEE_material_mesh_get(scene, ma,
				        stl->effects->use_ao, stl->effects->use_bent_normals);

				shgrp = DRW_shgroup_material_create(gpumat, psl->material_pass);
				if (shgrp) {
					add_standard_uniforms(shgrp, sldata, vedata);

					BLI_ghash_insert(material_hash, ma, shgrp);
					shgrp_array[i] = shgrp;  /* ADD_SHGROUP_CALL below */

					gpumat_array[i] = gpumat;
				}
				else {
					/* Shader failed : pink color */
					static float col[3] = {1.0f, 0.0f, 1.0f};
					static float half = 0.5f;

					color_p = col;
					metal_p = spec_p = rough_p = &half;
				}
			}

			/* Fallback to default shader */
			if (shgrp == NULL) {
				shgrp = EEVEE_default_shading_group_get(sldata, vedata, false, use_flat_nor,
				        stl->effects->use_ao, stl->effects->use_bent_normals);
				DRW_shgroup_uniform_vec3(shgrp, "basecol", color_p, 1);
				DRW_shgroup_uniform_float(shgrp, "metallic", metal_p, 1);
				DRW_shgroup_uniform_float(shgrp, "specular", spec_p, 1);
				DRW_shgroup_uniform_float(shgrp, "roughness", rough_p, 1);

				BLI_ghash_insert(material_hash, ma, shgrp);

				shgrp_array[i] = shgrp;  /* ADD_SHGROUP_CALL below */
			}
		}

		/* Get per-material split surface */
		struct Gwn_Batch **mat_geom = DRW_cache_object_surface_material_get(ob, gpumat_array, materials_len);
		if (mat_geom) {
			for (int i = 0; i < materials_len; ++i) {
				ADD_SHGROUP_CALL(shgrp_array[i], ob, mat_geom[i]);
			}
		}
	}

	if (ob->type == OB_MESH) {
		if (ob != draw_ctx->scene->obedit) {
			material_hash = stl->g_data->hair_material_hash;

			for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
				if (md->type == eModifierType_ParticleSystem) {
					ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;

					if (psys_check_enabled(ob, psys, false)) {
						ParticleSettings *part = psys->part;
						int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

						if (draw_as == PART_DRAW_PATH && (psys->pathcache || psys->childcache)) {
							struct Gwn_Batch *hair_geom = DRW_cache_particles_get_hair(psys, md);
							DRWShadingGroup *shgrp = NULL;
							Material *ma = give_current_material(ob, part->omat);
							static float mat[4][4];

							unit_m4(mat);

							if (ma == NULL) {
								ma = &defmaterial;
							}

							float *color_p = &ma->r;
							float *metal_p = &ma->ray_mirror;
							float *spec_p = &ma->spec;
							float *rough_p = &ma->gloss_mir;

							DRW_shgroup_call_add(stl->g_data->depth_shgrp, hair_geom, mat);
							DRW_shgroup_call_add(stl->g_data->depth_shgrp_clip, hair_geom, mat);

							shgrp = BLI_ghash_lookup(material_hash, (const void *)ma);

							if (shgrp) {
								DRW_shgroup_call_add(shgrp, hair_geom, mat);
							}
							else {
								if (ma->use_nodes && ma->nodetree) {
									Scene *scene = draw_ctx->scene;
									struct GPUMaterial *gpumat = EEVEE_material_hair_get(scene, ma,
									        stl->effects->use_ao, stl->effects->use_bent_normals);

									shgrp = DRW_shgroup_material_create(gpumat, psl->material_pass);
									if (shgrp) {
										add_standard_uniforms(shgrp, sldata, vedata);

										BLI_ghash_insert(material_hash, ma, shgrp);

										DRW_shgroup_call_add(shgrp, hair_geom, mat);
									}
									else {
										/* Shader failed : pink color */
										static float col[3] = {1.0f, 0.0f, 1.0f};
										static float half = 0.5f;

										color_p = col;
										metal_p = spec_p = rough_p = &half;
									}
								}

								/* Fallback to default shader */
								if (shgrp == NULL) {
									shgrp = EEVEE_default_shading_group_get(sldata, vedata, true, false,
									        stl->effects->use_ao, stl->effects->use_bent_normals);
									DRW_shgroup_uniform_vec3(shgrp, "basecol", color_p, 1);
									DRW_shgroup_uniform_float(shgrp, "metallic", metal_p, 1);
									DRW_shgroup_uniform_float(shgrp, "specular", spec_p, 1);
									DRW_shgroup_uniform_float(shgrp, "roughness", rough_p, 1);

									BLI_ghash_insert(material_hash, ma, shgrp);

									DRW_shgroup_call_add(shgrp, hair_geom, mat);
								}
							}
						}
					}
				}
			}
		}
	}
}

void EEVEE_materials_cache_finish(EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	BLI_ghash_free(stl->g_data->material_hash, NULL, NULL);
	BLI_ghash_free(stl->g_data->hair_material_hash, NULL, NULL);
}

void EEVEE_materials_free(void)
{
	for (int i = 0; i < VAR_MAT_MAX; ++i) {
		DRW_SHADER_FREE_SAFE(e_data.default_lit[i]);
	}
	MEM_SAFE_FREE(e_data.frag_shader_lib);
	DRW_SHADER_FREE_SAFE(e_data.default_prepass_sh);
	DRW_SHADER_FREE_SAFE(e_data.default_prepass_clip_sh);
	DRW_SHADER_FREE_SAFE(e_data.default_background);
	DRW_TEXTURE_FREE_SAFE(e_data.util_tex);
}

void EEVEE_draw_default_passes(EEVEE_PassList *psl)
{
	for (int i = 0; i < VAR_MAT_MAX; ++i) {
		if (psl->default_pass[i]) {
			DRW_draw_pass(psl->default_pass[i]);
		}
	}
}
