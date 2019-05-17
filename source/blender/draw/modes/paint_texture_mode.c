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
 * \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "BIF_gl.h"

#include "BKE_node.h"

#include "BLI_string_utils.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

#include "DNA_mesh_types.h"

#include "DEG_depsgraph_query.h"

extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_paint_texture_vert_glsl[];
extern char datatoc_paint_texture_frag_glsl[];
extern char datatoc_paint_wire_vert_glsl[];
extern char datatoc_paint_wire_frag_glsl[];
extern char datatoc_paint_face_vert_glsl[];

extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use PAINT_TEXTURE_engine_init() to
 * initialize most of them and PAINT_TEXTURE_cache_init()
 * for PAINT_TEXTURE_PassList */

typedef struct PAINT_TEXTURE_PassList {
  /* Declare all passes here and init them in
   * PAINT_TEXTURE_cache_init().
   * Only contains (DRWPass *) */
  struct DRWPass *image_faces;

  struct DRWPass *wire_select_overlay;
  struct DRWPass *face_select_overlay;
} PAINT_TEXTURE_PassList;

typedef struct PAINT_TEXTURE_FramebufferList {
  /* Contains all framebuffer objects needed by this engine.
   * Only contains (GPUFrameBuffer *) */
  struct GPUFrameBuffer *fb;
} PAINT_TEXTURE_FramebufferList;

typedef struct PAINT_TEXTURE_TextureList {
  /* Contains all framebuffer textures / utility textures
   * needed by this engine. Only viewport specific textures
   * (not per object). Only contains (GPUTexture *) */
  struct GPUTexture *texture;
} PAINT_TEXTURE_TextureList;

typedef struct PAINT_TEXTURE_StorageList {
  /* Contains any other memory block that the engine needs.
   * Only directly MEM_(m/c)allocN'ed blocks because they are
   * free with MEM_freeN() when viewport is freed.
   * (not per object) */
  struct CustomStruct *block;
  struct PAINT_TEXTURE_PrivateData *g_data;
} PAINT_TEXTURE_StorageList;

typedef struct PAINT_TEXTURE_Data {
  /* Struct returned by DRW_viewport_engine_data_ensure.
   * If you don't use one of these, just make it a (void *) */
  // void *fbl;
  void *engine_type; /* Required */
  PAINT_TEXTURE_FramebufferList *fbl;
  PAINT_TEXTURE_TextureList *txl;
  PAINT_TEXTURE_PassList *psl;
  PAINT_TEXTURE_StorageList *stl;
} PAINT_TEXTURE_Data;

typedef struct PAINT_TEXTURE_Shaders {
  /* Custom shaders :
   * Add sources to source/blender/draw/modes/shaders
   * init in PAINT_TEXTURE_engine_init();
   * free in PAINT_TEXTURE_engine_free(); */
  struct GPUShader *fallback;
  struct GPUShader *image;
  struct GPUShader *image_mask;

  struct GPUShader *wire_select_overlay;
  struct GPUShader *face_select_overlay;
} PAINT_TEXTURE_Shaders;

/* *********** STATIC *********** */

static struct {
  PAINT_TEXTURE_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{NULL}}}; /* Engine data */

typedef struct PAINT_TEXTURE_PrivateData {
  /* This keeps the references of the shading groups for
   * easy access in PAINT_TEXTURE_cache_populate() */
  DRWShadingGroup *shgroup_fallback;
  DRWShadingGroup **shgroup_image_array;

  /* face-mask  */
  DRWShadingGroup *lwire_select_shgrp;
  DRWShadingGroup *face_select_shgrp;
} PAINT_TEXTURE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Init Textures, Framebuffers, Storage and Shaders.
 * It is called for every frames. */
static void PAINT_TEXTURE_engine_init(void *UNUSED(vedata))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  PAINT_TEXTURE_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  if (!sh_data->fallback) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[draw_ctx->sh_cfg];
    sh_data->fallback = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_3D_UNIFORM_COLOR,
                                                                  draw_ctx->sh_cfg);

    sh_data->image = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_texture_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_paint_texture_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });

    sh_data->wire_select_overlay = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_wire_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_paint_wire_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define VERTEX_MODE\n", NULL},
    });

    sh_data->face_select_overlay = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_paint_face_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_common_view_lib_glsl,
                                 datatoc_gpu_shader_uniform_color_frag_glsl,
                                 NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }
}

static DRWShadingGroup *create_texture_paint_shading_group(PAINT_TEXTURE_PassList *psl,
                                                           const struct GPUTexture *texture,
                                                           const DRWContextState *draw_ctx,
                                                           const bool nearest_interp)
{
  PAINT_TEXTURE_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  Scene *scene = draw_ctx->scene;
  const ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;
  const bool masking_enabled = imapaint->flag & IMAGEPAINT_PROJECT_LAYER_STENCIL &&
                               imapaint->stencil != NULL;

  DRWShadingGroup *grp = DRW_shgroup_create(masking_enabled ? sh_data->image_mask : sh_data->image,
                                            psl->image_faces);
  DRW_shgroup_uniform_texture(grp, "image", texture);
  DRW_shgroup_uniform_float(grp, "alpha", &draw_ctx->v3d->overlay.texture_paint_mode_opacity, 1);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_bool_copy(grp, "nearestInterp", nearest_interp);

  if (masking_enabled) {
    const bool masking_inverted = (imapaint->flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) > 0;
    GPUTexture *stencil = GPU_texture_from_blender(imapaint->stencil, NULL, GL_TEXTURE_2D);
    DRW_shgroup_uniform_texture(grp, "maskingImage", stencil);
    DRW_shgroup_uniform_vec3(grp, "maskingColor", imapaint->stencil_col, 1);
    DRW_shgroup_uniform_bool_copy(grp, "maskingInvertStencil", masking_inverted);
  }
  return grp;
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void PAINT_TEXTURE_cache_init(void *vedata)
{
  PAINT_TEXTURE_PassList *psl = ((PAINT_TEXTURE_Data *)vedata)->psl;
  PAINT_TEXTURE_StorageList *stl = ((PAINT_TEXTURE_Data *)vedata)->stl;

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
    stl->g_data->shgroup_image_array = NULL;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  PAINT_TEXTURE_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  /* Create a pass */
  {
    DRWPass *pass = DRW_pass_create(
        "Image Color Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND);
    DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->fallback, pass);

    /* Uniforms need a pointer to it's value so be sure it's accessible at
     * any given time (i.e. use static vars) */
    static float color[4] = {1.0f, 0.0f, 1.0f, 1.0};
    DRW_shgroup_uniform_vec4(shgrp, "color", color, 1);

    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_world_clip_planes_from_rv3d(shgrp, draw_ctx->rv3d);
    }
    psl->image_faces = pass;
    stl->g_data->shgroup_fallback = shgrp;
  }

  MEM_SAFE_FREE(stl->g_data->shgroup_image_array);

  Object *ob = draw_ctx->obact;
  if (ob && ob->type == OB_MESH) {
    Scene *scene = draw_ctx->scene;
    const ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;
    const bool use_material_slots = (imapaint->mode == IMAGEPAINT_MODE_MATERIAL);
    const Mesh *me = ob->data;
    const int mat_nr = max_ii(1, me->totcol);

    stl->g_data->shgroup_image_array = MEM_mallocN(
        sizeof(*stl->g_data->shgroup_image_array) * (use_material_slots ? mat_nr : 1), __func__);

    if (use_material_slots) {
      for (int i = 0; i < mat_nr; i++) {
        Material *ma = give_current_material(ob, i + 1);
        Image *ima = (ma && ma->texpaintslot) ? ma->texpaintslot[ma->paint_active_slot].ima : NULL;
        int interp = (ma && ma->texpaintslot) ? ma->texpaintslot[ma->paint_active_slot].interp : 0;
        GPUTexture *tex = GPU_texture_from_blender(ima, NULL, GL_TEXTURE_2D);

        if (tex) {
          DRWShadingGroup *grp = create_texture_paint_shading_group(
              psl, tex, draw_ctx, interp == SHD_INTERP_CLOSEST);
          stl->g_data->shgroup_image_array[i] = grp;
        }
        else {
          stl->g_data->shgroup_image_array[i] = NULL;
        }
      }
    }
    else {
      Image *ima = imapaint->canvas;
      GPUTexture *tex = GPU_texture_from_blender(ima, NULL, GL_TEXTURE_2D);

      if (tex) {
        DRWShadingGroup *grp = create_texture_paint_shading_group(
            psl, tex, draw_ctx, imapaint->interp == IMAGEPAINT_INTERP_CLOSEST);
        stl->g_data->shgroup_image_array[0] = grp;
      }
      else {
        stl->g_data->shgroup_image_array[0] = NULL;
      }
    }
  }

  /* Face Mask */
  {
    DRWPass *pass = DRW_pass_create("Wire Mask Pass",
                                    (DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                     DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_OFFSET_NEGATIVE));
    DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->wire_select_overlay, pass);

    DRW_shgroup_uniform_block(shgrp, "globalsBlock", G_draw.block_ubo);

    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_world_clip_planes_from_rv3d(shgrp, draw_ctx->rv3d);
    }
    psl->wire_select_overlay = pass;
    stl->g_data->lwire_select_shgrp = shgrp;
  }

  {

    DRWPass *pass = DRW_pass_create("Face Mask Pass",
                                    DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                        DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND);
    DRWShadingGroup *shgrp = DRW_shgroup_create(sh_data->face_select_overlay, pass);
    static float col[4] = {1.0f, 1.0f, 1.0f, 0.2f};
    DRW_shgroup_uniform_vec4(shgrp, "color", col, 1);

    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_world_clip_planes_from_rv3d(shgrp, draw_ctx->rv3d);
    }
    psl->face_select_overlay = pass;
    stl->g_data->face_select_shgrp = shgrp;
  }
}

/* Add geometry to shadingGroups. Execute for each objects */
static void PAINT_TEXTURE_cache_populate(void *vedata, Object *ob)
{
  PAINT_TEXTURE_PassList *psl = ((PAINT_TEXTURE_Data *)vedata)->psl;
  PAINT_TEXTURE_StorageList *stl = ((PAINT_TEXTURE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  UNUSED_VARS(psl, stl);

  if ((ob->type == OB_MESH) && (draw_ctx->obact == ob)) {
    /* Get geometry cache */
    const Mesh *me = ob->data;
    const Mesh *me_orig = DEG_get_original_object(ob)->data;
    Scene *scene = draw_ctx->scene;
    const bool use_surface = draw_ctx->v3d->overlay.texture_paint_mode_opacity !=
                             0.0;  // DRW_object_is_mode_shade(ob) == true;
    const bool use_material_slots = (scene->toolsettings->imapaint.mode ==
                                     IMAGEPAINT_MODE_MATERIAL);
    const bool use_face_sel = (me_orig->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

    if (use_surface) {
      if (me->mloopuv != NULL) {
        if (use_material_slots) {
          int mat_nr = max_ii(1, me->totcol);
          struct GPUBatch **geom_array = DRW_cache_mesh_surface_texpaint_get(ob);

          for (int i = 0; i < mat_nr; i++) {
            const int index = use_material_slots ? i : 0;
            if ((i < me->totcol) && stl->g_data->shgroup_image_array[index]) {
              DRW_shgroup_call(stl->g_data->shgroup_image_array[index], geom_array[i], ob->obmat);
            }
            else {
              DRW_shgroup_call(stl->g_data->shgroup_fallback, geom_array[i], ob->obmat);
            }
          }
        }
        else {
          if (stl->g_data->shgroup_image_array[0]) {
            struct GPUBatch *geom = DRW_cache_mesh_surface_texpaint_single_get(ob);
            DRW_shgroup_call(stl->g_data->shgroup_image_array[0], geom, ob->obmat);
          }
        }
      }
      else {
        struct GPUBatch *geom = DRW_cache_mesh_surface_get(ob);
        DRW_shgroup_call(stl->g_data->shgroup_fallback, geom, ob->obmat);
      }
    }

    /* Face Mask */
    if (use_face_sel) {
      struct GPUBatch *geom;
      geom = DRW_cache_mesh_surface_edges_get(ob);
      DRW_shgroup_call(stl->g_data->lwire_select_shgrp, geom, ob->obmat);

      geom = DRW_cache_mesh_surface_get(ob);
      DRW_shgroup_call(stl->g_data->face_select_shgrp, geom, ob->obmat);
    }
  }
}

/* Optional: Post-cache_populate callback */
static void PAINT_TEXTURE_cache_finish(void *vedata)
{
  PAINT_TEXTURE_PassList *psl = ((PAINT_TEXTURE_Data *)vedata)->psl;
  PAINT_TEXTURE_StorageList *stl = ((PAINT_TEXTURE_Data *)vedata)->stl;

  /* Do something here! dependent on the objects gathered */
  UNUSED_VARS(psl);

  MEM_SAFE_FREE(stl->g_data->shgroup_image_array);
}

/* Draw time ! Control rendering pipeline from here */
static void PAINT_TEXTURE_draw_scene(void *vedata)
{
  PAINT_TEXTURE_PassList *psl = ((PAINT_TEXTURE_Data *)vedata)->psl;
  PAINT_TEXTURE_FramebufferList *fbl = ((PAINT_TEXTURE_Data *)vedata)->fbl;

  /* Default framebuffer and texture */
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  UNUSED_VARS(fbl, dfbl, dtxl);

  DRW_draw_pass(psl->image_faces);

  DRW_draw_pass(psl->face_select_overlay);
  DRW_draw_pass(psl->wire_select_overlay);
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void PAINT_TEXTURE_engine_free(void)
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    PAINT_TEXTURE_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    /* Don't free builtins. */
    sh_data->fallback = NULL;
    GPUShader **sh_data_as_array = (GPUShader **)sh_data;
    for (int i = 0; i < (sizeof(PAINT_TEXTURE_Shaders) / sizeof(GPUShader *)); i++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
    }
  }
}

static const DrawEngineDataSize PAINT_TEXTURE_data_size = DRW_VIEWPORT_DATA_SIZE(
    PAINT_TEXTURE_Data);

DrawEngineType draw_engine_paint_texture_type = {
    NULL,
    NULL,
    N_("PaintTextureMode"),
    &PAINT_TEXTURE_data_size,
    &PAINT_TEXTURE_engine_init,
    &PAINT_TEXTURE_engine_free,
    &PAINT_TEXTURE_cache_init,
    &PAINT_TEXTURE_cache_populate,
    &PAINT_TEXTURE_cache_finish,
    NULL, /* draw_background but not needed by mode engines */
    &PAINT_TEXTURE_draw_scene,
    NULL,
    NULL,
};
