/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

/* System includes ----------------------------------------------------- */

#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_dlrbTree.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "GPU_immediate.h"
#include "GPU_shader_shared.h"
#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_keylist.h"

/* *************************** Keyframe Drawing *************************** */

void draw_keyframe_shape(float x,
                         float y,
                         float size,
                         bool sel,
                         short key_type,
                         short mode,
                         float alpha,
                         const KeyframeShaderBindings *sh_bindings,
                         short handle_type,
                         short extreme_type)
{
  bool draw_fill = ELEM(mode, KEYFRAME_SHAPE_INSIDE, KEYFRAME_SHAPE_BOTH);
  bool draw_outline = ELEM(mode, KEYFRAME_SHAPE_FRAME, KEYFRAME_SHAPE_BOTH);

  BLI_assert(draw_fill || draw_outline);

  /* tweak size of keyframe shape according to type of keyframe
   * - 'proper' keyframes have key_type = 0, so get drawn at full size
   */
  switch (key_type) {
    case BEZT_KEYTYPE_KEYFRAME: /* must be full size */
      break;

    case BEZT_KEYTYPE_BREAKDOWN: /* slightly smaller than normal keyframe */
      size *= 0.85f;
      break;

    case BEZT_KEYTYPE_MOVEHOLD: /* Slightly smaller than normal keyframes
                                 * (but by less than for breakdowns). */
      size *= 0.925f;
      break;

    case BEZT_KEYTYPE_EXTREME: /* slightly larger */
      size *= 1.2f;
      break;

    default:
      size -= 0.8f * key_type;
  }

  uchar fill_col[4];
  uchar outline_col[4];
  uint flags = 0;

  /* draw! */
  if (draw_fill) {
    /* get interior colors from theme (for selected and unselected only) */
    switch (key_type) {
      case BEZT_KEYTYPE_BREAKDOWN: /* bluish frames (default theme) */
        UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_BREAKDOWN_SELECT : TH_KEYTYPE_BREAKDOWN, fill_col);
        break;
      case BEZT_KEYTYPE_EXTREME: /* reddish frames (default theme) */
        UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_EXTREME_SELECT : TH_KEYTYPE_EXTREME, fill_col);
        break;
      case BEZT_KEYTYPE_JITTER: /* greenish frames (default theme) */
        UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_JITTER_SELECT : TH_KEYTYPE_JITTER, fill_col);
        break;
      case BEZT_KEYTYPE_MOVEHOLD: /* similar to traditional keyframes, but different... */
        UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_MOVEHOLD_SELECT : TH_KEYTYPE_MOVEHOLD, fill_col);
        break;
      case BEZT_KEYTYPE_KEYFRAME: /* traditional yellowish frames (default theme) */
      default:
        UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_KEYFRAME_SELECT : TH_KEYTYPE_KEYFRAME, fill_col);
    }

    /* NOTE: we don't use the straight alpha from the theme, or else effects such as
     * graying out protected/muted channels doesn't work correctly!
     */
    fill_col[3] *= alpha;

    if (!draw_outline) {
      /* force outline color to match */
      outline_col[0] = fill_col[0];
      outline_col[1] = fill_col[1];
      outline_col[2] = fill_col[2];
      outline_col[3] = fill_col[3];
    }
  }

  if (draw_outline) {
    /* exterior - black frame */
    UI_GetThemeColor4ubv(sel ? TH_KEYBORDER_SELECT : TH_KEYBORDER, outline_col);
    outline_col[3] *= alpha;

    if (!draw_fill) {
      /* fill color needs to be (outline.rgb, 0) */
      fill_col[0] = outline_col[0];
      fill_col[1] = outline_col[1];
      fill_col[2] = outline_col[2];
      fill_col[3] = 0;
    }

    /* Handle type to outline shape. */
    switch (handle_type) {
      case KEYFRAME_HANDLE_AUTO_CLAMP:
        flags = GPU_KEYFRAME_SHAPE_CIRCLE;
        break; /* circle */
      case KEYFRAME_HANDLE_AUTO:
        flags = GPU_KEYFRAME_SHAPE_CIRCLE | GPU_KEYFRAME_SHAPE_INNER_DOT;
        break; /* circle with dot */
      case KEYFRAME_HANDLE_VECTOR:
        flags = GPU_KEYFRAME_SHAPE_SQUARE;
        break; /* square */
      case KEYFRAME_HANDLE_ALIGNED:
        flags = GPU_KEYFRAME_SHAPE_DIAMOND | GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL;
        break; /* clipped diamond */

      case KEYFRAME_HANDLE_FREE:
      default:
        flags = GPU_KEYFRAME_SHAPE_DIAMOND; /* diamond */
    }

    /* Extreme type to arrow-like shading. */
    if (extreme_type & KEYFRAME_EXTREME_MAX) {
      flags |= GPU_KEYFRAME_SHAPE_ARROW_END_MAX;
    }
    if (extreme_type & KEYFRAME_EXTREME_MIN) {
      flags |= GPU_KEYFRAME_SHAPE_ARROW_END_MIN;
    }
    if (extreme_type & GPU_KEYFRAME_SHAPE_ARROW_END_MIXED) {
      flags |= 0x400;
    }
  }

  immAttr1f(sh_bindings->size_id, size);
  immAttr4ubv(sh_bindings->color_id, fill_col);
  immAttr4ubv(sh_bindings->outline_color_id, outline_col);
  immAttr1u(sh_bindings->flags_id, flags);
  immVertex2f(sh_bindings->pos_id, x, y);
}

/* Common attributes shared between the draw calls. */
typedef struct DrawKeylistUIData {
  float alpha;
  float icon_size;
  float half_icon_size;
  float smaller_size;
  float ipo_size;
  float gpencil_size;
  float screenspace_margin;
  float sel_color[4];
  float unsel_color[4];
  float sel_mhcol[4];
  float unsel_mhcol[4];
  float ipo_color[4];
  float ipo_color_mix[4];

  /* Show interpolation and handle type? */
  bool show_ipo;
} DrawKeylistUIData;

static void draw_keylist_ui_data_init(DrawKeylistUIData *ctx,
                                      View2D *v2d,
                                      float yscale_fac,
                                      bool channel_locked,
                                      eSAction_Flag saction_flag)
{
  /* locked channels are less strongly shown, as feedback for locked channels in DopeSheet */
  /* TODO: allow this opacity factor to be themed? */
  ctx->alpha = channel_locked ? 0.25f : 1.0f;

  ctx->icon_size = U.widget_unit * 0.5f * yscale_fac;
  ctx->half_icon_size = 0.5f * ctx->icon_size;
  ctx->smaller_size = 0.35f * ctx->icon_size;
  ctx->ipo_size = 0.1f * ctx->icon_size;
  ctx->gpencil_size = ctx->smaller_size * 0.8f;
  ctx->screenspace_margin = (0.35f * (float)UI_UNIT_X) / UI_view2d_scale_get_x(v2d);

  ctx->show_ipo = (saction_flag & SACTION_SHOW_INTERPOLATION) != 0;

  UI_GetThemeColor4fv(TH_STRIP_SELECT, ctx->sel_color);
  UI_GetThemeColor4fv(TH_STRIP, ctx->unsel_color);
  UI_GetThemeColor4fv(TH_DOPESHEET_IPOLINE, ctx->ipo_color);

  ctx->sel_color[3] *= ctx->alpha;
  ctx->unsel_color[3] *= ctx->alpha;
  ctx->ipo_color[3] *= ctx->alpha;

  copy_v4_v4(ctx->sel_mhcol, ctx->sel_color);
  ctx->sel_mhcol[3] *= 0.8f;
  copy_v4_v4(ctx->unsel_mhcol, ctx->unsel_color);
  ctx->unsel_mhcol[3] *= 0.8f;
  copy_v4_v4(ctx->ipo_color_mix, ctx->ipo_color);
  ctx->ipo_color_mix[3] *= 0.5f;
}

static void draw_keylist_block_gpencil(const DrawKeylistUIData *ctx,
                                       const ActKeyColumn *ab,
                                       float ypos)
{
  UI_draw_roundbox_corner_set(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
  float size = 1.0f;
  switch (ab->next->key_type) {
    case BEZT_KEYTYPE_BREAKDOWN:
    case BEZT_KEYTYPE_MOVEHOLD:
    case BEZT_KEYTYPE_JITTER:
      size *= 0.5f;
      break;
    case BEZT_KEYTYPE_KEYFRAME:
      size *= 0.8f;
      break;
    default:
      break;
  }
  UI_draw_roundbox_4fv(
      &(const rctf){
          .xmin = ab->cfra,
          .xmax = min_ff(ab->next->cfra - (ctx->screenspace_margin * size), ab->next->cfra),
          .ymin = ypos - ctx->gpencil_size,
          .ymax = ypos + ctx->gpencil_size,
      },
      true,
      0.25f * (float)UI_UNIT_X,
      (ab->block.sel) ? ctx->sel_mhcol : ctx->unsel_mhcol);
}

static void draw_keylist_block_moving_hold(const DrawKeylistUIData *ctx,
                                           const ActKeyColumn *ab,
                                           float ypos)
{

  UI_draw_roundbox_4fv(
      &(const rctf){
          .xmin = ab->cfra,
          .xmax = ab->next->cfra,
          .ymin = ypos - ctx->smaller_size,
          .ymax = ypos + ctx->smaller_size,
      },
      true,
      3.0f,
      (ab->block.sel) ? ctx->sel_mhcol : ctx->unsel_mhcol);
}

static void draw_keylist_block_standard(const DrawKeylistUIData *ctx,
                                        const ActKeyColumn *ab,
                                        float ypos)
{
  UI_draw_roundbox_4fv(
      &(const rctf){
          .xmin = ab->cfra,
          .xmax = ab->next->cfra,
          .ymin = ypos - ctx->half_icon_size,
          .ymax = ypos + ctx->half_icon_size,
      },
      true,
      3.0f,
      (ab->block.sel) ? ctx->sel_color : ctx->unsel_color);
}

static void draw_keylist_block_interpolation_line(const DrawKeylistUIData *ctx,
                                                  const ActKeyColumn *ab,
                                                  float ypos)
{
  UI_draw_roundbox_4fv(
      &(const rctf){
          .xmin = ab->cfra,
          .xmax = ab->next->cfra,
          .ymin = ypos - ctx->ipo_size,
          .ymax = ypos + ctx->ipo_size,
      },
      true,
      3.0f,
      (ab->block.conflict & ACTKEYBLOCK_FLAG_NON_BEZIER) ? ctx->ipo_color_mix : ctx->ipo_color);
}

static void draw_keylist_block(const DrawKeylistUIData *ctx, const ActKeyColumn *ab, float ypos)
{

  /* Draw grease pencil bars between keyframes. */
  if ((ab->next != NULL) && (ab->block.flag & ACTKEYBLOCK_FLAG_GPENCIL)) {
    draw_keylist_block_gpencil(ctx, ab, ypos);
  }
  else {
    /* Draw other types. */
    UI_draw_roundbox_corner_set(UI_CNR_NONE);

    int valid_hold = actkeyblock_get_valid_hold(ab);
    if (valid_hold != 0) {
      if ((valid_hold & ACTKEYBLOCK_FLAG_STATIC_HOLD) == 0) {
        /* draw "moving hold" long-keyframe block - slightly smaller */
        draw_keylist_block_moving_hold(ctx, ab, ypos);
      }
      else {
        /* draw standard long-keyframe block */
        draw_keylist_block_standard(ctx, ab, ypos);
      }
    }
    if (ctx->show_ipo && actkeyblock_is_valid(ab) &&
        (ab->block.flag & ACTKEYBLOCK_FLAG_NON_BEZIER)) {
      /* draw an interpolation line */
      draw_keylist_block_interpolation_line(ctx, ab, ypos);
    }
  }
}

static void draw_keylist_blocks(const DrawKeylistUIData *ctx,
                                const ActKeyColumn *keys,
                                const int key_len,
                                float ypos)
{
  for (int i = 0; i < key_len; i++) {
    const ActKeyColumn *ab = &keys[i];
    draw_keylist_block(ctx, ab, ypos);
  }
}

static bool draw_keylist_is_visible_key(const View2D *v2d, const ActKeyColumn *ak)
{
  return IN_RANGE_INCL(ak->cfra, v2d->cur.xmin, v2d->cur.xmax);
}

static void draw_keylist_keys(const DrawKeylistUIData *ctx,
                              View2D *v2d,
                              const KeyframeShaderBindings *sh_bindings,
                              const ActKeyColumn *keys,
                              const int key_len,
                              float ypos,
                              eSAction_Flag saction_flag)
{
  short handle_type = KEYFRAME_HANDLE_NONE, extreme_type = KEYFRAME_EXTREME_NONE;

  for (int i = 0; i < key_len; i++) {
    const ActKeyColumn *ak = &keys[i];
    if (draw_keylist_is_visible_key(v2d, ak)) {
      if (ctx->show_ipo) {
        handle_type = ak->handle_type;
      }
      if (saction_flag & SACTION_SHOW_EXTREMES) {
        extreme_type = ak->extreme_type;
      }

      draw_keyframe_shape(ak->cfra,
                          ypos,
                          ctx->icon_size,
                          (ak->sel & SELECT),
                          ak->key_type,
                          KEYFRAME_SHAPE_BOTH,
                          ctx->alpha,
                          sh_bindings,
                          handle_type,
                          extreme_type);
    }
  }
}

/* *************************** Drawing Stack *************************** */
typedef enum eAnimKeylistDrawListElemType {
  ANIM_KEYLIST_SUMMARY,
  ANIM_KEYLIST_SCENE,
  ANIM_KEYLIST_OBJECT,
  ANIM_KEYLIST_FCURVE,
  ANIM_KEYLIST_ACTION,
  ANIM_KEYLIST_AGROUP,
  ANIM_KEYLIST_GP_LAYER,
  ANIM_KEYLIST_MASK_LAYER,
} eAnimKeylistDrawListElemType;

typedef struct AnimKeylistDrawListElem {
  struct AnimKeylistDrawListElem *next, *prev;
  struct AnimKeylist *keylist;
  eAnimKeylistDrawListElemType type;

  float yscale_fac;
  float ypos;
  eSAction_Flag saction_flag;
  bool channel_locked;

  bAnimContext *ac;
  bDopeSheet *ads;
  Scene *sce;
  Object *ob;
  AnimData *adt;
  FCurve *fcu;
  bAction *act;
  bActionGroup *agrp;
  bGPDlayer *gpl;
  MaskLayer *masklay;

} AnimKeylistDrawListElem;

static void ED_keylist_draw_list_elem_build_keylist(AnimKeylistDrawListElem *elem)
{
  switch (elem->type) {
    case ANIM_KEYLIST_SUMMARY: {
      summary_to_keylist(elem->ac, elem->keylist, elem->saction_flag);
      break;
    }
    case ANIM_KEYLIST_SCENE: {
      scene_to_keylist(elem->ads, elem->sce, elem->keylist, elem->saction_flag);
      break;
    }
    case ANIM_KEYLIST_OBJECT: {
      ob_to_keylist(elem->ads, elem->ob, elem->keylist, elem->saction_flag);
      break;
    }
    case ANIM_KEYLIST_FCURVE: {
      fcurve_to_keylist(elem->adt, elem->fcu, elem->keylist, elem->saction_flag);
      break;
    }
    case ANIM_KEYLIST_ACTION: {
      action_to_keylist(elem->adt, elem->act, elem->keylist, elem->saction_flag);
      break;
    }
    case ANIM_KEYLIST_AGROUP: {
      agroup_to_keylist(elem->adt, elem->agrp, elem->keylist, elem->saction_flag);
      break;
    }
    case ANIM_KEYLIST_GP_LAYER: {
      gpl_to_keylist(elem->ads, elem->gpl, elem->keylist);
      break;
    }
    case ANIM_KEYLIST_MASK_LAYER: {
      mask_to_keylist(elem->ads, elem->masklay, elem->keylist);
      break;
    }
  }
}

static void ED_keylist_draw_list_elem_draw_blocks(AnimKeylistDrawListElem *elem, View2D *v2d)
{
  DrawKeylistUIData ctx;
  draw_keylist_ui_data_init(&ctx, v2d, elem->yscale_fac, elem->channel_locked, elem->saction_flag);

  const int key_len = ED_keylist_array_len(elem->keylist);
  const ActKeyColumn *keys = ED_keylist_array(elem->keylist);
  draw_keylist_blocks(&ctx, keys, key_len, elem->ypos);
}

static void ED_keylist_draw_list_elem_draw_keys(AnimKeylistDrawListElem *elem,
                                                View2D *v2d,
                                                const KeyframeShaderBindings *sh_bindings)
{
  DrawKeylistUIData ctx;
  draw_keylist_ui_data_init(&ctx, v2d, elem->yscale_fac, elem->channel_locked, elem->saction_flag);

  const int key_len = ED_keylist_array_len(elem->keylist);
  const ActKeyColumn *keys = ED_keylist_array(elem->keylist);
  draw_keylist_keys(&ctx, v2d, sh_bindings, keys, key_len, elem->ypos, elem->saction_flag);
}

static void ED_keylist_draw_list_elem_prepare_for_drawing(AnimKeylistDrawListElem *elem)
{
  ED_keylist_prepare_for_direct_access(elem->keylist);
}

typedef struct AnimKeylistDrawList {
  ListBase /*AnimKeylistDrawListElem*/ channels;
} AnimKeylistDrawList;

AnimKeylistDrawList *ED_keylist_draw_list_create(void)
{
  return MEM_callocN(sizeof(AnimKeylistDrawList), __func__);
}

static void ED_keylist_draw_list_build_keylists(AnimKeylistDrawList *draw_list)
{
  LISTBASE_FOREACH (AnimKeylistDrawListElem *, elem, &draw_list->channels) {
    ED_keylist_draw_list_elem_build_keylist(elem);
    ED_keylist_draw_list_elem_prepare_for_drawing(elem);
  }
}

static void ED_keylist_draw_list_draw_blocks(AnimKeylistDrawList *draw_list, View2D *v2d)
{
  LISTBASE_FOREACH (AnimKeylistDrawListElem *, elem, &draw_list->channels) {
    ED_keylist_draw_list_elem_draw_blocks(elem, v2d);
  }
}

static int ED_keylist_draw_keylist_visible_key_len(const View2D *v2d,
                                                   const ListBase * /*ActKeyColumn*/ keys)
{
  /* count keys */
  uint len = 0;

  LISTBASE_FOREACH (ActKeyColumn *, ak, keys) {
    /* Optimization: if keyframe doesn't appear within 5 units (screenspace)
     * in visible area, don't draw.
     * This might give some improvements,
     * since we current have to flip between view/region matrices.
     */
    if (draw_keylist_is_visible_key(v2d, ak)) {
      len++;
    }
  }
  return len;
}

static int ED_keylist_draw_list_visible_key_len(const AnimKeylistDrawList *draw_list,
                                                const View2D *v2d)
{
  uint len = 0;
  LISTBASE_FOREACH (AnimKeylistDrawListElem *, elem, &draw_list->channels) {
    const ListBase *keys = ED_keylist_listbase(elem->keylist);
    len += ED_keylist_draw_keylist_visible_key_len(v2d, keys);
  }
  return len;
}

static void ED_keylist_draw_list_draw_keys(AnimKeylistDrawList *draw_list, View2D *v2d)
{
  const int visible_key_len = ED_keylist_draw_list_visible_key_len(draw_list, v2d);
  if (visible_key_len == 0) {
    return;
  }

  GPU_blend(GPU_BLEND_ALPHA);

  GPUVertFormat *format = immVertexFormat();
  KeyframeShaderBindings sh_bindings;

  sh_bindings.pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  sh_bindings.size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  sh_bindings.color_id = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  sh_bindings.outline_color_id = GPU_vertformat_attr_add(
      format, "outlineColor", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  sh_bindings.flags_id = GPU_vertformat_attr_add(format, "flags", GPU_COMP_U32, 1, GPU_FETCH_INT);

  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", 1.0f);
  immUniform2f("ViewportSize", BLI_rcti_size_x(&v2d->mask) + 1, BLI_rcti_size_y(&v2d->mask) + 1);
  immBegin(GPU_PRIM_POINTS, visible_key_len);

  LISTBASE_FOREACH (AnimKeylistDrawListElem *, elem, &draw_list->channels) {
    ED_keylist_draw_list_elem_draw_keys(elem, v2d, &sh_bindings);
  }

  immEnd();
  GPU_program_point_size(false);
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

static void ED_keylist_draw_list_draw(AnimKeylistDrawList *draw_list, View2D *v2d)
{
  ED_keylist_draw_list_draw_blocks(draw_list, v2d);
  ED_keylist_draw_list_draw_keys(draw_list, v2d);
}

void ED_keylist_draw_list_flush(AnimKeylistDrawList *draw_list, View2D *v2d)
{
  ED_keylist_draw_list_build_keylists(draw_list);
  ED_keylist_draw_list_draw(draw_list, v2d);
}

void ED_keylist_draw_list_free(AnimKeylistDrawList *draw_list)
{
  LISTBASE_FOREACH (AnimKeylistDrawListElem *, elem, &draw_list->channels) {
    ED_keylist_free(elem->keylist);
  }
  BLI_freelistN(&draw_list->channels);
  MEM_freeN(draw_list);
}

static AnimKeylistDrawListElem *ed_keylist_draw_list_add_elem(
    AnimKeylistDrawList *draw_list,
    eAnimKeylistDrawListElemType elem_type,
    float ypos,
    float yscale_fac,
    eSAction_Flag saction_flag)
{
  AnimKeylistDrawListElem *draw_elem = MEM_callocN(sizeof(AnimKeylistDrawListElem), __func__);
  BLI_addtail(&draw_list->channels, draw_elem);
  draw_elem->type = elem_type;
  draw_elem->keylist = ED_keylist_create();
  draw_elem->ypos = ypos;
  draw_elem->yscale_fac = yscale_fac;
  draw_elem->saction_flag = saction_flag;
  return draw_elem;
}

/* *************************** Channel Drawing Functions *************************** */

void draw_summary_channel(struct AnimKeylistDrawList *draw_list,
                          bAnimContext *ac,
                          float ypos,
                          float yscale_fac,
                          int saction_flag)
{
  saction_flag &= ~SACTION_SHOW_EXTREMES;
  AnimKeylistDrawListElem *draw_elem = ed_keylist_draw_list_add_elem(
      draw_list, ANIM_KEYLIST_SUMMARY, ypos, yscale_fac, saction_flag);
  draw_elem->ac = ac;
}

void draw_scene_channel(AnimKeylistDrawList *draw_list,
                        bDopeSheet *ads,
                        Scene *sce,
                        float ypos,
                        float yscale_fac,
                        int saction_flag)
{
  saction_flag &= ~SACTION_SHOW_EXTREMES;
  AnimKeylistDrawListElem *draw_elem = ed_keylist_draw_list_add_elem(
      draw_list, ANIM_KEYLIST_SCENE, ypos, yscale_fac, saction_flag);
  draw_elem->ads = ads;
  draw_elem->sce = sce;
}

void draw_object_channel(AnimKeylistDrawList *draw_list,
                         bDopeSheet *ads,
                         Object *ob,
                         float ypos,
                         float yscale_fac,
                         int saction_flag)
{
  saction_flag &= ~SACTION_SHOW_EXTREMES;
  AnimKeylistDrawListElem *draw_elem = ed_keylist_draw_list_add_elem(
      draw_list, ANIM_KEYLIST_OBJECT, ypos, yscale_fac, saction_flag);
  draw_elem->ads = ads;
  draw_elem->ob = ob;
}

void draw_fcurve_channel(AnimKeylistDrawList *draw_list,
                         AnimData *adt,
                         FCurve *fcu,
                         float ypos,
                         float yscale_fac,
                         int saction_flag)
{
  const bool locked = (fcu->flag & FCURVE_PROTECTED) ||
                      ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) ||
                      ((adt && adt->action) &&
                       (ID_IS_LINKED(adt->action) || ID_IS_OVERRIDE_LIBRARY(adt->action)));

  AnimKeylistDrawListElem *draw_elem = ed_keylist_draw_list_add_elem(
      draw_list, ANIM_KEYLIST_FCURVE, ypos, yscale_fac, saction_flag);
  draw_elem->adt = adt;
  draw_elem->fcu = fcu;
  draw_elem->channel_locked = locked;
}

void draw_agroup_channel(AnimKeylistDrawList *draw_list,
                         AnimData *adt,
                         bActionGroup *agrp,
                         float ypos,
                         float yscale_fac,
                         int saction_flag)
{
  bool locked = (agrp->flag & AGRP_PROTECTED) ||
                ((adt && adt->action) &&
                 (ID_IS_LINKED(adt->action) || ID_IS_OVERRIDE_LIBRARY(adt->action)));

  AnimKeylistDrawListElem *draw_elem = ed_keylist_draw_list_add_elem(
      draw_list, ANIM_KEYLIST_AGROUP, ypos, yscale_fac, saction_flag);
  draw_elem->adt = adt;
  draw_elem->agrp = agrp;
  draw_elem->channel_locked = locked;
}

void draw_action_channel(AnimKeylistDrawList *draw_list,
                         AnimData *adt,
                         bAction *act,
                         float ypos,
                         float yscale_fac,
                         int saction_flag)
{
  const bool locked = (act && (ID_IS_LINKED(act) || ID_IS_OVERRIDE_LIBRARY(act)));
  saction_flag &= ~SACTION_SHOW_EXTREMES;

  AnimKeylistDrawListElem *draw_elem = ed_keylist_draw_list_add_elem(
      draw_list, ANIM_KEYLIST_ACTION, ypos, yscale_fac, saction_flag);
  draw_elem->adt = adt;
  draw_elem->act = act;
  draw_elem->channel_locked = locked;
}

void draw_gpl_channel(AnimKeylistDrawList *draw_list,
                      bDopeSheet *ads,
                      bGPDlayer *gpl,
                      float ypos,
                      float yscale_fac,
                      int saction_flag)
{
  bool locked = (gpl->flag & GP_LAYER_LOCKED) != 0;
  AnimKeylistDrawListElem *draw_elem = ed_keylist_draw_list_add_elem(
      draw_list, ANIM_KEYLIST_GP_LAYER, ypos, yscale_fac, saction_flag);
  draw_elem->ads = ads;
  draw_elem->gpl = gpl;
  draw_elem->channel_locked = locked;
}

void draw_masklay_channel(AnimKeylistDrawList *draw_list,
                          bDopeSheet *ads,
                          MaskLayer *masklay,
                          float ypos,
                          float yscale_fac,
                          int saction_flag)
{
  bool locked = (masklay->flag & MASK_LAYERFLAG_LOCKED) != 0;
  AnimKeylistDrawListElem *draw_elem = ed_keylist_draw_list_add_elem(
      draw_list, ANIM_KEYLIST_MASK_LAYER, ypos, yscale_fac, saction_flag);
  draw_elem->ads = ads;
  draw_elem->masklay = masklay;
  draw_elem->channel_locked = locked;
}
