/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_report.h"

#include "ED_anim_api.h"
#include "ED_gpencil_legacy.h"
#include "ED_keyframes_edit.h"
#include "ED_markers.h"

#include "WM_api.h"

#include "DEG_depsgraph.h"

/* ***************************************** */
/* NOTE ABOUT THIS FILE:
 * This file contains code for editing Grease Pencil data in the Action Editor
 * as a 'keyframes', so that a user can adjust the timing of Grease Pencil drawings.
 * Therefore, this file mostly contains functions for selecting Grease-Pencil frames.
 */
/* ***************************************** */
/* Generics - Loopers */

bool ED_gpencil_layer_frames_looper(bGPDlayer *gpl,
                                    Scene *scene,
                                    bool (*gpf_cb)(bGPDframe *, Scene *))
{
  /* error checker */
  if (gpl == nullptr) {
    return false;
  }

  /* do loop */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    /* execute callback */
    if (gpf_cb(gpf, scene)) {
      return true;
    }
  }

  /* nothing to return */
  return false;
}

/* ****************************************** */
/* Data Conversion Tools */

void ED_gpencil_layer_make_cfra_list(bGPDlayer *gpl, ListBase *elems, bool onlysel)
{
  CfraElem *ce;

  /* error checking */
  if (ELEM(nullptr, gpl, elems)) {
    return;
  }

  /* loop through gp-frames, adding */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if ((onlysel == 0) || (gpf->flag & GP_FRAME_SELECT)) {
      ce = static_cast<CfraElem *>(MEM_callocN(sizeof(CfraElem), "CfraElem"));

      ce->cfra = float(gpf->framenum);
      ce->sel = (gpf->flag & GP_FRAME_SELECT) ? 1 : 0;

      BLI_addtail(elems, ce);
    }
  }
}

/* ***************************************** */
/* Selection Tools */

bool ED_gpencil_layer_frame_select_check(const bGPDlayer *gpl)
{
  /* error checking */
  if (gpl == nullptr) {
    return false;
  }

  /* stop at the first one found */
  LISTBASE_FOREACH (const bGPDframe *, gpf, &gpl->frames) {
    if (gpf->flag & GP_FRAME_SELECT) {
      return true;
    }
  }

  /* not found */
  return false;
}

/* helper function - select gp-frame based on SELECT_* mode */
static void gpencil_frame_select(bGPDframe *gpf, short select_mode)
{
  if (gpf == nullptr) {
    return;
  }

  switch (select_mode) {
    case SELECT_ADD:
      gpf->flag |= GP_FRAME_SELECT;
      break;
    case SELECT_SUBTRACT:
      gpf->flag &= ~GP_FRAME_SELECT;
      break;
    case SELECT_INVERT:
      gpf->flag ^= GP_FRAME_SELECT;
      break;
  }
}

void ED_gpencil_select_frames(bGPDlayer *gpl, short select_mode)
{
  /* error checking */
  if (gpl == nullptr) {
    return;
  }

  /* handle according to mode */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    gpencil_frame_select(gpf, select_mode);
  }
}

void ED_gpencil_layer_frame_select_set(bGPDlayer *gpl, short mode)
{
  /* error checking */
  if (gpl == nullptr) {
    return;
  }

  /* now call the standard function */
  ED_gpencil_select_frames(gpl, mode);
}

void ED_gpencil_select_frame(bGPDlayer *gpl, int selx, short select_mode)
{
  bGPDframe *gpf;

  if (gpl == nullptr) {
    return;
  }

  gpf = BKE_gpencil_layer_frame_find(gpl, selx);

  if (gpf) {
    gpencil_frame_select(gpf, select_mode);
  }
}

void ED_gpencil_layer_frames_select_box(bGPDlayer *gpl, float min, float max, short select_mode)
{
  if (gpl == nullptr) {
    return;
  }

  /* only select those frames which are in bounds */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if (IN_RANGE(gpf->framenum, min, max)) {
      gpencil_frame_select(gpf, select_mode);
    }
  }
}

void ED_gpencil_layer_frames_select_region(KeyframeEditData *ked,
                                           bGPDlayer *gpl,
                                           short tool,
                                           short select_mode)
{
  if (gpl == nullptr) {
    return;
  }

  /* only select frames which are within the region */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    /* construct a dummy point coordinate to do this testing with */
    float pt[2] = {0};

    pt[0] = gpf->framenum;
    pt[1] = ked->channel_y;

    /* check the necessary regions */
    if (tool == BEZT_OK_CHANNEL_LASSO) {
      /* Lasso */
      if (keyframe_region_lasso_test(static_cast<const KeyframeEdit_LassoData *>(ked->data), pt)) {
        gpencil_frame_select(gpf, select_mode);
      }
    }
    else if (tool == BEZT_OK_CHANNEL_CIRCLE) {
      /* Circle */
      if (keyframe_region_circle_test(static_cast<const KeyframeEdit_CircleData *>(ked->data), pt))
      {
        gpencil_frame_select(gpf, select_mode);
      }
    }
  }
}

void ED_gpencil_set_active_channel(bGPdata *gpd, bGPDlayer *gpl)
{
  gpl->flag |= GP_LAYER_SELECT;

  /* Update other layer status. */
  if (BKE_gpencil_layer_active_get(gpd) != gpl) {
    BKE_gpencil_layer_active_set(gpd, gpl);
    BKE_gpencil_layer_autolock_set(gpd, false);
    WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }
}

/* ***************************************** */
/* Frame Editing Tools */

bool ED_gpencil_layer_frames_delete(bGPDlayer *gpl)
{
  bool changed = false;

  /* error checking */
  if (gpl == nullptr) {
    return false;
  }

  /* check for frames to delete */
  LISTBASE_FOREACH_MUTABLE (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->flag & GP_FRAME_SELECT) {
      BKE_gpencil_layer_frame_delete(gpl, gpf);
      changed = true;
    }
  }

  return changed;
}

void ED_gpencil_layer_frames_duplicate(bGPDlayer *gpl)
{
  /* error checking */
  if (gpl == nullptr) {
    return;
  }

  /* Duplicate selected frames. */
  LISTBASE_FOREACH_MUTABLE (bGPDframe *, gpf, &gpl->frames) {

    /* duplicate this frame */
    if (gpf->flag & GP_FRAME_SELECT) {
      bGPDframe *gpfd;

      /* duplicate frame, and deselect self */
      gpfd = BKE_gpencil_frame_duplicate(gpf, true);
      gpf->flag &= ~GP_FRAME_SELECT;

      BLI_insertlinkafter(&gpl->frames, gpf, gpfd);
    }
  }
}

void ED_gpencil_layer_frames_keytype_set(bGPDlayer *gpl, short type)
{
  if (gpl == nullptr) {
    return;
  }

  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->flag & GP_FRAME_SELECT) {
      gpf->key_type = type;
    }
  }
}

/* -------------------------------------- */
/* Copy and Paste Tools:
 * - The copy/paste buffer currently stores a set of GP_Layers, with temporary
 *   GP_Frames with the necessary strokes
 * - Unless there is only one element in the buffer,
 *   names are also tested to check for compatibility.
 * - All pasted frames are offset by the same amount.
 *   This is calculated as the difference in the times of the current frame and the
 *   'first keyframe' (i.e. the earliest one in all channels).
 * - The earliest frame is calculated per copy operation.
 */

/* globals for copy/paste data (like for other copy/paste buffers) */
static ListBase gpencil_anim_copybuf = {nullptr, nullptr};
static int gpencil_anim_copy_firstframe = 999999999;
static int gpencil_anim_copy_lastframe = -999999999;
static int gpencil_anim_copy_cfra = 0;

void ED_gpencil_anim_copybuf_free()
{
  BKE_gpencil_free_layers(&gpencil_anim_copybuf);
  BLI_listbase_clear(&gpencil_anim_copybuf);

  gpencil_anim_copy_firstframe = 999999999;
  gpencil_anim_copy_lastframe = -999999999;
  gpencil_anim_copy_cfra = 0;
}

bool ED_gpencil_anim_copybuf_copy(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  bAnimListElem *ale;
  int filter;

  Scene *scene = ac->scene;

  /* clear buffer first */
  ED_gpencil_anim_copybuf_free();

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  for (ale = static_cast<bAnimListElem *>(anim_data.first); ale; ale = ale->next) {
    /* This function only deals with grease pencil layer frames.
     * This check is needed in the case of a call from the main dopesheet. */
    if (ale->type != ANIMTYPE_GPLAYER) {
      continue;
    }

    ListBase copied_frames = {nullptr, nullptr};
    bGPDlayer *gpl = (bGPDlayer *)ale->data;

    /* loop over frames, and copy only selected frames */
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* if frame is selected, make duplicate it and its strokes */
      if (gpf->flag & GP_FRAME_SELECT) {
        /* make a copy of this frame */
        bGPDframe *new_frame = BKE_gpencil_frame_duplicate(gpf, true);
        BLI_addtail(&copied_frames, new_frame);

        /* extend extents for keyframes encountered */
        if (gpf->framenum < gpencil_anim_copy_firstframe) {
          gpencil_anim_copy_firstframe = gpf->framenum;
        }
        if (gpf->framenum > gpencil_anim_copy_lastframe) {
          gpencil_anim_copy_lastframe = gpf->framenum;
        }
      }
    }

    /* create a new layer in buffer if there were keyframes here */
    if (BLI_listbase_is_empty(&copied_frames) == false) {
      bGPDlayer *new_layer = static_cast<bGPDlayer *>(
          MEM_callocN(sizeof(bGPDlayer), "GPCopyPasteLayer"));
      BLI_addtail(&gpencil_anim_copybuf, new_layer);

      /* move over copied frames */
      BLI_movelisttolist(&new_layer->frames, &copied_frames);
      BLI_assert(copied_frames.first == nullptr);

      /* make a copy of the layer's name - for name-based matching later... */
      STRNCPY(new_layer->info, gpl->info);
    }
  }

  /* in case 'relative' paste method is used */
  gpencil_anim_copy_cfra = scene->r.cfra;

  /* clean up */
  ANIM_animdata_freelist(&anim_data);

  /* report success */
  return !BLI_listbase_is_empty(&gpencil_anim_copybuf);
}

bool ED_gpencil_anim_copybuf_paste(bAnimContext *ac, const short offset_mode)
{
  ListBase anim_data = {nullptr, nullptr};
  bAnimListElem *ale;
  int filter;

  Scene *scene = ac->scene;
  bool no_name = false;
  int offset = 0;

  /* check if buffer is empty */
  if (BLI_listbase_is_empty(&gpencil_anim_copybuf)) {
    return false;
  }

  /* Check if single channel in buffer (disregard names if so). */
  if (gpencil_anim_copybuf.first == gpencil_anim_copybuf.last) {
    no_name = true;
  }

  /* methods of offset (eKeyPasteOffset) */
  switch (offset_mode) {
    case KEYFRAME_PASTE_OFFSET_CFRA_START:
      offset = (scene->r.cfra - gpencil_anim_copy_firstframe);
      break;
    case KEYFRAME_PASTE_OFFSET_CFRA_END:
      offset = (scene->r.cfra - gpencil_anim_copy_lastframe);
      break;
    case KEYFRAME_PASTE_OFFSET_CFRA_RELATIVE:
      offset = (scene->r.cfra - gpencil_anim_copy_cfra);
      break;
    case KEYFRAME_PASTE_OFFSET_NONE:
      offset = 0;
      break;
  }

  /* filter data */
  /* TODO: try doing it with selection, then without selection limits. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* from selected channels */
  for (ale = static_cast<bAnimListElem *>(anim_data.first); ale; ale = ale->next) {
    /* only deal with GPlayers (case of calls from general dopesheet) */
    if (ale->type != ANIMTYPE_GPLAYER) {
      continue;
    }

    bGPDlayer *gpld = (bGPDlayer *)ale->data;
    bGPDlayer *gpls = nullptr;
    bGPDframe *gpfs, *gpf;

    /* find suitable layer from buffer to use to paste from */
    for (gpls = static_cast<bGPDlayer *>(gpencil_anim_copybuf.first); gpls; gpls = gpls->next) {
      /* check if layer name matches */
      if ((no_name) || STREQ(gpls->info, gpld->info)) {
        break;
      }
    }

    /* this situation might occur! */
    if (gpls == nullptr) {
      continue;
    }

    /* add frames from buffer */
    for (gpfs = static_cast<bGPDframe *>(gpls->frames.first); gpfs; gpfs = gpfs->next) {
      /* temporarily apply offset to buffer-frame while copying */
      gpfs->framenum += offset;

      /* get frame to copy data into (if no frame returned, then just ignore) */
      gpf = BKE_gpencil_layer_frame_get(gpld, gpfs->framenum, GP_GETFRAME_ADD_NEW);
      if (gpf) {
        /* Ensure to use same keyframe type. */
        gpf->key_type = gpfs->key_type;

        bGPDstroke *gps, *gpsn;

        /* This should be the right frame... as it may be a pre-existing frame,
         * must make sure that only compatible stroke types get copied over
         * - We cannot just add a duplicate frame, as that would cause errors
         * - For now, we don't check if the types will be compatible since we
         *   don't have enough info to do so. Instead, we simply just paste,
         *   if it works, it will show up.
         */
        for (gps = static_cast<bGPDstroke *>(gpfs->strokes.first); gps; gps = gps->next) {
          /* make a copy of stroke, then of its points array */
          gpsn = BKE_gpencil_stroke_duplicate(gps, true, true);

          /* append stroke to frame */
          BLI_addtail(&gpf->strokes, gpsn);
        }

        /* if no strokes (i.e. new frame) added, free gpf */
        if (BLI_listbase_is_empty(&gpf->strokes)) {
          BKE_gpencil_layer_frame_delete(gpld, gpf);
        }
      }

      /* unapply offset from buffer-frame */
      gpfs->framenum -= offset;
    }

    /* Tag destination datablock. */
    DEG_id_tag_update(ale->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }

  /* clean up */
  ANIM_animdata_freelist(&anim_data);
  return true;
}

/* -------------------------------------- */
/* Snap Tools */

static bool gpencil_frame_snap_nearest(bGPDframe * /*gpf*/, Scene * /*scene*/)
{
#if 0 /* NOTE: gpf->framenum is already an int! */
  if (gpf->flag & GP_FRAME_SELECT) {
    gpf->framenum = int(floor(gpf->framenum + 0.5));
  }
#endif
  return false;
}

static bool gpencil_frame_snap_nearestsec(bGPDframe *gpf, Scene *scene)
{
  float secf = float(FPS);
  if (gpf->flag & GP_FRAME_SELECT) {
    gpf->framenum = int(floorf(gpf->framenum / secf + 0.5f) * secf);
  }
  return false;
}

static bool gpencil_frame_snap_cframe(bGPDframe *gpf, Scene *scene)
{
  if (gpf->flag & GP_FRAME_SELECT) {
    gpf->framenum = int(scene->r.cfra);
  }
  return false;
}

static bool gpencil_frame_snap_nearmarker(bGPDframe *gpf, Scene *scene)
{
  if (gpf->flag & GP_FRAME_SELECT) {
    gpf->framenum = (int)ED_markers_find_nearest_marker_time(&scene->markers,
                                                             float(gpf->framenum));
  }
  return false;
}

void ED_gpencil_layer_snap_frames(bGPDlayer *gpl, Scene *scene, short mode)
{
  switch (mode) {
    case SNAP_KEYS_NEARFRAME: /* snap to nearest frame */
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_snap_nearest);
      break;
    case SNAP_KEYS_CURFRAME: /* snap to current frame */
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_snap_cframe);
      break;
    case SNAP_KEYS_NEARMARKER: /* snap to nearest marker */
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_snap_nearmarker);
      break;
    case SNAP_KEYS_NEARSEC: /* snap to nearest second */
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_snap_nearestsec);
      break;
    default: /* just in case */
      break;
  }
}

/* -------------------------------------- */
/* Mirror Tools */

static bool gpencil_frame_mirror_cframe(bGPDframe *gpf, Scene *scene)
{
  int diff;

  if (gpf->flag & GP_FRAME_SELECT) {
    diff = scene->r.cfra - gpf->framenum;
    gpf->framenum = scene->r.cfra + diff;
  }

  return false;
}

static bool gpencil_frame_mirror_yaxis(bGPDframe *gpf, Scene * /*scene*/)
{
  int diff;

  if (gpf->flag & GP_FRAME_SELECT) {
    diff = -gpf->framenum;
    gpf->framenum = diff;
  }

  return false;
}

static bool gpencil_frame_mirror_xaxis(bGPDframe *gpf, Scene * /*scene*/)
{
  int diff;

  /* NOTE: since we can't really do this, we just do the same as for yaxis... */
  if (gpf->flag & GP_FRAME_SELECT) {
    diff = -gpf->framenum;
    gpf->framenum = diff;
  }

  return false;
}

static bool gpencil_frame_mirror_marker(bGPDframe *gpf, Scene *scene)
{
  static TimeMarker *marker;
  static short initialized = 0;
  int diff;

  /* In order for this mirror function to work without
   * any extra arguments being added, we use the case
   * of gpf==nullptr to denote that we should find the
   * marker to mirror over. The static pointer is safe
   * to use this way, as it will be set to null after
   * each cycle in which this is called.
   */

  if (gpf != nullptr) {
    /* mirroring time */
    if ((gpf->flag & GP_FRAME_SELECT) && (marker)) {
      diff = (marker->frame - gpf->framenum);
      gpf->framenum = (marker->frame + diff);
    }
  }
  else {
    /* initialization time */
    if (initialized) {
      /* reset everything for safety */
      marker = nullptr;
      initialized = 0;
    }
    else {
      /* try to find a marker */
      marker = ED_markers_get_first_selected(&scene->markers);
      if (marker) {
        initialized = 1;
      }
    }
  }

  return false;
}

void ED_gpencil_layer_mirror_frames(bGPDlayer *gpl, Scene *scene, short mode)
{
  switch (mode) {
    case MIRROR_KEYS_CURFRAME: /* mirror over current frame */
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_mirror_cframe);
      break;
    case MIRROR_KEYS_YAXIS: /* mirror over frame 0 */
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_mirror_yaxis);
      break;
    case MIRROR_KEYS_XAXIS: /* mirror over value 0 */
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_mirror_xaxis);
      break;
    case MIRROR_KEYS_MARKER: /* mirror over marker */
      gpencil_frame_mirror_marker(nullptr, scene);
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_mirror_marker);
      gpencil_frame_mirror_marker(nullptr, scene);
      break;
    default: /* just in case */
      ED_gpencil_layer_frames_looper(gpl, scene, gpencil_frame_mirror_yaxis);
      break;
  }
}

/* ***************************************** */
