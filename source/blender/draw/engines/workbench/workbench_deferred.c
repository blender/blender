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

/** \file workbench_deferred.c
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


/* *********** STATIC *********** */

// #define DEBUG_SHADOW_VOLUME

#ifdef DEBUG_SHADOW_VOLUME
#  include "draw_debug.h"
#endif

static struct {
	struct GPUShader *prepass_sh_cache[MAX_SHADERS];
	struct GPUShader *composite_sh_cache[MAX_SHADERS];
	struct GPUShader *shadow_fail_sh;
	struct GPUShader *shadow_fail_manifold_sh;
	struct GPUShader *shadow_pass_sh;
	struct GPUShader *shadow_pass_manifold_sh;
	struct GPUShader *shadow_caps_sh;
	struct GPUShader *shadow_caps_manifold_sh;

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
extern char datatoc_workbench_deferred_composite_frag_glsl[];

extern char datatoc_workbench_shadow_vert_glsl[];
extern char datatoc_workbench_shadow_geom_glsl[];
extern char datatoc_workbench_shadow_caps_geom_glsl[];
extern char datatoc_workbench_shadow_debug_frag_glsl[];

extern char datatoc_workbench_background_lib_glsl[];
extern char datatoc_workbench_common_lib_glsl[];
extern char datatoc_workbench_data_lib_glsl[];
extern char datatoc_workbench_object_outline_lib_glsl[];
extern char datatoc_workbench_world_light_lib_glsl[];

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
		BLI_dynstr_append(ds, datatoc_workbench_object_outline_lib_glsl);
	}

	BLI_dynstr_append(ds, datatoc_workbench_deferred_composite_frag_glsl);

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

static void ensure_deferred_shaders(WORKBENCH_PrivateData *wpd, int index, int drawtype)
{
	if (e_data.prepass_sh_cache[index] == NULL) {
		char *defines = workbench_material_build_defines(wpd, drawtype);
		char *composite_frag = workbench_build_composite_frag(wpd);
		char *prepass_frag = workbench_build_prepass_frag();
		e_data.prepass_sh_cache[index] = DRW_shader_create(
		        datatoc_workbench_prepass_vert_glsl, NULL, prepass_frag, defines);
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
	int index_solid = workbench_material_get_shader_index(wpd, OB_SOLID);
	int index_texture = workbench_material_get_shader_index(wpd, OB_TEXTURE);

	ensure_deferred_shaders(wpd, index_solid, OB_SOLID);
	ensure_deferred_shaders(wpd, index_texture, OB_TEXTURE);

	wpd->prepass_solid_sh = e_data.prepass_sh_cache[index_solid];
	wpd->prepass_texture_sh = e_data.prepass_sh_cache[index_texture];
	wpd->composite_sh = e_data.composite_sh_cache[index_solid];
}

/* Functions */


static void workbench_init_object_data(ObjectEngineData *engine_data)
{
	WORKBENCH_ObjectData *data = (WORKBENCH_ObjectData *)engine_data;
	data->object_id = e_data.next_object_id++;
	data->shadow_bbox_dirty = true;
}

void workbench_deferred_engine_init(WORKBENCH_Data *vedata)
{
	WORKBENCH_FramebufferList *fbl = vedata->fbl;
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	if (!e_data.next_object_id) {
		memset(e_data.prepass_sh_cache,   0x00, sizeof(struct GPUShader *) * MAX_SHADERS);
		memset(e_data.composite_sh_cache, 0x00, sizeof(struct GPUShader *) * MAX_SHADERS);
		e_data.next_object_id = 1;
#ifdef DEBUG_SHADOW_VOLUME
		const char *shadow_frag = datatoc_workbench_shadow_debug_frag_glsl;
#else
		const char *shadow_frag = NULL;
#endif
		e_data.shadow_pass_sh = DRW_shader_create(
		        datatoc_workbench_shadow_vert_glsl,
		        datatoc_workbench_shadow_geom_glsl,
		        shadow_frag,
		        "#define SHADOW_PASS\n"
		        "#define DOUBLE_MANIFOLD\n");
		e_data.shadow_pass_manifold_sh = DRW_shader_create(
		        datatoc_workbench_shadow_vert_glsl,
		        datatoc_workbench_shadow_geom_glsl,
		        shadow_frag,
		        "#define SHADOW_PASS\n");
		e_data.shadow_fail_sh = DRW_shader_create(
		        datatoc_workbench_shadow_vert_glsl,
		        datatoc_workbench_shadow_geom_glsl,
		        shadow_frag,
		        "#define SHADOW_FAIL\n"
		        "#define DOUBLE_MANIFOLD\n");
		e_data.shadow_fail_manifold_sh = DRW_shader_create(
		        datatoc_workbench_shadow_vert_glsl,
		        datatoc_workbench_shadow_geom_glsl,
		        shadow_frag,
		        "#define SHADOW_FAIL\n");
		e_data.shadow_caps_sh = DRW_shader_create(
		        datatoc_workbench_shadow_vert_glsl,
		        datatoc_workbench_shadow_caps_geom_glsl,
		        shadow_frag,
		        "#define SHADOW_FAIL\n"
		        "#define DOUBLE_MANIFOLD\n");
		e_data.shadow_caps_manifold_sh = DRW_shader_create(
		        datatoc_workbench_shadow_vert_glsl,
		        datatoc_workbench_shadow_caps_geom_glsl,
		        shadow_frag,
		        "#define SHADOW_FAIL\n");
	}

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	workbench_private_data_init(stl->g_data);

	{
		const float *viewport_size = DRW_viewport_size_get();
		const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};
		e_data.object_id_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_R32UI, &draw_engine_workbench_solid);
		e_data.color_buffer_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA8, &draw_engine_workbench_solid);
		e_data.composite_buffer_tx = DRW_texture_pool_query_2D(
		        size[0], size[1], GPU_RGBA16F, &draw_engine_workbench_solid);

		if (NORMAL_ENCODING_ENABLED()) {
			e_data.normal_buffer_tx = DRW_texture_pool_query_2D(
			        size[0], size[1], GPU_RG16, &draw_engine_workbench_solid);
		}
		else {
			e_data.normal_buffer_tx = DRW_texture_pool_query_2D(
			        size[0], size[1], GPU_RGBA32F, &draw_engine_workbench_solid);
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
		int state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
		psl->prepass_pass = DRW_pass_create("Prepass", state);
	}
}

void workbench_deferred_engine_free()
{
	for (int index = 0; index < MAX_SHADERS; index++) {
		DRW_SHADER_FREE_SAFE(e_data.prepass_sh_cache[index]);
		DRW_SHADER_FREE_SAFE(e_data.composite_sh_cache[index]);
	}
	DRW_SHADER_FREE_SAFE(e_data.shadow_pass_sh);
	DRW_SHADER_FREE_SAFE(e_data.shadow_pass_manifold_sh);
	DRW_SHADER_FREE_SAFE(e_data.shadow_fail_sh);
	DRW_SHADER_FREE_SAFE(e_data.shadow_fail_manifold_sh);
	DRW_SHADER_FREE_SAFE(e_data.shadow_caps_sh);
	DRW_SHADER_FREE_SAFE(e_data.shadow_caps_manifold_sh);
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

	workbench_material_set_normal_world_matrix(grp, wpd, e_data.normal_world_matrix);
}

void workbench_deferred_cache_init(WORKBENCH_Data *vedata)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	DRWShadingGroup *grp;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	static float light_multiplier = 1.0f;


	Scene *scene = draw_ctx->scene;

	select_deferred_shaders(wpd);
	/* Deferred Mix Pass */
	{
		copy_v3_v3(e_data.display.light_direction, scene->display.light_direction);
		negate_v3(e_data.display.light_direction);
#if 0
	if (STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
		BKE_studiolight_ensure_flag(wpd->studio_light, STUDIOLIGHT_LIGHT_DIRECTION_CALCULATED);
		float rot_matrix[3][3];
		// float dir[3] = {0.57, 0.57, -0.57};
		axis_angle_to_mat3_single(rot_matrix, 'Z', wpd->shading.studiolight_rot_z);
		mul_v3_m3v3(e_data.display.light_direction, rot_matrix, wpd->studio_light->light_direction);
	}
#endif
		float view_matrix[4][4];
		DRW_viewport_matrix_get(view_matrix, DRW_MAT_VIEW);
		mul_v3_mat3_m4v3(e_data.light_direction_vs, view_matrix, e_data.display.light_direction);

		e_data.display.shadow_shift = scene->display.shadow_shift;

		if (SHADOW_ENABLED(wpd)) {
			psl->composite_pass = DRW_pass_create(
			        "Composite", DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_EQUAL);
			grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_pass);
			workbench_composite_uniforms(wpd, grp);
			DRW_shgroup_stencil_mask(grp, 0x00);
			DRW_shgroup_uniform_vec3(grp, "lightDirection", e_data.light_direction_vs, 1);
			DRW_shgroup_uniform_float(grp, "lightMultiplier", &light_multiplier, 1);
			DRW_shgroup_uniform_float(grp, "shadowMultiplier", &wpd->shadow_multiplier, 1);
			DRW_shgroup_uniform_float(grp, "shadowShift", &scene->display.shadow_shift, 1);
			DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);

			/* Stencil Shadow passes. */
#ifdef DEBUG_SHADOW_VOLUME
			DRWState depth_pass_state = DRW_STATE_DEPTH_LESS | DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE;
			DRWState depth_fail_state = DRW_STATE_DEPTH_GREATER_EQUAL | DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE;
#else
			DRWState depth_pass_state = DRW_STATE_DEPTH_LESS | DRW_STATE_WRITE_STENCIL_SHADOW_PASS;
			DRWState depth_fail_state = DRW_STATE_DEPTH_LESS | DRW_STATE_WRITE_STENCIL_SHADOW_FAIL;
#endif
			psl->shadow_depth_pass_pass = DRW_pass_create("Shadow Pass", depth_pass_state);
			psl->shadow_depth_pass_mani_pass = DRW_pass_create("Shadow Pass Mani", depth_pass_state);
			psl->shadow_depth_fail_pass = DRW_pass_create("Shadow Fail", depth_fail_state);
			psl->shadow_depth_fail_mani_pass = DRW_pass_create("Shadow Fail Mani", depth_fail_state);
			psl->shadow_depth_fail_caps_pass = DRW_pass_create("Shadow Fail Caps", depth_fail_state);
			psl->shadow_depth_fail_caps_mani_pass = DRW_pass_create("Shadow Fail Caps Mani", depth_fail_state);

#ifndef DEBUG_SHADOW_VOLUME
			grp = DRW_shgroup_create(e_data.shadow_caps_sh, psl->shadow_depth_fail_caps_pass);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			grp = DRW_shgroup_create(e_data.shadow_pass_sh, psl->shadow_depth_pass_pass);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			grp = DRW_shgroup_create(e_data.shadow_pass_manifold_sh, psl->shadow_depth_pass_mani_pass);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			grp = DRW_shgroup_create(e_data.shadow_fail_sh, psl->shadow_depth_fail_pass);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			grp = DRW_shgroup_create(e_data.shadow_fail_manifold_sh, psl->shadow_depth_fail_mani_pass);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			grp = DRW_shgroup_create(e_data.shadow_caps_sh, psl->shadow_depth_fail_caps_pass);
			DRW_shgroup_stencil_mask(grp, 0xFF);
			grp = DRW_shgroup_create(e_data.shadow_caps_manifold_sh, psl->shadow_depth_fail_caps_mani_pass);
			DRW_shgroup_stencil_mask(grp, 0xFF);

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

			studiolight_update_light(wpd, e_data.display.light_direction);
		}
		else {
			psl->composite_pass = DRW_pass_create(
			        "Composite", DRW_STATE_WRITE_COLOR);
			grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_pass);
			workbench_composite_uniforms(wpd, grp);
			DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
		}
	}
}

static WORKBENCH_MaterialData *get_or_create_material_data(
        WORKBENCH_Data *vedata, Object *ob, Material *mat, Image *ima, int drawtype)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
	WORKBENCH_PrivateData *wpd = stl->g_data;
	WORKBENCH_MaterialData *material;
	WORKBENCH_ObjectData *engine_object_data = (WORKBENCH_ObjectData *)DRW_object_engine_data_ensure(
	        ob, &draw_engine_workbench_solid, sizeof(WORKBENCH_ObjectData), &workbench_init_object_data, NULL);
	WORKBENCH_MaterialData material_template;

	/* Solid */
	workbench_material_get_solid_color(wpd, ob, mat, material_template.color);
	material_template.object_id = engine_object_data->object_id;
	material_template.drawtype = drawtype;
	material_template.ima = ima;
	uint hash = workbench_material_get_hash(&material_template);

	material = BLI_ghash_lookup(wpd->material_hash, SET_UINT_IN_POINTER(hash));
	if (material == NULL) {
		material = MEM_mallocN(sizeof(WORKBENCH_MaterialData), __func__);
		material->shgrp = DRW_shgroup_create(
		        drawtype == OB_SOLID ? wpd->prepass_solid_sh : wpd->prepass_texture_sh, psl->prepass_pass);
		DRW_shgroup_stencil_mask(material->shgrp, 0xFF);
		material->object_id = engine_object_data->object_id;
		copy_v4_v4(material->color, material_template.color);
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

void workbench_deferred_solid_cache_populate(WORKBENCH_Data *vedata, Object *ob)
{
	WORKBENCH_StorageList *stl = vedata->stl;
	WORKBENCH_PassList *psl = vedata->psl;
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

				struct Gwn_Batch **mat_geom = DRW_cache_object_surface_material_get(
				        ob, gpumat_array, materials_len, NULL, NULL, NULL);
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
			bool is_manifold;
			struct Gwn_Batch *geom_shadow = DRW_cache_object_edge_detection_get(ob, &is_manifold);
			if (geom_shadow) {
				if (is_sculpt_mode) {
					/* Currently unsupported in sculpt mode. We could revert to the slow
					 * method in this case but i'm not sure if it's a good idea given that
					 * sculped meshes are heavy to begin with. */
					// DRW_shgroup_call_sculpt_add(wpd->shadow_shgrp, ob, ob->obmat);
				}
				else {
					WORKBENCH_ObjectData *engine_object_data = (WORKBENCH_ObjectData *)DRW_object_engine_data_ensure(
					        ob, &draw_engine_workbench_solid, sizeof(WORKBENCH_ObjectData), &workbench_init_object_data, NULL);

					if (studiolight_object_cast_visible_shadow(wpd, ob, engine_object_data)) {

						invert_m4_m4(ob->imat, ob->obmat);
						mul_v3_mat3_m4v3(engine_object_data->shadow_dir, ob->imat, e_data.display.light_direction);

						DRWShadingGroup *grp;
						bool use_shadow_pass_technique = !studiolight_camera_in_object_shadow(wpd, ob, engine_object_data);

						/* Unless we expose a parameter to the user, it's better to use the depth pass technique if the object is
						 * non manifold. Exposing a switch to the user to force depth fail in this case can be beneficial for
						 * planes and non-closed terrains. */
						if (!is_manifold) {
							use_shadow_pass_technique = true;
						}

						if (use_shadow_pass_technique) {
							if (is_manifold) {
								grp = DRW_shgroup_create(e_data.shadow_pass_manifold_sh, psl->shadow_depth_pass_mani_pass);
							}
							else {
								grp = DRW_shgroup_create(e_data.shadow_pass_sh, psl->shadow_depth_pass_pass);
							}
							DRW_shgroup_uniform_vec3(grp, "lightDirection", engine_object_data->shadow_dir, 1);
							DRW_shgroup_call_add(grp, geom_shadow, ob->obmat);
#ifdef DEBUG_SHADOW_VOLUME
							DRW_debug_bbox(&engine_object_data->shadow_bbox, (float[4]){1.0f, 0.0f, 0.0f, 1.0f});
#endif
						}
						else {
							/* TODO(fclem): only use caps if they are in the view frustum. */
							const bool need_caps = true;
							if (need_caps) {
								if (is_manifold) {
									grp = DRW_shgroup_create(e_data.shadow_caps_manifold_sh, psl->shadow_depth_fail_caps_mani_pass);
								}
								else {
									grp = DRW_shgroup_create(e_data.shadow_caps_sh, psl->shadow_depth_fail_caps_pass);
								}
								DRW_shgroup_uniform_vec3(grp, "lightDirection", engine_object_data->shadow_dir, 1);
								DRW_shgroup_call_add(grp, DRW_cache_object_surface_get(ob), ob->obmat);
							}

							if (is_manifold) {
								grp = DRW_shgroup_create(e_data.shadow_fail_manifold_sh, psl->shadow_depth_fail_mani_pass);
							}
							else {
								grp = DRW_shgroup_create(e_data.shadow_fail_sh, psl->shadow_depth_fail_pass);
							}
							DRW_shgroup_uniform_vec3(grp, "lightDirection", engine_object_data->shadow_dir, 1);
							DRW_shgroup_call_add(grp, geom_shadow, ob->obmat);
#ifdef DEBUG_SHADOW_VOLUME
							DRW_debug_bbox(&engine_object_data->shadow_bbox, (float[4]){0.0f, 1.0f, 0.0f, 1.0f});
#endif
						}
					}
				}
			}
		}
	}
}

void workbench_deferred_cache_finish(WORKBENCH_Data *UNUSED(vedata))
{
}

void workbench_deferred_draw_background(WORKBENCH_Data *vedata)
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

void workbench_deferred_draw_scene(WORKBENCH_Data *vedata)
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
		GPU_framebuffer_bind(fbl->composite_fb);
		DRW_draw_pass(psl->composite_pass);
#else
		GPU_framebuffer_bind(dfbl->depth_only_fb);
#endif
		DRW_draw_pass(psl->shadow_depth_pass_pass);
		DRW_draw_pass(psl->shadow_depth_pass_mani_pass);
		DRW_draw_pass(psl->shadow_depth_fail_pass);
		DRW_draw_pass(psl->shadow_depth_fail_mani_pass);
		DRW_draw_pass(psl->shadow_depth_fail_caps_pass);
		DRW_draw_pass(psl->shadow_depth_fail_caps_mani_pass);
#ifndef DEBUG_SHADOW_VOLUME
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

	workbench_private_data_free(wpd);
}
