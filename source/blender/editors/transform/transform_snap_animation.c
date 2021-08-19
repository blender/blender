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
 * \ingroup edtransform
 */

#include "DNA_anim_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_nla.h"

#include "ED_markers.h"
#include "ED_screen.h"

#include "transform.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/** \name Snappint in Anim Editors
 * \{ */

/**
 * This function returns the snapping 'mode' for Animation Editors only.
 * We cannot use the standard snapping due to NLA-strip scaling complexities.
 *
 * TODO: these modifier checks should be key-mappable.
 */
short getAnimEdit_SnapMode(TransInfo *t)
{
  short autosnap = SACTSNAP_OFF;

  if (t->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;

    if (saction) {
      autosnap = saction->autosnap;
    }
  }
  else if (t->spacetype == SPACE_GRAPH) {
    SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;

    if (sipo) {
      autosnap = sipo->autosnap;
    }
  }
  else if (t->spacetype == SPACE_NLA) {
    SpaceNla *snla = (SpaceNla *)t->area->spacedata.first;

    if (snla) {
      autosnap = snla->autosnap;
    }
  }
  else {
    autosnap = SACTSNAP_OFF;
  }

  /* toggle autosnap on/off
   * - when toggling on, prefer nearest frame over 1.0 frame increments
   */
  if (t->modifiers & MOD_SNAP_INVERT) {
    if (autosnap) {
      autosnap = SACTSNAP_OFF;
    }
    else {
      autosnap = SACTSNAP_FRAME;
    }
  }

  return autosnap;
}

void snapFrameTransform(TransInfo *t,
                        const eAnimEdit_AutoSnap autosnap,
                        const bool is_frame_value,
                        const float delta,
                        /* return args */
                        float *r_val)
{
  double val = delta;
  switch (autosnap) {
    case SACTSNAP_STEP:
    case SACTSNAP_FRAME:
      val = floor(val + 0.5);
      break;
    case SACTSNAP_MARKER:
      /* snap to nearest marker */
      /* TODO: need some more careful checks for where data comes from. */
      val = ED_markers_find_nearest_marker_time(&t->scene->markers, (float)val);
      break;
    case SACTSNAP_SECOND:
    case SACTSNAP_TSTEP: {
      /* second step */
      const Scene *scene = t->scene;
      const double secf = FPS;
      val = floor((val / secf) + 0.5);
      if (is_frame_value) {
        val *= secf;
      }
      break;
    }
    case SACTSNAP_OFF: {
      break;
    }
  }
  *r_val = (float)val;
}

/* This function is used by Animation Editor specific transform functions to do
 * the Snap Keyframe to Nearest Frame/Marker
 */
void doAnimEdit_SnapFrame(
    TransInfo *t, TransData *td, TransData2D *td2d, AnimData *adt, short autosnap)
{
  if (autosnap != SACTSNAP_OFF) {
    float val;

    /* convert frame to nla-action time (if needed) */
    if (adt && (t->spacetype != SPACE_SEQ)) {
      val = BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
    }
    else {
      val = *(td->val);
    }

    snapFrameTransform(t, autosnap, true, val, &val);

    /* convert frame out of nla-action time */
    if (adt && (t->spacetype != SPACE_SEQ)) {
      *(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
    }
    else {
      *(td->val) = val;
    }
  }
}

/** \} */
