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

#include "BIF_glutil.h"

#include "BKE_font.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use EDIT_TEXT_engine_init() to
 * initialize most of them and EDIT_TEXT_cache_init()
 * for EDIT_TEXT_PassList */

typedef struct EDIT_TEXT_PassList {
  /* Declare all passes here and init them in
   * EDIT_TEXT_cache_init().
   * Only contains (DRWPass *) */
  struct DRWPass *wire_pass;
  struct DRWPass *overlay_select_pass;
  struct DRWPass *overlay_cursor_pass;
  struct DRWPass *text_box_pass;
} EDIT_TEXT_PassList;

typedef struct EDIT_TEXT_FramebufferList {
  /* Contains all framebuffer objects needed by this engine.
   * Only contains (GPUFrameBuffer *) */
  struct GPUFrameBuffer *fb;
} EDIT_TEXT_FramebufferList;

typedef struct EDIT_TEXT_TextureList {
  /* Contains all framebuffer textures / utility textures
   * needed by this engine. Only viewport specific textures
   * (not per object). Only contains (GPUTexture *) */
  struct GPUTexture *texture;
} EDIT_TEXT_TextureList;

typedef struct EDIT_TEXT_StorageList {
  /* Contains any other memory block that the engine needs.
   * Only directly MEM_(m/c)allocN'ed blocks because they are
   * free with MEM_freeN() when viewport is freed.
   * (not per object) */
  struct CustomStruct *block;
  struct EDIT_TEXT_PrivateData *g_data;
} EDIT_TEXT_StorageList;

typedef struct EDIT_TEXT_Data {
  /* Struct returned by DRW_viewport_engine_data_ensure.
   * If you don't use one of these, just make it a (void *) */
  // void *fbl;
  void *engine_type; /* Required */
  EDIT_TEXT_FramebufferList *fbl;
  EDIT_TEXT_TextureList *txl;
  EDIT_TEXT_PassList *psl;
  EDIT_TEXT_StorageList *stl;
} EDIT_TEXT_Data;

/* *********** STATIC *********** */

static struct {
  /* Custom shaders :
   * Add sources to source/blender/draw/modes/shaders
   * init in EDIT_TEXT_engine_init();
   * free in EDIT_TEXT_engine_free(); */
  GPUShader *wire_sh;
  GPUShader *overlay_select_sh;
  GPUShader *overlay_cursor_sh;
} e_data = {NULL}; /* Engine data */

typedef struct EDIT_TEXT_PrivateData {
  /* resulting curve as 'wire' for fast editmode drawing */
  DRWShadingGroup *wire_shgrp;
  DRWShadingGroup *overlay_select_shgrp;
  DRWShadingGroup *overlay_cursor_shgrp;
  DRWCallBuffer *box_shgrp;
  DRWCallBuffer *box_active_shgrp;
} EDIT_TEXT_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Init Textures, Framebuffers, Storage and Shaders.
 * It is called for every frames.
 * (Optional) */
static void EDIT_TEXT_engine_init(void *vedata)
{
  EDIT_TEXT_TextureList *txl = ((EDIT_TEXT_Data *)vedata)->txl;
  EDIT_TEXT_FramebufferList *fbl = ((EDIT_TEXT_Data *)vedata)->fbl;
  EDIT_TEXT_StorageList *stl = ((EDIT_TEXT_Data *)vedata)->stl;

  UNUSED_VARS(txl, fbl, stl);

  /* Init Framebuffers like this: order is attachment order (for color texs) */
  /*
   * DRWFboTexture tex[2] = {{&txl->depth, GPU_DEPTH_COMPONENT24, 0},
   *                         {&txl->color, GPU_RGBA8, DRW_TEX_FILTER}};
   */

  /* DRW_framebuffer_init takes care of checking if
   * the framebuffer is valid and has the right size*/
  /*
   * float *viewport_size = DRW_viewport_size_get();
   * DRW_framebuffer_init(&fbl->occlude_wire_fb,
   *                     (int)viewport_size[0], (int)viewport_size[1],
   *                     tex, 2);
   */

  if (!e_data.wire_sh) {
    e_data.wire_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
  }

  if (!e_data.overlay_select_sh) {
    e_data.overlay_select_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
  }

  if (!e_data.overlay_cursor_sh) {
    e_data.overlay_cursor_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
  }
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void EDIT_TEXT_cache_init(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EDIT_TEXT_PassList *psl = ((EDIT_TEXT_Data *)vedata)->psl;
  EDIT_TEXT_StorageList *stl = ((EDIT_TEXT_Data *)vedata)->stl;

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  {
    /* Text outline (fast drawing!) */
    psl->wire_pass = DRW_pass_create("Font Wire",
                                     DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                         DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WIRE);
    stl->g_data->wire_shgrp = DRW_shgroup_create(e_data.wire_sh, psl->wire_pass);

    psl->overlay_select_pass = DRW_pass_create("Font Select",
                                               DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH);
    stl->g_data->overlay_select_shgrp = DRW_shgroup_create(e_data.overlay_select_sh,
                                                           psl->overlay_select_pass);

    psl->overlay_cursor_pass = DRW_pass_create("Font Cursor",
                                               DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH);
    stl->g_data->overlay_cursor_shgrp = DRW_shgroup_create(e_data.overlay_cursor_sh,
                                                           psl->overlay_cursor_pass);

    psl->text_box_pass = DRW_pass_create("Font Text Boxes",
                                         DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH);
    stl->g_data->box_shgrp = buffer_dynlines_dashed_uniform_color(
        psl->text_box_pass, G_draw.block.colorWire, draw_ctx->sh_cfg);
    stl->g_data->box_active_shgrp = buffer_dynlines_dashed_uniform_color(
        psl->text_box_pass, G_draw.block.colorActive, draw_ctx->sh_cfg);
  }
}

/* Use 2D quad corners to create a matrix that set
 * a [-1..1] quad at the right position. */
static void v2_quad_corners_to_mat4(float corners[4][2], float r_mat[4][4])
{
  unit_m4(r_mat);
  sub_v2_v2v2(r_mat[0], corners[1], corners[0]);
  sub_v2_v2v2(r_mat[1], corners[3], corners[0]);
  mul_v2_fl(r_mat[0], 0.5f);
  mul_v2_fl(r_mat[1], 0.5f);
  copy_v2_v2(r_mat[3], corners[0]);
  add_v2_v2(r_mat[3], r_mat[0]);
  add_v2_v2(r_mat[3], r_mat[1]);
}

static void edit_text_cache_populate_select(void *vedata, Object *ob)
{
  EDIT_TEXT_StorageList *stl = ((EDIT_TEXT_Data *)vedata)->stl;
  const Curve *cu = ob->data;
  EditFont *ef = cu->editfont;
  float final_mat[4][4], box[4][2];
  struct GPUBatch *geom = DRW_cache_quad_get();

  for (int i = 0; i < ef->selboxes_len; i++) {
    EditFontSelBox *sb = &ef->selboxes[i];

    float selboxw;
    if (i + 1 != ef->selboxes_len) {
      if (ef->selboxes[i + 1].y == sb->y) {
        selboxw = ef->selboxes[i + 1].x - sb->x;
      }
      else {
        selboxw = sb->w;
      }
    }
    else {
      selboxw = sb->w;
    }
    /* NOTE: v2_quad_corners_to_mat4 don't need the 3rd corner. */
    if (sb->rot == 0.0f) {
      copy_v2_fl2(box[0], sb->x, sb->y);
      copy_v2_fl2(box[1], sb->x + selboxw, sb->y);
      copy_v2_fl2(box[3], sb->x, sb->y + sb->h);
    }
    else {
      float mat[2][2];
      angle_to_mat2(mat, sb->rot);
      copy_v2_fl2(box[0], sb->x, sb->y);
      mul_v2_v2fl(box[1], mat[0], selboxw);
      add_v2_v2(box[1], &sb->x);
      mul_v2_v2fl(box[3], mat[1], sb->h);
      add_v2_v2(box[3], &sb->x);
    }
    v2_quad_corners_to_mat4(box, final_mat);
    mul_m4_m4m4(final_mat, ob->obmat, final_mat);

    DRW_shgroup_call(stl->g_data->overlay_select_shgrp, geom, final_mat);
  }
}

static void edit_text_cache_populate_cursor(void *vedata, Object *ob)
{
  EDIT_TEXT_StorageList *stl = ((EDIT_TEXT_Data *)vedata)->stl;
  const Curve *cu = ob->data;
  EditFont *edit_font = cu->editfont;
  float(*cursor)[2] = edit_font->textcurs;
  float mat[4][4];

  v2_quad_corners_to_mat4(cursor, mat);
  mul_m4_m4m4(mat, ob->obmat, mat);

  struct GPUBatch *geom = DRW_cache_quad_get();
  DRW_shgroup_call(stl->g_data->overlay_cursor_shgrp, geom, mat);
}

static void edit_text_cache_populate_boxes(void *vedata, Object *ob)
{
  EDIT_TEXT_StorageList *stl = ((EDIT_TEXT_Data *)vedata)->stl;
  const Curve *cu = ob->data;

  DRWCallBuffer *callbufs[] = {
      stl->g_data->box_active_shgrp,
      stl->g_data->box_shgrp,
  };

  float vec[3], vec1[3], vec2[3];
  for (int i = 0; i < cu->totbox; i++) {
    TextBox *tb = &cu->tb[i];

    if ((tb->w == 0.0f) && (tb->h == 0.0f)) {
      continue;
    }

    const bool is_active = i == (cu->actbox - 1);
    DRWCallBuffer *callbuf = callbufs[is_active ? 0 : 1];

    vec[0] = cu->xof + tb->x;
    vec[1] = cu->yof + tb->y + cu->fsize_realtime;
    vec[2] = 0.001;

    mul_v3_m4v3(vec1, ob->obmat, vec);
    vec[0] += tb->w;
    mul_v3_m4v3(vec2, ob->obmat, vec);

    DRW_buffer_add_entry(callbuf, vec1);
    DRW_buffer_add_entry(callbuf, vec2);

    vec[1] -= tb->h;
    copy_v3_v3(vec1, vec2);
    mul_v3_m4v3(vec2, ob->obmat, vec);

    DRW_buffer_add_entry(callbuf, vec1);
    DRW_buffer_add_entry(callbuf, vec2);

    vec[0] -= tb->w;
    copy_v3_v3(vec1, vec2);
    mul_v3_m4v3(vec2, ob->obmat, vec);

    DRW_buffer_add_entry(callbuf, vec1);
    DRW_buffer_add_entry(callbuf, vec2);

    vec[1] += tb->h;
    copy_v3_v3(vec1, vec2);
    mul_v3_m4v3(vec2, ob->obmat, vec);

    DRW_buffer_add_entry(callbuf, vec1);
    DRW_buffer_add_entry(callbuf, vec2);
  }
}

/* Add geometry to shadingGroups. Execute for each objects */
static void EDIT_TEXT_cache_populate(void *vedata, Object *ob)
{
  EDIT_TEXT_PassList *psl = ((EDIT_TEXT_Data *)vedata)->psl;
  EDIT_TEXT_StorageList *stl = ((EDIT_TEXT_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  UNUSED_VARS(psl, stl);

  if (ob->type == OB_FONT) {
    if (ob == draw_ctx->object_edit) {
      const Curve *cu = ob->data;
      /* Get geometry cache */
      struct GPUBatch *geom;

      bool has_surface = (cu->flag & (CU_FRONT | CU_BACK)) || cu->ext1 != 0.0f || cu->ext2 != 0.0f;
      if ((cu->flag & CU_FAST) || !has_surface) {
        geom = DRW_cache_text_edge_wire_get(ob);
        if (geom) {
          DRW_shgroup_call(stl->g_data->wire_shgrp, geom, ob->obmat);
        }
      }
      else {
        /* object mode draws */
      }

      edit_text_cache_populate_select(vedata, ob);
      edit_text_cache_populate_cursor(vedata, ob);
      edit_text_cache_populate_boxes(vedata, ob);
    }
  }
}

/* Optional: Post-cache_populate callback */
static void EDIT_TEXT_cache_finish(void *vedata)
{
  EDIT_TEXT_PassList *psl = ((EDIT_TEXT_Data *)vedata)->psl;
  EDIT_TEXT_StorageList *stl = ((EDIT_TEXT_Data *)vedata)->stl;

  /* Do something here! dependent on the objects gathered */
  UNUSED_VARS(psl, stl);
}

/* Draw time ! Control rendering pipeline from here */
static void EDIT_TEXT_draw_scene(void *vedata)
{
  EDIT_TEXT_PassList *psl = ((EDIT_TEXT_Data *)vedata)->psl;
  EDIT_TEXT_FramebufferList *fbl = ((EDIT_TEXT_Data *)vedata)->fbl;

  /* Default framebuffer and texture */
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  UNUSED_VARS(fbl, dfbl, dtxl);

  /* Show / hide entire passes, swap framebuffers ... whatever you fancy */
  /*
   * DRW_framebuffer_texture_detach(dtxl->depth);
   * DRW_framebuffer_bind(fbl->custom_fb);
   * DRW_draw_pass(psl->pass);
   * DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0, 0);
   * DRW_framebuffer_bind(dfbl->default_fb);
   */

  DRW_draw_pass(psl->wire_pass);

  if (!DRW_pass_is_empty(psl->text_box_pass)) {
    DRW_draw_pass(psl->text_box_pass);
  }

  set_inverted_drawing(1);
  DRW_draw_pass(psl->overlay_select_pass);
  DRW_draw_pass(psl->overlay_cursor_pass);
  set_inverted_drawing(0);

  /* If you changed framebuffer, double check you rebind
   * the default one with its textures attached before finishing */
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void EDIT_TEXT_engine_free(void)
{
  // DRW_SHADER_FREE_SAFE(custom_shader);
}

static const DrawEngineDataSize EDIT_TEXT_data_size = DRW_VIEWPORT_DATA_SIZE(EDIT_TEXT_Data);

DrawEngineType draw_engine_edit_text_type = {
    NULL,
    NULL,
    N_("EditTextMode"),
    &EDIT_TEXT_data_size,
    &EDIT_TEXT_engine_init,
    &EDIT_TEXT_engine_free,
    &EDIT_TEXT_cache_init,
    &EDIT_TEXT_cache_populate,
    &EDIT_TEXT_cache_finish,
    NULL, /* draw_background but not needed by mode engines */
    &EDIT_TEXT_draw_scene,
    NULL,
    NULL,
};
