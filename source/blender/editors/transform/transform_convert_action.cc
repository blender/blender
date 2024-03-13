/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_anim_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_mask_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_mask.h"
#include "BKE_nla.h"

#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"
#include "ED_markers.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "transform.hh"
#include "transform_snap.hh"

#include "transform_convert.hh"

/* Weak way of identifying whether TransData was set by #GPLayerToTransData or
 * #MaskLayerToTransData. This way we can identify whether the #td->loc2d_i is a pointer to an
 * integer value and we can correctly flush in #recalcData_actedit. */
static bool is_td2d_int(TransData2D *td2d)
{
  return td2d->loc2d_i && td2d->h1 == nullptr;
}

/* -------------------------------------------------------------------- */
/** \name Grease Pencil Transform helpers
 * \{ */

static bool grease_pencil_layer_initialize_trans_data(blender::bke::greasepencil::Layer &layer)
{
  using namespace blender::bke::greasepencil;
  LayerTransformData &trans_data = layer.runtime->trans_data_;

  if (trans_data.status != LayerTransformData::TRANS_CLEAR) {
    return false;
  }

  /* Make a copy of the current frames in the layer. This map will be changed during the
   * transformation, and we need to be able to reset it if the operation is canceled. */
  trans_data.frames_copy = layer.frames();
  trans_data.frames_duration.clear();
  trans_data.frames_destination.clear();

  for (const auto [frame_number, frame] : layer.frames().items()) {
    if (frame.is_null()) {
      continue;
    }

    /* Store frames' duration to keep them visually correct while moving the frames. */
    if (!frame.is_implicit_hold()) {
      trans_data.frames_duration.add(frame_number, layer.get_frame_duration_at(frame_number));
    }
  }

  trans_data.status = LayerTransformData::TRANS_INIT;
  return true;
}

static bool grease_pencil_layer_reset_trans_data(blender::bke::greasepencil::Layer &layer)
{
  using namespace blender::bke::greasepencil;
  LayerTransformData &trans_data = layer.runtime->trans_data_;

  /* If the layer frame map was affected by the transformation, set its status to initialized so
   * that the frames map gets reset the next time this modal function is called.
   */
  if (trans_data.status == LayerTransformData::TRANS_CLEAR) {
    return false;
  }
  trans_data.status = LayerTransformData::TRANS_INIT;
  return true;
}

static bool grease_pencil_layer_update_trans_data(blender::bke::greasepencil::Layer &layer,
                                                  const int src_frame_number,
                                                  const int dst_frame_number,
                                                  const bool duplicated)
{
  using namespace blender::bke::greasepencil;
  LayerTransformData &trans_data = layer.runtime->trans_data_;

  if (trans_data.status == LayerTransformData::TRANS_CLEAR) {
    return false;
  }

  if (trans_data.status == LayerTransformData::TRANS_INIT) {
    /* The transdata was only initialized. No transformation was applied yet.
     * The frame mapping is always defined relatively to the initial frame map, so we first need
     * to set the frames back to its initial state before applying any frame transformation. */
    layer.frames_for_write() = trans_data.frames_copy;
    layer.tag_frames_map_keys_changed();
    trans_data.status = LayerTransformData::TRANS_RUNNING;
  }

  const bool use_duplicated = duplicated &&
                              trans_data.temp_frames_buffer.contains(src_frame_number);
  const blender::Map<int, GreasePencilFrame> &frame_map = use_duplicated ?
                                                              (trans_data.temp_frames_buffer) :
                                                              (trans_data.frames_copy);

  if (!frame_map.contains(src_frame_number)) {
    return false;
  }

  /* Apply the transformation directly in the layer frame map, so that we display the transformed
   * frame numbers. We don't want to edit the frames or remove any drawing here. This will be
   * done at once at the end of the transformation. */
  const GreasePencilFrame src_frame = frame_map.lookup(src_frame_number);
  const int src_duration = trans_data.frames_duration.lookup_default(src_frame_number, 0);

  if (!use_duplicated) {
    layer.remove_frame(src_frame_number);
  }

  layer.remove_frame(dst_frame_number);

  GreasePencilFrame *frame = layer.add_frame(
      dst_frame_number, src_frame.drawing_index, src_duration);
  *frame = src_frame;

  trans_data.frames_destination.add_overwrite(src_frame_number, dst_frame_number);

  return true;
}

static bool grease_pencil_layer_apply_trans_data(GreasePencil &grease_pencil,
                                                 blender::bke::greasepencil::Layer &layer,
                                                 const bool canceled,
                                                 const bool duplicate)
{
  using namespace blender::bke::greasepencil;
  LayerTransformData &trans_data = layer.runtime->trans_data_;

  if (trans_data.status == LayerTransformData::TRANS_CLEAR) {
    /* The layer was not affected by the transformation, so do nothing. */
    return false;
  }

  /* Reset the frames to their initial state. */
  layer.frames_for_write() = trans_data.frames_copy;
  layer.tag_frames_map_keys_changed();

  if (!canceled) {
    /* Moves all the selected frames according to the transformation, and inserts the potential
     * duplicate frames in the layer. */
    grease_pencil.move_duplicate_frames(
        layer, trans_data.frames_destination, trans_data.temp_frames_buffer);
  }

  if (canceled && duplicate) {
    /* Duplicates were done, so we need to delete the corresponding duplicate drawings. */
    for (const GreasePencilFrame &duplicate_frame : trans_data.temp_frames_buffer.values()) {
      GreasePencilDrawingBase *drawing_base = grease_pencil.drawing(duplicate_frame.drawing_index);
      if (drawing_base->type == GP_DRAWING) {
        reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap().remove_user();
      }
    }
    grease_pencil.remove_drawings_with_no_users();
  }

  /* Clear the frames copy. */
  trans_data.frames_copy.clear();
  trans_data.frames_destination.clear();
  trans_data.temp_frames_buffer.clear();
  trans_data.status = LayerTransformData::TRANS_CLEAR;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Transform Creation
 * \{ */

/**
 * Fully select selected beztriples, but only include if it's on the right side of cfra.
 */
static int count_fcurve_keys(FCurve *fcu, char side, float cfra, bool is_prop_edit)
{
  BezTriple *bezt;
  int i, count = 0, count_all = 0;

  if (ELEM(nullptr, fcu, fcu->bezt)) {
    return count;
  }

  /* Only include points that occur on the right side of cfra. */
  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    if (FrameOnMouseSide(side, bezt->vec[1][0], cfra)) {
      /* No need to adjust the handle selection since they are assumed
       * selected (like graph editor with #SIPO_NOHANDLES). */
      if (bezt->f2 & SELECT) {
        count++;
      }

      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  return count;
}

/**
 * Fully select selected beztriples, but only include if it's on the right side of cfra.
 */
static int count_gplayer_frames(bGPDlayer *gpl, char side, float cfra, bool is_prop_edit)
{
  int count = 0, count_all = 0;

  if (gpl == nullptr) {
    return count;
  }

  /* Only include points that occur on the right side of cfra. */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if (FrameOnMouseSide(side, float(gpf->framenum), cfra)) {
      if (gpf->flag & GP_FRAME_SELECT) {
        count++;
      }
      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  return count;
}

static int count_grease_pencil_frames(const blender::bke::greasepencil::Layer *layer,
                                      const char side,
                                      const float cfra,
                                      const bool is_prop_edit,
                                      const bool use_duplicated)
{
  if (layer == nullptr) {
    return 0;
  }

  int count_selected = 0;
  int count_all = 0;

  if (use_duplicated) {
    /* Only count the frames that were duplicated. */
    count_selected += layer->runtime->trans_data_.temp_frames_buffer.size();
    count_all += count_selected;
  }
  else {
    /* Only include points that occur on the right side of cfra. */
    for (const auto &[frame_number, frame] : layer->frames().items()) {
      if (!FrameOnMouseSide(side, float(frame_number), cfra)) {
        continue;
      }
      if (frame.is_selected()) {
        count_selected++;
      }
      count_all++;
    }
  }

  if (is_prop_edit && count_selected > 0) {
    return count_all;
  }
  return count_selected;
}

/** Fully select selected beztriples, but only include if it's on the right side of cfra. */
static int count_masklayer_frames(MaskLayer *masklay, char side, float cfra, bool is_prop_edit)
{
  int count = 0, count_all = 0;

  if (masklay == nullptr) {
    return count;
  }

  /* Only include points that occur on the right side of cfra. */
  LISTBASE_FOREACH (MaskLayerShape *, masklayer_shape, &masklay->splines_shapes) {
    if (FrameOnMouseSide(side, float(masklayer_shape->frame), cfra)) {
      if (masklayer_shape->flag & MASK_SHAPE_SELECT) {
        count++;
      }
      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  return count;
}

/* This function assigns the information to transdata. */
static void TimeToTransData(
    TransData *td, TransData2D *td2d, BezTriple *bezt, AnimData *adt, float ypos)
{
  float *time = bezt->vec[1];

  /* Setup #TransData2D. */
  td2d->loc[0] = *time;
  td2d->loc2d = time;
  td2d->h1 = bezt->vec[0];
  td2d->h2 = bezt->vec[2];
  copy_v2_v2(td2d->ih1, td2d->h1);
  copy_v2_v2(td2d->ih2, td2d->h2);

  /* Setup #TransData. */

  /* Usually #td2d->loc is used here.
   * But this is for when the original location is not float[3]. */
  td->loc = time;

  copy_v3_v3(td->iloc, td->loc);
  td->val = time;
  td->ival = *(time);
  if (adt) {
    td->center[0] = BKE_nla_tweakedit_remap(adt, td->ival, NLATIME_CONVERT_MAP);
  }
  else {
    td->center[0] = td->ival;
  }
  td->center[1] = ypos;

  /* Store the AnimData where this keyframe exists as a keyframe of the
   * active action as #td->extra. */
  td->extra = adt;

  if (bezt->f2 & SELECT) {
    td->flag |= TD_SELECTED;
  }

  /* Set flags to move handles as necessary. */
  td->flag |= TD_MOVEHANDLE1 | TD_MOVEHANDLE2;

  BLI_assert(!is_td2d_int(td2d));
}

/* This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones...  i.e.   td = IcuToTransData(td, icu, ob, side, cfra);
 *
 * The 'side' argument is needed for the extend mode. 'B' = both sides, 'R'/'L' mean only data
 * on the named side are used.
 */
static TransData *ActionFCurveToTransData(TransData *td,
                                          TransData2D **td2dv,
                                          FCurve *fcu,
                                          AnimData *adt,
                                          char side,
                                          float cfra,
                                          bool is_prop_edit,
                                          float ypos)
{
  BezTriple *bezt;
  TransData2D *td2d = *td2dv;
  int i;

  if (ELEM(nullptr, fcu, fcu->bezt)) {
    return td;
  }

  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    /* Only add selected keyframes (for now, proportional edit is not enabled). */
    if (is_prop_edit || (bezt->f2 & SELECT))
    { /* Note this MUST match #count_fcurve_keys(), so can't use #BEZT_ISSEL_ANY() macro. */
      /* Only add if on the right 'side' of the current frame. */
      if (FrameOnMouseSide(side, bezt->vec[1][0], cfra)) {
        TimeToTransData(td, td2d, bezt, adt, ypos);

        td++;
        td2d++;
      }
    }
  }

  *td2dv = td2d;

  return td;
}

/**
 * This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones: e.g. `td += GPLayerToTransData(td, ...);`
 *
 * \param side: is needed for the extend mode. 'B' = both sides,
 * 'R'/'L' mean only data on the named side are used.
 */
static int GPLayerToTransData(TransData *td,
                              TransData2D *td2d,
                              bGPDlayer *gpl,
                              char side,
                              float cfra,
                              bool is_prop_edit,
                              float ypos)
{
  int count = 0;

  /* Check for select frames on right side of current frame. */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    const bool is_selected = (gpf->flag & GP_FRAME_SELECT) != 0;
    if (is_prop_edit || is_selected) {
      if (FrameOnMouseSide(side, float(gpf->framenum), cfra)) {
        td2d->loc[0] = float(gpf->framenum);
        td2d->loc2d_i = &gpf->framenum;

        td->loc = td->val = td2d->loc;
        td->iloc[0] = td->ival = td2d->loc[0];

        td->center[0] = td->ival;
        td->center[1] = ypos;

        if (is_selected) {
          td->flag = TD_SELECTED;
        }

        BLI_assert(is_td2d_int(td2d));

        /* Advance `td` now. */
        td++;
        td2d++;
        count++;
      }
    }
  }

  return count;
}

/**
 * Fills \a td and \a td2d with transform data for each frame of the grease pencil \a layer that is
 * on the \a side of the frame \a cfra. It also updates the runtime data of the \a layer to keep
 * track of the transform. This is why the \a layer is not const here.
 */
static int GreasePencilLayerToTransData(TransData *td,
                                        TransData2D *td2d,
                                        blender::bke::greasepencil::Layer *layer,
                                        const char side,
                                        const float cfra,
                                        const bool is_prop_edit,
                                        const float ypos,
                                        const bool duplicate)
{
  using namespace blender;
  using namespace bke::greasepencil;

  int total_trans_frames = 0;
  bool any_frame_affected = false;

  const auto grease_pencil_frame_to_trans_data = [&](const int frame_number,
                                                     const bool frame_selected) {
    /* We only add transform data for selected frames that are on the right side of current frame.
     * If proportional edit is set, then we should also account for non selected frames.
     */
    if ((!is_prop_edit && !frame_selected) || !FrameOnMouseSide(side, frame_number, cfra)) {
      return;
    }

    td2d->loc[0] = float(frame_number);

    td->val = td->loc = &td2d->loc[0];
    td->ival = td->iloc[0] = td2d->loc[0];

    td->center[0] = td->ival;
    td->center[1] = ypos;

    if (frame_selected) {
      td->flag |= TD_SELECTED;
    }
    /* Set a pointer to the layer in the transform data so that we can apply the transformation
     * while the operator is running.
     */
    td->flag |= TD_GREASE_PENCIL_FRAME;
    td->extra = layer;

    BLI_assert(!is_td2d_int(td2d));

    /* Advance `td` now. */
    td++;
    td2d++;
    total_trans_frames++;
    any_frame_affected = true;
  };

  const blender::Map<int, GreasePencilFrame> &frame_map =
      duplicate ? (layer->runtime->trans_data_.temp_frames_buffer) : layer->frames();

  for (const auto [frame_number, frame] : frame_map.items()) {
    grease_pencil_frame_to_trans_data(frame_number, frame.is_selected());
  }

  if (total_trans_frames == 0) {
    return total_trans_frames;
  }

  /* If it was not previously done, initialize the transform data in the layer, and if some frames
   * are actually concerned by the transform. */
  if (any_frame_affected) {
    grease_pencil_layer_initialize_trans_data(*layer);
  }

  return total_trans_frames;
}

/**
 * Refer to comment above #GPLayerToTransData, this is the same but for masks.
 */
static int MaskLayerToTransData(TransData *td,
                                TransData2D *td2d,
                                MaskLayer *masklay,
                                char side,
                                float cfra,
                                bool is_prop_edit,
                                float ypos)
{
  int count = 0;

  /* Check for select frames on right side of current frame. */
  LISTBASE_FOREACH (MaskLayerShape *, masklay_shape, &masklay->splines_shapes) {
    if (is_prop_edit || (masklay_shape->flag & MASK_SHAPE_SELECT)) {
      if (FrameOnMouseSide(side, float(masklay_shape->frame), cfra)) {
        td2d->loc[0] = float(masklay_shape->frame);
        td2d->loc2d_i = &masklay_shape->frame;

        td->loc = td->val = td2d->loc;
        td->iloc[0] = td->ival = td2d->loc[0];

        td->center[0] = td->ival;
        td->center[1] = ypos;

        BLI_assert(is_td2d_int(td2d));

        /* Advance td now. */
        td++;
        td2d++;
        count++;
      }
    }
  }

  return count;
}

static void createTransActionData(bContext *C, TransInfo *t)
{
  Scene *scene = t->scene;
  TransData *td = nullptr;
  TransData2D *td2d = nullptr;

  /* The T_DUPLICATED_KEYFRAMES flag is only set if we made some duplicates of the selected frames,
   * and they are the ones that are being transformed. */
  const bool use_duplicated = (t->flag & T_DUPLICATED_KEYFRAMES) != 0;

  rcti *mask = &t->region->v2d.mask;
  rctf *datamask = &t->region->v2d.cur;

  float xsize = BLI_rctf_size_x(datamask);
  float ysize = BLI_rctf_size_y(datamask);
  float xmask = BLI_rcti_size_x(mask);
  float ymask = BLI_rcti_size_y(mask);

  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

  int count = 0;
  int gpf_count = 0;
  float cfra;
  float ypos = 1.0f / ((ysize / xsize) * (xmask / ymask)) * BLI_rctf_cent_y(&t->region->v2d.cur);

  /* Determine what type of data we are operating on. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT);
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Which side of the current frame should be allowed. */
  if (t->mode == TFM_TIME_EXTEND) {
    t->frame_side = transform_convert_frame_side_dir_get(t, float(scene->r.cfra));
  }
  else {
    /* Normal transform - both sides of current frame are considered. */
    t->frame_side = 'B';
  }

  /* Loop 1: fully select F-Curve keys and count how many BezTriples are selected. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
    int adt_count = 0;
    /* Convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points). */
    if (adt) {
      cfra = BKE_nla_tweakedit_remap(adt, float(scene->r.cfra), NLATIME_CONVERT_UNMAP);
    }
    else {
      cfra = float(scene->r.cfra);
    }

    if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
      adt_count = count_fcurve_keys(
          static_cast<FCurve *>(ale->key_data), t->frame_side, cfra, is_prop_edit);
    }
    else if (ale->type == ANIMTYPE_GPLAYER) {
      adt_count = count_gplayer_frames(
          static_cast<bGPDlayer *>(ale->data), t->frame_side, cfra, is_prop_edit);
    }
    else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
      using namespace blender::bke::greasepencil;
      adt_count = count_grease_pencil_frames(
          static_cast<Layer *>(ale->data), t->frame_side, cfra, is_prop_edit, use_duplicated);
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      adt_count = count_masklayer_frames(
          static_cast<MaskLayer *>(ale->data), t->frame_side, cfra, is_prop_edit);
    }
    else {
      BLI_assert(0);
    }

    if (adt_count > 0) {
      if (ELEM(ale->type, ANIMTYPE_GPLAYER, ANIMTYPE_MASKLAYER)) {
        gpf_count += adt_count;
      }
      count += adt_count;
      ale->tag = true;
    }
  }

  /* Stop if trying to build list if nothing selected. */
  if (count == 0 && gpf_count == 0) {
    /* Cleanup temp list. */
    ANIM_animdata_freelist(&anim_data);
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* Allocate memory for data. */
  tc->data_len = count;

  tc->data = static_cast<TransData *>(
      MEM_callocN(tc->data_len * sizeof(TransData), "TransData(Action Editor)"));
  tc->data_2d = static_cast<TransData2D *>(
      MEM_callocN(tc->data_len * sizeof(TransData2D), "transdata2d"));
  td = tc->data;
  td2d = tc->data_2d;

  /* Loop 2: build transdata array. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {

    if (is_prop_edit && !ale->tag) {
      continue;
    }

    cfra = float(scene->r.cfra);

    {
      AnimData *adt;
      adt = ANIM_nla_mapping_get(&ac, ale);
      if (adt) {
        cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
      }
    }

    if (ale->type == ANIMTYPE_GPLAYER) {
      bGPDlayer *gpl = (bGPDlayer *)ale->data;
      int i;

      i = GPLayerToTransData(td, td2d, gpl, t->frame_side, cfra, is_prop_edit, ypos);
      td += i;
      td2d += i;
    }
    else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
      using namespace blender::bke::greasepencil;
      Layer *layer = static_cast<Layer *>(ale->data);
      int i;

      i = GreasePencilLayerToTransData(
          td, td2d, layer, t->frame_side, cfra, is_prop_edit, ypos, use_duplicated);
      td += i;
      td2d += i;
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      MaskLayer *masklay = (MaskLayer *)ale->data;
      int i;

      i = MaskLayerToTransData(td, td2d, masklay, t->frame_side, cfra, is_prop_edit, ypos);
      td += i;
      td2d += i;
    }
    else {
      AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
      FCurve *fcu = (FCurve *)ale->key_data;

      td = ActionFCurveToTransData(td, &td2d, fcu, adt, t->frame_side, cfra, is_prop_edit, ypos);
    }
  }

  /* Calculate distances for proportional editing. */
  if (is_prop_edit) {
    td = tc->data;

    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      AnimData *adt;

      /* F-Curve may not have any keyframes. */
      if (!ale->tag) {
        continue;
      }

      adt = ANIM_nla_mapping_get(&ac, ale);
      if (adt) {
        cfra = BKE_nla_tweakedit_remap(adt, float(scene->r.cfra), NLATIME_CONVERT_UNMAP);
      }
      else {
        cfra = float(scene->r.cfra);
      }

      if (ale->type == ANIMTYPE_GPLAYER) {
        bGPDlayer *gpl = (bGPDlayer *)ale->data;

        LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
          if (gpf->flag & GP_FRAME_SELECT) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            int min = INT_MAX;
            LISTBASE_FOREACH (bGPDframe *, gpf_iter, &gpl->frames) {
              if (gpf_iter->flag & GP_FRAME_SELECT) {
                if (FrameOnMouseSide(t->frame_side, float(gpf_iter->framenum), cfra)) {
                  int val = abs(gpf->framenum - gpf_iter->framenum);
                  if (val < min) {
                    min = val;
                  }
                }
              }
            }
            td->dist = td->rdist = min;
          }
          td++;
        }
      }
      else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
        using namespace blender::bke::greasepencil;
        Layer *layer = static_cast<Layer *>(ale->data);

        const auto grease_pencil_closest_selected_frame = [&](const int frame_number,
                                                              const bool frame_selected) {
          if (frame_selected) {
            td->dist = td->rdist = 0.0f;
            ++td;
            return;
          }

          int closest_selected = INT_MAX;
          for (const auto [neighbor_frame_number, neighbor_frame] : layer->frames().items()) {
            if (!neighbor_frame.is_selected() ||
                !FrameOnMouseSide(t->frame_side, float(neighbor_frame_number), cfra))
            {
              continue;
            }
            const int distance = abs(neighbor_frame_number - frame_number);
            closest_selected = std::min(closest_selected, distance);
          }

          td->dist = td->rdist = closest_selected;
          ++td;
        };

        for (const auto [frame_number, frame] : layer->frames().items()) {
          grease_pencil_closest_selected_frame(frame_number, frame.is_selected());
        }

        if (use_duplicated) {
          /* Also count for duplicated frames. */
          for (const auto [frame_number, frame] :
               layer->runtime->trans_data_.temp_frames_buffer.items())
          {
            grease_pencil_closest_selected_frame(frame_number, frame.is_selected());
          }
        }
      }
      else if (ale->type == ANIMTYPE_MASKLAYER) {
        MaskLayer *masklay = (MaskLayer *)ale->data;

        LISTBASE_FOREACH (MaskLayerShape *, masklay_shape, &masklay->splines_shapes) {
          if (FrameOnMouseSide(t->frame_side, float(masklay_shape->frame), cfra)) {
            if (masklay_shape->flag & MASK_SHAPE_SELECT) {
              td->dist = td->rdist = 0.0f;
            }
            else {
              int min = INT_MAX;
              LISTBASE_FOREACH (MaskLayerShape *, masklay_iter, &masklay->splines_shapes) {
                if (masklay_iter->flag & MASK_SHAPE_SELECT) {
                  if (FrameOnMouseSide(t->frame_side, float(masklay_iter->frame), cfra)) {
                    int val = abs(masklay_shape->frame - masklay_iter->frame);
                    if (val < min) {
                      min = val;
                    }
                  }
                }
              }
              td->dist = td->rdist = min;
            }
            td++;
          }
        }
      }
      else {
        FCurve *fcu = (FCurve *)ale->key_data;
        BezTriple *bezt;
        int i;

        for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
          if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
            if (bezt->f2 & SELECT) {
              td->dist = td->rdist = 0.0f;
            }
            else {
              BezTriple *bezt_iter;
              int j;
              float min = FLT_MAX;
              for (j = 0, bezt_iter = fcu->bezt; j < fcu->totvert; j++, bezt_iter++) {
                if (bezt_iter->f2 & SELECT) {
                  if (FrameOnMouseSide(t->frame_side, float(bezt_iter->vec[1][0]), cfra)) {
                    float val = fabs(bezt->vec[1][0] - bezt_iter->vec[1][0]);
                    if (val < min) {
                      min = val;
                    }
                  }
                }
              }
              td->dist = td->rdist = min;
            }
            td++;
          }
        }
      }
    }
  }

  /* Cleanup temp list. */
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Transform Flush
 * \{ */

static void invert_snap(eSnapMode &snap_mode)
{
  /* Make snapping work like before 4.0 where pressing CTRL will switch between snapping to seconds
   * and frames. */
  if (snap_mode & SCE_SNAP_TO_FRAME) {
    snap_mode &= ~SCE_SNAP_TO_FRAME;
    snap_mode |= SCE_SNAP_TO_SECOND;
  }
  else if (snap_mode & SCE_SNAP_TO_SECOND) {
    snap_mode &= ~SCE_SNAP_TO_SECOND;
    snap_mode |= SCE_SNAP_TO_FRAME;
  }
}

static void recalcData_actedit(TransInfo *t)
{
  ViewLayer *view_layer = t->view_layer;
  SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;

  bAnimContext ac = {nullptr};
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  BKE_view_layer_synced_ensure(t->scene, t->view_layer);

  /* Initialize relevant anim-context `context` data from #TransInfo data. */
  /* NOTE: sync this with the code in #ANIM_animdata_get_context(). */
  ac.bmain = CTX_data_main(t->context);
  ac.scene = t->scene;
  ac.view_layer = t->view_layer;
  ac.obact = BKE_view_layer_active_object_get(view_layer);
  ac.area = t->area;
  ac.region = t->region;
  ac.sl = static_cast<SpaceLink *>((t->area) ? t->area->spacedata.first : nullptr);
  ac.spacetype = (t->area) ? t->area->spacetype : 0;
  ac.regiontype = (t->region) ? t->region->regiontype : 0;

  ANIM_animdata_context_getdata(&ac);

  /* Flush 2d vector. */
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  eSnapMode snap_mode = t->tsnap.mode;
  if (t->modifiers & MOD_SNAP_INVERT) {
    invert_snap(snap_mode);
  }

  TransData *td;
  TransData2D *td2d;
  int i = 0;
  for (td = tc->data, td2d = tc->data_2d; i < tc->data_len; i++, td++, td2d++) {
    if ((t->tsnap.flag & SCE_SNAP) && (t->state != TRANS_CANCEL) && !(td->flag & TD_NOTIMESNAP)) {
      transform_snap_anim_flush_data(t, td, snap_mode, td->loc);
    }

    /* Constrain Y. */
    td->loc[1] = td->iloc[1];

    transform_convert_flush_handle2D(td, td2d, 0.0f);

    if ((t->state == TRANS_RUNNING) && ((td->flag & TD_GREASE_PENCIL_FRAME) != 0)) {
      const bool use_duplicated = (t->flag & T_DUPLICATED_KEYFRAMES) != 0;
      grease_pencil_layer_update_trans_data(
          *static_cast<blender::bke::greasepencil::Layer *>(td->extra),
          round_fl_to_int(td->ival),
          round_fl_to_int(td2d->loc[0]),
          use_duplicated);
    }
    else if (is_td2d_int(td2d)) {
      /* (Grease Pencil Legacy)
       * This helps flush transdata written to tempdata into the gp-frames. */
      *td2d->loc2d_i = round_fl_to_int(td2d->loc[0]);
    }
  }

  if (ac.datatype != ANIMCONT_MASK) {
    /* Get animdata blocks visible in editor,
     * assuming that these will be the ones where things changed. */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA);
    ANIM_animdata_filter(
        &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

    /* Just tag these animdata-blocks to recalc, assuming that some data there changed
     * BUT only do this if realtime updates are enabled. */
    if ((saction->flag & SACTION_NOREALTIMEUPDATES) == 0) {
      LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
        /* Set refresh tags for objects using this animation. */
        ANIM_list_elem_update(CTX_data_main(t->context), t->scene, ale);
      }

      /* Now free temp channels. */
      ANIM_animdata_freelist(&anim_data);
    }

    if (ac.datatype == ANIMCONT_GPENCIL) {
      filter = ANIMFILTER_DATA_VISIBLE;
      ANIM_animdata_filter(
          &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

      LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
        if (ale->type != ANIMTYPE_GREASE_PENCIL_LAYER) {
          continue;
        }
        grease_pencil_layer_reset_trans_data(
            *static_cast<blender::bke::greasepencil::Layer *>(ale->data));
      }
      ANIM_animdata_freelist(&anim_data);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Action
 * \{ */

static int masklay_shape_cmp_frame(void *thunk, const void *a, const void *b)
{
  const MaskLayerShape *frame_a = static_cast<const MaskLayerShape *>(a);
  const MaskLayerShape *frame_b = static_cast<const MaskLayerShape *>(b);

  if (frame_a->frame < frame_b->frame) {
    return -1;
  }
  if (frame_a->frame > frame_b->frame) {
    return 1;
  }
  *((bool *)thunk) = true;
  /* Selected last. */
  if ((frame_a->flag & MASK_SHAPE_SELECT) && ((frame_b->flag & MASK_SHAPE_SELECT) == 0)) {
    return 1;
  }
  return 0;
}

static void posttrans_mask_clean(Mask *mask)
{
  LISTBASE_FOREACH (MaskLayer *, masklay, &mask->masklayers) {
    MaskLayerShape *masklay_shape, *masklay_shape_next;
    bool is_double = false;

    BLI_listbase_sort_r(&masklay->splines_shapes, masklay_shape_cmp_frame, &is_double);

    if (is_double) {
      for (masklay_shape = static_cast<MaskLayerShape *>(masklay->splines_shapes.first);
           masklay_shape;
           masklay_shape = masklay_shape_next)
      {
        masklay_shape_next = masklay_shape->next;
        if (masklay_shape_next && masklay_shape->frame == masklay_shape_next->frame) {
          BKE_mask_layer_shape_unlink(masklay, masklay_shape);
        }
      }
    }

#ifndef NDEBUG
    for (masklay_shape = static_cast<MaskLayerShape *>(masklay->splines_shapes.first);
         masklay_shape;
         masklay_shape = masklay_shape->next)
    {
      BLI_assert(!masklay_shape->next || masklay_shape->frame < masklay_shape->next->frame);
    }
#endif
  }

  WM_main_add_notifier(NC_MASK | NA_EDITED, mask);
}

/* Called by special_aftertrans_update to make sure selected gp-frames replace
 * any other gp-frames which may reside on that frame (that are not selected).
 * It also makes sure gp-frames are still stored in chronological order after
 * transform.
 */
static void posttrans_gpd_clean(bGPdata *gpd)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf, *gpfn;
    bool is_double = false;

    BKE_gpencil_layer_frames_sort(gpl, &is_double);

    if (is_double) {
      for (gpf = static_cast<bGPDframe *>(gpl->frames.first); gpf; gpf = gpfn) {
        gpfn = gpf->next;
        if (gpfn && gpf->framenum == gpfn->framenum) {
          BKE_gpencil_layer_frame_delete(gpl, gpf);
        }
      }
    }

#ifndef NDEBUG
    for (gpf = static_cast<bGPDframe *>(gpl->frames.first); gpf; gpf = gpf->next) {
      BLI_assert(!gpf->next || gpf->framenum < gpf->next->framenum);
    }
#endif
  }
  /* Set cache flag to dirty. */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, gpd);
}

/**
 * Called by #special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 * remake_action_ipos should have already been called.
 */
static void posttrans_action_clean(bAnimContext *ac, bAction *act)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, eAnimFilter_Flags(filter), act, ANIMCONT_ACTION);

  /* Loop through relevant data, removing keyframes as appropriate.
   *      - all keyframes are converted in/out of global time.
   */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, false);
      BKE_fcurve_merge_duplicate_keys(static_cast<FCurve *>(ale->key_data),
                                      SELECT,
                                      false); /* Only use handles in graph editor. */
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, false);
    }
    else {
      BKE_fcurve_merge_duplicate_keys(static_cast<FCurve *>(ale->key_data),
                                      SELECT,
                                      false); /* Only use handles in graph editor. */
    }
  }

  /* Free temp data. */
  ANIM_animdata_freelist(&anim_data);
}

static void special_aftertrans_update__actedit(bContext *C, TransInfo *t)
{
  SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;
  bAnimContext ac;

  const bool canceled = (t->state == TRANS_CANCEL);
  const bool duplicate = (t->flag & T_DUPLICATED_KEYFRAMES) != 0;

  /* Initialize relevant anim-context 'context' data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  Object *ob = ac.obact;

  if (ELEM(ac.datatype, ANIMCONT_DOPESHEET, ANIMCONT_SHAPEKEY, ANIMCONT_TIMELINE)) {
    ListBase anim_data = {nullptr, nullptr};
    short filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT);

    /* Get channels to work on. */
    ANIM_animdata_filter(
        &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      switch (ale->datatype) {
        case ALE_GPFRAME:
          ale->id->tag &= ~LIB_TAG_DOIT;
          posttrans_gpd_clean((bGPdata *)ale->id);
          break;

        case ALE_FCURVE: {
          AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
          FCurve *fcu = (FCurve *)ale->key_data;

          /* 3 cases here for curve cleanups:
           * 1) NOTRANSKEYCULL on    -> cleanup of duplicates shouldn't be done.
           * 2) canceled == 0        -> user confirmed the transform,
           *                            so duplicates should be removed.
           * 3) canceled + duplicate -> user canceled the transform,
           *                            but we made duplicates, so get rid of these.
           */
          if ((saction->flag & SACTION_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
            if (adt) {
              ANIM_nla_mapping_apply_fcurve(adt, fcu, false, false);
              BKE_fcurve_merge_duplicate_keys(
                  fcu, SELECT, false); /* Only use handles in graph editor. */
              ANIM_nla_mapping_apply_fcurve(adt, fcu, true, false);
            }
            else {
              BKE_fcurve_merge_duplicate_keys(
                  fcu, SELECT, false); /* Only use handles in graph editor. */
            }
          }
          break;
        }

        default:
          BLI_assert_msg(false, "Keys cannot be transformed into this animation type.");
      }
    }

    /* Free temp memory. */
    ANIM_animdata_freelist(&anim_data);
  }
  else if (ac.datatype == ANIMCONT_ACTION) { /* TODO: just integrate into the above. */
    /* Depending on the lock status, draw necessary views. */
    /* FIXME: some of this stuff is not good. */
    if (ob) {
      if (ob->pose || BKE_key_from_object(ob)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
      }
      else {
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
      }
    }

    /* 3 cases here for curve cleanups:
     * 1) NOTRANSKEYCULL on    -> cleanup of duplicates shouldn't be done
     * 2) canceled == 0        -> user confirmed the transform,
     *                            so duplicates should be removed.
     * 3) canceled + duplicate -> user canceled the transform,
     *                            but we made duplicates, so get rid of these.
     */
    if ((saction->flag & SACTION_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
      posttrans_action_clean(&ac, (bAction *)ac.data);
    }
  }
  else if (ac.datatype == ANIMCONT_GPENCIL) {
    /* Remove duplicate frames and also make sure points are in order! */
    /* 3 cases here for curve cleanups:
     * 1) NOTRANSKEYCULL on    -> cleanup of duplicates shouldn't be done.
     * 2) canceled == 0        -> user confirmed the transform,
     *                            so duplicates should be removed.
     * 3) canceled + duplicate -> user canceled the transform,
     *                            but we made duplicates, so get rid of these.
     */
    ListBase anim_data = {nullptr, nullptr};
    const int filter = ANIMFILTER_DATA_VISIBLE;
    ANIM_animdata_filter(
        &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      switch (ale->datatype) {
        case ALE_GPFRAME:
          /* Grease Pencil legacy. */
          if ((saction->flag & SACTION_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
            ale->id->tag &= ~LIB_TAG_DOIT;
            posttrans_gpd_clean((bGPdata *)ale->id);
          }
          break;

        case ALE_GREASE_PENCIL_CEL: {
          GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
          grease_pencil_layer_apply_trans_data(
              *grease_pencil,
              *static_cast<blender::bke::greasepencil::Layer *>(ale->data),
              canceled,
              duplicate);
          break;
        }

        default:
          break;
      }
    }
    ANIM_animdata_freelist(&anim_data);
  }
  else if (ac.datatype == ANIMCONT_MASK) {
    /* Remove duplicate frames and also make sure points are in order! */
    /* 3 cases here for curve cleanups:
     * 1) NOTRANSKEYCULL on:
     *    Cleanup of duplicates shouldn't be done.
     * 2) canceled == 0:
     *    User confirmed the transform, so duplicates should be removed.
     * 3) Canceled + duplicate:
     *    User canceled the transform, but we made duplicates, so get rid of these.
     */
    if ((saction->flag & SACTION_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
      ListBase anim_data = {nullptr, nullptr};
      const int filter = ANIMFILTER_DATA_VISIBLE;
      ANIM_animdata_filter(
          &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

      LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
        if (ale->datatype == ALE_MASKLAY) {
          ale->id->tag &= ~LIB_TAG_DOIT;
          posttrans_mask_clean((Mask *)ale->id);
        }
      }
      ANIM_animdata_freelist(&anim_data);
    }
  }

  /* Marker transform, not especially nice but we may want to move markers
   * at the same time as keyframes in the dope sheet. */
  if ((saction->flag & SACTION_MARKERS_MOVE) && (canceled == 0)) {
    if (t->mode == TFM_TIME_TRANSLATE) {
#if 0
      if (ELEM(t->frame_side, 'L', 'R')) { /* #TFM_TIME_EXTEND. */
        /* Same as below. */
        ED_markers_post_apply_transform(
            ED_context_get_markers(C), t->scene, t->mode, t->values_final[0], t->frame_side);
      }
      else /* #TFM_TIME_TRANSLATE. */
#endif
      {
        ED_markers_post_apply_transform(
            ED_context_get_markers(C), t->scene, t->mode, t->values_final[0], t->frame_side);
      }
    }
    else if (t->mode == TFM_TIME_SCALE) {
      ED_markers_post_apply_transform(
          ED_context_get_markers(C), t->scene, t->mode, t->values_final[0], t->frame_side);
    }
  }

  /* Make sure all F-Curves are set correctly. */
  if (!ELEM(ac.datatype, ANIMCONT_GPENCIL)) {
    ANIM_editkeyframes_refresh(&ac);
  }

  /* Clear flag that was set for time-slide drawing. */
  saction->flag &= ~SACTION_MOVING;
}

/** \} */

TransConvertTypeInfo TransConvertType_Action = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransActionData,
    /*recalc_data*/ recalcData_actedit,
    /*special_aftertrans_update*/ special_aftertrans_update__actedit,
};
