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

#include "DNA_curve_types.h"
#include "DNA_view3d_types.h"

#include "BKE_object.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

/* If needed, contains all global/Theme colors
 * Add needed theme colors / values to DRW_globals_update() and update UBO
 * Not needed for constant color. */

extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_edit_curve_overlay_loosevert_vert_glsl[];
extern char datatoc_edit_curve_overlay_normals_vert_glsl[];
extern char datatoc_edit_curve_overlay_handle_vert_glsl[];
extern char datatoc_edit_curve_overlay_handle_geom_glsl[];

extern char datatoc_gpu_shader_point_varying_color_frag_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];
extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use EDIT_CURVE_engine_init() to
 * initialize most of them and EDIT_CURVE_cache_init()
 * for EDIT_CURVE_PassList */

typedef struct EDIT_CURVE_PassList {
  struct DRWPass *wire_pass;
  struct DRWPass *wire_pass_xray;
  struct DRWPass *overlay_edge_pass;
  struct DRWPass *overlay_vert_pass;
} EDIT_CURVE_PassList;

typedef struct EDIT_CURVE_StorageList {
  struct CustomStruct *block;
  struct EDIT_CURVE_PrivateData *g_data;
} EDIT_CURVE_StorageList;

typedef struct EDIT_CURVE_Data {
  void *engine_type; /* Required */
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  EDIT_CURVE_PassList *psl;
  EDIT_CURVE_StorageList *stl;
} EDIT_CURVE_Data;

/* *********** STATIC *********** */

typedef struct EDIT_CURVE_Shaders {
  GPUShader *wire_sh;
  GPUShader *wire_normals_sh;
  GPUShader *overlay_edge_sh; /* handles and nurbs control cage */
  GPUShader *overlay_vert_sh;
} EDIT_CURVE_Shaders;

static struct {
  EDIT_CURVE_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{NULL}}}; /* Engine data */

typedef struct EDIT_CURVE_PrivateData {
  /* resulting curve as 'wire' for curves (and optionally normals) */
  DRWShadingGroup *wire_shgrp;
  DRWShadingGroup *wire_shgrp_xray;
  DRWShadingGroup *wire_normals_shgrp;
  DRWShadingGroup *wire_normals_shgrp_xray;

  DRWShadingGroup *overlay_edge_shgrp;
  DRWShadingGroup *overlay_vert_shgrp;

  int show_handles;
} EDIT_CURVE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Init Textures, Framebuffers, Storage and Shaders.
 * It is called for every frames.
 * (Optional) */
static void EDIT_CURVE_engine_init(void *UNUSED(vedata))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EDIT_CURVE_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[draw_ctx->sh_cfg];

  if (!sh_data->wire_sh) {
    sh_data->wire_sh = GPU_shader_get_builtin_shader_with_config(GPU_SHADER_3D_UNIFORM_COLOR,
                                                                 draw_ctx->sh_cfg);
  }

  if (!sh_data->wire_normals_sh) {
    sh_data->wire_normals_sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_curve_overlay_normals_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  if (!sh_data->overlay_edge_sh) {
    sh_data->overlay_edge_sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_curve_overlay_handle_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_edit_curve_overlay_handle_geom_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_3D_smooth_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }

  if (!sh_data->overlay_vert_sh) {
    sh_data->overlay_vert_sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_globals_lib_glsl,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_edit_curve_overlay_loosevert_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_gpu_shader_point_varying_color_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }
}

static void EDIT_CURVE_wire_shgrp_create(EDIT_CURVE_Shaders *sh_data,
                                         const View3D *v3d,
                                         const RegionView3D *rv3d,
                                         DRWPass *pass,
                                         DRWShadingGroup **wire_shgrp,
                                         DRWShadingGroup **wire_normals_shgrp)
{
  DRWShadingGroup *grp = DRW_shgroup_create(sh_data->wire_sh, pass);
  DRW_shgroup_uniform_vec4(grp, "color", G_draw.block.colorWireEdit, 1);
  if (rv3d->rflag & RV3D_CLIPPING) {
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }
  *wire_shgrp = grp;

  grp = DRW_shgroup_create(sh_data->wire_normals_sh, pass);
  DRW_shgroup_uniform_vec4(grp, "color", G_draw.block.colorWireEdit, 1);
  DRW_shgroup_uniform_float_copy(grp, "normalSize", v3d->overlay.normals_length);
  if (rv3d->rflag & RV3D_CLIPPING) {
    DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
  }
  *wire_normals_shgrp = grp;
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void EDIT_CURVE_cache_init(void *vedata)
{
  EDIT_CURVE_PassList *psl = ((EDIT_CURVE_Data *)vedata)->psl;
  EDIT_CURVE_StorageList *stl = ((EDIT_CURVE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  const RegionView3D *rv3d = draw_ctx->rv3d;
  EDIT_CURVE_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }

  stl->g_data->show_handles = (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_CU_HANDLES) != 0;

  {
    DRWShadingGroup *grp;

    /* Center-Line (wire) */
    psl->wire_pass = DRW_pass_create(
        "Curve Wire", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);
    EDIT_CURVE_wire_shgrp_create(sh_data,
                                 v3d,
                                 rv3d,
                                 psl->wire_pass,
                                 &stl->g_data->wire_shgrp,
                                 &stl->g_data->wire_normals_shgrp);

    psl->wire_pass_xray = DRW_pass_create(
        "Curve Wire Xray", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
    EDIT_CURVE_wire_shgrp_create(sh_data,
                                 v3d,
                                 rv3d,
                                 psl->wire_pass_xray,
                                 &stl->g_data->wire_shgrp_xray,
                                 &stl->g_data->wire_normals_shgrp_xray);

    psl->overlay_edge_pass = DRW_pass_create("Curve Handle Overlay",
                                             DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);

    grp = DRW_shgroup_create(sh_data->overlay_edge_sh, psl->overlay_edge_pass);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
    DRW_shgroup_uniform_bool(grp, "showCurveHandles", &stl->g_data->show_handles, 1);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
    }
    stl->g_data->overlay_edge_shgrp = grp;

    psl->overlay_vert_pass = DRW_pass_create("Curve Vert Overlay", DRW_STATE_WRITE_COLOR);

    grp = DRW_shgroup_create(sh_data->overlay_vert_sh, psl->overlay_vert_pass);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    if (rv3d->rflag & RV3D_CLIPPING) {
      DRW_shgroup_state_enable(grp, DRW_STATE_CLIP_PLANES);
    }
    stl->g_data->overlay_vert_shgrp = grp;
  }
}

/* Add geometry to shadingGroups. Execute for each objects */
static void EDIT_CURVE_cache_populate(void *vedata, Object *ob)
{
  EDIT_CURVE_StorageList *stl = ((EDIT_CURVE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;

  if (ob->type == OB_CURVE) {
    if (BKE_object_is_in_editmode(ob)) {
      Curve *cu = ob->data;
      /* Get geometry cache */
      struct GPUBatch *geom;

      DRWShadingGroup *wire_shgrp, *wire_normals_shgrp;

      if (ob->dtx & OB_DRAWXRAY) {
        wire_shgrp = stl->g_data->wire_shgrp_xray;
        wire_normals_shgrp = stl->g_data->wire_normals_shgrp_xray;
      }
      else {
        wire_shgrp = stl->g_data->wire_shgrp;
        wire_normals_shgrp = stl->g_data->wire_normals_shgrp;
      }

      geom = DRW_cache_curve_edge_wire_get(ob);
      DRW_shgroup_call(wire_shgrp, geom, ob);

      if ((cu->flag & CU_3D) && (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_CU_NORMALS) != 0) {
        geom = DRW_cache_curve_edge_normal_get(ob);
        DRW_shgroup_call_instances(wire_normals_shgrp, ob, geom, 2);
      }

      geom = DRW_cache_curve_edge_overlay_get(ob);
      if (geom) {
        DRW_shgroup_call(stl->g_data->overlay_edge_shgrp, geom, ob);
      }

      geom = DRW_cache_curve_vert_overlay_get(ob, stl->g_data->show_handles);
      DRW_shgroup_call(stl->g_data->overlay_vert_shgrp, geom, ob);
    }
  }

  if (ob->type == OB_SURF) {
    if (BKE_object_is_in_editmode(ob)) {
      struct GPUBatch *geom = DRW_cache_curve_edge_overlay_get(ob);
      DRW_shgroup_call(stl->g_data->overlay_edge_shgrp, geom, ob);

      geom = DRW_cache_curve_vert_overlay_get(ob, false);
      DRW_shgroup_call(stl->g_data->overlay_vert_shgrp, geom, ob);
    }
  }
}

/* Draw time ! Control rendering pipeline from here */
static void EDIT_CURVE_draw_scene(void *vedata)
{
  EDIT_CURVE_PassList *psl = ((EDIT_CURVE_Data *)vedata)->psl;

  /* Default framebuffer and texture */
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (!DRW_pass_is_empty(psl->wire_pass)) {
    MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl);

    DRW_draw_pass(psl->wire_pass);

    MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl);
  }

  /* Unfortunately this pass cannot be AA'd without
   * MULTISAMPLE_SYNC_DISABLE_NO_DEPTH. While it's
   * quite unlikely to happen to multi-edit curves
   * with a mix of xray enabled/disabled we still
   * support this case.  */
  if (!DRW_pass_is_empty(psl->wire_pass_xray)) {
    MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl);

    DRW_draw_pass(psl->wire_pass_xray);

    MULTISAMPLE_SYNC_DISABLE_NO_DEPTH(dfbl, dtxl);
  }

  /* Thoses passes don't write to depth and are AA'ed using other tricks. */
  DRW_draw_pass(psl->overlay_edge_pass);
  DRW_draw_pass(psl->overlay_vert_pass);
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void EDIT_CURVE_engine_free(void)
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    EDIT_CURVE_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    /* Don't free builtins. */
    sh_data->wire_sh = NULL;
    GPUShader **sh_data_as_array = (GPUShader **)sh_data;
    for (int i = 0; i < (sizeof(EDIT_CURVE_Shaders) / sizeof(GPUShader *)); i++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
    }
  }
}

static const DrawEngineDataSize EDIT_CURVE_data_size = DRW_VIEWPORT_DATA_SIZE(EDIT_CURVE_Data);

DrawEngineType draw_engine_edit_curve_type = {
    NULL,
    NULL,
    N_("EditCurveMode"),
    &EDIT_CURVE_data_size,
    &EDIT_CURVE_engine_init,
    &EDIT_CURVE_engine_free,
    &EDIT_CURVE_cache_init,
    &EDIT_CURVE_cache_populate,
    NULL,
    NULL, /* draw_background but not needed by mode engines */
    &EDIT_CURVE_draw_scene,
    NULL,
    NULL,
};
