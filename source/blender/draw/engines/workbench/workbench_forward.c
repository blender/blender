/*
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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "workbench_private.h"

#include "BIF_gl.h"

#include "BLI_alloca.h"
#include "BLI_dynstr.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BKE_particle.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "ED_view3d.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

/* *********** STATIC *********** */

typedef struct WORKBENCH_FORWARD_Shaders {
  struct GPUShader *transparent_accum_sh_cache[MAX_ACCUM_SHADERS];
  struct GPUShader *object_outline_sh;
  struct GPUShader *object_outline_texture_sh;
  struct GPUShader *object_outline_hair_sh;
} WORKBENCH_FORWARD_Shaders;

static struct {
  WORKBENCH_FORWARD_Shaders sh_data[GPU_SHADER_CFG_LEN];

  struct GPUShader *composite_sh_cache[2];
  struct GPUShader *checker_depth_sh;

  struct GPUTexture *object_id_tx;             /* ref only, not alloced */
  struct GPUTexture *transparent_accum_tx;     /* ref only, not alloced */
  struct GPUTexture *transparent_revealage_tx; /* ref only, not alloced */
  struct GPUTexture *composite_buffer_tx;      /* ref only, not alloced */

  int next_object_id;
} e_data = {{{{NULL}}}};

/* Shaders */
extern char datatoc_common_hair_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

extern char datatoc_workbench_forward_composite_frag_glsl[];
extern char datatoc_workbench_forward_depth_frag_glsl[];
extern char datatoc_workbench_forward_transparent_accum_frag_glsl[];
extern char datatoc_workbench_data_lib_glsl[];
extern char datatoc_workbench_background_lib_glsl[];
extern char datatoc_workbench_checkerboard_depth_frag_glsl[];
extern char datatoc_workbench_object_outline_lib_glsl[];
extern char datatoc_workbench_curvature_lib_glsl[];
extern char datatoc_workbench_prepass_vert_glsl[];
extern char datatoc_workbench_common_lib_glsl[];
extern char datatoc_workbench_world_light_lib_glsl[];

/* static functions */
static char *workbench_build_forward_vert(bool is_hair)
{
  DynStr *ds = BLI_dynstr_new();
  if (is_hair) {
    BLI_dynstr_append(ds, datatoc_common_hair_lib_glsl);
  }
  BLI_dynstr_append(ds, datatoc_common_view_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_prepass_vert_glsl);

  char *str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  return str;
}

static char *workbench_build_forward_transparent_accum_frag(void)
{
  DynStr *ds = BLI_dynstr_new();

  BLI_dynstr_append(ds, datatoc_common_view_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_data_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_common_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_world_light_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_forward_transparent_accum_frag_glsl);

  char *str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  return str;
}

static char *workbench_build_forward_composite_frag(void)
{
  DynStr *ds = BLI_dynstr_new();

  BLI_dynstr_append(ds, datatoc_workbench_data_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_common_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_background_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_object_outline_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_curvature_lib_glsl);
  BLI_dynstr_append(ds, datatoc_workbench_forward_composite_frag_glsl);

  char *str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  return str;
}

static void workbench_init_object_data(DrawData *dd)
{
  WORKBENCH_ObjectData *data = (WORKBENCH_ObjectData *)dd;
  data->object_id = ((e_data.next_object_id++) & 0xff) + 1;
}

WORKBENCH_MaterialData *workbench_forward_get_or_create_material_data(WORKBENCH_Data *vedata,
                                                                      Object *ob,
                                                                      Material *mat,
                                                                      Image *ima,
                                                                      ImageUser *iuser,
                                                                      int color_type,
                                                                      int interp,
                                                                      bool is_sculpt_mode)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  WORKBENCH_FORWARD_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_PrivateData *wpd = stl->g_data;
  WORKBENCH_MaterialData *material;
  WORKBENCH_ObjectData *engine_object_data = (WORKBENCH_ObjectData *)DRW_drawdata_ensure(
      &ob->id,
      &draw_engine_workbench_solid,
      sizeof(WORKBENCH_ObjectData),
      &workbench_init_object_data,
      NULL);
  WORKBENCH_MaterialData material_template;
  DRWShadingGroup *grp;

  /* Solid */
  workbench_material_update_data(wpd, ob, mat, &material_template);
  material_template.object_id = OBJECT_ID_PASS_ENABLED(wpd) ? engine_object_data->object_id : 1;
  material_template.color_type = color_type;
  material_template.ima = ima;
  material_template.iuser = iuser;
  material_template.interp = interp;
  uint hash = workbench_material_get_hash(&material_template, false);

  material = BLI_ghash_lookup(wpd->material_transp_hash, POINTER_FROM_UINT(hash));
  if (material == NULL) {
    material = MEM_mallocN(sizeof(WORKBENCH_MaterialData), __func__);

    /* transparent accum */
    grp = DRW_shgroup_create(wpd->shading.color_type == color_type ?
                                 wpd->transparent_accum_sh :
                                 wpd->transparent_accum_uniform_sh,
                             psl->transparent_accum_pass);
    DRW_shgroup_uniform_block(grp, "world_block", wpd->world_ubo);
    DRW_shgroup_uniform_float_copy(grp, "alpha", wpd->shading.xray_alpha);
    DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)wpd->viewvecs, 3);
    workbench_material_copy(material, &material_template);
    if (STUDIOLIGHT_TYPE_MATCAP_ENABLED(wpd)) {
      BKE_studiolight_ensure_flag(wpd->studio_light, STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE);
      DRW_shgroup_uniform_texture(
          grp, "matcapImage", wpd->studio_light->equirect_radiance_gputexture);
    }
    if (SPECULAR_HIGHLIGHT_ENABLED(wpd) || MATCAP_ENABLED(wpd)) {
      DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
    }
    if (SHADOW_ENABLED(wpd)) {
      DRW_shgroup_uniform_float_copy(grp, "shadowMultiplier", wpd->shadow_multiplier);
      DRW_shgroup_uniform_float_copy(grp, "shadowShift", wpd->shadow_shift);
      DRW_shgroup_uniform_float_copy(grp, "shadowFocus", wpd->shadow_focus);
    }

    workbench_material_shgroup_uniform(wpd, grp, material, ob, false, false, interp);
    material->shgrp = grp;

    /* Depth */
    if (workbench_material_determine_color_type(wpd, material->ima, ob, is_sculpt_mode) ==
        V3D_SHADING_TEXTURE_COLOR) {
      material->shgrp_object_outline = DRW_shgroup_create(sh_data->object_outline_texture_sh,
                                                          psl->object_outline_pass);
      GPUTexture *tex = GPU_texture_from_blender(material->ima, material->iuser, GL_TEXTURE_2D);
      DRW_shgroup_uniform_texture(material->shgrp_object_outline, "image", tex);
    }
    else {
      material->shgrp_object_outline = DRW_shgroup_create(sh_data->object_outline_sh,
                                                          psl->object_outline_pass);
    }
    material->object_id = engine_object_data->object_id;
    DRW_shgroup_uniform_int(material->shgrp_object_outline, "object_id", &material->object_id, 1);
    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_state_enable(material->shgrp_object_outline, DRW_STATE_CLIP_PLANES);
    }
    BLI_ghash_insert(wpd->material_transp_hash, POINTER_FROM_UINT(hash), material);
  }
  return material;
}

static GPUShader *ensure_forward_accum_shaders(WORKBENCH_PrivateData *wpd,
                                               bool is_uniform_color,
                                               bool is_hair,
                                               eGPUShaderConfig sh_cfg)
{
  WORKBENCH_FORWARD_Shaders *sh_data = &e_data.sh_data[sh_cfg];
  int index = workbench_material_get_accum_shader_index(wpd, is_uniform_color, is_hair);
  if (sh_data->transparent_accum_sh_cache[index] == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    char *defines = workbench_material_build_defines(wpd, is_uniform_color, is_hair);
    char *transparent_accum_vert = workbench_build_forward_vert(is_hair);
    char *transparent_accum_frag = workbench_build_forward_transparent_accum_frag();
    sh_data->transparent_accum_sh_cache[index] = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, transparent_accum_vert, NULL},
        .frag = (const char *[]){transparent_accum_frag, NULL},
        .defs = (const char *[]){sh_cfg_data->def, defines, NULL},
    });
    MEM_freeN(transparent_accum_vert);
    MEM_freeN(transparent_accum_frag);
    MEM_freeN(defines);
  }
  return sh_data->transparent_accum_sh_cache[index];
}

static GPUShader *ensure_forward_composite_shaders(WORKBENCH_PrivateData *wpd)
{
  int index = OBJECT_OUTLINE_ENABLED(wpd) ? 1 : 0;
  if (e_data.composite_sh_cache[index] == NULL) {
    char *defines = workbench_material_build_defines(wpd, false, false);
    char *composite_frag = workbench_build_forward_composite_frag();
    e_data.composite_sh_cache[index] = DRW_shader_create_fullscreen(composite_frag, defines);
    MEM_freeN(composite_frag);
    MEM_freeN(defines);
  }
  return e_data.composite_sh_cache[index];
}

void workbench_forward_choose_shaders(WORKBENCH_PrivateData *wpd, eGPUShaderConfig sh_cfg)
{
  wpd->composite_sh = ensure_forward_composite_shaders(wpd);
  wpd->transparent_accum_sh = ensure_forward_accum_shaders(wpd, false, false, sh_cfg);
  wpd->transparent_accum_hair_sh = ensure_forward_accum_shaders(wpd, false, true, sh_cfg);
  wpd->transparent_accum_uniform_sh = ensure_forward_accum_shaders(wpd, true, false, sh_cfg);
  wpd->transparent_accum_uniform_hair_sh = ensure_forward_accum_shaders(wpd, true, true, sh_cfg);
}

void workbench_forward_outline_shaders_ensure(WORKBENCH_PrivateData *wpd, eGPUShaderConfig sh_cfg)
{
  WORKBENCH_FORWARD_Shaders *sh_data = &e_data.sh_data[sh_cfg];

  if (sh_data->object_outline_sh == NULL) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    char *defines = workbench_material_build_defines(wpd, false, false);
    char *defines_texture = workbench_material_build_defines(wpd, true, false);
    char *defines_hair = workbench_material_build_defines(wpd, false, true);
    char *forward_vert = workbench_build_forward_vert(false);
    char *forward_hair_vert = workbench_build_forward_vert(true);

    sh_data->object_outline_sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, forward_vert, NULL},
        .frag = (const char *[]){datatoc_workbench_forward_depth_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, defines, NULL},
    });
    sh_data->object_outline_texture_sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, forward_vert, NULL},
        .frag = (const char *[]){datatoc_workbench_forward_depth_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, defines_texture, NULL},
    });
    sh_data->object_outline_hair_sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib, forward_hair_vert, NULL},
        .frag = (const char *[]){datatoc_workbench_forward_depth_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, defines_hair, NULL},
    });

    MEM_freeN(forward_hair_vert);
    MEM_freeN(forward_vert);
    MEM_freeN(defines);
    MEM_freeN(defines_texture);
    MEM_freeN(defines_hair);
  }
}

/* public functions */
void workbench_forward_engine_init(WORKBENCH_Data *vedata)
{
  WORKBENCH_FramebufferList *fbl = vedata->fbl;
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_StorageList *stl = vedata->stl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  DRWShadingGroup *grp;

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }
  if (!stl->effects) {
    stl->effects = MEM_callocN(sizeof(*stl->effects), __func__);
    workbench_effect_info_init(stl->effects);
  }
  WORKBENCH_PrivateData *wpd = stl->g_data;
  workbench_private_data_init(wpd);
  float light_direction[3];
  workbench_private_data_get_light_direction(wpd, light_direction);

  if (!e_data.checker_depth_sh) {
    e_data.checker_depth_sh = DRW_shader_create_fullscreen(
        datatoc_workbench_checkerboard_depth_frag_glsl, NULL);
  }

  workbench_forward_outline_shaders_ensure(wpd, draw_ctx->sh_cfg);

  workbench_volume_engine_init();
  workbench_fxaa_engine_init();
  workbench_taa_engine_init(vedata);

  workbench_forward_outline_shaders_ensure(wpd, draw_ctx->sh_cfg);
  workbench_forward_choose_shaders(wpd, draw_ctx->sh_cfg);

  const float *viewport_size = DRW_viewport_size_get();
  const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};
  const eGPUTextureFormat comp_tex_format = DRW_state_is_image_render() ? GPU_RGBA16F :
                                                                          GPU_R11F_G11F_B10F;

  e_data.object_id_tx = DRW_texture_pool_query_2d(
      size[0], size[1], GPU_R32UI, &draw_engine_workbench_transparent);
  e_data.transparent_accum_tx = DRW_texture_pool_query_2d(
      size[0], size[1], GPU_RGBA16F, &draw_engine_workbench_transparent);
  e_data.transparent_revealage_tx = DRW_texture_pool_query_2d(
      size[0], size[1], GPU_R16F, &draw_engine_workbench_transparent);
  e_data.composite_buffer_tx = DRW_texture_pool_query_2d(
      size[0], size[1], comp_tex_format, &draw_engine_workbench_transparent);

  GPU_framebuffer_ensure_config(&fbl->object_outline_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(e_data.object_id_tx),
                                });
  GPU_framebuffer_ensure_config(&fbl->transparent_accum_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(e_data.transparent_accum_tx),
                                    GPU_ATTACHMENT_TEXTURE(e_data.transparent_revealage_tx),
                                });
  GPU_framebuffer_ensure_config(&fbl->composite_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(e_data.composite_buffer_tx),
                                });
  GPU_framebuffer_ensure_config(&fbl->effect_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(e_data.transparent_accum_tx),
                                });

  workbench_volume_cache_init(vedata);
  const bool do_cull = CULL_BACKFACE_ENABLED(wpd);
  const int cull_state = (do_cull) ? DRW_STATE_CULL_BACK : 0;

  /* Transparency Accum */
  {
    int state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_OIT | cull_state;
    psl->transparent_accum_pass = DRW_pass_create("Transparent Accum", state);
  }
  /* Depth */
  {
    int state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | cull_state;
    psl->object_outline_pass = DRW_pass_create("Object Outline Pass", state);
  }
  /* Composite */
  {
    int state = DRW_STATE_WRITE_COLOR;
    psl->composite_pass = DRW_pass_create("Composite", state);

    grp = DRW_shgroup_create(wpd->composite_sh, psl->composite_pass);
    if (OBJECT_ID_PASS_ENABLED(wpd)) {
      DRW_shgroup_uniform_texture_ref(grp, "objectId", &e_data.object_id_tx);
    }
    DRW_shgroup_uniform_texture_ref(grp, "transparentAccum", &e_data.transparent_accum_tx);
    DRW_shgroup_uniform_texture_ref(grp, "transparentRevealage", &e_data.transparent_revealage_tx);
    DRW_shgroup_uniform_block(grp, "world_block", wpd->world_ubo);
    DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
  }

  /* TODO(campbell): displays but masks geometry,
   * only use with wire or solid-without-xray for now. */
  if ((wpd->shading.type != OB_WIRE && !XRAY_FLAG_ENABLED(wpd)) &&
      (draw_ctx->rv3d && (draw_ctx->rv3d->rflag & RV3D_CLIPPING) && draw_ctx->rv3d->clipbb)) {
    psl->background_pass = DRW_pass_create("Background",
                                           DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);
    GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR_BACKGROUND);
    grp = DRW_shgroup_create(shader, psl->background_pass);
    wpd->world_clip_planes_batch = DRW_draw_background_clipping_batch_from_rv3d(draw_ctx->rv3d);
    DRW_shgroup_call(grp, wpd->world_clip_planes_batch, NULL);
    DRW_shgroup_uniform_vec4(grp, "color", &wpd->world_clip_planes_color[0], 1);
  }

  {
    workbench_aa_create_pass(vedata, &e_data.transparent_accum_tx);
  }

  /* Checker Depth */
  {
    static float noise_offset = 0.0f;
    float blend_threshold = 0.0f;

    if (DRW_state_is_image_render()) {
      /* TODO: Should be based on the number of samples used for render. */
      noise_offset = fmodf(noise_offset + 1.0f / 8.0f, 1.0f);
    }

    if (XRAY_ENABLED(wpd)) {
      blend_threshold = 1.0f - XRAY_ALPHA(wpd) * 0.9f;
    }

    if (wpd->shading.type == OB_WIRE) {
      wpd->shading.xray_alpha = 0.0f;
      wpd->shading.xray_alpha_wire = 0.0f;
    }

    int state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS;
    psl->checker_depth_pass = DRW_pass_create("Checker Depth", state);
    grp = DRW_shgroup_create(e_data.checker_depth_sh, psl->checker_depth_pass);
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
    DRW_shgroup_uniform_float_copy(grp, "threshold", blend_threshold);
    DRW_shgroup_uniform_float_copy(grp, "offset", noise_offset);
  }
}

void workbench_forward_engine_free()
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    WORKBENCH_FORWARD_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    for (int index = 0; index < MAX_ACCUM_SHADERS; index++) {
      DRW_SHADER_FREE_SAFE(sh_data->transparent_accum_sh_cache[index]);
    }
    DRW_SHADER_FREE_SAFE(sh_data->object_outline_sh);
    DRW_SHADER_FREE_SAFE(sh_data->object_outline_texture_sh);
    DRW_SHADER_FREE_SAFE(sh_data->object_outline_hair_sh);
  }

  for (int index = 0; index < 2; index++) {
    DRW_SHADER_FREE_SAFE(e_data.composite_sh_cache[index]);
  }
  DRW_SHADER_FREE_SAFE(e_data.checker_depth_sh);

  workbench_volume_engine_free();
  workbench_fxaa_engine_free();
  workbench_taa_engine_free();
  workbench_dof_engine_free();
}

void workbench_forward_cache_init(WORKBENCH_Data *UNUSED(vedata))
{
}

static void workbench_forward_cache_populate_particles(WORKBENCH_Data *vedata, Object *ob)
{
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_PrivateData *wpd = stl->g_data;

  for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
    if (md->type != eModifierType_ParticleSystem) {
      continue;
    }
    ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;
    if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
      continue;
    }
    ParticleSettings *part = psys->part;
    const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

    if (draw_as == PART_DRAW_PATH) {
      const DRWContextState *draw_ctx = DRW_context_state_get();
      Material *mat;
      Image *image;
      ImageUser *iuser;
      int interp;
      workbench_material_get_image_and_mat(ob, part->omat, &image, &iuser, &interp, &mat);
      int color_type = workbench_material_determine_color_type(wpd, image, ob, false);
      WORKBENCH_MaterialData *material = workbench_forward_get_or_create_material_data(
          vedata, ob, mat, image, iuser, color_type, interp, false);

      struct GPUShader *shader = (wpd->shading.color_type == color_type) ?
                                     wpd->transparent_accum_hair_sh :
                                     wpd->transparent_accum_uniform_hair_sh;
      DRWShadingGroup *shgrp = DRW_shgroup_hair_create(
          ob, psys, md, psl->transparent_accum_pass, shader);
      DRW_shgroup_uniform_block(shgrp, "world_block", wpd->world_ubo);
      workbench_material_shgroup_uniform(wpd, shgrp, material, ob, false, false, interp);
      DRW_shgroup_uniform_vec4(shgrp, "viewvecs[0]", (float *)wpd->viewvecs, 3);
      /* Hairs have lots of layer and can rapidly become the most prominent surface.
       * So lower their alpha artificially. */
      float hair_alpha = XRAY_ALPHA(wpd) * 0.33f;
      DRW_shgroup_uniform_float_copy(shgrp, "alpha", hair_alpha);
      if (STUDIOLIGHT_TYPE_MATCAP_ENABLED(wpd)) {
        BKE_studiolight_ensure_flag(wpd->studio_light, STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE);
        DRW_shgroup_uniform_texture(
            shgrp, "matcapImage", wpd->studio_light->equirect_radiance_gputexture);
      }
      if (SPECULAR_HIGHLIGHT_ENABLED(wpd) || MATCAP_ENABLED(wpd)) {
        DRW_shgroup_uniform_vec2(shgrp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
      }

      WORKBENCH_FORWARD_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
      shgrp = DRW_shgroup_hair_create(
          ob, psys, md, vedata->psl->object_outline_pass, sh_data->object_outline_hair_sh);
      DRW_shgroup_uniform_int(shgrp, "object_id", &material->object_id, 1);
    }
  }
}

void workbench_forward_cache_populate(WORKBENCH_Data *vedata, Object *ob)
{
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PrivateData *wpd = stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  const bool is_wire = (ob->dt == OB_WIRE);

  if (!DRW_object_is_renderable(ob)) {
    return;
  }

  if (ob->type == OB_MESH) {
    workbench_forward_cache_populate_particles(vedata, ob);
  }

  ModifierData *md;
  if (((ob->base_flag & BASE_FROM_DUPLI) == 0) &&
      (md = modifiers_findByType(ob, eModifierType_Smoke)) &&
      (modifier_isEnabled(scene, md, eModifierMode_Realtime)) &&
      (((SmokeModifierData *)md)->domain != NULL)) {
    workbench_volume_cache_populate(vedata, scene, ob, md);
    return; /* Do not draw solid in this case. */
  }

  if (!(DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF)) {
    return;
  }
  if (ob->dt < OB_WIRE) {
    return;
  }

  WORKBENCH_MaterialData *material;
  if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)) {
    const bool is_sculpt_mode = DRW_object_use_pbvh_drawing(ob);
    const int materials_len = MAX2(1, ob->totcol);
    const Mesh *me = (ob->type == OB_MESH) ? ob->data : NULL;

    if (!is_sculpt_mode && TEXTURE_DRAWING_ENABLED(wpd) && me && me->mloopuv) {
      struct GPUBatch **geom_array = DRW_cache_mesh_surface_texpaint_get(ob);
      for (int i = 0; i < materials_len; i++) {
        Material *mat;
        Image *image;
        ImageUser *iuser;
        int interp;
        workbench_material_get_image_and_mat(ob, i + 1, &image, &iuser, &interp, &mat);
        int color_type = workbench_material_determine_color_type(wpd, image, ob, is_sculpt_mode);
        material = workbench_forward_get_or_create_material_data(
            vedata, ob, mat, image, iuser, color_type, interp, is_sculpt_mode);
        DRW_shgroup_call_object(material->shgrp_object_outline, geom_array[i], ob);
        DRW_shgroup_call_object(material->shgrp, geom_array[i], ob);
      }
    }
    else if (ELEM(wpd->shading.color_type,
                  V3D_SHADING_SINGLE_COLOR,
                  V3D_SHADING_OBJECT_COLOR,
                  V3D_SHADING_RANDOM_COLOR,
                  V3D_SHADING_VERTEX_COLOR)) {
      /* No material split needed */
      int color_type = workbench_material_determine_color_type(wpd, NULL, ob, is_sculpt_mode);

      if (is_sculpt_mode) {
        material = workbench_forward_get_or_create_material_data(
            vedata, ob, NULL, NULL, NULL, color_type, 0, is_sculpt_mode);
        bool use_vcol = (color_type == V3D_SHADING_VERTEX_COLOR);
        /* TODO(fclem) make this call optional */
        DRW_shgroup_call_sculpt(material->shgrp_object_outline, ob, false, false, false);
        if (!is_wire) {
          DRW_shgroup_call_sculpt(material->shgrp, ob, false, false, use_vcol);
        }
      }
      else {
        struct GPUBatch *geom = (color_type == V3D_SHADING_VERTEX_COLOR) ?
                                    DRW_cache_mesh_surface_vertpaint_get(ob) :
                                    DRW_cache_object_surface_get(ob);
        if (geom) {
          material = workbench_forward_get_or_create_material_data(
              vedata, ob, NULL, NULL, NULL, color_type, 0, is_sculpt_mode);
          /* TODO(fclem) make this call optional */
          DRW_shgroup_call_object(material->shgrp_object_outline, geom, ob);
          if (!is_wire) {
            DRW_shgroup_call_object(material->shgrp, geom, ob);
          }
        }
      }
    }
    else {
      /* Draw material color */
      if (is_sculpt_mode) {
        struct DRWShadingGroup **shgrps = BLI_array_alloca(shgrps, materials_len);

        for (int i = 0; i < materials_len; ++i) {
          struct Material *mat = give_current_material(ob, i + 1);
          material = workbench_forward_get_or_create_material_data(
              vedata, ob, mat, NULL, NULL, V3D_SHADING_MATERIAL_COLOR, 0, is_sculpt_mode);
          shgrps[i] = material->shgrp;
        }
        /* TODO(fclem) make this call optional */
        DRW_shgroup_call_sculpt(material->shgrp_object_outline, ob, false, false, false);
        if (!is_wire) {
          DRW_shgroup_call_sculpt_with_materials(shgrps, ob, false);
        }
      }
      else {
        struct GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
        memset(gpumat_array, 0, sizeof(*gpumat_array) * materials_len);

        struct GPUBatch **mat_geom = DRW_cache_object_surface_material_get(
            ob, gpumat_array, materials_len, NULL, NULL, NULL);
        if (mat_geom) {
          for (int i = 0; i < materials_len; ++i) {
            if (mat_geom[i] == NULL) {
              continue;
            }

            Material *mat = give_current_material(ob, i + 1);
            material = workbench_forward_get_or_create_material_data(
                vedata, ob, mat, NULL, NULL, V3D_SHADING_MATERIAL_COLOR, 0, is_sculpt_mode);
            /* TODO(fclem) make this call optional */
            DRW_shgroup_call_object(material->shgrp_object_outline, mat_geom[i], ob);
            if (!is_wire) {
              DRW_shgroup_call_object(material->shgrp, mat_geom[i], ob);
            }
          }
        }
      }
    }
  }
}

void workbench_forward_cache_finish(WORKBENCH_Data *UNUSED(vedata))
{
}

void workbench_forward_draw_background(WORKBENCH_Data *UNUSED(vedata))
{
  const float clear_depth = 1.0f;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DRW_stats_group_start("Clear depth");
  GPU_framebuffer_bind(dfbl->default_fb);
  GPU_framebuffer_clear_depth(dfbl->default_fb, clear_depth);
  DRW_stats_group_end();
}

void workbench_forward_draw_scene(WORKBENCH_Data *vedata)
{
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_FramebufferList *fbl = vedata->fbl;
  WORKBENCH_PrivateData *wpd = stl->g_data;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (workbench_is_taa_enabled(wpd)) {
    workbench_taa_draw_scene_start(vedata);
  }

  /* Write Depth + Object ID */
  const float clear_outline[4] = {0.0f};
  GPU_framebuffer_bind(fbl->object_outline_fb);
  GPU_framebuffer_clear_color(fbl->object_outline_fb, clear_outline);
  DRW_draw_pass(psl->object_outline_pass);

  if (XRAY_ALPHA(wpd) > 0.0) {
    const float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    GPU_framebuffer_bind(fbl->transparent_accum_fb);
    GPU_framebuffer_clear_color(fbl->transparent_accum_fb, clear_color);
    DRW_draw_pass(psl->transparent_accum_pass);
  }
  else {
    /* TODO(fclem): this is unnecessary and takes up perf.
     * Better change the composite frag shader to not use the tx. */
    const float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    GPU_framebuffer_bind(fbl->transparent_accum_fb);
    GPU_framebuffer_clear_color(fbl->transparent_accum_fb, clear_color);
  }

  /* Composite */
  GPU_framebuffer_bind(fbl->composite_fb);
  DRW_draw_pass(psl->composite_pass);
  DRW_draw_pass(psl->volume_pass);

  /* Only when clipping is enabled. */
  if (psl->background_pass) {
    DRW_draw_pass(psl->background_pass);
  }

  /* Color correct and Anti aliasing */
  workbench_aa_draw_pass(vedata, e_data.composite_buffer_tx);

  /* Apply checker pattern */
  GPU_framebuffer_bind(dfbl->depth_only_fb);
  DRW_draw_pass(psl->checker_depth_pass);
}

void workbench_forward_draw_finish(WORKBENCH_Data *vedata)
{
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PrivateData *wpd = stl->g_data;

  workbench_private_data_free(wpd);
  workbench_volume_smoke_textures_free(wpd);
}
