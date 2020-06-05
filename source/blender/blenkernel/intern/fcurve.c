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
 * \ingroup bke
 */

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_text_types.h"

#include "BLI_blenlib.h"
#include "BLI_easing.h"
#include "BLI_math.h"

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

#include "RNA_access.h"

#include "CLG_log.h"

#define SMALL -1.0e-10
#define SELECT 1

static CLG_LogRef LOG = {"bke.fcurve"};

/* ************************** Data-Level Functions ************************* */
FCurve *BKE_fcurve_create(void)
{
  FCurve *fcu = MEM_callocN(sizeof(FCurve), __func__);
  return fcu;
}
/* ---------------------- Freeing --------------------------- */

/* Frees the F-Curve itself too, so make sure BLI_remlink is called before calling this... */
void BKE_fcurve_free(FCurve *fcu)
{
  if (fcu == NULL) {
    return;
  }

  /* free curve data */
  MEM_SAFE_FREE(fcu->bezt);
  MEM_SAFE_FREE(fcu->fpt);

  /* free RNA-path, as this were allocated when getting the path string */
  MEM_SAFE_FREE(fcu->rna_path);

  /* free extra data - i.e. modifiers, and driver */
  fcurve_free_driver(fcu);
  free_fmodifiers(&fcu->modifiers);

  /* free f-curve itself */
  MEM_freeN(fcu);
}

/* Frees a list of F-Curves */
void BKE_fcurves_free(ListBase *list)
{
  FCurve *fcu, *fcn;

  /* sanity check */
  if (list == NULL) {
    return;
  }

  /* free data - no need to call remlink before freeing each curve,
   * as we store reference to next, and freeing only touches the curve
   * it's given
   */
  for (fcu = list->first; fcu; fcu = fcn) {
    fcn = fcu->next;
    BKE_fcurve_free(fcu);
  }

  /* clear pointers just in case */
  BLI_listbase_clear(list);
}

/* ---------------------- Copy --------------------------- */

/* duplicate an F-Curve */
FCurve *BKE_fcurve_copy(const FCurve *fcu)
{
  FCurve *fcu_d;

  /* sanity check */
  if (fcu == NULL) {
    return NULL;
  }

  /* make a copy */
  fcu_d = MEM_dupallocN(fcu);

  fcu_d->next = fcu_d->prev = NULL;
  fcu_d->grp = NULL;

  /* copy curve data */
  fcu_d->bezt = MEM_dupallocN(fcu_d->bezt);
  fcu_d->fpt = MEM_dupallocN(fcu_d->fpt);

  /* copy rna-path */
  fcu_d->rna_path = MEM_dupallocN(fcu_d->rna_path);

  /* copy driver */
  fcu_d->driver = fcurve_copy_driver(fcu_d->driver);

  /* copy modifiers */
  copy_fmodifiers(&fcu_d->modifiers, &fcu->modifiers);

  /* return new data */
  return fcu_d;
}

/* duplicate a list of F-Curves */
void BKE_fcurves_copy(ListBase *dst, ListBase *src)
{
  FCurve *dfcu, *sfcu;

  /* sanity checks */
  if (ELEM(NULL, dst, src)) {
    return;
  }

  /* clear destination list first */
  BLI_listbase_clear(dst);

  /* copy one-by-one */
  for (sfcu = src->first; sfcu; sfcu = sfcu->next) {
    dfcu = BKE_fcurve_copy(sfcu);
    BLI_addtail(dst, dfcu);
  }
}

/** Callback used by lib_query to walk over all ID usages (mimics `foreach_id` callback of
 * `IDTypeInfo` structure). */
void BKE_fcurve_foreach_id(FCurve *fcu, LibraryForeachIDData *data)
{
  ChannelDriver *driver = fcu->driver;

  if (driver != NULL) {
    LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
      /* only used targets */
      DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
        BKE_LIB_FOREACHID_PROCESS_ID(data, dtar->id, IDWALK_CB_NOP);
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }

  LISTBASE_FOREACH (FModifier *, fcm, &fcu->modifiers) {
    switch (fcm->type) {
      case FMODIFIER_TYPE_PYTHON: {
        FMod_Python *fcm_py = (FMod_Python *)fcm->data;
        BKE_LIB_FOREACHID_PROCESS(data, fcm_py->script, IDWALK_CB_NOP);

        IDP_foreach_property(fcm_py->prop,
                             IDP_TYPE_FILTER_ID,
                             BKE_lib_query_idpropertiesForeachIDLink_callback,
                             data);
        break;
      }
    }
  }
}

/* ----------------- Finding F-Curves -------------------------- */

/* high level function to get an fcurve from C without having the rna */
FCurve *id_data_find_fcurve(
    ID *id, void *data, StructRNA *type, const char *prop_name, int index, bool *r_driven)
{
  /* anim vars */
  AnimData *adt = BKE_animdata_from_id(id);
  FCurve *fcu = NULL;

  /* rna vars */
  PointerRNA ptr;
  PropertyRNA *prop;
  char *path;

  if (r_driven) {
    *r_driven = false;
  }

  /* only use the current action ??? */
  if (ELEM(NULL, adt, adt->action)) {
    return NULL;
  }

  RNA_pointer_create(id, type, data, &ptr);
  prop = RNA_struct_find_property(&ptr, prop_name);
  if (prop == NULL) {
    return NULL;
  }

  path = RNA_path_from_ID_to_property(&ptr, prop);
  if (path == NULL) {
    return NULL;
  }

  /* animation takes priority over drivers */
  if (adt->action && adt->action->curves.first) {
    fcu = BKE_fcurve_find(&adt->action->curves, path, index);
  }

  /* if not animated, check if driven */
  if (fcu == NULL && adt->drivers.first) {
    fcu = BKE_fcurve_find(&adt->drivers, path, index);
    if (fcu && r_driven) {
      *r_driven = true;
    }
    fcu = NULL;
  }

  MEM_freeN(path);

  return fcu;
}

/* Find the F-Curve affecting the given RNA-access path + index,
 * in the list of F-Curves provided. */
FCurve *BKE_fcurve_find(ListBase *list, const char rna_path[], const int array_index)
{
  FCurve *fcu;

  /* sanity checks */
  if (ELEM(NULL, list, rna_path) || (array_index < 0)) {
    return NULL;
  }

  /* check paths of curves, then array indices... */
  for (fcu = list->first; fcu; fcu = fcu->next) {
    /* simple string-compare (this assumes that they have the same root...) */
    if (fcu->rna_path && STREQ(fcu->rna_path, rna_path)) {
      /* now check indices */
      if (fcu->array_index == array_index) {
        return fcu;
      }
    }
  }

  /* return */
  return NULL;
}

/* quick way to loop over all fcurves of a given 'path' */
FCurve *BKE_fcurve_iter_step(FCurve *fcu_iter, const char rna_path[])
{
  FCurve *fcu;

  /* sanity checks */
  if (ELEM(NULL, fcu_iter, rna_path)) {
    return NULL;
  }

  /* check paths of curves, then array indices... */
  for (fcu = fcu_iter; fcu; fcu = fcu->next) {
    /* simple string-compare (this assumes that they have the same root...) */
    if (fcu->rna_path && STREQ(fcu->rna_path, rna_path)) {
      return fcu;
    }
  }

  /* return */
  return NULL;
}

/**
 * Get list of LinkData's containing pointers to the F-Curves
 * which control the types of data indicated.
 *
 * Lists...
 * - dst: list of LinkData's matching the criteria returned.
 *   List must be freed after use, and is assumed to be empty when passed.
 * - src: list of F-Curves to search through
 * Filters...
 * - dataPrefix: i.e. 'pose.bones[' or 'nodes['
 * - dataName: name of entity within "" immediately following the prefix
 */
int BKE_fcurves_filter(ListBase *dst, ListBase *src, const char *dataPrefix, const char *dataName)
{
  FCurve *fcu;
  int matches = 0;

  /* sanity checks */
  if (ELEM(NULL, dst, src, dataPrefix, dataName)) {
    return 0;
  }
  else if ((dataPrefix[0] == 0) || (dataName[0] == 0)) {
    return 0;
  }

  /* search each F-Curve one by one */
  for (fcu = src->first; fcu; fcu = fcu->next) {
    /* check if quoted string matches the path */
    if (fcu->rna_path == NULL || !strstr(fcu->rna_path, dataPrefix)) {
      continue;
    }

    char *quotedName = BLI_str_quoted_substrN(fcu->rna_path, dataPrefix);
    if (quotedName == NULL) {
      continue;
    }

    /* check if the quoted name matches the required name */
    if (STREQ(quotedName, dataName)) {
      LinkData *ld = MEM_callocN(sizeof(LinkData), __func__);

      ld->data = fcu;
      BLI_addtail(dst, ld);

      matches++;
    }

    /* always free the quoted string, since it needs freeing */
    MEM_freeN(quotedName);
  }
  /* return the number of matches */
  return matches;
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
      NULL, ptr, prop, rnaindex, r_adt, r_action, r_driven, r_special);
}

FCurve *BKE_fcurve_find_by_rna_context_ui(bContext *C,
                                          PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          int rnaindex,
                                          AnimData **r_animdata,
                                          bAction **r_action,
                                          bool *r_driven,
                                          bool *r_special)
{
  FCurve *fcu = NULL;
  PointerRNA tptr = *ptr;

  *r_driven = false;
  *r_special = false;

  if (r_animdata) {
    *r_animdata = NULL;
  }
  if (r_action) {
    *r_action = NULL;
  }

  /* Special case for NLA Control Curves... */
  if (BKE_nlastrip_has_curves_for_property(ptr, prop)) {
    NlaStrip *strip = ptr->data;

    /* Set the special flag, since it cannot be a normal action/driver
     * if we've been told to start looking here...
     */
    *r_special = true;

    /* The F-Curve either exists or it doesn't here... */
    fcu = BKE_fcurve_find(&strip->fcurves, RNA_property_identifier(prop), rnaindex);
    return fcu;
  }

  /* there must be some RNA-pointer + property combon */
  if (prop && tptr.owner_id && RNA_property_animateable(&tptr, prop)) {
    AnimData *adt = BKE_animdata_from_id(tptr.owner_id);
    int step = (
        /* Always 1 in case we have no context (can't check in 'ancestors' of given RNA ptr). */
        C ? 2 : 1);
    char *path = NULL;

    if (!adt && C) {
      path = BKE_animdata_driver_path_hack(C, &tptr, prop, NULL);
      adt = BKE_animdata_from_id(tptr.owner_id);
      step--;
    }

    /* Standard F-Curve - Animation (Action) or Drivers */
    while (adt && step--) {
      if ((adt->action == NULL || adt->action->curves.first == NULL) &&
          (adt->drivers.first == NULL)) {
        continue;
      }

      /* XXX this function call can become a performance bottleneck */
      if (step) {
        path = RNA_path_from_ID_to_property(&tptr, prop);
      }
      if (path == NULL) {
        continue;
      }

      // XXX: the logic here is duplicated with a function up above
      /* animation takes priority over drivers */
      if (adt->action && adt->action->curves.first) {
        fcu = BKE_fcurve_find(&adt->action->curves, path, rnaindex);

        if (fcu && r_action) {
          *r_action = adt->action;
        }
      }

      /* if not animated, check if driven */
      if (!fcu && (adt->drivers.first)) {
        fcu = BKE_fcurve_find(&adt->drivers, path, rnaindex);

        if (fcu) {
          if (r_animdata) {
            *r_animdata = adt;
          }
          *r_driven = true;
        }
      }

      if (fcu && r_action) {
        if (r_animdata) {
          *r_animdata = adt;
        }
        *r_action = adt->action;
        break;
      }

      if (step) {
        char *tpath = BKE_animdata_driver_path_hack(C, &tptr, prop, path);
        if (tpath && tpath != path) {
          MEM_freeN(path);
          path = tpath;
          adt = BKE_animdata_from_id(tptr.owner_id);
        }
        else {
          adt = NULL;
        }
      }
    }
    MEM_SAFE_FREE(path);
  }

  return fcu;
}

/* ----------------- Finding Keyframes/Extents -------------------------- */

/* Binary search algorithm for finding where to insert BezTriple,
 * with optional argument for precision required.
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
static int binarysearch_bezt_index_ex(
    BezTriple array[], float frame, int arraylen, float threshold, bool *r_replace)
{
  int start = 0, end = arraylen;
  int loopbreaker = 0, maxloop = arraylen * 2;

  /* initialize replace-flag first */
  *r_replace = false;

  /* sneaky optimizations (don't go through searching process if...):
   * - keyframe to be added is to be added out of current bounds
   * - keyframe to be added would replace one of the existing ones on bounds
   */
  if ((arraylen <= 0) || (array == NULL)) {
    CLOG_WARN(&LOG, "encountered invalid array");
    return 0;
  }

  /* check whether to add before/after/on */
  float framenum;

  /* 'First' Keyframe (when only one keyframe, this case is used) */
  framenum = array[0].vec[1][0];
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

  /* most of the time, this loop is just to find where to put it
   * 'loopbreaker' is just here to prevent infinite loops
   */
  for (loopbreaker = 0; (start <= end) && (loopbreaker < maxloop); loopbreaker++) {
    /* compute and get midpoint */

    /* We calculate the midpoint this way to avoid int overflows... */
    int mid = start + ((end - start) / 2);

    float midfra = array[mid].vec[1][0];

    /* check if exactly equal to midpoint */
    if (IS_EQT(frame, midfra, threshold)) {
      *r_replace = true;
      return mid;
    }

    /* repeat in upper/lower half */
    if (frame > midfra) {
      start = mid + 1;
    }
    else if (frame < midfra) {
      end = mid - 1;
    }
  }

  /* print error if loop-limit exceeded */
  if (loopbreaker == (maxloop - 1)) {
    CLOG_ERROR(&LOG, "search taking too long");

    /* include debug info */
    CLOG_ERROR(&LOG,
               "\tround = %d: start = %d, end = %d, arraylen = %d",
               loopbreaker,
               start,
               end,
               arraylen);
  }

  /* not found, so return where to place it */
  return start;
}

/* Binary search algorithm for finding where to insert BezTriple. (for use by insert_bezt_fcurve)
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
int binarysearch_bezt_index(BezTriple array[], float frame, int arraylen, bool *r_replace)
{
  /* this is just a wrapper which uses the default threshold */
  return binarysearch_bezt_index_ex(array, frame, arraylen, BEZT_BINARYSEARCH_THRESH, r_replace);
}

/* ...................................... */

/* helper for calc_fcurve_* functions -> find first and last BezTriple to be used */
static short get_fcurve_end_keyframes(FCurve *fcu,
                                      BezTriple **first,
                                      BezTriple **last,
                                      const bool do_sel_only)
{
  bool found = false;

  /* init outputs */
  *first = NULL;
  *last = NULL;

  /* sanity checks */
  if (fcu->bezt == NULL) {
    return found;
  }

  /* only include selected items? */
  if (do_sel_only) {
    BezTriple *bezt;
    unsigned int i;

    /* find first selected */
    bezt = fcu->bezt;
    for (i = 0; i < fcu->totvert; bezt++, i++) {
      if (BEZT_ISSEL_ANY(bezt)) {
        *first = bezt;
        found = true;
        break;
      }
    }

    /* find last selected */
    bezt = ARRAY_LAST_ITEM(fcu->bezt, BezTriple, fcu->totvert);
    for (i = 0; i < fcu->totvert; bezt--, i++) {
      if (BEZT_ISSEL_ANY(bezt)) {
        *last = bezt;
        found = true;
        break;
      }
    }
  }
  else {
    /* just full array */
    *first = fcu->bezt;
    *last = ARRAY_LAST_ITEM(fcu->bezt, BezTriple, fcu->totvert);
    found = true;
  }

  return found;
}

/* Calculate the extents of F-Curve's data */
bool BKE_fcurve_calc_bounds(FCurve *fcu,
                            float *xmin,
                            float *xmax,
                            float *ymin,
                            float *ymax,
                            const bool do_sel_only,
                            const bool include_handles)
{
  float xminv = 999999999.0f, xmaxv = -999999999.0f;
  float yminv = 999999999.0f, ymaxv = -999999999.0f;
  bool foundvert = false;
  unsigned int i;

  if (fcu->totvert) {
    if (fcu->bezt) {
      BezTriple *bezt_first = NULL, *bezt_last = NULL;

      if (xmin || xmax) {
        /* get endpoint keyframes */
        foundvert = get_fcurve_end_keyframes(fcu, &bezt_first, &bezt_last, do_sel_only);

        if (bezt_first) {
          BLI_assert(bezt_last != NULL);

          if (include_handles) {
            xminv = min_fff(xminv, bezt_first->vec[0][0], bezt_first->vec[1][0]);
            xmaxv = max_fff(xmaxv, bezt_last->vec[1][0], bezt_last->vec[2][0]);
          }
          else {
            xminv = min_ff(xminv, bezt_first->vec[1][0]);
            xmaxv = max_ff(xmaxv, bezt_last->vec[1][0]);
          }
        }
      }

      /* only loop over keyframes to find extents for values if needed */
      if (ymin || ymax) {
        BezTriple *bezt, *prevbezt = NULL;

        for (bezt = fcu->bezt, i = 0; i < fcu->totvert; prevbezt = bezt, bezt++, i++) {
          if ((do_sel_only == false) || BEZT_ISSEL_ANY(bezt)) {
            /* keyframe itself */
            yminv = min_ff(yminv, bezt->vec[1][1]);
            ymaxv = max_ff(ymaxv, bezt->vec[1][1]);

            if (include_handles) {
              /* left handle - only if applicable
               * NOTE: for the very first keyframe,
               * the left handle actually has no bearings on anything. */
              if (prevbezt && (prevbezt->ipo == BEZT_IPO_BEZ)) {
                yminv = min_ff(yminv, bezt->vec[0][1]);
                ymaxv = max_ff(ymaxv, bezt->vec[0][1]);
              }

              /* right handle - only if applicable */
              if (bezt->ipo == BEZT_IPO_BEZ) {
                yminv = min_ff(yminv, bezt->vec[2][1]);
                ymaxv = max_ff(ymaxv, bezt->vec[2][1]);
              }
            }

            foundvert = true;
          }
        }
      }
    }
    else if (fcu->fpt) {
      /* frame range can be directly calculated from end verts */
      if (xmin || xmax) {
        xminv = min_ff(xminv, fcu->fpt[0].vec[0]);
        xmaxv = max_ff(xmaxv, fcu->fpt[fcu->totvert - 1].vec[0]);
      }

      /* only loop over keyframes to find extents for values if needed */
      if (ymin || ymax) {
        FPoint *fpt;

        for (fpt = fcu->fpt, i = 0; i < fcu->totvert; fpt++, i++) {
          if (fpt->vec[1] < yminv) {
            yminv = fpt->vec[1];
          }
          if (fpt->vec[1] > ymaxv) {
            ymaxv = fpt->vec[1];
          }

          foundvert = true;
        }
      }
    }
  }

  if (foundvert) {
    if (xmin) {
      *xmin = xminv;
    }
    if (xmax) {
      *xmax = xmaxv;
    }

    if (ymin) {
      *ymin = yminv;
    }
    if (ymax) {
      *ymax = ymaxv;
    }
  }
  else {
    if (G.debug & G_DEBUG) {
      printf("F-Curve calc bounds didn't find anything, so assuming minimum bounds of 1.0\n");
    }

    if (xmin) {
      *xmin = 0.0f;
    }
    if (xmax) {
      *xmax = 1.0f;
    }

    if (ymin) {
      *ymin = 0.0f;
    }
    if (ymax) {
      *ymax = 1.0f;
    }
  }

  return foundvert;
}

/* Calculate the extents of F-Curve's keyframes */
bool BKE_fcurve_calc_range(
    FCurve *fcu, float *start, float *end, const bool do_sel_only, const bool do_min_length)
{
  float min = 999999999.0f, max = -999999999.0f;
  bool foundvert = false;

  if (fcu->totvert) {
    if (fcu->bezt) {
      BezTriple *bezt_first = NULL, *bezt_last = NULL;

      /* get endpoint keyframes */
      get_fcurve_end_keyframes(fcu, &bezt_first, &bezt_last, do_sel_only);

      if (bezt_first) {
        BLI_assert(bezt_last != NULL);

        min = min_ff(min, bezt_first->vec[1][0]);
        max = max_ff(max, bezt_last->vec[1][0]);

        foundvert = true;
      }
    }
    else if (fcu->fpt) {
      min = min_ff(min, fcu->fpt[0].vec[0]);
      max = max_ff(max, fcu->fpt[fcu->totvert - 1].vec[0]);

      foundvert = true;
    }
  }

  if (foundvert == false) {
    min = max = 0.0f;
  }

  if (do_min_length) {
    /* minimum length is 1 frame */
    if (min == max) {
      max += 1.0f;
    }
  }

  *start = min;
  *end = max;

  return foundvert;
}

/* ----------------- Status Checks -------------------------- */

/* Are keyframes on F-Curve of any use?
 * Usability of keyframes refers to whether they should be displayed,
 * and also whether they will have any influence on the final result.
 */
bool BKE_fcurve_are_keyframes_usable(FCurve *fcu)
{
  /* F-Curve must exist */
  if (fcu == NULL) {
    return false;
  }

  /* F-Curve must not have samples - samples are mutually exclusive of keyframes */
  if (fcu->fpt) {
    return false;
  }

  /* if it has modifiers, none of these should "drastically" alter the curve */
  if (fcu->modifiers.first) {
    FModifier *fcm;

    /* check modifiers from last to first, as last will be more influential */
    /* TODO: optionally, only check modifier if it is the active one... */
    for (fcm = fcu->modifiers.last; fcm; fcm = fcm->prev) {
      /* ignore if muted/disabled */
      if (fcm->flag & (FMODIFIER_FLAG_DISABLED | FMODIFIER_FLAG_MUTED)) {
        continue;
      }

      /* type checks */
      switch (fcm->type) {
        /* clearly harmless - do nothing */
        case FMODIFIER_TYPE_CYCLES:
        case FMODIFIER_TYPE_STEPPED:
        case FMODIFIER_TYPE_NOISE:
          break;

        /* sometimes harmful - depending on whether they're "additive" or not */
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
        /* always harmful - cannot allow */
        default:
          return false;
      }
    }
  }

  /* keyframes are usable */
  return true;
}

bool BKE_fcurve_is_protected(FCurve *fcu)
{
  return ((fcu->flag & FCURVE_PROTECTED) || ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)));
}

/* Can keyframes be added to F-Curve?
 * Keyframes can only be added if they are already visible
 */
bool BKE_fcurve_is_keyframable(FCurve *fcu)
{
  /* F-Curve's keyframes must be "usable" (i.e. visible + have an effect on final result) */
  if (BKE_fcurve_are_keyframes_usable(fcu) == 0) {
    return false;
  }

  /* F-Curve must currently be editable too */
  if (BKE_fcurve_is_protected(fcu)) {
    return false;
  }

  /* F-Curve is keyframable */
  return true;
}

/* ***************************** Keyframe Column Tools ********************************* */

/* add a BezTriple to a column */
void bezt_add_to_cfra_elem(ListBase *lb, BezTriple *bezt)
{
  CfraElem *ce, *cen;

  for (ce = lb->first; ce; ce = ce->next) {
    /* double key? */
    if (IS_EQT(ce->cfra, bezt->vec[1][0], BEZT_BINARYSEARCH_THRESH)) {
      if (bezt->f2 & SELECT) {
        ce->sel = bezt->f2;
      }
      return;
    }
    /* should key be inserted before this column? */
    else if (ce->cfra > bezt->vec[1][0]) {
      break;
    }
  }

  /* create a new column */
  cen = MEM_callocN(sizeof(CfraElem), "add_to_cfra_elem");
  if (ce) {
    BLI_insertlinkbefore(lb, ce, cen);
  }
  else {
    BLI_addtail(lb, cen);
  }

  cen->cfra = bezt->vec[1][0];
  cen->sel = bezt->f2;
}

/* ***************************** Samples Utilities ******************************* */
/* Some utilities for working with FPoints (i.e. 'sampled' animation curve data, such as
 * data imported from BVH/Mocap files), which are specialized for use with high density datasets,
 * which BezTriples/Keyframe data are ill equipped to do.
 */

/* Basic sampling callback which acts as a wrapper for evaluate_fcurve()
 * 'data' arg here is unneeded here...
 */
float fcurve_samplingcb_evalcurve(FCurve *fcu, void *UNUSED(data), float evaltime)
{
  /* assume any interference from drivers on the curve is intended... */
  return evaluate_fcurve(fcu, evaltime);
}

/* Main API function for creating a set of sampled curve data, given some callback function
 * used to retrieve the values to store.
 */
void fcurve_store_samples(FCurve *fcu, void *data, int start, int end, FcuSampleFunc sample_cb)
{
  FPoint *fpt, *new_fpt;
  int cfra;

  /* sanity checks */
  /* TODO: make these tests report errors using reports not CLOG's */
  if (ELEM(NULL, fcu, sample_cb)) {
    CLOG_ERROR(&LOG, "No F-Curve with F-Curve Modifiers to Bake");
    return;
  }
  if (start > end) {
    CLOG_ERROR(&LOG, "Error: Frame range for Sampled F-Curve creation is inappropriate");
    return;
  }

  /* set up sample data */
  fpt = new_fpt = MEM_callocN(sizeof(FPoint) * (end - start + 1), "FPoint Samples");

  /* use the sampling callback at 1-frame intervals from start to end frames */
  for (cfra = start; cfra <= end; cfra++, fpt++) {
    fpt->vec[0] = (float)cfra;
    fpt->vec[1] = sample_cb(fcu, data, (float)cfra);
  }

  /* free any existing sample/keyframe data on curve  */
  if (fcu->bezt) {
    MEM_freeN(fcu->bezt);
  }
  if (fcu->fpt) {
    MEM_freeN(fcu->fpt);
  }

  /* store the samples */
  fcu->bezt = NULL;
  fcu->fpt = new_fpt;
  fcu->totvert = end - start + 1;
}

/* ***************************** F-Curve Sanity ********************************* */
/* The functions here are used in various parts of Blender, usually after some editing
 * of keyframe data has occurred. They ensure that keyframe data is properly ordered and
 * that the handles are correctly
 */

/* Checks if the F-Curve has a Cycles modifier, and returns the type of the cycle behavior. */
eFCU_Cycle_Type BKE_fcurve_get_cycle_type(FCurve *fcu)
{
  FModifier *fcm = fcu->modifiers.first;

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
    if (data->before_mode == FCM_EXTRAPOLATE_CYCLIC &&
        data->after_mode == FCM_EXTRAPOLATE_CYCLIC) {
      return FCU_CYCLE_PERFECT;
    }

    if (ELEM(data->before_mode, FCM_EXTRAPOLATE_CYCLIC, FCM_EXTRAPOLATE_CYCLIC_OFFSET) &&
        ELEM(data->after_mode, FCM_EXTRAPOLATE_CYCLIC, FCM_EXTRAPOLATE_CYCLIC_OFFSET)) {
      return FCU_CYCLE_OFFSET;
    }
  }

  return FCU_CYCLE_NONE;
}

/* Checks if the F-Curve has a Cycles modifier with simple settings
 * that warrant transition smoothing. */
bool BKE_fcurve_is_cyclic(FCurve *fcu)
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
    return NULL;
  }

  memcpy(out, in, sizeof(BezTriple));

  float delta[3];
  sub_v3_v3v3(delta, to->vec[1], from->vec[1]);

  for (int i = 0; i < 3; i++) {
    add_v3_v3(out->vec[i], delta);
  }

  return out;
}

/**
 * Variant of #calchandles_fcurve() that allows calculating based on a different select flag.
 *
 * \param sel_flag: The flag (bezt.f1/2/3) value to use to determine selection. Usually `SELECT`,
 *                  but may want to use a different one at times (if caller does not operate on
 *                  selection).
 */
void calchandles_fcurve_ex(FCurve *fcu, eBezTriple_Flag handle_sel_flag)
{
  BezTriple *bezt, *prev, *next;
  int a = fcu->totvert;

  /* Error checking:
   * - need at least two points
   * - need bezier keys
   * - only bezier-interpolation has handles (for now)
   */
  if (ELEM(NULL, fcu, fcu->bezt) || (a < 2) /*|| ELEM(fcu->ipo, BEZT_IPO_CONST, BEZT_IPO_LIN)*/) {
    return;
  }

  /* if the first modifier is Cycles, smooth the curve through the cycle */
  BezTriple *first = &fcu->bezt[0], *last = &fcu->bezt[fcu->totvert - 1];
  BezTriple tmp;

  bool cycle = BKE_fcurve_is_cyclic(fcu) && BEZT_IS_AUTOH(first) && BEZT_IS_AUTOH(last);

  /* get initial pointers */
  bezt = fcu->bezt;
  prev = cycle_offset_triple(cycle, &tmp, &fcu->bezt[fcu->totvert - 2], last, first);
  next = (bezt + 1);

  /* loop over all beztriples, adjusting handles */
  while (a--) {
    /* clamp timing of handles to be on either side of beztriple */
    if (bezt->vec[0][0] > bezt->vec[1][0]) {
      bezt->vec[0][0] = bezt->vec[1][0];
    }
    if (bezt->vec[2][0] < bezt->vec[1][0]) {
      bezt->vec[2][0] = bezt->vec[1][0];
    }

    /* calculate auto-handles */
    BKE_nurb_handle_calc_ex(bezt, prev, next, handle_sel_flag, true, fcu->auto_smoothing);

    /* for automatic ease in and out */
    if (BEZT_IS_AUTOH(bezt) && !cycle) {
      /* only do this on first or last beztriple */
      if ((a == 0) || (a == fcu->totvert - 1)) {
        /* set both handles to have same horizontal value as keyframe */
        if (fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) {
          bezt->vec[0][1] = bezt->vec[2][1] = bezt->vec[1][1];
          /* remember that these keyframes are special, they don't need to be adjusted */
          bezt->f5 = HD_AUTOTYPE_SPECIAL;
        }
      }
    }

    /* avoid total smoothing failure on duplicate keyframes (can happen during grab) */
    if (prev && prev->vec[1][0] >= bezt->vec[1][0]) {
      prev->f5 = bezt->f5 = HD_AUTOTYPE_SPECIAL;
    }

    /* advance pointers for next iteration */
    prev = bezt;

    if (a == 1) {
      next = cycle_offset_triple(cycle, &tmp, &fcu->bezt[1], first, last);
    }
    else {
      next++;
    }

    bezt++;
  }

  /* if cyclic extrapolation and Auto Clamp has triggered, ensure it is symmetric */
  if (cycle && (first->f5 != HD_AUTOTYPE_NORMAL || last->f5 != HD_AUTOTYPE_NORMAL)) {
    first->vec[0][1] = first->vec[2][1] = first->vec[1][1];
    last->vec[0][1] = last->vec[2][1] = last->vec[1][1];
    first->f5 = last->f5 = HD_AUTOTYPE_SPECIAL;
  }

  /* do a second pass for auto handle: compute the handle to have 0 acceleration step */
  if (fcu->auto_smoothing != FCURVE_SMOOTH_NONE) {
    BKE_nurb_handle_smooth_fcurve(fcu->bezt, fcu->totvert, cycle);
  }
}

/**
 * This function recalculates the handles of an F-Curve. Acts based on selection with `SELECT`
 * flag. To use a different flag, use #calchandles_fcurve_ex().
 *
 * If the BezTriples have been rearranged, sort them first before using this.
 */
void calchandles_fcurve(FCurve *fcu)
{
  calchandles_fcurve_ex(fcu, SELECT);
}

/**
 * Update handles, making sure the handle-types are valid (e.g. correctly deduced from an "Auto"
 * type), and recalculating their position vectors.
 * Use when something has changed handle positions.
 *
 * \param sel_flag: The flag (bezt.f1/2/3) value to use to determine selection. Usually `SELECT`,
 *                  but may want to use a different one at times (if caller does not operate on
 *                  selection).
 * \param use_handle: Check selection state of individual handles, otherwise always update both
 *                    handles if the key is selected.
 */
void testhandles_fcurve(FCurve *fcu, eBezTriple_Flag sel_flag, const bool use_handle)
{
  BezTriple *bezt;
  unsigned int a;

  /* only beztriples have handles (bpoints don't though) */
  if (ELEM(NULL, fcu, fcu->bezt)) {
    return;
  }

  /* loop over beztriples */
  for (a = 0, bezt = fcu->bezt; a < fcu->totvert; a++, bezt++) {
    BKE_nurb_bezt_handle_test(bezt, sel_flag, use_handle, false);
  }

  /* recalculate handles */
  calchandles_fcurve_ex(fcu, sel_flag);
}

/* This function sorts BezTriples so that they are arranged in chronological order,
 * as tools working on F-Curves expect that the BezTriples are in order.
 */
void sort_time_fcurve(FCurve *fcu)
{
  if (fcu->bezt == NULL) {
    return;
  }

  /* keep adjusting order of beztriples until nothing moves (bubble-sort) */
  BezTriple *bezt;
  uint a;

  bool ok = true;
  while (ok) {
    ok = 0;
    /* currently, will only be needed when there are beztriples */

    /* loop over ALL points to adjust position in array and recalculate handles */
    for (a = 0, bezt = fcu->bezt; a < fcu->totvert; a++, bezt++) {
      /* check if thee's a next beztriple which we could try to swap with current */
      if (a < (fcu->totvert - 1)) {
        /* swap if one is after the other (and indicate that order has changed) */
        if (bezt->vec[1][0] > (bezt + 1)->vec[1][0]) {
          SWAP(BezTriple, *bezt, *(bezt + 1));
          ok = 1;
        }
      }
    }
  }

  for (a = 0, bezt = fcu->bezt; a < fcu->totvert; a++, bezt++) {
    /* if either one of both of the points exceeds crosses over the keyframe time... */
    if ((bezt->vec[0][0] > bezt->vec[1][0]) && (bezt->vec[2][0] < bezt->vec[1][0])) {
      /* swap handles if they have switched sides for some reason */
      swap_v2_v2(bezt->vec[0], bezt->vec[2]);
    }
    else {
      /* clamp handles */
      CLAMP_MAX(bezt->vec[0][0], bezt->vec[1][0]);
      CLAMP_MIN(bezt->vec[2][0], bezt->vec[1][0]);
    }
  }
}

/* This function tests if any BezTriples are out of order, thus requiring a sort */
short test_time_fcurve(FCurve *fcu)
{
  unsigned int a;

  /* sanity checks */
  if (fcu == NULL) {
    return 0;
  }

  /* currently, only need to test beztriples */
  if (fcu->bezt) {
    BezTriple *bezt;

    /* loop through all BezTriples, stopping when one exceeds the one after it */
    for (a = 0, bezt = fcu->bezt; a < (fcu->totvert - 1); a++, bezt++) {
      if (bezt->vec[1][0] > (bezt + 1)->vec[1][0]) {
        return 1;
      }
    }
  }
  else if (fcu->fpt) {
    FPoint *fpt;

    /* loop through all FPoints, stopping when one exceeds the one after it */
    for (a = 0, fpt = fcu->fpt; a < (fcu->totvert - 1); a++, fpt++) {
      if (fpt->vec[0] > (fpt + 1)->vec[0]) {
        return 1;
      }
    }
  }

  /* none need any swapping */
  return 0;
}

/* ***************************** Curve Calculations ********************************* */

/* The total length of the handles is not allowed to be more
 * than the horizontal distance between (v1-v4).
 * This is to prevent curve loops.
 */
void correct_bezpart(float v1[2], float v2[2], float v3[2], float v4[2])
{
  float h1[2], h2[2], len1, len2, len, fac;

  /* calculate handle deltas */
  h1[0] = v1[0] - v2[0];
  h1[1] = v1[1] - v2[1];

  h2[0] = v4[0] - v3[0];
  h2[1] = v4[1] - v3[1];

  /* calculate distances:
   * - len  = span of time between keyframes
   * - len1 = length of handle of start key
   * - len2 = length of handle of end key
   */
  len = v4[0] - v1[0];
  len1 = fabsf(h1[0]);
  len2 = fabsf(h2[0]);

  /* if the handles have no length, no need to do any corrections */
  if ((len1 + len2) == 0.0f) {
    return;
  }

  /* the two handles cross over each other, so force them
   * apart using the proportion they overlap
   */
  if ((len1 + len2) > len) {
    fac = len / (len1 + len2);

    v2[0] = (v1[0] - fac * h1[0]);
    v2[1] = (v1[1] - fac * h1[1]);

    v3[0] = (v4[0] - fac * h2[0]);
    v3[1] = (v4[1] - fac * h2[1]);
  }
}

/* find root ('zero') */
static int findzero(float x, float q0, float q1, float q2, float q3, float *o)
{
  double c0, c1, c2, c3, a, b, c, p, q, d, t, phi;
  int nr = 0;

  c0 = q0 - x;
  c1 = 3.0f * (q1 - q0);
  c2 = 3.0f * (q0 - 2.0f * q1 + q2);
  c3 = q3 - q0 + 3.0f * (q1 - q2);

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
      o[0] = (float)(sqrt3d(-q + t) + sqrt3d(-q - t) - a);

      if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
        return 1;
      }
      return 0;
    }

    if (d == 0.0) {
      t = sqrt3d(-q);
      o[0] = (float)(2 * t - a);

      if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
        nr++;
      }
      o[nr] = (float)(-t - a);

      if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) {
        return nr + 1;
      }
      return nr;
    }

    phi = acos(-q / sqrt(-(p * p * p)));
    t = sqrt(-p);
    p = cos(phi / 3);
    q = sqrt(3 - 3 * p * p);
    o[0] = (float)(2 * t * p - a);

    if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
      nr++;
    }
    o[nr] = (float)(-t * (p + q) - a);

    if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) {
      nr++;
    }
    o[nr] = (float)(-t * (p - q) - a);

    if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) {
      return nr + 1;
    }
    return nr;
  }
  a = c2;
  b = c1;
  c = c0;

  if (a != 0.0) {
    /* discriminant */
    p = b * b - 4 * a * c;

    if (p > 0) {
      p = sqrt(p);
      o[0] = (float)((-b - p) / (2 * a));

      if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
        nr++;
      }
      o[nr] = (float)((-b + p) / (2 * a));

      if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) {
        return nr + 1;
      }
      return nr;
    }

    if (p == 0) {
      o[0] = (float)(-b / (2 * a));
      if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
        return 1;
      }
    }

    return 0;
  }

  if (b != 0.0) {
    o[0] = (float)(-c / b);

    if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
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

/* -------------------------- */

static float fcurve_eval_keyframes_extrapolate(
    FCurve *fcu, BezTriple *bezts, float evaltime, int endpoint_offset, int direction_to_neighbor)
{
  BezTriple *endpoint_bezt = bezts + endpoint_offset; /* The first/last keyframe. */
  BezTriple *neighbor_bezt = endpoint_bezt +
                             direction_to_neighbor; /* The second (to last) keyframe. */

  if (endpoint_bezt->ipo == BEZT_IPO_CONST || fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT ||
      (fcu->flag & FCURVE_DISCRETE_VALUES) != 0) {
    /* Constant (BEZT_IPO_HORIZ) extrapolation or constant interpolation, so just extend the
     * endpoint's value. */
    return endpoint_bezt->vec[1][1];
  }

  if (endpoint_bezt->ipo == BEZT_IPO_LIN) {
    /* Use the next center point instead of our own handle for linear interpolated extrapolate. */
    if (fcu->totvert == 1) {
      return endpoint_bezt->vec[1][1];
    }

    float dx = endpoint_bezt->vec[1][0] - evaltime;
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
  int handle = direction_to_neighbor > 0 ? 0 : 2;
  float dx = endpoint_bezt->vec[1][0] - evaltime;
  float fac = endpoint_bezt->vec[1][0] - endpoint_bezt->vec[handle][0];

  /* Prevent division by zero. */
  if (fac == 0.0f) {
    return endpoint_bezt->vec[1][1];
  }

  fac = (endpoint_bezt->vec[1][1] - endpoint_bezt->vec[handle][1]) / fac;
  return endpoint_bezt->vec[1][1] - (fac * dx);
}

static float fcurve_eval_keyframes_interpolate(FCurve *fcu, BezTriple *bezts, float evaltime)
{
  const float eps = 1.e-8f;
  BezTriple *bezt, *prevbezt;
  unsigned int a;

  /* evaltime occurs somewhere in the middle of the curve */
  bool exact = false;

  /* Use binary search to find appropriate keyframes...
   *
   * The threshold here has the following constraints:
   * - 0.001 is too coarse:
   *   We get artifacts with 2cm driver movements at 1BU = 1m (see T40332)
   *
   * - 0.00001 is too fine:
   *   Weird errors, like selecting the wrong keyframe range (see T39207), occur.
   *   This lower bound was established in b888a32eee8147b028464336ad2404d8155c64dd.
   */
  a = binarysearch_bezt_index_ex(bezts, evaltime, fcu->totvert, 0.0001, &exact);
  bezt = bezts + a;

  if (exact) {
    /* index returned must be interpreted differently when it sits on top of an existing keyframe
     * - that keyframe is the start of the segment we need (see action_bug_2.blend in T39207)
     */
    return bezt->vec[1][1];
  }

  /* index returned refers to the keyframe that the eval-time occurs *before*
   * - hence, that keyframe marks the start of the segment we're dealing with
   */
  prevbezt = (a > 0) ? (bezt - 1) : bezt;

  /* Use if the key is directly on the frame, in rare cases this is needed else we get 0.0 instead.
   * XXX: consult T39207 for examples of files where failure of these checks can cause issues */
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

  /* value depends on interpolation mode */
  if ((prevbezt->ipo == BEZT_IPO_CONST) || (fcu->flag & FCURVE_DISCRETE_VALUES) ||
      (duration == 0)) {
    /* constant (evaltime not relevant, so no interpolation needed) */
    return prevbezt->vec[1][1];
  }

  switch (prevbezt->ipo) {
    /* interpolation ...................................... */
    case BEZT_IPO_BEZ: {
      float v1[2], v2[2], v3[2], v4[2], opl[32];

      /* bezier interpolation */
      /* (v1, v2) are the first keyframe and its 2nd handle */
      v1[0] = prevbezt->vec[1][0];
      v1[1] = prevbezt->vec[1][1];
      v2[0] = prevbezt->vec[2][0];
      v2[1] = prevbezt->vec[2][1];
      /* (v3, v4) are the last keyframe's 1st handle + the last keyframe */
      v3[0] = bezt->vec[0][0];
      v3[1] = bezt->vec[0][1];
      v4[0] = bezt->vec[1][0];
      v4[1] = bezt->vec[1][1];

      if (fabsf(v1[1] - v4[1]) < FLT_EPSILON && fabsf(v2[1] - v3[1]) < FLT_EPSILON &&
          fabsf(v3[1] - v4[1]) < FLT_EPSILON) {
        /* Optimization: If all the handles are flat/at the same values,
         * the value is simply the shared value (see T40372 -> F91346)
         */
        return v1[1];
      }
      /* adjust handles so that they don't overlap (forming a loop) */
      correct_bezpart(v1, v2, v3, v4);

      /* try to get a value for this position - if failure, try another set of points */
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
      /* linear - simply linearly interpolate between values of the two keyframes */
      return BLI_easing_linear_ease(time, begin, change, duration);

    /* easing ............................................ */
    case BEZT_IPO_BACK:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_back_ease_in(time, begin, change, duration, prevbezt->back);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_back_ease_out(time, begin, change, duration, prevbezt->back);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_back_ease_in_out(time, begin, change, duration, prevbezt->back);

        default: /* default/auto: same as ease out */
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

        default: /* default/auto: same as ease out */
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

        default: /* default/auto: same as ease in */
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

        default: /* default/auto: same as ease in */
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

        default: /* default/auto: same as ease out */
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

        default: /* default/auto: same as ease in */
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

        default: /* default/auto: same as ease in */
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

        default: /* default/auto: same as ease in */
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

        default: /* default/auto: same as ease in */
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

        default: /* default/auto: same as ease in */
          return BLI_easing_sine_ease_in(time, begin, change, duration);
      }
      break;

    default:
      return prevbezt->vec[1][1];
  }

  return 0.0f;
}

/* Calculate F-Curve value for 'evaltime' using BezTriple keyframes */
static float fcurve_eval_keyframes(FCurve *fcu, BezTriple *bezts, float evaltime)
{
  if (evaltime <= bezts->vec[1][0]) {
    return fcurve_eval_keyframes_extrapolate(fcu, bezts, evaltime, 0, +1);
  }

  BezTriple *lastbezt = bezts + fcu->totvert - 1;
  if (lastbezt->vec[1][0] <= evaltime) {
    return fcurve_eval_keyframes_extrapolate(fcu, bezts, evaltime, fcu->totvert - 1, -1);
  }

  return fcurve_eval_keyframes_interpolate(fcu, bezts, evaltime);
}

/* Calculate F-Curve value for 'evaltime' using FPoint samples */
static float fcurve_eval_samples(FCurve *fcu, FPoint *fpts, float evaltime)
{
  FPoint *prevfpt, *lastfpt, *fpt;
  float cvalue = 0.0f;

  /* get pointers */
  prevfpt = fpts;
  lastfpt = prevfpt + fcu->totvert - 1;

  /* evaluation time at or past endpoints? */
  if (prevfpt->vec[0] >= evaltime) {
    /* before or on first sample, so just extend value */
    cvalue = prevfpt->vec[1];
  }
  else if (lastfpt->vec[0] <= evaltime) {
    /* after or on last sample, so just extend value */
    cvalue = lastfpt->vec[1];
  }
  else {
    float t = fabsf(evaltime - floorf(evaltime));

    /* find the one on the right frame (assume that these are spaced on 1-frame intervals) */
    fpt = prevfpt + ((int)evaltime - (int)prevfpt->vec[0]);

    /* if not exactly on the frame, perform linear interpolation with the next one */
    if ((t != 0.0f) && (t < 1.0f)) {
      cvalue = interpf(fpt->vec[1], (fpt + 1)->vec[1], 1.0f - t);
    }
    else {
      cvalue = fpt->vec[1];
    }
  }

  /* return value */
  return cvalue;
}

/* ***************************** F-Curve - Evaluation ********************************* */

/* Evaluate and return the value of the given F-Curve at the specified frame ("evaltime")
 * Note: this is also used for drivers
 */
static float evaluate_fcurve_ex(FCurve *fcu, float evaltime, float cvalue)
{
  float devaltime;

  /* evaluate modifiers which modify time to evaluate the base curve at */
  FModifiersStackStorage storage;
  storage.modifier_count = BLI_listbase_count(&fcu->modifiers);
  storage.size_per_modifier = evaluate_fmodifiers_storage_size_per_modifier(&fcu->modifiers);
  storage.buffer = alloca(storage.modifier_count * storage.size_per_modifier);

  devaltime = evaluate_time_fmodifiers(&storage, &fcu->modifiers, fcu, cvalue, evaltime);

  /* evaluate curve-data
   * - 'devaltime' instead of 'evaltime', as this is the time that the last time-modifying
   *   F-Curve modifier on the stack requested the curve to be evaluated at
   */
  if (fcu->bezt) {
    cvalue = fcurve_eval_keyframes(fcu, fcu->bezt, devaltime);
  }
  else if (fcu->fpt) {
    cvalue = fcurve_eval_samples(fcu, fcu->fpt, devaltime);
  }

  /* evaluate modifiers */
  evaluate_value_fmodifiers(&storage, &fcu->modifiers, fcu, &cvalue, devaltime);

  /* if curve can only have integral values, perform truncation (i.e. drop the decimal part)
   * here so that the curve can be sampled correctly
   */
  if (fcu->flag & FCURVE_INT_VALUES) {
    cvalue = floorf(cvalue + 0.5f);
  }

  /* return evaluated value */
  return cvalue;
}

float evaluate_fcurve(FCurve *fcu, float evaltime)
{
  BLI_assert(fcu->driver == NULL);

  return evaluate_fcurve_ex(fcu, evaltime, 0.0);
}

float evaluate_fcurve_only_curve(FCurve *fcu, float evaltime)
{
  /* Can be used to evaluate the (keyframed) fcurve only.
   * Also works for driver-fcurves when the driver itself is not relevant.
   * E.g. when inserting a keyframe in a driver fcurve. */
  return evaluate_fcurve_ex(fcu, evaltime, 0.0);
}

float evaluate_fcurve_driver(PathResolvedRNA *anim_rna,
                             FCurve *fcu,
                             ChannelDriver *driver_orig,
                             float evaltime)
{
  BLI_assert(fcu->driver != NULL);
  float cvalue = 0.0f;

  /* If there is a driver (only if this F-Curve is acting as 'driver'),
   * evaluate it to find value to use as "evaltime" since drivers essentially act as alternative
   * input (i.e. in place of 'time') for F-Curves. */
  if (fcu->driver) {
    /* evaltime now serves as input for the curve */
    evaltime = evaluate_driver(anim_rna, fcu->driver, driver_orig, evaltime);

    /* only do a default 1-1 mapping if it's unlikely that anything else will set a value... */
    if (fcu->totvert == 0) {
      FModifier *fcm;
      bool do_linear = true;

      /* out-of-range F-Modifiers will block, as will those which just plain overwrite the values
       * XXX: additive is a bit more dicey; it really depends then if things are in range or not...
       */
      for (fcm = fcu->modifiers.first; fcm; fcm = fcm->next) {
        /* if there are range-restrictions, we must definitely block [#36950] */
        if ((fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) == 0 ||
            ((fcm->sfra <= evaltime) && (fcm->efra >= evaltime))) {
          /* Within range: here it probably doesn't matter,
           * though we'd want to check on additive. */
        }
        else {
          /* Outside range: modifier shouldn't contribute to the curve here,
           * though it does in other areas, so neither should the driver! */
          do_linear = false;
        }
      }

      /* only copy over results if none of the modifiers disagreed with this */
      if (do_linear) {
        cvalue = evaltime;
      }
    }
  }

  return evaluate_fcurve_ex(fcu, evaltime, cvalue);
}

/* Checks if the curve has valid keys, drivers or modifiers that produce an actual curve. */
bool BKE_fcurve_is_empty(FCurve *fcu)
{
  return (fcu->totvert == 0) && (fcu->driver == NULL) &&
         !list_has_suitable_fmodifier(&fcu->modifiers, 0, FMI_TYPE_GENERATE_CURVE);
}

/* Calculate the value of the given F-Curve at the given frame, and set its curval */
float calculate_fcurve(PathResolvedRNA *anim_rna, FCurve *fcu, float evaltime)
{
  /* only calculate + set curval (overriding the existing value) if curve has
   * any data which warrants this...
   */
  if (BKE_fcurve_is_empty(fcu)) {
    return 0.0f;
  }

  /* calculate and set curval (evaluates driver too if necessary) */
  float curval;
  if (fcu->driver) {
    curval = evaluate_fcurve_driver(anim_rna, fcu, fcu->driver, evaltime);
  }
  else {
    curval = evaluate_fcurve(fcu, evaltime);
  }
  fcu->curval = curval; /* debug display only, not thread safe! */
  return curval;
}
