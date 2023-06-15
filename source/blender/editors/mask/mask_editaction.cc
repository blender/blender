/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmask
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"
#include "BKE_mask.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_markers.h"
#include "ED_mask.h" /* own include */

/* ***************************************** */
/* NOTE ABOUT THIS FILE:
 * This file contains code for editing Mask data in the Action Editor
 * as a 'keyframes', so that a user can adjust the timing of Mask shape-keys.
 * Therefore, this file mostly contains functions for selecting Mask frames (shape-keys).
 */
/* ***************************************** */
/* Generics - Loopers */

bool ED_masklayer_frames_looper(MaskLayer *mask_layer,
                                Scene *scene,
                                bool (*mask_layer_shape_cb)(MaskLayerShape *, Scene *))
{
  /* error checker */
  if (mask_layer == nullptr) {
    return false;
  }

  /* do loop */
  LISTBASE_FOREACH (MaskLayerShape *, mask_layer_shape, &mask_layer->splines_shapes) {
    /* execute callback */
    if (mask_layer_shape_cb(mask_layer_shape, scene)) {
      return true;
    }
  }

  /* nothing to return */
  return false;
}

/* ****************************************** */
/* Data Conversion Tools */

void ED_masklayer_make_cfra_list(MaskLayer *mask_layer, ListBase *elems, bool onlysel)
{

  /* error checking */
  if (ELEM(nullptr, mask_layer, elems)) {
    return;
  }

  /* loop through mask-frames, adding */
  LISTBASE_FOREACH (MaskLayerShape *, mask_layer_shape, &mask_layer->splines_shapes) {
    if ((onlysel == false) || (mask_layer_shape->flag & MASK_SHAPE_SELECT)) {
      CfraElem *ce = MEM_cnew<CfraElem>("CfraElem");

      ce->cfra = float(mask_layer_shape->frame);
      ce->sel = (mask_layer_shape->flag & MASK_SHAPE_SELECT) ? 1 : 0;

      BLI_addtail(elems, ce);
    }
  }
}

/* ***************************************** */
/* Selection Tools */

bool ED_masklayer_frame_select_check(const MaskLayer *mask_layer)
{
  /* error checking */
  if (mask_layer == nullptr) {
    return 0;
  }

  /* stop at the first one found */
  LISTBASE_FOREACH (MaskLayerShape *, mask_layer_shape, &mask_layer->splines_shapes) {
    if (mask_layer_shape->flag & MASK_SHAPE_SELECT) {
      return true;
    }
  }

  /* not found */
  return false;
}

/* helper function - select mask-frame based on SELECT_* mode */
static void mask_layer_shape_select(MaskLayerShape *mask_layer_shape, short select_mode)
{
  if (mask_layer_shape == nullptr) {
    return;
  }

  switch (select_mode) {
    case SELECT_ADD:
      mask_layer_shape->flag |= MASK_SHAPE_SELECT;
      break;
    case SELECT_SUBTRACT:
      mask_layer_shape->flag &= ~MASK_SHAPE_SELECT;
      break;
    case SELECT_INVERT:
      mask_layer_shape->flag ^= MASK_SHAPE_SELECT;
      break;
  }
}

void ED_mask_select_frames(MaskLayer *mask_layer, short select_mode)
{
  /* error checking */
  if (mask_layer == nullptr) {
    return;
  }

  /* handle according to mode */
  LISTBASE_FOREACH (MaskLayerShape *, mask_layer_shape, &mask_layer->splines_shapes) {
    mask_layer_shape_select(mask_layer_shape, select_mode);
  }
}

void ED_masklayer_frame_select_set(MaskLayer *mask_layer, short mode)
{
  /* error checking */
  if (mask_layer == nullptr) {
    return;
  }

  /* now call the standard function */
  ED_mask_select_frames(mask_layer, mode);
}

void ED_mask_select_frame(MaskLayer *mask_layer, int selx, short select_mode)
{
  MaskLayerShape *mask_layer_shape;

  if (mask_layer == nullptr) {
    return;
  }

  mask_layer_shape = BKE_mask_layer_shape_find_frame(mask_layer, selx);

  if (mask_layer_shape) {
    mask_layer_shape_select(mask_layer_shape, select_mode);
  }
}

void ED_masklayer_frames_select_box(MaskLayer *mask_layer, float min, float max, short select_mode)
{
  if (mask_layer == nullptr) {
    return;
  }

  /* only select those frames which are in bounds */
  LISTBASE_FOREACH (MaskLayerShape *, mask_layer_shape, &mask_layer->splines_shapes) {
    if (IN_RANGE(mask_layer_shape->frame, min, max)) {
      mask_layer_shape_select(mask_layer_shape, select_mode);
    }
  }
}

void ED_masklayer_frames_select_region(KeyframeEditData *ked,
                                       MaskLayer *mask_layer,
                                       short tool,
                                       short select_mode)
{
  if (mask_layer == nullptr) {
    return;
  }

  /* only select frames which are within the region */
  LISTBASE_FOREACH (MaskLayerShape *, mask_layer_shape, &mask_layer->splines_shapes) {
    /* construct a dummy point coordinate to do this testing with */
    float pt[2] = {0};

    pt[0] = mask_layer_shape->frame;
    pt[1] = ked->channel_y;

    /* check the necessary regions */
    if (tool == BEZT_OK_CHANNEL_LASSO) {
      /* Lasso */
      if (keyframe_region_lasso_test(static_cast<KeyframeEdit_LassoData *>(ked->data), pt)) {
        mask_layer_shape_select(mask_layer_shape, select_mode);
      }
    }
    else if (tool == BEZT_OK_CHANNEL_CIRCLE) {
      /* Circle */
      if (keyframe_region_circle_test(static_cast<KeyframeEdit_CircleData *>(ked->data), pt)) {
        mask_layer_shape_select(mask_layer_shape, select_mode);
      }
    }
  }
}

/* ***************************************** */
/* Frame Editing Tools */

bool ED_masklayer_frames_delete(MaskLayer *mask_layer)
{
  bool changed = false;

  /* error checking */
  if (mask_layer == nullptr) {
    return false;
  }

  /* check for frames to delete */
  LISTBASE_FOREACH_MUTABLE (MaskLayerShape *, mask_layer_shape, &mask_layer->splines_shapes) {
    if (mask_layer_shape->flag & MASK_SHAPE_SELECT) {
      BKE_mask_layer_shape_unlink(mask_layer, mask_layer_shape);
      changed = true;
    }
  }

  return changed;
}

void ED_masklayer_frames_duplicate(MaskLayer *mask_layer)
{
  /* Error checking. */
  if (mask_layer == nullptr) {
    return;
  }

  /* Duplicate selected frames. */
  LISTBASE_FOREACH_MUTABLE (MaskLayerShape *, mask_layer_shape, &mask_layer->splines_shapes) {

    /* Duplicate this frame. */
    if (mask_layer_shape->flag & MASK_SHAPE_SELECT) {
      MaskLayerShape *mask_shape_dupe;

      /* Duplicate frame, and deselect self. */
      mask_shape_dupe = BKE_mask_layer_shape_duplicate(mask_layer_shape);
      mask_layer_shape->flag &= ~MASK_SHAPE_SELECT;

      /* XXX: how to handle duplicate frames? */
      BLI_insertlinkafter(&mask_layer->splines_shapes, mask_layer_shape, mask_shape_dupe);
    }
  }
}

/* -------------------------------------- */
/* Snap Tools */

static bool snap_mask_layer_nearest(MaskLayerShape *mask_layer_shape, Scene * /*scene*/)
{
  if (mask_layer_shape->flag & MASK_SHAPE_SELECT) {
    mask_layer_shape->frame = int(floor(mask_layer_shape->frame + 0.5));
  }
  return false;
}

static bool snap_mask_layer_nearestsec(MaskLayerShape *mask_layer_shape, Scene *scene)
{
  float secf = float(FPS);
  if (mask_layer_shape->flag & MASK_SHAPE_SELECT) {
    mask_layer_shape->frame = int(floorf(mask_layer_shape->frame / secf + 0.5f) * secf);
  }
  return false;
}

static bool snap_mask_layer_cframe(MaskLayerShape *mask_layer_shape, Scene *scene)
{
  if (mask_layer_shape->flag & MASK_SHAPE_SELECT) {
    mask_layer_shape->frame = int(scene->r.cfra);
  }
  return false;
}

static bool snap_mask_layer_nearmarker(MaskLayerShape *mask_layer_shape, Scene *scene)
{
  if (mask_layer_shape->flag & MASK_SHAPE_SELECT) {
    mask_layer_shape->frame = (int)ED_markers_find_nearest_marker_time(
        &scene->markers, float(mask_layer_shape->frame));
  }
  return false;
}

void ED_masklayer_snap_frames(MaskLayer *mask_layer, Scene *scene, short mode)
{
  switch (mode) {
    case SNAP_KEYS_NEARFRAME: /* snap to nearest frame */
      ED_masklayer_frames_looper(mask_layer, scene, snap_mask_layer_nearest);
      break;
    case SNAP_KEYS_CURFRAME: /* snap to current frame */
      ED_masklayer_frames_looper(mask_layer, scene, snap_mask_layer_cframe);
      break;
    case SNAP_KEYS_NEARMARKER: /* snap to nearest marker */
      ED_masklayer_frames_looper(mask_layer, scene, snap_mask_layer_nearmarker);
      break;
    case SNAP_KEYS_NEARSEC: /* snap to nearest second */
      ED_masklayer_frames_looper(mask_layer, scene, snap_mask_layer_nearestsec);
      break;
    default: /* just in case */
      break;
  }
}
