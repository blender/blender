/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

/* System includes ----------------------------------------------------- */

#include <cfloat>

#include "MEM_guardedalloc.h"

#include "BKE_grease_pencil.hh"
#include "BKE_library.hh"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_mask_types.h"

#include "GPU_immediate.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_draw.hh"
#include "ED_keyframes_keylist.hh"

#include "ANIM_action.hh"

using namespace blender;

/* *************************** Keyframe Drawing *************************** */

void draw_keyframe_shape(const float x,
                         const float y,
                         float size,
                         const bool sel,
                         const eBezTriple_KeyframeType key_type,
                         const eKeyframeShapeDrawOpts mode,
                         const float alpha,
                         const KeyframeShaderBindings *sh_bindings,
                         const short handle_type,
                         const short extreme_type)
{
  bool draw_fill = ELEM(mode, KEYFRAME_SHAPE_INSIDE, KEYFRAME_SHAPE_BOTH);
  bool draw_outline = ELEM(mode, KEYFRAME_SHAPE_FRAME, KEYFRAME_SHAPE_BOTH);

  BLI_assert(draw_fill || draw_outline);

  /* Adjust size of keyframe shape according to type of keyframe. */
  switch (key_type) {
    case BEZT_KEYTYPE_KEYFRAME:
      break;

    case BEZT_KEYTYPE_BREAKDOWN:
      size *= 0.85f;
      break;

    case BEZT_KEYTYPE_MOVEHOLD:
      size *= 0.925f;
      break;

    case BEZT_KEYTYPE_EXTREME:
      size *= 1.2f;
      break;

    case BEZT_KEYTYPE_JITTER:
      size *= 0.8f;
      break;

    case BEZT_KEYTYPE_GENERATED:
      size *= 0.75;
      break;
  }

  uchar fill_col[4];
  uchar outline_col[4];
  uint flags = 0;

  /* draw! */
  if (draw_fill) {
    /* get interior colors from theme (for selected and unselected only) */
    switch (key_type) {
      case BEZT_KEYTYPE_BREAKDOWN:
        UI_GetThemeColor3ubv(sel ? TH_KEYTYPE_BREAKDOWN_SELECT : TH_KEYTYPE_BREAKDOWN, fill_col);
        break;
      case BEZT_KEYTYPE_EXTREME:
        UI_GetThemeColor3ubv(sel ? TH_KEYTYPE_EXTREME_SELECT : TH_KEYTYPE_EXTREME, fill_col);
        break;
      case BEZT_KEYTYPE_JITTER:
        UI_GetThemeColor3ubv(sel ? TH_KEYTYPE_JITTER_SELECT : TH_KEYTYPE_JITTER, fill_col);
        break;
      case BEZT_KEYTYPE_MOVEHOLD:
        UI_GetThemeColor3ubv(sel ? TH_KEYTYPE_MOVEHOLD_SELECT : TH_KEYTYPE_MOVEHOLD, fill_col);
        break;
      case BEZT_KEYTYPE_KEYFRAME:
        UI_GetThemeColor3ubv(sel ? TH_KEYTYPE_KEYFRAME_SELECT : TH_KEYTYPE_KEYFRAME, fill_col);
        break;
      case BEZT_KEYTYPE_GENERATED:
        UI_GetThemeColor3ubv(sel ? TH_KEYTYPE_GENERATED_SELECT : TH_KEYTYPE_GENERATED, fill_col);
        break;
    }

    /* For effects like graying out protected/muted channels. The theme RNA/UI doesn't allow users
     * to set the alpha. */
    fill_col[3] = 255.0f * alpha;

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
struct DrawKeylistUIData {
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

  float ipo_color_linear[4];
  float ipo_color_constant[4];
  float ipo_color_other[4];
  float ipo_color_mix[4];

  /* Show interpolation and handle type? */
  bool show_ipo;
};

static void channel_ui_data_init(DrawKeylistUIData *ctx,
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
  ctx->screenspace_margin = (0.35f * float(UI_UNIT_X)) / UI_view2d_scale_get_x(v2d);

  ctx->show_ipo = (saction_flag & SACTION_SHOW_INTERPOLATION) != 0;

  UI_GetThemeColor4fv(TH_LONGKEY_SELECT, ctx->sel_color);
  UI_GetThemeColor4fv(TH_LONGKEY, ctx->unsel_color);
  UI_GetThemeColor4fv(TH_DOPESHEET_IPOLINE, ctx->ipo_color_linear);
  UI_GetThemeColor4fv(TH_DOPESHEET_IPOCONST, ctx->ipo_color_constant);
  UI_GetThemeColor4fv(TH_DOPESHEET_IPOOTHER, ctx->ipo_color_other);
  UI_GetThemeColor4fv(TH_KEYTYPE_KEYFRAME, ctx->ipo_color_mix);

  ctx->sel_color[3] *= ctx->alpha;
  ctx->unsel_color[3] *= ctx->alpha;
  ctx->ipo_color_linear[3] *= ctx->alpha;
  ctx->ipo_color_constant[3] *= ctx->alpha;
  ctx->ipo_color_other[3] *= ctx->alpha;
  ctx->ipo_color_mix[3] *= ctx->alpha * 0.5f;

  copy_v4_v4(ctx->sel_mhcol, ctx->sel_color);
  ctx->sel_mhcol[3] *= 0.8f;
  copy_v4_v4(ctx->unsel_mhcol, ctx->unsel_color);
  ctx->unsel_mhcol[3] *= 0.8f;
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
    case BEZT_KEYTYPE_GENERATED:
      size *= 0.5f;
      break;
    case BEZT_KEYTYPE_KEYFRAME:
      size *= 0.8f;
      break;
    case BEZT_KEYTYPE_EXTREME:
      break;
  }

  rctf box;
  box.xmin = ab->cfra;
  box.xmax = min_ff(ab->next->cfra - (ctx->screenspace_margin * size), ab->next->cfra);
  box.ymin = ypos - ctx->gpencil_size;
  box.ymax = ypos + ctx->gpencil_size;

  UI_draw_roundbox_4fv(
      &box, true, 0.25f * float(UI_UNIT_X), (ab->block.sel) ? ctx->sel_mhcol : ctx->unsel_mhcol);
}

static void draw_keylist_block_moving_hold(const DrawKeylistUIData *ctx,
                                           const ActKeyColumn *ab,
                                           float ypos)
{
  rctf box;
  box.xmin = ab->cfra;
  box.xmax = ab->next->cfra;
  box.ymin = ypos - ctx->smaller_size;
  box.ymax = ypos + ctx->smaller_size;

  UI_draw_roundbox_4fv(&box, true, 3.0f, (ab->block.sel) ? ctx->sel_mhcol : ctx->unsel_mhcol);
}

static void draw_keylist_block_standard(const DrawKeylistUIData *ctx,
                                        const ActKeyColumn *ab,
                                        float ypos)
{
  rctf box;
  box.xmin = ab->cfra;
  box.xmax = ab->next->cfra;
  box.ymin = ypos - ctx->half_icon_size;
  box.ymax = ypos + ctx->half_icon_size;

  UI_draw_roundbox_4fv(&box, true, 3.0f, (ab->block.sel) ? ctx->sel_color : ctx->unsel_color);
}

static void draw_keylist_block_interpolation_line(const DrawKeylistUIData *ctx,
                                                  const ActKeyColumn *ab,
                                                  float ypos)
{
  rctf box;
  box.xmin = ab->cfra;
  box.xmax = ab->next->cfra;
  box.ymin = ypos - ctx->ipo_size;
  box.ymax = ypos + ctx->ipo_size;

  /* Color for interpolation lines based on their type */
  const float *color = nullptr;

  constexpr short IPO_FLAGS = ACTKEYBLOCK_FLAG_IPO_OTHER | ACTKEYBLOCK_FLAG_IPO_LINEAR |
                              ACTKEYBLOCK_FLAG_IPO_CONSTANT;
  if (ab->block.conflict & IPO_FLAGS) {
    /* This is a summary line that combines multiple interpolation modes. */
    color = ctx->ipo_color_mix;
  }
  else if (ab->block.flag & ACTKEYBLOCK_FLAG_IPO_OTHER) {
    color = ctx->ipo_color_other;
  }
  else if (ab->block.flag & ACTKEYBLOCK_FLAG_IPO_LINEAR) {
    color = ctx->ipo_color_linear;
  }
  else if (ab->block.flag & ACTKEYBLOCK_FLAG_IPO_CONSTANT) {
    color = ctx->ipo_color_constant;
  }
  else {
    /* No line to draw. */
    return;
  }

  UI_draw_roundbox_4fv(&box, true, 3.0f, color);
}

static void draw_keylist_block(const DrawKeylistUIData *ctx, const ActKeyColumn *ab, float ypos)
{
  /* Draw grease pencil bars between keyframes. */
  if ((ab->next != nullptr) && (ab->block.flag & ACTKEYBLOCK_FLAG_GPENCIL)) {
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
    if (ctx->show_ipo && actkeyblock_is_valid(ab) && (ab->block.flag)) {
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
                          eBezTriple_KeyframeType(ak->key_type),
                          KEYFRAME_SHAPE_BOTH,
                          ctx->alpha,
                          sh_bindings,
                          handle_type,
                          extreme_type);
    }
  }
}

/* *************************** Drawing Stack *************************** */
enum class ChannelType {
  SUMMARY,
  SCENE,
  OBJECT,
  FCURVE,
  ACTION_LAYERED,
  ACTION_SLOT,
  ACTION_LEGACY,
  ACTION_GROUP,
  GREASE_PENCIL_CELS,
  GREASE_PENCIL_GROUP,
  GREASE_PENCIL_DATA,
  GREASE_PENCIL_LAYER,
  MASK_LAYER,
};

struct ChannelListElement {
  ChannelListElement *next, *prev;
  AnimKeylist *keylist;
  ChannelType type;

  float yscale_fac;
  float ypos;
  eSAction_Flag saction_flag;
  bool channel_locked;

  /* Currently only used for F-Curve channels, because some should be nla
   * remapped but not others. All other channel types ignore this, as it's clear
   * from the type whether they should be nla remapped or not. */
  bool use_nla_remapping;

  /* TODO: check which of these can be put into a `union`: */
  bAnimContext *ac;
  bDopeSheet *ads;
  Scene *sce;
  Object *ob;
  ID *animated_id; /* The ID that adt (below) belongs to. */
  AnimData *adt;
  FCurve *fcu;
  bAction *act;
  animrig::Slot *action_slot;
  bActionGroup *agrp;
  bGPDlayer *gpl;
  const GreasePencilLayer *grease_pencil_layer;
  const GreasePencilLayerTreeGroup *grease_pencil_layer_group;
  const GreasePencil *grease_pencil;
  MaskLayer *masklay;
};

static void build_channel_keylist(ChannelListElement *elem, blender::float2 range)
{
  switch (elem->type) {
    case ChannelType::SUMMARY: {
      summary_to_keylist(elem->ac, elem->keylist, elem->saction_flag, range);
      break;
    }
    case ChannelType::SCENE: {
      scene_to_keylist(elem->ads, elem->sce, elem->keylist, elem->saction_flag, range);
      break;
    }
    case ChannelType::OBJECT: {
      ob_to_keylist(elem->ads, elem->ob, elem->keylist, elem->saction_flag, range);
      break;
    }
    case ChannelType::FCURVE: {
      fcurve_to_keylist(
          elem->adt, elem->fcu, elem->keylist, elem->saction_flag, range, elem->use_nla_remapping);
      break;
    }
    case ChannelType::ACTION_LAYERED: {
      /* This is only called for action summaries in the Dope-sheet, *not* the
       * Action Editor. Therefore despite the name `ACTION_LAYERED`, this is
       * only used to show a *single slot* of the action: the slot used by the
       * ID the action is listed under.
       *
       * Thus we use the same function as the `ChannelType::ACTION_SLOT` case
       * below because in practice the only distinction between these cases is
       * where they get the slot from. In this case, we get it from `elem`'s
       * ADT. */
      BLI_assert(elem->act);
      BLI_assert(elem->adt);
      action_slot_summary_to_keylist(elem->ac,
                                     elem->animated_id,
                                     elem->act->wrap(),
                                     elem->adt->slot_handle,
                                     elem->keylist,
                                     elem->saction_flag,
                                     range);
      break;
    }
    case ChannelType::ACTION_SLOT: {
      BLI_assert(elem->act);
      BLI_assert(elem->action_slot);
      action_slot_summary_to_keylist(elem->ac,
                                     elem->animated_id,
                                     elem->act->wrap(),
                                     elem->action_slot->handle,
                                     elem->keylist,
                                     elem->saction_flag,
                                     range);
      break;
    }
    case ChannelType::ACTION_LEGACY: {
      action_to_keylist(elem->adt, elem->act, elem->keylist, elem->saction_flag, range);
      break;
    }
    case ChannelType::ACTION_GROUP: {
      action_group_to_keylist(elem->adt, elem->agrp, elem->keylist, elem->saction_flag, range);
      break;
    }
    case ChannelType::GREASE_PENCIL_CELS: {
      grease_pencil_cels_to_keylist(
          elem->adt, elem->grease_pencil_layer, elem->keylist, elem->saction_flag);
      break;
    }
    case ChannelType::GREASE_PENCIL_GROUP: {
      grease_pencil_layer_group_to_keylist(
          elem->adt, elem->grease_pencil_layer_group, elem->keylist, elem->saction_flag);
      break;
    }
    case ChannelType::GREASE_PENCIL_DATA: {
      if (elem->ac->datatype != ANIMCONT_GPENCIL && elem->adt) {
        action_to_keylist(elem->adt, elem->adt->action, elem->keylist, elem->saction_flag, range);
      }
      grease_pencil_data_block_to_keylist(
          elem->adt, elem->grease_pencil, elem->keylist, elem->saction_flag, false);
      break;
    }
    case ChannelType::GREASE_PENCIL_LAYER: {
      gpl_to_keylist(elem->ads, elem->gpl, elem->keylist);
      break;
    }
    case ChannelType::MASK_LAYER: {
      mask_to_keylist(elem->ads, elem->masklay, elem->keylist);
      break;
    }
  }
}

static void draw_channel_blocks(ChannelListElement *elem, View2D *v2d)
{
  DrawKeylistUIData ctx;
  channel_ui_data_init(&ctx, v2d, elem->yscale_fac, elem->channel_locked, elem->saction_flag);

  const int key_len = ED_keylist_array_len(elem->keylist);
  const ActKeyColumn *keys = ED_keylist_array(elem->keylist);
  draw_keylist_blocks(&ctx, keys, key_len, elem->ypos);
}

static void draw_channel_keys(ChannelListElement *elem,
                              View2D *v2d,
                              const KeyframeShaderBindings *sh_bindings)
{
  DrawKeylistUIData ctx;
  channel_ui_data_init(&ctx, v2d, elem->yscale_fac, elem->channel_locked, elem->saction_flag);

  const int key_len = ED_keylist_array_len(elem->keylist);
  const ActKeyColumn *keys = ED_keylist_array(elem->keylist);
  draw_keylist_keys(&ctx, v2d, sh_bindings, keys, key_len, elem->ypos, elem->saction_flag);
}

static void prepare_channel_for_drawing(ChannelListElement *elem)
{
  ED_keylist_prepare_for_direct_access(elem->keylist);
}

/** List of channels that are actually drawn because they are in view. */
struct ChannelDrawList {
  ListBase /*ChannelListElement*/ channels;
};

ChannelDrawList *ED_channel_draw_list_create()
{
  return MEM_callocN<ChannelDrawList>(__func__);
}

static void channel_list_build_keylists(ChannelDrawList *channel_list, blender::float2 range)
{
  LISTBASE_FOREACH (ChannelListElement *, elem, &channel_list->channels) {
    build_channel_keylist(elem, range);
    prepare_channel_for_drawing(elem);
  }
}

static void channel_list_draw_blocks(ChannelDrawList *channel_list, View2D *v2d)
{
  LISTBASE_FOREACH (ChannelListElement *, elem, &channel_list->channels) {
    draw_channel_blocks(elem, v2d);
  }
}

static int channel_visible_key_len(const View2D *v2d, const ListBase * /*ActKeyColumn*/ keys)
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

static int channel_list_visible_key_len(const ChannelDrawList *channel_list, const View2D *v2d)
{
  uint len = 0;
  LISTBASE_FOREACH (ChannelListElement *, elem, &channel_list->channels) {
    const ListBase *keys = ED_keylist_listbase(elem->keylist);
    len += channel_visible_key_len(v2d, keys);
  }
  return len;
}

static void channel_list_draw_keys(ChannelDrawList *channel_list, View2D *v2d)
{
  const int visible_key_len = channel_list_visible_key_len(channel_list, v2d);
  if (visible_key_len == 0) {
    return;
  }

  GPU_blend(GPU_BLEND_ALPHA);

  GPUVertFormat *format = immVertexFormat();
  KeyframeShaderBindings sh_bindings;

  sh_bindings.pos_id = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  sh_bindings.size_id = GPU_vertformat_attr_add(
      format, "size", blender::gpu::VertAttrType::SFLOAT_32);
  sh_bindings.color_id = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::UNORM_8_8_8_8);
  sh_bindings.outline_color_id = GPU_vertformat_attr_add(
      format, "outlineColor", blender::gpu::VertAttrType::UNORM_8_8_8_8);
  sh_bindings.flags_id = GPU_vertformat_attr_add(
      format, "flags", blender::gpu::VertAttrType::UINT_32);

  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", 1.0f);
  immUniform2f("ViewportSize", BLI_rcti_size_x(&v2d->mask) + 1, BLI_rcti_size_y(&v2d->mask) + 1);
  immBegin(GPU_PRIM_POINTS, visible_key_len);

  LISTBASE_FOREACH (ChannelListElement *, elem, &channel_list->channels) {
    draw_channel_keys(elem, v2d, &sh_bindings);
  }

  immEnd();
  GPU_program_point_size(false);
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

static void channel_list_draw(ChannelDrawList *channel_list, View2D *v2d)
{
  channel_list_draw_blocks(channel_list, v2d);
  channel_list_draw_keys(channel_list, v2d);
}

void ED_channel_list_flush(ChannelDrawList *channel_list, View2D *v2d)
{
  channel_list_build_keylists(channel_list, {v2d->cur.xmin, v2d->cur.xmax});
  channel_list_draw(channel_list, v2d);
}

void ED_channel_list_free(ChannelDrawList *channel_list)
{
  LISTBASE_FOREACH (ChannelListElement *, elem, &channel_list->channels) {
    ED_keylist_free(elem->keylist);
  }
  BLI_freelistN(&channel_list->channels);
  MEM_freeN(channel_list);
}

static ChannelListElement *channel_list_add_element(ChannelDrawList *channel_list,
                                                    ChannelType elem_type,
                                                    float ypos,
                                                    float yscale_fac,
                                                    eSAction_Flag saction_flag)
{
  ChannelListElement *draw_elem = MEM_callocN<ChannelListElement>(__func__);
  BLI_addtail(&channel_list->channels, draw_elem);
  draw_elem->type = elem_type;
  draw_elem->keylist = ED_keylist_create();
  draw_elem->ypos = ypos;
  draw_elem->yscale_fac = yscale_fac;
  draw_elem->saction_flag = saction_flag;
  return draw_elem;
}

/* *************************** Channel Drawing Functions *************************** */

void ED_add_summary_channel(ChannelDrawList *channel_list,
                            bAnimContext *ac,
                            float ypos,
                            float yscale_fac,
                            int saction_flag)
{
  saction_flag &= ~SACTION_SHOW_EXTREMES;
  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::SUMMARY, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->ac = ac;
}

void ED_add_scene_channel(ChannelDrawList *channel_list,
                          bDopeSheet *ads,
                          Scene *sce,
                          float ypos,
                          float yscale_fac,
                          int saction_flag)
{
  saction_flag &= ~SACTION_SHOW_EXTREMES;
  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::SCENE, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->ads = ads;
  draw_elem->sce = sce;
}

void ED_add_object_channel(ChannelDrawList *channel_list,
                           bDopeSheet *ads,
                           Object *ob,
                           float ypos,
                           float yscale_fac,
                           int saction_flag)
{
  saction_flag &= ~SACTION_SHOW_EXTREMES;
  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::OBJECT, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->ads = ads;
  draw_elem->ob = ob;
}

void ED_add_fcurve_channel(ChannelDrawList *channel_list,
                           bAnimListElem *ale,
                           FCurve *fcu,
                           float ypos,
                           float yscale_fac,
                           int saction_flag)
{
  const bool locked = (fcu->flag & FCURVE_PROTECTED) ||
                      ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) ||
                      ((ale->adt && ale->adt->action) &&
                       (!ID_IS_EDITABLE(ale->adt->action) ||
                        ID_IS_OVERRIDE_LIBRARY(ale->adt->action)));

  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::FCURVE, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->animated_id = ale->id;
  draw_elem->adt = ale->adt;
  draw_elem->fcu = fcu;
  draw_elem->channel_locked = locked;
  draw_elem->use_nla_remapping = ANIM_nla_mapping_allowed(ale);
}

void ED_add_action_group_channel(ChannelDrawList *channel_list,
                                 bAnimListElem *ale,
                                 bActionGroup *agrp,
                                 float ypos,
                                 float yscale_fac,
                                 int saction_flag)
{
  bool locked = (agrp->flag & AGRP_PROTECTED) ||
                ((ale->adt && ale->adt->action) &&
                 (!ID_IS_EDITABLE(ale->adt->action) || ID_IS_OVERRIDE_LIBRARY(ale->adt->action)));

  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::ACTION_GROUP, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->animated_id = ale->id;
  draw_elem->adt = ale->adt;
  draw_elem->agrp = agrp;
  draw_elem->channel_locked = locked;
}

void ED_add_action_layered_channel(ChannelDrawList *channel_list,
                                   bAnimContext *ac,
                                   bAnimListElem *ale,
                                   bAction *action,
                                   const float ypos,
                                   const float yscale_fac,
                                   int saction_flag)
{
  BLI_assert(action);
  BLI_assert(action->wrap().is_action_layered());

  const bool locked = (!ID_IS_EDITABLE(action) || ID_IS_OVERRIDE_LIBRARY(action));
  saction_flag &= ~SACTION_SHOW_EXTREMES;

  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::ACTION_LAYERED, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->ac = ac;
  draw_elem->animated_id = ale->id;
  draw_elem->adt = ale->adt;
  draw_elem->act = action;
  draw_elem->channel_locked = locked;
}

void ED_add_action_slot_channel(ChannelDrawList *channel_list,
                                bAnimContext *ac,
                                bAnimListElem *ale,
                                animrig::Action &action,
                                animrig::Slot &slot,
                                const float ypos,
                                const float yscale_fac,
                                int saction_flag)
{
  const bool locked = (ID_IS_LINKED(&action) || ID_IS_OVERRIDE_LIBRARY(&action));
  saction_flag &= ~SACTION_SHOW_EXTREMES;

  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::ACTION_SLOT, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->ac = ac;
  draw_elem->animated_id = ale->id;
  draw_elem->adt = ale->adt;
  draw_elem->act = &action;
  draw_elem->action_slot = &slot;
  draw_elem->channel_locked = locked;
}

void ED_add_action_channel(ChannelDrawList *channel_list,
                           bAnimListElem *ale,
                           bAction *act,
                           float ypos,
                           float yscale_fac,
                           int saction_flag)
{
  BLI_assert(!act || act->wrap().is_action_legacy());

  const bool locked = (act && (!ID_IS_EDITABLE(act) || ID_IS_OVERRIDE_LIBRARY(act)));
  saction_flag &= ~SACTION_SHOW_EXTREMES;

  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::ACTION_LEGACY, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->animated_id = ale->id;
  draw_elem->adt = ale->adt;
  draw_elem->act = act;
  draw_elem->channel_locked = locked;
}

void ED_add_grease_pencil_datablock_channel(ChannelDrawList *channel_list,
                                            bAnimContext *ac,
                                            bAnimListElem *ale,
                                            const GreasePencil *grease_pencil,
                                            const float ypos,
                                            const float yscale_fac,
                                            int saction_flag)
{
  ChannelListElement *draw_elem = channel_list_add_element(channel_list,
                                                           ChannelType::GREASE_PENCIL_DATA,
                                                           ypos,
                                                           yscale_fac,
                                                           eSAction_Flag(saction_flag));
  /* GreasePencil properties can be animated via an Action, so the GP-related
   * animation data is not limited to GP drawings. */
  draw_elem->animated_id = ale->id;
  draw_elem->adt = ale->adt;
  draw_elem->act = ale->adt ? ale->adt->action : nullptr;
  draw_elem->grease_pencil = grease_pencil;
  draw_elem->ac = ac;
}

void ED_add_grease_pencil_cels_channel(ChannelDrawList *channel_list,
                                       bDopeSheet *ads,
                                       const GreasePencilLayer *layer,
                                       const float ypos,
                                       const float yscale_fac,
                                       int saction_flag)
{
  ChannelListElement *draw_elem = channel_list_add_element(channel_list,
                                                           ChannelType::GREASE_PENCIL_CELS,
                                                           ypos,
                                                           yscale_fac,
                                                           eSAction_Flag(saction_flag));
  draw_elem->ads = ads;
  draw_elem->grease_pencil_layer = layer;
  draw_elem->channel_locked = layer->wrap().is_locked();
}

void ED_add_grease_pencil_layer_group_channel(ChannelDrawList *channel_list,
                                              bDopeSheet *ads,
                                              const GreasePencilLayerTreeGroup *layer_group,
                                              const float ypos,
                                              const float yscale_fac,
                                              int saction_flag)
{
  ChannelListElement *draw_elem = channel_list_add_element(channel_list,
                                                           ChannelType::GREASE_PENCIL_GROUP,
                                                           ypos,
                                                           yscale_fac,
                                                           eSAction_Flag(saction_flag));
  draw_elem->ads = ads;
  draw_elem->grease_pencil_layer_group = layer_group;
  draw_elem->channel_locked = layer_group->wrap().is_locked();
}

void ED_add_grease_pencil_layer_legacy_channel(ChannelDrawList *channel_list,
                                               bDopeSheet *ads,
                                               bGPDlayer *gpl,
                                               float ypos,
                                               float yscale_fac,
                                               int saction_flag)
{
  bool locked = (gpl->flag & GP_LAYER_LOCKED) != 0;
  ChannelListElement *draw_elem = channel_list_add_element(channel_list,
                                                           ChannelType::GREASE_PENCIL_LAYER,
                                                           ypos,
                                                           yscale_fac,
                                                           eSAction_Flag(saction_flag));
  draw_elem->ads = ads;
  draw_elem->gpl = gpl;
  draw_elem->channel_locked = locked;
}

void ED_add_mask_layer_channel(ChannelDrawList *channel_list,
                               bDopeSheet *ads,
                               MaskLayer *masklay,
                               float ypos,
                               float yscale_fac,
                               int saction_flag)
{
  bool locked = (masklay->flag & MASK_LAYERFLAG_LOCKED) != 0;
  ChannelListElement *draw_elem = channel_list_add_element(
      channel_list, ChannelType::MASK_LAYER, ypos, yscale_fac, eSAction_Flag(saction_flag));
  draw_elem->ads = ads;
  draw_elem->masklay = masklay;
  draw_elem->channel_locked = locked;
}
