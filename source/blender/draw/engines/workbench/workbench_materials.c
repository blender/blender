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

/** \file workbench_materials.c
 *  \ingroup draw_engine
 */

#include "workbench_private.h"

#include "BLI_alloca.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BKE_particle.h"

#include "DNA_modifier_types.h"

#include "GPU_shader.h"

#include "UI_resources.h"

/* *********** STATIC *********** */

// #define DEBUG_SHADOW_VOLUME
#define MAX_SHADERS 255

static struct {
	struct GPUShader *prepass_sh_cache[MAX_SHADERS];
	struct GPUShader *composite_sh_cache[MAX_SHADERS];
	struct GPUShader *shadow_sh;

	struct GPUTexture *object_id_tx; /* ref only, not alloced */
	struct GPUTexture *color_buffer_tx; /* ref only, not alloced */
	struct GPUTexture *normal_buffer_tx; /* ref only, not alloced */

	float light_direction[3]; /* world light direction for shadows */
	int next_object_id;
} e_data = {NULL};

/* Shaders */
extern char datatoc_workbench_prepass_vert_glsl[];
extern char datatoc_workbench_prepass_frag_glsl[];
extern char datatoc_workbench_composite_frag_glsl[];

extern char datatoc_workbench_shadow_vert_glsl[];
extern char datatoc_workbench_shadow_geom_glsl[];

extern char datatoc_workbench_background_lib_glsl[];
extern char datatoc_workbench_common_lib_glsl[];
extern char datatoc_workbench_data_lib_glsl[];
extern char datatoc_workbench_object_overlap_lib_glsl[];
extern char datatoc_workbench_world_light_lib_glsl[];

extern DrawEngineType draw_engine_workbench_solid;

#define OBJECT_ID_PASS_ENABLED(wpd) (wpd->drawtype_options & V3D_DRAWOPTION_OBJECT_OVERLAP)
#define NORMAL_VIEWPORT_PASS_ENABLED(wpd) (wpd->drawtype_lighting & V3D_LIGHTING_STUDIO)
#define SHADOW_ENABLED(wpd) (wpd->drawtype_options & V3D_DRAWOPTION_SHADOW)
static char *workbench_build_defines(WORKBENCH_PrivateData *wpd)
{
	char *str = NULL;

	DynStr *ds = BLI_dynstr_new();

	if (wpd->drawtype_options & V3D_DRAWOPTION_OBJECT_OVERLAP) {
		BLI_dynstr_appendf(ds, "#define V3D_DRAWOPTION_OBJECT_OVERLAP\n");
	}
	if (wpd->drawtype_lighting & V3D_LIGHTING_STUDIO) {
		BLI_dynstr_appendf(ds, "#define V3D_LIGHTING_STUDIO\n");
	}

#ifdef WORKBENCH_ENCODE_NORMALS
	BLI_dynstr_appendf(ds, "#define WORKBENCH_ENCODE_NORMALS\n");
#endif

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return str;
}

static char *workbench_build_composite_frag(WORKBENCH_PrivateData *wpd)
{
	char *str = NULL;

	DynStr *ds = BLI_dynstr_new();

	BLI_dynstr_append(ds, datatoc_workbench_data_lib_glsl);
	BLI_dynstr_append(ds, datatoc_workbench_common_lib_glsl);
	BLI_dynstr_append(ds, datatoc_workbench_background_lib_glsl);

	if (wpd->drawtype_lighting & V3D_LIGHTING_STUDIO) {
		BLI_dynstr_append(ds, datatoc_workbench_world_light_lib_glsl);
	}
	if (wpd->drawtype_options & V3D_DRAWOPTION_OBJECT_OVERLAP) {
		BLI_dynstr_append(ds, datatoc_workbench_object_overlap_lib_glsl);
	}

	BLI_dynstr_append(ds, datatoc_workbench_composite_frag_glsl);

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return str;
}

static char *workbench_build_prepass_frag(void)
{
	char *str = NULL;

	DynStr *ds = BLI_dynstr_new();

	BLI_dynstr_append(ds, datatoc_workbench_common_lib_glsl);
	BLI_dynstr_append(ds, datatoc_workbench_prepass_frag_glsl);

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return str;
}

static int get_shader_index(WORKBENCH_PrivateData *wpd)
{
	const int DRAWOPTIONS_MASK = V3D_DRAWOPTION_OBJECT_OVERLAP;
	return ((wpd->drawtype_options & DRAWOPTIONS_MASK) << 2) + wpd->drawtype_lighting;
}

static void select_deferred_shaders(WORKBENCH_PrivateData *wpd)
{
	int index = get_shader_index(wpd);

	if (e_data.prepass_sh_cache[index] == NULL) {
		char *defines = workbench_build_defines(wpd);
		char *composite_frag = workbench_build_composite_frag(wpd);
		char *prepass_frag = workbench_build_prepass_frag();
		e_data.prepass_sh_cache[index] = DRW_shader_create(datatoc_workbench_prepass_vert_glsl, NULL, prepass_frag, defines);
		e_data.composite_sh_cache[index] = DRW_shader_create_fullscreen(composite_frag, defines);
		MEM_freeN(prepass_frag);
		MEM_freeN(composite_frag);
		MEM_freeN(defines);
	}

	wpd->prepass_sh = e_data.prepass_sh_cache[index];
	wpd->composite_sh = e_data.composite_sh_cache[index];
}

/* Functions */
static uint get_material_hash(WORKBENCH_PrivateData *wpd, WORKBENCH_MaterialData *material_template)
{
	uint input[4];
	float *color = material_template->color;
	input[0] = (uint)(color[0] * 512);
	input[1] = (uint)(color[1] * 512);
	input[2] = (uint)(color[2] * 512);

	/* Only hash object id when needed */
	input[3] = (uint)0;
	if (OBJECT_ID_PASS_ENABLED(wpd)) {
		input[3] = material_template->object_id;
	}

	return BLI_ghashutil_uinthash_v4_murmur(input);
}

static void workbench_init_object_data(ObjectEngineData *engine_data)
{
	WORKBENCH_ObjectData *data = (WORKBENCH_ObjectData *)engine_data;
	data->object_id = e_data.next_object_id++;
}

static void get_material_solid_color(WORKBENCH_PrivateData *wpd, WORKBENCH_ObjectData *engine_object_data, Object *ob, Material *mat, float *color, float hsv_saturation, float hsv_value)
{
	static float default_color[] = {1.0f, 1.0f, 1.0f};
	if (DRW_object_is_paint_mode(ob) || wpd->drawtype_options & V3D_DRAWOPTION_SINGLE_COLOR) {
		copy_v3_v3(color, default_color);
	}
	else if (wpd->drawtype_options & V3D_DRAWOPTION_RANDOMIZE) {
		float offset = fmodf(engine_object_data->object_id * M_GOLDEN_RATION_CONJUGATE, 1.0);
		float hsv[3] = {offset, hsv_saturation, hsv_value};
		hsv_to_rgb_v(hsv, color);
	}
	else if (wpd->drawtype_options & V3D_DRAWOPTION_OBJECT_COLOR) {
		copy_v3_v3(color, ob->col);
	}
	else {
		/* V3D_DRAWOPTION_MATERIAL_COLOR */
		if (mat) {
			copy_v3_v3(color, &mat->r);
		}
		else {
			copy_v3_v3(color, default_color);
		}
	}
}

void workbench_materials_engine_init(WORKBENCH_Data *vedata)
{
	WORKBENCH_FramebufferList *fbl = vedata->fbl;
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	if (!e_data.next_object_id) {
		memset(e_data.prepass_sh_cache,   0x00, sizeof(struct GPUShader *) * MAX_SHADERS);
		memset(e_data.composite_sh_cache, 0x00, sizeof(struct GPUShader *) * MAX_SHADERS);
		e_data.next_object_id = 1;
		e_data.shadow_sh = DRW_shader_create(datatoc_workbench_shadow_vert_glsl, datatoc_workbench_shadow_geom_glsl, NULL, NULL);
	}

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		const float *viewport_size = DRW_viewport_size_get();
		const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};
		e_data.object_id_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_R32UI, &draw_engine_workbench_solid);
		e_data.color_buffer_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA8, &draw_engine_workbench_solid);
#ifdef WORKBENCH_ENCODE_NORMALS
		e_data.normal_buffer_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RG8, &draw_engine_workbench_solid);
#else
		e_data.normal_buffer_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA32F, &draw_engine_workbench_solid);
#endif

		GPU_framebuffer_ensure_config(&fbl->prepass_fb, {
			GPU_ATTACHMENT_TEXTURE(dtxl->depth),
			GPU_ATTACHMENT_TEXTURE(e_data.object_id_tx),
			GPU_ATTACHMENT_TEXTURE(e_data.color_buffer_tx),
			GPU_ATTACHMENT_TEXTURE(e_data.normal_buffer_tx),
		});
	}

	/* Prepass */
	{
		int state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->prepass_pass = DRW_pass_create("Prepass", state);
	}
}

void workbench_materials_engine_free()
{
	for (int index = 0; index < MAX_SHADERS; index ++) {
		DRW_SHADER_FREE_SAFE(e_data.prepass_sh_cache[index]);
		DRW_SHADER_FREE_SAFE(e_data.composite_sh_cache[index]);
	}
	DRW_SHADER_FREE_SAFE(e_data.shadow_sh);
}

static void workbench_composite_uniforms(WORKBENCH_PrivateData *wpd, DRWShadingGroup *grp)
{
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
	DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
	DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", &e_data.color_buffer_tx);
	if (OBJECT_ID_PASS_ENABLED(wpd)) {
		DRW_shgroup_uniform_texture_ref(grp, "objectId", &e_data.object_id_tx);
	}
	if (NORMAL_VIEWPORT_PASS_ENABLED(wpd)) {
		DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &e_data.normal_buffer_tx);
	}
	DRW_shgroup_uniform_block(grp, "world_block", wpd->world_ubo);
	DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
}

void workbench_materials_cache_init(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	DRWShadingGroup *grp;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_WORKBENCH);
	static float light_multiplier = 1.0f;

	const DRWContextState *DCS = DRW_context_state_get();

	wpd->material_hash = BLI_ghash_ptr_new(__func__);

	View3D *v3d = DCS->v3d;
	if (v3d) {
		wpd->drawtype_lighting = v3d->drawtype_lighting;
		wpd->drawtype_options = v3d->drawtype_options;
		wpd->drawtype_studiolight = v3d->drawtype_studiolight;
		wpd->drawtype_ambient_intensity = v3d->drawtype_ambient_intensity;
	}
	else {
		wpd->drawtype_lighting = V3D_LIGHTING_STUDIO;
		wpd->drawtype_options = 0;
		wpd->drawtype_studiolight = 0;
		wpd->drawtype_ambient_intensity = 0.5;
	}

	select_deferred_shaders(wpd);
	/* Deferred Mix Pass */
	{
		WORKBENCH_UBO_World *wd = &wpd->world_data;
		UI_GetThemeColor3fv(UI_GetThemeValue(TH_SHOW_BACK_GRAD) ? TH_LOW_GRAD:TH_HIGH_GRAD, wd->background_color_low);
		UI_GetThemeColor3fv(TH_HIGH_GRAD, wd->background_color_high);
		studiolight_update_world(wpd->drawtype_studiolight, wd);

		wpd->world_ubo = DRW_uniformbuffer_create(sizeof(WORKBENCH_UBO_World), NULL);
		DRW_uniformbuffer_update(wpd->world_ubo, &wpd->world_data);

		copy_v3_v3(e_data.light_direction, BKE_collection_engine_property_value_get_float_array(props, "light_direction"));
		negate_v3(e_data.light_direction);

		if (SHADOW_ENABLED(wpd)) {
			psl->composite_pass = DRW_pass_create("Composite", DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_EQUAL);
			grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_pass);
			workbench_composite_uniforms(wpd, grp);
			DRW_shgroup_stencil_mask(grp, 0x00);
			DRW_shgroup_uniform_float(grp, "lightMultiplier", &light_multiplier, 1);
			DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);

#ifdef DEBUG_SHADOW_VOLUME
			psl->shadow_pass = DRW_pass_create("Shadow", DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK | DRW_STATE_WRITE_COLOR);
			grp = DRW_shgroup_create(e_data.shadow_sh, psl->shadow_pass);
			DRW_shgroup_uniform_vec3(grp, "lightDirection", e_data.light_direction, 1);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			wpd->shadow_shgrp = grp;
#else
			psl->shadow_pass = DRW_pass_create("Shadow", DRW_STATE_DEPTH_GREATER | DRW_STATE_WRITE_STENCIL_SHADOW);
			grp = DRW_shgroup_create(e_data.shadow_sh, psl->shadow_pass);
			DRW_shgroup_uniform_vec3(grp, "lightDirection", e_data.light_direction, 1);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			wpd->shadow_shgrp = grp;

			psl->composite_shadow_pass = DRW_pass_create("Composite Shadow", DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_NEQUAL);
			grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_shadow_pass);
			DRW_shgroup_stencil_mask(grp, 0x00);
			workbench_composite_uniforms(wpd, grp);
			DRW_shgroup_uniform_float(grp, "lightMultiplier", &wpd->drawtype_ambient_intensity, 1);
			DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
#endif
		}
		else {
			psl->composite_pass = DRW_pass_create("Composite", DRW_STATE_WRITE_COLOR);
			grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_pass);
			workbench_composite_uniforms(wpd, grp);
			DRW_shgroup_uniform_float(grp, "lightMultiplier", &light_multiplier, 1);
			DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
		}
	}
}
static WORKBENCH_MaterialData *get_or_create_material_data(WORKBENCH_Data *vedata, IDProperty *props, Object *ob, Material *mat)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	WORKBENCH_MaterialData *material;
	WORKBENCH_ObjectData *engine_object_data = (WORKBENCH_ObjectData *)DRW_object_engine_data_ensure(
	        ob, &draw_engine_workbench_solid, sizeof(WORKBENCH_ObjectData), &workbench_init_object_data, NULL);
	WORKBENCH_MaterialData material_template;
	float color[3];
	const float hsv_saturation = BKE_collection_engine_property_value_get_float(props, "random_object_color_saturation");
	const float hsv_value = BKE_collection_engine_property_value_get_float(props, "random_object_color_value");

	/* Solid */
	get_material_solid_color(wpd, engine_object_data, ob, mat, color, hsv_saturation, hsv_value);
	copy_v3_v3(material_template.color, color);
	material_template.object_id = engine_object_data->object_id;
	unsigned int hash = get_material_hash(wpd, &material_template);

	material = BLI_ghash_lookup(wpd->material_hash, SET_UINT_IN_POINTER(hash));
	if (material == NULL) {
		material = MEM_mallocN(sizeof(WORKBENCH_MaterialData), __func__);
		material->shgrp = DRW_shgroup_create(wpd->prepass_sh, psl->prepass_pass);
		DRW_shgroup_stencil_mask(material->shgrp, 0xFF);
		material->object_id = engine_object_data->object_id;
		copy_v3_v3(material->color, material_template.color);
		DRW_shgroup_uniform_vec3(material->shgrp, "object_color", material->color, 1);
		DRW_shgroup_uniform_int(material->shgrp, "object_id", &material->object_id, 1);
		BLI_ghash_insert(wpd->material_hash, SET_UINT_IN_POINTER(hash), material);
	}
	return material;
}

static void workbench_cache_populate_particles(WORKBENCH_Data *vedata, IDProperty *props, Object *ob)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();

	if (ob != draw_ctx->object_edit) {
		for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
			if (md->type == eModifierType_ParticleSystem) {
				ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;

				if (psys_check_enabled(ob, psys, false)) {
					ParticleSettings *part = psys->part;
					int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

					if (draw_as == PART_DRAW_PATH && !psys->pathcache && !psys->childcache) {
						draw_as = PART_DRAW_DOT;
					}

					static float mat[4][4];
					unit_m4(mat);

					if (draw_as == PART_DRAW_PATH) {
						struct Gwn_Batch *geom = DRW_cache_particles_get_hair(psys, NULL);
						WORKBENCH_MaterialData *material = get_or_create_material_data(vedata, props, ob, NULL);
						DRW_shgroup_call_add(material->shgrp, geom, mat);
					}
				}
			}
		}
	}
}

void workbench_materials_solid_cache_populate(WORKBENCH_Data *vedata, Object *ob)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PrivateData *wpd = stl->g_data;

	if (!DRW_object_is_renderable(ob))
		return;

	IDProperty *props = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_WORKBENCH);
	if (ob->type == OB_MESH) {
		workbench_cache_populate_particles(vedata, props, ob);
	}

	WORKBENCH_MaterialData *material;
	if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT)) {
		const DRWContextState *draw_ctx = DRW_context_state_get();
		const bool is_active = (ob == draw_ctx->obact);
		const bool is_sculpt_mode = is_active && (draw_ctx->object_mode & OB_MODE_SCULPT) != 0;

		if ((vedata->stl->g_data->drawtype_options & V3D_DRAWOPTION_SOLID_COLOR_MASK) != 0 || is_sculpt_mode) {
			/* No material split needed */
			struct Gwn_Batch *geom = DRW_cache_object_surface_get(ob);
			if (geom) {
				material = get_or_create_material_data(vedata, props, ob, NULL);
				if (is_sculpt_mode) {
					DRW_shgroup_call_sculpt_add(material->shgrp, ob, ob->obmat);
				}
				else {
					DRW_shgroup_call_object_add(material->shgrp, geom, ob);
				}
			}
		}
		else { /* MATERIAL colors */
			const int materials_len = MAX2(1, (is_sculpt_mode ? 1 : ob->totcol));
			struct GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
			for (int i = 0; i < materials_len; i ++) {
				gpumat_array[i] = NULL;
			}

			struct Gwn_Batch **mat_geom = DRW_cache_object_surface_material_get(ob, gpumat_array, materials_len, NULL, NULL, NULL);
			if (mat_geom) {
				for (int i = 0; i < materials_len; ++i) {
					Material *mat = give_current_material(ob, i + 1);
					material = get_or_create_material_data(vedata, props, ob, mat);
					DRW_shgroup_call_object_add(material->shgrp, mat_geom[i], ob);
				}
			}
		}

		if (SHADOW_ENABLED(wpd)) {
			struct Gwn_Batch *geom_shadow = DRW_cache_object_surface_get(ob);
			if (geom_shadow) {
				DRW_shgroup_call_object_add(wpd->shadow_shgrp, geom_shadow, ob);
			}
		}
	}
}

void workbench_materials_cache_finish(WORKBENCH_Data *UNUSED(vedata))
{
}

void workbench_materials_draw_background(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_FramebufferList *fbl = vedata->fbl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	const float clear_depth = 1.0f;
	const float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	unsigned int clear_stencil = 0xFF;

	GPU_framebuffer_bind(fbl->prepass_fb);
	int clear_bits = GPU_DEPTH_BIT;
	SET_FLAG_FROM_TEST(clear_bits, OBJECT_ID_PASS_ENABLED(wpd), GPU_COLOR_BIT);
	SET_FLAG_FROM_TEST(clear_bits, SHADOW_ENABLED(wpd), GPU_STENCIL_BIT);
	GPU_framebuffer_clear(fbl->prepass_fb, clear_bits, clear_color, clear_depth, clear_stencil);
}

void workbench_materials_draw_scene(WORKBENCH_Data *vedata)
{
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_FramebufferList *fbl = vedata->fbl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

	/* clear in background */
	GPU_framebuffer_bind(fbl->prepass_fb);
	DRW_draw_pass(psl->prepass_pass);
	if (SHADOW_ENABLED(wpd)) {
#ifdef DEBUG_SHADOW_VOLUME
		GPU_framebuffer_bind(dfbl->default_fb);
		DRW_draw_pass(psl->composite_pass);
		DRW_draw_pass(psl->shadow_pass);
#else
		GPU_framebuffer_bind(dfbl->depth_only_fb);
		DRW_draw_pass(psl->shadow_pass);
		GPU_framebuffer_bind(dfbl->default_fb);
		DRW_draw_pass(psl->composite_pass);
		DRW_draw_pass(psl->composite_shadow_pass);
#endif
	}
	else {
		GPU_framebuffer_bind(dfbl->default_fb);
		DRW_draw_pass(psl->composite_pass);
	}

	BLI_ghash_free(wpd->material_hash, NULL, MEM_freeN);
	DRW_UBO_FREE_SAFE(wpd->world_ubo);

}
