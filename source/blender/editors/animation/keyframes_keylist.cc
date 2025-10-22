/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

/* System includes ----------------------------------------------------- */

#include <algorithm>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_fcurve.hh"
#include "BKE_grease_pencil.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_keylist.hh"

#include "SEQ_retiming.hh"

#include "ANIM_action.hh"

using namespace blender;

/* *************************** Keyframe Processing *************************** */

/* ActKeyColumns (Keyframe Columns) ------------------------------------------ */

BLI_INLINE bool is_cfra_eq(const float a, const float b)
{
  return IS_EQT(a, b, BEZT_BINARYSEARCH_THRESH);
}

BLI_INLINE bool is_cfra_lt(const float a, const float b)
{
  return (b - a) > BEZT_BINARYSEARCH_THRESH;
}

/* --------------- */

/**
 * Animation data of Grease Pencil cels,
 * which are drawings positioned in time.
 */
struct GreasePencilCel {
  int frame_number;
  GreasePencilFrame frame;
};

struct AnimKeylist {
  /* Number of ActKeyColumn's in the keylist. */
  size_t column_len = 0;

  bool is_runtime_initialized = false;

  /* Before initializing the runtime, the key_columns list base is used to quickly add columns.
   * Contains `ActKeyColumn`. Should not be used after runtime is initialized. */
  ListBase /*ActKeyColumn*/ key_columns;
  /* Last accessed column in the key_columns list base. Inserting columns are typically done in
   * order. The last accessed column is used as starting point to search for a location to add or
   * update the next column. */
  std::optional<ActKeyColumn *> last_accessed_column = std::nullopt;

  struct {
    /* When initializing the runtime the columns from the list base `AnimKeyList.key_columns` are
     * transferred to an array to support binary searching and index based access. */
    blender::Array<ActKeyColumn> key_columns;
    /* Wrapper around runtime.key_columns so it can still be accessed as a ListBase.
     * Elements are owned by `runtime.key_columns`. */
    ListBase /*ActKeyColumn*/ list_wrapper;
  } runtime;

  AnimKeylist()
  {
    BLI_listbase_clear(&this->key_columns);
    BLI_listbase_clear(&this->runtime.list_wrapper);
  }

  ~AnimKeylist()
  {
    BLI_freelistN(&this->key_columns);
    BLI_listbase_clear(&this->runtime.list_wrapper);
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("editors:AnimKeylist")
};

AnimKeylist *ED_keylist_create()
{
  AnimKeylist *keylist = new AnimKeylist();
  return keylist;
}

void ED_keylist_free(AnimKeylist *keylist)
{
  BLI_assert(keylist);
  delete keylist;
}

static void keylist_convert_key_columns_to_array(AnimKeylist *keylist)
{
  size_t index;
  LISTBASE_FOREACH_INDEX (ActKeyColumn *, key, &keylist->key_columns, index) {
    keylist->runtime.key_columns[index] = *key;
  }
}

static void keylist_runtime_update_key_column_next_prev(AnimKeylist *keylist)
{
  for (size_t index = 0; index < keylist->column_len; index++) {
    const bool is_first = (index == 0);
    keylist->runtime.key_columns[index].prev = is_first ? nullptr :
                                                          &keylist->runtime.key_columns[index - 1];
    const bool is_last = (index == keylist->column_len - 1);
    keylist->runtime.key_columns[index].next = is_last ? nullptr :
                                                         &keylist->runtime.key_columns[index + 1];
  }
}

static void keylist_runtime_init_listbase(AnimKeylist *keylist)
{
  if (ED_keylist_is_empty(keylist)) {
    BLI_listbase_clear(&keylist->runtime.list_wrapper);
    return;
  }

  keylist->runtime.list_wrapper.first = keylist->runtime.key_columns.data();
  keylist->runtime.list_wrapper.last = &keylist->runtime.key_columns[keylist->column_len - 1];
}

static void keylist_runtime_init(AnimKeylist *keylist)
{
  BLI_assert(!keylist->is_runtime_initialized);

  keylist->runtime.key_columns = blender::Array<ActKeyColumn>(keylist->column_len);

  /* Convert linked list to array to support fast searching. */
  keylist_convert_key_columns_to_array(keylist);
  /* Ensure that the array can also be used as a listbase for external usages. */
  keylist_runtime_update_key_column_next_prev(keylist);
  keylist_runtime_init_listbase(keylist);

  keylist->is_runtime_initialized = true;
}

static void keylist_reset_last_accessed(AnimKeylist *keylist)
{
  BLI_assert(!keylist->is_runtime_initialized);
  keylist->last_accessed_column.reset();
}

void ED_keylist_prepare_for_direct_access(AnimKeylist *keylist)
{
  if (keylist->is_runtime_initialized) {
    return;
  }
  keylist_runtime_init(keylist);
}

static const ActKeyColumn *keylist_find_lower_bound(const AnimKeylist *keylist, const float cfra)
{
  BLI_assert(!ED_keylist_is_empty(keylist));
  const ActKeyColumn *begin = std::begin(keylist->runtime.key_columns);
  const ActKeyColumn *end = std::end(keylist->runtime.key_columns);
  ActKeyColumn value;
  value.cfra = cfra;

  const ActKeyColumn *found_column = std::lower_bound(
      begin, end, value, [](const ActKeyColumn &column, const ActKeyColumn &other) {
        return is_cfra_lt(column.cfra, other.cfra);
      });
  return found_column;
}

static const ActKeyColumn *keylist_find_upper_bound(const AnimKeylist *keylist, const float cfra)
{
  BLI_assert(!ED_keylist_is_empty(keylist));
  const ActKeyColumn *begin = std::begin(keylist->runtime.key_columns);
  const ActKeyColumn *end = std::end(keylist->runtime.key_columns);
  ActKeyColumn value;
  value.cfra = cfra;

  const ActKeyColumn *found_column = std::upper_bound(
      begin, end, value, [](const ActKeyColumn &column, const ActKeyColumn &other) {
        return is_cfra_lt(column.cfra, other.cfra);
      });
  return found_column;
}

const ActKeyColumn *ED_keylist_find_exact(const AnimKeylist *keylist, const float cfra)
{
  BLI_assert_msg(keylist->is_runtime_initialized,
                 "ED_keylist_prepare_for_direct_access needs to be called before searching.");

  if (ED_keylist_is_empty(keylist)) {
    return nullptr;
  }

  const ActKeyColumn *found_column = keylist_find_lower_bound(keylist, cfra);

  const ActKeyColumn *end = std::end(keylist->runtime.key_columns);
  if (found_column == end) {
    return nullptr;
  }
  if (is_cfra_eq(found_column->cfra, cfra)) {
    return found_column;
  }
  return nullptr;
}

const ActKeyColumn *ED_keylist_find_next(const AnimKeylist *keylist, const float cfra)
{
  BLI_assert_msg(keylist->is_runtime_initialized,
                 "ED_keylist_prepare_for_direct_access needs to be called before searching.");

  if (ED_keylist_is_empty(keylist)) {
    return nullptr;
  }

  const ActKeyColumn *found_column = keylist_find_upper_bound(keylist, cfra);

  const ActKeyColumn *end = std::end(keylist->runtime.key_columns);
  if (found_column == end) {
    return nullptr;
  }
  return found_column;
}

const ActKeyColumn *ED_keylist_find_prev(const AnimKeylist *keylist, const float cfra)
{
  BLI_assert_msg(keylist->is_runtime_initialized,
                 "ED_keylist_prepare_for_direct_access needs to be called before searching.");

  if (ED_keylist_is_empty(keylist)) {
    return nullptr;
  }

  const ActKeyColumn *end = std::end(keylist->runtime.key_columns);
  const ActKeyColumn *found_column = keylist_find_lower_bound(keylist, cfra);

  if (found_column == end) {
    /* Nothing found, return the last item. */
    return end - 1;
  }

  const ActKeyColumn *prev_column = found_column->prev;
  return prev_column;
}

const ActKeyColumn *ED_keylist_find_closest(const AnimKeylist *keylist, const float cfra)
{
  BLI_assert_msg(keylist->is_runtime_initialized,
                 "ED_keylist_prepare_for_direct_access needs to be called before searching.");
  if (ED_keylist_is_empty(keylist)) {
    return nullptr;
  }
  /* Need to check against #BEZT_BINARYSEARCH_THRESH because #ED_keylist_find_prev does so as well.
   * Not doing that here could cause that function to return a nullptr. */
  if (cfra - keylist->runtime.key_columns.first().cfra < BEZT_BINARYSEARCH_THRESH) {
    return &keylist->runtime.key_columns.first();
  }
  if (cfra - keylist->runtime.key_columns.last().cfra > BEZT_BINARYSEARCH_THRESH) {
    keylist->runtime.key_columns.last();
  }
  const ActKeyColumn *prev = ED_keylist_find_prev(keylist, cfra);
  BLI_assert_msg(prev != nullptr,
                 "This should exist since we checked for cfra bounds just before");
  /* This could be a nullptr though. */
  const ActKeyColumn *next = prev->next;

  if (!next) {
    return prev;
  }

  const float prev_delta = cfra - prev->cfra;
  const float next_delta = next->cfra - cfra;
  /* `prev_delta` and `next_delta` can both be 0 if the given `cfra` is exactly at a key column. */

  if (prev_delta <= next_delta) {
    return prev;
  }
  return next;
}

const ActKeyColumn *ED_keylist_find_any_between(const AnimKeylist *keylist,
                                                const Bounds<float> frame_range)
{
  BLI_assert_msg(keylist->is_runtime_initialized,
                 "ED_keylist_prepare_for_direct_access needs to be called before searching.");

  if (ED_keylist_is_empty(keylist)) {
    return nullptr;
  }

  const ActKeyColumn *column = keylist_find_lower_bound(keylist, frame_range.min);
  const ActKeyColumn *end = std::end(keylist->runtime.key_columns);
  if (column == end) {
    return nullptr;
  }
  if (column->cfra >= frame_range.max) {
    return nullptr;
  }
  return column;
}

const ActKeyColumn *ED_keylist_array(const AnimKeylist *keylist)
{
  BLI_assert_msg(
      keylist->is_runtime_initialized,
      "ED_keylist_prepare_for_direct_access needs to be called before accessing array.");
  return keylist->runtime.key_columns.data();
}

int64_t ED_keylist_array_len(const AnimKeylist *keylist)
{
  return keylist->column_len;
}

bool ED_keylist_is_empty(const AnimKeylist *keylist)
{
  return keylist->column_len == 0;
}

const ListBase *ED_keylist_listbase(const AnimKeylist *keylist)
{
  if (keylist->is_runtime_initialized) {
    return &keylist->runtime.list_wrapper;
  }
  return &keylist->key_columns;
}

static void keylist_first_last(const AnimKeylist *keylist,
                               const ActKeyColumn **first_column,
                               const ActKeyColumn **last_column)
{
  if (keylist->is_runtime_initialized) {
    *first_column = keylist->runtime.key_columns.data();
    *last_column = &keylist->runtime.key_columns[keylist->column_len - 1];
  }
  else {
    *first_column = static_cast<const ActKeyColumn *>(keylist->key_columns.first);
    *last_column = static_cast<const ActKeyColumn *>(keylist->key_columns.last);
  }
}

bool ED_keylist_all_keys_frame_range(const AnimKeylist *keylist, Bounds<float> *r_frame_range)
{
  BLI_assert(r_frame_range);

  if (ED_keylist_is_empty(keylist)) {
    return false;
  }

  const ActKeyColumn *first_column;
  const ActKeyColumn *last_column;
  keylist_first_last(keylist, &first_column, &last_column);
  r_frame_range->min = first_column->cfra;
  r_frame_range->max = last_column->cfra;

  return true;
}

bool ED_keylist_selected_keys_frame_range(const AnimKeylist *keylist, Bounds<float> *r_frame_range)
{
  BLI_assert(r_frame_range);

  if (ED_keylist_is_empty(keylist)) {
    return false;
  }

  const ActKeyColumn *first_column;
  const ActKeyColumn *last_column;
  keylist_first_last(keylist, &first_column, &last_column);
  while (first_column && !(first_column->sel & SELECT)) {
    first_column = first_column->next;
  }
  while (last_column && !(last_column->sel & SELECT)) {
    last_column = last_column->prev;
  }
  if (!first_column || !last_column || first_column == last_column) {
    return false;
  }
  r_frame_range->min = first_column->cfra;
  r_frame_range->max = last_column->cfra;

  return true;
}

/* Set of references to three logically adjacent keys. */
struct BezTripleChain {
  /* Current keyframe. */
  BezTriple *cur;

  /* Logical neighbors. May be nullptr. */
  BezTriple *prev, *next;
};

/* Categorize the interpolation & handle type of the keyframe. */
static eKeyframeHandleDrawOpts bezt_handle_type(const BezTriple *bezt)
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
static eKeyframeExtremeDrawOpts bezt_extreme_type(const BezTripleChain *chain)
{
  if (chain->prev == nullptr && chain->next == nullptr) {
    return KEYFRAME_EXTREME_NONE;
  }

  /* Keyframe values for the current one and neighbors. */
  const float cur_y = chain->cur->vec[1][1];
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
  const bool l_bezier = chain->prev && chain->prev->ipo == BEZT_IPO_BEZ;
  const bool r_bezier = chain->next && chain->cur->ipo == BEZT_IPO_BEZ;
  const float handle_l = l_bezier ? chain->cur->vec[0][1] : cur_y;
  const float handle_r = r_bezier ? chain->cur->vec[2][1] : cur_y;

  /* Detect extremes. One of the neighbors is allowed to be equal to current. */
  if (prev_y < cur_y || next_y < cur_y) {
    const bool is_overshoot = (handle_l > cur_y || handle_r > cur_y);

    return static_cast<eKeyframeExtremeDrawOpts>(KEYFRAME_EXTREME_MAX |
                                                 (is_overshoot ? KEYFRAME_EXTREME_MIXED : 0));
  }

  if (prev_y > cur_y || next_y > cur_y) {
    const bool is_overshoot = (handle_l < cur_y || handle_r < cur_y);

    return static_cast<eKeyframeExtremeDrawOpts>(KEYFRAME_EXTREME_MIN |
                                                 (is_overshoot ? KEYFRAME_EXTREME_MIXED : 0));
  }

  return KEYFRAME_EXTREME_NONE;
}

/* New node callback used for building ActKeyColumns from BezTripleChain */
static ActKeyColumn *nalloc_ak_bezt(void *data)
{
  ActKeyColumn *ak = MEM_callocN<ActKeyColumn>("ActKeyColumn");
  const BezTripleChain *chain = static_cast<const BezTripleChain *>(data);
  const BezTriple *bezt = chain->cur;

  /* store settings based on state of BezTriple */
  ak->cfra = bezt->vec[1][0];
  ak->sel = BEZT_ISSEL_ANY(bezt) ? SELECT : 0;
  ak->key_type = BEZKEYTYPE(bezt);
  ak->handle_type = bezt_handle_type(bezt);
  ak->extreme_type = bezt_extreme_type(chain);

  /* count keyframes in this column */
  ak->totkey = 1;

  return ak;
}

/* Node updater callback used for building ActKeyColumns from BezTripleChain */
static void nupdate_ak_bezt(ActKeyColumn *ak, void *data)
{
  const BezTripleChain *chain = static_cast<const BezTripleChain *>(data);
  const BezTriple *bezt = chain->cur;

  /* set selection status and 'touched' status */
  if (BEZT_ISSEL_ANY(bezt)) {
    ak->sel = SELECT;
  }

  ak->totkey++;

  /* For keyframe type, 'proper' keyframes have priority over breakdowns
   * (and other types for now). */
  if (BEZKEYTYPE(bezt) == BEZT_KEYTYPE_KEYFRAME) {
    ak->key_type = BEZT_KEYTYPE_KEYFRAME;
  }

  /* For interpolation type, select the highest value (enum is sorted). */
  ak->handle_type = std::max((eKeyframeHandleDrawOpts)ak->handle_type, bezt_handle_type(bezt));

  /* For extremes, detect when combining different states. */
  const char new_extreme = bezt_extreme_type(chain);

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
/* New node callback used for building ActKeyColumns from GPencil frames */
static ActKeyColumn *nalloc_ak_cel(void *data)
{
  ActKeyColumn *ak = MEM_callocN<ActKeyColumn>("ActKeyColumnCel");
  GreasePencilCel &cel = *static_cast<GreasePencilCel *>(data);

  /* Store settings based on state of BezTriple */
  ak->cfra = cel.frame_number;
  ak->sel = (cel.frame.flag & SELECT) != 0;
  ak->key_type = eBezTriple_KeyframeType(cel.frame.type);

  /* Count keyframes in this column */
  ak->totkey = 1;
  /* Set as visible block. */
  ak->totblock = 1;
  ak->block.sel = ak->sel;
  ak->block.flag |= ACTKEYBLOCK_FLAG_GPENCIL;

  return ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames */
static void nupdate_ak_cel(ActKeyColumn *ak, void *data)
{
  GreasePencilCel &cel = *static_cast<GreasePencilCel *>(data);

  /* Update selection status. */
  if (cel.frame.flag & GP_FRAME_SELECTED) {
    ak->sel = SELECT;
  }

  ak->totkey++;

  /* Update keytype status. */
  if (cel.frame.type == BEZT_KEYTYPE_KEYFRAME) {
    ak->key_type = BEZT_KEYTYPE_KEYFRAME;
  }
}

/* ......... */

/* New node callback used for building ActKeyColumns from GPencil frames. */
static ActKeyColumn *nalloc_ak_gpframe(void *data)
{
  ActKeyColumn *ak = MEM_callocN<ActKeyColumn>("ActKeyColumnGPF");
  const bGPDframe *gpf = (bGPDframe *)data;

  /* store settings based on state of BezTriple */
  ak->cfra = gpf->framenum;
  ak->sel = (gpf->flag & GP_FRAME_SELECT) ? SELECT : 0;
  ak->key_type = eBezTriple_KeyframeType(gpf->key_type);

  /* Count keyframes in this column. */
  ak->totkey = 1;
  /* Set as visible block. */
  ak->totblock = 1;
  ak->block.sel = ak->sel;
  ak->block.flag |= ACTKEYBLOCK_FLAG_GPENCIL;

  return ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames. */
static void nupdate_ak_gpframe(ActKeyColumn *ak, void *data)
{
  bGPDframe *gpf = static_cast<bGPDframe *>(data);

  /* Set selection status and 'touched' status. */
  if (gpf->flag & GP_FRAME_SELECT) {
    ak->sel = SELECT;
  }

  ak->totkey++;

  /* For keyframe type, 'proper' keyframes have priority over breakdowns
   * (and other types for now). */
  if (gpf->key_type == BEZT_KEYTYPE_KEYFRAME) {
    ak->key_type = BEZT_KEYTYPE_KEYFRAME;
  }
}

/* ......... */

/* Extra struct to pass the timeline frame since the retiming key doesn't contain that and we would
 * need the scene to get it. */
struct SeqAllocateData {
  const SeqRetimingKey *key;
  float timeline_frame;
};

/* New node callback used for building ActKeyColumns from Sequencer keys. */
static ActKeyColumn *nalloc_ak_seqframe(void *data)
{
  ActKeyColumn *ak = MEM_callocN<ActKeyColumn>("ActKeyColumnGPF");
  const SeqAllocateData *allocate_data = (SeqAllocateData *)data;
  const SeqRetimingKey *timing_key = allocate_data->key;

  /* store settings based on state of BezTriple */
  ak->cfra = allocate_data->timeline_frame;
  ak->sel = (timing_key->flag & SEQ_KEY_SELECTED) ? SELECT : 0;
  ak->key_type = eBezTriple_KeyframeType::BEZT_KEYTYPE_KEYFRAME;

  /* Count keyframes in this column. */
  ak->totkey = 1;
  /* Set as visible block. */
  ak->totblock = 1;
  ak->block.sel = ak->sel;
  ak->block.flag = 0;

  return ak;
}

/* Node updater callback used for building ActKeyColumns from Sequencer keys. */
static void nupdate_ak_seqframe(ActKeyColumn *ak, void *data)
{
  SeqAllocateData *allocate_data = static_cast<SeqAllocateData *>(data);

  /* Set selection status and 'touched' status. */
  if (allocate_data->key->flag & SEQ_KEY_SELECTED) {
    ak->sel = SELECT;
  }

  ak->totkey++;
}

/* ......... */

/* New node callback used for building ActKeyColumns from GPencil frames */
static ActKeyColumn *nalloc_ak_masklayshape(void *data)
{
  ActKeyColumn *ak = MEM_callocN<ActKeyColumn>("ActKeyColumnGPF");
  const MaskLayerShape *masklay_shape = (const MaskLayerShape *)data;

  /* Store settings based on state of BezTriple. */
  ak->cfra = masklay_shape->frame;
  ak->sel = (masklay_shape->flag & MASK_SHAPE_SELECT) ? SELECT : 0;

  /* Count keyframes in this column. */
  ak->totkey = 1;

  return ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames */
static void nupdate_ak_masklayshape(ActKeyColumn *ak, void *data)
{
  MaskLayerShape *masklay_shape = static_cast<MaskLayerShape *>(data);

  /* Set selection status and 'touched' status. */
  if (masklay_shape->flag & MASK_SHAPE_SELECT) {
    ak->sel = SELECT;
  }

  ak->totkey++;
}

/* --------------- */
using KeylistCreateColumnFunction = std::function<ActKeyColumn *(void *userdata)>;
using KeylistUpdateColumnFunction = std::function<void(ActKeyColumn *, void *)>;

/* `keylist_find_neighbor_front_to_back` is called before the runtime can be initialized so we
 * cannot use bin searching. */
static ActKeyColumn *keylist_find_neighbor_front_to_back(ActKeyColumn *cursor, float cfra)
{
  while (cursor->next && cursor->next->cfra <= cfra) {
    cursor = cursor->next;
  }
  return cursor;
}

/* `keylist_find_neighbor_back_to_front` is called before the runtime can be initialized so we
 * cannot use bin searching. */
static ActKeyColumn *keylist_find_neighbor_back_to_front(ActKeyColumn *cursor, float cfra)
{
  while (cursor->prev && cursor->prev->cfra >= cfra) {
    cursor = cursor->prev;
  }
  return cursor;
}

/*
 * `keylist_find_exact_or_neighbor_column` is called before the runtime can be initialized so
 * we cannot use bin searching.
 *
 * This function is called to add or update columns in the keylist.
 * Typically columns are sorted by frame number so keeping track of the last_accessed_column
 * reduces searching.
 */
static ActKeyColumn *keylist_find_exact_or_neighbor_column(AnimKeylist *keylist, float cfra)
{
  BLI_assert(!keylist->is_runtime_initialized);
  if (ED_keylist_is_empty(keylist)) {
    return nullptr;
  }

  ActKeyColumn *cursor = keylist->last_accessed_column.value_or(
      static_cast<ActKeyColumn *>(keylist->key_columns.first));
  if (!is_cfra_eq(cursor->cfra, cfra)) {
    const bool walking_direction_front_to_back = cursor->cfra <= cfra;
    if (walking_direction_front_to_back) {
      cursor = keylist_find_neighbor_front_to_back(cursor, cfra);
    }
    else {
      cursor = keylist_find_neighbor_back_to_front(cursor, cfra);
    }
  }

  keylist->last_accessed_column = cursor;
  return cursor;
}

static void keylist_add_or_update_column(AnimKeylist *keylist,
                                         float cfra,
                                         KeylistCreateColumnFunction create_func,
                                         KeylistUpdateColumnFunction update_func,
                                         void *userdata)
{
  BLI_assert_msg(
      !keylist->is_runtime_initialized,
      "Modifying AnimKeylist isn't allowed after runtime is initialized "
      "keylist->key_columns/columns_len will get out of sync with runtime.key_columns.");
  if (ED_keylist_is_empty(keylist)) {
    ActKeyColumn *key_column = create_func(userdata);
    BLI_addhead(&keylist->key_columns, key_column);
    keylist->column_len += 1;
    keylist->last_accessed_column = key_column;
    return;
  }

  ActKeyColumn *nearest = keylist_find_exact_or_neighbor_column(keylist, cfra);
  if (is_cfra_eq(nearest->cfra, cfra)) {
    update_func(nearest, userdata);
  }
  else if (is_cfra_lt(nearest->cfra, cfra)) {
    ActKeyColumn *key_column = create_func(userdata);
    BLI_insertlinkafter(&keylist->key_columns, nearest, key_column);
    keylist->column_len += 1;
    keylist->last_accessed_column = key_column;
  }
  else {
    ActKeyColumn *key_column = create_func(userdata);
    BLI_insertlinkbefore(&keylist->key_columns, nearest, key_column);
    keylist->column_len += 1;
    keylist->last_accessed_column = key_column;
  }
}

/* Add the given BezTriple to the given 'list' of Keyframes */
static void add_bezt_to_keycolumns_list(AnimKeylist *keylist, BezTripleChain *bezt)
{
  if (ELEM(nullptr, keylist, bezt)) {
    return;
  }

  float cfra = bezt->cur->vec[1][0];
  keylist_add_or_update_column(keylist, cfra, nalloc_ak_bezt, nupdate_ak_bezt, bezt);
}

/* Add the given GPencil Frame to the given 'list' of Keyframes */
static void add_gpframe_to_keycolumns_list(AnimKeylist *keylist, bGPDframe *gpf)
{
  if (ELEM(nullptr, keylist, gpf)) {
    return;
  }

  float cfra = gpf->framenum;
  keylist_add_or_update_column(keylist, cfra, nalloc_ak_gpframe, nupdate_ak_gpframe, gpf);
}

/* Add the given MaskLayerShape Frame to the given 'list' of Keyframes */
static void add_masklay_to_keycolumns_list(AnimKeylist *keylist, MaskLayerShape *masklay_shape)
{
  if (ELEM(nullptr, keylist, masklay_shape)) {
    return;
  }

  float cfra = masklay_shape->frame;
  keylist_add_or_update_column(
      keylist, cfra, nalloc_ak_masklayshape, nupdate_ak_masklayshape, masklay_shape);
}

/* ActKeyBlocks (Long Keyframes) ------------------------------------------ */

static const ActKeyBlockInfo dummy_keyblock = {0};

static void compute_keyblock_data(ActKeyBlockInfo *info,
                                  const BezTriple *prev,
                                  const BezTriple *beztn)
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
    /* This interpolation type induces movement even between identical columns. */
    else {
      hold = !ELEM(prev->ipo, BEZT_IPO_ELASTIC);
    }

    if (hold) {
      info->flag |= ACTKEYBLOCK_FLAG_STATIC_HOLD | ACTKEYBLOCK_FLAG_ANY_HOLD;
    }
  }

  /* Remember non-bezier interpolation info. */
  switch (eBezTriple_Interpolation(prev->ipo)) {
    case BEZT_IPO_BEZ:
      break;
    case BEZT_IPO_LIN:
      info->flag |= ACTKEYBLOCK_FLAG_IPO_LINEAR;
      break;
    case BEZT_IPO_CONST:
      info->flag |= ACTKEYBLOCK_FLAG_IPO_CONSTANT;
      break;
    default:
      /* For automatic bezier interpolations, such as easings (cubic, circular, etc), and dynamic
       * (back, bounce, elastic). */
      info->flag |= ACTKEYBLOCK_FLAG_IPO_OTHER;
      break;
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

static void add_bezt_to_keyblocks_list(AnimKeylist *keylist, BezTriple *bezt, const int bezt_len)
{
  ActKeyColumn *col = static_cast<ActKeyColumn *>(keylist->key_columns.first);

  if (bezt && bezt_len >= 2) {
    ActKeyBlockInfo block;

    /* Find the first key column while inserting dummy blocks. */
    for (; col != nullptr && is_cfra_lt(col->cfra, bezt[0].vec[1][0]); col = col->next) {
      add_keyblock_info(col, &dummy_keyblock);
    }

    BLI_assert(col != nullptr);

    /* Insert real blocks. */
    for (int v = 1; col != nullptr && v < bezt_len; v++, bezt++) {
      /* Wrong order of bezier keys: resync position. */
      if (is_cfra_lt(bezt[1].vec[1][0], bezt[0].vec[1][0])) {
        /* Backtrack to find the right location. */
        if (is_cfra_lt(bezt[1].vec[1][0], col->cfra)) {
          ActKeyColumn *newcol = keylist_find_exact_or_neighbor_column(keylist, col->cfra);

          BLI_assert(newcol);
          BLI_assert(newcol->cfra == col->cfra);

          col = newcol;
          /* The previous keyblock is garbage too. */
          if (col->prev != nullptr) {
            add_keyblock_info(col->prev, &dummy_keyblock);
          }
        }

        continue;
      }

      /* In normal situations all keyframes are sorted. However, while keys are transformed, they
       * may change order and then this assertion no longer holds. The effect is that the drawing
       * isn't perfect during the transform; the "constant value" bars aren't updated until the
       * transformation is confirmed. */
      // BLI_assert(is_cfra_eq(col->cfra, bezt[0].vec[1][0]));

      compute_keyblock_data(&block, bezt, bezt + 1);

      for (; col != nullptr && is_cfra_lt(col->cfra, bezt[1].vec[1][0]); col = col->next) {
        add_keyblock_info(col, &block);
      }

      BLI_assert(col != nullptr);
    }
  }

  /* Insert dummy blocks at the end. */
  for (; col != nullptr; col = col->next) {
    add_keyblock_info(col, &dummy_keyblock);
  }
}

/* Walk through columns and propagate blocks and totcurve.
 *
 * This must be called even by animation sources that don't generate
 * keyblocks to keep the data structure consistent after adding columns.
 */
static void update_keyblocks(AnimKeylist *keylist, BezTriple *bezt, const int bezt_len)
{
  /* Find the curve count. */
  int max_curve = 0;

  LISTBASE_FOREACH (ActKeyColumn *, col, &keylist->key_columns) {
    max_curve = std::max(max_curve, int(col->totcurve));
  }

  /* Propagate blocks to inserted keys. */
  ActKeyColumn *prev_ready = nullptr;

  LISTBASE_FOREACH (ActKeyColumn *, col, &keylist->key_columns) {
    /* Pre-existing column. */
    if (col->totcurve > 0) {
      prev_ready = col;
    }
    /* Newly inserted column, so copy block data from previous. */
    else if (prev_ready != nullptr) {
      col->totblock = prev_ready->totblock;
      memcpy(&col->block, &prev_ready->block, sizeof(ActKeyBlockInfo));
    }

    col->totcurve = max_curve + 1;
  }

  /* Add blocks on top. */
  add_bezt_to_keyblocks_list(keylist, bezt, bezt_len);
}

/* --------- */

bool actkeyblock_is_valid(const ActKeyColumn *ac)
{
  return ac != nullptr && ac->next != nullptr && ac->totblock > 0;
}

int actkeyblock_get_valid_hold(const ActKeyColumn *ac)
{
  if (!actkeyblock_is_valid(ac)) {
    return 0;
  }

  const int hold_mask = (ACTKEYBLOCK_FLAG_ANY_HOLD | ACTKEYBLOCK_FLAG_STATIC_HOLD);
  return (ac->block.flag & ~ac->block.conflict) & hold_mask;
}

/* *************************** Keyframe List Conversions *************************** */

void summary_to_keylist(bAnimContext *ac,
                        AnimKeylist *keylist,
                        const int saction_flag,
                        blender::float2 range)
{
  if (!ac) {
    return;
  }

  ListBase anim_data = {nullptr, nullptr};

  /* Get F-Curves to take keyframes from. */
  const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE;
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* Loop through each F-Curve, grabbing the keyframes. */
  LISTBASE_FOREACH (const bAnimListElem *, ale, &anim_data) {
    /* Why not use all #eAnim_KeyType here?
     * All of the other key types are actually "summaries" themselves,
     * and will just end up duplicating stuff that comes up through
     * standard filtering of just F-Curves. Given the way that these work,
     * there isn't really any benefit at all from including them. - Aligorith */
    switch (ale->datatype) {
      case ALE_FCURVE:
        fcurve_to_keylist(ale->adt,
                          static_cast<FCurve *>(ale->data),
                          keylist,
                          saction_flag,
                          range,
                          ANIM_nla_mapping_allowed(ale));
        break;
      case ALE_MASKLAY:
        mask_to_keylist(ac->ads, static_cast<MaskLayer *>(ale->data), keylist);
        break;
      case ALE_GPFRAME:
        gpl_to_keylist(ac->ads, static_cast<bGPDlayer *>(ale->data), keylist);
        break;
      case ALE_GREASE_PENCIL_CEL:
        grease_pencil_cels_to_keylist(
            ale->adt, static_cast<const GreasePencilLayer *>(ale->data), keylist, saction_flag);
        break;
      default:
        break;
    }
  }

  ANIM_animdata_freelist(&anim_data);
}

void action_slot_summary_to_keylist(bAnimContext *ac,
                                    ID *animated_id,
                                    animrig::Action &action,
                                    const animrig::slot_handle_t slot_handle,
                                    AnimKeylist *keylist,
                                    const int /* eSAction_Flag */ saction_flag,
                                    blender::float2 range)
{
  /* TODO: downstream code depends on this being non-null (see e.g.
   * `ANIM_animfilter_action_slot()` and `animfilter_fcurves_span()`). Either
   * change this parameter to be a reference, or modify the downstream code to
   * not assume that it's non-null and do something reasonable when it is null. */
  BLI_assert(animated_id);

  if (!ac) {
    return;
  }

  animrig::Slot *slot = action.slot_for_handle(slot_handle);
  if (!slot) {
    /* In the Dope Sheet mode of the Dope Sheet, an _Action_ channel actually shows the _Slot_ keys
     * in its summary line. When there are animated NLA control curves, that Action channel is also
     * shown, even when there is no slot assigned. So this function needs to be able to handle the
     * "no slot" case as valid. It just doesn't produce any keys.
     *
     * Also see `build_channel_keylist()` in `keyframes_draw.cc`. */
    return;
  }

  ListBase anim_data = {nullptr, nullptr};

  /* Get F-Curves to take keyframes from. */
  const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE;
  ANIM_animfilter_action_slot(ac, &anim_data, action, *slot, filter, animated_id);

  LISTBASE_FOREACH (const bAnimListElem *, ale, &anim_data) {
    /* As of the writing of this code, Actions ultimately only contain FCurves.
     * If/when that changes in the future, this may need to be updated. */
    if (ale->datatype != ALE_FCURVE) {
      continue;
    }
    fcurve_to_keylist(ale->adt,
                      static_cast<FCurve *>(ale->data),
                      keylist,
                      saction_flag,
                      range,
                      ANIM_nla_mapping_allowed(ale));
  }

  ANIM_animdata_freelist(&anim_data);
}

void scene_to_keylist(bDopeSheet *ads,
                      Scene *sce,
                      AnimKeylist *keylist,
                      const int saction_flag,
                      blender::float2 range)
{
  bAnimContext ac = {nullptr};
  ListBase anim_data = {nullptr, nullptr};

  bAnimListElem dummy_chan = {nullptr};

  if (sce == nullptr) {
    return;
  }

  /* Create a dummy wrapper data to work with. */
  dummy_chan.type = ANIMTYPE_SCENE;
  dummy_chan.data = sce;
  dummy_chan.id = &sce->id;
  dummy_chan.adt = sce->adt;

  ac.ads = ads;
  ac.data = &dummy_chan;
  ac.datatype = ANIMCONT_CHANNEL;
  ac.filters.flag = eDopeSheet_FilterFlag(ads->filterflag);
  ac.filters.flag2 = eDopeSheet_FilterFlag2(ads->filterflag2);

  /* Get F-Curves to take keyframes from. */
  const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY;

  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* Loop through each F-Curve, grabbing the keyframes. */
  LISTBASE_FOREACH (const bAnimListElem *, ale, &anim_data) {
    fcurve_to_keylist(ale->adt,
                      static_cast<FCurve *>(ale->data),
                      keylist,
                      saction_flag,
                      range,
                      ANIM_nla_mapping_allowed(ale));
  }

  ANIM_animdata_freelist(&anim_data);
}

void ob_to_keylist(bDopeSheet *ads,
                   Object *ob,
                   AnimKeylist *keylist,
                   const int saction_flag,
                   blender::float2 range)
{
  bAnimContext ac = {nullptr};
  ListBase anim_data = {nullptr, nullptr};

  bAnimListElem dummy_chan = {nullptr};
  Base dummy_base = {nullptr};

  if (ob == nullptr) {
    return;
  }

  /* Create a dummy wrapper data to work with. */
  dummy_base.object = ob;

  dummy_chan.type = ANIMTYPE_OBJECT;
  dummy_chan.data = &dummy_base;
  dummy_chan.id = &ob->id;
  dummy_chan.adt = ob->adt;

  ac.ads = ads;
  ac.data = &dummy_chan;
  ac.datatype = ANIMCONT_CHANNEL;
  ac.filters.flag = eDopeSheet_FilterFlag(ads->filterflag);
  ac.filters.flag2 = eDopeSheet_FilterFlag2(ads->filterflag2);

  /* Get F-Curves to take keyframes from. */
  const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY;
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* Loop through each F-Curve, grabbing the keyframes. */
  LISTBASE_FOREACH (const bAnimListElem *, ale, &anim_data) {
    fcurve_to_keylist(ale->adt,
                      static_cast<FCurve *>(ale->data),
                      keylist,
                      saction_flag,
                      range,
                      ANIM_nla_mapping_allowed(ale));
  }

  ANIM_animdata_freelist(&anim_data);
}

void cachefile_to_keylist(bDopeSheet *ads,
                          CacheFile *cache_file,
                          AnimKeylist *keylist,
                          const int saction_flag)
{
  if (cache_file == nullptr) {
    return;
  }

  /* Create a dummy wrapper data to work with. */
  bAnimListElem dummy_chan = {nullptr};
  dummy_chan.type = ANIMTYPE_DSCACHEFILE;
  dummy_chan.data = cache_file;
  dummy_chan.id = &cache_file->id;
  dummy_chan.adt = cache_file->adt;

  bAnimContext ac = {nullptr};
  ac.ads = ads;
  ac.data = &dummy_chan;
  ac.datatype = ANIMCONT_CHANNEL;
  ac.filters.flag = eDopeSheet_FilterFlag(ads->filterflag);
  ac.filters.flag2 = eDopeSheet_FilterFlag2(ads->filterflag2);

  /* Get F-Curves to take keyframes from. */
  ListBase anim_data = {nullptr, nullptr};
  const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY;
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* Loop through each F-Curve, grabbing the keyframes. */
  LISTBASE_FOREACH (const bAnimListElem *, ale, &anim_data) {
    fcurve_to_keylist(ale->adt,
                      static_cast<FCurve *>(ale->data),
                      keylist,
                      saction_flag,
                      {-FLT_MAX, FLT_MAX},
                      ANIM_nla_mapping_allowed(ale));
  }

  ANIM_animdata_freelist(&anim_data);
}

static inline void set_up_beztriple_chain(BezTripleChain &chain,
                                          const FCurve *fcu,
                                          const int key_index,
                                          const bool do_extremes,
                                          const bool is_cyclic)
{
  chain.cur = &fcu->bezt[key_index];

  /* Neighbor columns, accounting for being cyclic. */
  if (do_extremes) {
    chain.prev = (key_index > 0) ? &fcu->bezt[key_index - 1] :
                 is_cyclic       ? &fcu->bezt[fcu->totvert - 2] :
                                   nullptr;
    chain.next = (key_index + 1 < fcu->totvert) ? &fcu->bezt[key_index + 1] :
                 is_cyclic                      ? &fcu->bezt[1] :
                                                  nullptr;
  }
}

void fcurve_to_keylist(AnimData *adt,
                       FCurve *fcu,
                       AnimKeylist *keylist,
                       const int saction_flag,
                       blender::float2 range,
                       const bool use_nla_remapping)
{
  /* This is not strictly necessary because `ANIM_nla_mapping_apply_fcurve()`
   * will just not do remapping if `adt` is null. Nevertheless, saying that you
   * want NLA remapping to be performed while not passing an `adt` (needed for
   * NLA remapping) almost certainly indicates a mistake somewhere. */
  BLI_assert_msg(!(use_nla_remapping && adt == nullptr),
                 "Cannot perform NLA time remapping without an adt.");

  if (!fcu || fcu->totvert == 0 || !fcu->bezt) {
    return;
  }
  keylist_reset_last_accessed(keylist);

  if (use_nla_remapping) {
    ANIM_nla_mapping_apply_fcurve(adt, fcu, false, false);
  }

  const bool is_cyclic = BKE_fcurve_is_cyclic(fcu) && (fcu->totvert >= 2);
  const bool do_extremes = (saction_flag & SACTION_SHOW_EXTREMES) != 0;

  BezTripleChain chain = {nullptr};
  /* The indices for which keys have been added to the key columns. Initialized as invalid bounds
   * for the case that no keyframes get added to the key-columns, which happens when the given
   * range doesn't overlap with the existing keyframes. */
  blender::Bounds<int> index_bounds(int(fcu->totvert), 0);
  /* The following is used to find the keys that are JUST outside the range. This is done so
   * drawing in the dope sheet can create lines that extend off-screen. */
  float left_outside_key_x = -FLT_MAX;
  float right_outside_key_x = FLT_MAX;
  int left_outside_key_index = -1;
  int right_outside_key_index = -1;
  /* Loop through beztriples, making ActKeysColumns. */
  for (int v = 0; v < fcu->totvert; v++) {
    /* Not using binary search to limit the range because the FCurve might not be sorted e.g. when
     * transforming in the Dope Sheet. */
    const float x = fcu->bezt[v].vec[1][0];
    if (x < range[0] && x > left_outside_key_x) {
      left_outside_key_x = x;
      left_outside_key_index = v;
    }
    if (x > range[1] && x < right_outside_key_x) {
      right_outside_key_x = x;
      right_outside_key_index = v;
    }
    if (x < range[0] || x > range[1]) {
      continue;
    }
    blender::math::min_max(v, index_bounds.min, index_bounds.max);
    set_up_beztriple_chain(chain, fcu, v, do_extremes, is_cyclic);

    add_bezt_to_keycolumns_list(keylist, &chain);
  }

  if (left_outside_key_index >= 0) {
    set_up_beztriple_chain(chain, fcu, left_outside_key_index, do_extremes, is_cyclic);
    add_bezt_to_keycolumns_list(keylist, &chain);
    /* Checking min and max because the FCurve might not be sorted. */
    index_bounds.min = blender::math::min(index_bounds.min, left_outside_key_index);
    index_bounds.max = blender::math::max(index_bounds.max, left_outside_key_index);
  }
  if (right_outside_key_index >= 0) {
    set_up_beztriple_chain(chain, fcu, right_outside_key_index, do_extremes, is_cyclic);
    add_bezt_to_keycolumns_list(keylist, &chain);
    index_bounds.min = blender::math::min(index_bounds.min, right_outside_key_index);
    index_bounds.max = blender::math::max(index_bounds.max, right_outside_key_index);
  }
  /* Not using index_bounds.is_empty() because that returns true if min and max are the same. That
   * is a valid configuration in this case though. */
  if (index_bounds.min <= index_bounds.max) {
    update_keyblocks(
        keylist, &fcu->bezt[index_bounds.min], (index_bounds.max + 1) - index_bounds.min);
  }

  if (use_nla_remapping) {
    ANIM_nla_mapping_apply_fcurve(adt, fcu, true, false);
  }
}

void action_group_to_keylist(AnimData *adt,
                             bActionGroup *agrp,
                             AnimKeylist *keylist,
                             const int saction_flag,
                             blender::float2 range)
{
  if (!agrp) {
    return;
  }

  /* Legacy actions. */
  if (agrp->wrap().is_legacy()) {
    LISTBASE_FOREACH (FCurve *, fcu, &agrp->channels) {
      if (fcu->grp != agrp) {
        break;
      }
      fcurve_to_keylist(adt, fcu, keylist, saction_flag, range, true);
    }
    return;
  }

  /* Layered actions. */
  animrig::Channelbag &channelbag = agrp->channelbag->wrap();
  Span<FCurve *> fcurves = channelbag.fcurves().slice(agrp->fcurve_range_start,
                                                      agrp->fcurve_range_length);
  for (FCurve *fcurve : fcurves) {
    fcurve_to_keylist(adt, fcurve, keylist, saction_flag, range, true);
  }
}

void action_to_keylist(AnimData *adt,
                       bAction *dna_action,
                       AnimKeylist *keylist,
                       const int saction_flag,
                       blender::float2 range)
{
  if (!dna_action) {
    return;
  }

  blender::animrig::Action &action = dna_action->wrap();

  /* TODO: move this into fcurves_for_action_slot(). */
  if (action.is_action_legacy()) {
    LISTBASE_FOREACH (FCurve *, fcu, &action.curves) {
      fcurve_to_keylist(adt, fcu, keylist, saction_flag, range, true);
    }
    return;
  }

  /**
   * Assumption: the animation is bound to adt->slot_handle. This assumption will break when we
   * have things like reference strips, where the strip can reference another slot handle.
   */
  BLI_assert(adt);
  for (FCurve *fcurve : fcurves_for_action_slot(action, adt->slot_handle)) {
    fcurve_to_keylist(adt, fcurve, keylist, saction_flag, range, true);
  }
}

void gpencil_to_keylist(bDopeSheet *ads, bGPdata *gpd, AnimKeylist *keylist, const bool active)
{
  if (!gpd || !keylist) {
    return;
  }

  /* For now, just aggregate out all the frames, but only for visible layers. */
  LISTBASE_FOREACH_BACKWARD (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }
    if ((!active) || ((active) && (gpl->flag & GP_LAYER_SELECT))) {
      gpl_to_keylist(ads, gpl, keylist);
    }
  }
}

void grease_pencil_data_block_to_keylist(AnimData *adt,
                                         const GreasePencil *grease_pencil,
                                         AnimKeylist *keylist,
                                         const int saction_flag,
                                         const bool active_layer_only)
{
  if ((grease_pencil == nullptr) || (keylist == nullptr)) {
    return;
  }

  if (active_layer_only && grease_pencil->has_active_layer()) {
    grease_pencil_cels_to_keylist(adt, grease_pencil->get_active_layer(), keylist, saction_flag);
    return;
  }

  for (const blender::bke::greasepencil::Layer *layer : grease_pencil->layers()) {
    grease_pencil_cels_to_keylist(adt, layer, keylist, saction_flag);
  }
}

void grease_pencil_cels_to_keylist(AnimData * /*adt*/,
                                   const GreasePencilLayer *gpl,
                                   AnimKeylist *keylist,
                                   int /*saction_flag*/)
{
  using namespace blender::bke::greasepencil;
  const Layer &layer = gpl->wrap();
  for (auto item : layer.frames().items()) {
    GreasePencilCel cel{};
    cel.frame_number = item.key;
    cel.frame = item.value;

    float cfra = float(item.key);
    keylist_add_or_update_column(
        keylist, cfra, nalloc_ak_cel, nupdate_ak_cel, static_cast<void *>(&cel));
  }
}

void grease_pencil_layer_group_to_keylist(AnimData *adt,
                                          const GreasePencilLayerTreeGroup *layer_group,
                                          AnimKeylist *keylist,
                                          const int saction_flag)
{
  if ((layer_group == nullptr) || (keylist == nullptr)) {
    return;
  }

  LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, node_, &layer_group->children) {
    const blender::bke::greasepencil::TreeNode &node = node_->wrap();
    if (node.is_group()) {
      grease_pencil_layer_group_to_keylist(adt, &node.as_group(), keylist, saction_flag);
    }
    else if (node.is_layer()) {
      grease_pencil_cels_to_keylist(adt, &node.as_layer(), keylist, saction_flag);
    }
  }
}

void gpl_to_keylist(bDopeSheet * /*ads*/, bGPDlayer *gpl, AnimKeylist *keylist)
{
  if (!gpl || !keylist) {
    return;
  }

  keylist_reset_last_accessed(keylist);
  /* Although the frames should already be in an ordered list,
   * they are not suitable for displaying yet. */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    add_gpframe_to_keycolumns_list(keylist, gpf);
  }

  update_keyblocks(keylist, nullptr, 0);
}

void mask_to_keylist(bDopeSheet * /*ads*/, MaskLayer *masklay, AnimKeylist *keylist)
{
  if (!masklay || !keylist) {
    return;
  }
  keylist_reset_last_accessed(keylist);
  LISTBASE_FOREACH (MaskLayerShape *, masklay_shape, &masklay->splines_shapes) {
    add_masklay_to_keycolumns_list(keylist, masklay_shape);
  }

  update_keyblocks(keylist, nullptr, 0);
}

void sequencer_strip_to_keylist(const Strip &strip, AnimKeylist &keylist, Scene &scene)
{
  if (!blender::seq::retiming_is_active(&strip)) {
    return;
  }
  keylist_reset_last_accessed(&keylist);
  for (const SeqRetimingKey &retime_key : blender::seq::retiming_keys_get(&strip)) {
    const float cfra = blender::seq::retiming_key_timeline_frame_get(&scene, &strip, &retime_key);
    SeqAllocateData allocate_data = {&retime_key, cfra};
    keylist_add_or_update_column(
        &keylist, cfra, nalloc_ak_seqframe, nupdate_ak_seqframe, &allocate_data);
  }
  update_keyblocks(&keylist, nullptr, 0);
}
