/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup spaction
 */

/* System includes ----------------------------------------------------- */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

/* Types --------------------------------------------------------------- */

#include "DNA_anim_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_pointcache.h"

/* Everything from source (BIF, BDR, BSE) ------------------------------ */

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"

#include "action_intern.h"

/* ************************************************************************* */
/* Channel List */

void draw_channel_names(bContext *C, bAnimContext *ac, ARegion *region)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  View2D *v2d = &region->v2d;
  size_t items;

  /* build list of channels to draw */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  const int height = ANIM_UI_get_channels_total_height(v2d, items);
  const float pad_bottom = BLI_listbase_is_empty(ac->markers) ? 0 : UI_MARKER_MARGIN_Y;
  v2d->tot.ymin = -(height + pad_bottom);

  /* need to do a view-sync here, so that the keys area doesn't jump around (it must copy this) */
  UI_view2d_sync(NULL, ac->area, v2d, V2D_LOCK_COPY);

  const float channel_step = ANIM_UI_get_channel_step();
  /* Loop through channels, and set up drawing depending on their type. */
  { /* first pass: just the standard GL-drawing for backdrop + text */
    size_t channel_index = 0;
    float ymax = ANIM_UI_get_first_channel_top(v2d);

    for (ale = anim_data.first; ale; ale = ale->next, ymax -= channel_step, channel_index++) {
      const float ymin = ymax - ANIM_UI_get_channel_height();

      /* check if visible */
      if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
          IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
        /* draw all channels using standard channel-drawing API */
        ANIM_channel_draw(ac, ale, ymin, ymax, channel_index);
      }
    }
  }
  { /* second pass: widgets */
    uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
    size_t channel_index = 0;
    float ymax = ANIM_UI_get_first_channel_top(v2d);

    for (ale = anim_data.first; ale; ale = ale->next, ymax -= channel_step, channel_index++) {
      const float ymin = ymax - ANIM_UI_get_channel_height();

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
  }

  /* Free temporary channels. */
  ANIM_animdata_freelist(&anim_data);
}

/* ************************************************************************* */
/* Keyframes */

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD 100.0f

/* Draw manually set intended playback frame ranges for actions. */
static void draw_channel_action_ranges(ListBase *anim_data, View2D *v2d)
{
  /* Variables for coalescing the Y region of one action. */
  bAction *cur_action = NULL;
  AnimData *cur_adt = NULL;
  float cur_ymax;

  /* Walk through channels, grouping contiguous spans referencing the same action. */
  float ymax = ANIM_UI_get_first_channel_top(v2d) + ANIM_UI_get_channel_skip() / 2;
  const float ystep = ANIM_UI_get_channel_step();
  float ymin = ymax - ystep;

  for (bAnimListElem *ale = anim_data->first; ale; ale = ale->next, ymax = ymin, ymin -= ystep) {
    bAction *action = NULL;
    AnimData *adt = NULL;

    /* check if visible */
    if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
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

void draw_channel_strips(bAnimContext *ac, SpaceAction *saction, ARegion *region)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;

  View2D *v2d = &region->v2d;
  bDopeSheet *ads = &saction->ads;
  AnimData *adt = NULL;

  uchar col1[4], col2[4];
  uchar col1a[4], col2a[4];
  uchar col1b[4], col2b[4];
  uchar col_summary[4];

  const bool show_group_colors = U.animation_flag & USER_ANIM_SHOW_CHANNEL_GROUP_COLORS;

  /* get theme colors */
  UI_GetThemeColor4ubv(TH_SHADE2, col2);
  UI_GetThemeColor4ubv(TH_HILITE, col1);
  UI_GetThemeColor4ubv(TH_ANIM_ACTIVE, col_summary);

  UI_GetThemeColor4ubv(TH_GROUP, col2a);
  UI_GetThemeColor4ubv(TH_GROUP_ACTIVE, col1a);

  UI_GetThemeColor4ubv(TH_DOPESHEET_CHANNELOB, col1b);
  UI_GetThemeColor4ubv(TH_DOPESHEET_CHANNELSUBOB, col2b);

  /* build list of channels to draw */
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  size_t items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  const int height = ANIM_UI_get_channels_total_height(v2d, items);
  const float pad_bottom = BLI_listbase_is_empty(ac->markers) ? 0 : UI_MARKER_MARGIN_Y;
  v2d->tot.ymin = -(height + pad_bottom);

  /* Draw the manual frame ranges for actions in the background of the dopesheet.
   * The action editor has already drawn the range for its action so it's not needed. */
  if (ac->datatype == ANIMCONT_DOPESHEET) {
    draw_channel_action_ranges(&anim_data, v2d);
  }

  /* Draw the background strips. */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  GPU_blend(GPU_BLEND_ALPHA);

  /* first backdrop strips */
  float ymax = ANIM_UI_get_first_channel_top(v2d);
  const float channel_step = ANIM_UI_get_channel_step();
  for (ale = anim_data.first; ale; ale = ale->next, ymax -= channel_step) {
    const float ymin = ymax - ANIM_UI_get_channel_height();

    /* check if visible */
    if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
      const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
      int sel = 0;

      /* determine if any need to draw channel */
      if (ale->datatype != ALE_NONE) {
        /* determine if channel is selected */
        if (acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT)) {
          sel = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT);
        }

        if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET, ANIMCONT_SHAPEKEY)) {
          switch (ale->type) {
            case ANIMTYPE_SUMMARY: {
              /* reddish color from NLA */
              immUniformThemeColor(TH_ANIM_ACTIVE);
              break;
            }
            case ANIMTYPE_SCENE:
            case ANIMTYPE_OBJECT: {
              immUniformColor3ubvAlpha(col1b, sel ? col1[3] : col1b[3]);
              break;
            }
            case ANIMTYPE_FILLACTD:
            case ANIMTYPE_DSSKEY:
            case ANIMTYPE_DSWOR: {
              immUniformColor3ubvAlpha(col2b, sel ? col1[3] : col2b[3]);
              break;
            }
            case ANIMTYPE_GROUP: {
              bActionGroup *agrp = ale->data;
              if (show_group_colors && agrp->customCol) {
                if (sel) {
                  immUniformColor3ubvAlpha((uchar *)agrp->cs.select, col1a[3]);
                }
                else {
                  immUniformColor3ubvAlpha((uchar *)agrp->cs.solid, col2a[3]);
                }
              }
              else {
                immUniformColor4ubv(sel ? col1a : col2a);
              }
              break;
            }
            case ANIMTYPE_FCURVE: {
              FCurve *fcu = ale->data;
              if (show_group_colors && fcu->grp && fcu->grp->customCol) {
                immUniformColor3ubvAlpha((uchar *)fcu->grp->cs.active, sel ? col1[3] : col2[3]);
              }
              else {
                immUniformColor4ubv(sel ? col1 : col2);
              }
              break;
            }
            case ANIMTYPE_GPLAYER: {
              if (show_group_colors) {
                uchar gpl_col[4];
                bGPDlayer *gpl = (bGPDlayer *)ale->data;
                rgb_float_to_uchar(gpl_col, gpl->color);
                gpl_col[3] = col1[3];

                immUniformColor4ubv(sel ? col1 : gpl_col);
              }
              else {
                immUniformColor4ubv(sel ? col1 : col2);
              }
              break;
            }
            default: {
              immUniformColor4ubv(sel ? col1 : col2);
            }
          }

          /* draw region twice: firstly backdrop, then the current range */
          immRectf(pos, v2d->cur.xmin, ymin, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymax);
        }
        else if (ac->datatype == ANIMCONT_GPENCIL) {
          uchar *color;
          uchar gpl_col[4];
          if (ale->type == ANIMTYPE_SUMMARY) {
            color = col_summary;
          }
          else if ((show_group_colors) && (ale->type == ANIMTYPE_GPLAYER)) {
            bGPDlayer *gpl = (bGPDlayer *)ale->data;
            rgb_float_to_uchar(gpl_col, gpl->color);
            gpl_col[3] = col1[3];

            color = sel ? col1 : gpl_col;
          }
          else {
            color = sel ? col1 : col2;
          }

          /* Color overlay on frames between the start/end frames. */
          immUniformColor4ubv(color);
          immRectf(pos, ac->scene->r.sfra, ymin, ac->scene->r.efra, ymax);

          /* Color overlay outside the start/end frame range get a more transparent overlay. */
          immUniformColor3ubvAlpha(color, MIN2(255, color[3] / 2));
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
          immUniformColor3ubvAlpha(color, MIN2(255, color[3] / 2));
          immRectf(pos, v2d->cur.xmin, ymin, ac->scene->r.sfra, ymax);
          immRectf(pos, ac->scene->r.efra, ymin, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymax);
        }
      }
    }
  }
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

  /* Draw keyframes
   * 1) Only channels that are visible in the Action Editor get drawn/evaluated.
   *    This is to try to optimize this for heavier data sets
   * 2) Keyframes which are out of view horizontally are disregarded
   */
  int action_flag = saction->flag;

  if (saction->mode == SACTCONT_TIMELINE) {
    action_flag &= ~(SACTION_SHOW_INTERPOLATION | SACTION_SHOW_EXTREMES);
  }

  ymax = ANIM_UI_get_first_channel_top(v2d);

  struct AnimKeylistDrawList *draw_list = ED_keylist_draw_list_create();

  const float scale_factor = ANIM_UI_get_keyframe_scale_factor();

  for (ale = anim_data.first; ale; ale = ale->next, ymax -= channel_step) {
    const float ymin = ymax - ANIM_UI_get_channel_height();
    float ycenter = (ymin + ymax) / 2.0f;

    /* check if visible */
    if (IN_RANGE(ymin, v2d->cur.ymin, v2d->cur.ymax) ||
        IN_RANGE(ymax, v2d->cur.ymin, v2d->cur.ymax)) {
      /* check if anything to show for this channel */
      if (ale->datatype != ALE_NONE) {
        adt = ANIM_nla_mapping_get(ac, ale);

        /* draw 'keyframes' for each specific datatype */
        switch (ale->datatype) {
          case ALE_ALL:
            draw_summary_channel(draw_list, ale->data, ycenter, scale_factor, action_flag);
            break;
          case ALE_SCE:
            draw_scene_channel(draw_list, ads, ale->key_data, ycenter, scale_factor, action_flag);
            break;
          case ALE_OB:
            draw_object_channel(draw_list, ads, ale->key_data, ycenter, scale_factor, action_flag);
            break;
          case ALE_ACT:
            draw_action_channel(draw_list, adt, ale->key_data, ycenter, scale_factor, action_flag);
            break;
          case ALE_GROUP:
            draw_agroup_channel(draw_list, adt, ale->data, ycenter, scale_factor, action_flag);
            break;
          case ALE_FCURVE:
            draw_fcurve_channel(draw_list, adt, ale->key_data, ycenter, scale_factor, action_flag);
            break;
          case ALE_GPFRAME:
            draw_gpl_channel(draw_list, ads, ale->data, ycenter, scale_factor, action_flag);
            break;
          case ALE_MASKLAY:
            draw_masklay_channel(draw_list, ads, ale->data, ycenter, scale_factor, action_flag);
            break;
        }
      }
    }
  }

  ED_keylist_draw_list_flush(draw_list, v2d);
  ED_keylist_draw_list_free(draw_list);

  /* free temporary channels used for drawing */
  ANIM_animdata_freelist(&anim_data);
}

/* ************************************************************************* */
/* Timeline - Caches */

static bool timeline_cache_is_hidden_by_setting(SpaceAction *saction, PTCacheID *pid)
{
  switch (pid->type) {
    case PTCACHE_TYPE_SOFTBODY:
      if ((saction->cache_display & TIME_CACHE_SOFTBODY) == 0) {
        return true;
      }
      break;
    case PTCACHE_TYPE_PARTICLES:
    case PTCACHE_TYPE_SIM_PARTICLES:
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
    case PTCACHE_TYPE_SIM_PARTICLES:
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

static void timeline_cache_modify_color_based_on_state(PointCache *cache, float color[4])
{
  if (cache->flag & PTCACHE_BAKED) {
    color[0] -= 0.4f;
    color[1] -= 0.4f;
    color[2] -= 0.4f;
  }
  else if (cache->flag & PTCACHE_OUTDATED) {
    color[0] += 0.4f;
    color[1] += 0.4f;
    color[2] += 0.4f;
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
    immRectf_fast(pos_id, segment_start - 0.5f, 0, segment_end + 0.5f, 1.0f);
    current = segment_end + 1;
  }

  immEnd();
}

static void timeline_cache_draw_single(PTCacheID *pid, float y_offset, float height, uint pos_id)
{
  GPU_matrix_push();
  GPU_matrix_translate_2f(0.0, (float)V2D_SCROLL_HANDLE_HEIGHT + y_offset);
  GPU_matrix_scale_2f(1.0, height);

  float color[4];
  timeline_cache_color_get(pid, color);

  immUniformColor4fv(color);
  immRectf(pos_id, (float)pid->cache->startframe, 0.0, (float)pid->cache->endframe, 1.0);

  color[3] = 0.4f;
  timeline_cache_modify_color_based_on_state(pid->cache, color);
  immUniformColor4fv(color);

  timeline_cache_draw_cached_segments(pid->cache, pos_id);

  GPU_matrix_pop();
}

void timeline_draw_cache(SpaceAction *saction, Object *ob, Scene *scene)
{
  if ((saction->cache_display & TIME_CACHE_DISPLAY) == 0 || ob == NULL) {
    return;
  }

  ListBase pidlist;
  BKE_ptcache_ids_from_object(&pidlist, ob, scene, 0);

  uint pos_id = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  GPU_blend(GPU_BLEND_ALPHA);

  /* Iterate over pointcaches on the active object, and draw each one's range. */
  float y_offset = 0.0f;
  const float cache_draw_height = 4.0f * UI_DPI_FAC * U.pixelsize;
  LISTBASE_FOREACH (PTCacheID *, pid, &pidlist) {
    if (timeline_cache_is_hidden_by_setting(saction, pid)) {
      continue;
    }

    if (pid->cache->cached_frames == NULL) {
      continue;
    }

    timeline_cache_draw_single(pid, y_offset, cache_draw_height, pos_id);

    y_offset += cache_draw_height;
  }

  GPU_blend(GPU_BLEND_NONE);
  immUnbindProgram();

  BLI_freelistN(&pidlist);
}

/* ************************************************************************* */
