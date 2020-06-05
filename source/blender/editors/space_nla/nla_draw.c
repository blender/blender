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
 * \ingroup spnla
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_dlrbTree.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_state.h"

#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h" /* own include */
#include "nla_private.h"

/* *********************************************** */
/* Strips */

/* Action-Line ---------------------- */

/* get colors for drawing Action-Line
 * NOTE: color returned includes fine-tuned alpha!
 */
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
  /* get a list of the keyframes with NLA-scaling applied */
  DLRBT_Tree keys;
  BLI_dlrbTree_init(&keys);
  action_to_keylist(adt, act, &keys, 0);

  if (ELEM(NULL, act, keys.first)) {
    return;
  }

  /* draw a darkened region behind the strips
   * - get and reset the background color, this time without the alpha to stand out better
   *   (amplified alpha is used instead)
   */
  float color[4];
  nla_action_get_color(adt, act, color);
  color[3] *= 2.5f;

  GPUVertFormat *format = immVertexFormat();
  uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4fv(color);

  /* - draw a rect from the first to the last frame (no extra overlaps for now)
   *   that is slightly stumpier than the track background (hardcoded 2-units here)
   */
  float f1 = ((ActKeyColumn *)keys.first)->cfra;
  float f2 = ((ActKeyColumn *)keys.last)->cfra;

  immRectf(pos_id, f1, ymin + 2, f2, ymax - 2);
  immUnbindProgram();

  /* count keys before drawing */
  /* Note: It's safe to cast DLRBT_Tree, as it's designed to degrade down to a ListBase */
  uint key_len = BLI_listbase_count((ListBase *)&keys);

  if (key_len > 0) {
    format = immVertexFormat();
    pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
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

    /* - disregard the selection status of keyframes so they draw a certain way
     * - size is 6.0f which is smaller than the editable keyframes, so that there is a distinction
     */
    LISTBASE_FOREACH (ActKeyColumn *, ak, &keys) {
      draw_keyframe_shape(ak->cfra,
                          y,
                          6.0f,
                          false,
                          ak->key_type,
                          KEYFRAME_SHAPE_FRAME,
                          1.0f,
                          pos_id,
                          size_id,
                          color_id,
                          outline_color_id,
                          flags_id,
                          KEYFRAME_HANDLE_NONE,
                          KEYFRAME_EXTREME_NONE);
    }

    immEnd();
    GPU_program_point_size(false);
    immUnbindProgram();
  }

  /* free icons */
  BLI_dlrbTree_free(&keys);
}

/* Strip Markers ------------------------ */

/* Markers inside an action strip */
static void nla_actionclip_draw_markers(
    NlaStrip *strip, float yminc, float ymaxc, int shade, const bool dashed)
{
  const bAction *act = strip->act;

  if (ELEM(NULL, act, act->markers.first)) {
    return;
  }

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  if (dashed) {
    immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

    immUniform1i("colors_len", 0); /* "simple" mode */
    immUniform1f("dash_width", 6.0f);
    immUniform1f("dash_factor", 0.5f);
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
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

/* get colors for drawing NLA-Strips */
static void nla_strip_get_color_inside(AnimData *adt, NlaStrip *strip, float color[3])
{
  if (strip->type == NLASTRIP_TYPE_TRANSITION) {
    /* Transition Clip */
    if (strip->flag & NLASTRIP_FLAG_SELECT) {
      /* selected - use a bright blue color */
      UI_GetThemeColor3fv(TH_NLA_TRANSITION_SEL, color);
    }
    else {
      /* normal, unselected strip - use (hardly noticeable) blue tinge */
      UI_GetThemeColor3fv(TH_NLA_TRANSITION, color);
    }
  }
  else if (strip->type == NLASTRIP_TYPE_META) {
    /* Meta Clip */
    // TODO: should temporary metas get different colors too?
    if (strip->flag & NLASTRIP_FLAG_SELECT) {
      /* selected - use a bold purple color */
      UI_GetThemeColor3fv(TH_NLA_META_SEL, color);
    }
    else {
      /* normal, unselected strip - use (hardly noticeable) dark purple tinge */
      UI_GetThemeColor3fv(TH_NLA_META, color);
    }
  }
  else if (strip->type == NLASTRIP_TYPE_SOUND) {
    /* Sound Clip */
    if (strip->flag & NLASTRIP_FLAG_SELECT) {
      /* selected - use a bright teal color */
      UI_GetThemeColor3fv(TH_NLA_SOUND_SEL, color);
    }
    else {
      /* normal, unselected strip - use (hardly noticeable) teal tinge */
      UI_GetThemeColor3fv(TH_NLA_SOUND, color);
    }
  }
  else {
    /* Action Clip (default/normal type of strip) */
    if (adt && (adt->flag & ADT_NLA_EDIT_ON) && (adt->actstrip == strip)) {
      /* active strip should be drawn green when it is acting as the tweaking strip.
       * however, this case should be skipped for when not in EditMode...
       */
      UI_GetThemeColor3fv(TH_NLA_TWEAK, color);
    }
    else if (strip->flag & NLASTRIP_FLAG_TWEAKUSER) {
      /* alert user that this strip is also used by the tweaking track (this is set when going into
       * 'editmode' for that strip), since the edits made here may not be what the user anticipated
       */
      UI_GetThemeColor3fv(TH_NLA_TWEAK_DUPLI, color);
    }
    else if (strip->flag & NLASTRIP_FLAG_SELECT) {
      /* selected strip - use theme color for selected */
      UI_GetThemeColor3fv(TH_STRIP_SELECT, color);
    }
    else {
      /* normal, unselected strip - use standard strip theme color */
      UI_GetThemeColor3fv(TH_STRIP, color);
    }
  }
}

/* helper call for drawing influence/time control curves for a given NLA-strip */
static void nla_draw_strip_curves(NlaStrip *strip, float yminc, float ymaxc, uint pos)
{
  const float yheight = ymaxc - yminc;

  immUniformColor3f(0.7f, 0.7f, 0.7f);

  /* draw with AA'd line */
  GPU_line_smooth(true);
  GPU_blend(true);

  /* influence -------------------------- */
  if (strip->flag & NLASTRIP_FLAG_USR_INFLUENCE) {
    FCurve *fcu = BKE_fcurve_find(&strip->fcurves, "influence", 0);
    float cfra;

    /* plot the curve (over the strip's main region) */
    if (fcu) {
      immBegin(GPU_PRIM_LINE_STRIP, abs((int)(strip->end - strip->start) + 1));

      /* sample at 1 frame intervals, and draw
       * - min y-val is yminc, max is y-maxc, so clamp in those regions
       */
      for (cfra = strip->start; cfra <= strip->end; cfra += 1.0f) {
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
  GPU_blend(false);
}

/* helper call to setup dashed-lines for strip outlines */
static uint nla_draw_use_dashed_outlines(float color[4], bool muted)
{
  /* Note that we use dashed shader here, and make it draw solid lines if not muted... */
  uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniform1i("colors_len", 0); /* Simple dashes. */
  immUniformColor3fv(color);

  /* line style: dotted for muted */
  if (muted) {
    /* dotted - and slightly thicker for readability of the dashes */
    immUniform1f("dash_width", 5.0f);
    immUniform1f("dash_factor", 0.4f);
    GPU_line_width(1.5f);
  }
  else {
    /* solid line */
    immUniform1f("dash_factor", 2.0f);
    GPU_line_width(1.0f);
  }

  return shdr_pos;
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
  const bool non_solo = ((adt && (adt->flag & ADT_NLA_SOLO_TRACK)) &&
                         (nlt->flag & NLATRACK_SOLO) == 0);
  const bool muted = ((nlt->flag & NLATRACK_MUTED) || (strip->flag & NLASTRIP_FLAG_MUTED));
  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  uint shdr_pos;

  /* get color of strip */
  nla_strip_get_color_inside(adt, strip, color);

  shdr_pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* draw extrapolation info first (as backdrop)
   * - but this should only be drawn if track has some contribution
   */
  if ((strip->extendmode != NLASTRIP_EXTEND_NOTHING) && (non_solo == 0)) {
    /* enable transparency... */
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    GPU_blend(true);

    switch (strip->extendmode) {
      /* since this does both sides,
       * only do the 'before' side, and leave the rest to the next case */
      case NLASTRIP_EXTEND_HOLD:
        /* only need to draw here if there's no strip before since
         * it only applies in such a situation
         */
        if (strip->prev == NULL) {
          /* set the drawing color to the color of the strip, but with very faint alpha */
          immUniformColor3fvAlpha(color, 0.15f);

          /* draw the rect to the edge of the screen */
          immRectf(shdr_pos, v2d->cur.xmin, yminc, strip->start, ymaxc);
        }
        ATTR_FALLTHROUGH;

      /* this only draws after the strip */
      case NLASTRIP_EXTEND_HOLD_FORWARD:
        /* only need to try and draw if the next strip doesn't occur immediately after */
        if ((strip->next == NULL) || (IS_EQF(strip->next->start, strip->end) == 0)) {
          /* set the drawing color to the color of the strip, but this time less faint */
          immUniformColor3fvAlpha(color, 0.3f);

          /* draw the rect to the next strip or the edge of the screen */
          float x2 = strip->next ? strip->next->start : v2d->cur.xmax;
          immRectf(shdr_pos, strip->end, yminc, x2, ymaxc);
        }
        break;
    }

    GPU_blend(false);
  }

  /* draw 'inside' of strip itself */
  if (non_solo == 0) {
    immUnbindProgram();

    /* strip is in normal track */
    UI_draw_roundbox_corner_set(UI_CNR_ALL); /* all corners rounded */
    UI_draw_roundbox_shade_x(true, strip->start, yminc, strip->end, ymaxc, 0.0, 0.5, 0.1, color);

    /* restore current vertex format & program (roundbox trashes it) */
    shdr_pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  }
  else {
    /* strip is in disabled track - make less visible */
    immUniformColor3fvAlpha(color, 0.1f);

    GPU_blend(true);
    immRectf(shdr_pos, strip->start, yminc, strip->end, ymaxc);
    GPU_blend(false);
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
  if (strip->flag & NLASTRIP_FLAG_ACTIVE) {
    /* strip should appear 'sunken', so draw a light border around it */
    color[0] = 0.9f; /* FIXME: hardcoded temp-hack colors */
    color[1] = 1.0f;
    color[2] = 0.9f;
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
    UI_draw_roundbox_shade_x(false, strip->start, yminc, strip->end, ymaxc, 0.0, 0.0, 0.1, color);

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

/* add the relevant text to the cache of text-strings to draw in pixelspace */
static void nla_draw_strip_text(AnimData *adt,
                                NlaTrack *nlt,
                                NlaStrip *strip,
                                int index,
                                View2D *v2d,
                                float xminc,
                                float xmaxc,
                                float yminc,
                                float ymaxc)
{
  const bool non_solo = ((adt && (adt->flag & ADT_NLA_SOLO_TRACK)) &&
                         (nlt->flag & NLATRACK_SOLO) == 0);
  char str[256];
  size_t str_len;
  uchar col[4];

  /* just print the name and the range */
  if (strip->flag & NLASTRIP_FLAG_TEMP_META) {
    str_len = BLI_snprintf_rlen(str, sizeof(str), "%d) Temp-Meta", index);
  }
  else {
    str_len = BLI_strncpy_rlen(str, strip->name, sizeof(str));
  }

  /* set text color - if colors (see above) are light, draw black text, otherwise draw white */
  if (strip->flag & (NLASTRIP_FLAG_ACTIVE | NLASTRIP_FLAG_SELECT | NLASTRIP_FLAG_TWEAKUSER)) {
    col[0] = col[1] = col[2] = 0;
  }
  else {
    col[0] = col[1] = col[2] = 255;
  }

  /* text opacity depends on whether if there's a solo'd track, this isn't it */
  if (non_solo == 0) {
    col[3] = 255;
  }
  else {
    col[3] = 128;
  }

  /* set bounding-box for text
   * - padding of 2 'units' on either side
   */
  /* TODO: make this centered? */
  rctf rect = {
      .xmin = xminc,
      .ymin = yminc,
      .xmax = xmaxc,
      .ymax = ymaxc,
  };

  /* add this string to the cache of texts to draw */
  UI_view2d_text_cache_add_rectf(v2d, &rect, str, str_len, col);
}

/* add frame extents to cache of text-strings to draw in pixelspace
 * for now, only used when transforming strips
 */
static void nla_draw_strip_frames_text(
    NlaTrack *UNUSED(nlt), NlaStrip *strip, View2D *v2d, float UNUSED(yminc), float ymaxc)
{
  const float ytol = 1.0f; /* small offset to vertical positioning of text, for legibility */
  const uchar col[4] = {220, 220, 220, 255}; /* light gray */
  char numstr[32];
  size_t numstr_len;

  /* Always draw times above the strip, whereas sequencer drew below + above.
   * However, we should be fine having everything on top, since these tend to be
   * quite spaced out.
   * - 1 dp is compromise between lack of precision (ints only, as per sequencer)
   *   while also preserving some accuracy, since we do use floats
   */
  /* start frame */
  numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%.1f", strip->start);
  UI_view2d_text_cache_add(v2d, strip->start - 1.0f, ymaxc + ytol, numstr, numstr_len, col);

  /* end frame */
  numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%.1f", strip->end);
  UI_view2d_text_cache_add(v2d, strip->end, ymaxc + ytol, numstr, numstr_len, col);
}

/* ---------------------- */

void draw_nla_main_data(bAnimContext *ac, SpaceNla *snla, ARegion *region)
{
  View2D *v2d = &region->v2d;
  const float pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);
  const float text_margin_x = (8 * UI_DPI_FAC) * pixelx;

  /* build list of channels to draw */
  ListBase anim_data = {NULL, NULL};
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  size_t items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* Update max-extent of channels here (taking into account scrollers):
   * - this is done to allow the channel list to be scrollable, but must be done here
   *   to avoid regenerating the list again and/or also because channels list is drawn first
   * - offset of NLACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for
   *   start of list offset, and the second is as a correction for the scrollers.
   */
  int height = NLACHANNEL_TOT_HEIGHT(ac, items);
  v2d->tot.ymin = -height;

  /* loop through channels, and set up drawing depending on their type  */
  float ymax = NLACHANNEL_FIRST_TOP(ac);

  for (bAnimListElem *ale = anim_data.first; ale; ale = ale->next, ymax -= NLACHANNEL_STEP(snla)) {
    float ymin = ymax - NLACHANNEL_HEIGHT(snla);
    float ycenter = (ymax + ymin) / 2.0f;

    /* check if visible */
    if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
      /* data to draw depends on the type of channel */
      switch (ale->type) {
        case ANIMTYPE_NLATRACK: {
          AnimData *adt = ale->adt;
          NlaTrack *nlt = (NlaTrack *)ale->data;
          NlaStrip *strip;
          int index;

          /* draw each strip in the track (if visible) */
          for (strip = nlt->strips.first, index = 1; strip; strip = strip->next, index++) {
            if (BKE_nlastrip_within_bounds(strip, v2d->cur.xmin, v2d->cur.xmax)) {
              const float xminc = strip->start + text_margin_x;
              const float xmaxc = strip->end + text_margin_x;

              /* draw the visualization of the strip */
              nla_draw_strip(snla, adt, nlt, strip, v2d, ymin, ymax);

              /* add the text for this strip to the cache */
              if (xminc < xmaxc) {
                nla_draw_strip_text(adt, nlt, strip, index, v2d, xminc, xmaxc, ymin, ymax);
              }

              /* if transforming strips (only real reason for temp-metas currently),
               * add to the cache the frame numbers of the strip's extents
               */
              if (strip->flag & NLASTRIP_FLAG_TEMP_META) {
                nla_draw_strip_frames_text(nlt, strip, v2d, ymin, ymax);
              }
            }
          }
          break;
        }
        case ANIMTYPE_NLAACTION: {
          AnimData *adt = ale->adt;

          uint pos = GPU_vertformat_attr_add(
              immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
          immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

          /* just draw a semi-shaded rect spanning the width of the viewable area if there's data,
           * and a second darker rect within which we draw keyframe indicator dots if there's data
           */
          GPU_blend_set_func_separate(
              GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
          GPU_blend(true);

          /* get colors for drawing */
          float color[4];
          nla_action_get_color(adt, ale->data, color);
          immUniformColor4fv(color);

          /* draw slightly shifted up for greater separation from standard channels,
           * but also slightly shorter for some more contrast when viewing the strips
           */
          immRectf(
              pos, v2d->cur.xmin, ymin + NLACHANNEL_SKIP, v2d->cur.xmax, ymax - NLACHANNEL_SKIP);

          /* draw 'embossed' lines above and below the strip for effect */
          /* white base-lines */
          GPU_line_width(2.0f);
          immUniformColor4f(1.0f, 1.0f, 1.0f, 0.3f);
          immBegin(GPU_PRIM_LINES, 4);
          immVertex2f(pos, v2d->cur.xmin, ymin + NLACHANNEL_SKIP);
          immVertex2f(pos, v2d->cur.xmax, ymin + NLACHANNEL_SKIP);
          immVertex2f(pos, v2d->cur.xmin, ymax - NLACHANNEL_SKIP);
          immVertex2f(pos, v2d->cur.xmax, ymax - NLACHANNEL_SKIP);
          immEnd();

          /* black top-lines */
          GPU_line_width(1.0f);
          immUniformColor3f(0.0f, 0.0f, 0.0f);
          immBegin(GPU_PRIM_LINES, 4);
          immVertex2f(pos, v2d->cur.xmin, ymin + NLACHANNEL_SKIP);
          immVertex2f(pos, v2d->cur.xmax, ymin + NLACHANNEL_SKIP);
          immVertex2f(pos, v2d->cur.xmin, ymax - NLACHANNEL_SKIP);
          immVertex2f(pos, v2d->cur.xmax, ymax - NLACHANNEL_SKIP);
          immEnd();

          /* TODO: these lines but better --^ */

          immUnbindProgram();

          /* draw keyframes in the action */
          nla_action_draw_keyframes(
              v2d, adt, ale->data, ycenter, ymin + NLACHANNEL_SKIP, ymax - NLACHANNEL_SKIP);

          GPU_blend(false);
          break;
        }
      }
    }
  }

  /* free tempolary channels */
  ANIM_animdata_freelist(&anim_data);
}

/* *********************************************** */
/* Channel List */

void draw_nla_channel_list(const bContext *C, bAnimContext *ac, ARegion *region)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  SpaceNla *snla = (SpaceNla *)ac->sl;
  View2D *v2d = &region->v2d;
  size_t items;

  /* build list of channels to draw */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* Update max-extent of channels here (taking into account scrollers):
   * - this is done to allow the channel list to be scrollable, but must be done here
   *   to avoid regenerating the list again and/or also because channels list is drawn first
   * - offset of NLACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for
   *  start of list offset, and the second is as a correction for the scrollers.
   */
  int height = NLACHANNEL_TOT_HEIGHT(ac, items);
  v2d->tot.ymin = -height;

  /* need to do a view-sync here, so that the keys area doesn't jump around
   * (it must copy this) */
  UI_view2d_sync(NULL, ac->area, v2d, V2D_LOCK_COPY);

  /* draw channels */
  { /* first pass: just the standard GL-drawing for backdrop + text */
    size_t channel_index = 0;
    float ymax = NLACHANNEL_FIRST_TOP(ac);

    for (ale = anim_data.first; ale;
         ale = ale->next, ymax -= NLACHANNEL_STEP(snla), channel_index++) {
      float ymin = ymax - NLACHANNEL_HEIGHT(snla);

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
        /* draw all channels using standard channel-drawing API */
        ANIM_channel_draw(ac, ale, ymin, ymax, channel_index);
      }
    }
  }
  { /* second pass: UI widgets */
    uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
    size_t channel_index = 0;
    float ymax = NLACHANNEL_FIRST_TOP(ac);

    /* set blending again, as may not be set in previous step */
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    GPU_blend(true);

    /* loop through channels, and set up drawing depending on their type  */
    for (ale = anim_data.first; ale;
         ale = ale->next, ymax -= NLACHANNEL_STEP(snla), channel_index++) {
      float ymin = ymax - NLACHANNEL_HEIGHT(snla);

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
        /* draw all channels using standard channel-drawing API */
        rctf channel_rect;
        BLI_rctf_init(&channel_rect, 0, v2d->cur.xmax, ymin, ymax);
        ANIM_channel_draw_widgets(C, ac, ale, block, &channel_rect, channel_index);
      }
    }

    UI_block_end(C, block);
    UI_block_draw(C, block);

    GPU_blend(false);
  }

  /* free temporary channels */
  ANIM_animdata_freelist(&anim_data);
}

/* *********************************************** */
