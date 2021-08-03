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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_dlrbTree.h"
#include "BLI_listbase.h"
#include "BLI_range.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"

#include "ED_anim_api.h"
#include "ED_keyframes_keylist.h"

/* *************************** Keyframe Processing *************************** */

typedef struct AnimKeylist {
  DLRBT_Tree keys;
} AnimKeylist;

static void ED_keylist_init(AnimKeylist *keylist)
{
  BLI_dlrbTree_init(&keylist->keys);
}

AnimKeylist *ED_keylist_create(void)
{
  AnimKeylist *keylist = MEM_callocN(sizeof(AnimKeylist), __func__);
  ED_keylist_init(keylist);
  return keylist;
}

void ED_keylist_free(AnimKeylist *keylist)
{
  BLI_assert(keylist);
  BLI_dlrbTree_free(&keylist->keys);
  MEM_freeN(keylist);
}

ActKeyColumn *ED_keylist_find_exact(const AnimKeylist *keylist, float cfra)
{
  return (ActKeyColumn *)BLI_dlrbTree_search_exact(&keylist->keys, compare_ak_cfraPtr, &cfra);
}

ActKeyColumn *ED_keylist_find_next(const AnimKeylist *keylist, float cfra)
{
  return (ActKeyColumn *)BLI_dlrbTree_search_next(&keylist->keys, compare_ak_cfraPtr, &cfra);
}

ActKeyColumn *ED_keylist_find_prev(const AnimKeylist *keylist, float cfra)
{
  return (ActKeyColumn *)BLI_dlrbTree_search_prev(&keylist->keys, compare_ak_cfraPtr, &cfra);
}

/* TODO(jbakker): Should we change this to use `ED_keylist_find_next(keys, min_fra)` and only check
 * boundary of `max_fra`. */
ActKeyColumn *ED_keylist_find_any_between(const AnimKeylist *keylist, float min_fra, float max_fra)
{
  for (ActKeyColumn *ak = keylist->keys.root; ak;
       ak = (ak->cfra < min_fra) ? ak->right : ak->left) {
    if (IN_RANGE(ak->cfra, min_fra, max_fra)) {
      return ak;
    }
  }
  return NULL;
}

bool ED_keylist_is_empty(const struct AnimKeylist *keylist)
{
  return keylist->keys.root == NULL;
}

const struct ListBase *ED_keylist_listbase(const AnimKeylist *keylist)
{
  return (ListBase *)&keylist->keys;
}

bool ED_keylist_frame_range(const struct AnimKeylist *keylist, Range2f *r_frame_range)
{
  BLI_assert(r_frame_range);

  if (ED_keylist_is_empty(keylist)) {
    return false;
  }

  const ActKeyColumn *first_column = (const ActKeyColumn *)keylist->keys.first;
  r_frame_range->min = first_column->cfra;

  const ActKeyColumn *last_column = (const ActKeyColumn *)keylist->keys.last;
  r_frame_range->max = last_column->cfra;

  return true;
}
/* ActKeyColumns (Keyframe Columns) ------------------------------------------ */

BLI_INLINE bool is_cfra_eq(float a, float b)
{
  return IS_EQT(a, b, BEZT_BINARYSEARCH_THRESH);
}

BLI_INLINE bool is_cfra_lt(float a, float b)
{
  return (b - a) > BEZT_BINARYSEARCH_THRESH;
}

/* Comparator callback used for ActKeyColumns and cframe float-value pointer */
/* NOTE: this is exported to other modules that use the ActKeyColumns for finding keyframes */
short compare_ak_cfraPtr(void *node, void *data)
{
  ActKeyColumn *ak = (ActKeyColumn *)node;
  const float *cframe = data;
  float val = *cframe;

  if (is_cfra_eq(val, ak->cfra)) {
    return 0;
  }

  if (val < ak->cfra) {
    return -1;
  }
  return 1;
}

/* --------------- */

/* Set of references to three logically adjacent keys. */
typedef struct BezTripleChain {
  /* Current keyframe. */
  BezTriple *cur;

  /* Logical neighbors. May be NULL. */
  BezTriple *prev, *next;
} BezTripleChain;

/* Categorize the interpolation & handle type of the keyframe. */
static eKeyframeHandleDrawOpts bezt_handle_type(BezTriple *bezt)
{
  if (bezt->h1 == HD_AUTO_ANIM && bezt->h2 == HD_AUTO_ANIM) {
    return KEYFRAME_HANDLE_AUTO_CLAMP;
  }
  if (ELEM(bezt->h1, HD_AUTO_ANIM, HD_AUTO) && ELEM(bezt->h2, HD_AUTO_ANIM, HD_AUTO)) {
    return KEYFRAME_HANDLE_AUTO;
  }
  if (bezt->h1 == HD_VECT && bezt->h2 == HD_VECT) {
    return KEYFRAME_HANDLE_VECTOR;
  }
  if (ELEM(HD_FREE, bezt->h1, bezt->h2)) {
    return KEYFRAME_HANDLE_FREE;
  }
  return KEYFRAME_HANDLE_ALIGNED;
}

/* Determine if the keyframe is an extreme by comparing with neighbors.
 * Ends of fixed-value sections and of the whole curve are also marked.
 */
static eKeyframeExtremeDrawOpts bezt_extreme_type(BezTripleChain *chain)
{
  if (chain->prev == NULL && chain->next == NULL) {
    return KEYFRAME_EXTREME_NONE;
  }

  /* Keyframe values for the current one and neighbors. */
  float cur_y = chain->cur->vec[1][1];
  float prev_y = cur_y, next_y = cur_y;

  if (chain->prev && !IS_EQF(cur_y, chain->prev->vec[1][1])) {
    prev_y = chain->prev->vec[1][1];
  }
  if (chain->next && !IS_EQF(cur_y, chain->next->vec[1][1])) {
    next_y = chain->next->vec[1][1];
  }

  /* Static hold. */
  if (prev_y == cur_y && next_y == cur_y) {
    return KEYFRAME_EXTREME_FLAT;
  }

  /* Middle of an incline. */
  if ((prev_y < cur_y && next_y > cur_y) || (prev_y > cur_y && next_y < cur_y)) {
    return KEYFRAME_EXTREME_NONE;
  }

  /* Bezier handle values for the overshoot check. */
  bool l_bezier = chain->prev && chain->prev->ipo == BEZT_IPO_BEZ;
  bool r_bezier = chain->next && chain->cur->ipo == BEZT_IPO_BEZ;
  float handle_l = l_bezier ? chain->cur->vec[0][1] : cur_y;
  float handle_r = r_bezier ? chain->cur->vec[2][1] : cur_y;

  /* Detect extremes. One of the neighbors is allowed to be equal to current. */
  if (prev_y < cur_y || next_y < cur_y) {
    bool is_overshoot = (handle_l > cur_y || handle_r > cur_y);

    return KEYFRAME_EXTREME_MAX | (is_overshoot ? KEYFRAME_EXTREME_MIXED : 0);
  }

  if (prev_y > cur_y || next_y > cur_y) {
    bool is_overshoot = (handle_l < cur_y || handle_r < cur_y);

    return KEYFRAME_EXTREME_MIN | (is_overshoot ? KEYFRAME_EXTREME_MIXED : 0);
  }

  return KEYFRAME_EXTREME_NONE;
}

/* Comparator callback used for ActKeyColumns and BezTripleChain */
static short compare_ak_bezt(void *node, void *data)
{
  BezTripleChain *chain = data;

  return compare_ak_cfraPtr(node, &chain->cur->vec[1][0]);
}

/* New node callback used for building ActKeyColumns from BezTripleChain */
static DLRBT_Node *nalloc_ak_bezt(void *data)
{
  ActKeyColumn *ak = MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumn");
  BezTripleChain *chain = data;
  BezTriple *bezt = chain->cur;

  /* store settings based on state of BezTriple */
  ak->cfra = bezt->vec[1][0];
  ak->sel = BEZT_ISSEL_ANY(bezt) ? SELECT : 0;
  ak->key_type = BEZKEYTYPE(bezt);
  ak->handle_type = bezt_handle_type(bezt);
  ak->extreme_type = bezt_extreme_type(chain);

  /* count keyframes in this column */
  ak->totkey = 1;

  return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from BezTripleChain */
static void nupdate_ak_bezt(void *node, void *data)
{
  ActKeyColumn *ak = node;
  BezTripleChain *chain = data;
  BezTriple *bezt = chain->cur;

  /* set selection status and 'touched' status */
  if (BEZT_ISSEL_ANY(bezt)) {
    ak->sel = SELECT;
  }

  /* count keyframes in this column */
  ak->totkey++;

  /* For keyframe type, 'proper' keyframes have priority over breakdowns
   * (and other types for now). */
  if (BEZKEYTYPE(bezt) == BEZT_KEYTYPE_KEYFRAME) {
    ak->key_type = BEZT_KEYTYPE_KEYFRAME;
  }

  /* For interpolation type, select the highest value (enum is sorted). */
  ak->handle_type = MAX2(ak->handle_type, bezt_handle_type(bezt));

  /* For extremes, detect when combining different states. */
  char new_extreme = bezt_extreme_type(chain);

  if (new_extreme != ak->extreme_type) {
    /* Replace the flat status without adding mixed. */
    if (ak->extreme_type == KEYFRAME_EXTREME_FLAT) {
      ak->extreme_type = new_extreme;
    }
    else if (new_extreme != KEYFRAME_EXTREME_FLAT) {
      ak->extreme_type |= (new_extreme | KEYFRAME_EXTREME_MIXED);
    }
  }
}

/* ......... */

/* Comparator callback used for ActKeyColumns and GPencil frame */
static short compare_ak_gpframe(void *node, void *data)
{
  bGPDframe *gpf = (bGPDframe *)data;

  float frame = gpf->framenum;
  return compare_ak_cfraPtr(node, &frame);
}

/* New node callback used for building ActKeyColumns from GPencil frames */
static DLRBT_Node *nalloc_ak_gpframe(void *data)
{
  ActKeyColumn *ak = MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumnGPF");
  bGPDframe *gpf = (bGPDframe *)data;

  /* store settings based on state of BezTriple */
  ak->cfra = gpf->framenum;
  ak->sel = (gpf->flag & GP_FRAME_SELECT) ? SELECT : 0;
  ak->key_type = gpf->key_type;

  /* count keyframes in this column */
  ak->totkey = 1;
  /* Set as visible block. */
  ak->totblock = 1;
  ak->block.sel = ak->sel;
  ak->block.flag |= ACTKEYBLOCK_FLAG_GPENCIL;

  return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames */
static void nupdate_ak_gpframe(void *node, void *data)
{
  ActKeyColumn *ak = (ActKeyColumn *)node;
  bGPDframe *gpf = (bGPDframe *)data;

  /* set selection status and 'touched' status */
  if (gpf->flag & GP_FRAME_SELECT) {
    ak->sel = SELECT;
  }

  /* count keyframes in this column */
  ak->totkey++;

  /* for keyframe type, 'proper' keyframes have priority over breakdowns
   * (and other types for now). */
  if (gpf->key_type == BEZT_KEYTYPE_KEYFRAME) {
    ak->key_type = BEZT_KEYTYPE_KEYFRAME;
  }
}

/* ......... */

/* Comparator callback used for ActKeyColumns and GPencil frame */
static short compare_ak_masklayshape(void *node, void *data)
{
  MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

  float frame = masklay_shape->frame;
  return compare_ak_cfraPtr(node, &frame);
}

/* New node callback used for building ActKeyColumns from GPencil frames */
static DLRBT_Node *nalloc_ak_masklayshape(void *data)
{
  ActKeyColumn *ak = MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumnGPF");
  MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

  /* store settings based on state of BezTriple */
  ak->cfra = masklay_shape->frame;
  ak->sel = (masklay_shape->flag & MASK_SHAPE_SELECT) ? SELECT : 0;

  /* count keyframes in this column */
  ak->totkey = 1;

  return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames */
static void nupdate_ak_masklayshape(void *node, void *data)
{
  ActKeyColumn *ak = (ActKeyColumn *)node;
  MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

  /* set selection status and 'touched' status */
  if (masklay_shape->flag & MASK_SHAPE_SELECT) {
    ak->sel = SELECT;
  }

  /* count keyframes in this column */
  ak->totkey++;
}

/* --------------- */

/* Add the given BezTriple to the given 'list' of Keyframes */
static void add_bezt_to_keycolumns_list(AnimKeylist *keylist, BezTripleChain *bezt)
{
  if (ELEM(NULL, keylist, bezt)) {
    return;
  }

  BLI_dlrbTree_add(&keylist->keys, compare_ak_bezt, nalloc_ak_bezt, nupdate_ak_bezt, bezt);
}

/* Add the given GPencil Frame to the given 'list' of Keyframes */
static void add_gpframe_to_keycolumns_list(AnimKeylist *keylist, bGPDframe *gpf)
{
  if (ELEM(NULL, keylist, gpf)) {
    return;
  }

  BLI_dlrbTree_add(&keylist->keys, compare_ak_gpframe, nalloc_ak_gpframe, nupdate_ak_gpframe, gpf);
}

/* Add the given MaskLayerShape Frame to the given 'list' of Keyframes */
static void add_masklay_to_keycolumns_list(AnimKeylist *keylist, MaskLayerShape *masklay_shape)
{
  if (ELEM(NULL, keylist, masklay_shape)) {
    return;
  }

  BLI_dlrbTree_add(&keylist->keys,
                   compare_ak_masklayshape,
                   nalloc_ak_masklayshape,
                   nupdate_ak_masklayshape,
                   masklay_shape);
}

/* ActKeyBlocks (Long Keyframes) ------------------------------------------ */

static const ActKeyBlockInfo dummy_keyblock = {0};

static void compute_keyblock_data(ActKeyBlockInfo *info, BezTriple *prev, BezTriple *beztn)
{
  memset(info, 0, sizeof(ActKeyBlockInfo));

  if (BEZKEYTYPE(beztn) == BEZT_KEYTYPE_MOVEHOLD) {
    /* Animator tagged a "moving hold"
     *   - Previous key must also be tagged as a moving hold, otherwise
     *     we're just dealing with the first of a pair, and we don't
     *     want to be creating any phantom holds...
     */
    if (BEZKEYTYPE(prev) == BEZT_KEYTYPE_MOVEHOLD) {
      info->flag |= ACTKEYBLOCK_FLAG_MOVING_HOLD | ACTKEYBLOCK_FLAG_ANY_HOLD;
    }
  }

  /* Check for same values...
   *  - Handles must have same central value as each other
   *  - Handles which control that section of the curve must be constant
   */
  if (IS_EQF(beztn->vec[1][1], prev->vec[1][1])) {
    bool hold;

    /* Only check handles in case of actual bezier interpolation. */
    if (prev->ipo == BEZT_IPO_BEZ) {
      hold = IS_EQF(beztn->vec[1][1], beztn->vec[0][1]) &&
             IS_EQF(prev->vec[1][1], prev->vec[2][1]);
    }
    /* This interpolation type induces movement even between identical keys. */
    else {
      hold = !ELEM(prev->ipo, BEZT_IPO_ELASTIC);
    }

    if (hold) {
      info->flag |= ACTKEYBLOCK_FLAG_STATIC_HOLD | ACTKEYBLOCK_FLAG_ANY_HOLD;
    }
  }

  /* Remember non-bezier interpolation info. */
  if (prev->ipo != BEZT_IPO_BEZ) {
    info->flag |= ACTKEYBLOCK_FLAG_NON_BEZIER;
  }

  info->sel = BEZT_ISSEL_ANY(prev) || BEZT_ISSEL_ANY(beztn);
}

static void add_keyblock_info(ActKeyColumn *col, const ActKeyBlockInfo *block)
{
  /* New curve and block. */
  if (col->totcurve <= 1 && col->totblock == 0) {
    memcpy(&col->block, block, sizeof(ActKeyBlockInfo));
  }
  /* Existing curve. */
  else {
    col->block.conflict |= (col->block.flag ^ block->flag);
    col->block.flag |= block->flag;
    col->block.sel |= block->sel;
  }

  if (block->flag) {
    col->totblock++;
  }
}

static void add_bezt_to_keyblocks_list(AnimKeylist *keylist, BezTriple *bezt, int bezt_len)
{
  ActKeyColumn *col = keylist->keys.first;

  if (bezt && bezt_len >= 2) {
    ActKeyBlockInfo block;

    /* Find the first key column while inserting dummy blocks. */
    for (; col != NULL && is_cfra_lt(col->cfra, bezt[0].vec[1][0]); col = col->next) {
      add_keyblock_info(col, &dummy_keyblock);
    }

    BLI_assert(col != NULL);

    /* Insert real blocks. */
    for (int v = 1; col != NULL && v < bezt_len; v++, bezt++) {
      /* Wrong order of bezier keys: resync position. */
      if (is_cfra_lt(bezt[1].vec[1][0], bezt[0].vec[1][0])) {
        /* Backtrack to find the right location. */
        if (is_cfra_lt(bezt[1].vec[1][0], col->cfra)) {
          ActKeyColumn *newcol = (ActKeyColumn *)BLI_dlrbTree_search_exact(
              &keylist->keys, compare_ak_cfraPtr, &bezt[1].vec[1][0]);

          if (newcol != NULL) {
            col = newcol;

            /* The previous keyblock is garbage too. */
            if (col->prev != NULL) {
              add_keyblock_info(col->prev, &dummy_keyblock);
            }
          }
          else {
            BLI_assert(false);
          }
        }

        continue;
      }

      /* Normal sequence */
      BLI_assert(is_cfra_eq(col->cfra, bezt[0].vec[1][0]));

      compute_keyblock_data(&block, bezt, bezt + 1);

      for (; col != NULL && is_cfra_lt(col->cfra, bezt[1].vec[1][0]); col = col->next) {
        add_keyblock_info(col, &block);
      }

      BLI_assert(col != NULL);
    }
  }

  /* Insert dummy blocks at the end. */
  for (; col != NULL; col = col->next) {
    add_keyblock_info(col, &dummy_keyblock);
  }
}

/* Walk through columns and propagate blocks and totcurve.
 *
 * This must be called even by animation sources that don't generate
 * keyblocks to keep the data structure consistent after adding columns.
 */
static void update_keyblocks(AnimKeylist *keylist, BezTriple *bezt, int bezt_len)
{
  /* Recompute the prev/next linked list. */
  BLI_dlrbTree_linkedlist_sync(&keylist->keys);

  /* Find the curve count */
  int max_curve = 0;

  LISTBASE_FOREACH (ActKeyColumn *, col, &keylist->keys) {
    max_curve = MAX2(max_curve, col->totcurve);
  }

  /* Propagate blocks to inserted keys */
  ActKeyColumn *prev_ready = NULL;

  LISTBASE_FOREACH (ActKeyColumn *, col, &keylist->keys) {
    /* Pre-existing column. */
    if (col->totcurve > 0) {
      prev_ready = col;
    }
    /* Newly inserted column, so copy block data from previous. */
    else if (prev_ready != NULL) {
      col->totblock = prev_ready->totblock;
      memcpy(&col->block, &prev_ready->block, sizeof(ActKeyBlockInfo));
    }

    col->totcurve = max_curve + 1;
  }

  /* Add blocks on top */
  add_bezt_to_keyblocks_list(keylist, bezt, bezt_len);
}

/* --------- */

bool actkeyblock_is_valid(ActKeyColumn *ac)
{
  return ac != NULL && ac->next != NULL && ac->totblock > 0;
}

/* Checks if ActKeyBlock should exist... */
int actkeyblock_get_valid_hold(ActKeyColumn *ac)
{
  /* check that block is valid */
  if (!actkeyblock_is_valid(ac)) {
    return 0;
  }

  const int hold_mask = (ACTKEYBLOCK_FLAG_ANY_HOLD | ACTKEYBLOCK_FLAG_STATIC_HOLD);
  return (ac->block.flag & ~ac->block.conflict) & hold_mask;
}

/* *************************** Keyframe List Conversions *************************** */

void summary_to_keylist(bAnimContext *ac, AnimKeylist *keylist, int saction_flag)
{
  if (ac) {
    ListBase anim_data = {NULL, NULL};
    bAnimListElem *ale;
    int filter;

    /* get F-Curves to take keyframes from */
    filter = ANIMFILTER_DATA_VISIBLE;
    ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

    /* loop through each F-Curve, grabbing the keyframes */
    for (ale = anim_data.first; ale; ale = ale->next) {
      /* Why not use all #eAnim_KeyType here?
       * All of the other key types are actually "summaries" themselves,
       * and will just end up duplicating stuff that comes up through
       * standard filtering of just F-Curves. Given the way that these work,
       * there isn't really any benefit at all from including them. - Aligorith */

      switch (ale->datatype) {
        case ALE_FCURVE:
          fcurve_to_keylist(ale->adt, ale->data, keylist, saction_flag);
          break;
        case ALE_MASKLAY:
          mask_to_keylist(ac->ads, ale->data, keylist);
          break;
        case ALE_GPFRAME:
          gpl_to_keylist(ac->ads, ale->data, keylist);
          break;
        default:
          // printf("%s: datatype %d unhandled\n", __func__, ale->datatype);
          break;
      }
    }

    ANIM_animdata_freelist(&anim_data);
  }
}

void scene_to_keylist(bDopeSheet *ads, Scene *sce, AnimKeylist *keylist, int saction_flag)
{
  bAnimContext ac = {NULL};
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  bAnimListElem dummychan = {NULL};

  if (sce == NULL) {
    return;
  }

  /* create a dummy wrapper data to work with */
  dummychan.type = ANIMTYPE_SCENE;
  dummychan.data = sce;
  dummychan.id = &sce->id;
  dummychan.adt = sce->adt;

  ac.ads = ads;
  ac.data = &dummychan;
  ac.datatype = ANIMCONT_CHANNEL;

  /* get F-Curves to take keyframes from */
  filter = ANIMFILTER_DATA_VISIBLE; /* curves only */
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* loop through each F-Curve, grabbing the keyframes */
  for (ale = anim_data.first; ale; ale = ale->next) {
    fcurve_to_keylist(ale->adt, ale->data, keylist, saction_flag);
  }

  ANIM_animdata_freelist(&anim_data);
}

void ob_to_keylist(bDopeSheet *ads, Object *ob, AnimKeylist *keylist, int saction_flag)
{
  bAnimContext ac = {NULL};
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  bAnimListElem dummychan = {NULL};
  Base dummybase = {NULL};

  if (ob == NULL) {
    return;
  }

  /* create a dummy wrapper data to work with */
  dummybase.object = ob;

  dummychan.type = ANIMTYPE_OBJECT;
  dummychan.data = &dummybase;
  dummychan.id = &ob->id;
  dummychan.adt = ob->adt;

  ac.ads = ads;
  ac.data = &dummychan;
  ac.datatype = ANIMCONT_CHANNEL;

  /* get F-Curves to take keyframes from */
  filter = ANIMFILTER_DATA_VISIBLE; /* curves only */
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* loop through each F-Curve, grabbing the keyframes */
  for (ale = anim_data.first; ale; ale = ale->next) {
    fcurve_to_keylist(ale->adt, ale->data, keylist, saction_flag);
  }

  ANIM_animdata_freelist(&anim_data);
}

void cachefile_to_keylist(bDopeSheet *ads,
                          CacheFile *cache_file,
                          AnimKeylist *keylist,
                          int saction_flag)
{
  if (cache_file == NULL) {
    return;
  }

  /* create a dummy wrapper data to work with */
  bAnimListElem dummychan = {NULL};
  dummychan.type = ANIMTYPE_DSCACHEFILE;
  dummychan.data = cache_file;
  dummychan.id = &cache_file->id;
  dummychan.adt = cache_file->adt;

  bAnimContext ac = {NULL};
  ac.ads = ads;
  ac.data = &dummychan;
  ac.datatype = ANIMCONT_CHANNEL;

  /* get F-Curves to take keyframes from */
  ListBase anim_data = {NULL, NULL};
  int filter = ANIMFILTER_DATA_VISIBLE; /* curves only */
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* loop through each F-Curve, grabbing the keyframes */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    fcurve_to_keylist(ale->adt, ale->data, keylist, saction_flag);
  }

  ANIM_animdata_freelist(&anim_data);
}

void fcurve_to_keylist(AnimData *adt, FCurve *fcu, AnimKeylist *keylist, int saction_flag)
{
  if (fcu && fcu->totvert && fcu->bezt) {
    /* apply NLA-mapping (if applicable) */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 0);
    }

    /* Check if the curve is cyclic. */
    bool is_cyclic = BKE_fcurve_is_cyclic(fcu) && (fcu->totvert >= 2);
    bool do_extremes = (saction_flag & SACTION_SHOW_EXTREMES) != 0;

    /* loop through beztriples, making ActKeysColumns */
    BezTripleChain chain = {0};

    for (int v = 0; v < fcu->totvert; v++) {
      chain.cur = &fcu->bezt[v];

      /* Neighbor keys, accounting for being cyclic. */
      if (do_extremes) {
        chain.prev = (v > 0) ? &fcu->bezt[v - 1] : is_cyclic ? &fcu->bezt[fcu->totvert - 2] : NULL;
        chain.next = (v + 1 < fcu->totvert) ? &fcu->bezt[v + 1] : is_cyclic ? &fcu->bezt[1] : NULL;
      }

      add_bezt_to_keycolumns_list(keylist, &chain);
    }

    /* Update keyblocks. */
    update_keyblocks(keylist, fcu->bezt, fcu->totvert);

    /* unapply NLA-mapping if applicable */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 0);
    }
  }
}

void agroup_to_keylist(AnimData *adt, bActionGroup *agrp, AnimKeylist *keylist, int saction_flag)
{
  FCurve *fcu;

  if (agrp) {
    /* loop through F-Curves */
    for (fcu = agrp->channels.first; fcu && fcu->grp == agrp; fcu = fcu->next) {
      fcurve_to_keylist(adt, fcu, keylist, saction_flag);
    }
  }
}

void action_to_keylist(AnimData *adt, bAction *act, AnimKeylist *keylist, int saction_flag)
{
  FCurve *fcu;

  if (act) {
    /* loop through F-Curves */
    for (fcu = act->curves.first; fcu; fcu = fcu->next) {
      fcurve_to_keylist(adt, fcu, keylist, saction_flag);
    }
  }
}

void gpencil_to_keylist(bDopeSheet *ads, bGPdata *gpd, AnimKeylist *keylist, const bool active)
{
  bGPDlayer *gpl;

  if (gpd && keylist) {
    /* for now, just aggregate out all the frames, but only for visible layers */
    for (gpl = gpd->layers.last; gpl; gpl = gpl->prev) {
      if ((gpl->flag & GP_LAYER_HIDE) == 0) {
        if ((!active) || ((active) && (gpl->flag & GP_LAYER_SELECT))) {
          gpl_to_keylist(ads, gpl, keylist);
        }
      }
    }
  }
}

void gpl_to_keylist(bDopeSheet *UNUSED(ads), bGPDlayer *gpl, AnimKeylist *keylist)
{
  bGPDframe *gpf;

  if (gpl && keylist) {
    /* Although the frames should already be in an ordered list,
     * they are not suitable for displaying yet. */
    for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
      add_gpframe_to_keycolumns_list(keylist, gpf);
    }

    update_keyblocks(keylist, NULL, 0);
  }
}

void mask_to_keylist(bDopeSheet *UNUSED(ads), MaskLayer *masklay, AnimKeylist *keylist)
{
  MaskLayerShape *masklay_shape;

  if (masklay && keylist) {
    for (masklay_shape = masklay->splines_shapes.first; masklay_shape;
         masklay_shape = masklay_shape->next) {
      add_masklay_to_keycolumns_list(keylist, masklay_shape);
    }

    update_keyblocks(keylist, NULL, 0);
  }
}
