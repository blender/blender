/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_anim_types.h"

#include "BKE_context.h"
#include "BKE_nla.h"

#include "ED_markers.hh"
#include "ED_screen.hh"

#include "transform.hh"
#include "transform_snap.hh"

/* -------------------------------------------------------------------- */
/** \name Snapping in Anim Editors
 * \{ */

void snapFrameTransform(TransInfo *t,
                        const eSnapMode snap_mode,
                        const float val_initial,
                        const float val_final,
                        float *r_val_final)
{
  float deltax = val_final - val_initial;
  /* This is needed for the FPS macro. */
  const Scene *scene = t->scene;
  const eSnapFlag snap_flag = t->tsnap.flag;

  switch (snap_mode) {
    case SCE_SNAP_TO_FRAME: {
      if (snap_flag & SCE_SNAP_ABS_TIME_STEP) {
        *r_val_final = floorf(val_final + 0.5f);
      }
      else {
        deltax = floorf(deltax + 0.5f);
        *r_val_final = val_initial + deltax;
      }
      break;
    }
    case SCE_SNAP_TO_SECOND: {
      if (snap_flag & SCE_SNAP_ABS_TIME_STEP) {
        *r_val_final = floorf((val_final / FPS) + 0.5) * FPS;
      }
      else {
        deltax = float(floor((deltax / FPS) + 0.5) * FPS);
        *r_val_final = val_initial + deltax;
      }
      break;
    }
    case SCE_SNAP_TO_MARKERS: {
      /* Snap to nearest marker. */
      /* TODO: need some more careful checks for where data comes from. */
      const float nearest_marker_time = float(
          ED_markers_find_nearest_marker_time(&t->scene->markers, val_final));
      *r_val_final = nearest_marker_time;
      break;
    }
    default: {
      *r_val_final = val_final;
      break;
    }
  }
}

void transform_snap_anim_flush_data(TransInfo *t,
                                    TransData *td,
                                    const eSnapMode snap_mode,
                                    float *r_val_final)
{
  BLI_assert(t->tsnap.flag);

  float val = td->loc[0];
  float ival = td->iloc[0];
  AnimData *adt = static_cast<AnimData *>(!ELEM(t->spacetype, SPACE_NLA, SPACE_SEQ) ? td->extra :
                                                                                      nullptr);

  /* Convert frame to nla-action time (if needed) */
  if (adt) {
    val = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_MAP);
    ival = BKE_nla_tweakedit_remap(adt, ival, NLATIME_CONVERT_MAP);
  }

  snapFrameTransform(t, snap_mode, ival, val, &val);

  /* Convert frame out of nla-action time. */
  if (adt) {
    val = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
  }

  *r_val_final = val;
}

/** \} */
