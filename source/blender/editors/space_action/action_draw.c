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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

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

/* left hand part */
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

  int height = ACHANNEL_TOT_HEIGHT(ac, items);
  v2d->tot.ymin = -height;

  /* need to do a view-sync here, so that the keys area doesn't jump around (it must copy this) */
  UI_view2d_sync(NULL, ac->area, v2d, V2D_LOCK_COPY);

  /* loop through channels, and set up drawing depending on their type  */
  { /* first pass: just the standard GL-drawing for backdrop + text */
    size_t channel_index = 0;
    float ymax = ACHANNEL_FIRST_TOP(ac);

    for (ale = anim_data.first; ale; ale = ale->next, ymax -= ACHANNEL_STEP(ac), channel_index++) {
      float ymin = ymax - ACHANNEL_HEIGHT(ac);

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
    float ymax = ACHANNEL_FIRST_TOP(ac);

    for (ale = anim_data.first; ale; ale = ale->next, ymax -= ACHANNEL_STEP(ac), channel_index++) {
      float ymin = ymax - ACHANNEL_HEIGHT(ac);

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

  /* free tempolary channels */
  ANIM_animdata_freelist(&anim_data);
}

/* ************************************************************************* */
/* Keyframes */

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD 100.0f

/* draw keyframes in each channel */
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

  const bool show_group_colors = !(saction->flag & SACTION_NODRAWGCOLORS);

  /* get theme colors */
  UI_GetThemeColor4ubv(TH_SHADE2, col2);
  UI_GetThemeColor4ubv(TH_HILITE, col1);

  UI_GetThemeColor4ubv(TH_GROUP, col2a);
  UI_GetThemeColor4ubv(TH_GROUP_ACTIVE, col1a);

  UI_GetThemeColor4ubv(TH_DOPESHEET_CHANNELOB, col1b);
  UI_GetThemeColor4ubv(TH_DOPESHEET_CHANNELSUBOB, col2b);

  /* build list of channels to draw */
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  size_t items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  int height = ACHANNEL_TOT_HEIGHT(ac, items);
  v2d->tot.ymin = -height;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  GPU_blend(true);

  /* first backdrop strips */
  float ymax = ACHANNEL_FIRST_TOP(ac);

  for (ale = anim_data.first; ale; ale = ale->next, ymax -= ACHANNEL_STEP(ac)) {
    float ymin = ymax - ACHANNEL_HEIGHT(ac);

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
          if ((show_group_colors) && (ale->type == ANIMTYPE_GPLAYER)) {
            bGPDlayer *gpl = (bGPDlayer *)ale->data;
            rgb_float_to_uchar(gpl_col, gpl->color);
            gpl_col[3] = col1[3];

            color = sel ? col1 : gpl_col;
          }
          else {
            color = sel ? col1 : col2;
          }
          /* frames less than one get less saturated background */
          immUniformColor4ubv(color);
          immRectf(pos, 0.0f, ymin, v2d->cur.xmin, ymax);

          /* frames one and higher get a saturated background */
          immUniformColor3ubvAlpha(color, MIN2(255, color[3] * 2));
          immRectf(pos, v2d->cur.xmin, ymin, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymax);
        }
        else if (ac->datatype == ANIMCONT_MASK) {
          /* TODO --- this is a copy of gpencil */
          /* frames less than one get less saturated background */
          uchar *color = sel ? col1 : col2;
          immUniformColor4ubv(color);
          immRectf(pos, 0.0f, ymin, v2d->cur.xmin, ymax);

          /* frames one and higher get a saturated background */
          immUniformColor3ubvAlpha(color, MIN2(255, color[3] * 2));
          immRectf(pos, v2d->cur.xmin, ymin, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymax);
        }
      }
    }
  }
  GPU_blend(false);

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

  ymax = ACHANNEL_FIRST_TOP(ac);

  for (ale = anim_data.first; ale; ale = ale->next, ymax -= ACHANNEL_STEP(ac)) {
    float ymin = ymax - ACHANNEL_HEIGHT(ac);
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
            draw_summary_channel(v2d, ale->data, ycenter, ac->yscale_fac, action_flag);
            break;
          case ALE_SCE:
            draw_scene_channel(v2d, ads, ale->key_data, ycenter, ac->yscale_fac, action_flag);
            break;
          case ALE_OB:
            draw_object_channel(v2d, ads, ale->key_data, ycenter, ac->yscale_fac, action_flag);
            break;
          case ALE_ACT:
            draw_action_channel(v2d, adt, ale->key_data, ycenter, ac->yscale_fac, action_flag);
            break;
          case ALE_GROUP:
            draw_agroup_channel(v2d, adt, ale->data, ycenter, ac->yscale_fac, action_flag);
            break;
          case ALE_FCURVE:
            draw_fcurve_channel(v2d, adt, ale->key_data, ycenter, ac->yscale_fac, action_flag);
            break;
          case ALE_GPFRAME:
            draw_gpl_channel(v2d, ads, ale->data, ycenter, ac->yscale_fac, action_flag);
            break;
          case ALE_MASKLAY:
            draw_masklay_channel(v2d, ads, ale->data, ycenter, ac->yscale_fac, action_flag);
            break;
        }
      }
    }
  }

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
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  GPU_blend(true);

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

  GPU_blend(false);
  immUnbindProgram();

  BLI_freelistN(&pidlist);
}

/* ************************************************************************* */
