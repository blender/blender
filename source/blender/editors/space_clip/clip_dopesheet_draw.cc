/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_movieclip.h"

#include "ED_anim_api.hh"
#include "ED_clip.hh"
#include "ED_screen.hh"

#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLF_api.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "clip_intern.hh" /* own include */

static void track_channel_color(MovieTrackingTrack *track, bool default_color, float color[3])
{
  if (track->flag & TRACK_CUSTOMCOLOR) {
    float bg[3];
    UI_GetThemeColor3fv(TH_HEADER, bg);

    interp_v3_v3v3(color, track->color, bg, 0.5);
  }
  else {
    if (default_color) {
      UI_GetThemeColor4fv(TH_CHANNEL_SELECT, color);
    }
    else {
      UI_GetThemeColor3fv(TH_CHANNEL, color);
    }
  }
}

static void draw_keyframe_shape(
    float x, float y, bool sel, float alpha, uint pos_id, uint color_id)
{
  float color[4];
  if (sel) {
    UI_GetThemeColor4fv(TH_KEYTYPE_KEYFRAME_SELECT, color);
  }
  else {
    UI_GetThemeColor4fv(TH_KEYTYPE_KEYFRAME, color);
  }
  color[3] = alpha;

  immAttr4fv(color_id, color);
  immVertex2f(pos_id, x, y);
}

static void clip_draw_dopesheet_background(ARegion *region, MovieClip *clip, uint pos_id)
{
  View2D *v2d = &region->v2d;
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;

  LISTBASE_FOREACH (
      MovieTrackingDopesheetCoverageSegment *, coverage_segment, &dopesheet->coverage_segments)
  {
    if (coverage_segment->coverage < TRACKING_COVERAGE_OK) {
      int start_frame = BKE_movieclip_remap_clip_to_scene_frame(clip,
                                                                coverage_segment->start_frame);
      int end_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, coverage_segment->end_frame);

      if (coverage_segment->coverage == TRACKING_COVERAGE_BAD) {
        immUniformColor4f(1.0f, 0.0f, 0.0f, 0.07f);
      }
      else {
        immUniformColor4f(1.0f, 1.0f, 0.0f, 0.07f);
      }

      immRectf(pos_id, start_frame, v2d->cur.ymin, end_frame, v2d->cur.ymax);
    }
  }
}

void clip_draw_dopesheet_main(SpaceClip *sc, ARegion *region, Scene *scene)
{
  MovieClip *clip = ED_space_clip_get_clip(sc);
  View2D *v2d = &region->v2d;

  /* Frame and preview range. */
  UI_view2d_view_ortho(v2d);
  ANIM_draw_framerange(scene, v2d);
  ANIM_draw_previewrange(scene, v2d, 0);

  if (clip) {
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
    float strip[4], selected_strip[4];
    float height = (dopesheet->tot_channel * CHANNEL_STEP) + CHANNEL_HEIGHT;

    uint keyframe_len = 0;

    GPUVertFormat *format = immVertexFormat();
    uint pos_id = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    /* don't use totrect set, as the width stays the same
     * (NOTE: this is ok here, the configuration is pretty straightforward)
     */
    v2d->tot.ymin = (-height);

    float y = (CHANNEL_FIRST);

    /* setup colors for regular and selected strips */
    UI_GetThemeColor4fv(TH_LONGKEY, strip);
    UI_GetThemeColor4fv(TH_LONGKEY_SELECT, selected_strip);

    GPU_blend(GPU_BLEND_ALPHA);

    clip_draw_dopesheet_background(region, clip, pos_id);

    LISTBASE_FOREACH (MovieTrackingDopesheetChannel *, channel, &dopesheet->channels) {
      float yminc = (y - CHANNEL_HEIGHT_HALF);
      float ymaxc = (y + CHANNEL_HEIGHT_HALF);

      /* check if visible */
      if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
      {
        MovieTrackingTrack *track = channel->track;
        int i;
        bool sel = (track->flag & TRACK_DOPE_SEL) != 0;

        /* selection background */
        if (sel) {
          float color[4] = {0.0f, 0.0f, 0.0f, 0.3f};

          track_channel_color(track, true, color);
          immUniformColor4fv(color);

          immRectf(pos_id,
                   v2d->cur.xmin,
                   y - CHANNEL_HEIGHT_HALF,
                   v2d->cur.xmax + EXTRA_SCROLL_PAD,
                   y + CHANNEL_HEIGHT_HALF);
        }

        /* tracked segments */
        for (i = 0; i < channel->tot_segment; i++) {
          int start_frame = BKE_movieclip_remap_clip_to_scene_frame(clip,
                                                                    channel->segments[2 * i]);
          int end_frame = BKE_movieclip_remap_clip_to_scene_frame(clip,
                                                                  channel->segments[2 * i + 1]);

          immUniformColor4fv(sel ? selected_strip : strip);

          if (start_frame != end_frame) {
            immRectf(pos_id, start_frame, y - STRIP_HEIGHT_HALF, end_frame, y + STRIP_HEIGHT_HALF);
            keyframe_len += 2;
          }
          else {
            keyframe_len++;
          }
        }

        /* keyframes */
        i = 0;
        while (i < track->markersnr) {
          MovieTrackingMarker *marker = &track->markers[i];

          if ((marker->flag & (MARKER_DISABLED | MARKER_TRACKED)) == 0) {
            keyframe_len++;
          }

          i++;
        }
      }

      /* adjust y-position for next one */
      y -= CHANNEL_STEP;
    }

    immUnbindProgram();

    if (keyframe_len > 0) {
      /* draw keyframe markers */
      format = immVertexFormat();
      pos_id = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
      uint size_id = GPU_vertformat_attr_add(
          format, "size", blender::gpu::VertAttrType::SFLOAT_32);
      uint color_id = GPU_vertformat_attr_add(
          format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);
      uint outline_color_id = GPU_vertformat_attr_add(
          format, "outlineColor", blender::gpu::VertAttrType::UNORM_8_8_8_8);
      uint flags_id = GPU_vertformat_attr_add(
          format, "flags", blender::gpu::VertAttrType::UINT_32);

      GPU_program_point_size(true);
      immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
      immUniform1f("outline_scale", 1.0f);
      immUniform2f(
          "ViewportSize", BLI_rcti_size_x(&v2d->mask) + 1, BLI_rcti_size_y(&v2d->mask) + 1);
      immBegin(GPU_PRIM_POINTS, keyframe_len);

      /* all same size with black outline */
      immAttr1f(size_id, 2.0f * STRIP_HEIGHT_HALF);
      immAttr4ub(outline_color_id, 0, 0, 0, 255);
      immAttr1u(flags_id, 0);

      y = (CHANNEL_FIRST); /* start again at the top */
      LISTBASE_FOREACH (MovieTrackingDopesheetChannel *, channel, &dopesheet->channels) {
        float yminc = (y - CHANNEL_HEIGHT_HALF);
        float ymaxc = (y + CHANNEL_HEIGHT_HALF);

        /* check if visible */
        if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
            IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
        {
          MovieTrackingTrack *track = channel->track;
          int i;
          bool sel = (track->flag & TRACK_DOPE_SEL) != 0;
          float alpha = (track->flag & TRACK_LOCKED) ? 0.5f : 1.0f;

          /* tracked segments */
          for (i = 0; i < channel->tot_segment; i++) {
            int start_frame = BKE_movieclip_remap_clip_to_scene_frame(clip,
                                                                      channel->segments[2 * i]);
            int end_frame = BKE_movieclip_remap_clip_to_scene_frame(clip,
                                                                    channel->segments[2 * i + 1]);

            if (start_frame != end_frame) {
              draw_keyframe_shape(start_frame, y, sel, alpha, pos_id, color_id);
              draw_keyframe_shape(end_frame, y, sel, alpha, pos_id, color_id);
            }
            else {
              draw_keyframe_shape(start_frame, y, sel, alpha, pos_id, color_id);
            }
          }

          /* keyframes */
          i = 0;
          while (i < track->markersnr) {
            MovieTrackingMarker *marker = &track->markers[i];

            if ((marker->flag & (MARKER_DISABLED | MARKER_TRACKED)) == 0) {
              int framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);

              draw_keyframe_shape(framenr, y, sel, alpha, pos_id, color_id);
            }

            i++;
          }
        }

        /* adjust y-position for next one */
        y -= CHANNEL_STEP;
      }

      immEnd();
      GPU_program_point_size(false);
      immUnbindProgram();
    }

    GPU_blend(GPU_BLEND_NONE);
  }
}

void clip_draw_dopesheet_channels(const bContext *C, ARegion *region)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  View2D *v2d = &region->v2d;
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const uiStyle *style = UI_style_get();
  int fontid = style->widget.uifont_id;

  if (!clip) {
    return;
  }

  MovieTracking *tracking = &clip->tracking;
  MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
  int height = (dopesheet->tot_channel * CHANNEL_STEP) + CHANNEL_HEIGHT;

  if (height > BLI_rcti_size_y(&v2d->mask)) {
    /* don't use totrect set, as the width stays the same
     * (NOTE: this is ok here, the configuration is pretty straightforward)
     */
    v2d->tot.ymin = float(-height);
  }

  /* need to do a view-sync here, so that the keys area doesn't jump around
   * (it must copy this) */
  UI_view2d_sync(nullptr, area, v2d, V2D_LOCK_COPY);

  /* loop through channels, and set up drawing depending on their type
   * first pass: just the standard GL-drawing for backdrop + text
   */
  float y = (CHANNEL_FIRST);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  LISTBASE_FOREACH (MovieTrackingDopesheetChannel *, channel, &dopesheet->channels) {
    float yminc = (y - CHANNEL_HEIGHT_HALF);
    float ymaxc = (y + CHANNEL_HEIGHT_HALF);

    /* check if visible */
    if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
    {
      MovieTrackingTrack *track = channel->track;
      float color[3];
      track_channel_color(track, false, color);
      immUniformColor3fv(color);

      immRectf(pos,
               v2d->cur.xmin,
               y - CHANNEL_HEIGHT_HALF,
               v2d->cur.xmax + EXTRA_SCROLL_PAD,
               y + CHANNEL_HEIGHT_HALF);
    }

    /* adjust y-position for next one */
    y -= CHANNEL_STEP;
  }
  immUnbindProgram();

  /* second pass: text */
  y = (CHANNEL_FIRST);

  BLF_size(fontid, 11.0f * UI_SCALE_FAC);

  LISTBASE_FOREACH (MovieTrackingDopesheetChannel *, channel, &dopesheet->channels) {
    float yminc = (y - CHANNEL_HEIGHT_HALF);
    float ymaxc = (y + CHANNEL_HEIGHT_HALF);

    /* check if visible */
    if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
    {
      MovieTrackingTrack *track = channel->track;
      bool sel = (track->flag & TRACK_DOPE_SEL) != 0;

      UI_FontThemeColor(fontid, sel ? TH_TEXT_HI : TH_TEXT);

      float font_height = BLF_height(fontid, channel->name, sizeof(channel->name));
      BLF_position(fontid, v2d->cur.xmin + CHANNEL_PAD, y - font_height / 2.0f, 0.0f);
      BLF_draw(fontid, channel->name, strlen(channel->name));
    }

    /* adjust y-position for next one */
    y -= CHANNEL_STEP;
  }

  /* third pass: widgets */
  uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
  y = (CHANNEL_FIRST);

  /* get RNA properties (once) */
  PropertyRNA *chan_prop_lock = RNA_struct_type_find_property(&RNA_MovieTrackingTrack, "lock");
  BLI_assert(chan_prop_lock);

  GPU_blend(GPU_BLEND_ALPHA);
  LISTBASE_FOREACH (MovieTrackingDopesheetChannel *, channel, &dopesheet->channels) {
    float yminc = (y - CHANNEL_HEIGHT_HALF);
    float ymaxc = (y + CHANNEL_HEIGHT_HALF);

    /* check if visible */
    if (IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax))
    {
      MovieTrackingTrack *track = channel->track;
      const int icon = (track->flag & TRACK_LOCKED) ? ICON_LOCKED : ICON_UNLOCKED;
      PointerRNA ptr = RNA_pointer_create_discrete(&clip->id, &RNA_MovieTrackingTrack, track);

      UI_block_emboss_set(block, blender::ui::EmbossType::None);
      uiDefIconButR_prop(block,
                         ButType::IconToggle,
                         1,
                         icon,
                         v2d->cur.xmax - UI_UNIT_X - CHANNEL_PAD,
                         y - UI_UNIT_Y / 2.0f,
                         UI_UNIT_X,
                         UI_UNIT_Y,
                         &ptr,
                         chan_prop_lock,
                         0,
                         0,
                         0,
                         std::nullopt);
      UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);
    }

    /* adjust y-position for next one */
    y -= CHANNEL_STEP;
  }
  GPU_blend(GPU_BLEND_NONE);

  UI_block_end(C, block);
  UI_block_draw(C, block);
}
