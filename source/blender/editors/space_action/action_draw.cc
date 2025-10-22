/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

/* System includes ----------------------------------------------------- */

#include <cfloat>
#include <cstdlib>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

/* Types --------------------------------------------------------------- */

#include "DNA_anim_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_pointcache.h"

#include "ANIM_action.hh"

/* Everything from source (BIF, BDR, BSE) ------------------------------ */

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_draw.hh"

#include "MOD_nodes.hh"

#include "action_intern.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Channel List
 * \{ */

void draw_channel_names(bContext *C,
                        bAnimContext *ac,
                        ARegion *region,
                        const ListBase /*bAnimListElem*/ &anim_data)
{
  bAnimListElem *ale;
  View2D *v2d = &region->v2d;
  /* need to do a view-sync here, so that the keys area doesn't jump around (it must copy this) */
  UI_view2d_sync(nullptr, ac->area, v2d, V2D_LOCK_COPY);

  const float channel_step = ANIM_UI_get_channel_step();
  /* Loop through channels, and set up drawing depending on their type. */
  { /* first pass: just the standard GL-drawing for backdrop + text */
    size_t channel_index = 0;
    float ymax = ANIM_UI_get_first_channel_top(v2d);

    for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
         ale = ale->next, ymax -= channel_step, channel_index++)
    {
      const float ymin = ymax - ANIM_UI_get_channel_height();

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax))
      {
        /* draw all channels using standard channel-drawing API */
        ANIM_channel_draw(ac, ale, ymin, ymax, channel_index);
      }
    }
  }
  { /* second pass: widgets */
    uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
    size_t channel_index = 0;
    float ymax = ANIM_UI_get_first_channel_top(v2d);

    for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
         ale = ale->next, ymax -= channel_step, channel_index++)
    {
      const float ymin = ymax - ANIM_UI_get_channel_height();

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax))
      {
        /* draw all channels using standard channel-drawing API */
        rctf channel_rect;
        BLI_rctf_init(&channel_rect, 0, v2d->cur.xmax, ymin, ymax);
        ANIM_channel_draw_widgets(C, ac, ale, block, &channel_rect, channel_index);
      }
    }

    UI_block_end(C, block);
    UI_block_draw(C, block);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframes
 * \{ */

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD 100.0f

/* Draw manually set intended playback frame ranges for actions. */
static void draw_channel_action_ranges(ListBase *anim_data, View2D *v2d)
{
  /* Variables for coalescing the Y region of one action. */
  bAction *cur_action = nullptr;
  AnimData *cur_adt = nullptr;
  float cur_ymax;

  /* Walk through channels, grouping contiguous spans referencing the same action. */
  float ymax = ANIM_UI_get_first_channel_top(v2d) + ANIM_UI_get_channel_skip() / 2;
  const float ystep = ANIM_UI_get_channel_step();
  float ymin = ymax - ystep;

  for (bAnimListElem *ale = static_cast<bAnimListElem *>(anim_data->first); ale;
       ale = ale->next, ymax = ymin, ymin -= ystep)
  {
    bAction *action = nullptr;
    AnimData *adt = nullptr;

    /* check if visible */
    if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax))
    {
      /* check if anything to show for this channel */
      if (ale->datatype != ALE_NONE) {
        action = ANIM_channel_action_get(ale);

        if (action) {
          adt = ale->adt;
        }
      }
    }

    /* Extend the current region, or flush and restart. */
    if (action != cur_action || adt != cur_adt) {
      if (cur_action) {
        ANIM_draw_action_framerange(cur_adt, cur_action, v2d, ymax, cur_ymax);
      }

      cur_action = action;
      cur_adt = adt;
      cur_ymax = ymax;
    }
  }

  /* Flush the last region. */
  if (cur_action) {
    ANIM_draw_action_framerange(cur_adt, cur_action, v2d, ymax, cur_ymax);
  }
}

static void draw_backdrops(bAnimContext *ac, ListBase &anim_data, View2D *v2d, uint pos)
{
  uchar col1[4], col2[4];
  uchar col1a[4], col2a[4];
  uchar col1b[4], col2b[4];
  uchar col_summary[4];

  /* get theme colors */
  UI_GetThemeColor4ubv(TH_CHANNEL, col2);
  UI_GetThemeColor4ubv(TH_CHANNEL_SELECT, col1);
  UI_GetThemeColor4ubv(TH_ANIM_ACTIVE, col_summary);

  UI_GetThemeColor4ubv(TH_GROUP, col2a);
  UI_GetThemeColor4ubv(TH_GROUP_ACTIVE, col1a);

  UI_GetThemeColor4ubv(TH_DOPESHEET_CHANNELOB, col1b);
  UI_GetThemeColor4ubv(TH_DOPESHEET_CHANNELSUBOB, col2b);

  float ymax = ANIM_UI_get_first_channel_top(v2d);
  const float channel_step = ANIM_UI_get_channel_step();
  bAnimListElem *ale;
  for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
       ale = ale->next, ymax -= channel_step)
  {
    const float ymin = ymax - ANIM_UI_get_channel_height();

    /* check if visible */
    if (!(IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)))
    {
      continue;
    }
    const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
    int sel = 0;

    /* determine if any need to draw channel */
    if (ale->datatype == ALE_NONE) {
      continue;
    }
    /* determine if channel is selected */
    if (acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT)) {
      sel = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT);
    }

    if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET, ANIMCONT_SHAPEKEY)) {
      switch (ale->type) {
        case ANIMTYPE_SUMMARY: {
          if (!ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND)) {
            /* Only draw the summary line backdrop when it is expanded. If the entire dope sheet is
             * just one line, there is no need for any distinction between lines, and the red-ish
             * color is only going to be a distraction. */
            continue;
          }
          immUniformThemeColor(TH_ANIM_ACTIVE);
          break;
        }
        case ANIMTYPE_ACTION_SLOT:
        case ANIMTYPE_SCENE:
        case ANIMTYPE_OBJECT: {
          immUniformColor3ubvAlpha(col1b, sel ? col1[3] : col1b[3]);
          break;
        }
        case ANIMTYPE_FILLACTD:
        case ANIMTYPE_FILLACT_LAYERED:
        case ANIMTYPE_DSSKEY:
        case ANIMTYPE_DSWOR: {
          immUniformColor3ubvAlpha(col2b, sel ? col1[3] : col2b[3]);
          break;
        }
        case ANIMTYPE_GROUP:
          immUniformColor4ubv(sel ? col1a : col2a);
          break;
        default: {
          immUniformColor4ubv(sel ? col1 : col2);
        }
      }

      /* draw region twice: firstly backdrop, then the current range */
      immRectf(pos, v2d->cur.xmin, ymin, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymax);
    }
    else if (ac->datatype == ANIMCONT_GPENCIL) {
      uchar *color;
      switch (ale->type) {
        case ANIMTYPE_SUMMARY:
          color = col_summary;
          break;

        case ANIMTYPE_GREASE_PENCIL_LAYER_GROUP:
          color = sel ? col1a : col2a;
          break;

        case ANIMTYPE_GREASE_PENCIL_DATABLOCK:
          color = col2b;
          color[3] = sel ? col1[3] : col2b[3];
          break;

        default:
          color = sel ? col1 : col2;
          break;
      }

      /* Color overlay on frames between the start/end frames. */
      immUniformColor4ubv(color);
      immRectf(pos, ac->scene->r.sfra, ymin, ac->scene->r.efra, ymax);

      /* Color overlay outside the start/end frame range get a more transparent overlay. */
      immUniformColor3ubvAlpha(color, std::min(255, color[3] / 2));
      immRectf(pos, v2d->cur.xmin, ymin, ac->scene->r.sfra, ymax);
      immRectf(pos, ac->scene->r.efra, ymin, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymax);
    }
    else if (ac->datatype == ANIMCONT_MASK) {
      /* TODO: this is a copy of gpencil. */
      uchar *color;
      if (ale->type == ANIMTYPE_SUMMARY) {
        color = col_summary;
      }
      else {
        color = sel ? col1 : col2;
      }

      /* Color overlay on frames between the start/end frames. */
      immUniformColor4ubv(color);
      immRectf(pos, ac->scene->r.sfra, ymin, ac->scene->r.efra, ymax);

      /* Color overlay outside the start/end frame range get a more transparent overlay. */
      immUniformColor3ubvAlpha(color, std::min(255, color[3] / 2));
      immRectf(pos, v2d->cur.xmin, ymin, ac->scene->r.sfra, ymax);
      immRectf(pos, ac->scene->r.efra, ymin, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymax);
    }

    /* Alpha-over the channel color, if it's there. */
    {
      const bool show_group_colors = U.animation_flag & USER_ANIM_SHOW_CHANNEL_GROUP_COLORS;
      uint8_t color[3];
      if (show_group_colors && acf->get_channel_color && acf->get_channel_color(ale, color)) {
        immUniformColor3ubvAlpha(color, 32);
        immRectf(pos, v2d->cur.xmin, ymin, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymax);
      }
    }
  }
}

static void draw_keyframes(bAnimContext *ac,
                           View2D *v2d,
                           SpaceAction *saction,
                           ListBase &anim_data)
{
  /* Draw keyframes
   * 1) Only channels that are visible in the Action Editor get drawn/evaluated.
   *    This is to try to optimize this for heavier data sets
   * 2) Keyframes which are out of view horizontally are disregarded
   */
  int action_flag = saction->flag;
  bDopeSheet *ads = &saction->ads;

  if (saction->mode == SACTCONT_TIMELINE) {
    action_flag &= ~SACTION_SHOW_INTERPOLATION;
  }

  const float channel_step = ANIM_UI_get_channel_step();
  float ymax = ANIM_UI_get_first_channel_top(v2d);

  ChannelDrawList *draw_list = ED_channel_draw_list_create();

  const float scale_factor = ANIM_UI_get_keyframe_scale_factor();

  bAnimListElem *ale;
  for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
       ale = ale->next, ymax -= channel_step)
  {
    const float ymin = ymax - ANIM_UI_get_channel_height();
    float ycenter = (ymin + ymax) / 2.0f;

    /* check if visible */
    if (!(IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)))
    {
      continue;
    }

    /* check if anything to show for this channel */
    if (ale->datatype == ALE_NONE) {
      continue;
    }

    /* Add channels to list to draw later. */
    switch (ale->datatype) {
      case ALE_ALL:
        ED_add_summary_channel(
            draw_list, static_cast<bAnimContext *>(ale->data), ycenter, scale_factor, action_flag);
        break;
      case ALE_SCE:
        ED_add_scene_channel(draw_list,
                             ads,
                             static_cast<Scene *>(ale->key_data),
                             ycenter,
                             scale_factor,
                             action_flag);
        break;
      case ALE_OB:
        ED_add_object_channel(draw_list,
                              ads,
                              static_cast<Object *>(ale->key_data),
                              ycenter,
                              scale_factor,
                              action_flag);
        break;
      case ALE_ACTION_LAYERED:
        ED_add_action_layered_channel(draw_list,
                                      ac,
                                      ale,
                                      static_cast<bAction *>(ale->key_data),
                                      ycenter,
                                      scale_factor,
                                      action_flag);
        break;
      case ALE_ACTION_SLOT:
        ED_add_action_slot_channel(draw_list,
                                   ac,
                                   ale,
                                   static_cast<bAction *>(ale->key_data)->wrap(),
                                   *static_cast<animrig::Slot *>(ale->data),
                                   ycenter,
                                   scale_factor,
                                   action_flag);
        break;
      case ALE_ACT:
        ED_add_action_channel(draw_list,
                              ale,
                              static_cast<bAction *>(ale->key_data),
                              ycenter,
                              scale_factor,
                              action_flag);
        break;
      case ALE_GROUP:
        ED_add_action_group_channel(draw_list,
                                    ale,
                                    static_cast<bActionGroup *>(ale->data),
                                    ycenter,
                                    scale_factor,
                                    action_flag);
        break;
      case ALE_FCURVE: {
        ED_add_fcurve_channel(draw_list,
                              ale,
                              static_cast<FCurve *>(ale->key_data),
                              ycenter,
                              scale_factor,
                              action_flag);
        break;
      }
      case ALE_GREASE_PENCIL_CEL:
        ED_add_grease_pencil_cels_channel(draw_list,
                                          ads,
                                          static_cast<const GreasePencilLayer *>(ale->data),
                                          ycenter,
                                          scale_factor,
                                          action_flag);
        break;
      case ALE_GREASE_PENCIL_GROUP:
        ED_add_grease_pencil_layer_group_channel(
            draw_list,
            ads,
            static_cast<const GreasePencilLayerTreeGroup *>(ale->data),
            ycenter,
            scale_factor,
            action_flag);
        break;
      case ALE_GREASE_PENCIL_DATA:
        ED_add_grease_pencil_datablock_channel(draw_list,
                                               ac,
                                               ale,
                                               static_cast<const GreasePencil *>(ale->data),
                                               ycenter,
                                               scale_factor,
                                               action_flag);
        break;
      case ALE_GPFRAME:
        ED_add_grease_pencil_layer_legacy_channel(draw_list,
                                                  ads,
                                                  static_cast<bGPDlayer *>(ale->data),
                                                  ycenter,
                                                  scale_factor,
                                                  action_flag);
        break;
      case ALE_MASKLAY:
        ED_add_mask_layer_channel(draw_list,
                                  ads,
                                  static_cast<MaskLayer *>(ale->data),
                                  ycenter,
                                  scale_factor,
                                  action_flag);
        break;
      case ALE_NONE:
      case ALE_NLASTRIP:
        break;
    }
  }

  /* Drawing happens in here. */
  ED_channel_list_flush(draw_list, v2d);
  ED_channel_list_free(draw_list);
}

void draw_channel_strips(bAnimContext *ac,
                         SpaceAction *saction,
                         ARegion *region,
                         ListBase *anim_data)
{
  View2D *v2d = &region->v2d;

  /* Draw the manual frame ranges for actions in the background of the dope-sheet.
   * The action editor has already drawn the range for its action so it's not needed. */
  if (ac->datatype == ANIMCONT_DOPESHEET) {
    draw_channel_action_ranges(anim_data, v2d);
  }

  /* Draw the background strips. */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  GPU_blend(GPU_BLEND_ALPHA);

  /* first backdrop strips */
  draw_backdrops(ac, *anim_data, v2d, pos);

  GPU_blend(GPU_BLEND_NONE);

  /* black line marking 'current frame' for Time-Slide transform mode */
  if (saction->flag & SACTION_MOVING) {
    immUniformColor3f(0.0f, 0.0f, 0.0f);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, saction->timeslide, v2d->cur.ymin - EXTRA_SCROLL_PAD);
    immVertex2f(pos, saction->timeslide, v2d->cur.ymax);
    immEnd();
  }
  immUnbindProgram();

  draw_keyframes(ac, v2d, saction, *anim_data);

  /* free temporary channels used for drawing */
  ANIM_animdata_freelist(anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Timeline - Caches
 * \{ */

static bool timeline_cache_is_hidden_by_setting(const SpaceAction *saction, const PTCacheID *pid)
{
  switch (pid->type) {
    case PTCACHE_TYPE_SOFTBODY:
      if ((saction->cache_display & TIME_CACHE_SOFTBODY) == 0) {
        return true;
      }
      break;
    case PTCACHE_TYPE_PARTICLES:
      if ((saction->cache_display & TIME_CACHE_PARTICLES) == 0) {
        return true;
      }
      break;
    case PTCACHE_TYPE_CLOTH:
      if ((saction->cache_display & TIME_CACHE_CLOTH) == 0) {
        return true;
      }
      break;
    case PTCACHE_TYPE_SMOKE_DOMAIN:
    case PTCACHE_TYPE_SMOKE_HIGHRES:
      if ((saction->cache_display & TIME_CACHE_SMOKE) == 0) {
        return true;
      }
      break;
    case PTCACHE_TYPE_DYNAMICPAINT:
      if ((saction->cache_display & TIME_CACHE_DYNAMICPAINT) == 0) {
        return true;
      }
      break;
    case PTCACHE_TYPE_RIGIDBODY:
      if ((saction->cache_display & TIME_CACHE_RIGIDBODY) == 0) {
        return true;
      }
      break;
  }
  return false;
}

static void timeline_cache_color_get(PTCacheID *pid, float color[4])
{
  switch (pid->type) {
    case PTCACHE_TYPE_SOFTBODY:
      color[0] = 1.0;
      color[1] = 0.4;
      color[2] = 0.02;
      color[3] = 0.1;
      break;
    case PTCACHE_TYPE_PARTICLES:
      color[0] = 1.0;
      color[1] = 0.1;
      color[2] = 0.02;
      color[3] = 0.1;
      break;
    case PTCACHE_TYPE_CLOTH:
      color[0] = 0.1;
      color[1] = 0.1;
      color[2] = 0.75;
      color[3] = 0.1;
      break;
    case PTCACHE_TYPE_SMOKE_DOMAIN:
    case PTCACHE_TYPE_SMOKE_HIGHRES:
      color[0] = 0.2;
      color[1] = 0.2;
      color[2] = 0.2;
      color[3] = 0.1;
      break;
    case PTCACHE_TYPE_DYNAMICPAINT:
      color[0] = 1.0;
      color[1] = 0.1;
      color[2] = 0.75;
      color[3] = 0.1;
      break;
    case PTCACHE_TYPE_RIGIDBODY:
      color[0] = 1.0;
      color[1] = 0.6;
      color[2] = 0.0;
      color[3] = 0.1;
      break;
    default:
      color[0] = 1.0;
      color[1] = 0.0;
      color[2] = 1.0;
      color[3] = 0.1;
      BLI_assert(0);
      break;
  }
}

static void timeline_cache_modify_color_based_on_state(PointCache *cache,
                                                       float color[4],
                                                       float color_state[4])
{
  if (cache->flag & PTCACHE_BAKED) {
    color[3] = color_state[3] = 1.0f;
  }
  else if (cache->flag & PTCACHE_OUTDATED) {
    color[3] = color_state[3] = 0.7f;
    mul_v3_fl(color_state, 0.5f);
  }
  else {
    color[3] = color_state[3] = 0.7f;
  }
}

static bool timeline_cache_find_next_cached_segment(PointCache *cache,
                                                    int search_start_frame,
                                                    int *r_segment_start,
                                                    int *r_segment_end)
{
  int offset = cache->startframe;
  int current = search_start_frame;

  /* Find segment start frame. */
  while (true) {
    if (current > cache->endframe) {
      return false;
    }
    if (cache->cached_frames[current - offset]) {
      *r_segment_start = current;
      break;
    }
    current++;
  }

  /* Find segment end frame. */
  while (true) {
    if (current > cache->endframe) {
      *r_segment_end = current - 1;
      return true;
    }
    if (!cache->cached_frames[current - offset]) {
      *r_segment_end = current - 1;
      return true;
    }
    current++;
  }
}

static uint timeline_cache_segments_count(PointCache *cache)
{
  uint count = 0;

  int current = cache->startframe;
  int segment_start;
  int segment_end;
  while (timeline_cache_find_next_cached_segment(cache, current, &segment_start, &segment_end)) {
    count++;
    current = segment_end + 1;
  }

  return count;
}

static void timeline_cache_draw_cached_segments(PointCache *cache, uint pos_id)
{
  uint segments_count = timeline_cache_segments_count(cache);
  if (segments_count == 0) {
    return;
  }

  immBeginAtMost(GPU_PRIM_TRIS, segments_count * 6);

  int current = cache->startframe;
  int segment_start;
  int segment_end;
  while (timeline_cache_find_next_cached_segment(cache, current, &segment_start, &segment_end)) {
    immRectf_fast(pos_id, segment_start, 0, segment_end + 1.0f, 1.0f);
    current = segment_end + 1;
  }

  immEnd();
}

static void timeline_cache_draw_single(PTCacheID *pid, float y_offset, float height, uint pos_id)
{
  GPU_matrix_push();
  GPU_matrix_translate_2f(0.0, float(V2D_SCROLL_HANDLE_HEIGHT) + y_offset);
  GPU_matrix_scale_2f(1.0, height);

  blender::ColorTheme4f color;
  timeline_cache_color_get(pid, color);

  /* Mix in the background color to tone it down a bit. */
  blender::ColorTheme4f background;
  UI_GetThemeColor4fv(TH_BACK, background);

  interp_v3_v3v3(color, color, background, 0.6f);

  /* Highlight the frame range of the simulation. */
  immUniform4fv("color1", color);
  immUniform4fv("color2", color);
  immRectf(pos_id, float(pid->cache->startframe), 0.0, float(pid->cache->endframe), 1.0);

  /* Now show the cached frames on top. */
  blender::ColorTheme4f color_state;
  copy_v4_v4(color_state, color);

  timeline_cache_modify_color_based_on_state(pid->cache, color, color_state);

  immUniform4fv("color1", color);
  immUniform4fv("color2", color_state);

  timeline_cache_draw_cached_segments(pid->cache, pos_id);

  GPU_matrix_pop();
}

struct CacheRange {
  blender::IndexRange frames;
  blender::bke::bake::CacheStatus status;
};

static void timeline_cache_draw_geometry_nodes(const blender::Span<CacheRange> cache_ranges,
                                               const bool all_simulations_baked,
                                               float *y_offset,
                                               const float line_height,
                                               const uint pos_id)
{
  if (cache_ranges.is_empty()) {
    return;
  }

  bool has_bake = false;

  for (const CacheRange &sim_range : cache_ranges) {
    switch (sim_range.status) {
      case blender::bke::bake::CacheStatus::Invalid:
      case blender::bke::bake::CacheStatus::Valid:
        break;
      case blender::bke::bake::CacheStatus::Baked:
        has_bake = true;
        break;
    }
  }

  blender::Set<int> status_change_frames_set;
  for (const CacheRange &sim_range : cache_ranges) {
    status_change_frames_set.add(sim_range.frames.first());
    status_change_frames_set.add(sim_range.frames.one_after_last());
  }
  blender::Vector<int> status_change_frames;
  status_change_frames.extend(status_change_frames_set.begin(), status_change_frames_set.end());
  std::sort(status_change_frames.begin(), status_change_frames.end());
  const blender::OffsetIndices<int> frame_ranges = status_change_frames.as_span();

  GPU_matrix_push();
  GPU_matrix_translate_2f(0.0, float(V2D_SCROLL_HANDLE_HEIGHT) + *y_offset);
  GPU_matrix_scale_2f(1.0, line_height);

  blender::ColorTheme4f base_color;
  UI_GetThemeColor4fv(TH_SIMULATED_FRAMES, base_color);
  blender::ColorTheme4f invalid_color = base_color;
  mul_v3_fl(invalid_color, 0.5f);
  invalid_color.a *= 0.7f;
  blender::ColorTheme4f valid_color = base_color;
  valid_color.a *= 0.7f;
  blender::ColorTheme4f baked_color = base_color;

  float max_used_height = 1.0f;
  for (const int range_i : frame_ranges.index_range()) {
    const blender::IndexRange frame_range = frame_ranges[range_i];
    const int start_frame = frame_range.first();
    const int end_frame = frame_range.last();

    bool has_bake_at_frame = false;
    bool has_valid_at_frame = false;
    bool has_invalid_at_frame = false;
    for (const CacheRange &sim_range : cache_ranges) {
      if (sim_range.frames.contains(start_frame)) {
        switch (sim_range.status) {
          case blender::bke::bake::CacheStatus::Invalid:
            has_invalid_at_frame = true;
            break;
          case blender::bke::bake::CacheStatus::Valid:
            has_valid_at_frame = true;
            break;
          case blender::bke::bake::CacheStatus::Baked:
            has_bake_at_frame = true;
            break;
        }
      }
    }
    if (!(has_bake_at_frame || has_valid_at_frame || has_invalid_at_frame)) {
      continue;
    }

    if (all_simulations_baked) {
      immUniform4fv("color1", baked_color);
      immUniform4fv("color2", baked_color);
      immBeginAtMost(GPU_PRIM_TRIS, 6);
      immRectf_fast(pos_id, start_frame, 0, end_frame + 1.0f, 1.0f);
      immEnd();
    }
    else {
      if (has_valid_at_frame || has_invalid_at_frame) {
        immUniform4fv("color1", valid_color);
        immUniform4fv("color2", has_invalid_at_frame ? invalid_color : valid_color);
        immBeginAtMost(GPU_PRIM_TRIS, 6);
        const float top = has_bake ? 2.0f : 1.0f;
        immRectf_fast(pos_id, start_frame, 0.0f, end_frame + 1.0f, top);
        immEnd();
        max_used_height = top;
      }
      if (has_bake_at_frame) {
        immUniform4fv("color1", baked_color);
        immUniform4fv("color2", baked_color);
        immBeginAtMost(GPU_PRIM_TRIS, 6);
        immRectf_fast(pos_id, start_frame, 0, end_frame + 1.0f, 1.0f);
        immEnd();
      }
    }
  }
  GPU_matrix_pop();

  *y_offset += max_used_height * 2;
}

void timeline_draw_cache(const SpaceAction *saction, const Object *ob, const Scene *scene)
{
  if ((saction->cache_display & TIME_CACHE_DISPLAY) == 0 || ob == nullptr) {
    return;
  }

  ListBase pidlist;
  BKE_ptcache_ids_from_object(&pidlist, const_cast<Object *>(ob), const_cast<Scene *>(scene), 0);

  uint pos_id = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_2D_DIAG_STRIPES);

  GPU_blend(GPU_BLEND_ALPHA);

  /* Iterate over point-caches on the active object, and draw each one's range. */
  float y_offset = 0.0f;
  const float cache_draw_height = 4.0f * UI_SCALE_FAC * U.pixelsize;

  immUniform1i("size1", cache_draw_height * 2.0f);
  immUniform1i("size2", cache_draw_height);

  LISTBASE_FOREACH (PTCacheID *, pid, &pidlist) {
    if (timeline_cache_is_hidden_by_setting(saction, pid)) {
      continue;
    }

    if (pid->cache->cached_frames == nullptr) {
      continue;
    }

    timeline_cache_draw_single(pid, y_offset, cache_draw_height, pos_id);

    y_offset += cache_draw_height;
  }
  if (saction->cache_display & TIME_CACHE_SIMULATION_NODES) {
    blender::Vector<CacheRange> cache_ranges;
    bool all_simulations_baked = true;
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type != eModifierType_Nodes) {
        continue;
      }
      const NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
      if (nmd->node_group == nullptr) {
        continue;
      }
      if (!nmd->runtime->cache) {
        continue;
      }
      if (nmd->node_group->nested_node_refs_num == 0) {
        /* Skip when there are no bake nodes or simulations. */
        continue;
      }
      const blender::bke::bake::ModifierCache &modifier_cache = *nmd->runtime->cache;
      {
        std::lock_guard lock{modifier_cache.mutex};
        for (const auto item : modifier_cache.simulation_cache_by_id.items()) {
          const blender::bke::bake::SimulationNodeCache &node_cache = *item.value;
          if (node_cache.bake.frames.is_empty()) {
            all_simulations_baked = false;
            continue;
          }
          if (node_cache.cache_status != blender::bke::bake::CacheStatus::Baked) {
            all_simulations_baked = false;
          }
          cache_ranges.append({node_cache.bake.frame_range(), node_cache.cache_status});
        }
        for (const auto item : modifier_cache.bake_cache_by_id.items()) {
          const NodesModifierBake *bake = nmd->find_bake(item.key);
          if (!bake) {
            continue;
          }
          if (bake->bake_mode == NODES_MODIFIER_BAKE_MODE_STILL) {
            continue;
          }
          const blender::bke::bake::BakeNodeCache &node_cache = *item.value;
          if (node_cache.bake.frames.is_empty()) {
            continue;
          }
          cache_ranges.append(
              {node_cache.bake.frame_range(), blender::bke::bake::CacheStatus::Baked});
        }
      }
    }
    timeline_cache_draw_geometry_nodes(
        cache_ranges, all_simulations_baked, &y_offset, cache_draw_height, pos_id);
  }

  GPU_blend(GPU_BLEND_NONE);
  immUnbindProgram();

  BLI_freelistN(&pidlist);
}

/** \} */
