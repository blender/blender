/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_screen.h"

#include "ED_anim_api.hh"
#include "ED_screen.hh"
#include "UI_interface.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "graph_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Set Up Drivers Editor
 * \{ */

void ED_drivers_editor_init(bContext *C, ScrArea *area)
{
  SpaceGraph *sipo = (SpaceGraph *)area->spacedata.first;

  /* Set mode */
  sipo->mode = SIPO_MODE_DRIVERS;

  /* Show Properties Region (or else the settings can't be edited) */
  ARegion *region_props = BKE_area_find_region_type(area, RGN_TYPE_UI);
  if (region_props) {
    UI_panel_category_active_set(region_props, "Drivers");

    region_props->flag &= ~RGN_FLAG_HIDDEN;
    /* XXX: Adjust width of this too? */

    ED_region_visibility_change_update(C, area, region_props);
  }
  else {
    printf("%s: Couldn't find properties region for Drivers Editor - %p\n", __func__, area);
  }

  /* Adjust framing in graph region */
  /* TODO: Have a way of not resetting this every time?
   * (e.g. So that switching back and forth between editors doesn't keep jumping?)
   */
  ARegion *region_main = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (region_main) {
    /* XXX: Ideally we recenter based on the range instead... */
    region_main->v2d.tot.xmin = -2.0f;
    region_main->v2d.tot.ymin = -2.0f;
    region_main->v2d.tot.xmax = 2.0f;
    region_main->v2d.tot.ymax = 2.0f;

    region_main->v2d.cur = region_main->v2d.tot;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Active F-Curve
 * \{ */

bAnimListElem *get_active_fcurve_channel(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_ACTIVE |
                ANIMFILTER_FCURVESONLY);
  size_t items = ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* We take the first F-Curve only, since some other ones may have had 'active' flag set
   * if they were from linked data.
   */
  if (items) {
    bAnimListElem *ale = (bAnimListElem *)anim_data.first;

    /* remove first item from list, then free the rest of the list and return the stored one */
    BLI_remlink(&anim_data, ale);
    ANIM_animdata_freelist(&anim_data);

    return ale;
  }

  /* no active F-Curve */
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Polling Callbacks
 * \{ */

bool graphop_visible_keyframes_poll(bContext *C)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  ScrArea *area = CTX_wm_area(C);
  size_t items;
  int filter;
  bool found = false;

  /* firstly, check if in Graph Editor */
  /* TODO: also check for region? */
  if ((area == nullptr) || (area->spacetype != SPACE_GRAPH)) {
    return found;
  }

  /* try to init Anim-Context stuff ourselves and check */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return found;
  }

  /* loop over the visible (selection doesn't matter) F-Curves, and see if they're suitable
   * stopping on the first successful match
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY);
  items = ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));
  if (items == 0) {
    return found;
  }

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->data;

    /* visible curves for selection must fulfill the following criteria:
     * - it has bezier keyframes
     * - F-Curve modifiers do not interfere with the result too much
     *   (i.e. the modifier-control drawing check returns false)
     */
    if (fcu->bezt == nullptr) {
      continue;
    }
    if (BKE_fcurve_are_keyframes_usable(fcu)) {
      found = true;
      break;
    }
  }

  /* cleanup and return findings */
  ANIM_animdata_freelist(&anim_data);
  return found;
}

bool graphop_editable_keyframes_poll(bContext *C)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  ScrArea *area = CTX_wm_area(C);
  size_t items;
  int filter;
  bool found = false;

  /* firstly, check if in Graph Editor or Dopesheet */
  /* TODO: also check for region? */
  if (area == nullptr || !ELEM(area->spacetype, SPACE_GRAPH, SPACE_ACTION)) {
    return found;
  }

  /* try to init Anim-Context stuff ourselves and check */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return found;
  }

  /* loop over the editable F-Curves, and see if they're suitable
   * stopping on the first successful match
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE |
            ANIMFILTER_FCURVESONLY);
  items = ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));
  if (items == 0) {
    CTX_wm_operator_poll_msg_set(C, "There is no animation data to operate on");
    return found;
  }

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->data;

    /* editable curves must fulfill the following criteria:
     * - it has bezier keyframes
     * - it must not be protected from editing (this is already checked for with the edit flag
     * - F-Curve modifiers do not interfere with the result too much
     *   (i.e. the modifier-control drawing check returns false)
     */
    if (fcu->bezt == nullptr && fcu->fpt != nullptr) {
      /* This is a baked curve, it is never editable. */
      continue;
    }
    if (BKE_fcurve_is_keyframable(fcu)) {
      found = true;
      break;
    }
  }

  /* cleanup and return findings */
  ANIM_animdata_freelist(&anim_data);
  return found;
}

bool graphop_active_fcurve_poll(bContext *C)
{
  bAnimContext ac;
  bAnimListElem *ale;
  ScrArea *area = CTX_wm_area(C);
  bool has_fcurve = false;

  /* firstly, check if in Graph Editor */
  /* TODO: also check for region? */
  if ((area == nullptr) || (area->spacetype != SPACE_GRAPH)) {
    return has_fcurve;
  }

  /* try to init Anim-Context stuff ourselves and check */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return has_fcurve;
  }

  /* try to get the Active F-Curve */
  ale = get_active_fcurve_channel(&ac);
  if (ale == nullptr) {
    return has_fcurve;
  }

  /* Do we have a suitable F-Curves?
   * - For most cases, NLA Control Curves are sufficiently similar to NLA
   *   curves to serve this role too. Under the hood, they are F-Curves too.
   *   The only problems which will arise here are if these need to be
   *   in an Action too (but drivers would then also be affected!)
   */
  has_fcurve = ((ale->data) && ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE));
  if (has_fcurve) {
    FCurve *fcu = (FCurve *)ale->data;
    has_fcurve = (fcu->flag & FCURVE_VISIBLE) != 0;
  }

  /* free temp data... */
  MEM_freeN(ale);

  /* return success */
  return has_fcurve;
}

bool graphop_active_editable_fcurve_ctx_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "active_editable_fcurve", &RNA_FCurve);

  return ptr.data != nullptr;
}

bool graphop_selected_fcurve_poll(bContext *C)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  ScrArea *area = CTX_wm_area(C);
  size_t items;
  int filter;

  /* firstly, check if in Graph Editor */
  /* TODO: also check for region? */
  if ((area == nullptr) || (area->spacetype != SPACE_GRAPH)) {
    return false;
  }

  /* try to init Anim-Context stuff ourselves and check */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return false;
  }

  /* Get the editable + selected F-Curves, and as long as we got some, we can return.
   * NOTE: curve-visible flag isn't included,
   * otherwise selecting a curve via list to edit is too cumbersome. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  items = ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));
  if (items == 0) {
    return false;
  }

  /* cleanup and return findings */
  ANIM_animdata_freelist(&anim_data);
  return true;
}

/** \} */
