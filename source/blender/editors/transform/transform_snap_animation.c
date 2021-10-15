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
/** \name Snapping in Anim Editors
 * \{ */

/**
 * This function returns the snapping 'mode' for Animation Editors only.
 * We cannot use the standard snapping due to NLA-strip scaling complexities.
 *
 * TODO: these modifier checks should be accessible from the key-map.
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
    if ((t->mode == TFM_TRANSLATION) && activeSnap(t)) {
      return autosnap;
    }
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
                        const float val_initial,
                        const float val_final,
                        float *r_val_final)
{
  float deltax = val_final - val_initial;
  switch (autosnap) {
    case SACTSNAP_FRAME:
      *r_val_final = floorf(val_final + 0.5f);
      break;
    case SACTSNAP_MARKER:
      /* Snap to nearest marker. */
      /* TODO: need some more careful checks for where data comes from. */
      *r_val_final = (float)ED_markers_find_nearest_marker_time(&t->scene->markers, val_final);
      break;
    case SACTSNAP_SECOND:
    case SACTSNAP_TSTEP: {
      const Scene *scene = t->scene;
      const double secf = FPS;
      if (autosnap == SACTSNAP_SECOND) {
        *r_val_final = floorf((val_final / secf) + 0.5) * secf;
      }
      else {
        deltax = (float)(floor((deltax / secf) + 0.5) * secf);
        *r_val_final = val_initial + deltax;
      }
      break;
    }
    case SACTSNAP_STEP:
      deltax = floorf(deltax + 0.5f);
      *r_val_final = val_initial + deltax;
      break;
    case SACTSNAP_OFF:
      break;
  }
}

/* This function is used by Animation Editor specific transform functions to do
 * the Snap Keyframe to Nearest Frame/Marker
 */
void transform_snap_anim_flush_data(TransInfo *t,
                                    TransData *td,
                                    const eAnimEdit_AutoSnap autosnap,
                                    float *r_val_final)
{
  BLI_assert(autosnap != SACTSNAP_OFF);

  float val = td->loc[0];
  float ival = td->iloc[0];
  AnimData *adt = (!ELEM(t->spacetype, SPACE_NLA, SPACE_SEQ)) ? td->extra : NULL;

  /* Convert frame to nla-action time (if needed) */
  if (adt) {
    val = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_MAP);
    ival = BKE_nla_tweakedit_remap(adt, ival, NLATIME_CONVERT_MAP);
  }

  snapFrameTransform(t, autosnap, ival, val, &val);

  /* Convert frame out of nla-action time. */
  if (adt) {
    val = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
  }

  *r_val_final = val;
}

/** \} */
