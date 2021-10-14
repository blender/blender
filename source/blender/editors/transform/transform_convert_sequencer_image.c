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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_report.h"

#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

#include "UI_view2d.h"

#include "transform.h"
#include "transform_convert.h"

/** Used for sequencer transform. */
typedef struct TransDataSeq {
  struct Sequence *seq;
  float orig_origin_position[2];
  float orig_translation[2];
  float orig_scale[2];
  float orig_rotation;
} TransDataSeq;

static TransData *SeqToTransData(const Scene *scene,
                                 Sequence *seq,
                                 TransData *td,
                                 TransData2D *td2d,
                                 TransDataSeq *tdseq,
                                 int vert_index)
{
  const StripTransform *transform = seq->strip->transform;
  float origin[2];
  SEQ_image_transform_origin_offset_pixelspace_get(scene, seq, origin);
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
  td2d->loc2d = NULL;
  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);

  td->center[0] = origin[0];
  td->center[1] = origin[1];

  unit_m3(td->mtx);
  unit_m3(td->smtx);
  unit_m3(td->axismtx);

  rotate_m3(td->axismtx, transform->rotation);
  normalize_m3(td->axismtx);

  tdseq->seq = seq;
  copy_v2_v2(tdseq->orig_origin_position, origin);
  tdseq->orig_translation[0] = transform->xofs;
  tdseq->orig_translation[1] = transform->yofs;
  tdseq->orig_scale[0] = transform->scale_x;
  tdseq->orig_scale[1] = transform->scale_y;
  tdseq->orig_rotation = transform->rotation;

  td->extra = (void *)tdseq;
  td->ext = NULL;
  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  return td;
}

static void freeSeqData(TransInfo *UNUSED(t),
                        TransDataContainer *tc,
                        TransCustomData *UNUSED(custom_data))
{
  TransData *td = (TransData *)tc->data;
  MEM_freeN(td->extra);
}

void createTransSeqImageData(TransInfo *t)
{
  Editing *ed = SEQ_editing_get(t->scene);

  if (ed == NULL) {
    return;
  }

  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  SeqCollection *strips = SEQ_query_rendered_strips(seqbase, t->scene->r.cfra, 0);
  SEQ_filter_selected_strips(strips);

  const int count = SEQ_collection_len(strips);
  if (count == 0) {
    SEQ_collection_free(strips);
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  tc->custom.type.free_cb = freeSeqData;

  tc->data_len = count * 3; /* 3 vertices per sequence are needed. */
  TransData *td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransSeq TransData");
  TransData2D *td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D),
                                                "TransSeq TransData2D");
  TransDataSeq *tdseq = MEM_callocN(tc->data_len * sizeof(TransDataSeq), "TransSeq TransDataSeq");

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, strips) {
    /* One `Sequence` needs 3 `TransData` entries - center point placed in image origin, then 2
     * points offset by 1 in X and Y direction respectively, so rotation and scale can be
     * calculated from these points. */
    SeqToTransData(t->scene, seq, td++, td2d++, tdseq++, 0);
    SeqToTransData(t->scene, seq, td++, td2d++, tdseq++, 1);
    SeqToTransData(t->scene, seq, td++, td2d++, tdseq++, 2);
  }

  SEQ_collection_free(strips);
}

void recalcData_sequencer_image(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  int i;

  for (i = 0, td = tc->data, td2d = tc->data_2d; i < tc->data_len; i++, td++, td2d++) {
    /* Origin. */
    float origin[2];
    copy_v2_v2(origin, td2d->loc);
    i++, td++, td2d++;

    /* X and Y control points used to read scale and rotation. */
    float handle_x[2];
    copy_v2_v2(handle_x, td2d->loc);
    sub_v2_v2(handle_x, origin);
    i++, td++, td2d++;
    float handle_y[2];
    copy_v2_v2(handle_y, td2d->loc);
    sub_v2_v2(handle_y, origin);

    TransDataSeq *tdseq = td->extra;
    Sequence *seq = tdseq->seq;
    StripTransform *transform = seq->strip->transform;
    float mirror[2];
    SEQ_image_transform_mirror_factor_get(seq, mirror);

    /* Calculate translation. */
    float translation[2];
    copy_v2_v2(translation, tdseq->orig_origin_position);
    sub_v2_v2(translation, origin);
    mul_v2_v2(translation, mirror);

    transform->xofs = tdseq->orig_translation[0] - translation[0];
    transform->yofs = tdseq->orig_translation[1] - translation[1];

    /* Scale. */
    transform->scale_x = tdseq->orig_scale[0] * fabs(len_v2(handle_x));
    transform->scale_y = tdseq->orig_scale[1] * fabs(len_v2(handle_y));

    /* Rotation. Scaling can cause negative rotation. */
    if (t->mode == TFM_ROTATION) {
      const float orig_dir[2] = {cosf(tdseq->orig_rotation), sinf(tdseq->orig_rotation)};
      float rotation = angle_signed_v2v2(handle_x, orig_dir) * mirror[0] * mirror[1];
      transform->rotation = tdseq->orig_rotation + rotation;
      transform->rotation += DEG2RAD(360.0);
      transform->rotation = fmod(transform->rotation, DEG2RAD(360.0));
    }
    SEQ_relations_invalidate_cache_preprocessed(t->scene, seq);
  }
}
