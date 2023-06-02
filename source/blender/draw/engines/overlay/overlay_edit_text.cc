/* SPDX-FileCopyrightText: 2019 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "UI_resources.h"

#include "BKE_vfont.h"

#include "DNA_curve_types.h"

#include "overlay_private.hh"

void OVERLAY_edit_text_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  DRWShadingGroup *grp;
  GPUShader *sh;
  DRWState state;

  pd->edit_curve.show_handles = v3d->overlay.handle_display != CURVE_HANDLE_NONE;
  pd->edit_curve.handle_display = v3d->overlay.handle_display;
  pd->shdata.edit_curve_normal_length = v3d->overlay.normals_length;

  /* Run Twice for in-front passes. */
  for (int i = 0; i < 2; i++) {
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH;
    state |= ((i == 0) ? DRW_STATE_DEPTH_LESS_EQUAL : DRW_STATE_DEPTH_ALWAYS);
    DRW_PASS_CREATE(psl->edit_text_wire_ps[i], state | pd->clipping_state);

    sh = OVERLAY_shader_uniform_color();
    pd->edit_text_wire_grp[i] = grp = DRW_shgroup_create(sh, psl->edit_text_wire_ps[i]);
    DRW_shgroup_uniform_vec4_copy(grp, "ucolor", G_draw.block.color_wire);
  }
  {
    /* Cursor (text caret). */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->edit_text_cursor_ps, state | pd->clipping_state);
    sh = OVERLAY_shader_uniform_color();
    pd->edit_text_cursor_grp = grp = DRW_shgroup_create(sh, psl->edit_text_cursor_ps);
    DRW_shgroup_uniform_vec4(grp, "ucolor", pd->edit_text.cursor_color, 1);

    /* Selection boxes. */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->edit_text_selection_ps, state | pd->clipping_state);
    sh = OVERLAY_shader_uniform_color();
    pd->edit_text_selection_grp = grp = DRW_shgroup_create(sh, psl->edit_text_selection_ps);
    DRW_shgroup_uniform_vec4(grp, "ucolor", pd->edit_text.selection_color, 1);

    /* Highlight text within selection boxes. */
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA | DRW_STATE_DEPTH_GREATER_EQUAL |
            pd->clipping_state;
    DRW_PASS_INSTANCE_CREATE(psl->edit_text_highlight_ps, psl->edit_text_selection_ps, state);
  }
  {
    /* Create view which will render everything (hopefully) behind the text geometry. */
    DRWView *default_view = (DRWView *)DRW_view_default_get();
    pd->view_edit_text = DRW_view_create_with_zoffset(default_view, draw_ctx->rv3d, -5.0f);
  }
}

/* Use 2D quad corners to create a matrix that set
 * a [-1..1] quad at the right position. */
static void v2_quad_corners_to_mat4(const float corners[4][2], float r_mat[4][4])
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

static void edit_text_cache_populate_select(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const Curve *cu = static_cast<Curve *>(ob->data);
  EditFont *ef = cu->editfont;
  float final_mat[4][4], box[4][2];
  GPUBatch *geom = DRW_cache_quad_get();

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
    mul_m4_m4m4(final_mat, ob->object_to_world, final_mat);

    DRW_shgroup_call_obmat(pd->edit_text_selection_grp, geom, final_mat);
  }
}

static void edit_text_cache_populate_cursor(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const Curve *cu = static_cast<Curve *>(ob->data);
  EditFont *edit_font = cu->editfont;
  float(*cursor)[2] = edit_font->textcurs;
  float mat[4][4];

  v2_quad_corners_to_mat4(cursor, mat);
  mul_m4_m4m4(mat, ob->object_to_world, mat);

  GPUBatch *geom = DRW_cache_quad_get();
  DRW_shgroup_call_obmat(pd->edit_text_cursor_grp, geom, mat);
}

static void edit_text_cache_populate_boxes(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
  const Curve *cu = static_cast<Curve *>(ob->data);

  for (int i = 0; i < cu->totbox; i++) {
    TextBox *tb = &cu->tb[i];
    const bool is_active = (i == (cu->actbox - 1));
    float *color = is_active ? G_draw.block.color_active : G_draw.block.color_wire;

    if ((tb->w != 0.0f) || (tb->h != 0.0f)) {
      float vecs[4][3];
      vecs[0][0] = vecs[1][0] = vecs[2][0] = vecs[3][0] = cu->xof + tb->x;
      vecs[0][1] = vecs[1][1] = vecs[2][1] = vecs[3][1] = cu->yof + tb->y + cu->fsize_realtime;
      vecs[0][2] = vecs[1][2] = vecs[2][2] = vecs[3][2] = 0.001;

      vecs[1][0] += tb->w;
      vecs[2][0] += tb->w;
      vecs[2][1] -= tb->h;
      vecs[3][1] -= tb->h;

      for (int j = 0; j < 4; j++) {
        mul_v3_m4v3(vecs[j], ob->object_to_world, vecs[j]);
      }
      for (int j = 0; j < 4; j++) {
        OVERLAY_extra_line_dashed(cb, vecs[j], vecs[(j + 1) % 4], color);
      }
    }
  }
}

void OVERLAY_edit_text_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  GPUBatch *geom;
  bool do_in_front = (ob->dtx & OB_DRAW_IN_FRONT) != 0;

  geom = DRW_cache_text_edge_wire_get(ob);
  if (geom) {
    DRW_shgroup_call(pd->edit_text_wire_grp[do_in_front], geom, ob);
  }

  edit_text_cache_populate_select(vedata, ob);
  edit_text_cache_populate_cursor(vedata, ob);
  edit_text_cache_populate_boxes(vedata, ob);
}

void OVERLAY_edit_text_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_FramebufferList *fbl = vedata->fbl;

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_default_fb);
  }

  DRW_draw_pass(psl->edit_text_wire_ps[0]);
  DRW_draw_pass(psl->edit_text_wire_ps[1]);

  DRW_view_set_active(pd->view_edit_text);

  /* Selection Boxes. */
  UI_GetThemeColor4fv(TH_WIDGET_TEXT_SELECTION, pd->edit_text.selection_color);
  srgb_to_linearrgb_v4(pd->edit_text.selection_color, pd->edit_text.selection_color);
  DRW_draw_pass(psl->edit_text_selection_ps);

  /* Highlight text within selection boxes. */
  UI_GetThemeColor4fv(TH_WIDGET_TEXT_HIGHLIGHT, pd->edit_text.selection_color);
  srgb_to_linearrgb_v4(pd->edit_text.selection_color, pd->edit_text.selection_color);
  DRW_draw_pass(psl->edit_text_highlight_ps);

  /* Cursor (text caret). */
  UI_GetThemeColor4fv(TH_WIDGET_TEXT_CURSOR, pd->edit_text.cursor_color);
  srgb_to_linearrgb_v4(pd->edit_text.cursor_color, pd->edit_text.cursor_color);
  DRW_draw_pass(psl->edit_text_cursor_ps);
}
