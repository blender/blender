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

#include "BLI_dynstr.h"

#include "GPU_material.h"

#include "eevee_engine.h"
#include "eevee_lut.h"
#include "eevee_private.h"

#define SHADER_DEFINES \
	"#define EEVEE_ENGINE\n" \
	"#define MAX_LIGHT " STRINGIFY(MAX_LIGHT) "\n" \
	"#define MAX_SHADOW_CUBE " STRINGIFY(MAX_SHADOW_CUBE) "\n" \
	"#define MAX_SHADOW_MAP " STRINGIFY(MAX_SHADOW_MAP) "\n" \
	"#define MAX_SHADOW_CASCADE " STRINGIFY(MAX_SHADOW_CASCADE) "\n" \
	"#define MAX_CASCADE_NUM " STRINGIFY(MAX_CASCADE_NUM) "\n"

/* World shader variations */
enum {
	VAR_WORLD_BACKGROUND,
	VAR_WORLD_PROBE,
};

/* Material shader variations */
enum {
	VAR_MAT_MESH     = (1 << 0),
	VAR_MAT_PROBE    = (1 << 1),
};

/* *********** STATIC *********** */
static struct {
	char *frag_shader_lib;

	struct GPUShader *default_lit;
	struct GPUShader *default_lit_flat;

	struct GPUShader *default_background;

	/* 64*64 array texture containing all LUTs and other utilitarian arrays.
	 * Packing enables us to same precious textures slots. */
	struct GPUTexture *util_tex;
} e_data = {NULL}; /* Engine data */

extern char datatoc_default_frag_glsl[];
extern char datatoc_default_world_frag_glsl[];
extern char datatoc_ltc_lib_glsl[];
extern char datatoc_bsdf_lut_frag_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_direct_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_lit_surface_frag_glsl[];
extern char datatoc_lit_surface_vert_glsl[];
extern char datatoc_shadow_frag_glsl[];
extern char datatoc_shadow_geom_glsl[];
extern char datatoc_shadow_vert_glsl[];
extern char datatoc_probe_filter_frag_glsl[];
extern char datatoc_probe_sh_frag_glsl[];
extern char datatoc_probe_geom_glsl[];
extern char datatoc_probe_vert_glsl[];
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
	        datatoc_probe_vert_glsl, datatoc_probe_geom_glsl, datatoc_bsdf_lut_frag_glsl, lib_str,
	        "#define HAMMERSLEY_SIZE 8192\n"
	        "#define BRDF_LUT_SIZE 64\n"
	        "#define NOISE_SIZE 64\n");

	DRWPass *pass = DRW_pass_create("Probe Filtering", DRW_STATE_WRITE_COLOR);
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_float(grp, "sampleCount", &samples_ct, 1);
	DRW_shgroup_uniform_float(grp, "invSampleCount", &inv_samples_ct, 1);
	DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
	DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);

	struct Batch *geom = DRW_cache_fullscreen_quad_get();
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

void EEVEE_materials_init(void)
{
	if (!e_data.frag_shader_lib) {
		char *frag_str = NULL;

		/* Shaders */
		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
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

		e_data.default_lit = DRW_shader_create(
		        datatoc_lit_surface_vert_glsl, NULL, frag_str,
		        SHADER_DEFINES
		        "#define MESH_SHADER\n");
		e_data.default_lit_flat = DRW_shader_create(
		        datatoc_lit_surface_vert_glsl, NULL, frag_str,
		        SHADER_DEFINES
		        "#define MESH_SHADER\n"
		        "#define USE_FLAT_NORMAL\n");

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

		/* Copy ltc_mag_ggx into 2nd layer blue channel */
		for (int i = 0; i < 64 * 64; i++) {
			texels_layer[i][0] = blue_noise[i*3 + 0];
			texels_layer[i][1] = blue_noise[i*3 + 1];
			texels_layer[i][2] = blue_noise[i*3 + 2];
		}

		e_data.util_tex = DRW_texture_create_2D_array(64, 64, layers, DRW_TEX_RGBA_16, DRW_TEX_FILTER, (float *)texels);
		MEM_freeN(texels);
	}
}

struct GPUMaterial *EEVEE_material_world_probe_get(struct Scene *scene, World *wo)
{
	return GPU_material_from_nodetree(
	    scene, wo->nodetree, &wo->gpumaterial, &DRW_engine_viewport_eevee_type,
	    VAR_WORLD_PROBE,
	    datatoc_probe_vert_glsl, datatoc_probe_geom_glsl, e_data.frag_shader_lib,
	    SHADER_DEFINES "#define PROBE_CAPTURE\n");
}

struct GPUMaterial *EEVEE_material_world_background_get(struct Scene *scene, World *wo)
{
	return GPU_material_from_nodetree(
	    scene, wo->nodetree, &wo->gpumaterial, &DRW_engine_viewport_eevee_type,
	    VAR_WORLD_BACKGROUND,
	    datatoc_background_vert_glsl, NULL, e_data.frag_shader_lib,
	    SHADER_DEFINES "#define WORLD_BACKGROUND\n");
}

struct GPUMaterial *EEVEE_material_mesh_probe_get(struct Scene *scene, Material *ma)
{
	return GPU_material_from_nodetree(
	    scene, ma->nodetree, &ma->gpumaterial, &DRW_engine_viewport_eevee_type,
	    VAR_MAT_MESH | VAR_MAT_PROBE,
	    datatoc_probe_vert_glsl, datatoc_probe_geom_glsl, e_data.frag_shader_lib,
	    SHADER_DEFINES "#define MESH_SHADER\n" "#define PROBE_CAPTURE\n");
}

struct GPUMaterial *EEVEE_material_mesh_get(struct Scene *scene, Material *ma)
{
	return GPU_material_from_nodetree(
	    scene, ma->nodetree, &ma->gpumaterial, &DRW_engine_viewport_eevee_type,
	    VAR_MAT_MESH,
	    datatoc_lit_surface_vert_glsl, NULL, e_data.frag_shader_lib,
	    SHADER_DEFINES "#define MESH_SHADER\n");
}

static void add_standard_uniforms(DRWShadingGroup *shgrp, EEVEE_SceneLayerData *sldata)
{
	DRW_shgroup_uniform_block(shgrp, "light_block", sldata->light_ubo);
	DRW_shgroup_uniform_block(shgrp, "shadow_block", sldata->shadow_ubo);
	DRW_shgroup_uniform_int(shgrp, "light_count", &sldata->lamps->num_light, 1);
	DRW_shgroup_uniform_texture(shgrp, "utilTex", e_data.util_tex);
	DRW_shgroup_uniform_buffer(shgrp, "probeCubes", &sldata->probe_pool);
	DRW_shgroup_uniform_float(shgrp, "lodMax", &sldata->probes->lodmax, 1);
	DRW_shgroup_uniform_buffer(shgrp, "shadowCubes", &sldata->shadow_depth_cube_pool);
	DRW_shgroup_uniform_buffer(shgrp, "shadowCascades", &sldata->shadow_depth_cascade_pool);
}

void EEVEE_materials_cache_init(EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	EEVEE_SceneLayerData *sldata = EEVEE_scene_layer_data_get();

	{
		psl->background_pass = DRW_pass_create("Background Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();
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
			DRW_shgroup_call_add(grp, geom, NULL);
		}
	}

	{
		struct GPUShader *depth_sh = DRW_shader_create_3D_depth_only();
		DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->depth_pass = DRW_pass_create("Depth Pass", state);
		stl->g_data->depth_shgrp = DRW_shgroup_create(depth_sh, psl->depth_pass);

		state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK;
		psl->depth_pass_cull = DRW_pass_create("Depth Pass Cull", state);
		stl->g_data->depth_shgrp_cull = DRW_shgroup_create(depth_sh, psl->depth_pass_cull);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
		psl->default_pass = DRW_pass_create("Default Lit Pass", state);
		DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.default_lit, psl->default_pass);
		add_standard_uniforms(shgrp, sldata);

		psl->default_flat_pass = DRW_pass_create("Default Flat Lit Pass", state);
		shgrp = DRW_shgroup_create(e_data.default_lit_flat, psl->default_flat_pass);
		add_standard_uniforms(shgrp, sldata);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
		psl->material_pass = DRW_pass_create("Material Shader Pass", state);
	}
}

/* Macro to call the right  */
#define ADD_MATERIAL_CALL(shgrp, ob, geom) do { \
	if (is_sculpt_mode) { \
		DRW_shgroup_call_sculpt_add(shgrp, ob, ob->obmat); \
	} \
	else { \
		DRW_shgroup_call_object_add(shgrp, geom, ob); \
	} \
} while (0)

void EEVEE_materials_cache_populate(EEVEE_Data *vedata, EEVEE_SceneLayerData *sldata, Object *ob, struct Batch *geom)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();

	IDProperty *ces_mode_ob = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_OBJECT, "");
	const bool do_cull = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_backface_culling");
	const bool is_active = (ob == draw_ctx->obact);
	const bool is_sculpt_mode = is_active && (ob->mode & OB_MODE_SCULPT) != 0;
	const bool is_default_mode_shader = is_sculpt_mode;

	/* Depth Prepass */
	DRWShadingGroup *depth_shgrp = do_cull ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp;
	ADD_MATERIAL_CALL(depth_shgrp, ob, geom);

	/* Get per-material split surface */
	struct Batch **mat_geom = DRW_cache_object_surface_material_get(ob);
	if (mat_geom) {
		struct GPUShader *default_shader = e_data.default_lit;
		struct DRWPass *default_pass = psl->default_pass;

		if (is_default_mode_shader) {
			if (is_sculpt_mode) {
				bool use_flat = DRW_object_is_flat_normal(ob);
				default_shader = use_flat ? e_data.default_lit_flat : e_data.default_lit;
			}
		}

		for (int i = 0; i < MAX2(1, (is_sculpt_mode ? 1 : ob->totcol)); ++i) {
			DRWShadingGroup *shgrp = NULL;
			Material *ma = give_current_material(ob, i + 1);

			if (ma == NULL)
				ma = &defmaterial;

			float *color_p = &ma->r;
			float *metal_p = &ma->ray_mirror;
			float *spec_p = &ma->spec;
			float *rough_p = &ma->gloss_mir;

			if (ma->use_nodes && ma->nodetree) {
				Scene *scene = draw_ctx->scene;
				struct GPUMaterial *gpumat = EEVEE_material_mesh_get(scene, ma);

				shgrp = DRW_shgroup_material_create(gpumat, psl->material_pass);
				if (shgrp) {
					add_standard_uniforms(shgrp, sldata);

					ADD_MATERIAL_CALL(shgrp, ob, mat_geom[i]);
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
				shgrp = DRW_shgroup_create(default_shader, default_pass);
				DRW_shgroup_uniform_vec3(shgrp, "basecol", color_p, 1);
				DRW_shgroup_uniform_float(shgrp, "metallic", metal_p, 1);
				DRW_shgroup_uniform_float(shgrp, "specular", spec_p, 1);
				DRW_shgroup_uniform_float(shgrp, "roughness", rough_p, 1);

				ADD_MATERIAL_CALL(shgrp, ob, mat_geom[i]);
			}
		}
	}
}

void EEVEE_materials_free(void)
{
	MEM_SAFE_FREE(e_data.frag_shader_lib);
	DRW_SHADER_FREE_SAFE(e_data.default_lit);
	DRW_SHADER_FREE_SAFE(e_data.default_lit_flat);
	DRW_SHADER_FREE_SAFE(e_data.default_background);
	DRW_TEXTURE_FREE_SAFE(e_data.util_tex);
}