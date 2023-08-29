/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_text_types.h"

#include "BLI_blenlib.h"
#include "BLI_easing.h"
#include "BLI_ghash.h"
#include "BLI_math_vector.h"
#include "BLI_sort_utils.h"
#include "BLI_string_utils.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_lib_query.h"
#include "BKE_nla.h"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"

#include "CLG_log.h"

#define SMALL -1.0e-10
#define SELECT 1

static CLG_LogRef LOG = {"bke.fcurve"};

/* -------------------------------------------------------------------- */
/** \name F-Curve Data Create
 * \{ */

FCurve *BKE_fcurve_create()
{
  FCurve *fcu = static_cast<FCurve *>(MEM_callocN(sizeof(FCurve), __func__));
  return fcu;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name F-Curve Data Free
 * \{ */

void BKE_fcurve_free(FCurve *fcu)
{
  if (fcu == nullptr) {
    return;
  }

  /* Free curve data. */
  MEM_SAFE_FREE(fcu->bezt);
  MEM_SAFE_FREE(fcu->fpt);

  /* Free RNA-path, as this were allocated when getting the path string. */
  MEM_SAFE_FREE(fcu->rna_path);

  /* Free extra data - i.e. modifiers, and driver. */
  fcurve_free_driver(fcu);
  free_fmodifiers(&fcu->modifiers);

  /* Free the f-curve itself. */
  MEM_freeN(fcu);
}

void BKE_fcurves_free(ListBase *list)
{
  /* Sanity check. */
  if (list == nullptr) {
    return;
  }

  /* Free data - no need to call remlink before freeing each curve,
   * as we store reference to next, and freeing only touches the curve
   * it's given.
   */
  FCurve *fcn = nullptr;
  for (FCurve *fcu = static_cast<FCurve *>(list->first); fcu; fcu = fcn) {
    fcn = fcu->next;
    BKE_fcurve_free(fcu);
  }

  /* Clear pointers just in case. */
  BLI_listbase_clear(list);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name F-Curve Data Copy
 * \{ */

FCurve *BKE_fcurve_copy(const FCurve *fcu)
{
  /* Sanity check. */
  if (fcu == nullptr) {
    return nullptr;
  }

  /* Make a copy. */
  FCurve *fcu_d = static_cast<FCurve *>(MEM_dupallocN(fcu));

  fcu_d->next = fcu_d->prev = nullptr;
  fcu_d->grp = nullptr;

  /* Copy curve data. */
  fcu_d->bezt = static_cast<BezTriple *>(MEM_dupallocN(fcu_d->bezt));
  fcu_d->fpt = static_cast<FPoint *>(MEM_dupallocN(fcu_d->fpt));

  /* Copy rna-path. */
  fcu_d->rna_path = static_cast<char *>(MEM_dupallocN(fcu_d->rna_path));

  /* Copy driver. */
  fcu_d->driver = fcurve_copy_driver(fcu_d->driver);

  /* Copy modifiers. */
  copy_fmodifiers(&fcu_d->modifiers, &fcu->modifiers);

  /* Return new data. */
  return fcu_d;
}

void BKE_fcurves_copy(ListBase *dst, ListBase *src)
{
  /* Sanity checks. */
  if (ELEM(nullptr, dst, src)) {
    return;
  }

  /* Clear destination list first. */
  BLI_listbase_clear(dst);

  /* Copy one-by-one. */
  LISTBASE_FOREACH (FCurve *, sfcu, src) {
    FCurve *dfcu = BKE_fcurve_copy(sfcu);
    BLI_addtail(dst, dfcu);
  }
}

void BKE_fmodifier_name_set(FModifier *fcm, const char *name)
{
  /* Copy new Modifier name. */
  STRNCPY(fcm->name, name);

  /* Set default modifier name when name parameter is an empty string.
   * Ensure the name is unique. */
  const FModifierTypeInfo *fmi = get_fmodifier_typeinfo(fcm->type);
  ListBase list = BLI_listbase_from_link((Link *)fcm);
  BLI_uniquename(&list, fcm, fmi->name, '.', offsetof(FModifier, name), sizeof(fcm->name));
}

void BKE_fmodifiers_foreach_id(ListBase *fmodifiers, LibraryForeachIDData *data)
{
  LISTBASE_FOREACH (FModifier *, fcm, fmodifiers) {
    /* library data for specific F-Modifier types */
    switch (fcm->type) {
      case FMODIFIER_TYPE_PYTHON: {
        FMod_Python *fcm_py = (FMod_Python *)fcm->data;
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, fcm_py->script, IDWALK_CB_NOP);

        BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
            data,
            IDP_foreach_property(fcm_py->prop,
                                 IDP_TYPE_FILTER_ID,
                                 BKE_lib_query_idpropertiesForeachIDLink_callback,
                                 data));
        break;
      }
      default:
        break;
    }
  }
}

void BKE_fcurve_foreach_id(FCurve *fcu, LibraryForeachIDData *data)
{
  ChannelDriver *driver = fcu->driver;

  if (driver != nullptr) {
    LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
      /* only used targets */
      DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
        BKE_LIB_FOREACHID_PROCESS_ID(data, dtar->id, IDWALK_CB_NOP);
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }

  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, BKE_fmodifiers_foreach_id(&fcu->modifiers, data));
}

/* ----------------- Finding F-Curves -------------------------- */

FCurve *id_data_find_fcurve(
    ID *id, void *data, StructRNA *type, const char *prop_name, int index, bool *r_driven)
{
  /* Anim vars */
  AnimData *adt = BKE_animdata_from_id(id);

  /* Rna vars */
  PointerRNA ptr;
  PropertyRNA *prop;

  if (r_driven) {
    *r_driven = false;
  }

  /* Only use the current action ??? */
  if (ELEM(nullptr, adt, adt->action)) {
    return nullptr;
  }

  RNA_pointer_create(id, type, data, &ptr);
  prop = RNA_struct_find_property(&ptr, prop_name);
  if (prop == nullptr) {
    return nullptr;
  }

  char *path = RNA_path_from_ID_to_property(&ptr, prop);
  if (path == nullptr) {
    return nullptr;
  }

  /* FIXME: The way drivers are handled here (always nullptr-ifying `fcu`) is very weird, this
   * needs to be re-checked I think?. */
  bool is_driven = false;
  FCurve *fcu = BKE_animadata_fcurve_find_by_rna_path(adt, path, index, nullptr, &is_driven);
  if (is_driven) {
    if (r_driven != nullptr) {
      *r_driven = is_driven;
    }
    fcu = nullptr;
  }

  MEM_freeN(path);

  return fcu;
}

FCurve *BKE_fcurve_find(ListBase *list, const char rna_path[], const int array_index)
{
  /* Sanity checks. */
  if (ELEM(nullptr, list, rna_path) || array_index < 0) {
    return nullptr;
  }

  /* Check paths of curves, then array indices... */
  LISTBASE_FOREACH (FCurve *, fcu, list) {
    /* Check indices first, much cheaper than a string comparison. */
    /* Simple string-compare (this assumes that they have the same root...) */
    if (UNLIKELY(fcu->array_index == array_index && fcu->rna_path &&
                 fcu->rna_path[0] == rna_path[0] && STREQ(fcu->rna_path, rna_path)))
    {
      return fcu;
    }
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FCurve Iteration
 * \{ */

FCurve *BKE_fcurve_iter_step(FCurve *fcu_iter, const char rna_path[])
{
  /* Sanity checks. */
  if (ELEM(nullptr, fcu_iter, rna_path)) {
    return nullptr;
  }

  /* Check paths of curves, then array indices... */
  for (FCurve *fcu = fcu_iter; fcu; fcu = fcu->next) {
    /* Simple string-compare (this assumes that they have the same root...) */
    if (fcu->rna_path && STREQ(fcu->rna_path, rna_path)) {
      return fcu;
    }
  }

  return nullptr;
}

int BKE_fcurves_filter(ListBase *dst, ListBase *src, const char *dataPrefix, const char *dataName)
{
  int matches = 0;

  /* Sanity checks. */
  if (ELEM(nullptr, dst, src, dataPrefix, dataName)) {
    return 0;
  }
  if ((dataPrefix[0] == 0) || (dataName[0] == 0)) {
    return 0;
  }

  const size_t quotedName_size = strlen(dataName) + 1;
  char *quotedName = static_cast<char *>(alloca(quotedName_size));

  /* Search each F-Curve one by one. */
  LISTBASE_FOREACH (FCurve *, fcu, src) {
    /* Check if quoted string matches the path. */
    if (fcu->rna_path == nullptr) {
      continue;
    }
    /* Skipping names longer than `quotedName_size` is OK since we're after an exact match. */
    if (!BLI_str_quoted_substr(fcu->rna_path, dataPrefix, quotedName, quotedName_size)) {
      continue;
    }
    if (!STREQ(quotedName, dataName)) {
      continue;
    }

    /* Check if the quoted name matches the required name. */
    LinkData *ld = static_cast<LinkData *>(MEM_callocN(sizeof(LinkData), __func__));

    ld->data = fcu;
    BLI_addtail(dst, ld);

    matches++;
  }
  /* Return the number of matches. */
  return matches;
}

FCurve *BKE_animadata_fcurve_find_by_rna_path(
    AnimData *animdata, const char *rna_path, int rna_index, bAction **r_action, bool *r_driven)
{
  if (r_driven != nullptr) {
    *r_driven = false;
  }
  if (r_action != nullptr) {
    *r_action = nullptr;
  }

  const bool has_action_fcurves = animdata->action != nullptr &&
                                  !BLI_listbase_is_empty(&animdata->action->curves);
  const bool has_drivers = !BLI_listbase_is_empty(&animdata->drivers);

  /* Animation takes priority over drivers. */
  if (has_action_fcurves) {
    FCurve *fcu = BKE_fcurve_find(&animdata->action->curves, rna_path, rna_index);

    if (fcu != nullptr) {
      if (r_action != nullptr) {
        *r_action = animdata->action;
      }
      return fcu;
    }
  }

  /* If not animated, check if driven. */
  if (has_drivers) {
    FCurve *fcu = BKE_fcurve_find(&animdata->drivers, rna_path, rna_index);

    if (fcu != nullptr) {
      if (r_driven != nullptr) {
        *r_driven = true;
      }
      return fcu;
    }
  }

  return nullptr;
}

FCurve *BKE_fcurve_find_by_rna(PointerRNA *ptr,
                               PropertyRNA *prop,
                               int rnaindex,
                               AnimData **r_adt,
                               bAction **r_action,
                               bool *r_driven,
                               bool *r_special)
{
  return BKE_fcurve_find_by_rna_context_ui(
      nullptr, ptr, prop, rnaindex, r_adt, r_action, r_driven, r_special);
}

FCurve *BKE_fcurve_find_by_rna_context_ui(bContext * /*C*/,
                                          const PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          int rnaindex,
                                          AnimData **r_animdata,
                                          bAction **r_action,
                                          bool *r_driven,
                                          bool *r_special)
{
  if (r_animdata != nullptr) {
    *r_animdata = nullptr;
  }
  if (r_action != nullptr) {
    *r_action = nullptr;
  }
  if (r_driven != nullptr) {
    *r_driven = false;
  }
  if (r_special) {
    *r_special = false;
  }

  /* Special case for NLA Control Curves... */
  if (BKE_nlastrip_has_curves_for_property(ptr, prop)) {
    NlaStrip *strip = static_cast<NlaStrip *>(ptr->data);

    /* Set the special flag, since it cannot be a normal action/driver
     * if we've been told to start looking here...
     */
    if (r_special) {
      *r_special = true;
    }

    *r_driven = false;
    if (r_animdata) {
      *r_animdata = nullptr;
    }
    if (r_action) {
      *r_action = nullptr;
    }

    /* The F-Curve either exists or it doesn't here... */
    return BKE_fcurve_find(&strip->fcurves, RNA_property_identifier(prop), rnaindex);
  }

  /* There must be some RNA-pointer + property combo. */
  if (!prop || !ptr->owner_id || !RNA_property_animateable(ptr, prop)) {
    return nullptr;
  }

  AnimData *adt = BKE_animdata_from_id(ptr->owner_id);
  if (adt == nullptr) {
    return nullptr;
  }

  /* XXX This function call can become a performance bottleneck. */
  char *rna_path = RNA_path_from_ID_to_property(ptr, prop);
  if (rna_path == nullptr) {
    return nullptr;
  }

  /* Standard F-Curve from animdata - Animation (Action) or Drivers. */
  FCurve *fcu = BKE_animadata_fcurve_find_by_rna_path(adt, rna_path, rnaindex, r_action, r_driven);

  if (fcu != nullptr && r_animdata != nullptr) {
    *r_animdata = adt;
  }

  MEM_freeN(rna_path);
  return fcu;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Finding Keyframes/Extents
 * \{ */

/* Binary search algorithm for finding where to insert BezTriple,
 * with optional argument for precision required.
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
static int BKE_fcurve_bezt_binarysearch_index_ex(const BezTriple array[],
                                                 const float frame,
                                                 const int arraylen,
                                                 const float threshold,
                                                 bool *r_replace)
{
  int start = 0, end = arraylen;
  int loopbreaker = 0, maxloop = arraylen * 2;

  /* Initialize replace-flag first. */
  *r_replace = false;

  /* Sneaky optimizations (don't go through searching process if...):
   * - Keyframe to be added is to be added out of current bounds.
   * - Keyframe to be added would replace one of the existing ones on bounds.
   */
  if (arraylen <= 0 || array == nullptr) {
    CLOG_WARN(&LOG, "encountered invalid array");
    return 0;
  }

  /* Check whether to add before/after/on. */
  /* 'First' Keyframe (when only one keyframe, this case is used) */
  float framenum = array[0].vec[1][0];
  if (IS_EQT(frame, framenum, threshold)) {
    *r_replace = true;
    return 0;
  }
  if (frame < framenum) {
    return 0;
  }

  /* 'Last' Keyframe */
  framenum = array[(arraylen - 1)].vec[1][0];
  if (IS_EQT(frame, framenum, threshold)) {
    *r_replace = true;
    return (arraylen - 1);
  }
  if (frame > framenum) {
    return arraylen;
  }

  /* Most of the time, this loop is just to find where to put it
   * 'loopbreaker' is just here to prevent infinite loops.
   */
  for (loopbreaker = 0; (start <= end) && (loopbreaker < maxloop); loopbreaker++) {
    /* Compute and get midpoint. */

    /* We calculate the midpoint this way to avoid int overflows... */
    const int mid = start + ((end - start) / 2);

    const float midfra = array[mid].vec[1][0];

    /* Check if exactly equal to midpoint. */
    if (IS_EQT(frame, midfra, threshold)) {
      *r_replace = true;
      return mid;
    }

    /* Repeat in upper/lower half. */
    if (frame > midfra) {
      start = mid + 1;
    }
    else if (frame < midfra) {
      end = mid - 1;
    }
  }

  /* Print error if loop-limit exceeded. */
  if (loopbreaker == (maxloop - 1)) {
    CLOG_ERROR(&LOG, "search taking too long");

    /* Include debug info. */
    CLOG_ERROR(&LOG,
               "\tround = %d: start = %d, end = %d, arraylen = %d",
               loopbreaker,
               start,
               end,
               arraylen);
  }

  /* Not found, so return where to place it. */
  return start;
}

int BKE_fcurve_bezt_binarysearch_index(const BezTriple array[],
                                       const float frame,
                                       const int arraylen,
                                       bool *r_replace)
{
  /* This is just a wrapper which uses the default threshold. */
  return BKE_fcurve_bezt_binarysearch_index_ex(
      array, frame, arraylen, BEZT_BINARYSEARCH_THRESH, r_replace);
}

/* ...................................... */

/**
 * Get the first and last index to the bezt array that satisfies the given parameters.
 *
 * \param selected_keys_only: Only accept indices of bezt that are selected.
 * Is a subset of frame_range.
 * \param frame_range: Only consider keyframes in that frame interval. Can be nullptr.
 */
static bool get_bounding_bezt_indices(const FCurve *fcu,
                                      const bool selected_keys_only,
                                      const float frame_range[2],
                                      int *r_first,
                                      int *r_last)
{
  /* Sanity checks. */
  if (fcu->bezt == nullptr) {
    return false;
  }

  *r_first = 0;
  *r_last = fcu->totvert - 1;

  bool found = false;
  if (frame_range != nullptr) {
    /* If a range is passed in find the first and last keyframe within that range. */
    bool replace = false;
    *r_first = BKE_fcurve_bezt_binarysearch_index(
        fcu->bezt, frame_range[0], fcu->totvert, &replace);
    *r_last = BKE_fcurve_bezt_binarysearch_index(
        fcu->bezt, frame_range[1], fcu->totvert, &replace);

    /* If first and last index are the same, no keyframes were found in the range. */
    if (*r_first == *r_last) {
      return false;
    }

    /* The binary search returns an index where a keyframe would be inserted,
     * so it needs to be clamped to ensure it is in range of the array. */
    *r_first = clamp_i(*r_first, 0, fcu->totvert - 1);
    *r_last = clamp_i(*r_last - 1, 0, fcu->totvert - 1);
  }

  /* Only include selected items? */
  if (selected_keys_only) {
    /* Find first selected. */
    for (int i = *r_first; i <= *r_last; i++) {
      BezTriple *bezt = &fcu->bezt[i];
      if (BEZT_ISSEL_ANY(bezt)) {
        *r_first = i;
        found = true;
        break;
      }
    }

    /* Find last selected. */
    for (int i = *r_last; i >= *r_first; i--) {
      BezTriple *bezt = &fcu->bezt[i];
      if (BEZT_ISSEL_ANY(bezt)) {
        *r_last = i;
        found = true;
        break;
      }
    }
  }
  else {
    found = true;
  }

  return found;
}

static void calculate_bezt_bounds_x(BezTriple *bezt_array,
                                    const int index_range[2],
                                    const bool include_handles,
                                    float *r_min,
                                    float *r_max)
{
  *r_min = bezt_array[index_range[0]].vec[1][0];
  *r_max = bezt_array[index_range[1]].vec[1][0];

  if (include_handles) {
    /* Need to check all handles because they might extend beyond their neighboring keys. */
    for (int i = index_range[0]; i <= index_range[1]; i++) {
      const BezTriple *bezt = &bezt_array[i];
      *r_min = min_fff(*r_min, bezt->vec[0][0], bezt->vec[1][0]);
      *r_max = max_fff(*r_max, bezt->vec[1][0], bezt->vec[2][0]);
    }
  }
}

static void calculate_bezt_bounds_y(BezTriple *bezt_array,
                                    const int index_range[2],
                                    const bool selected_keys_only,
                                    const bool include_handles,
                                    float *r_min,
                                    float *r_max)
{
  *r_min = bezt_array[index_range[0]].vec[1][1];
  *r_max = bezt_array[index_range[0]].vec[1][1];

  for (int i = index_range[0]; i <= index_range[1]; i++) {
    const BezTriple *bezt = &bezt_array[i];

    if (selected_keys_only && !BEZT_ISSEL_ANY(bezt)) {
      continue;
    }

    *r_min = min_ff(*r_min, bezt->vec[1][1]);
    *r_max = max_ff(*r_max, bezt->vec[1][1]);

    if (include_handles) {
      *r_min = min_fff(*r_min, bezt->vec[0][1], bezt->vec[2][1]);
      *r_max = max_fff(*r_max, bezt->vec[0][1], bezt->vec[2][1]);
    }
  }
}

static bool calculate_bezt_bounds(const FCurve *fcu,
                                  const bool selected_keys_only,
                                  const bool include_handles,
                                  const float frame_range[2],
                                  rctf *r_bounds)
{
  int index_range[2];
  const bool found_indices = get_bounding_bezt_indices(
      fcu, selected_keys_only, frame_range, &index_range[0], &index_range[1]);
  if (!found_indices) {
    return false;
  }
  calculate_bezt_bounds_x(
      fcu->bezt, index_range, include_handles, &r_bounds->xmin, &r_bounds->xmax);
  calculate_bezt_bounds_y(fcu->bezt,
                          index_range,
                          selected_keys_only,
                          include_handles,
                          &r_bounds->ymin,
                          &r_bounds->ymax);
  return true;
}

static bool calculate_fpt_bounds(const FCurve *fcu, const float frame_range[2], rctf *r_bounds)
{
  r_bounds->xmin = INFINITY;
  r_bounds->xmax = -INFINITY;
  r_bounds->ymin = INFINITY;
  r_bounds->ymax = -INFINITY;

  const int first_index = 0;
  const int last_index = fcu->totvert - 1;
  int start_index = first_index;
  int end_index = last_index;

  if (frame_range != nullptr) {
    /* Start index can be calculated because fpt has a key on every full frame. */
    const float start_index_f = frame_range[0] - fcu->fpt[0].vec[0];
    const float end_index_f = start_index_f + frame_range[1] - frame_range[0];

    if (start_index_f > fcu->totvert - 1 || end_index_f < 0) {
      /* Range is outside of keyframe samples. */
      return false;
    }

    /* Range might be partially covering keyframe samples. */
    start_index = clamp_i(start_index_f, 0, fcu->totvert - 1);
    end_index = clamp_i(end_index_f, 0, fcu->totvert - 1);
  }

  /* X range can be directly calculated from end verts. */
  r_bounds->xmin = fcu->fpt[start_index].vec[0];
  r_bounds->xmax = fcu->fpt[end_index].vec[0];

  for (int i = start_index; i <= end_index; i++) {
    r_bounds->ymin = min_ff(r_bounds->ymin, fcu->fpt[i].vec[1]);
    r_bounds->ymax = max_ff(r_bounds->ymax, fcu->fpt[i].vec[1]);
  }

  return BLI_rctf_is_valid(r_bounds);
}

bool BKE_fcurve_calc_bounds(const FCurve *fcu,
                            const bool selected_keys_only,
                            const bool include_handles,
                            const float frame_range[2],
                            rctf *r_bounds)
{
  if (fcu->totvert == 0) {
    return false;
  }

  if (fcu->bezt) {
    const bool found_bounds = calculate_bezt_bounds(
        fcu, selected_keys_only, include_handles, frame_range, r_bounds);
    return found_bounds;
  }

  if (fcu->fpt) {
    const bool founds_bounds = calculate_fpt_bounds(fcu, frame_range, r_bounds);
    return founds_bounds;
  }

  return false;
}

bool BKE_fcurve_calc_range(const FCurve *fcu,
                           float *r_start,
                           float *r_end,
                           const bool selected_keys_only)
{
  float min = 0.0f;
  float max = 0.0f;
  bool foundvert = false;

  if (fcu->totvert == 0) {
    return false;
  }

  if (fcu->bezt) {
    int index_range[2];
    foundvert = get_bounding_bezt_indices(
        fcu, selected_keys_only, nullptr, &index_range[0], &index_range[1]);
    if (!foundvert) {
      return false;
    }
    const bool include_handles = false;
    calculate_bezt_bounds_x(fcu->bezt, index_range, include_handles, &min, &max);
  }
  else if (fcu->fpt) {
    min = fcu->fpt[0].vec[0];
    max = fcu->fpt[fcu->totvert - 1].vec[0];

    foundvert = true;
  }

  *r_start = min;
  *r_end = max;

  return foundvert;
}

float *BKE_fcurves_calc_keyed_frames_ex(FCurve **fcurve_array,
                                        int fcurve_array_len,
                                        const float interval,
                                        int *r_frames_len)
{
  /* Use `1e-3f` as the smallest possible value since these are converted to integers
   * and we can be sure `MAXFRAME / 1e-3f < INT_MAX` as it's around half the size. */
  const double interval_db = max_ff(interval, 1e-3f);
  GSet *frames_unique = BLI_gset_int_new(__func__);
  for (int fcurve_index = 0; fcurve_index < fcurve_array_len; fcurve_index++) {
    const FCurve *fcu = fcurve_array[fcurve_index];
    for (int i = 0; i < fcu->totvert; i++) {
      const BezTriple *bezt = &fcu->bezt[i];
      const double value = round(double(bezt->vec[1][0]) / interval_db);
      BLI_assert(value > INT_MIN && value < INT_MAX);
      BLI_gset_add(frames_unique, POINTER_FROM_INT(int(value)));
    }
  }

  const size_t frames_len = BLI_gset_len(frames_unique);
  float *frames = static_cast<float *>(MEM_mallocN(sizeof(*frames) * frames_len, __func__));

  GSetIterator gs_iter;
  int i = 0;
  GSET_ITER_INDEX (gs_iter, frames_unique, i) {
    const int value = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
    frames[i] = double(value) * interval_db;
  }
  BLI_gset_free(frames_unique, nullptr);

  qsort(frames, frames_len, sizeof(*frames), BLI_sortutil_cmp_float);
  *r_frames_len = frames_len;
  return frames;
}

float *BKE_fcurves_calc_keyed_frames(FCurve **fcurve_array,
                                     int fcurve_array_len,
                                     int *r_frames_len)
{
  return BKE_fcurves_calc_keyed_frames_ex(fcurve_array, fcurve_array_len, 1.0f, r_frames_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Active Keyframe
 * \{ */

void BKE_fcurve_active_keyframe_set(FCurve *fcu, const BezTriple *active_bezt)
{
  if (active_bezt == nullptr) {
    fcu->active_keyframe_index = FCURVE_ACTIVE_KEYFRAME_NONE;
    return;
  }

  /* Gracefully handle out-of-bounds pointers. Ideally this would do a BLI_assert() as well, but
   * then the unit tests would break in debug mode. */
  const ptrdiff_t offset = active_bezt - fcu->bezt;
  if (offset < 0 || offset >= fcu->totvert) {
    fcu->active_keyframe_index = FCURVE_ACTIVE_KEYFRAME_NONE;
    return;
  }

  /* The active keyframe should always be selected. */
  BLI_assert_msg(BEZT_ISSEL_ANY(active_bezt), "active keyframe must be selected");

  fcu->active_keyframe_index = int(offset);
}

int BKE_fcurve_active_keyframe_index(const FCurve *fcu)
{
  const int active_keyframe_index = fcu->active_keyframe_index;

  /* Array access boundary checks. */
  if (fcu->bezt == nullptr || active_keyframe_index >= fcu->totvert || active_keyframe_index < 0) {
    return FCURVE_ACTIVE_KEYFRAME_NONE;
  }

  const BezTriple *active_bezt = &fcu->bezt[active_keyframe_index];
  if (((active_bezt->f1 | active_bezt->f2 | active_bezt->f3) & SELECT) == 0) {
    /* The active keyframe should always be selected. If it's not selected, it can't be active. */
    return FCURVE_ACTIVE_KEYFRAME_NONE;
  }

  return active_keyframe_index;
}

/** \} */

void BKE_fcurve_keyframe_move_time_with_handles(BezTriple *keyframe, const float new_time)
{
  const float time_delta = new_time - keyframe->vec[1][0];
  keyframe->vec[0][0] += time_delta;
  keyframe->vec[1][0] = new_time;
  keyframe->vec[2][0] += time_delta;
}

void BKE_fcurve_keyframe_move_value_with_handles(BezTriple *keyframe, const float new_value)
{
  const float value_delta = new_value - keyframe->vec[1][1];
  keyframe->vec[0][1] += value_delta;
  keyframe->vec[1][1] = new_value;
  keyframe->vec[2][1] += value_delta;
}

/* -------------------------------------------------------------------- */
/** \name Status Checks
 * \{ */

bool BKE_fcurve_are_keyframes_usable(const FCurve *fcu)
{
  /* F-Curve must exist. */
  if (fcu == nullptr) {
    return false;
  }

  /* F-Curve must not have samples - samples are mutually exclusive of keyframes. */
  if (fcu->fpt) {
    return false;
  }

  /* If it has modifiers, none of these should "drastically" alter the curve. */
  if (fcu->modifiers.first) {
    /* Check modifiers from last to first, as last will be more influential. */
    /* TODO: optionally, only check modifier if it is the active one... (Joshua Leung 2010) */
    LISTBASE_FOREACH_BACKWARD (FModifier *, fcm, &fcu->modifiers) {
      /* Ignore if muted/disabled. */
      if (fcm->flag & (FMODIFIER_FLAG_DISABLED | FMODIFIER_FLAG_MUTED)) {
        continue;
      }

      /* Type checks. */
      switch (fcm->type) {
        /* Clearly harmless - do nothing. */
        case FMODIFIER_TYPE_CYCLES:
        case FMODIFIER_TYPE_STEPPED:
        case FMODIFIER_TYPE_NOISE:
          break;

        /* Sometimes harmful - depending on whether they're "additive" or not. */
        case FMODIFIER_TYPE_GENERATOR: {
          FMod_Generator *data = (FMod_Generator *)fcm->data;

          if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0) {
            return false;
          }
          break;
        }
        case FMODIFIER_TYPE_FN_GENERATOR: {
          FMod_FunctionGenerator *data = (FMod_FunctionGenerator *)fcm->data;

          if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0) {
            return false;
          }
          break;
        }
        /* Always harmful - cannot allow. */
        default:
          return false;
      }
    }
  }

  /* Keyframes are usable. */
  return true;
}

bool BKE_fcurve_is_protected(const FCurve *fcu)
{
  return ((fcu->flag & FCURVE_PROTECTED) || (fcu->grp && (fcu->grp->flag & AGRP_PROTECTED)));
}

bool BKE_fcurve_has_selected_control_points(const FCurve *fcu)
{
  int i;
  BezTriple *bezt;
  for (bezt = fcu->bezt, i = 0; i < fcu->totvert; ++i, ++bezt) {
    if ((bezt->f2 & SELECT) != 0) {
      return true;
    }
  }
  return false;
}

bool BKE_fcurve_is_keyframable(const FCurve *fcu)
{
  /* F-Curve's keyframes must be "usable" (i.e. visible + have an effect on final result) */
  if (BKE_fcurve_are_keyframes_usable(fcu) == 0) {
    return false;
  }

  /* F-Curve must currently be editable too. */
  if (BKE_fcurve_is_protected(fcu)) {
    return false;
  }

  /* F-Curve is keyframable. */
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Samples Utilities
 * \{ */

/* Some utilities for working with FPoints (i.e. 'sampled' animation curve data, such as
 * data imported from BVH/motion-capture files), which are specialized for use with high density
 * datasets, which BezTriples/Keyframe data are ill equipped to do. */

float fcurve_samplingcb_evalcurve(FCurve *fcu, void * /*data*/, float evaltime)
{
  /* Assume any interference from drivers on the curve is intended... */
  return evaluate_fcurve(fcu, evaltime);
}

void fcurve_store_samples(FCurve *fcu, void *data, int start, int end, FcuSampleFunc sample_cb)
{
  /* Sanity checks. */
  /* TODO: make these tests report errors using reports not CLOG's (Joshua Leung 2009) */
  if (ELEM(nullptr, fcu, sample_cb)) {
    CLOG_ERROR(&LOG, "No F-Curve with F-Curve Modifiers to Bake");
    return;
  }
  if (start > end) {
    CLOG_ERROR(&LOG, "Error: Frame range for Sampled F-Curve creation is inappropriate");
    return;
  }

  /* Set up sample data. */
  FPoint *new_fpt;
  FPoint *fpt = new_fpt = static_cast<FPoint *>(
      MEM_callocN(sizeof(FPoint) * (end - start + 1), "FPoint Samples"));

  /* Use the sampling callback at 1-frame intervals from start to end frames. */
  for (int cfra = start; cfra <= end; cfra++, fpt++) {
    fpt->vec[0] = float(cfra);
    fpt->vec[1] = sample_cb(fcu, data, float(cfra));
  }

  /* Free any existing sample/keyframe data on curve. */
  if (fcu->bezt) {
    MEM_freeN(fcu->bezt);
  }
  if (fcu->fpt) {
    MEM_freeN(fcu->fpt);
  }

  /* Store the samples. */
  fcu->bezt = nullptr;
  fcu->fpt = new_fpt;
  fcu->totvert = end - start + 1;
}

static void init_unbaked_bezt_data(BezTriple *bezt)
{
  bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
  /* Baked FCurve points always use linear interpolation. */
  bezt->ipo = BEZT_IPO_LIN;
  bezt->h1 = bezt->h2 = HD_AUTO_ANIM;
}

void fcurve_samples_to_keyframes(FCurve *fcu, const int start, const int end)
{

  /* Sanity checks. */
  /* TODO: make these tests report errors using reports not CLOG's (Joshua Leung 2009). */
  if (fcu == nullptr) {
    CLOG_ERROR(&LOG, "No F-Curve with F-Curve Modifiers to Un-Bake");
    return;
  }

  if (start > end) {
    CLOG_ERROR(&LOG, "Error: Frame range to unbake F-Curve is inappropriate");
    return;
  }

  if (fcu->fpt == nullptr) {
    /* No data to unbake. */
    CLOG_ERROR(&LOG, "Error: Curve contains no baked keyframes");
    return;
  }

  /* Free any existing sample/keyframe data on the curve. */
  if (fcu->bezt) {
    MEM_freeN(fcu->bezt);
  }

  FPoint *fpt = fcu->fpt;
  int keyframes_to_insert = end - start;
  int sample_points = fcu->totvert;

  BezTriple *bezt = fcu->bezt = static_cast<BezTriple *>(
      MEM_callocN(sizeof(*fcu->bezt) * size_t(keyframes_to_insert), __func__));
  fcu->totvert = keyframes_to_insert;

  /* Get first sample point to 'copy' as keyframe. */
  for (; sample_points && (fpt->vec[0] < start); fpt++, sample_points--) {
    /* pass */
  }

  /* Current position in the timeline. */
  int cur_pos = start;

  /* Add leading dummy flat points if needed. */
  for (; keyframes_to_insert && (fpt->vec[0] > start); cur_pos++, bezt++, keyframes_to_insert--) {
    init_unbaked_bezt_data(bezt);
    bezt->vec[1][0] = float(cur_pos);
    bezt->vec[1][1] = fpt->vec[1];
  }

  /* Copy actual sample points. */
  for (; keyframes_to_insert && sample_points;
       cur_pos++, bezt++, keyframes_to_insert--, fpt++, sample_points--)
  {
    init_unbaked_bezt_data(bezt);
    copy_v2_v2(bezt->vec[1], fpt->vec);
  }

  /* Add trailing dummy flat points if needed. */
  for (fpt--; keyframes_to_insert; cur_pos++, bezt++, keyframes_to_insert--) {
    init_unbaked_bezt_data(bezt);
    bezt->vec[1][0] = float(cur_pos);
    bezt->vec[1][1] = fpt->vec[1];
  }

  MEM_SAFE_FREE(fcu->fpt);

  /* Not strictly needed since we use linear interpolation, but better be consistent here. */
  BKE_fcurve_handles_recalc(fcu);
}

/* ***************************** F-Curve Sanity ********************************* */
/* The functions here are used in various parts of Blender, usually after some editing
 * of keyframe data has occurred. They ensure that keyframe data is properly ordered and
 * that the handles are correct.
 */

eFCU_Cycle_Type BKE_fcurve_get_cycle_type(const FCurve *fcu)
{
  FModifier *fcm = static_cast<FModifier *>(fcu->modifiers.first);

  if (!fcm || fcm->type != FMODIFIER_TYPE_CYCLES) {
    return FCU_CYCLE_NONE;
  }

  if (fcm->flag & (FMODIFIER_FLAG_DISABLED | FMODIFIER_FLAG_MUTED)) {
    return FCU_CYCLE_NONE;
  }

  if (fcm->flag & (FMODIFIER_FLAG_RANGERESTRICT | FMODIFIER_FLAG_USEINFLUENCE)) {
    return FCU_CYCLE_NONE;
  }

  FMod_Cycles *data = (FMod_Cycles *)fcm->data;

  if (data && data->after_cycles == 0 && data->before_cycles == 0) {
    if (data->before_mode == FCM_EXTRAPOLATE_CYCLIC && data->after_mode == FCM_EXTRAPOLATE_CYCLIC)
    {
      return FCU_CYCLE_PERFECT;
    }

    if (ELEM(data->before_mode, FCM_EXTRAPOLATE_CYCLIC, FCM_EXTRAPOLATE_CYCLIC_OFFSET) &&
        ELEM(data->after_mode, FCM_EXTRAPOLATE_CYCLIC, FCM_EXTRAPOLATE_CYCLIC_OFFSET))
    {
      return FCU_CYCLE_OFFSET;
    }
  }

  return FCU_CYCLE_NONE;
}

bool BKE_fcurve_is_cyclic(const FCurve *fcu)
{
  return BKE_fcurve_get_cycle_type(fcu) != FCU_CYCLE_NONE;
}

/* Shifts 'in' by the difference in coordinates between 'to' and 'from',
 * using 'out' as the output buffer.
 * When 'to' and 'from' are end points of the loop, this moves the 'in' point one loop cycle.
 */
static BezTriple *cycle_offset_triple(
    bool cycle, BezTriple *out, const BezTriple *in, const BezTriple *from, const BezTriple *to)
{
  if (!cycle) {
    return nullptr;
  }

  memcpy(out, in, sizeof(BezTriple));

  float delta[3];
  sub_v3_v3v3(delta, to->vec[1], from->vec[1]);

  for (int i = 0; i < 3; i++) {
    add_v3_v3(out->vec[i], delta);
  }

  return out;
}

void BKE_fcurve_handles_recalc_ex(FCurve *fcu, eBezTriple_Flag handle_sel_flag)
{
  /* Error checking:
   * - Need at least two points.
   * - Need bezier keys.
   * - Only bezier-interpolation has handles (for now).
   */
  if (ELEM(nullptr, fcu, fcu->bezt) ||
      (fcu->totvert < 2) /*|| ELEM(fcu->ipo, BEZT_IPO_CONST, BEZT_IPO_LIN) */)
  {
    return;
  }

  /* If the first modifier is Cycles, smooth the curve through the cycle. */
  BezTriple *first = &fcu->bezt[0], *last = &fcu->bezt[fcu->totvert - 1];
  BezTriple tmp;

  const bool cycle = BKE_fcurve_is_cyclic(fcu) && BEZT_IS_AUTOH(first) && BEZT_IS_AUTOH(last);

  /* Get initial pointers. */
  BezTriple *bezt = fcu->bezt;
  BezTriple *prev = cycle_offset_triple(cycle, &tmp, &fcu->bezt[fcu->totvert - 2], last, first);
  BezTriple *next = (bezt + 1);

  /* Loop over all beztriples, adjusting handles. */
  int a = fcu->totvert;
  while (a--) {
    /* Clamp timing of handles to be on either side of beztriple. */
    if (bezt->vec[0][0] > bezt->vec[1][0]) {
      bezt->vec[0][0] = bezt->vec[1][0];
    }
    if (bezt->vec[2][0] < bezt->vec[1][0]) {
      bezt->vec[2][0] = bezt->vec[1][0];
    }

    /* Calculate auto-handles. */
    BKE_nurb_handle_calc_ex(bezt, prev, next, handle_sel_flag, true, fcu->auto_smoothing);

    /* For automatic ease in and out. */
    if (BEZT_IS_AUTOH(bezt) && !cycle) {
      /* Only do this on first or last beztriple. */
      if (ELEM(a, 0, fcu->totvert - 1)) {
        /* Set both handles to have same horizontal value as keyframe. */
        if (fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) {
          bezt->vec[0][1] = bezt->vec[2][1] = bezt->vec[1][1];
          /* Remember that these keyframes are special, they don't need to be adjusted. */
          bezt->auto_handle_type = HD_AUTOTYPE_LOCKED_FINAL;
        }
      }
    }

    /* Avoid total smoothing failure on duplicate keyframes (can happen during grab). */
    if (prev && prev->vec[1][0] >= bezt->vec[1][0]) {
      prev->auto_handle_type = bezt->auto_handle_type = HD_AUTOTYPE_LOCKED_FINAL;
    }

    /* Advance pointers for next iteration. */
    prev = bezt;

    if (a == 1) {
      next = cycle_offset_triple(cycle, &tmp, &fcu->bezt[1], first, last);
    }
    else if (next != nullptr) {
      next++;
    }

    bezt++;
  }

  /* If cyclic extrapolation and Auto Clamp has triggered, ensure it is symmetric. */
  if (cycle && (first->auto_handle_type != HD_AUTOTYPE_NORMAL ||
                last->auto_handle_type != HD_AUTOTYPE_NORMAL))
  {
    first->vec[0][1] = first->vec[2][1] = first->vec[1][1];
    last->vec[0][1] = last->vec[2][1] = last->vec[1][1];
    first->auto_handle_type = last->auto_handle_type = HD_AUTOTYPE_LOCKED_FINAL;
  }

  /* Do a second pass for auto handle: compute the handle to have 0 acceleration step. */
  if (fcu->auto_smoothing != FCURVE_SMOOTH_NONE) {
    BKE_nurb_handle_smooth_fcurve(fcu->bezt, fcu->totvert, cycle);
  }
}

void BKE_fcurve_handles_recalc(FCurve *fcu)
{
  BKE_fcurve_handles_recalc_ex(fcu, eBezTriple_Flag(SELECT));
}

void testhandles_fcurve(FCurve *fcu, eBezTriple_Flag sel_flag, const bool use_handle)
{
  /* Only beztriples have handles (bpoints don't though). */
  if (ELEM(nullptr, fcu, fcu->bezt)) {
    return;
  }

  /* Loop over beztriples. */
  BezTriple *bezt;
  uint a;
  for (a = 0, bezt = fcu->bezt; a < fcu->totvert; a++, bezt++) {
    BKE_nurb_bezt_handle_test(bezt, sel_flag, use_handle, false);
  }

  /* Recalculate handles. */
  BKE_fcurve_handles_recalc_ex(fcu, sel_flag);
}

void sort_time_fcurve(FCurve *fcu)
{
  if (fcu->bezt == nullptr) {
    return;
  }

  /* Keep adjusting order of beztriples until nothing moves (bubble-sort). */
  BezTriple *bezt;
  uint a;

  bool ok = true;
  while (ok) {
    ok = false;
    /* Currently, will only be needed when there are beztriples. */

    /* Loop over ALL points to adjust position in array and recalculate handles. */
    for (a = 0, bezt = fcu->bezt; a < fcu->totvert; a++, bezt++) {
      /* Check if there's a next beztriple which we could try to swap with current. */
      if (a < (fcu->totvert - 1)) {
        /* Swap if one is after the other (and indicate that order has changed). */
        if (bezt->vec[1][0] > (bezt + 1)->vec[1][0]) {
          SWAP(BezTriple, *bezt, *(bezt + 1));
          ok = true;
        }
      }
    }
  }

  for (a = 0, bezt = fcu->bezt; a < fcu->totvert; a++, bezt++) {
    /* If either one of both of the points exceeds crosses over the keyframe time... */
    if ((bezt->vec[0][0] > bezt->vec[1][0]) && (bezt->vec[2][0] < bezt->vec[1][0])) {
      /* Swap handles if they have switched sides for some reason. */
      swap_v2_v2(bezt->vec[0], bezt->vec[2]);
    }
    else {
      /* Clamp handles. */
      CLAMP_MAX(bezt->vec[0][0], bezt->vec[1][0]);
      CLAMP_MIN(bezt->vec[2][0], bezt->vec[1][0]);
    }
  }
}

bool test_time_fcurve(FCurve *fcu)
{
  uint a;

  /* Sanity checks. */
  if (fcu == nullptr) {
    return false;
  }

  /* Currently, only need to test beztriples. */
  if (fcu->bezt) {
    BezTriple *bezt;

    /* Loop through all BezTriples, stopping when one exceeds the one after it. */
    for (a = 0, bezt = fcu->bezt; a < (fcu->totvert - 1); a++, bezt++) {
      if (bezt->vec[1][0] > (bezt + 1)->vec[1][0]) {
        return true;
      }
    }
  }
  else if (fcu->fpt) {
    FPoint *fpt;

    /* Loop through all FPoints, stopping when one exceeds the one after it. */
    for (a = 0, fpt = fcu->fpt; a < (fcu->totvert - 1); a++, fpt++) {
      if (fpt->vec[0] > (fpt + 1)->vec[0]) {
        return true;
      }
    }
  }

  /* None need any swapping. */
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name F-Curve Calculations
 * \{ */

void BKE_fcurve_correct_bezpart(const float v1[2], float v2[2], float v3[2], const float v4[2])
{
  float h1[2], h2[2], len1, len2, len, fac;

  /* Calculate handle deltas. */
  h1[0] = v1[0] - v2[0];
  h1[1] = v1[1] - v2[1];

  h2[0] = v4[0] - v3[0];
  h2[1] = v4[1] - v3[1];

  /* Calculate distances:
   * - len  = Span of time between keyframes.
   * - len1 = Length of handle of start key.
   * - len2 = Length of handle of end key.
   */
  len = v4[0] - v1[0];
  len1 = fabsf(h1[0]);
  len2 = fabsf(h2[0]);

  /* If the handles have no length, no need to do any corrections. */
  if ((len1 + len2) == 0.0f) {
    return;
  }

  /* To prevent looping or rewinding, handles cannot
   * exceed the adjacent key-frames time position. */
  if (len1 > len) {
    fac = len / len1;
    v2[0] = (v1[0] - fac * h1[0]);
    v2[1] = (v1[1] - fac * h1[1]);
  }

  if (len2 > len) {
    fac = len / len2;
    v3[0] = (v4[0] - fac * h2[0]);
    v3[1] = (v4[1] - fac * h2[1]);
  }
}

/**
 * Find roots of cubic equation (c0 + c1 x + c2 x^2 + c3 x^3)
 * \return number of roots in `o`.
 *
 * \note it is up to the caller to allocate enough memory for `o`.
 */
static int solve_cubic(double c0, double c1, double c2, double c3, float *o)
{
  double a, b, c, p, q, d, t, phi;
  int nr = 0;

  if (c3 != 0.0) {
    a = c2 / c3;
    b = c1 / c3;
    c = c0 / c3;
    a = a / 3;

    p = b / 3 - a * a;
    q = (2 * a * a * a - a * b + c) / 2;
    d = q * q + p * p * p;

    if (d > 0.0) {
      t = sqrt(d);
      o[0] = float(sqrt3d(-q + t) + sqrt3d(-q - t) - a);

      if ((o[0] >= float(SMALL)) && (o[0] <= 1.000001f)) {
        return 1;
      }
      return 0;
    }

    if (d == 0.0) {
      t = sqrt3d(-q);
      o[0] = float(2 * t - a);

      if ((o[0] >= float(SMALL)) && (o[0] <= 1.000001f)) {
        nr++;
      }
      o[nr] = float(-t - a);

      if ((o[nr] >= float(SMALL)) && (o[nr] <= 1.000001f)) {
        return nr + 1;
      }
      return nr;
    }

    phi = acos(-q / sqrt(-(p * p * p)));
    t = sqrt(-p);
    p = cos(phi / 3);
    q = sqrt(3 - 3 * p * p);
    o[0] = float(2 * t * p - a);

    if ((o[0] >= float(SMALL)) && (o[0] <= 1.000001f)) {
      nr++;
    }
    o[nr] = float(-t * (p + q) - a);

    if ((o[nr] >= float(SMALL)) && (o[nr] <= 1.000001f)) {
      nr++;
    }
    o[nr] = float(-t * (p - q) - a);

    if ((o[nr] >= float(SMALL)) && (o[nr] <= 1.000001f)) {
      return nr + 1;
    }
    return nr;
  }
  a = c2;
  b = c1;
  c = c0;

  if (a != 0.0) {
    /* Discriminant */
    p = b * b - 4 * a * c;

    if (p > 0) {
      p = sqrt(p);
      o[0] = float((-b - p) / (2 * a));

      if ((o[0] >= float(SMALL)) && (o[0] <= 1.000001f)) {
        nr++;
      }
      o[nr] = float((-b + p) / (2 * a));

      if ((o[nr] >= float(SMALL)) && (o[nr] <= 1.000001f)) {
        return nr + 1;
      }
      return nr;
    }

    if (p == 0) {
      o[0] = float(-b / (2 * a));
      if ((o[0] >= float(SMALL)) && (o[0] <= 1.000001f)) {
        return 1;
      }
    }

    return 0;
  }

  if (b != 0.0) {
    o[0] = float(-c / b);

    if ((o[0] >= float(SMALL)) && (o[0] <= 1.000001f)) {
      return 1;
    }
    return 0;
  }

  if (c == 0.0) {
    o[0] = 0.0;
    return 1;
  }

  return 0;
}

/* Find root(s) ('zero') of a Bezier curve. */
static int findzero(float x, float q0, float q1, float q2, float q3, float *o)
{
  const double c0 = q0 - x;
  const double c1 = 3.0f * (q1 - q0);
  const double c2 = 3.0f * (q0 - 2.0f * q1 + q2);
  const double c3 = q3 - q0 + 3.0f * (q1 - q2);

  return solve_cubic(c0, c1, c2, c3, o);
}

static void berekeny(float f1, float f2, float f3, float f4, float *o, int b)
{
  float t, c0, c1, c2, c3;
  int a;

  c0 = f1;
  c1 = 3.0f * (f2 - f1);
  c2 = 3.0f * (f1 - 2.0f * f2 + f3);
  c3 = f4 - f1 + 3.0f * (f2 - f3);

  for (a = 0; a < b; a++) {
    t = o[a];
    o[a] = c0 + t * c1 + t * t * c2 + t * t * t * c3;
  }
}

static void fcurve_bezt_free(FCurve *fcu)
{
  MEM_SAFE_FREE(fcu->bezt);
  fcu->totvert = 0;
}

bool BKE_fcurve_bezt_subdivide_handles(BezTriple *bezt,
                                       BezTriple *prev,
                                       BezTriple *next,
                                       float *r_pdelta)
{
  /* The four points that make up this section of the Bezier curve. */
  const float *prev_coords = prev->vec[1];
  float *prev_handle_right = prev->vec[2];
  float *next_handle_left = next->vec[0];
  const float *next_coords = next->vec[1];

  float *new_handle_left = bezt->vec[0];
  const float *new_coords = bezt->vec[1];
  float *new_handle_right = bezt->vec[2];

  if (new_coords[0] <= prev_coords[0] || new_coords[0] >= next_coords[0]) {
    /* The new keyframe is outside the (prev_coords, next_coords) range. */
    return false;
  }

  /* Apply evaluation-time limits and compute the effective curve. */
  BKE_fcurve_correct_bezpart(prev_coords, prev_handle_right, next_handle_left, next_coords);
  float roots[4];
  if (!findzero(new_coords[0],
                prev_coords[0],
                prev_handle_right[0],
                next_handle_left[0],
                next_coords[0],
                roots))
  {
    return false;
  }

  const float t = roots[0]; /* Percentage of the curve at which the split should occur. */
  if (t <= 0.0f || t >= 1.0f) {
    /* The split would occur outside the curve, which isn't possible. */
    return false;
  }

  /* De Casteljau split, requires three iterations of splitting.
   * See https://pomax.github.io/bezierinfo/#decasteljau */
  float split1[3][2], split2[2][2], split3[2];
  interp_v2_v2v2(split1[0], prev_coords, prev_handle_right, t);
  interp_v2_v2v2(split1[1], prev_handle_right, next_handle_left, t);
  interp_v2_v2v2(split1[2], next_handle_left, next_coords, t);
  interp_v2_v2v2(split2[0], split1[0], split1[1], t);
  interp_v2_v2v2(split2[1], split1[1], split1[2], t);
  interp_v2_v2v2(split3, split2[0], split2[1], t);

  /* Update the existing handles. */
  copy_v2_v2(prev_handle_right, split1[0]);
  copy_v2_v2(next_handle_left, split1[2]);

  float diff_coords[2];
  sub_v2_v2v2(diff_coords, new_coords, split3);
  add_v2_v2v2(new_handle_left, split2[0], diff_coords);
  add_v2_v2v2(new_handle_right, split2[1], diff_coords);

  *r_pdelta = diff_coords[1];
  return true;
}

void BKE_fcurve_bezt_shrink(FCurve *fcu, const int new_totvert)
{
  BLI_assert(new_totvert >= 0);
  BLI_assert(new_totvert <= fcu->totvert);

  /* No early return when new_totvert == fcu->totvert. There is no way to know the intention of the
   * caller, nor the history of the FCurve so far, so `fcu->bezt` may actually have allocated space
   * for more than `fcu->totvert` keys. */

  if (new_totvert == 0) {
    fcurve_bezt_free(fcu);
    return;
  }

  fcu->bezt = static_cast<BezTriple *>(
      MEM_reallocN(fcu->bezt, new_totvert * sizeof(*(fcu->bezt))));
  fcu->totvert = new_totvert;
}

void BKE_fcurve_delete_key(FCurve *fcu, int index)
{
  /* sanity check */
  if (fcu == nullptr) {
    return;
  }

  /* verify the index:
   * 1) cannot be greater than the number of available keyframes
   * 2) negative indices are for specifying a value from the end of the array
   */
  if (abs(index) >= fcu->totvert) {
    return;
  }
  if (index < 0) {
    index += fcu->totvert;
  }

  /* Delete this keyframe */
  memmove(
      &fcu->bezt[index], &fcu->bezt[index + 1], sizeof(BezTriple) * (fcu->totvert - index - 1));
  fcu->totvert--;

  /* Free the array of BezTriples if there are not keyframes */
  if (fcu->totvert == 0) {
    fcurve_bezt_free(fcu);
  }
}

bool BKE_fcurve_delete_keys_selected(FCurve *fcu)
{
  if (fcu->bezt == nullptr) { /* ignore baked curves */
    return false;
  }

  bool changed = false;

  /* Delete selected BezTriples */
  for (int i = 0; i < fcu->totvert; i++) {
    if (fcu->bezt[i].f2 & SELECT) {
      if (i == fcu->active_keyframe_index) {
        BKE_fcurve_active_keyframe_set(fcu, nullptr);
      }
      memmove(&fcu->bezt[i], &fcu->bezt[i + 1], sizeof(BezTriple) * (fcu->totvert - i - 1));
      fcu->totvert--;
      i--;
      changed = true;
    }
  }

  /* Free the array of BezTriples if there are not keyframes */
  if (fcu->totvert == 0) {
    fcurve_bezt_free(fcu);
  }

  return changed;
}

void BKE_fcurve_delete_keys_all(FCurve *fcu)
{
  fcurve_bezt_free(fcu);
}

/* Time + Average value */
struct tRetainedKeyframe {
  tRetainedKeyframe *next, *prev;
  float frame; /* frame to cluster around */
  float val;   /* average value */

  size_t tot_count; /* number of keyframes that have been averaged */
  size_t del_count; /* number of keyframes of this sort that have been deleted so far */
};

void BKE_fcurve_merge_duplicate_keys(FCurve *fcu, const int sel_flag, const bool use_handle)
{
  /* NOTE: We assume that all keys are sorted */
  ListBase retained_keys = {nullptr, nullptr};
  const bool can_average_points = ((fcu->flag & (FCURVE_INT_VALUES | FCURVE_DISCRETE_VALUES)) ==
                                   0);

  /* sanity checks */
  if ((fcu->totvert == 0) || (fcu->bezt == nullptr)) {
    return;
  }

  /* 1) Identify selected keyframes, and average the values on those
   * in case there are collisions due to multiple keys getting scaled
   * to all end up on the same frame
   */
  for (int i = 0; i < fcu->totvert; i++) {
    BezTriple *bezt = &fcu->bezt[i];

    if (BEZT_ISSEL_ANY(bezt)) {
      bool found = false;

      /* If there's another selected frame here, merge it */
      LISTBASE_FOREACH_BACKWARD (tRetainedKeyframe *, rk, &retained_keys) {
        if (IS_EQT(rk->frame, bezt->vec[1][0], BEZT_BINARYSEARCH_THRESH)) {
          rk->val += bezt->vec[1][1];
          rk->tot_count++;

          found = true;
          break;
        }
        if (rk->frame < bezt->vec[1][0]) {
          /* Terminate early if have passed the supposed insertion point? */
          break;
        }
      }

      /* If nothing found yet, create a new one */
      if (found == false) {
        tRetainedKeyframe *rk = static_cast<tRetainedKeyframe *>(
            MEM_callocN(sizeof(tRetainedKeyframe), "tRetainedKeyframe"));

        rk->frame = bezt->vec[1][0];
        rk->val = bezt->vec[1][1];
        rk->tot_count = 1;

        BLI_addtail(&retained_keys, rk);
      }
    }
  }

  if (BLI_listbase_is_empty(&retained_keys)) {
    /* This may happen if none of the points were selected... */
    if (G.debug & G_DEBUG) {
      printf("%s: nothing to do for FCurve %p (rna_path = '%s')\n", __func__, fcu, fcu->rna_path);
    }
    return;
  }

  /* Compute the average values for each retained keyframe */
  LISTBASE_FOREACH (tRetainedKeyframe *, rk, &retained_keys) {
    rk->val = rk->val / float(rk->tot_count);
  }

  /* 2) Delete all keyframes duplicating the "retained keys" found above
   *   - Most of these will be unselected keyframes
   *   - Some will be selected keyframes though. For those, we only keep the last one
   *     (or else everything is gone), and replace its value with the averaged value.
   */
  for (int i = fcu->totvert - 1; i >= 0; i--) {
    BezTriple *bezt = &fcu->bezt[i];

    /* Is this keyframe a candidate for deletion? */
    /* TODO: Replace loop with an O(1) lookup instead */
    LISTBASE_FOREACH_BACKWARD (tRetainedKeyframe *, rk, &retained_keys) {
      if (IS_EQT(bezt->vec[1][0], rk->frame, BEZT_BINARYSEARCH_THRESH)) {
        /* Selected keys are treated with greater care than unselected ones... */
        if (BEZT_ISSEL_ANY(bezt)) {
          /* - If this is the last selected key left (based on rk->del_count) ==> UPDATE IT
           *   (or else we wouldn't have any keyframe left here)
           * - Otherwise, there are still other selected keyframes on this frame
           *   to be merged down still ==> DELETE IT
           */
          if (rk->del_count == rk->tot_count - 1) {
            /* Update keyframe... */
            if (can_average_points) {
              /* TODO: update handles too? */
              bezt->vec[1][1] = rk->val;
            }
          }
          else {
            /* Delete Keyframe */
            BKE_fcurve_delete_key(fcu, i);
          }

          /* Update count of how many we've deleted
           * - It should only matter that we're doing this for all but the last one
           */
          rk->del_count++;
        }
        else {
          /* Always delete - Unselected keys don't matter */
          BKE_fcurve_delete_key(fcu, i);
        }

        /* Stop the RK search... we've found our match now */
        break;
      }
    }
  }

  /* 3) Recalculate handles */
  testhandles_fcurve(fcu, eBezTriple_Flag(sel_flag), use_handle);

  /* cleanup */
  BLI_freelistN(&retained_keys);
}

void BKE_fcurve_deduplicate_keys(FCurve *fcu)
{
  BLI_assert_msg(fcu->bezt, "this function only works with regular (non-sampled) FCurves");
  if (fcu->totvert < 2 || fcu->bezt == nullptr) {
    return;
  }

  int prev_bezt_index = 0;
  for (int i = 1; i < fcu->totvert; i++) {
    BezTriple *bezt = &fcu->bezt[i];
    BezTriple *prev_bezt = &fcu->bezt[prev_bezt_index];

    const float bezt_x = bezt->vec[1][0];
    const float prev_x = prev_bezt->vec[1][0];

    if (bezt_x - prev_x <= BEZT_BINARYSEARCH_THRESH) {
      /* Replace 'prev_bezt', as it has the same X-coord as 'bezt' and the last one wins. */
      *prev_bezt = *bezt;

      if (floor(bezt_x) == bezt_x) {
        /* Keep the 'bezt_x' coordinate, as being on a frame is more desirable
         * than being ever so slightly off. */
      }
      else {
        /* Move the retained key to the old X-coordinate, to 'anchor' the X-coordinate used for
         * subsequent comparisons. Without this, the reference X-coordinate would keep moving
         * forward in time, potentially merging in more keys than desired. */
        BKE_fcurve_keyframe_move_time_with_handles(prev_bezt, prev_x);
      }
      continue;
    }

    /* Next iteration should look at the current element. However, because of the deletions, that
     * may not be at index 'i'; after this increment, `prev_bezt_index` points at where the current
     * element should go. */
    prev_bezt_index++;

    if (prev_bezt_index != i) {
      /* This bezt should be kept, so copy it to its new location in the array. */
      fcu->bezt[prev_bezt_index] = *bezt;
    }
  }

  BKE_fcurve_bezt_shrink(fcu, prev_bezt_index + 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name F-Curve Evaluation
 * \{ */

static float fcurve_eval_keyframes_extrapolate(
    FCurve *fcu, BezTriple *bezts, float evaltime, int endpoint_offset, int direction_to_neighbor)
{
  /* The first/last keyframe. */
  const BezTriple *endpoint_bezt = bezts + endpoint_offset;
  /* The second (to last) keyframe. */
  const BezTriple *neighbor_bezt = endpoint_bezt + direction_to_neighbor;

  if (endpoint_bezt->ipo == BEZT_IPO_CONST || fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT ||
      (fcu->flag & FCURVE_DISCRETE_VALUES) != 0)
  {
    /* Constant (BEZT_IPO_HORIZ) extrapolation or constant interpolation, so just extend the
     * endpoint's value. */
    return endpoint_bezt->vec[1][1];
  }

  if (endpoint_bezt->ipo == BEZT_IPO_LIN) {
    /* Use the next center point instead of our own handle for linear interpolated extrapolate. */
    if (fcu->totvert == 1) {
      return endpoint_bezt->vec[1][1];
    }

    const float dx = endpoint_bezt->vec[1][0] - evaltime;
    float fac = neighbor_bezt->vec[1][0] - endpoint_bezt->vec[1][0];

    /* Prevent division by zero. */
    if (fac == 0.0f) {
      return endpoint_bezt->vec[1][1];
    }

    fac = (neighbor_bezt->vec[1][1] - endpoint_bezt->vec[1][1]) / fac;
    return endpoint_bezt->vec[1][1] - (fac * dx);
  }

  /* Use the gradient of the second handle (later) of neighbor to calculate the gradient and thus
   * the value of the curve at evaluation time. */
  const int handle = direction_to_neighbor > 0 ? 0 : 2;
  const float dx = endpoint_bezt->vec[1][0] - evaltime;
  float fac = endpoint_bezt->vec[1][0] - endpoint_bezt->vec[handle][0];

  /* Prevent division by zero. */
  if (fac == 0.0f) {
    return endpoint_bezt->vec[1][1];
  }

  fac = (endpoint_bezt->vec[1][1] - endpoint_bezt->vec[handle][1]) / fac;
  return endpoint_bezt->vec[1][1] - (fac * dx);
}

static float fcurve_eval_keyframes_interpolate(const FCurve *fcu,
                                               const BezTriple *bezts,
                                               float evaltime)
{
  const float eps = 1.e-8f;
  uint a;

  /* Evaltime occurs somewhere in the middle of the curve. */
  bool exact = false;

  /* Use binary search to find appropriate keyframes...
   *
   * The threshold here has the following constraints:
   * - 0.001 is too coarse:
   *   We get artifacts with 2cm driver movements at 1BU = 1m (see #40332).
   *
   * - 0.00001 is too fine:
   *   Weird errors, like selecting the wrong keyframe range (see #39207), occur.
   *   This lower bound was established in b888a32eee8147b028464336ad2404d8155c64dd.
   */
  a = BKE_fcurve_bezt_binarysearch_index_ex(bezts, evaltime, fcu->totvert, 0.0001, &exact);
  const BezTriple *bezt = bezts + a;

  if (exact) {
    /* Index returned must be interpreted differently when it sits on top of an existing keyframe
     * - That keyframe is the start of the segment we need (see action_bug_2.blend in #39207).
     */
    return bezt->vec[1][1];
  }

  /* Index returned refers to the keyframe that the eval-time occurs *before*
   * - hence, that keyframe marks the start of the segment we're dealing with.
   */
  const BezTriple *prevbezt = (a > 0) ? (bezt - 1) : bezt;

  /* Use if the key is directly on the frame, in rare cases this is needed else we get 0.0 instead.
   * XXX: consult #39207 for examples of files where failure of these checks can cause issues. */
  if (fabsf(bezt->vec[1][0] - evaltime) < eps) {
    return bezt->vec[1][1];
  }

  if (evaltime < prevbezt->vec[1][0] || bezt->vec[1][0] < evaltime) {
    if (G.debug & G_DEBUG) {
      printf("   ERROR: failed eval - p=%f b=%f, t=%f (%f)\n",
             prevbezt->vec[1][0],
             bezt->vec[1][0],
             evaltime,
             fabsf(bezt->vec[1][0] - evaltime));
    }
    return 0.0f;
  }

  /* Evaltime occurs within the interval defined by these two keyframes. */
  const float begin = prevbezt->vec[1][1];
  const float change = bezt->vec[1][1] - prevbezt->vec[1][1];
  const float duration = bezt->vec[1][0] - prevbezt->vec[1][0];
  const float time = evaltime - prevbezt->vec[1][0];
  const float amplitude = prevbezt->amplitude;
  const float period = prevbezt->period;

  /* Value depends on interpolation mode. */
  if ((prevbezt->ipo == BEZT_IPO_CONST) || (fcu->flag & FCURVE_DISCRETE_VALUES) || (duration == 0))
  {
    /* Constant (evaltime not relevant, so no interpolation needed). */
    return prevbezt->vec[1][1];
  }

  switch (prevbezt->ipo) {
    /* Interpolation ...................................... */
    case BEZT_IPO_BEZ: {
      float v1[2], v2[2], v3[2], v4[2], opl[32];

      /* Bezier interpolation. */
      /* (v1, v2) are the first keyframe and its 2nd handle. */
      v1[0] = prevbezt->vec[1][0];
      v1[1] = prevbezt->vec[1][1];
      v2[0] = prevbezt->vec[2][0];
      v2[1] = prevbezt->vec[2][1];
      /* (v3, v4) are the last keyframe's 1st handle + the last keyframe. */
      v3[0] = bezt->vec[0][0];
      v3[1] = bezt->vec[0][1];
      v4[0] = bezt->vec[1][0];
      v4[1] = bezt->vec[1][1];

      if (fabsf(v1[1] - v4[1]) < FLT_EPSILON && fabsf(v2[1] - v3[1]) < FLT_EPSILON &&
          fabsf(v3[1] - v4[1]) < FLT_EPSILON)
      {
        /* Optimization: If all the handles are flat/at the same values,
         * the value is simply the shared value (see #40372 -> F91346).
         */
        return v1[1];
      }
      /* Adjust handles so that they don't overlap (forming a loop). */
      BKE_fcurve_correct_bezpart(v1, v2, v3, v4);

      /* Try to get a value for this position - if failure, try another set of points. */
      if (!findzero(evaltime, v1[0], v2[0], v3[0], v4[0], opl)) {
        if (G.debug & G_DEBUG) {
          printf("    ERROR: findzero() failed at %f with %f %f %f %f\n",
                 evaltime,
                 v1[0],
                 v2[0],
                 v3[0],
                 v4[0]);
        }
        return 0.0;
      }

      berekeny(v1[1], v2[1], v3[1], v4[1], opl, 1);
      return opl[0];
    }
    case BEZT_IPO_LIN:
      /* Linear - simply linearly interpolate between values of the two keyframes. */
      return BLI_easing_linear_ease(time, begin, change, duration);

    /* Easing ............................................ */
    case BEZT_IPO_BACK:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_back_ease_in(time, begin, change, duration, prevbezt->back);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_back_ease_out(time, begin, change, duration, prevbezt->back);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_back_ease_in_out(time, begin, change, duration, prevbezt->back);

        default: /* Default/Auto: same as ease out. */
          return BLI_easing_back_ease_out(time, begin, change, duration, prevbezt->back);
      }
      break;

    case BEZT_IPO_BOUNCE:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_bounce_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_bounce_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_bounce_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease out. */
          return BLI_easing_bounce_ease_out(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_CIRC:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_circ_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_circ_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_circ_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_circ_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_CUBIC:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_cubic_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_cubic_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_cubic_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_cubic_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_ELASTIC:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_elastic_ease_in(time, begin, change, duration, amplitude, period);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_elastic_ease_in_out(time, begin, change, duration, amplitude, period);

        default: /* Default/Auto: same as ease out. */
          return BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
      }
      break;

    case BEZT_IPO_EXPO:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_expo_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_expo_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_expo_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_expo_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_QUAD:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quad_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quad_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quad_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_quad_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_QUART:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quart_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quart_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quart_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_quart_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_QUINT:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quint_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quint_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quint_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_quint_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_SINE:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_sine_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_sine_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_sine_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_sine_ease_in(time, begin, change, duration);
      }
      break;

    default:
      return prevbezt->vec[1][1];
  }

  return 0.0f;
}

/* Calculate F-Curve value for 'evaltime' using #BezTriple keyframes. */
static float fcurve_eval_keyframes(FCurve *fcu, BezTriple *bezts, float evaltime)
{
  if (evaltime <= bezts->vec[1][0]) {
    return fcurve_eval_keyframes_extrapolate(fcu, bezts, evaltime, 0, +1);
  }

  const BezTriple *lastbezt = bezts + fcu->totvert - 1;
  if (lastbezt->vec[1][0] <= evaltime) {
    return fcurve_eval_keyframes_extrapolate(fcu, bezts, evaltime, fcu->totvert - 1, -1);
  }

  return fcurve_eval_keyframes_interpolate(fcu, bezts, evaltime);
}

/* Calculate F-Curve value for 'evaltime' using #FPoint samples. */
static float fcurve_eval_samples(const FCurve *fcu, const FPoint *fpts, float evaltime)
{
  float cvalue = 0.0f;

  /* Get pointers. */
  const FPoint *prevfpt = fpts;
  const FPoint *lastfpt = prevfpt + fcu->totvert - 1;

  /* Evaluation time at or past endpoints? */
  if (prevfpt->vec[0] >= evaltime) {
    /* Before or on first sample, so just extend value. */
    cvalue = prevfpt->vec[1];
  }
  else if (lastfpt->vec[0] <= evaltime) {
    /* After or on last sample, so just extend value. */
    cvalue = lastfpt->vec[1];
  }
  else {
    float t = fabsf(evaltime - floorf(evaltime));

    /* Find the one on the right frame (assume that these are spaced on 1-frame intervals). */
    const FPoint *fpt = prevfpt + (int(evaltime) - int(prevfpt->vec[0]));

    /* If not exactly on the frame, perform linear interpolation with the next one. */
    if (t != 0.0f && t < 1.0f) {
      cvalue = interpf(fpt->vec[1], (fpt + 1)->vec[1], 1.0f - t);
    }
    else {
      cvalue = fpt->vec[1];
    }
  }

  return cvalue;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name F-Curve - Evaluation
 * \{ */

/* Evaluate and return the value of the given F-Curve at the specified frame ("evaltime")
 * NOTE: this is also used for drivers.
 */
static float evaluate_fcurve_ex(FCurve *fcu, float evaltime, float cvalue)
{
  /* Evaluate modifiers which modify time to evaluate the base curve at. */
  FModifiersStackStorage storage;
  storage.modifier_count = BLI_listbase_count(&fcu->modifiers);
  storage.size_per_modifier = evaluate_fmodifiers_storage_size_per_modifier(&fcu->modifiers);
  storage.buffer = alloca(storage.modifier_count * storage.size_per_modifier);

  const float devaltime = evaluate_time_fmodifiers(
      &storage, &fcu->modifiers, fcu, cvalue, evaltime);

  /* Evaluate curve-data
   * - 'devaltime' instead of 'evaltime', as this is the time that the last time-modifying
   *   F-Curve modifier on the stack requested the curve to be evaluated at.
   */
  if (fcu->bezt) {
    cvalue = fcurve_eval_keyframes(fcu, fcu->bezt, devaltime);
  }
  else if (fcu->fpt) {
    cvalue = fcurve_eval_samples(fcu, fcu->fpt, devaltime);
  }

  /* Evaluate modifiers. */
  evaluate_value_fmodifiers(&storage, &fcu->modifiers, fcu, &cvalue, devaltime);

  /* If curve can only have integral values, perform truncation (i.e. drop the decimal part)
   * here so that the curve can be sampled correctly.
   */
  if (fcu->flag & FCURVE_INT_VALUES) {
    cvalue = floorf(cvalue + 0.5f);
  }

  return cvalue;
}

float evaluate_fcurve(FCurve *fcu, float evaltime)
{
  BLI_assert(fcu->driver == nullptr);

  return evaluate_fcurve_ex(fcu, evaltime, 0.0);
}

float evaluate_fcurve_only_curve(FCurve *fcu, float evaltime)
{
  /* Can be used to evaluate the (key-framed) f-curve only.
   * Also works for driver-f-curves when the driver itself is not relevant.
   * E.g. when inserting a keyframe in a driver f-curve. */
  return evaluate_fcurve_ex(fcu, evaltime, 0.0);
}

float evaluate_fcurve_driver(PathResolvedRNA *anim_rna,
                             FCurve *fcu,
                             ChannelDriver *driver_orig,
                             const AnimationEvalContext *anim_eval_context)
{
  BLI_assert(fcu->driver != nullptr);
  float cvalue = 0.0f;
  float evaltime = anim_eval_context->eval_time;

  /* If there is a driver (only if this F-Curve is acting as 'driver'),
   * evaluate it to find value to use as "evaltime" since drivers essentially act as alternative
   * input (i.e. in place of 'time') for F-Curves. */
  if (fcu->driver) {
    /* Evaltime now serves as input for the curve. */
    evaltime = evaluate_driver(anim_rna, fcu->driver, driver_orig, anim_eval_context);

    /* Only do a default 1-1 mapping if it's unlikely that anything else will set a value... */
    if (fcu->totvert == 0) {
      bool do_linear = true;

      /* Out-of-range F-Modifiers will block, as will those which just plain overwrite the values
       * XXX: additive is a bit more dicey; it really depends then if things are in range or not...
       */
      LISTBASE_FOREACH (FModifier *, fcm, &fcu->modifiers) {
        /* If there are range-restrictions, we must definitely block #36950. */
        if ((fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) == 0 ||
            (fcm->sfra <= evaltime && fcm->efra >= evaltime))
        {
          /* Within range: here it probably doesn't matter,
           * though we'd want to check on additive. */
        }
        else {
          /* Outside range: modifier shouldn't contribute to the curve here,
           * though it does in other areas, so neither should the driver! */
          do_linear = false;
        }
      }

      /* Only copy over results if none of the modifiers disagreed with this. */
      if (do_linear) {
        cvalue = evaltime;
      }
    }
  }

  return evaluate_fcurve_ex(fcu, evaltime, cvalue);
}

bool BKE_fcurve_is_empty(const FCurve *fcu)
{
  return fcu->totvert == 0 && fcu->driver == nullptr &&
         !list_has_suitable_fmodifier(&fcu->modifiers, 0, FMI_TYPE_GENERATE_CURVE);
}

float calculate_fcurve(PathResolvedRNA *anim_rna,
                       FCurve *fcu,
                       const AnimationEvalContext *anim_eval_context)
{
  /* Only calculate + set curval (overriding the existing value) if curve has
   * any data which warrants this...
   */
  if (BKE_fcurve_is_empty(fcu)) {
    return 0.0f;
  }

  /* Calculate and set curval (evaluates driver too if necessary). */
  float curval;
  if (fcu->driver) {
    curval = evaluate_fcurve_driver(anim_rna, fcu, fcu->driver, anim_eval_context);
  }
  else {
    curval = evaluate_fcurve(fcu, anim_eval_context->eval_time);
  }
  fcu->curval = curval; /* Debug display only, not thread safe! */
  return curval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name F-Curve - .blend file API
 * \{ */

void BKE_fmodifiers_blend_write(BlendWriter *writer, ListBase *fmodifiers)
{
  /* Write all modifiers first (for faster reloading) */
  BLO_write_struct_list(writer, FModifier, fmodifiers);

  /* Modifiers */
  LISTBASE_FOREACH (FModifier *, fcm, fmodifiers) {
    const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);

    /* Write the specific data */
    if (fmi && fcm->data) {
      /* firstly, just write the plain fmi->data struct */
      BLO_write_struct_by_name(writer, fmi->struct_name, fcm->data);

      /* do any modifier specific stuff */
      switch (fcm->type) {
        case FMODIFIER_TYPE_GENERATOR: {
          FMod_Generator *data = static_cast<FMod_Generator *>(fcm->data);

          /* write coefficients array */
          if (data->coefficients) {
            BLO_write_float_array(writer, data->arraysize, data->coefficients);
          }

          break;
        }
        case FMODIFIER_TYPE_ENVELOPE: {
          FMod_Envelope *data = static_cast<FMod_Envelope *>(fcm->data);

          /* write envelope data */
          if (data->data) {
            BLO_write_struct_array(writer, FCM_EnvelopeData, data->totvert, data->data);
          }

          break;
        }
        case FMODIFIER_TYPE_PYTHON: {
          FMod_Python *data = static_cast<FMod_Python *>(fcm->data);

          /* Write ID Properties -- and copy this comment EXACTLY for easy finding
           * of library blocks that implement this. */
          IDP_BlendWrite(writer, data->prop);

          break;
        }
      }
    }
  }
}

void BKE_fmodifiers_blend_read_data(BlendDataReader *reader, ListBase *fmodifiers, FCurve *curve)
{
  LISTBASE_FOREACH (FModifier *, fcm, fmodifiers) {
    /* relink general data */
    BLO_read_data_address(reader, &fcm->data);
    fcm->curve = curve;

    /* do relinking of data for specific types */
    switch (fcm->type) {
      case FMODIFIER_TYPE_GENERATOR: {
        FMod_Generator *data = (FMod_Generator *)fcm->data;
        BLO_read_float_array(reader, data->arraysize, &data->coefficients);
        break;
      }
      case FMODIFIER_TYPE_ENVELOPE: {
        FMod_Envelope *data = (FMod_Envelope *)fcm->data;

        BLO_read_data_address(reader, &data->data);

        break;
      }
      case FMODIFIER_TYPE_PYTHON: {
        FMod_Python *data = (FMod_Python *)fcm->data;

        BLO_read_data_address(reader, &data->prop);
        IDP_BlendDataRead(reader, &data->prop);

        break;
      }
    }
  }
}

void BKE_fcurve_blend_write(BlendWriter *writer, ListBase *fcurves)
{
  BLO_write_struct_list(writer, FCurve, fcurves);
  LISTBASE_FOREACH (FCurve *, fcu, fcurves) {
    /* curve data */
    if (fcu->bezt) {
      BLO_write_struct_array(writer, BezTriple, fcu->totvert, fcu->bezt);
    }
    if (fcu->fpt) {
      BLO_write_struct_array(writer, FPoint, fcu->totvert, fcu->fpt);
    }

    if (fcu->rna_path) {
      BLO_write_string(writer, fcu->rna_path);
    }

    /* driver data */
    if (fcu->driver) {
      ChannelDriver *driver = fcu->driver;

      BLO_write_struct(writer, ChannelDriver, driver);

      /* variables */
      BLO_write_struct_list(writer, DriverVar, &driver->variables);
      LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
        DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
          if (dtar->rna_path) {
            BLO_write_string(writer, dtar->rna_path);
          }
        }
        DRIVER_TARGETS_LOOPER_END;
      }
    }

    /* write F-Modifiers */
    BKE_fmodifiers_blend_write(writer, &fcu->modifiers);
  }
}

void BKE_fcurve_blend_read_data(BlendDataReader *reader, ListBase *fcurves)
{
  /* Link F-Curve data to F-Curve again (non ID-libraries). */
  LISTBASE_FOREACH (FCurve *, fcu, fcurves) {
    /* curve data */
    BLO_read_data_address(reader, &fcu->bezt);
    BLO_read_data_address(reader, &fcu->fpt);

    /* rna path */
    BLO_read_data_address(reader, &fcu->rna_path);

    /* group */
    BLO_read_data_address(reader, &fcu->grp);

    /* clear disabled flag - allows disabled drivers to be tried again (#32155),
     * but also means that another method for "reviving disabled F-Curves" exists
     */
    fcu->flag &= ~FCURVE_DISABLED;

    /* driver */
    BLO_read_data_address(reader, &fcu->driver);
    if (fcu->driver) {
      ChannelDriver *driver = fcu->driver;

      /* Compiled expression data will need to be regenerated
       * (old pointer may still be set here). */
      driver->expr_comp = nullptr;
      driver->expr_simple = nullptr;

      /* Give the driver a fresh chance - the operating environment may be different now
       * (addons, etc. may be different) so the driver namespace may be sane now #32155. */
      driver->flag &= ~DRIVER_FLAG_INVALID;

      /* relink variables, targets and their paths */
      BLO_read_list(reader, &driver->variables);
      LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
        DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
          /* only relink the targets being used */
          if (tarIndex < dvar->num_targets) {
            BLO_read_data_address(reader, &dtar->rna_path);
          }
          else {
            dtar->rna_path = nullptr;
            dtar->id = nullptr;
          }
        }
        DRIVER_TARGETS_LOOPER_END;
      }
    }

    /* modifiers */
    BLO_read_list(reader, &fcu->modifiers);
    BKE_fmodifiers_blend_read_data(reader, &fcu->modifiers, fcu);
  }
}

/** \} */
