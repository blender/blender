/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"

#include "BLI_array.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

#include "SEQ_channels.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"

#include "ANIM_keyframing.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "transform.hh"
#include "transform_convert.hh"

namespace blender::ed::transform {

namespace {

/** Used for sequencer transform. */
struct TransDataSeq {
  Strip *strip;
  Array<float2> quad_orig;
  float3x3 orig_matrix;

  float2 orig_origin_relative; /* 0-1 range within image bounds. */
  float2 orig_origin_pixelspace;
  float2 orig_translation;
  float2 orig_scale;
  float orig_rotation;
  int orig_flag;
  float active_seq_orig_rotation;
  float2 orig_mirror;
};

}  // namespace

static void store_transform_properties(const Scene *scene,
                                       Strip *strip,
                                       float2 origin,
                                       TransData *td)
{
  Editing *ed = seq::editing_get(scene);
  const StripTransform *transform = strip->data->transform;
  TransDataSeq *tdseq = MEM_new<TransDataSeq>("TransSeq TransDataSeq");
  tdseq->strip = strip;
  copy_v2_v2(tdseq->orig_origin_relative, transform->origin);
  tdseq->orig_origin_pixelspace = origin;
  tdseq->quad_orig = seq::image_transform_final_quad_get(scene, strip);
  tdseq->orig_matrix = math::invert(seq::image_transform_matrix_get(scene, strip));

  tdseq->orig_translation[0] = transform->xofs;
  tdseq->orig_translation[1] = transform->yofs;
  tdseq->orig_scale[0] = transform->scale_x;
  tdseq->orig_scale[1] = transform->scale_y;
  tdseq->orig_rotation = transform->rotation;
  tdseq->orig_flag = strip->flag;
  tdseq->orig_mirror = seq::image_transform_mirror_factor_get(strip);
  tdseq->active_seq_orig_rotation = ed->act_strip && ed->act_strip->data->transform ?
                                        ed->act_strip->data->transform->rotation :
                                        transform->rotation;
  tdseq->strip = strip;
  td->extra = static_cast<void *>(tdseq);
}

static TransData *SeqToTransData(
    const Scene *scene, Strip *strip, TransData *td, TransData2D *td2d, int vert_index)
{
  const StripTransform *transform = strip->data->transform;
  const float2 origin = seq::image_transform_origin_offset_pixelspace_get(scene, strip);
  const float2 mirror = seq::image_transform_mirror_factor_get(strip);
  float vertex[2] = {origin[0], origin[1]};

  /* Add control vertex, so rotation and scale can be calculated.
   * All three vertices will form a "L" shape that is aligned to the local strip axis.
   */
  if (vert_index == 1) {
    vertex[0] += cosf(transform->rotation);
    vertex[1] += sinf(transform->rotation);
  }
  else if (vert_index == 2) {
    vertex[0] -= sinf(transform->rotation);
    vertex[1] += cosf(transform->rotation);
  }

  td2d->loc[0] = vertex[0];
  td2d->loc[1] = vertex[1];
  td2d->loc2d = nullptr;
  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);

  td->center[0] = origin[0];
  td->center[1] = origin[1];

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  axis_angle_to_mat3_single(td->axismtx, 'Z', transform->rotation * mirror[0] * mirror[1]);
  normalize_m3(td->axismtx);

  /* Store properties only once per vertex "triad". */
  if (vert_index == 0) {
    store_transform_properties(scene, strip, origin, td);
  }

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  return td;
}

static void freeSeqData(TransInfo * /*t*/,
                        TransDataContainer *tc,
                        TransCustomData * /*custom_data*/)
{
  TransData *td = tc->data;
  for (int i = 0; i < tc->data_len; i += 3) {
    TransDataSeq *tdseq = static_cast<TransDataSeq *>((td + i)->extra);
    MEM_delete(tdseq);
  }
}

static void createTransSeqImageData(bContext *C, TransInfo *t)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  const SpaceSeq *sseq = static_cast<const SpaceSeq *>(t->area->spacedata.first);
  const ARegion *region = t->region;

  if (ed == nullptr) {
    return;
  }
  if (sseq->mainb != SEQ_DRAW_IMG_IMBUF) {
    return;
  }
  if (region->regiontype == RGN_TYPE_PREVIEW && sseq->view == SEQ_VIEW_SEQUENCE_PREVIEW) {
    return;
  }

  ListBase *seqbase = seq::active_seqbase_get(ed);
  ListBase *channels = seq::channels_displayed_get(ed);
  VectorSet strips = seq::query_rendered_strips(scene, channels, seqbase, scene->r.cfra, 0);
  strips.remove_if([&](Strip *strip) { return (strip->flag & SELECT) == 0; });

  if (strips.is_empty()) {
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  tc->custom.type.free_cb = freeSeqData;

  tc->data_len = strips.size() * 3; /* 3 vertices per sequence are needed. */
  TransData *td = tc->data = MEM_calloc_arrayN<TransData>(tc->data_len, "TransSeq TransData");
  TransData2D *td2d = tc->data_2d = MEM_calloc_arrayN<TransData2D>(tc->data_len,
                                                                   "TransSeq TransData2D");

  for (Strip *strip : strips) {
    /* One `Sequence` needs 3 `TransData` entries - center point placed in image origin, then 2
     * points offset by 1 in X and Y direction respectively, so rotation and scale can be
     * calculated from these points. */
    SeqToTransData(scene, strip, td++, td2d++, 0);
    SeqToTransData(scene, strip, td++, td2d++, 1);
    SeqToTransData(scene, strip, td++, td2d++, 2);
  }
}

static bool autokeyframe_sequencer_image(bContext *C,
                                         Scene *scene,
                                         StripTransform *transform,
                                         const int tmode)
{
  PropertyRNA *prop;
  PointerRNA ptr = RNA_pointer_create_discrete(&scene->id, &RNA_StripTransform, transform);

  const bool around_cursor = scene->toolsettings->sequencer_tool_settings->pivot_point ==
                             V3D_AROUND_CURSOR;
  const bool do_loc = tmode == TFM_TRANSLATION || around_cursor;
  const bool do_rot = tmode == TFM_ROTATION;
  const bool do_scale = tmode == TFM_RESIZE;
  const bool only_when_keyed = animrig::is_keying_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE);

  bool changed = false;
  if (do_rot) {
    prop = RNA_struct_find_property(&ptr, "rotation");
    changed |= animrig::autokeyframe_property(
        C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
  }
  if (do_loc) {
    prop = RNA_struct_find_property(&ptr, "offset_x");
    changed |= animrig::autokeyframe_property(
        C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
    prop = RNA_struct_find_property(&ptr, "offset_y");
    changed |= animrig::autokeyframe_property(
        C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
  }
  if (do_scale) {
    prop = RNA_struct_find_property(&ptr, "scale_x");
    changed |= animrig::autokeyframe_property(
        C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
    prop = RNA_struct_find_property(&ptr, "scale_y");
    changed |= animrig::autokeyframe_property(
        C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
  }

  return changed;
}

struct TransformResult {
  float2 translation;
  float2 scale;
  float rotation;
};

static TransformResult transform_result_get(TransInfo *t,
                                            TransDataSeq *tdseq,
                                            TransData2D *td2d,
                                            Strip *strip)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  float2 handle_origin = {td2d->loc[0], td2d->loc[1]};
  /* X and Y control points used to read scale and rotation. */
  float2 handle_x = float2((td2d + 1)->loc) - handle_origin;
  float2 handle_y = float2((td2d + 2)->loc) - handle_origin;
  float2 aspect = {scene->r.yasp / scene->r.xasp, 1.0f};
  float2 mirror = seq::image_transform_mirror_factor_get(strip);
  float2 orig_strip_origin_pixelspace = tdseq->orig_origin_pixelspace;

  return TransformResult{(orig_strip_origin_pixelspace - handle_origin) * mirror * aspect,
                         {math::length(handle_x), math::length(handle_y)},
                         t->values_final[0] * mirror[0] * mirror[1]};
}

static void image_transform_set(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransData *td = nullptr;
  TransData2D *td2d = nullptr;
  Scene *scene = CTX_data_sequencer_scene(t->context);
  Editing *ed = seq::editing_get(scene);
  int i;

  for (i = 0, td = tc->data, td2d = tc->data_2d; i < tc->data_len; i += 3, td += 3, td2d += 3) {
    TransDataSeq *tdseq = static_cast<TransDataSeq *>(td->extra);
    Strip *strip = tdseq->strip;
    StripTransform *transform = strip->data->transform;
    TransformResult result = transform_result_get(t, tdseq, td2d, strip);

    /* Round resulting position to integer pixels. Resulting strip
     * will more often end up using faster interpolation (without bilinear),
     * and avoids "text edges are too dark" artifacts with light text strips
     * on light backgrounds. The latter happens because bilinear filtering
     * does not do full alpha pre-multiplication. */
    transform->xofs = roundf(tdseq->orig_translation.x - result.translation.x);
    transform->yofs = roundf(tdseq->orig_translation.y - result.translation.y);

    /* Scale. */
    transform->scale_x = tdseq->orig_scale.x * result.scale.x;
    transform->scale_y = tdseq->orig_scale.y * result.scale.y;

    /* Rotation. Scaling can cause negative rotation. */
    if (t->mode == TFM_ROTATION) {
      transform->rotation = tdseq->orig_rotation + result.rotation;
    }

    if (t->mode == TFM_MIRROR) {

      transform->xofs *= t->values_final[0];
      transform->yofs *= t->values_final[1];

      if (t->orient_curr == O_SET) {
        if (strip == ed->act_strip) {
          transform->rotation = tdseq->orig_rotation;
        }
        else {
          transform->rotation = tdseq->orig_rotation + (2 * tdseq->active_seq_orig_rotation);
        }
      }
      else {
        strip->flag = tdseq->orig_flag;
        if (t->values_final[0] == -1) {
          strip->flag ^= SEQ_FLIPX;
        }
        if (t->values_final[1] == -1) {
          strip->flag ^= SEQ_FLIPY;
        }
        transform->rotation = tdseq->orig_rotation;
      }
    }

    if ((t->animtimer) && animrig::is_autokey_on(scene)) {
      animrecord_check_state(t, &scene->id);
      autokeyframe_sequencer_image(t->context, scene, transform, t->mode);
    }

    seq::relations_invalidate_cache(scene, strip);
  }
}

static float2 calculate_translation_offset(TransInfo *t, TransDataSeq *tdseq)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  Strip *strip = tdseq->strip;
  StripTransform *transform = strip->data->transform;

  /* During modal operation, transform->*ofs is adjusted. Reset this value to original state, so
   * that new offset can be calculated. */
  transform->xofs = tdseq->orig_translation[0];
  transform->yofs = tdseq->orig_translation[1];

  const float2 viewport_pixel_aspect = {scene->r.xasp / scene->r.yasp, 1.0f};
  float2 mirror = seq::image_transform_mirror_factor_get(strip);

  Array<float2> quad_new = seq::image_transform_final_quad_get(scene, strip);
  return (quad_new[0] - tdseq->quad_orig[0]) * mirror / viewport_pixel_aspect;
}

static float2 calculate_new_origin_position(TransInfo *t, TransDataSeq *tdseq, TransData2D *td2d)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  Strip *strip = tdseq->strip;

  const float2 image_size = seq::transform_image_raw_size_get(scene, strip);

  const float2 viewport_pixel_aspect = {scene->r.xasp / scene->r.yasp, 1.0f};
  const float2 mirror = seq::image_transform_mirror_factor_get(strip);

  const float2 origin = tdseq->orig_origin_pixelspace;
  const float2 translation = transform_result_get(t, tdseq, td2d, strip).translation;
  const float2 origin_pixelspace_unscaled = origin / viewport_pixel_aspect * mirror;
  const float2 origin_translated = origin_pixelspace_unscaled - translation;
  const float2 origin_raw_space = math::transform_point(tdseq->orig_matrix, origin_translated);
  const float2 origin_abs = origin_raw_space + image_size / 2;
  const float2 origin_rel = origin_abs / image_size;
  return origin_rel;
}

static void image_origin_set(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransData *td = nullptr;
  TransData2D *td2d = nullptr;
  Scene *scene = CTX_data_sequencer_scene(t->context);
  int i;

  for (i = 0, td = tc->data, td2d = tc->data_2d; i < tc->data_len; i += 3, td += 3, td2d += 3) {
    TransDataSeq *tdseq = static_cast<TransDataSeq *>(td->extra);
    Strip *strip = tdseq->strip;
    StripTransform *transform = strip->data->transform;

    const float2 origin_rel = calculate_new_origin_position(t, tdseq, td2d);
    transform->origin[0] = origin_rel.x;
    transform->origin[1] = origin_rel.y;

    /* Calculate offset, so image does not change it's position in preview. */
    float2 delta_translation = calculate_translation_offset(t, tdseq);
    transform->xofs = tdseq->orig_translation.x - delta_translation.x;
    transform->yofs = tdseq->orig_translation.y - delta_translation.y;

    seq::relations_invalidate_cache(scene, strip);
  }
}

static void recalcData_sequencer_image(TransInfo *t)
{
  if ((t->flag & T_ORIGIN) == 0) {
    image_transform_set(t);
  }
  else {
    image_origin_set(t);
  }
}

static void special_aftertrans_update__sequencer_image(bContext *C, TransInfo *t)
{

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransData *td = nullptr;
  TransData2D *td2d = nullptr;
  Scene *scene = CTX_data_sequencer_scene(C);
  int i;

  for (i = 0, td = tc->data, td2d = tc->data_2d; i < tc->data_len; i += 3, td += 3, td2d += 3) {
    TransDataSeq *tdseq = static_cast<TransDataSeq *>(td->extra);
    Strip *strip = tdseq->strip;
    StripTransform *transform = strip->data->transform;
    if (t->state == TRANS_CANCEL) {
      transform->xofs = tdseq->orig_translation.x;
      transform->yofs = tdseq->orig_translation.y;
      transform->rotation = tdseq->orig_rotation;
      transform->scale_x = tdseq->orig_scale.x;
      transform->scale_y = tdseq->orig_scale.y;
      transform->origin[0] = tdseq->orig_origin_relative.x;
      transform->origin[1] = tdseq->orig_origin_relative.y;
      strip->flag = tdseq->orig_flag;
      continue;
    }

    if (animrig::is_autokey_on(scene)) {
      autokeyframe_sequencer_image(t->context, scene, transform, t->mode);
    }
  }
}

TransConvertTypeInfo TransConvertType_SequencerImage = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransSeqImageData,
    /*recalc_data*/ recalcData_sequencer_image,
    /*special_aftertrans_update*/ special_aftertrans_update__sequencer_image,
};

}  // namespace blender::ed::transform
