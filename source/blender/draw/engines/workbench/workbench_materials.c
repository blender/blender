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

#include "BIF_gl.h"

#include "BLI_alloca.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BKE_node.h"
#include "BKE_particle.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "ED_uvedit.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

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
	struct GPUTexture *composite_buffer_tx; /* ref only, not alloced */

	SceneDisplay display; /* world light direction for shadows */
	float light_direction_vs[3];
	int next_object_id;
	float normal_world_matrix[3][3];
} e_data = {{NULL}};

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

#define OBJECT_ID_PASS_ENABLED(wpd) (wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE)
#define NORMAL_VIEWPORT_PASS_ENABLED(wpd) (wpd->shading.light & V3D_LIGHTING_STUDIO || wpd->shading.flag & V3D_SHADING_SHADOW)
#define SHADOW_ENABLED(wpd) (wpd->shading.flag & V3D_SHADING_SHADOW)
#define NORMAL_ENCODING_ENABLED() (true)
#define STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd) (wpd->studio_light->flag & STUDIOLIGHT_ORIENTATION_WORLD)


static char *workbench_build_defines(WORKBENCH_PrivateData *wpd, int drawtype)
{
	char *str = NULL;

	DynStr *ds = BLI_dynstr_new();

	if (wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE) {
		BLI_dynstr_appendf(ds, "#define V3D_SHADING_OBJECT_OUTLINE\n");
	}
	if (wpd->shading.flag & V3D_SHADING_SHADOW) {
		if (!STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
			BLI_dynstr_appendf(ds, "#define V3D_SHADING_SHADOW\n");
		}
	}
	if (wpd->shading.light & V3D_LIGHTING_STUDIO) {
		BLI_dynstr_appendf(ds, "#define V3D_LIGHTING_STUDIO\n");
		if (STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
			BLI_dynstr_appendf(ds, "#define STUDIOLIGHT_ORIENTATION_WORLD\n");
		}
		else {
			BLI_dynstr_appendf(ds, "#define STUDIOLIGHT_ORIENTATION_CAMERA\n");
		}
	}
	if (NORMAL_VIEWPORT_PASS_ENABLED(wpd)) {
		BLI_dynstr_appendf(ds, "#define NORMAL_VIEWPORT_PASS_ENABLED\n");
	}
	switch (drawtype) {
		case OB_SOLID:
			BLI_dynstr_appendf(ds, "#define OB_SOLID\n");
			break;
		case OB_TEXTURE:
			BLI_dynstr_appendf(ds, "#define OB_TEXTURE\n");
			break;
	}

	if (NORMAL_ENCODING_ENABLED()) {
		BLI_dynstr_appendf(ds, "#define WORKBENCH_ENCODE_NORMALS\n");
	}

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

	if (wpd->shading.light & V3D_LIGHTING_STUDIO) {
		BLI_dynstr_append(ds, datatoc_workbench_world_light_lib_glsl);
	}
	if (wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE) {
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

static int get_shader_index(WORKBENCH_PrivateData *wpd, int drawtype)
{
	const int DRAWOPTIONS_MASK = V3D_SHADING_OBJECT_OUTLINE | V3D_SHADING_SHADOW;
	int index = (wpd->shading.flag & DRAWOPTIONS_MASK);
	index = (index << 2) + wpd->shading.light;
	index = (index << 2);
	/* set the drawtype flag
	0 = OB_SOLID,
	1 = OB_TEXTURE
	2 = STUDIOLIGHT_ORIENTATION_WORLD
	*/
	SET_FLAG_FROM_TEST(index, wpd->studio_light->flag & STUDIOLIGHT_ORIENTATION_WORLD, 2);
	SET_FLAG_FROM_TEST(index, drawtype == OB_TEXTURE, 1);
	return index;
}

static void ensure_deferred_shaders(WORKBENCH_PrivateData *wpd, int index, int drawtype)
{
	if (e_data.prepass_sh_cache[index] == NULL) {
		char *defines = workbench_build_defines(wpd, drawtype);
		char *composite_frag = workbench_build_composite_frag(wpd);
		char *prepass_frag = workbench_build_prepass_frag();
		e_data.prepass_sh_cache[index] = DRW_shader_create(datatoc_workbench_prepass_vert_glsl, NULL, prepass_frag, defines);
		if (drawtype == OB_SOLID) {
			e_data.composite_sh_cache[index] = DRW_shader_create_fullscreen(composite_frag, defines);
		}
		MEM_freeN(prepass_frag);
		MEM_freeN(composite_frag);
		MEM_freeN(defines);
	}
}

static void select_deferred_shaders(WORKBENCH_PrivateData *wpd)
{
	int index_solid = get_shader_index(wpd, OB_SOLID);
	int index_texture = get_shader_index(wpd, OB_TEXTURE);

	ensure_deferred_shaders(wpd, index_solid, OB_SOLID);
	ensure_deferred_shaders(wpd, index_texture, OB_TEXTURE);

	wpd->prepass_solid_sh = e_data.prepass_sh_cache[index_solid];
	wpd->prepass_texture_sh = e_data.prepass_sh_cache[index_texture];
	wpd->composite_sh = e_data.composite_sh_cache[index_solid];
}

/* Functions */
static uint get_material_hash(WORKBENCH_MaterialData *material_template)
{
	/* TODO: make a C-string with settings and hash the string */
	uint input[4];
	uint result;
	float *color = material_template->color;
	input[0] = (uint)(color[0] * 512);
	input[1] = (uint)(color[1] * 512);
	input[2] = (uint)(color[2] * 512);
	input[3] = material_template->object_id;
	result = BLI_ghashutil_uinthash_v4_murmur(input);

	if (material_template->drawtype == OB_TEXTURE) {
		/* add texture reference */
		result += BLI_ghashutil_inthash_p_murmur(material_template->ima);
	}
	return result;
}

static void workbench_init_object_data(ObjectEngineData *engine_data)
{
	WORKBENCH_ObjectData *data = (WORKBENCH_ObjectData *)engine_data;
	data->object_id = e_data.next_object_id++;
}

static void get_material_solid_color(WORKBENCH_PrivateData *wpd, Object *ob, Material *mat, float *color, float hsv_saturation, float hsv_value)
{
	/* When in OB_TEXTURE always uyse V3D_SHADING_MATERIAL_COLOR as fallback when no texture could be determined */
	int color_type = wpd->drawtype == OB_SOLID ? wpd->shading.color_type : V3D_SHADING_MATERIAL_COLOR;
	static float default_color[] = {0.8f, 0.8f, 0.8f};
	if (DRW_object_is_paint_mode(ob) || color_type == V3D_SHADING_SINGLE_COLOR) {
		copy_v3_v3(color, wpd->shading.single_color);
	}
	else if (color_type == V3D_SHADING_RANDOM_COLOR) {
		uint hash = BLI_ghashutil_strhash_p_murmur(ob->id.name);
		if (ob->id.lib) {
			hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob->id.lib->name);
		}
		float offset = fmodf((hash / 100000.0) * M_GOLDEN_RATION_CONJUGATE, 1.0);

		float hsv[3] = {offset, hsv_saturation, hsv_value};
		hsv_to_rgb_v(hsv, color);
	}
	else if (color_type == V3D_SHADING_OBJECT_COLOR) {
		copy_v3_v3(color, ob->col);
	}
	else {
		/* V3D_SHADING_MATERIAL_COLOR */
		if (mat) {
			copy_v3_v3(color, &mat->r);
		}
		else {
			copy_v3_v3(color, default_color);
		}
	}
}
static void workbench_private_data_init(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	const DRWContextState *draw_ctx = DRW_context_state_get();

	View3D *v3d = draw_ctx->v3d;
	if (v3d) {
		wpd->shading = v3d->shading;
		wpd->drawtype = v3d->drawtype;
		wpd->studio_light = BKE_studiolight_find(wpd->shading.studio_light);
	}
	else {
		/* XXX: We should get the default shading from the view layer, after we implemented the render callback */
		memset(&wpd->shading, 0, sizeof(wpd->shading));
		wpd->shading.light = V3D_LIGHTING_STUDIO;
		wpd->shading.shadow_intensity = 0.5;
		copy_v3_fl(wpd->shading.single_color, 0.8f);
		wpd->drawtype = OB_SOLID;
		wpd->studio_light = BKE_studiolight_findindex(0);
	}
	wpd->shadow_multiplier = 1.0 - wpd->shading.shadow_intensity;

	WORKBENCH_UBO_World *wd = &wpd->world_data;
	UI_GetThemeColor3fv(UI_GetThemeValue(TH_SHOW_BACK_GRAD) ? TH_LOW_GRAD : TH_HIGH_GRAD, wd->background_color_low);
	UI_GetThemeColor3fv(TH_HIGH_GRAD, wd->background_color_high);

	/* XXX: Really quick conversion to avoid washed out background.
	 * Needs to be adressed properly (color managed using ocio). */
	srgb_to_linearrgb_v3_v3(wd->background_color_high, wd->background_color_high);
	srgb_to_linearrgb_v3_v3(wd->background_color_low, wd->background_color_low);

	studiolight_update_world(wpd->studio_light, wd);

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

	workbench_private_data_init(vedata);

	{
		const float *viewport_size = DRW_viewport_size_get();
		const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};
		e_data.object_id_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_R32UI, &draw_engine_workbench_solid);
		e_data.color_buffer_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA8, &draw_engine_workbench_solid);
		e_data.composite_buffer_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA16F, &draw_engine_workbench_solid);

		if (NORMAL_ENCODING_ENABLED()) {
			e_data.normal_buffer_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RG16, &draw_engine_workbench_solid);
		}
		else {
			e_data.normal_buffer_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA32F, &draw_engine_workbench_solid);
		}

		GPU_framebuffer_ensure_config(&fbl->prepass_fb, {
			GPU_ATTACHMENT_TEXTURE(dtxl->depth),
			GPU_ATTACHMENT_TEXTURE(e_data.object_id_tx),
			GPU_ATTACHMENT_TEXTURE(e_data.color_buffer_tx),
			GPU_ATTACHMENT_TEXTURE(e_data.normal_buffer_tx),
		});
		GPU_framebuffer_ensure_config(&fbl->composite_fb, {
			GPU_ATTACHMENT_TEXTURE(dtxl->depth),
			GPU_ATTACHMENT_TEXTURE(e_data.composite_buffer_tx),
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
	for (int index = 0; index < MAX_SHADERS; index++) {
		DRW_SHADER_FREE_SAFE(e_data.prepass_sh_cache[index]);
		DRW_SHADER_FREE_SAFE(e_data.composite_sh_cache[index]);
	}
	DRW_SHADER_FREE_SAFE(e_data.shadow_sh);
}

static void workbench_composite_uniforms(WORKBENCH_PrivateData *wpd, DRWShadingGroup *grp)
{
	DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", &e_data.color_buffer_tx);
	DRW_shgroup_uniform_texture_ref(grp, "objectId", &e_data.object_id_tx);
	if (NORMAL_VIEWPORT_PASS_ENABLED(wpd)) {
		DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &e_data.normal_buffer_tx);
	}
	DRW_shgroup_uniform_block(grp, "world_block", wpd->world_ubo);
	DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);

	if (STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
		float view_matrix_inverse[4][4];
		float rot_matrix[4][4];
		float matrix[4][4];
		axis_angle_to_mat4_single(rot_matrix, 'Z', -wpd->shading.studiolight_rot_z);
		DRW_viewport_matrix_get(view_matrix_inverse, DRW_MAT_VIEWINV);
		mul_m4_m4m4(matrix, rot_matrix, view_matrix_inverse);
		copy_m3_m4(e_data.normal_world_matrix, matrix);
		DRW_shgroup_uniform_mat3(grp, "normalWorldMatrix", e_data.normal_world_matrix);
	}
}

void workbench_materials_cache_init(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	DRWShadingGroup *grp;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	static float light_multiplier = 1.0f;

	wpd->material_hash = BLI_ghash_ptr_new(__func__);

	Scene *scene = draw_ctx->scene;

	select_deferred_shaders(wpd);
	/* Deferred Mix Pass */
	{
		wpd->world_ubo = DRW_uniformbuffer_create(sizeof(WORKBENCH_UBO_World), NULL);
		DRW_uniformbuffer_update(wpd->world_ubo, &wpd->world_data);

		if (STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
			BKE_studiolight_ensure_flag(wpd->studio_light, STUDIOLIGHT_LIGHT_DIRECTION_CALCULATED);
			float rot_matrix[3][3];
			// float dir[3] = {0.57, 0.57, -0.57};
			axis_angle_to_mat3_single(rot_matrix, 'Z', wpd->shading.studiolight_rot_z);
			mul_v3_m3v3(e_data.display.light_direction, rot_matrix, wpd->studio_light->light_direction);
		}
		else {
			copy_v3_v3(e_data.display.light_direction, scene->display.light_direction);
			negate_v3(e_data.display.light_direction);
		}
		float view_matrix[4][4];
		DRW_viewport_matrix_get(view_matrix, DRW_MAT_VIEW);
		mul_v3_mat3_m4v3(e_data.light_direction_vs, view_matrix, e_data.display.light_direction);

		e_data.display.shadow_shift = scene->display.shadow_shift;

		if (SHADOW_ENABLED(wpd)) {
			psl->composite_pass = DRW_pass_create("Composite", DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_EQUAL);
			grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_pass);
			workbench_composite_uniforms(wpd, grp);
			DRW_shgroup_stencil_mask(grp, 0x00);
			DRW_shgroup_uniform_vec3(grp, "lightDirection", e_data.light_direction_vs, 1);
			DRW_shgroup_uniform_float(grp, "lightMultiplier", &light_multiplier, 1);
			DRW_shgroup_uniform_float(grp, "shadowMultiplier", &wpd->shadow_multiplier, 1);
			DRW_shgroup_uniform_float(grp, "shadowShift", &scene->display.shadow_shift, 1);
			DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);

#ifdef DEBUG_SHADOW_VOLUME
			psl->shadow_pass = DRW_pass_create("Shadow", DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK | DRW_STATE_WRITE_COLOR);
			grp = DRW_shgroup_create(e_data.shadow_sh, psl->shadow_pass);
			DRW_shgroup_uniform_vec3(grp, "lightDirection", e_data.display.light_direction, 1);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			wpd->shadow_shgrp = grp;
#else
			psl->shadow_pass = DRW_pass_create("Shadow", DRW_STATE_DEPTH_GREATER | DRW_STATE_WRITE_STENCIL_SHADOW);
			grp = DRW_shgroup_create(e_data.shadow_sh, psl->shadow_pass);
			DRW_shgroup_uniform_vec3(grp, "lightDirection", e_data.display.light_direction, 1);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			wpd->shadow_shgrp = grp;

			psl->composite_shadow_pass = DRW_pass_create("Composite Shadow", DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_NEQUAL);
			grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_shadow_pass);
			DRW_shgroup_stencil_mask(grp, 0x00);
			workbench_composite_uniforms(wpd, grp);
			DRW_shgroup_uniform_vec3(grp, "lightDirection", e_data.light_direction_vs, 1);
			DRW_shgroup_uniform_float(grp, "lightMultiplier", &wpd->shadow_multiplier, 1);
			DRW_shgroup_uniform_float(grp, "shadowMultiplier", &wpd->shadow_multiplier, 1);
			DRW_shgroup_uniform_float(grp, "shadowShift", &scene->display.shadow_shift, 1);
			DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
#endif
		}
		else {
			psl->composite_pass = DRW_pass_create("Composite", DRW_STATE_WRITE_COLOR);
			grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_pass);
			workbench_composite_uniforms(wpd, grp);
			DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
		}
	}
}
static WORKBENCH_MaterialData *get_or_create_material_data(WORKBENCH_Data *vedata, Object *ob, Material *mat, Image *ima, int drawtype)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	WORKBENCH_MaterialData *material;
	WORKBENCH_ObjectData *engine_object_data = (WORKBENCH_ObjectData *)DRW_object_engine_data_ensure(
	        ob, &draw_engine_workbench_solid, sizeof(WORKBENCH_ObjectData), &workbench_init_object_data, NULL);
	WORKBENCH_MaterialData material_template;
	const float hsv_saturation = 0.5;
	const float hsv_value = 0.9;

	/* Solid */
	get_material_solid_color(wpd, ob, mat, material_template.color, hsv_saturation, hsv_value);
	material_template.object_id = engine_object_data->object_id;
	material_template.drawtype = drawtype;
	material_template.ima = ima;
	uint hash = get_material_hash(&material_template);

	material = BLI_ghash_lookup(wpd->material_hash, SET_UINT_IN_POINTER(hash));
	if (material == NULL) {
		material = MEM_mallocN(sizeof(WORKBENCH_MaterialData), __func__);
		material->shgrp = DRW_shgroup_create(drawtype == OB_SOLID ? wpd->prepass_solid_sh : wpd->prepass_texture_sh, psl->prepass_pass);
		DRW_shgroup_stencil_mask(material->shgrp, 0xFF);
		material->object_id = engine_object_data->object_id;
		copy_v3_v3(material->color, material_template.color);
		switch (drawtype) {
			case OB_SOLID:
				DRW_shgroup_uniform_vec3(material->shgrp, "object_color", material->color, 1);
				break;

			case OB_TEXTURE:
			{
				GPUTexture *tex = GPU_texture_from_blender(ima, NULL, GL_TEXTURE_2D, false, false, false);
				DRW_shgroup_uniform_texture(material->shgrp, "image", tex);
				break;
			}
		}
		DRW_shgroup_uniform_int(material->shgrp, "object_id", &material->object_id, 1);
		BLI_ghash_insert(wpd->material_hash, SET_UINT_IN_POINTER(hash), material);
	}
	return material;
}

static void workbench_cache_populate_particles(WORKBENCH_Data *vedata, Object *ob)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	if (ob == draw_ctx->object_edit) {
		return;
	}
	for (ParticleSystem *psys = ob->particlesystem.first; psys != NULL; psys = psys->next) {
		if (!psys_check_enabled(ob, psys, false)) {
			continue;
		}
		if (!DRW_check_psys_visible_within_active_context(ob, psys)) {
			return;
		}
		ParticleSettings *part = psys->part;
		const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

		static float mat[4][4];
		unit_m4(mat);

		if (draw_as == PART_DRAW_PATH) {
			struct Gwn_Batch *geom = DRW_cache_particles_get_hair(ob, psys, NULL);
			WORKBENCH_MaterialData *material = get_or_create_material_data(vedata, ob, NULL, NULL, OB_SOLID);
			DRW_shgroup_call_add(material->shgrp, geom, mat);
		}
	}
}

void workbench_materials_solid_cache_populate(WORKBENCH_Data *vedata, Object *ob)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PrivateData *wpd = stl->g_data;

	if (!DRW_object_is_renderable(ob))
		return;

	if (ob->type == OB_MESH) {
		workbench_cache_populate_particles(vedata, ob);
	}

	WORKBENCH_MaterialData *material;
	if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT)) {
		const DRWContextState *draw_ctx = DRW_context_state_get();
		const bool is_active = (ob == draw_ctx->obact);
		const bool is_sculpt_mode = is_active && (draw_ctx->object_mode & OB_MODE_SCULPT) != 0;
		bool is_drawn = false;
		if (!is_sculpt_mode && wpd->drawtype == OB_TEXTURE && ob->type == OB_MESH) {
			const Mesh *me = ob->data;
			if (me->mloopuv) {
				const int materials_len = MAX2(1, (is_sculpt_mode ? 1 : ob->totcol));
				struct GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
				struct Gwn_Batch **geom_array = me->totcol ? DRW_cache_mesh_surface_texpaint_get(ob) : NULL;
				if (materials_len > 0 && geom_array) {
					for (int i = 0; i < materials_len; i++) {
						Material *mat = give_current_material(ob, i + 1);
						Image *image;
						ED_object_get_active_image(ob, i + 1, &image, NULL, NULL, NULL);
						/* use OB_SOLID when no texture could be determined */
						int mat_drawtype = OB_SOLID;
						if (image) {
							mat_drawtype = OB_TEXTURE;
						}
						material = get_or_create_material_data(vedata, ob, mat, image, mat_drawtype);
						DRW_shgroup_call_object_add(material->shgrp, geom_array[i], ob);
					}
					is_drawn = true;
				}
			}
		}

		/* Fallback from not drawn OB_TEXTURE mode or just OB_SOLID mode */
		if (!is_drawn) {
			if ((wpd->shading.color_type != V3D_SHADING_MATERIAL_COLOR) || is_sculpt_mode) {
				/* No material split needed */
				struct Gwn_Batch *geom = DRW_cache_object_surface_get(ob);
				if (geom) {
					material = get_or_create_material_data(vedata, ob, NULL, NULL, OB_SOLID);
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
				for (int i = 0; i < materials_len; i++) {
					gpumat_array[i] = NULL;
				}

				struct Gwn_Batch **mat_geom = DRW_cache_object_surface_material_get(ob, gpumat_array, materials_len, NULL, NULL, NULL);
				if (mat_geom) {
					for (int i = 0; i < materials_len; ++i) {
						Material *mat = give_current_material(ob, i + 1);
						material = get_or_create_material_data(vedata, ob, mat, NULL, OB_SOLID);
						DRW_shgroup_call_object_add(material->shgrp, mat_geom[i], ob);
					}
				}
			}
		}

		if (SHADOW_ENABLED(wpd) && (ob->display.flag & OB_SHOW_SHADOW) > 0) {
			struct Gwn_Batch *geom_shadow = DRW_cache_object_surface_get(ob);
			if (geom_shadow) {
				if (is_sculpt_mode) {
					DRW_shgroup_call_sculpt_add(wpd->shadow_shgrp, ob, ob->obmat);
				}
				else {
					DRW_shgroup_call_object_add(wpd->shadow_shgrp, geom_shadow, ob);
				}
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
	uint clear_stencil = 0xFF;

	DRW_stats_group_start("Clear Background");
	GPU_framebuffer_bind(fbl->prepass_fb);
	int clear_bits = GPU_DEPTH_BIT | GPU_COLOR_BIT;
	SET_FLAG_FROM_TEST(clear_bits, SHADOW_ENABLED(wpd), GPU_STENCIL_BIT);
	GPU_framebuffer_clear(fbl->prepass_fb, clear_bits, clear_color, clear_depth, clear_stencil);
	DRW_stats_group_end();
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
		GPU_framebuffer_bind(fbl->composite_fb);
		DRW_draw_pass(psl->composite_pass);
		DRW_draw_pass(psl->composite_shadow_pass);
#endif
	}
	else {
		GPU_framebuffer_bind(fbl->composite_fb);
		DRW_draw_pass(psl->composite_pass);
	}

	GPU_framebuffer_bind(dfbl->color_only_fb);
	DRW_transform_to_display(e_data.composite_buffer_tx);

	BLI_ghash_free(wpd->material_hash, NULL, MEM_freeN);
	DRW_UBO_FREE_SAFE(wpd->world_ubo);

}
