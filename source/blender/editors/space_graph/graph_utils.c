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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spgraph
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_screen.h"

#include "WM_api.h"

#include "ED_anim_api.h"
#include "ED_screen.h"
#include "UI_interface.h"

#include "RNA_access.h"

#include "graph_intern.h"  // own include

/* ************************************************************** */
/* Set Up Drivers Editor */

/* Set up UI configuration for Drivers Editor */
/* NOTE: Currently called from window-manager
 * (new drivers editor window) and RNA (mode switching) */
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

/* ************************************************************** */
/* Active F-Curve */

/**
 * Find 'active' F-Curve.
 * It must be editable, since that's the purpose of these buttons (subject to change).
 * We return the 'wrapper' since it contains valuable context info (about hierarchy),
 * which will need to be freed when the caller is done with it.
 *
 * \note curve-visible flag isn't included,
 * otherwise selecting a curve via list to edit is too cumbersome.
 */
bAnimListElem *get_active_fcurve_channel(bAnimContext *ac)
{
  ListBase anim_data = {NULL, NULL};
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_ACTIVE);
  size_t items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

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
  return NULL;
}

/* ************************************************************** */
/* Operator Polling Callbacks */

/* Check if there are any visible keyframes (for selection tools) */
bool graphop_visible_keyframes_poll(bContext *C)
{
  bAnimContext ac;
  bAnimListElem *ale;
  ListBase anim_data = {NULL, NULL};
  ScrArea *area = CTX_wm_area(C);
  size_t items;
  int filter;
  short found = 0;

  /* firstly, check if in Graph Editor */
  // TODO: also check for region?
  if ((area == NULL) || (area->spacetype != SPACE_GRAPH)) {
    return 0;
  }

  /* try to init Anim-Context stuff ourselves and check */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return 0;
  }

  /* loop over the visible (selection doesn't matter) F-Curves, and see if they're suitable
   * stopping on the first successful match
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE);
  items = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
  if (items == 0) {
    return 0;
  }

  for (ale = anim_data.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->data;

    /* visible curves for selection must fulfill the following criteria:
     * - it has bezier keyframes
     * - F-Curve modifiers do not interfere with the result too much
     *   (i.e. the modifier-control drawing check returns false)
     */
    if (fcu->bezt == NULL) {
      continue;
    }
    if (BKE_fcurve_are_keyframes_usable(fcu)) {
      found = 1;
      break;
    }
  }

  /* cleanup and return findings */
  ANIM_animdata_freelist(&anim_data);
  return found;
}

/* Check if there are any visible + editable keyframes (for editing tools) */
bool graphop_editable_keyframes_poll(bContext *C)
{
  bAnimContext ac;
  bAnimListElem *ale;
  ListBase anim_data = {NULL, NULL};
  ScrArea *area = CTX_wm_area(C);
  size_t items;
  int filter;
  short found = 0;

  /* firstly, check if in Graph Editor */
  // TODO: also check for region?
  if ((area == NULL) || (area->spacetype != SPACE_GRAPH)) {
    return 0;
  }

  /* try to init Anim-Context stuff ourselves and check */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return 0;
  }

  /* loop over the editable F-Curves, and see if they're suitable
   * stopping on the first successful match
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE);
  items = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
  if (items == 0) {
    return 0;
  }

  for (ale = anim_data.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->data;

    /* editable curves must fulfill the following criteria:
     * - it has bezier keyframes
     * - it must not be protected from editing (this is already checked for with the edit flag
     * - F-Curve modifiers do not interfere with the result too much
     *   (i.e. the modifier-control drawing check returns false)
     */
    if (fcu->bezt == NULL) {
      continue;
    }
    if (BKE_fcurve_is_keyframable(fcu)) {
      found = 1;
      break;
    }
  }

  /* cleanup and return findings */
  ANIM_animdata_freelist(&anim_data);
  return found;
}

/* has active F-Curve that's editable */
bool graphop_active_fcurve_poll(bContext *C)
{
  bAnimContext ac;
  bAnimListElem *ale;
  ScrArea *area = CTX_wm_area(C);
  bool has_fcurve = 0;

  /* firstly, check if in Graph Editor */
  // TODO: also check for region?
  if ((area == NULL) || (area->spacetype != SPACE_GRAPH)) {
    return 0;
  }

  /* try to init Anim-Context stuff ourselves and check */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return 0;
  }

  /* try to get the Active F-Curve */
  ale = get_active_fcurve_channel(&ac);
  if (ale == NULL) {
    return 0;
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

/* has active F-Curve in the context that's editable */
bool graphop_active_editable_fcurve_ctx_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "active_editable_fcurve", &RNA_FCurve);

  return ptr.data != NULL;
}

/* has selected F-Curve that's editable */
bool graphop_selected_fcurve_poll(bContext *C)
{
  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  ScrArea *area = CTX_wm_area(C);
  size_t items;
  int filter;

  /* firstly, check if in Graph Editor */
  // TODO: also check for region?
  if ((area == NULL) || (area->spacetype != SPACE_GRAPH)) {
    return 0;
  }

  /* try to init Anim-Context stuff ourselves and check */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return 0;
  }

  /* Get the editable + selected F-Curves, and as long as we got some, we can return.
   * NOTE: curve-visible flag isn't included,
   * otherwise selecting a curve via list to edit is too cumbersome. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT);
  items = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
  if (items == 0) {
    return 0;
  }

  /* cleanup and return findings */
  ANIM_animdata_freelist(&anim_data);
  return 1;
}

/* ************************************************************** */
