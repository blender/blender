/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnla
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_blenlib.h"
#include "BLI_range.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_fcurve.hh"
#include "BKE_nla.h"

#include "ED_anim_api.hh"
#include "ED_keyframes_draw.hh"
#include "ED_keyframes_keylist.hh"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_state.h"

#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "nla_intern.hh" /* own include */
#include "nla_private.h"

/* *********************************************** */
/* Strips */

/* Action-Line ---------------------- */

void nla_action_get_color(AnimData *adt, bAction *act, float color[4])
{
  if (adt && (adt->flag & ADT_NLA_EDIT_ON)) {
    /* greenish color (same as tweaking strip) */
    UI_GetThemeColor4fv(TH_NLA_TWEAK, color);
  }
  else {
    if (act) {
      /* reddish color - same as dopesheet summary */
      UI_GetThemeColor4fv(TH_ANIM_ACTIVE, color);
    }
    else {
      /* grayish-red color */
      UI_GetThemeColor4fv(TH_ANIM_INACTIVE, color);
    }
  }

  /* when an NLA track is tagged "solo", action doesn't contribute,
   * so shouldn't be as prominent */
  if (adt && (adt->flag & ADT_NLA_SOLO_TRACK)) {
    color[3] *= 0.15f;
  }
}

/* draw the keyframes in the specified Action */
static void nla_action_draw_keyframes(
    View2D *v2d, AnimData *adt, bAction *act, float y, float ymin, float ymax)
{
  if (act == nullptr) {
    return;
  }

  /* get a list of the keyframes with NLA-scaling applied */
  AnimKeylist *keylist = ED_keylist_create();
  action_to_keylist(adt, act, keylist, 0, {-FLT_MAX, FLT_MAX});

  if (ED_keylist_is_empty(keylist)) {
    ED_keylist_free(keylist);
    return;
  }

  /* draw a darkened region behind the strips
   * - get and reset the background color, this time without the alpha to stand out better
   *   (amplified alpha is used instead, but clamped to avoid 100% opacity)
   */
  float color[4];
  nla_action_get_color(adt, act, color);
  color[3] = min_ff(0.7f, color[3] * 2.5f);

  GPUVertFormat *format = immVertexFormat();
  uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4fv(color);

  /* - draw a rect from the first to the last frame (no extra overlaps for now)
   *   that is slightly stumpier than the track background (hardcoded 2-units here)
   */

  Range2f frame_range;
  ED_keylist_all_keys_frame_range(keylist, &frame_range);
  immRectf(pos_id, frame_range.min, ymin + 2, frame_range.max, ymax - 2);
  immUnbindProgram();

  /* Count keys before drawing. */
  /* NOTE: It's safe to cast #DLRBT_Tree, as it's designed to degrade down to a #ListBase. */
  const ListBase *keys = ED_keylist_listbase(keylist);
  uint key_len = BLI_listbase_count(keys);

  if (key_len > 0) {
    format = immVertexFormat();
    KeyframeShaderBindings sh_bindings;
    sh_bindings.pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    sh_bindings.size_id = GPU_vertformat_attr_add(
        format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    sh_bindings.color_id = GPU_vertformat_attr_add(
        format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    sh_bindings.outline_color_id = GPU_vertformat_attr_add(
        format, "outlineColor", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    sh_bindings.flags_id = GPU_vertformat_attr_add(
        format, "flags", GPU_COMP_U32, 1, GPU_FETCH_INT);

    GPU_program_point_size(true);
    immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
    immUniform1f("outline_scale", 1.0f);
    immUniform2f("ViewportSize", BLI_rcti_size_x(&v2d->mask) + 1, BLI_rcti_size_y(&v2d->mask) + 1);
    immBegin(GPU_PRIM_POINTS, key_len);

    /* - disregard the selection status of keyframes so they draw a certain way
     * - size is 6.0f which is smaller than the editable keyframes, so that there is a distinction
     */
    LISTBASE_FOREACH (const ActKeyColumn *, ak, keys) {
      draw_keyframe_shape(ak->cfra,
                          y,
                          6.0f,
                          false,
                          ak->key_type,
                          KEYFRAME_SHAPE_FRAME,
                          1.0f,
                          &sh_bindings,
                          KEYFRAME_HANDLE_NONE,
                          KEYFRAME_EXTREME_NONE);
    }

    immEnd();
    GPU_program_point_size(false);
    immUnbindProgram();
  }

  /* free icons */
  ED_keylist_free(keylist);
}

/* Strip Markers ------------------------ */

/* Markers inside an action strip */
static void nla_actionclip_draw_markers(
    NlaStrip *strip, float yminc, float ymaxc, int shade, const bool dashed)
{
  const bAction *act = strip->act;

  if (ELEM(nullptr, act, act->markers.first)) {
    return;
  }

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  if (dashed) {
    immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f(
        "viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

    immUniform1i("colors_len", 0); /* "simple" mode */
    immUniform1f("dash_width", 6.0f);
    immUniform1f("udash_factor", 0.5f);
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  }
  immUniformThemeColorShade(TH_STRIP_SELECT, shade);

  immBeginAtMost(GPU_PRIM_LINES, BLI_listbase_count(&act->markers) * 2);
  LISTBASE_FOREACH (TimeMarker *, marker, &act->markers) {
    if ((marker->frame > strip->actstart) && (marker->frame < strip->actend)) {
      float frame = nlastrip_get_frame(strip, marker->frame, NLATIME_CONVERT_MAP);

      /* just a simple line for now */
      /* XXX: draw a triangle instead... */
      immVertex2f(shdr_pos, frame, yminc + 1);
      immVertex2f(shdr_pos, frame, ymaxc - 1);
    }
  }
  immEnd();

  immUnbindProgram();
}

/* Markers inside a NLA-Strip */
static void nla_strip_draw_markers(NlaStrip *strip, float yminc, float ymaxc)
{
  GPU_line_width(2.0f);

  if (strip->type == NLASTRIP_TYPE_CLIP) {
    /* try not to be too conspicuous, while being visible enough when transforming */
    int shade = (strip->flag & NLASTRIP_FLAG_SELECT) ? -60 : -40;

    /* just draw the markers in this clip */
    nla_actionclip_draw_markers(strip, yminc, ymaxc, shade, true);
  }
  else if (strip->flag & NLASTRIP_FLAG_TEMP_META) {
    /* just a solid color, so that it is very easy to spot */
    int shade = 20;
    /* draw the markers in the first level of strips only (if they are actions) */
    LISTBASE_FOREACH (NlaStrip *, nls, &strip->strips) {
      if (nls->type == NLASTRIP_TYPE_CLIP) {
        nla_actionclip_draw_markers(nls, yminc, ymaxc, shade, false);
      }
    }
  }

  GPU_line_width(1.0f);
}

/* Strips (Proper) ---------------------- */

/* Get colors for drawing NLA Strips. */
static void nla_strip_get_color_inside(AnimData *adt, NlaStrip *strip, float color[3])
{
  const bool is_selected = strip->flag & NLASTRIP_FLAG_SELECT;
  switch (strip->type) {
    case NLASTRIP_TYPE_CLIP:
      /* Action Strip. */
      if (adt && (adt->flag & ADT_NLA_EDIT_ON) && (adt->actstrip == strip)) {
        /* Active strip tweak - tweak theme is applied only to active edit strip,
         * not linked-duplicates.
         */
        UI_GetThemeColor3fv(TH_NLA_TWEAK, color);
        break;
      }

      if (strip->flag & NLASTRIP_FLAG_TWEAKUSER) {
        /* Non-active strip tweak - display warning theme
         * for non active linked-duplicates.
         */
        UI_GetThemeColor3fv(TH_NLA_TWEAK_DUPLI, color);
        break;
      }
      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        /* selected. */
        UI_GetThemeColor3fv(TH_STRIP_SELECT, color);
        break;
      }

      /* unselected - use standard strip theme. */
      UI_GetThemeColor3fv(TH_STRIP, color);
      break;

    case NLASTRIP_TYPE_META:
      /* Meta Strip. */
      UI_GetThemeColor3fv(is_selected ? TH_NLA_META_SEL : TH_NLA_META, color);
      break;
    case NLASTRIP_TYPE_TRANSITION: {
      /* Transition Strip. */
      UI_GetThemeColor3fv(is_selected ? TH_NLA_TRANSITION_SEL : TH_NLA_TRANSITION, color);
      break;
    }
    case NLASTRIP_TYPE_SOUND:
      /* Sound Strip. */
      UI_GetThemeColor3fv(is_selected ? TH_NLA_SOUND_SEL : TH_NLA_SOUND, color);
      break;
    default: {
      /* default to unselected theme. */
      UI_GetThemeColor3fv(TH_STRIP, color);
    } break;
  }
}

/* helper call for drawing influence/time control curves for a given NLA-strip */
static void nla_draw_strip_curves(NlaStrip *strip, float yminc, float ymaxc, uint pos)
{
  const float yheight = ymaxc - yminc;

  /* draw with AA'd line */
  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);

  /* Fully opaque line on selected strips. */
  if (strip->flag & NLASTRIP_FLAG_SELECT) {
    /* TODO: Use theme setting. */
    immUniformColor3f(1.0f, 1.0f, 1.0f);
  }
  else {
    immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  }

  /* influence -------------------------- */
  if (strip->flag & NLASTRIP_FLAG_USR_INFLUENCE) {
    FCurve *fcu = BKE_fcurve_find(&strip->fcurves, "influence", 0);

    /* plot the curve (over the strip's main region) */
    if (fcu) {
      immBegin(GPU_PRIM_LINE_STRIP, abs(int(strip->end - strip->start) + 1));

      /* sample at 1 frame intervals, and draw
       * - min y-val is yminc, max is y-maxc, so clamp in those regions
       */
      for (float cfra = strip->start; cfra <= strip->end; cfra += 1.0f) {
        float y = evaluate_fcurve(fcu, cfra); /* assume this to be in 0-1 range */
        CLAMP(y, 0.0f, 1.0f);
        immVertex2f(pos, cfra, ((y * yheight) + yminc));
      }

      immEnd();
    }
  }
  else {
    /* use blend in/out values only if both aren't zero */
    if ((IS_EQF(strip->blendin, 0.0f) && IS_EQF(strip->blendout, 0.0f)) == 0) {
      immBeginAtMost(GPU_PRIM_LINE_STRIP, 4);

      /* start of strip - if no blendin, start straight at 1,
       * otherwise from 0 to 1 over blendin frames */
      if (IS_EQF(strip->blendin, 0.0f) == 0) {
        immVertex2f(pos, strip->start, yminc);
        immVertex2f(pos, strip->start + strip->blendin, ymaxc);
      }
      else {
        immVertex2f(pos, strip->start, ymaxc);
      }

      /* end of strip */
      if (IS_EQF(strip->blendout, 0.0f) == 0) {
        immVertex2f(pos, strip->end - strip->blendout, ymaxc);
        immVertex2f(pos, strip->end, yminc);
      }
      else {
        immVertex2f(pos, strip->end, ymaxc);
      }

      immEnd();
    }
  }

  /* turn off AA'd lines */
  GPU_line_smooth(false);
  GPU_blend(GPU_BLEND_NONE);
}

/* helper call to setup dashed-lines for strip outlines */
static uint nla_draw_use_dashed_outlines(const float color[4], bool muted)
{
  /* Note that we use dashed shader here, and make it draw solid lines if not muted... */
  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

  immUniform1i("colors_len", 0); /* Simple dashes. */
  immUniformColor3fv(color);

  /* line style: dotted for muted */
  if (muted) {
    /* dotted - and slightly thicker for readability of the dashes */
    immUniform1f("dash_width", 5.0f);
    immUniform1f("udash_factor", 0.4f);
    GPU_line_width(1.5f);
  }
  else {
    /* solid line */
    immUniform1f("udash_factor", 2.0f);
    GPU_line_width(1.0f);
  }

  return shdr_pos;
}

/**
 * This check only accounts for the track's disabled flag and whether the strip is being tweaked.
 * It does not account for muting or soloing.
 */
static bool is_nlastrip_enabled(AnimData *adt, NlaTrack *nlt, NlaStrip *strip)
{
  /** This shouldn't happen. If passed nullptr, then just treat strip as enabled. */
  BLI_assert(adt);
  if (!adt) {
    return true;
  }

  if ((nlt->flag & NLATRACK_DISABLED) == 0) {
    return true;
  }

  /** For disabled tracks, only the tweaked strip is enabled. */
  return adt->actstrip == strip;
}

/* main call for drawing a single NLA-strip */
static void nla_draw_strip(SpaceNla *snla,
                           AnimData *adt,
                           NlaTrack *nlt,
                           NlaStrip *strip,
                           View2D *v2d,
                           float yminc,
                           float ymaxc)
{
  const bool solo = !((adt && (adt->flag & ADT_NLA_SOLO_TRACK)) &&
                      (nlt->flag & NLATRACK_SOLO) == 0);

  const bool muted = ((nlt->flag & NLATRACK_MUTED) || (strip->flag & NLASTRIP_FLAG_MUTED));
  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  uint shdr_pos;

  /* get color of strip */
  nla_strip_get_color_inside(adt, strip, color);

  shdr_pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* draw extrapolation info first (as backdrop)
   * - but this should only be drawn if track has some contribution
   */
  if ((strip->extendmode != NLASTRIP_EXTEND_NOTHING) && solo) {
    /* enable transparency... */
    GPU_blend(GPU_BLEND_ALPHA);

    switch (strip->extendmode) {
      /* since this does both sides,
       * only do the 'before' side, and leave the rest to the next case */
      case NLASTRIP_EXTEND_HOLD:
        /* only need to draw here if there's no strip before since
         * it only applies in such a situation
         */
        if (strip->prev == nullptr) {
          /* set the drawing color to the color of the strip, but with very faint alpha */
          immUniformColor3fvAlpha(color, 0.15f);

          /* draw the rect to the edge of the screen */
          immRectf(shdr_pos, v2d->cur.xmin, yminc, strip->start, ymaxc);
        }
        ATTR_FALLTHROUGH;

      /* this only draws after the strip */
      case NLASTRIP_EXTEND_HOLD_FORWARD:
        /* only need to try and draw if the next strip doesn't occur immediately after */
        if ((strip->next == nullptr) || (IS_EQF(strip->next->start, strip->end) == 0)) {
          /* set the drawing color to the color of the strip, but this time less faint */
          immUniformColor3fvAlpha(color, 0.3f);

          /* draw the rect to the next strip or the edge of the screen */
          float x2 = strip->next ? strip->next->start : v2d->cur.xmax;
          immRectf(shdr_pos, strip->end, yminc, x2, ymaxc);
        }
        break;
    }

    GPU_blend(GPU_BLEND_NONE);
  }

  /* draw 'inside' of strip itself */
  if (solo && is_nlastrip_enabled(adt, nlt, strip) &&
      !(strip->flag & NLASTRIP_FLAG_INVALID_LOCATION))
  {
    immUnbindProgram();

    /* strip is in normal track */
    UI_draw_roundbox_corner_set(UI_CNR_ALL); /* all corners rounded */
    rctf rect;
    rect.xmin = strip->start;
    rect.xmax = strip->end;
    rect.ymin = yminc;
    rect.ymax = ymaxc;
    UI_draw_roundbox_4fv(&rect, true, 0.0f, color);

    /* restore current vertex format & program (roundbox trashes it) */
    shdr_pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  }
  else {
    /* strip is in disabled track - make less visible */
    immUniformColor3fvAlpha(color, 0.1f);

    GPU_blend(GPU_BLEND_ALPHA);
    immRectf(shdr_pos, strip->start, yminc, strip->end, ymaxc);
    GPU_blend(GPU_BLEND_NONE);
  }

  /* draw strip's control 'curves'
   * - only if user hasn't hidden them...
   */
  if ((snla->flag & SNLA_NOSTRIPCURVES) == 0) {
    nla_draw_strip_curves(strip, yminc, ymaxc, shdr_pos);
  }

  immUnbindProgram();

  /* draw markings indicating locations of local markers
   * (useful for lining up different actions) */
  if ((snla->flag & SNLA_NOLOCALMARKERS) == 0) {
    nla_strip_draw_markers(strip, yminc, ymaxc);
  }

  /* draw strip outline
   * - color used here is to indicate active vs non-active
   */
  if (strip->flag & NLASTRIP_FLAG_INVALID_LOCATION) {
    color[0] = 1.0f;
    color[1] = color[2] = 0.15f;
  }
  else if (strip->flag & NLASTRIP_FLAG_ACTIVE) {
    /* strip should appear 'sunken', so draw a light border around it */
    color[0] = color[1] = color[2] = 1.0f; /* FIXME: hardcoded temp-hack colors */
  }
  else {
    /* strip should appear to stand out, so draw a dark border around it */
    color[0] = color[1] = color[2] = 0.0f; /* FIXME: or 1.0f ?? */
  }

  /* draw outline
   * - dashed-line shader is loaded after this block
   */
  if (muted) {
    /* muted - draw dotted, squarish outline (for simplicity) */
    shdr_pos = nla_draw_use_dashed_outlines(color, muted);
    imm_draw_box_wire_2d(shdr_pos, strip->start, yminc, strip->end, ymaxc);
  }
  else {
    /* non-muted - draw solid, rounded outline */
    rctf rect;
    rect.xmin = strip->start;
    rect.xmax = strip->end;
    rect.ymin = yminc;
    rect.ymax = ymaxc;
    UI_draw_roundbox_4fv(&rect, false, 0.0f, color);

    /* restore current vertex format & program (roundbox trashes it) */
    shdr_pos = nla_draw_use_dashed_outlines(color, muted);
  }

  /* if action-clip strip, draw lines delimiting repeats too (in the same color as outline) */
  if ((strip->type == NLASTRIP_TYPE_CLIP) && strip->repeat > 1.0f) {
    float repeatLen = (strip->actend - strip->actstart) * strip->scale;

    /* only draw lines for whole-numbered repeats, starting from the first full-repeat
     * up to the last full repeat (but not if it lies on the end of the strip)
     */
    immBeginAtMost(GPU_PRIM_LINES, 2 * floorf(strip->repeat));
    for (int i = 1; i < strip->repeat; i++) {
      float repeatPos = strip->start + (repeatLen * i);

      /* don't draw if line would end up on or after the end of the strip */
      if (repeatPos < strip->end) {
        immVertex2f(shdr_pos, repeatPos, yminc + 4);
        immVertex2f(shdr_pos, repeatPos, ymaxc - 4);
      }
    }
    immEnd();
  }
  /* or if meta-strip, draw lines delimiting extents of sub-strips
   * (in same color as outline, if more than 1 exists) */
  else if ((strip->type == NLASTRIP_TYPE_META) && (strip->strips.first != strip->strips.last)) {
    const float y = (ymaxc - yminc) * 0.5f + yminc;

    /* up to 2 lines per strip */
    immBeginAtMost(GPU_PRIM_LINES, 4 * BLI_listbase_count(&strip->strips));

    /* only draw first-level of child-strips, but don't draw any lines on the endpoints */
    LISTBASE_FOREACH (NlaStrip *, cs, &strip->strips) {
      /* draw start-line if not same as end of previous (and only if not the first strip)
       * - on upper half of strip
       */
      if ((cs->prev) && IS_EQF(cs->prev->end, cs->start) == 0) {
        immVertex2f(shdr_pos, cs->start, y);
        immVertex2f(shdr_pos, cs->start, ymaxc);
      }

      /* draw end-line if not the last strip
       * - on lower half of strip
       */
      if (cs->next) {
        immVertex2f(shdr_pos, cs->end, yminc);
        immVertex2f(shdr_pos, cs->end, y);
      }
    }

    immEnd();
  }

  immUnbindProgram();
}

/** Add the relevant text to the cache of text-strings to draw in pixel-space. */
static void nla_draw_strip_text(AnimData *adt,
                                NlaTrack *nlt,
                                NlaStrip *strip,
                                View2D *v2d,
                                float xminc,
                                float xmaxc,
                                float yminc,
                                float ymaxc)
{
  const bool solo = !((adt && (adt->flag & ADT_NLA_SOLO_TRACK)) &&
                      (nlt->flag & NLATRACK_SOLO) == 0);

  char str[256];
  size_t str_len;
  uchar col[4];

  /* just print the name and the range */
  if (strip->flag & NLASTRIP_FLAG_TEMP_META) {
    str_len = BLI_snprintf_rlen(str, sizeof(str), "Temp-Meta");
  }
  else {
    str_len = STRNCPY_RLEN(str, strip->name);
  }

  /* set text color - if colors (see above) are light, draw black text, otherwise draw white */
  if (strip->flag & (NLASTRIP_FLAG_ACTIVE | NLASTRIP_FLAG_TWEAKUSER)) {
    col[0] = col[1] = col[2] = 0;
  }
  else {
    col[0] = col[1] = col[2] = 255;
  }
  // Default strip to 100% opacity.
  col[3] = 255;

  /* Reduce text opacity if a track is soloed,
   * and if target track isn't the soloed track. */
  if (!solo) {
    col[3] = 128;
  }

  /* set bounding-box for text
   * - padding of 2 'units' on either side
   */
  /* TODO: make this centered? */
  rctf rect;
  rect.xmin = xminc;
  rect.ymin = yminc;
  rect.xmax = xmaxc;
  rect.ymax = ymaxc;

  /* add this string to the cache of texts to draw */
  UI_view2d_text_cache_add_rectf(v2d, &rect, str, str_len, col);
}

/**
 * Add frame extents to cache of text-strings to draw in pixel-space
 * for now, only used when transforming strips.
 */
static void nla_draw_strip_frames_text(
    NlaTrack * /*nlt*/, NlaStrip *strip, View2D *v2d, float /*yminc*/, float ymaxc)
{
  const float ytol = 1.0f; /* small offset to vertical positioning of text, for legibility */
  const uchar col[4] = {220, 220, 220, 255}; /* light gray */
  char numstr[32];
  size_t numstr_len;

  /* Always draw times above the strip, whereas sequencer drew below + above.
   * However, we should be fine having everything on top, since these tend to be
   * quite spaced out.
   * NOTE: 1 decimal point is a compromise between lack of precision (ints only, as per sequencer)
   * while also preserving some accuracy, since we do use floats. */

  /* start frame */
  numstr_len = SNPRINTF_RLEN(numstr, "%.1f", strip->start);
  UI_view2d_text_cache_add(v2d, strip->start - 1.0f, ymaxc + ytol, numstr, numstr_len, col);

  /* end frame */
  numstr_len = SNPRINTF_RLEN(numstr, "%.1f", strip->end);
  UI_view2d_text_cache_add(v2d, strip->end, ymaxc + ytol, numstr, numstr_len, col);
}

/* ---------------------- */

/**
 * Gets the first and last visible NLA strips on a track.
 * Note that this also includes tracks that might only be
 * visible because of their extendmode.
 */
static ListBase get_visible_nla_strips(NlaTrack *nlt, View2D *v2d)
{
  if (BLI_listbase_is_empty(&nlt->strips)) {
    ListBase empty = {nullptr, nullptr};
    return empty;
  }

  NlaStrip *first = nullptr;
  NlaStrip *last = nullptr;

  /* Find the first strip that is within the bounds of the view. */
  LISTBASE_FOREACH (NlaStrip *, strip, &nlt->strips) {
    if (BKE_nlastrip_within_bounds(strip, v2d->cur.xmin, v2d->cur.xmax)) {
      first = last = strip;
      break;
    }
  }

  const bool has_strips_within_bounds = first != nullptr;

  if (has_strips_within_bounds) {
    /* Find the last visible strip. */
    for (NlaStrip *strip = first->next; strip; strip = strip->next) {
      if (!BKE_nlastrip_within_bounds(strip, v2d->cur.xmin, v2d->cur.xmax)) {
        break;
      }
      last = strip;
    }
    /* Check if the first strip is adjacent to a strip outside the view to the left
     * that has an extendmode region that should be drawn.
     * If so, adjust the first strip to include drawing that strip as well.
     */
    NlaStrip *prev = first->prev;
    if (prev && prev->extendmode != NLASTRIP_EXTEND_NOTHING) {
      first = prev;
    }
  }
  else {
    /* No immediately visible strips.
     * Figure out where our view is relative to the strips, then determine
     * if the view is adjacent to a strip that should have its extendmode
     * rendered.
     */
    NlaStrip *first_strip = static_cast<NlaStrip *>(nlt->strips.first);
    NlaStrip *last_strip = static_cast<NlaStrip *>(nlt->strips.last);
    if (first_strip && v2d->cur.xmax < first_strip->start &&
        first_strip->extendmode == NLASTRIP_EXTEND_HOLD)
    {
      /* The view is to the left of all strips and the first strip has an
       * extendmode that should be drawn.
       */
      first = last = first_strip;
    }
    else if (last_strip && v2d->cur.xmin > last_strip->end &&
             last_strip->extendmode != NLASTRIP_EXTEND_NOTHING)
    {
      /* The view is to the right of all strips and the last strip has an
       * extendmode that should be drawn.
       */
      first = last = last_strip;
    }
    else {
      /* The view is in the middle of two strips. */
      LISTBASE_FOREACH (NlaStrip *, strip, &nlt->strips) {
        /* Find the strip to the left by finding the strip to the right and getting its prev. */
        if (v2d->cur.xmax < strip->start) {
          /* If the strip to the left has an extendmode, set that as the only visible strip. */
          if (strip->prev && strip->prev->extendmode != NLASTRIP_EXTEND_NOTHING) {
            first = last = strip->prev;
          }
          break;
        }
      }
    }
  }

  ListBase visible_strips = {first, last};
  return visible_strips;
}

void draw_nla_main_data(bAnimContext *ac, SpaceNla *snla, ARegion *region)
{
  View2D *v2d = &region->v2d;
  const float pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);
  const float text_margin_x = (8 * UI_SCALE_FAC) * pixelx;

  /* build list of tracks to draw */
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                              ANIMFILTER_LIST_CHANNELS | ANIMFILTER_FCURVESONLY);
  size_t items = ANIM_animdata_filter(
      ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* Update max-extent of tracks here (taking into account scrollers):
   * - this is done to allow the track list to be scrollable, but must be done here
   *   to avoid regenerating the list again and/or also because tracks list is drawn first
   * - offset of NLATRACK_HEIGHT*2 is added to the height of the tracks, as first is for
   *   start of list offset, and the second is as a correction for the scrollers.
   */
  int height = NLATRACK_TOT_HEIGHT(ac, items);
  v2d->tot.ymin = -height;

  /* Loop through tracks, and set up drawing depending on their type. */
  float ymax = NLATRACK_FIRST_TOP(ac);

  for (bAnimListElem *ale = static_cast<bAnimListElem *>(anim_data.first); ale;
       ale = ale->next, ymax -= NLATRACK_STEP(snla))
  {
    float ymin = ymax - NLATRACK_HEIGHT(snla);
    float ycenter = (ymax + ymin + 2 * NLATRACK_SKIP - 1) / 2.0f;

    /* check if visible */
    if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax))
    {
      /* data to draw depends on the type of track */
      switch (ale->type) {
        case ANIMTYPE_NLATRACK: {
          AnimData *adt = ale->adt;
          NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);
          ListBase visible_nla_strips = get_visible_nla_strips(nlt, v2d);

          /* Draw each visible strip in the track. */
          LISTBASE_FOREACH (NlaStrip *, strip, &visible_nla_strips) {
            const float xminc = strip->start + text_margin_x;
            const float xmaxc = strip->end - text_margin_x;

            /* draw the visualization of the strip */
            nla_draw_strip(snla, adt, nlt, strip, v2d, ymin, ymax);

            /* add the text for this strip to the cache */
            if (xminc < xmaxc) {
              nla_draw_strip_text(adt, nlt, strip, v2d, xminc, xmaxc, ymin, ymax);
            }

            /* if transforming strips (only real reason for temp-metas currently),
             * add to the cache the frame numbers of the strip's extents
             */
            if (strip->flag & NLASTRIP_FLAG_TEMP_META) {
              nla_draw_strip_frames_text(nlt, strip, v2d, ymin, ymax);
            }
          }
          break;
        }
        case ANIMTYPE_NLAACTION: {
          AnimData *adt = ale->adt;

          /* Draw the manually set intended playback frame range highlight. */
          if (ale->data) {
            ANIM_draw_action_framerange(adt, static_cast<bAction *>(ale->data), v2d, ymin, ymax);
          }

          uint pos = GPU_vertformat_attr_add(
              immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
          immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

          /* just draw a semi-shaded rect spanning the width of the viewable area, based on if
           * there's data and the action's extrapolation mode. Draw a second darker rect within
           * which we draw keyframe indicator dots if there's data.
           */
          GPU_blend(GPU_BLEND_ALPHA);

          /* get colors for drawing */
          float color[4];
          nla_action_get_color(adt, static_cast<bAction *>(ale->data), color);
          immUniformColor4fv(color);

          /* draw slightly shifted up for greater separation from standard tracks,
           * but also slightly shorter for some more contrast when viewing the strips
           */
          switch (adt->act_extendmode) {
            case NLASTRIP_EXTEND_HOLD: {
              immRectf(pos,
                       v2d->cur.xmin,
                       ymin + NLATRACK_SKIP,
                       v2d->cur.xmax,
                       ymax + NLATRACK_SKIP - 1);
              break;
            }
            case NLASTRIP_EXTEND_HOLD_FORWARD: {
              float r_start;
              float r_end;
              BKE_action_frame_range_get(static_cast<bAction *>(ale->data), &r_start, &r_end);
              BKE_nla_clip_length_ensure_nonzero(&r_start, &r_end);

              immRectf(pos, r_end, ymin + NLATRACK_SKIP, v2d->cur.xmax, ymax - NLATRACK_SKIP);
              break;
            }
            case NLASTRIP_EXTEND_NOTHING:
              break;
          }

          immUnbindProgram();

          /* draw keyframes in the action */
          nla_action_draw_keyframes(v2d,
                                    adt,
                                    static_cast<bAction *>(ale->data),
                                    ycenter,
                                    ymin + NLATRACK_SKIP,
                                    ymax + NLATRACK_SKIP - 1);

          GPU_blend(GPU_BLEND_NONE);
          break;
        }
      }
    }
  }

  /* Free temporary tracks. */
  ANIM_animdata_freelist(&anim_data);
}

/* *********************************************** */
/* Track List */

void draw_nla_track_list(const bContext *C,
                         bAnimContext *ac,
                         ARegion *region,
                         const ListBase /* bAnimListElem */ &anim_data)
{

  SpaceNla *snla = reinterpret_cast<SpaceNla *>(ac->sl);
  View2D *v2d = &region->v2d;

  /* need to do a view-sync here, so that the keys area doesn't jump around
   * (it must copy this) */
  UI_view2d_sync(nullptr, ac->area, v2d, V2D_LOCK_COPY);

  /* draw tracks */
  { /* first pass: just the standard GL-drawing for backdrop + text */
    size_t track_index = 0;
    float ymax = NLATRACK_FIRST_TOP(ac);

    for (bAnimListElem *ale = static_cast<bAnimListElem *>(anim_data.first); ale;
         ale = ale->next, ymax -= NLATRACK_STEP(snla), track_index++)
    {
      float ymin = ymax - NLATRACK_HEIGHT(snla);

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax))
      {
        /* draw all tracks using standard channel-drawing API */
        ANIM_channel_draw(ac, ale, ymin, ymax, track_index);
      }
    }
  }
  { /* second pass: UI widgets */
    uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
    size_t track_index = 0;
    float ymax = NLATRACK_FIRST_TOP(ac);

    /* set blending again, as may not be set in previous step */
    GPU_blend(GPU_BLEND_ALPHA);

    /* Loop through tracks, and set up drawing depending on their type. */
    for (bAnimListElem *ale = static_cast<bAnimListElem *>(anim_data.first); ale;
         ale = ale->next, ymax -= NLATRACK_STEP(snla), track_index++)
    {
      float ymin = ymax - NLATRACK_HEIGHT(snla);

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax))
      {
        /* draw all tracks using standard channel-drawing API */
        rctf track_rect;
        BLI_rctf_init(&track_rect, 0, v2d->cur.xmax, ymin, ymax);
        ANIM_channel_draw_widgets(C, ac, ale, block, &track_rect, track_index);
      }
    }

    UI_block_end(C, block);
    UI_block_draw(C, block);

    GPU_blend(GPU_BLEND_NONE);
  }
}

/* *********************************************** */
