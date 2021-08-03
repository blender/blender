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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

/** \file
 * \ingroup edanimation
 */

/* System includes ----------------------------------------------------- */

#include <float.h>

#include "BLI_dlrbTree.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "GPU_immediate.h"
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
                         uint pos_id,
                         uint size_id,
                         uint color_id,
                         uint outline_color_id,
                         uint flags_id,
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
        flags = 0x2;
        break; /* circle */
      case KEYFRAME_HANDLE_AUTO:
        flags = 0x12;
        break; /* circle with dot */
      case KEYFRAME_HANDLE_VECTOR:
        flags = 0xC;
        break; /* square */
      case KEYFRAME_HANDLE_ALIGNED:
        flags = 0x5;
        break; /* clipped diamond */

      case KEYFRAME_HANDLE_FREE:
      default:
        flags = 1; /* diamond */
    }

    /* Extreme type to arrow-like shading. */
    if (extreme_type & KEYFRAME_EXTREME_MAX) {
      flags |= 0x100;
    }
    if (extreme_type & KEYFRAME_EXTREME_MIN) {
      flags |= 0x200;
    }
    if (extreme_type & KEYFRAME_EXTREME_MIXED) {
      flags |= 0x400;
    }
  }

  immAttr1f(size_id, size);
  immAttr4ubv(color_id, fill_col);
  immAttr4ubv(outline_color_id, outline_col);
  immAttr1u(flags_id, flags);
  immVertex2f(pos_id, x, y);
}

static void draw_keylist(View2D *v2d,
                         const struct AnimKeylist *keylist,
                         float ypos,
                         float yscale_fac,
                         bool channelLocked,
                         int saction_flag)
{
  const float icon_sz = U.widget_unit * 0.5f * yscale_fac;
  const float half_icon_sz = 0.5f * icon_sz;
  const float smaller_sz = 0.35f * icon_sz;
  const float ipo_sz = 0.1f * icon_sz;
  const float gpencil_sz = smaller_sz * 0.8f;
  const float screenspace_margin = (0.35f * (float)UI_UNIT_X) / UI_view2d_scale_get_x(v2d);

  /* locked channels are less strongly shown, as feedback for locked channels in DopeSheet */
  /* TODO: allow this opacity factor to be themed? */
  float alpha = channelLocked ? 0.25f : 1.0f;

  /* Show interpolation and handle type? */
  bool show_ipo = (saction_flag & SACTION_SHOW_INTERPOLATION) != 0;
  /* draw keyblocks */
  float sel_color[4], unsel_color[4];
  float sel_mhcol[4], unsel_mhcol[4];
  float ipo_color[4], ipo_color_mix[4];

  /* cache colors first */
  UI_GetThemeColor4fv(TH_STRIP_SELECT, sel_color);
  UI_GetThemeColor4fv(TH_STRIP, unsel_color);
  UI_GetThemeColor4fv(TH_DOPESHEET_IPOLINE, ipo_color);

  sel_color[3] *= alpha;
  unsel_color[3] *= alpha;
  ipo_color[3] *= alpha;

  copy_v4_v4(sel_mhcol, sel_color);
  sel_mhcol[3] *= 0.8f;
  copy_v4_v4(unsel_mhcol, unsel_color);
  unsel_mhcol[3] *= 0.8f;
  copy_v4_v4(ipo_color_mix, ipo_color);
  ipo_color_mix[3] *= 0.5f;

  const ListBase *keys = ED_keylist_listbase(keylist);

  LISTBASE_FOREACH (ActKeyColumn *, ab, keys) {
    /* Draw grease pencil bars between keyframes. */
    if ((ab->next != NULL) && (ab->block.flag & ACTKEYBLOCK_FLAG_GPENCIL)) {
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
              .xmax = min_ff(ab->next->cfra - (screenspace_margin * size), ab->next->cfra),
              .ymin = ypos - gpencil_sz,
              .ymax = ypos + gpencil_sz,
          },
          true,
          0.25f * (float)UI_UNIT_X,
          (ab->block.sel) ? sel_mhcol : unsel_mhcol);
    }
    else {
      /* Draw other types. */
      UI_draw_roundbox_corner_set(UI_CNR_NONE);

      int valid_hold = actkeyblock_get_valid_hold(ab);
      if (valid_hold != 0) {
        if ((valid_hold & ACTKEYBLOCK_FLAG_STATIC_HOLD) == 0) {
          /* draw "moving hold" long-keyframe block - slightly smaller */
          UI_draw_roundbox_4fv(
              &(const rctf){
                  .xmin = ab->cfra,
                  .xmax = ab->next->cfra,
                  .ymin = ypos - smaller_sz,
                  .ymax = ypos + smaller_sz,
              },
              true,
              3.0f,
              (ab->block.sel) ? sel_mhcol : unsel_mhcol);
        }
        else {
          /* draw standard long-keyframe block */
          UI_draw_roundbox_4fv(
              &(const rctf){
                  .xmin = ab->cfra,
                  .xmax = ab->next->cfra,
                  .ymin = ypos - half_icon_sz,
                  .ymax = ypos + half_icon_sz,
              },
              true,
              3.0f,
              (ab->block.sel) ? sel_color : unsel_color);
        }
      }
      if (show_ipo && actkeyblock_is_valid(ab) && (ab->block.flag & ACTKEYBLOCK_FLAG_NON_BEZIER)) {
        /* draw an interpolation line */
        UI_draw_roundbox_4fv(
            &(const rctf){
                .xmin = ab->cfra,
                .xmax = ab->next->cfra,
                .ymin = ypos - ipo_sz,
                .ymax = ypos + ipo_sz,
            },
            true,
            3.0f,
            (ab->block.conflict & ACTKEYBLOCK_FLAG_NON_BEZIER) ? ipo_color_mix : ipo_color);
      }
    }
  }

  GPU_blend(GPU_BLEND_ALPHA);

  /* count keys */
  uint key_len = 0;
  LISTBASE_FOREACH (ActKeyColumn *, ak, keys) {
    /* Optimization: if keyframe doesn't appear within 5 units (screenspace)
     * in visible area, don't draw.
     * This might give some improvements,
     * since we current have to flip between view/region matrices.
     */
    if (IN_RANGE_INCL(ak->cfra, v2d->cur.xmin, v2d->cur.xmax)) {
      key_len++;
    }
  }

  if (key_len > 0) {
    /* draw keys */
    GPUVertFormat *format = immVertexFormat();
    uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    uint color_id = GPU_vertformat_attr_add(
        format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    uint outline_color_id = GPU_vertformat_attr_add(
        format, "outlineColor", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    uint flags_id = GPU_vertformat_attr_add(format, "flags", GPU_COMP_U32, 1, GPU_FETCH_INT);

    GPU_program_point_size(true);
    immBindBuiltinProgram(GPU_SHADER_KEYFRAME_DIAMOND);
    immUniform1f("outline_scale", 1.0f);
    immUniform2f("ViewportSize", BLI_rcti_size_x(&v2d->mask) + 1, BLI_rcti_size_y(&v2d->mask) + 1);
    immBegin(GPU_PRIM_POINTS, key_len);

    short handle_type = KEYFRAME_HANDLE_NONE, extreme_type = KEYFRAME_EXTREME_NONE;

    LISTBASE_FOREACH (ActKeyColumn *, ak, keys) {
      if (IN_RANGE_INCL(ak->cfra, v2d->cur.xmin, v2d->cur.xmax)) {
        if (show_ipo) {
          handle_type = ak->handle_type;
        }
        if (saction_flag & SACTION_SHOW_EXTREMES) {
          extreme_type = ak->extreme_type;
        }

        draw_keyframe_shape(ak->cfra,
                            ypos,
                            icon_sz,
                            (ak->sel & SELECT),
                            ak->key_type,
                            KEYFRAME_SHAPE_BOTH,
                            alpha,
                            pos_id,
                            size_id,
                            color_id,
                            outline_color_id,
                            flags_id,
                            handle_type,
                            extreme_type);
      }
    }

    immEnd();
    GPU_program_point_size(false);
    immUnbindProgram();
  }

  GPU_blend(GPU_BLEND_NONE);
}

/* *************************** Channel Drawing Funcs *************************** */

void draw_summary_channel(
    View2D *v2d, bAnimContext *ac, float ypos, float yscale_fac, int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  saction_flag &= ~SACTION_SHOW_EXTREMES;

  summary_to_keylist(ac, keylist, saction_flag);

  draw_keylist(v2d, keylist, ypos, yscale_fac, false, saction_flag);

  ED_keylist_free(keylist);
}

void draw_scene_channel(
    View2D *v2d, bDopeSheet *ads, Scene *sce, float ypos, float yscale_fac, int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  saction_flag &= ~SACTION_SHOW_EXTREMES;

  scene_to_keylist(ads, sce, keylist, saction_flag);

  draw_keylist(v2d, keylist, ypos, yscale_fac, false, saction_flag);

  ED_keylist_free(keylist);
}

void draw_object_channel(
    View2D *v2d, bDopeSheet *ads, Object *ob, float ypos, float yscale_fac, int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  saction_flag &= ~SACTION_SHOW_EXTREMES;

  ob_to_keylist(ads, ob, keylist, saction_flag);

  draw_keylist(v2d, keylist, ypos, yscale_fac, false, saction_flag);

  ED_keylist_free(keylist);
}

void draw_fcurve_channel(
    View2D *v2d, AnimData *adt, FCurve *fcu, float ypos, float yscale_fac, int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  bool locked = (fcu->flag & FCURVE_PROTECTED) ||
                ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) ||
                ((adt && adt->action) && ID_IS_LINKED(adt->action));

  fcurve_to_keylist(adt, fcu, keylist, saction_flag);

  draw_keylist(v2d, keylist, ypos, yscale_fac, locked, saction_flag);

  ED_keylist_free(keylist);
}

void draw_agroup_channel(
    View2D *v2d, AnimData *adt, bActionGroup *agrp, float ypos, float yscale_fac, int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  bool locked = (agrp->flag & AGRP_PROTECTED) ||
                ((adt && adt->action) && ID_IS_LINKED(adt->action));

  agroup_to_keylist(adt, agrp, keylist, saction_flag);

  draw_keylist(v2d, keylist, ypos, yscale_fac, locked, saction_flag);

  ED_keylist_free(keylist);
}

void draw_action_channel(
    View2D *v2d, AnimData *adt, bAction *act, float ypos, float yscale_fac, int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  bool locked = (act && ID_IS_LINKED(act));

  saction_flag &= ~SACTION_SHOW_EXTREMES;

  action_to_keylist(adt, act, keylist, saction_flag);

  draw_keylist(v2d, keylist, ypos, yscale_fac, locked, saction_flag);

  ED_keylist_free(keylist);
}

void draw_gpencil_channel(
    View2D *v2d, bDopeSheet *ads, bGPdata *gpd, float ypos, float yscale_fac, int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  saction_flag &= ~SACTION_SHOW_EXTREMES;

  gpencil_to_keylist(ads, gpd, keylist, false);

  draw_keylist(v2d, keylist, ypos, yscale_fac, false, saction_flag);

  ED_keylist_free(keylist);
}

void draw_gpl_channel(
    View2D *v2d, bDopeSheet *ads, bGPDlayer *gpl, float ypos, float yscale_fac, int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  bool locked = (gpl->flag & GP_LAYER_LOCKED) != 0;

  gpl_to_keylist(ads, gpl, keylist);

  draw_keylist(v2d, keylist, ypos, yscale_fac, locked, saction_flag);

  ED_keylist_free(keylist);
}

void draw_masklay_channel(View2D *v2d,
                          bDopeSheet *ads,
                          MaskLayer *masklay,
                          float ypos,
                          float yscale_fac,
                          int saction_flag)
{
  struct AnimKeylist *keylist = ED_keylist_create();

  bool locked = (masklay->flag & MASK_LAYERFLAG_LOCKED) != 0;

  mask_to_keylist(ads, masklay, keylist);

  draw_keylist(v2d, keylist, ypos, yscale_fac, locked, saction_flag);

  ED_keylist_free(keylist);
}
