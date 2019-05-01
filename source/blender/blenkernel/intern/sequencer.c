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
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

/** \file
 * \ingroup bke
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "MEM_guardedalloc.h"

#include "DNA_sequence_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_sound_types.h"

#include "BLI_math.h"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#else
#  include <unistd.h>
#endif

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_sequencer.h"
#include "BKE_movieclip.h"
#include "BKE_fcurve.h"
#include "BKE_scene.h"
#include "BKE_mask.h"
#include "BKE_library.h"
#include "BKE_idprop.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"

#include "RE_pipeline.h"

#include <pthread.h>

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"
#include "IMB_metadata.h"

#include "BKE_context.h"
#include "BKE_sound.h"

#include "RE_engine.h"

#ifdef WITH_AUDASPACE
#  include <AUD_Special.h>
#endif

/* mutable state for sequencer */
typedef struct SeqRenderState {
  LinkNode *scene_parents;
} SeqRenderState;

static ImBuf *seq_render_strip_stack(const SeqRenderData *context,
                                     SeqRenderState *state,
                                     ListBase *seqbasep,
                                     float cfra,
                                     int chanshown);
static ImBuf *seq_render_strip(const SeqRenderData *context,
                               SeqRenderState *state,
                               Sequence *seq,
                               float cfra);
static void seq_free_animdata(Scene *scene, Sequence *seq);
static ImBuf *seq_render_mask(const SeqRenderData *context, Mask *mask, float nr, bool make_float);
static int seq_num_files(Scene *scene, char views_format, const bool is_multiview);
static void seq_anim_add_suffix(Scene *scene, struct anim *anim, const int view_id);

/* **** XXX ******** */
#define SELECT 1
ListBase seqbase_clipboard;
int seqbase_clipboard_frame;
SequencerDrawView sequencer_view3d_cb = NULL; /* NULL in background mode */

#if 0 /* unused function */
static void printf_strip(Sequence *seq)
{
  fprintf(stderr,
          "name: '%s', len:%d, start:%d, (startofs:%d, endofs:%d), "
          "(startstill:%d, endstill:%d), machine:%d, (startdisp:%d, enddisp:%d)\n",
          seq->name,
          seq->len,
          seq->start,
          seq->startofs,
          seq->endofs,
          seq->startstill,
          seq->endstill,
          seq->machine,
          seq->startdisp,
          seq->enddisp);

  fprintf(stderr,
          "\tseq_tx_set_final_left: %d %d\n\n",
          seq_tx_get_final_left(seq, 0),
          seq_tx_get_final_right(seq, 0));
}
#endif

static void sequencer_state_init(SeqRenderState *state)
{
  state->scene_parents = NULL;
}

int BKE_sequencer_base_recursive_apply(ListBase *seqbase,
                                       int (*apply_func)(Sequence *seq, void *),
                                       void *arg)
{
  Sequence *iseq;
  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    if (BKE_sequencer_recursive_apply(iseq, apply_func, arg) == -1) {
      return -1; /* bail out */
    }
  }
  return 1;
}

int BKE_sequencer_recursive_apply(Sequence *seq, int (*apply_func)(Sequence *, void *), void *arg)
{
  int ret = apply_func(seq, arg);

  if (ret == -1) {
    return -1; /* bail out */
  }

  if (ret && seq->seqbase.first) {
    ret = BKE_sequencer_base_recursive_apply(&seq->seqbase, apply_func, arg);
  }

  return ret;
}

/*********************** alloc / free functions *************************/

/* free */

static void free_proxy_seq(Sequence *seq)
{
  if (seq->strip && seq->strip->proxy && seq->strip->proxy->anim) {
    IMB_free_anim(seq->strip->proxy->anim);
    seq->strip->proxy->anim = NULL;
  }
}

static void seq_free_strip(Strip *strip)
{
  strip->us--;
  if (strip->us > 0) {
    return;
  }
  if (strip->us < 0) {
    printf("error: negative users in strip\n");
    return;
  }

  if (strip->stripdata) {
    MEM_freeN(strip->stripdata);
  }

  if (strip->proxy) {
    if (strip->proxy->anim) {
      IMB_free_anim(strip->proxy->anim);
    }

    MEM_freeN(strip->proxy);
  }
  if (strip->crop) {
    MEM_freeN(strip->crop);
  }
  if (strip->transform) {
    MEM_freeN(strip->transform);
  }

  MEM_freeN(strip);
}

/* only give option to skip cache locally (static func) */
static void BKE_sequence_free_ex(Scene *scene,
                                 Sequence *seq,
                                 const bool do_cache,
                                 const bool do_id_user)
{
  if (seq->strip) {
    seq_free_strip(seq->strip);
  }

  BKE_sequence_free_anim(seq);

  if (seq->type & SEQ_TYPE_EFFECT) {
    struct SeqEffectHandle sh = BKE_sequence_get_effect(seq);
    sh.free(seq, do_id_user);
  }

  if (seq->sound && do_id_user) {
    id_us_min(((ID *)seq->sound));
  }

  if (seq->stereo3d_format) {
    MEM_freeN(seq->stereo3d_format);
  }

  /* clipboard has no scene and will never have a sound handle or be active
   * same goes to sequences copy for proxy rebuild job
   */
  if (scene) {
    Editing *ed = scene->ed;

    if (ed->act_seq == seq) {
      ed->act_seq = NULL;
    }

    if (seq->scene_sound && ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
      BKE_sound_remove_scene_sound(scene, seq->scene_sound);
    }

    seq_free_animdata(scene, seq);
  }

  if (seq->prop) {
    IDP_FreeProperty_ex(seq->prop, do_id_user);
    MEM_freeN(seq->prop);
  }

  /* free modifiers */
  BKE_sequence_modifier_clear(seq);

  /* free cached data used by this strip,
   * also invalidate cache for all dependent sequences
   *
   * be _very_ careful here, invalidating cache loops over the scene sequences and
   * assumes the listbase is valid for all strips,
   * this may not be the case if lists are being freed.
   * this is optional BKE_sequence_invalidate_cache
   */
  if (do_cache) {
    if (scene) {
      BKE_sequence_invalidate_cache(scene, seq);
    }
  }

  MEM_freeN(seq);
}

void BKE_sequence_free(Scene *scene, Sequence *seq)
{
  BKE_sequence_free_ex(scene, seq, true, true);
}

/* Function to free imbuf and anim data on changes */
void BKE_sequence_free_anim(Sequence *seq)
{
  while (seq->anims.last) {
    StripAnim *sanim = seq->anims.last;

    if (sanim->anim) {
      IMB_free_anim(sanim->anim);
      sanim->anim = NULL;
    }

    BLI_freelinkN(&seq->anims, sanim);
  }
  BLI_listbase_clear(&seq->anims);
}

/* cache must be freed before calling this function
 * since it leaves the seqbase in an invalid state */
static void seq_free_sequence_recurse(Scene *scene, Sequence *seq, const bool do_id_user)
{
  Sequence *iseq, *iseq_next;

  for (iseq = seq->seqbase.first; iseq; iseq = iseq_next) {
    iseq_next = iseq->next;
    seq_free_sequence_recurse(scene, iseq, do_id_user);
  }

  BKE_sequence_free_ex(scene, seq, false, do_id_user);
}

Editing *BKE_sequencer_editing_get(Scene *scene, bool alloc)
{
  if (alloc) {
    BKE_sequencer_editing_ensure(scene);
  }
  return scene->ed;
}

void BKE_sequencer_free_clipboard(void)
{
  Sequence *seq, *nseq;

  BKE_sequencer_base_clipboard_pointers_free(&seqbase_clipboard);

  for (seq = seqbase_clipboard.first; seq; seq = nseq) {
    nseq = seq->next;
    seq_free_sequence_recurse(NULL, seq, false);
  }
  BLI_listbase_clear(&seqbase_clipboard);
}

/* -------------------------------------------------------------------- */
/* Manage pointers in the clipboard.
 * note that these pointers should _never_ be access in the sequencer,
 * they are only for storage while in the clipboard
 * notice 'newid' is used for temp pointer storage here, validate on access (this is safe usage,
 * since those datablocks are fully out of Main lists).
 */
#define ID_PT (*id_pt)
static void seqclipboard_ptr_free(Main *UNUSED(bmain), ID **id_pt)
{
  if (ID_PT) {
    BLI_assert(ID_PT->newid != NULL);
    MEM_freeN(ID_PT);
    ID_PT = NULL;
  }
}
static void seqclipboard_ptr_store(Main *UNUSED(bmain), ID **id_pt)
{
  if (ID_PT) {
    ID *id_prev = ID_PT;
    ID_PT = MEM_dupallocN(ID_PT);
    ID_PT->newid = id_prev;
  }
}
static void seqclipboard_ptr_restore(Main *bmain, ID **id_pt)
{
  if (ID_PT) {
    const ListBase *lb = which_libbase(bmain, GS(ID_PT->name));
    void *id_restore;

    BLI_assert(ID_PT->newid != NULL);
    if (BLI_findindex(lb, (ID_PT)->newid) != -1) {
      /* the pointer is still valid */
      id_restore = (ID_PT)->newid;
    }
    else {
      /* the pointer of the same name still exists  */
      id_restore = BLI_findstring(lb, (ID_PT)->name + 2, offsetof(ID, name) + 2);
    }

    if (id_restore == NULL) {
      /* check for a data with the same filename */
      switch (GS(ID_PT->name)) {
        case ID_SO: {
          id_restore = BLI_findstring(lb, ((bSound *)ID_PT)->name, offsetof(bSound, name));
          if (id_restore == NULL) {
            id_restore = BKE_sound_new_file(bmain, ((bSound *)ID_PT)->name);
            (ID_PT)->newid = id_restore; /* reuse next time */
          }
          break;
        }
        case ID_MC: {
          id_restore = BLI_findstring(lb, ((MovieClip *)ID_PT)->name, offsetof(MovieClip, name));
          if (id_restore == NULL) {
            id_restore = BKE_movieclip_file_add(bmain, ((MovieClip *)ID_PT)->name);
            (ID_PT)->newid = id_restore; /* reuse next time */
          }
          break;
        }
        default:
          break;
      }
    }

    /* Replace with pointer to actual datablock. */
    seqclipboard_ptr_free(bmain, id_pt);
    ID_PT = id_restore;
  }
}
#undef ID_PT

static void sequence_clipboard_pointers(Main *bmain,
                                        Sequence *seq,
                                        void (*callback)(Main *, ID **))
{
  callback(bmain, (ID **)&seq->scene);
  callback(bmain, (ID **)&seq->scene_camera);
  callback(bmain, (ID **)&seq->clip);
  callback(bmain, (ID **)&seq->mask);
  callback(bmain, (ID **)&seq->sound);

  if (seq->type == SEQ_TYPE_TEXT && seq->effectdata) {
    TextVars *text_data = seq->effectdata;
    callback(bmain, (ID **)&text_data->text_font);
  }
}

/* recursive versions of functions above */
void BKE_sequencer_base_clipboard_pointers_free(ListBase *seqbase)
{
  Sequence *seq;
  for (seq = seqbase->first; seq; seq = seq->next) {
    sequence_clipboard_pointers(NULL, seq, seqclipboard_ptr_free);
    BKE_sequencer_base_clipboard_pointers_free(&seq->seqbase);
  }
}
void BKE_sequencer_base_clipboard_pointers_store(Main *bmain, ListBase *seqbase)
{
  Sequence *seq;
  for (seq = seqbase->first; seq; seq = seq->next) {
    sequence_clipboard_pointers(bmain, seq, seqclipboard_ptr_store);
    BKE_sequencer_base_clipboard_pointers_store(bmain, &seq->seqbase);
  }
}
void BKE_sequencer_base_clipboard_pointers_restore(ListBase *seqbase, Main *bmain)
{
  Sequence *seq;
  for (seq = seqbase->first; seq; seq = seq->next) {
    sequence_clipboard_pointers(bmain, seq, seqclipboard_ptr_restore);
    BKE_sequencer_base_clipboard_pointers_restore(&seq->seqbase, bmain);
  }
}

/* end clipboard pointer mess */

Editing *BKE_sequencer_editing_ensure(Scene *scene)
{
  if (scene->ed == NULL) {
    Editing *ed;

    ed = scene->ed = MEM_callocN(sizeof(Editing), "addseq");
    ed->seqbasep = &ed->seqbase;
    ed->cache = NULL;
    ed->cache_flag = SEQ_CACHE_STORE_FINAL_OUT;
    ed->cache_flag |= SEQ_CACHE_VIEW_FINAL_OUT;
    ed->cache_flag |= SEQ_CACHE_VIEW_ENABLE;
    ed->recycle_max_cost = 10.0f;
  }

  return scene->ed;
}

void BKE_sequencer_editing_free(Scene *scene, const bool do_id_user)
{
  Editing *ed = scene->ed;
  Sequence *seq;

  if (ed == NULL) {
    return;
  }

  BKE_sequencer_cache_destruct(scene);

  SEQ_BEGIN (ed, seq) {
    /* handle cache freeing above */
    BKE_sequence_free_ex(scene, seq, false, do_id_user);
  }
  SEQ_END;

  BLI_freelistN(&ed->metastack);

  MEM_freeN(ed);

  scene->ed = NULL;
}

/*********************** Sequencer color space functions  *************************/

static void sequencer_imbuf_assign_spaces(Scene *scene, ImBuf *ibuf)
{
#if 0
  /* Bute buffer is supposed to be in sequencer working space already. */
  if (ibuf->rect != NULL) {
    IMB_colormanagement_assign_rect_colorspace(ibuf, scene->sequencer_colorspace_settings.name);
  }
#endif
  if (ibuf->rect_float != NULL) {
    IMB_colormanagement_assign_float_colorspace(ibuf, scene->sequencer_colorspace_settings.name);
  }
}

void BKE_sequencer_imbuf_to_sequencer_space(Scene *scene, ImBuf *ibuf, bool make_float)
{
  /* Early output check: if both buffers are NULL we have nothing to convert. */
  if (ibuf->rect_float == NULL && ibuf->rect == NULL) {
    return;
  }
  /* Get common conversion settings. */
  const char *to_colorspace = scene->sequencer_colorspace_settings.name;
  /* Perform actual conversion logic. */
  if (ibuf->rect_float == NULL) {
    /* We are not requested to give float buffer and byte buffer is already
     * in thee required colorspace. Can skip doing anything here.
     */
    const char *from_colorspace = IMB_colormanagement_get_rect_colorspace(ibuf);
    if (!make_float && STREQ(from_colorspace, to_colorspace)) {
      return;
    }
    if (false) {
      /* The idea here is to provide as fast playback as possible and
       * enforcing float buffer here (a) uses more cache memory (b) might
       * make some other effects slower to apply.
       *
       * However, this might also have negative effect by adding weird
       * artifacts which will then not happen in final render.
       */
      IMB_colormanagement_transform_byte_threaded((unsigned char *)ibuf->rect,
                                                  ibuf->x,
                                                  ibuf->y,
                                                  ibuf->channels,
                                                  from_colorspace,
                                                  to_colorspace);
    }
    else {
      /* We perform conversion to a float buffer so we don't worry about
       * precision loss.
       */
      imb_addrectfloatImBuf(ibuf);
      IMB_colormanagement_transform_from_byte_threaded(ibuf->rect_float,
                                                       (unsigned char *)ibuf->rect,
                                                       ibuf->x,
                                                       ibuf->y,
                                                       ibuf->channels,
                                                       from_colorspace,
                                                       to_colorspace);
      /* We don't need byte buffer anymore. */
      imb_freerectImBuf(ibuf);
    }
  }
  else {
    const char *from_colorspace = IMB_colormanagement_get_float_colorspace(ibuf);
    /* Unknown input color space, can't perform conversion. */
    if (from_colorspace == NULL || from_colorspace[0] == '\0') {
      return;
    }
    /* We don't want both byte and float buffers around: they'll either run
     * out of sync or conversion of byte buffer will loose precision in there.
     */
    if (ibuf->rect != NULL) {
      imb_freerectImBuf(ibuf);
    }
    IMB_colormanagement_transform_threaded(
        ibuf->rect_float, ibuf->x, ibuf->y, ibuf->channels, from_colorspace, to_colorspace, true);
  }
  sequencer_imbuf_assign_spaces(scene, ibuf);
}

void BKE_sequencer_imbuf_from_sequencer_space(Scene *scene, ImBuf *ibuf)
{
  const char *from_colorspace = scene->sequencer_colorspace_settings.name;
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);

  if (!ibuf->rect_float) {
    return;
  }

  if (to_colorspace && to_colorspace[0] != '\0') {
    IMB_colormanagement_transform_threaded(
        ibuf->rect_float, ibuf->x, ibuf->y, ibuf->channels, from_colorspace, to_colorspace, true);
    IMB_colormanagement_assign_float_colorspace(ibuf, to_colorspace);
  }
}

void BKE_sequencer_pixel_from_sequencer_space_v4(struct Scene *scene, float pixel[4])
{
  const char *from_colorspace = scene->sequencer_colorspace_settings.name;
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);

  if (to_colorspace && to_colorspace[0] != '\0') {
    IMB_colormanagement_transform_v4(pixel, from_colorspace, to_colorspace);
  }
  else {
    /* if no color management enables fallback to legacy conversion */
    srgb_to_linearrgb_v4(pixel, pixel);
  }
}

/*********************** sequencer pipeline functions *************************/

void BKE_sequencer_new_render_data(Main *bmain,
                                   struct Depsgraph *depsgraph,
                                   Scene *scene,
                                   int rectx,
                                   int recty,
                                   int preview_render_size,
                                   int for_render,
                                   SeqRenderData *r_context)
{
  r_context->bmain = bmain;
  r_context->depsgraph = depsgraph;
  r_context->scene = scene;
  r_context->rectx = rectx;
  r_context->recty = recty;
  r_context->preview_render_size = preview_render_size;
  r_context->for_render = for_render;
  r_context->motion_blur_samples = 0;
  r_context->motion_blur_shutter = 0;
  r_context->skip_cache = false;
  r_context->is_proxy_render = false;
  r_context->view_id = 0;
  r_context->gpu_offscreen = NULL;
  r_context->gpu_samples = (scene->r.mode & R_OSA) ? scene->r.osa : 0;
  r_context->gpu_full_samples = (r_context->gpu_samples) && (scene->r.scemode & R_FULL_SAMPLE);
}

/* ************************* iterator ************************** */
/* *************** (replaces old WHILE_SEQ) ********************* */
/* **************** use now SEQ_BEGIN () SEQ_END ***************** */

/* sequence strip iterator:
 * - builds a full array, recursively into meta strips
 */

static void seq_count(ListBase *seqbase, int *tot)
{
  Sequence *seq;

  for (seq = seqbase->first; seq; seq = seq->next) {
    (*tot)++;

    if (seq->seqbase.first) {
      seq_count(&seq->seqbase, tot);
    }
  }
}

static void seq_build_array(ListBase *seqbase, Sequence ***array, int depth)
{
  Sequence *seq;

  for (seq = seqbase->first; seq; seq = seq->next) {
    seq->depth = depth;

    if (seq->seqbase.first) {
      seq_build_array(&seq->seqbase, array, depth + 1);
    }

    **array = seq;
    (*array)++;
  }
}

static void seq_array(Editing *ed, Sequence ***seqarray, int *tot, bool use_pointer)
{
  Sequence **array;

  *seqarray = NULL;
  *tot = 0;

  if (ed == NULL) {
    return;
  }

  if (use_pointer) {
    seq_count(ed->seqbasep, tot);
  }
  else {
    seq_count(&ed->seqbase, tot);
  }

  if (*tot == 0) {
    return;
  }

  *seqarray = array = MEM_mallocN(sizeof(Sequence *) * (*tot), "SeqArray");
  if (use_pointer) {
    seq_build_array(ed->seqbasep, &array, 0);
  }
  else {
    seq_build_array(&ed->seqbase, &array, 0);
  }
}

void BKE_sequence_iterator_begin(Editing *ed, SeqIterator *iter, bool use_pointer)
{
  memset(iter, 0, sizeof(*iter));
  seq_array(ed, &iter->array, &iter->tot, use_pointer);

  if (iter->tot) {
    iter->cur = 0;
    iter->seq = iter->array[iter->cur];
    iter->valid = 1;
  }
}

void BKE_sequence_iterator_next(SeqIterator *iter)
{
  if (++iter->cur < iter->tot) {
    iter->seq = iter->array[iter->cur];
  }
  else {
    iter->valid = 0;
  }
}

void BKE_sequence_iterator_end(SeqIterator *iter)
{
  if (iter->array) {
    MEM_freeN(iter->array);
  }

  iter->valid = 0;
}

static int metaseq_start(Sequence *metaseq)
{
  return metaseq->start + metaseq->startofs;
}

static int metaseq_end(Sequence *metaseq)
{
  return metaseq->start + metaseq->len - metaseq->endofs;
}

static void seq_update_sound_bounds_recursive_rec(Scene *scene,
                                                  Sequence *metaseq,
                                                  int start,
                                                  int end)
{
  Sequence *seq;

  /* for sound we go over full meta tree to update bounds of the sound strips,
   * since sound is played outside of evaluating the imbufs, */
  for (seq = metaseq->seqbase.first; seq; seq = seq->next) {
    if (seq->type == SEQ_TYPE_META) {
      seq_update_sound_bounds_recursive_rec(
          scene, seq, max_ii(start, metaseq_start(seq)), min_ii(end, metaseq_end(seq)));
    }
    else if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
      if (seq->scene_sound) {
        int startofs = seq->startofs;
        int endofs = seq->endofs;
        if (seq->startofs + seq->start < start) {
          startofs = start - seq->start;
        }

        if (seq->start + seq->len - seq->endofs > end) {
          endofs = seq->start + seq->len - end;
        }

        BKE_sound_move_scene_sound(scene,
                                   seq->scene_sound,
                                   seq->start + startofs,
                                   seq->start + seq->len - endofs,
                                   startofs + seq->anim_startofs);
      }
    }
  }
}

static void seq_update_sound_bounds_recursive(Scene *scene, Sequence *metaseq)
{
  seq_update_sound_bounds_recursive_rec(
      scene, metaseq, metaseq_start(metaseq), metaseq_end(metaseq));
}

void BKE_sequence_calc_disp(Scene *scene, Sequence *seq)
{
  if (seq->startofs && seq->startstill) {
    seq->startstill = 0;
  }
  if (seq->endofs && seq->endstill) {
    seq->endstill = 0;
  }

  seq->startdisp = seq->start + seq->startofs - seq->startstill;
  seq->enddisp = seq->start + seq->len - seq->endofs + seq->endstill;

  seq->handsize = 10.0; /* 10 frames */
  if (seq->enddisp - seq->startdisp < 10) {
    seq->handsize = (float)(0.5 * (seq->enddisp - seq->startdisp));
  }
  else if (seq->enddisp - seq->startdisp > 250) {
    seq->handsize = (float)((seq->enddisp - seq->startdisp) / 25);
  }

  if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
    BKE_sequencer_update_sound_bounds(scene, seq);
  }
  else if (seq->type == SEQ_TYPE_META) {
    seq_update_sound_bounds_recursive(scene, seq);
  }
}

void BKE_sequence_calc(Scene *scene, Sequence *seq)
{
  Sequence *seqm;
  int min, max;

  /* check all metas recursively */
  seqm = seq->seqbase.first;
  while (seqm) {
    if (seqm->seqbase.first) {
      BKE_sequence_calc(scene, seqm);
    }
    seqm = seqm->next;
  }

  /* effects and meta: automatic start and end */

  if (seq->type & SEQ_TYPE_EFFECT) {
    /* pointers */
    if (seq->seq2 == NULL) {
      seq->seq2 = seq->seq1;
    }
    if (seq->seq3 == NULL) {
      seq->seq3 = seq->seq1;
    }

    /* effecten go from seq1 -> seq2: test */

    /* we take the largest start and smallest end */

    // seq->start = seq->startdisp = MAX2(seq->seq1->startdisp, seq->seq2->startdisp);
    // seq->enddisp = MIN2(seq->seq1->enddisp, seq->seq2->enddisp);

    if (seq->seq1) {
      /* XXX These resets should not be necessary, but users used to be able to
       *     edit effect's length, leading to strange results. See [#29190] */
      seq->startofs = seq->endofs = seq->startstill = seq->endstill = 0;
      seq->start = seq->startdisp = max_iii(
          seq->seq1->startdisp, seq->seq2->startdisp, seq->seq3->startdisp);
      seq->enddisp = min_iii(seq->seq1->enddisp, seq->seq2->enddisp, seq->seq3->enddisp);
      /* we cant help if strips don't overlap, it wont give useful results.
       * but at least ensure 'len' is never negative which causes bad bugs elsewhere. */
      if (seq->enddisp < seq->startdisp) {
        /* simple start/end swap */
        seq->start = seq->enddisp;
        seq->enddisp = seq->startdisp;
        seq->startdisp = seq->start;
        seq->flag |= SEQ_INVALID_EFFECT;
      }
      else {
        seq->flag &= ~SEQ_INVALID_EFFECT;
      }

      seq->len = seq->enddisp - seq->startdisp;
    }
    else {
      BKE_sequence_calc_disp(scene, seq);
    }
  }
  else {
    if (seq->type == SEQ_TYPE_META) {
      seqm = seq->seqbase.first;
      if (seqm) {
        min = MAXFRAME * 2;
        max = -MAXFRAME * 2;
        while (seqm) {
          if (seqm->startdisp < min) {
            min = seqm->startdisp;
          }
          if (seqm->enddisp > max) {
            max = seqm->enddisp;
          }
          seqm = seqm->next;
        }
        seq->start = min + seq->anim_startofs;
        seq->len = max - min;
        seq->len -= seq->anim_startofs;
        seq->len -= seq->anim_endofs;
      }
      seq_update_sound_bounds_recursive(scene, seq);
    }
    BKE_sequence_calc_disp(scene, seq);
  }
}

static void seq_multiview_name(Scene *scene,
                               const int view_id,
                               const char *prefix,
                               const char *ext,
                               char *r_path,
                               size_t r_size)
{
  const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, view_id);
  BLI_snprintf(r_path, r_size, "%s%s%s", prefix, suffix, ext);
}

/* note: caller should run BKE_sequence_calc(scene, seq) after */
void BKE_sequence_reload_new_file(Scene *scene, Sequence *seq, const bool lock_range)
{
  char path[FILE_MAX];
  int prev_startdisp = 0, prev_enddisp = 0;
  /* note: don't rename the strip, will break animation curves */

  if (ELEM(seq->type,
           SEQ_TYPE_MOVIE,
           SEQ_TYPE_IMAGE,
           SEQ_TYPE_SOUND_RAM,
           SEQ_TYPE_SCENE,
           SEQ_TYPE_META,
           SEQ_TYPE_MOVIECLIP,
           SEQ_TYPE_MASK) == 0) {
    return;
  }

  if (lock_range) {
    /* keep so we don't have to move the actual start and end points (only the data) */
    BKE_sequence_calc_disp(scene, seq);
    prev_startdisp = seq->startdisp;
    prev_enddisp = seq->enddisp;
  }

  switch (seq->type) {
    case SEQ_TYPE_IMAGE: {
      /* Hack? */
      size_t olen = MEM_allocN_len(seq->strip->stripdata) / sizeof(StripElem);

      seq->len = olen;
      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    }
    case SEQ_TYPE_MOVIE: {
      StripAnim *sanim;
      bool is_multiview_loaded = false;
      const bool is_multiview = (seq->flag & SEQ_USE_VIEWS) != 0 &&
                                (scene->r.scemode & R_MULTIVIEW) != 0;

      BLI_join_dirfile(path, sizeof(path), seq->strip->dir, seq->strip->stripdata->name);
      BLI_path_abs(path, BKE_main_blendfile_path_from_global());

      BKE_sequence_free_anim(seq);

      if (is_multiview && (seq->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
        char prefix[FILE_MAX];
        const char *ext = NULL;
        const int totfiles = seq_num_files(scene, seq->views_format, true);
        int i = 0;

        BKE_scene_multiview_view_prefix_get(scene, path, prefix, &ext);

        if (prefix[0] != '\0') {
          for (i = 0; i < totfiles; i++) {
            struct anim *anim;
            char str[FILE_MAX];

            seq_multiview_name(scene, i, prefix, ext, str, FILE_MAX);
            anim = openanim(str,
                            IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                            seq->streamindex,
                            seq->strip->colorspace_settings.name);

            if (anim) {
              seq_anim_add_suffix(scene, anim, i);
              sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");
              BLI_addtail(&seq->anims, sanim);
              sanim->anim = anim;
            }
          }
          is_multiview_loaded = true;
        }
      }

      if (is_multiview_loaded == false) {
        struct anim *anim;
        anim = openanim(path,
                        IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                        seq->streamindex,
                        seq->strip->colorspace_settings.name);
        if (anim) {
          sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");
          BLI_addtail(&seq->anims, sanim);
          sanim->anim = anim;
        }
      }

      /* use the first video as reference for everything */
      sanim = seq->anims.first;

      if ((!sanim) || (!sanim->anim)) {
        return;
      }

      IMB_anim_load_metadata(sanim->anim);

      seq->len = IMB_anim_get_duration(
          sanim->anim, seq->strip->proxy ? seq->strip->proxy->tc : IMB_TC_RECORD_RUN);

      seq->anim_preseek = IMB_anim_get_preseek(sanim->anim);

      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    }
    case SEQ_TYPE_MOVIECLIP:
      if (seq->clip == NULL) {
        return;
      }

      seq->len = BKE_movieclip_get_duration(seq->clip);

      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    case SEQ_TYPE_MASK:
      if (seq->mask == NULL) {
        return;
      }
      seq->len = BKE_mask_get_duration(seq->mask);
      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    case SEQ_TYPE_SOUND_RAM:
#ifdef WITH_AUDASPACE
      if (!seq->sound) {
        return;
      }
      seq->len = ceil((double)AUD_getInfo(seq->sound->playback_handle).length * FPS);
      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
#else
      return;
#endif
      break;
    case SEQ_TYPE_SCENE: {
      seq->len = (seq->scene) ? seq->scene->r.efra - seq->scene->r.sfra + 1 : 0;
      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    }
  }

  free_proxy_seq(seq);

  if (lock_range) {
    BKE_sequence_tx_set_final_left(seq, prev_startdisp);
    BKE_sequence_tx_set_final_right(seq, prev_enddisp);
    BKE_sequence_single_fix(seq);
  }

  BKE_sequence_calc(scene, seq);
}

void BKE_sequencer_sort(Scene *scene)
{
  /* all strips together per kind, and in order of y location ("machine") */
  ListBase seqbase, effbase;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq, *seqt;

  if (ed == NULL) {
    return;
  }

  BLI_listbase_clear(&seqbase);
  BLI_listbase_clear(&effbase);

  while ((seq = BLI_pophead(ed->seqbasep))) {

    if (seq->type & SEQ_TYPE_EFFECT) {
      seqt = effbase.first;
      while (seqt) {
        if (seqt->machine >= seq->machine) {
          BLI_insertlinkbefore(&effbase, seqt, seq);
          break;
        }
        seqt = seqt->next;
      }
      if (seqt == NULL) {
        BLI_addtail(&effbase, seq);
      }
    }
    else {
      seqt = seqbase.first;
      while (seqt) {
        if (seqt->machine >= seq->machine) {
          BLI_insertlinkbefore(&seqbase, seqt, seq);
          break;
        }
        seqt = seqt->next;
      }
      if (seqt == NULL) {
        BLI_addtail(&seqbase, seq);
      }
    }
  }

  BLI_movelisttolist(&seqbase, &effbase);
  *(ed->seqbasep) = seqbase;
}

/** Comparison function suitable to be used with BLI_listbase_sort()... */
int BKE_sequencer_cmp_time_startdisp(const void *a, const void *b)
{
  const Sequence *seq_a = a;
  const Sequence *seq_b = b;

  return (seq_a->startdisp > seq_b->startdisp);
}

static int clear_scene_in_allseqs_cb(Sequence *seq, void *arg_pt)
{
  if (seq->scene == (Scene *)arg_pt) {
    seq->scene = NULL;
  }
  return 1;
}

void BKE_sequencer_clear_scene_in_allseqs(Main *bmain, Scene *scene)
{
  Scene *scene_iter;

  /* when a scene is deleted: test all seqs */
  for (scene_iter = bmain->scenes.first; scene_iter; scene_iter = scene_iter->id.next) {
    if (scene_iter != scene && scene_iter->ed) {
      BKE_sequencer_base_recursive_apply(
          &scene_iter->ed->seqbase, clear_scene_in_allseqs_cb, scene);
    }
  }
}

typedef struct SeqUniqueInfo {
  Sequence *seq;
  char name_src[SEQ_NAME_MAXSTR];
  char name_dest[SEQ_NAME_MAXSTR];
  int count;
  int match;
} SeqUniqueInfo;

static void seqbase_unique_name(ListBase *seqbasep, SeqUniqueInfo *sui)
{
  Sequence *seq;
  for (seq = seqbasep->first; seq; seq = seq->next) {
    if ((sui->seq != seq) && STREQ(sui->name_dest, seq->name + 2)) {
      /* SEQ_NAME_MAXSTR -4 for the number, -1 for \0, - 2 for prefix */
      BLI_snprintf(sui->name_dest,
                   sizeof(sui->name_dest),
                   "%.*s.%03d",
                   SEQ_NAME_MAXSTR - 4 - 1 - 2,
                   sui->name_src,
                   sui->count++);
      sui->match = 1; /* be sure to re-scan */
    }
  }
}

static int seqbase_unique_name_recursive_cb(Sequence *seq, void *arg_pt)
{
  if (seq->seqbase.first) {
    seqbase_unique_name(&seq->seqbase, (SeqUniqueInfo *)arg_pt);
  }
  return 1;
}

void BKE_sequence_base_unique_name_recursive(ListBase *seqbasep, Sequence *seq)
{
  SeqUniqueInfo sui;
  char *dot;
  sui.seq = seq;
  BLI_strncpy(sui.name_src, seq->name + 2, sizeof(sui.name_src));
  BLI_strncpy(sui.name_dest, seq->name + 2, sizeof(sui.name_dest));

  sui.count = 1;
  sui.match = 1; /* assume the worst to start the loop */

  /* Strip off the suffix */
  if ((dot = strrchr(sui.name_src, '.'))) {
    *dot = '\0';
    dot++;

    if (*dot) {
      sui.count = atoi(dot) + 1;
    }
  }

  while (sui.match) {
    sui.match = 0;
    seqbase_unique_name(seqbasep, &sui);
    BKE_sequencer_base_recursive_apply(seqbasep, seqbase_unique_name_recursive_cb, &sui);
  }

  BLI_strncpy(seq->name + 2, sui.name_dest, sizeof(seq->name) - 2);
}

static const char *give_seqname_by_type(int type)
{
  switch (type) {
    case SEQ_TYPE_META:
      return "Meta";
    case SEQ_TYPE_IMAGE:
      return "Image";
    case SEQ_TYPE_SCENE:
      return "Scene";
    case SEQ_TYPE_MOVIE:
      return "Movie";
    case SEQ_TYPE_MOVIECLIP:
      return "Clip";
    case SEQ_TYPE_MASK:
      return "Mask";
    case SEQ_TYPE_SOUND_RAM:
      return "Audio";
    case SEQ_TYPE_CROSS:
      return "Cross";
    case SEQ_TYPE_GAMCROSS:
      return "Gamma Cross";
    case SEQ_TYPE_ADD:
      return "Add";
    case SEQ_TYPE_SUB:
      return "Sub";
    case SEQ_TYPE_MUL:
      return "Mul";
    case SEQ_TYPE_ALPHAOVER:
      return "Alpha Over";
    case SEQ_TYPE_ALPHAUNDER:
      return "Alpha Under";
    case SEQ_TYPE_OVERDROP:
      return "Over Drop";
    case SEQ_TYPE_COLORMIX:
      return "Color Mix";
    case SEQ_TYPE_WIPE:
      return "Wipe";
    case SEQ_TYPE_GLOW:
      return "Glow";
    case SEQ_TYPE_TRANSFORM:
      return "Transform";
    case SEQ_TYPE_COLOR:
      return "Color";
    case SEQ_TYPE_MULTICAM:
      return "Multicam";
    case SEQ_TYPE_ADJUSTMENT:
      return "Adjustment";
    case SEQ_TYPE_SPEED:
      return "Speed";
    case SEQ_TYPE_GAUSSIAN_BLUR:
      return "Gaussian Blur";
    case SEQ_TYPE_TEXT:
      return "Text";
    default:
      return NULL;
  }
}

const char *BKE_sequence_give_name(Sequence *seq)
{
  const char *name = give_seqname_by_type(seq->type);

  if (!name) {
    if (!(seq->type & SEQ_TYPE_EFFECT)) {
      return seq->strip->dir;
    }
    else {
      return "Effect";
    }
  }
  return name;
}

ListBase *BKE_sequence_seqbase_get(Sequence *seq, int *r_offset)
{
  ListBase *seqbase = NULL;

  switch (seq->type) {
    case SEQ_TYPE_META: {
      seqbase = &seq->seqbase;
      *r_offset = seq->start;
      break;
    }
    case SEQ_TYPE_SCENE: {
      if (seq->flag & SEQ_SCENE_STRIPS && seq->scene) {
        Editing *ed = BKE_sequencer_editing_get(seq->scene, false);
        if (ed) {
          seqbase = &ed->seqbase;
          *r_offset = seq->scene->r.sfra;
        }
      }
      break;
    }
  }

  return seqbase;
}

/*********************** DO THE SEQUENCE *************************/

static void make_black_ibuf(ImBuf *ibuf)
{
  unsigned int *rect;
  float *rect_float;
  int tot;

  if (ibuf == NULL || (ibuf->rect == NULL && ibuf->rect_float == NULL)) {
    return;
  }

  tot = ibuf->x * ibuf->y;

  rect = ibuf->rect;
  rect_float = ibuf->rect_float;

  if (rect) {
    memset(rect, 0, tot * sizeof(char) * 4);
  }

  if (rect_float) {
    memset(rect_float, 0, tot * sizeof(float) * 4);
  }
}

static void multibuf(ImBuf *ibuf, const float fmul)
{
  char *rt;
  float *rt_float;

  int a;

  rt = (char *)ibuf->rect;
  rt_float = ibuf->rect_float;

  if (rt) {
    const int imul = (int)(256.0f * fmul);
    a = ibuf->x * ibuf->y;
    while (a--) {
      rt[0] = min_ii((imul * rt[0]) >> 8, 255);
      rt[1] = min_ii((imul * rt[1]) >> 8, 255);
      rt[2] = min_ii((imul * rt[2]) >> 8, 255);
      rt[3] = min_ii((imul * rt[3]) >> 8, 255);

      rt += 4;
    }
  }
  if (rt_float) {
    a = ibuf->x * ibuf->y;
    while (a--) {
      rt_float[0] *= fmul;
      rt_float[1] *= fmul;
      rt_float[2] *= fmul;
      rt_float[3] *= fmul;

      rt_float += 4;
    }
  }
}

static float give_stripelem_index(Sequence *seq, float cfra)
{
  float nr;
  int sta = seq->start;
  int end = seq->start + seq->len - 1;

  if (seq->type & SEQ_TYPE_EFFECT) {
    end = seq->enddisp;
  }

  if (end < sta) {
    return -1;
  }

  if (seq->flag & SEQ_REVERSE_FRAMES) {
    /*reverse frame in this sequence */
    if (cfra <= sta) {
      nr = end - sta;
    }
    else if (cfra >= end) {
      nr = 0;
    }
    else {
      nr = end - cfra;
    }
  }
  else {
    if (cfra <= sta) {
      nr = 0;
    }
    else if (cfra >= end) {
      nr = end - sta;
    }
    else {
      nr = cfra - sta;
    }
  }

  if (seq->strobe < 1.0f) {
    seq->strobe = 1.0f;
  }

  if (seq->strobe > 1.0f) {
    nr -= fmodf((double)nr, (double)seq->strobe);
  }

  return nr;
}

StripElem *BKE_sequencer_give_stripelem(Sequence *seq, int cfra)
{
  StripElem *se = seq->strip->stripdata;

  if (seq->type == SEQ_TYPE_IMAGE) {
    /* only IMAGE strips use the whole array, MOVIE strips use only the first element,
     * all other strips don't use this...
     */

    int nr = (int)give_stripelem_index(seq, cfra);

    if (nr == -1 || se == NULL) {
      return NULL;
    }

    se += nr + seq->anim_startofs;
  }
  return se;
}

static int evaluate_seq_frame_gen(Sequence **seq_arr, ListBase *seqbase, int cfra, int chanshown)
{
  /* Use arbitrary sized linked list, the size could be over MAXSEQ. */
  LinkNodePair effect_inputs = {NULL, NULL};
  int totseq = 0;

  memset(seq_arr, 0, sizeof(Sequence *) * (MAXSEQ + 1));

  for (Sequence *seq = seqbase->first; seq; seq = seq->next) {
    if ((seq->startdisp <= cfra) && (seq->enddisp > cfra)) {
      if ((seq->type & SEQ_TYPE_EFFECT) && !(seq->flag & SEQ_MUTE)) {

        if (seq->seq1) {
          BLI_linklist_append_alloca(&effect_inputs, seq->seq1);
        }

        if (seq->seq2) {
          BLI_linklist_append_alloca(&effect_inputs, seq->seq2);
        }

        if (seq->seq3) {
          BLI_linklist_append_alloca(&effect_inputs, seq->seq3);
        }
      }

      seq_arr[seq->machine] = seq;
      totseq++;
    }
  }

  /* Drop strips which are used for effect inputs, we don't want
   * them to blend into render stack in any other way than effect
   * string rendering.
   */
  for (LinkNode *seq_item = effect_inputs.list; seq_item; seq_item = seq_item->next) {
    Sequence *seq = seq_item->link;
    /* It's possible that effetc strip would be placed to the same
     * 'machine' as it's inputs. We don't want to clear such strips
     * from the stack.
     */
    if (seq_arr[seq->machine] && seq_arr[seq->machine]->type & SEQ_TYPE_EFFECT) {
      continue;
    }
    /* If we're shown a specified channel, then we want to see the stirps
     * which belongs to this machine.
     */
    if (chanshown != 0 && chanshown <= seq->machine) {
      continue;
    }
    seq_arr[seq->machine] = NULL;
  }

  return totseq;
}

int BKE_sequencer_evaluate_frame(Scene *scene, int cfra)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq_arr[MAXSEQ + 1];

  if (ed == NULL) {
    return 0;
  }

  return evaluate_seq_frame_gen(seq_arr, ed->seqbasep, cfra, 0);
}

static bool video_seq_is_rendered(Sequence *seq)
{
  return (seq && !(seq->flag & SEQ_MUTE) && seq->type != SEQ_TYPE_SOUND_RAM);
}

static int get_shown_sequences(ListBase *seqbasep, int cfra, int chanshown, Sequence **seq_arr_out)
{
  Sequence *seq_arr[MAXSEQ + 1];
  int b = chanshown;
  int cnt = 0;

  if (b > MAXSEQ) {
    return 0;
  }

  if (evaluate_seq_frame_gen(seq_arr, seqbasep, cfra, chanshown)) {
    if (b == 0) {
      b = MAXSEQ;
    }
    for (; b > 0; b--) {
      if (video_seq_is_rendered(seq_arr[b])) {
        break;
      }
    }
  }

  chanshown = b;

  for (; b > 0; b--) {
    if (video_seq_is_rendered(seq_arr[b])) {
      if (seq_arr[b]->blend_mode == SEQ_BLEND_REPLACE) {
        break;
      }
    }
  }

  for (; b <= chanshown && b >= 0; b++) {
    if (video_seq_is_rendered(seq_arr[b])) {
      seq_arr_out[cnt++] = seq_arr[b];
    }
  }

  return cnt;
}

/*********************** proxy management *************************/

typedef struct SeqIndexBuildContext {
  struct IndexBuildContext *index_context;

  int tc_flags;
  int size_flags;
  int quality;
  bool overwrite;
  int view_id;

  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  Sequence *seq, *orig_seq;
} SeqIndexBuildContext;

#define PROXY_MAXFILE (2 * FILE_MAXDIR + FILE_MAXFILE)

static IMB_Proxy_Size seq_rendersize_to_proxysize(int size)
{
  if (size >= 100) {
    return IMB_PROXY_NONE;
  }
  if (size >= 99) {
    return IMB_PROXY_100;
  }
  if (size >= 75) {
    return IMB_PROXY_75;
  }
  if (size >= 50) {
    return IMB_PROXY_50;
  }
  return IMB_PROXY_25;
}

static double seq_rendersize_to_scale_factor(int size)
{
  if (size >= 99) {
    return 1.0;
  }
  if (size >= 75) {
    return 0.75;
  }
  if (size >= 50) {
    return 0.50;
  }
  return 0.25;
}

/* the number of files will vary according to the stereo format */
static int seq_num_files(Scene *scene, char views_format, const bool is_multiview)
{
  if (!is_multiview) {
    return 1;
  }
  else if (views_format == R_IMF_VIEWS_STEREO_3D) {
    return 1;
  }
  /* R_IMF_VIEWS_INDIVIDUAL */
  else {
    return BKE_scene_multiview_num_views_get(&scene->r);
  }
}

static void seq_proxy_index_dir_set(struct anim *anim, const char *base_dir)
{
  char dir[FILE_MAX];
  char fname[FILE_MAXFILE];

  IMB_anim_get_fname(anim, fname, FILE_MAXFILE);
  BLI_strncpy(dir, base_dir, sizeof(dir));
  BLI_path_append(dir, sizeof(dir), fname);
  IMB_anim_set_index_dir(anim, dir);
}

static void seq_open_anim_file(Scene *scene, Sequence *seq, bool openfile)
{
  char dir[FILE_MAX];
  char name[FILE_MAX];
  StripProxy *proxy;
  bool use_proxy;
  bool is_multiview_loaded = false;
  Editing *ed = scene->ed;
  const bool is_multiview = (seq->flag & SEQ_USE_VIEWS) != 0 &&
                            (scene->r.scemode & R_MULTIVIEW) != 0;

  if ((seq->anims.first != NULL) && (((StripAnim *)seq->anims.first)->anim != NULL)) {
    return;
  }

  /* reset all the previously created anims */
  BKE_sequence_free_anim(seq);

  BLI_join_dirfile(name, sizeof(name), seq->strip->dir, seq->strip->stripdata->name);
  BLI_path_abs(name, BKE_main_blendfile_path_from_global());

  proxy = seq->strip->proxy;

  use_proxy = proxy && ((proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_DIR) != 0 ||
                        (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE));

  if (use_proxy) {
    if (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE) {
      if (ed->proxy_dir[0] == 0) {
        BLI_strncpy(dir, "//BL_proxy", sizeof(dir));
      }
      else {
        BLI_strncpy(dir, ed->proxy_dir, sizeof(dir));
      }
    }
    else {
      BLI_strncpy(dir, seq->strip->proxy->dir, sizeof(dir));
    }
    BLI_path_abs(dir, BKE_main_blendfile_path_from_global());
  }

  if (is_multiview && seq->views_format == R_IMF_VIEWS_INDIVIDUAL) {
    int totfiles = seq_num_files(scene, seq->views_format, true);
    char prefix[FILE_MAX];
    const char *ext = NULL;
    int i;

    BKE_scene_multiview_view_prefix_get(scene, name, prefix, &ext);

    if (prefix[0] != '\0') {
      for (i = 0; i < totfiles; i++) {
        const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, i);
        char str[FILE_MAX];
        StripAnim *sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");

        BLI_addtail(&seq->anims, sanim);

        BLI_snprintf(str, sizeof(str), "%s%s%s", prefix, suffix, ext);

        if (openfile) {
          sanim->anim = openanim(str,
                                 IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                 seq->streamindex,
                                 seq->strip->colorspace_settings.name);
        }
        else {
          sanim->anim = openanim_noload(str,
                                        IB_rect |
                                            ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                        seq->streamindex,
                                        seq->strip->colorspace_settings.name);
        }

        if (sanim->anim) {
          /* we already have the suffix */
          IMB_suffix_anim(sanim->anim, suffix);
        }
        else {
          if (openfile) {
            sanim->anim = openanim(name,
                                   IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                   seq->streamindex,
                                   seq->strip->colorspace_settings.name);
          }
          else {
            sanim->anim = openanim_noload(name,
                                          IB_rect |
                                              ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                          seq->streamindex,
                                          seq->strip->colorspace_settings.name);
          }

          /* no individual view files - monoscopic, stereo 3d or exr multiview */
          totfiles = 1;
        }

        if (sanim->anim && use_proxy) {
          seq_proxy_index_dir_set(sanim->anim, dir);
        }
      }
      is_multiview_loaded = true;
    }
  }

  if (is_multiview_loaded == false) {
    StripAnim *sanim;

    sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");
    BLI_addtail(&seq->anims, sanim);

    if (openfile) {
      sanim->anim = openanim(name,
                             IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                             seq->streamindex,
                             seq->strip->colorspace_settings.name);
    }
    else {
      sanim->anim = openanim_noload(name,
                                    IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                    seq->streamindex,
                                    seq->strip->colorspace_settings.name);
    }

    if (sanim->anim && use_proxy) {
      seq_proxy_index_dir_set(sanim->anim, dir);
    }
  }
}

static bool seq_proxy_get_fname(
    Editing *ed, Sequence *seq, int cfra, int render_size, char *name, const int view_id)
{
  int frameno;
  char dir[PROXY_MAXFILE];
  StripAnim *sanim;
  char suffix[24] = {'\0'};

  StripProxy *proxy = seq->strip->proxy;
  if (!proxy) {
    return false;
  }

  /* MOVIE tracks (only exception: custom files) are now handled
   * internally by ImBuf module for various reasons: proper time code
   * support, quicker index build, using one file instead
   * of a full directory of jpeg files, etc. Trying to support old
   * and new method at once could lead to funny effects, if people
   * have both, a directory full of jpeg files and proxy avis, so
   * sorry folks, please rebuild your proxies... */

  sanim = BLI_findlink(&seq->anims, view_id);

  if (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE) {
    char fname[FILE_MAXFILE];
    if (ed->proxy_dir[0] == 0) {
      BLI_strncpy(dir, "//BL_proxy", sizeof(dir));
    }
    else {
      BLI_strncpy(dir, ed->proxy_dir, sizeof(dir));
    }

    if (sanim && sanim->anim) {
      IMB_anim_get_fname(sanim->anim, fname, FILE_MAXFILE);
    }
    else if (seq->type == SEQ_TYPE_IMAGE) {
      fname[0] = 0;
    }
    else {
      /* We could make a name here, except non-movie's don't generate proxies,
       * cancel until other types of sequence strips are supported. */
      return false;
    }
    BLI_path_append(dir, sizeof(dir), fname);
    BLI_path_abs(name, BKE_main_blendfile_path_from_global());
  }
  else if ((proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_DIR) &&
           (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE)) {
    BLI_strncpy(dir, seq->strip->proxy->dir, sizeof(dir));
  }
  else if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE) {
    BLI_strncpy(dir, seq->strip->proxy->dir, sizeof(dir));
  }
  else if (sanim && sanim->anim && (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_DIR)) {
    char fname[FILE_MAXFILE];
    BLI_strncpy(dir, seq->strip->proxy->dir, sizeof(dir));
    IMB_anim_get_fname(sanim->anim, fname, FILE_MAXFILE);
    BLI_path_append(dir, sizeof(dir), fname);
  }
  else if (seq->type == SEQ_TYPE_IMAGE) {
    if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_DIR) {
      BLI_strncpy(dir, seq->strip->proxy->dir, sizeof(dir));
    }
    else {
      BLI_snprintf(dir, PROXY_MAXFILE, "%s/BL_proxy", seq->strip->dir);
    }
  }
  else {
    return false;
  }

  if (view_id > 0) {
    BLI_snprintf(suffix, sizeof(suffix), "_%d", view_id);
  }

  if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE &&
      ed->proxy_storage != SEQ_EDIT_PROXY_DIR_STORAGE) {
    char fname[FILE_MAXFILE];
    BLI_join_dirfile(fname, PROXY_MAXFILE, dir, proxy->file);
    BLI_path_abs(fname, BKE_main_blendfile_path_from_global());
    if (suffix[0] != '\0') {
      /* TODO(sergey): This will actually append suffix after extension
       * which is weird but how was originally coded in multiview branch.
       */
      BLI_snprintf(name, PROXY_MAXFILE, "%s_%s", fname, suffix);
    }
    else {
      BLI_strncpy(name, fname, PROXY_MAXFILE);
    }

    return true;
  }

  /* generate a separate proxy directory for each preview size */

  if (seq->type == SEQ_TYPE_IMAGE) {
    BLI_snprintf(name,
                 PROXY_MAXFILE,
                 "%s/images/%d/%s_proxy%s",
                 dir,
                 render_size,
                 BKE_sequencer_give_stripelem(seq, cfra)->name,
                 suffix);
    frameno = 1;
  }
  else {
    frameno = (int)give_stripelem_index(seq, cfra) + seq->anim_startofs;
    BLI_snprintf(name, PROXY_MAXFILE, "%s/proxy_misc/%d/####%s", dir, render_size, suffix);
  }

  BLI_path_abs(name, BKE_main_blendfile_path_from_global());
  BLI_path_frame(name, frameno, 0);

  strcat(name, ".jpg");

  return true;
}

static ImBuf *seq_proxy_fetch(const SeqRenderData *context, Sequence *seq, int cfra)
{
  char name[PROXY_MAXFILE];
  IMB_Proxy_Size psize = seq_rendersize_to_proxysize(context->preview_render_size);
  int size_flags;
  int render_size = context->preview_render_size;
  StripProxy *proxy = seq->strip->proxy;
  Editing *ed = context->scene->ed;
  StripAnim *sanim;

  if (!(seq->flag & SEQ_USE_PROXY)) {
    return NULL;
  }

  /* dirty hack to distinguish 100% render size from PROXY_100 */
  if (render_size == 99) {
    render_size = 100;
  }

  size_flags = proxy->build_size_flags;

  /* only use proxies, if they are enabled (even if present!) */
  if (psize == IMB_PROXY_NONE || ((size_flags & psize) != psize)) {
    return NULL;
  }

  if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE) {
    int frameno = (int)give_stripelem_index(seq, cfra) + seq->anim_startofs;
    if (proxy->anim == NULL) {
      if (seq_proxy_get_fname(ed, seq, cfra, render_size, name, context->view_id) == 0) {
        return NULL;
      }

      proxy->anim = openanim(name, IB_rect, 0, seq->strip->colorspace_settings.name);
    }
    if (proxy->anim == NULL) {
      return NULL;
    }

    seq_open_anim_file(context->scene, seq, true);
    sanim = seq->anims.first;

    frameno = IMB_anim_index_get_frame_index(
        sanim ? sanim->anim : NULL, seq->strip->proxy->tc, frameno);

    return IMB_anim_absolute(proxy->anim, frameno, IMB_TC_NONE, IMB_PROXY_NONE);
  }

  if (seq_proxy_get_fname(ed, seq, cfra, render_size, name, context->view_id) == 0) {
    return NULL;
  }

  if (BLI_exists(name)) {
    ImBuf *ibuf = IMB_loadiffname(name, IB_rect, NULL);

    if (ibuf) {
      sequencer_imbuf_assign_spaces(context->scene, ibuf);
    }

    return ibuf;
  }
  else {
    return NULL;
  }
}

static void seq_proxy_build_frame(const SeqRenderData *context,
                                  SeqRenderState *state,
                                  Sequence *seq,
                                  int cfra,
                                  int proxy_render_size,
                                  const bool overwrite)
{
  char name[PROXY_MAXFILE];
  int quality;
  int rectx, recty;
  int ok;
  ImBuf *ibuf_tmp, *ibuf;
  Editing *ed = context->scene->ed;

  if (!seq_proxy_get_fname(ed, seq, cfra, proxy_render_size, name, context->view_id)) {
    return;
  }

  if (!overwrite && BLI_exists(name)) {
    return;
  }

  ibuf_tmp = seq_render_strip(context, state, seq, cfra);

  rectx = (proxy_render_size * ibuf_tmp->x) / 100;
  recty = (proxy_render_size * ibuf_tmp->y) / 100;

  if (ibuf_tmp->x != rectx || ibuf_tmp->y != recty) {
    ibuf = IMB_dupImBuf(ibuf_tmp);
    IMB_metadata_copy(ibuf, ibuf_tmp);
    IMB_freeImBuf(ibuf_tmp);
    IMB_scalefastImBuf(ibuf, (short)rectx, (short)recty);
  }
  else {
    ibuf = ibuf_tmp;
  }

  /* depth = 32 is intentionally left in, otherwise ALPHA channels
   * won't work... */
  quality = seq->strip->proxy->quality;
  ibuf->ftype = IMB_FTYPE_JPG;
  ibuf->foptions.quality = quality;

  /* unsupported feature only confuses other s/w */
  if (ibuf->planes == 32) {
    ibuf->planes = 24;
  }

  BLI_make_existing_file(name);

  ok = IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat);
  if (ok == 0) {
    perror(name);
  }

  IMB_freeImBuf(ibuf);
}

/**
 * Returns whether the file this context would read from even exist,
 * if not, don't create the context
 */
static bool seq_proxy_multiview_context_invalid(Sequence *seq, Scene *scene, const int view_id)
{
  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return false;
  }

  if ((seq->type == SEQ_TYPE_IMAGE) && (seq->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
    static char prefix[FILE_MAX];
    static const char *ext = NULL;
    char str[FILE_MAX];

    if (view_id == 0) {
      char path[FILE_MAX];
      BLI_join_dirfile(path, sizeof(path), seq->strip->dir, seq->strip->stripdata->name);
      BLI_path_abs(path, BKE_main_blendfile_path_from_global());
      BKE_scene_multiview_view_prefix_get(scene, path, prefix, &ext);
    }
    else {
      prefix[0] = '\0';
    }

    if (prefix[0] == '\0') {
      return view_id != 0;
    }

    seq_multiview_name(scene, view_id, prefix, ext, str, FILE_MAX);

    if (BLI_access(str, R_OK) == 0) {
      return false;
    }
    else {
      return view_id != 0;
    }
  }
  return false;
}

/**
 * This returns the maximum possible number of required contexts
 */
static int seq_proxy_context_count(Sequence *seq, Scene *scene)
{
  int num_views = 1;

  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return 1;
  }

  switch (seq->type) {
    case SEQ_TYPE_MOVIE: {
      num_views = BLI_listbase_count(&seq->anims);
      break;
    }
    case SEQ_TYPE_IMAGE: {
      switch (seq->views_format) {
        case R_IMF_VIEWS_INDIVIDUAL:
          num_views = BKE_scene_multiview_num_views_get(&scene->r);
          break;
        case R_IMF_VIEWS_STEREO_3D:
          num_views = 2;
          break;
        case R_IMF_VIEWS_MULTIVIEW:
          /* not supported at the moment */
          /* pass through */
        default:
          num_views = 1;
      }
      break;
    }
  }

  return num_views;
}

void BKE_sequencer_proxy_rebuild_context(Main *bmain,
                                         Depsgraph *depsgraph,
                                         Scene *scene,
                                         Sequence *seq,
                                         struct GSet *file_list,
                                         ListBase *queue)
{
  SeqIndexBuildContext *context;
  Sequence *nseq;
  LinkData *link;
  int num_files;
  int i;

  if (!seq->strip || !seq->strip->proxy) {
    return;
  }

  if (!(seq->flag & SEQ_USE_PROXY)) {
    return;
  }

  num_files = seq_proxy_context_count(seq, scene);

  for (i = 0; i < num_files; i++) {
    if (seq_proxy_multiview_context_invalid(seq, scene, i)) {
      continue;
    }

    context = MEM_callocN(sizeof(SeqIndexBuildContext), "seq proxy rebuild context");

    nseq = BKE_sequence_dupli_recursive(scene, scene, NULL, seq, 0);

    context->tc_flags = nseq->strip->proxy->build_tc_flags;
    context->size_flags = nseq->strip->proxy->build_size_flags;
    context->quality = nseq->strip->proxy->quality;
    context->overwrite = (nseq->strip->proxy->build_flags & SEQ_PROXY_SKIP_EXISTING) == 0;

    context->bmain = bmain;
    context->depsgraph = depsgraph;
    context->scene = scene;
    context->orig_seq = seq;
    context->seq = nseq;

    context->view_id = i; /* only for images */

    link = BLI_genericNodeN(context);
    BLI_addtail(queue, link);

    if (nseq->type == SEQ_TYPE_MOVIE) {
      StripAnim *sanim;

      seq_open_anim_file(scene, nseq, true);
      sanim = BLI_findlink(&nseq->anims, i);

      if (sanim->anim) {
        context->index_context = IMB_anim_index_rebuild_context(sanim->anim,
                                                                context->tc_flags,
                                                                context->size_flags,
                                                                context->quality,
                                                                context->overwrite,
                                                                file_list);
      }
    }
  }
}

void BKE_sequencer_proxy_rebuild(SeqIndexBuildContext *context,
                                 short *stop,
                                 short *do_update,
                                 float *progress)
{
  const bool overwrite = context->overwrite;
  SeqRenderData render_context;
  Sequence *seq = context->seq;
  Scene *scene = context->scene;
  Main *bmain = context->bmain;
  int cfra;

  if (seq->type == SEQ_TYPE_MOVIE) {
    if (context->index_context) {
      IMB_anim_index_rebuild(context->index_context, stop, do_update, progress);
    }

    return;
  }

  if (!(seq->flag & SEQ_USE_PROXY)) {
    return;
  }

  /* that's why it is called custom... */
  if (seq->strip->proxy && seq->strip->proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE) {
    return;
  }

  /* fail safe code */

  BKE_sequencer_new_render_data(bmain,
                                context->depsgraph,
                                context->scene,
                                (scene->r.size * (float)scene->r.xsch) / 100.0f + 0.5f,
                                (scene->r.size * (float)scene->r.ysch) / 100.0f + 0.5f,
                                100,
                                false,
                                &render_context);

  render_context.skip_cache = true;
  render_context.is_proxy_render = true;
  render_context.view_id = context->view_id;

  SeqRenderState state;
  sequencer_state_init(&state);

  for (cfra = seq->startdisp + seq->startstill; cfra < seq->enddisp - seq->endstill; cfra++) {
    if (context->size_flags & IMB_PROXY_25) {
      seq_proxy_build_frame(&render_context, &state, seq, cfra, 25, overwrite);
    }
    if (context->size_flags & IMB_PROXY_50) {
      seq_proxy_build_frame(&render_context, &state, seq, cfra, 50, overwrite);
    }
    if (context->size_flags & IMB_PROXY_75) {
      seq_proxy_build_frame(&render_context, &state, seq, cfra, 75, overwrite);
    }
    if (context->size_flags & IMB_PROXY_100) {
      seq_proxy_build_frame(&render_context, &state, seq, cfra, 100, overwrite);
    }

    *progress = (float)(cfra - seq->startdisp - seq->startstill) /
                (seq->enddisp - seq->endstill - seq->startdisp - seq->startstill);
    *do_update = true;

    if (*stop || G.is_break) {
      break;
    }
  }
}

void BKE_sequencer_proxy_rebuild_finish(SeqIndexBuildContext *context, bool stop)
{
  if (context->index_context) {
    StripAnim *sanim;

    for (sanim = context->seq->anims.first; sanim; sanim = sanim->next) {
      IMB_close_anim_proxies(sanim->anim);
    }

    for (sanim = context->orig_seq->anims.first; sanim; sanim = sanim->next) {
      IMB_close_anim_proxies(sanim->anim);
    }

    IMB_anim_index_rebuild_finish(context->index_context, stop);
  }

  seq_free_sequence_recurse(NULL, context->seq, true);

  MEM_freeN(context);
}

void BKE_sequencer_proxy_set(struct Sequence *seq, bool value)
{
  if (value) {
    seq->flag |= SEQ_USE_PROXY;
    if (seq->strip->proxy == NULL) {
      seq->strip->proxy = MEM_callocN(sizeof(struct StripProxy), "StripProxy");
      seq->strip->proxy->quality = 90;
      seq->strip->proxy->build_tc_flags = SEQ_PROXY_TC_ALL;
      seq->strip->proxy->build_size_flags = SEQ_PROXY_IMAGE_SIZE_25;
    }
  }
  else {
    seq->flag &= ~SEQ_USE_PROXY;
  }
}

/*********************** color balance *************************/

static StripColorBalance calc_cb(StripColorBalance *cb_)
{
  StripColorBalance cb = *cb_;
  int c;

  for (c = 0; c < 3; c++) {
    cb.lift[c] = 2.0f - cb.lift[c];
  }

  if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_LIFT) {
    for (c = 0; c < 3; c++) {
      /* tweak to give more subtle results
       * values above 1.0 are scaled */
      if (cb.lift[c] > 1.0f) {
        cb.lift[c] = pow(cb.lift[c] - 1.0f, 2.0) + 1.0;
      }

      cb.lift[c] = 2.0f - cb.lift[c];
    }
  }

  if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_GAIN) {
    for (c = 0; c < 3; c++) {
      if (cb.gain[c] != 0.0f) {
        cb.gain[c] = 1.0f / cb.gain[c];
      }
      else {
        cb.gain[c] = 1000000; /* should be enough :) */
      }
    }
  }

  if (!(cb.flag & SEQ_COLOR_BALANCE_INVERSE_GAMMA)) {
    for (c = 0; c < 3; c++) {
      if (cb.gamma[c] != 0.0f) {
        cb.gamma[c] = 1.0f / cb.gamma[c];
      }
      else {
        cb.gamma[c] = 1000000; /* should be enough :) */
      }
    }
  }

  return cb;
}

/* note: lift is actually 2-lift */
MINLINE float color_balance_fl(
    float in, const float lift, const float gain, const float gamma, const float mul)
{
  float x = (((in - 1.0f) * lift) + 1.0f) * gain;

  /* prevent NaN */
  if (x < 0.f) {
    x = 0.f;
  }

  return powf(x, gamma) * mul;
}

static void make_cb_table_float(float lift, float gain, float gamma, float *table, float mul)
{
  int y;

  for (y = 0; y < 256; y++) {
    float v = color_balance_fl((float)y * (1.0f / 255.0f), lift, gain, gamma, mul);

    table[y] = v;
  }
}

static void color_balance_byte_byte(StripColorBalance *cb_,
                                    unsigned char *rect,
                                    unsigned char *mask_rect,
                                    int width,
                                    int height,
                                    float mul)
{
  // unsigned char cb_tab[3][256];
  unsigned char *cp = rect;
  unsigned char *e = cp + width * 4 * height;
  unsigned char *m = mask_rect;

  StripColorBalance cb = calc_cb(cb_);

  while (cp < e) {
    float p[4];
    int c;

    straight_uchar_to_premul_float(p, cp);

    for (c = 0; c < 3; c++) {
      float t = color_balance_fl(p[c], cb.lift[c], cb.gain[c], cb.gamma[c], mul);

      if (m) {
        float m_normal = (float)m[c] / 255.0f;

        p[c] = p[c] * (1.0f - m_normal) + t * m_normal;
      }
      else {
        p[c] = t;
      }
    }

    premul_float_to_straight_uchar(cp, p);

    cp += 4;
    if (m) {
      m += 4;
    }
  }
}

static void color_balance_byte_float(StripColorBalance *cb_,
                                     unsigned char *rect,
                                     float *rect_float,
                                     unsigned char *mask_rect,
                                     int width,
                                     int height,
                                     float mul)
{
  float cb_tab[4][256];
  int c, i;
  unsigned char *p = rect;
  unsigned char *e = p + width * 4 * height;
  unsigned char *m = mask_rect;
  float *o;
  StripColorBalance cb;

  o = rect_float;

  cb = calc_cb(cb_);

  for (c = 0; c < 3; c++) {
    make_cb_table_float(cb.lift[c], cb.gain[c], cb.gamma[c], cb_tab[c], mul);
  }

  for (i = 0; i < 256; i++) {
    cb_tab[3][i] = ((float)i) * (1.0f / 255.0f);
  }

  while (p < e) {
    if (m) {
      float t[3] = {m[0] / 255.0f, m[1] / 255.0f, m[2] / 255.0f};

      p[0] = p[0] * (1.0f - t[0]) + t[0] * cb_tab[0][p[0]];
      p[1] = p[1] * (1.0f - t[1]) + t[1] * cb_tab[1][p[1]];
      p[2] = p[2] * (1.0f - t[2]) + t[2] * cb_tab[2][p[2]];

      m += 4;
    }
    else {
      o[0] = cb_tab[0][p[0]];
      o[1] = cb_tab[1][p[1]];
      o[2] = cb_tab[2][p[2]];
    }

    o[3] = cb_tab[3][p[3]];

    p += 4;
    o += 4;
  }
}

static void color_balance_float_float(StripColorBalance *cb_,
                                      float *rect_float,
                                      float *mask_rect_float,
                                      int width,
                                      int height,
                                      float mul)
{
  float *p = rect_float;
  const float *e = rect_float + width * 4 * height;
  const float *m = mask_rect_float;
  StripColorBalance cb = calc_cb(cb_);

  while (p < e) {
    int c;
    for (c = 0; c < 3; c++) {
      float t = color_balance_fl(p[c], cb.lift[c], cb.gain[c], cb.gamma[c], mul);

      if (m) {
        p[c] = p[c] * (1.0f - m[c]) + t * m[c];
      }
      else {
        p[c] = t;
      }
    }

    p += 4;
    if (m) {
      m += 4;
    }
  }
}

typedef struct ColorBalanceInitData {
  StripColorBalance *cb;
  ImBuf *ibuf;
  float mul;
  ImBuf *mask;
  bool make_float;
} ColorBalanceInitData;

typedef struct ColorBalanceThread {
  StripColorBalance *cb;
  float mul;

  int width, height;

  unsigned char *rect, *mask_rect;
  float *rect_float, *mask_rect_float;

  bool make_float;
} ColorBalanceThread;

static void color_balance_init_handle(void *handle_v,
                                      int start_line,
                                      int tot_line,
                                      void *init_data_v)
{
  ColorBalanceThread *handle = (ColorBalanceThread *)handle_v;
  ColorBalanceInitData *init_data = (ColorBalanceInitData *)init_data_v;
  ImBuf *ibuf = init_data->ibuf;
  ImBuf *mask = init_data->mask;

  int offset = 4 * start_line * ibuf->x;

  memset(handle, 0, sizeof(ColorBalanceThread));

  handle->cb = init_data->cb;
  handle->mul = init_data->mul;
  handle->width = ibuf->x;
  handle->height = tot_line;
  handle->make_float = init_data->make_float;

  if (ibuf->rect) {
    handle->rect = (unsigned char *)ibuf->rect + offset;
  }

  if (ibuf->rect_float) {
    handle->rect_float = ibuf->rect_float + offset;
  }

  if (mask) {
    if (mask->rect) {
      handle->mask_rect = (unsigned char *)mask->rect + offset;
    }

    if (mask->rect_float) {
      handle->mask_rect_float = mask->rect_float + offset;
    }
  }
  else {
    handle->mask_rect = NULL;
    handle->mask_rect_float = NULL;
  }
}

static void *color_balance_do_thread(void *thread_data_v)
{
  ColorBalanceThread *thread_data = (ColorBalanceThread *)thread_data_v;
  StripColorBalance *cb = thread_data->cb;
  int width = thread_data->width, height = thread_data->height;
  unsigned char *rect = thread_data->rect;
  unsigned char *mask_rect = thread_data->mask_rect;
  float *rect_float = thread_data->rect_float;
  float *mask_rect_float = thread_data->mask_rect_float;
  float mul = thread_data->mul;

  if (rect_float) {
    color_balance_float_float(cb, rect_float, mask_rect_float, width, height, mul);
  }
  else if (thread_data->make_float) {
    color_balance_byte_float(cb, rect, rect_float, mask_rect, width, height, mul);
  }
  else {
    color_balance_byte_byte(cb, rect, mask_rect, width, height, mul);
  }

  return NULL;
}

/* cfra is offset by fra_offset only in case we are using a real mask. */
ImBuf *BKE_sequencer_render_mask_input(const SeqRenderData *context,
                                       int mask_input_type,
                                       Sequence *mask_sequence,
                                       Mask *mask_id,
                                       int cfra,
                                       int fra_offset,
                                       bool make_float)
{
  ImBuf *mask_input = NULL;

  if (mask_input_type == SEQUENCE_MASK_INPUT_STRIP) {
    if (mask_sequence) {
      SeqRenderState state;
      sequencer_state_init(&state);

      mask_input = seq_render_strip(context, &state, mask_sequence, cfra);

      if (make_float) {
        if (!mask_input->rect_float) {
          IMB_float_from_rect(mask_input);
        }
      }
      else {
        if (!mask_input->rect) {
          IMB_rect_from_float(mask_input);
        }
      }
    }
  }
  else if (mask_input_type == SEQUENCE_MASK_INPUT_ID) {
    mask_input = seq_render_mask(context, mask_id, cfra - fra_offset, make_float);
  }

  return mask_input;
}

void BKE_sequencer_color_balance_apply(
    StripColorBalance *cb, ImBuf *ibuf, float mul, bool make_float, ImBuf *mask_input)
{
  ColorBalanceInitData init_data;

  if (!ibuf->rect_float && make_float) {
    imb_addrectfloatImBuf(ibuf);
  }

  init_data.cb = cb;
  init_data.ibuf = ibuf;
  init_data.mul = mul;
  init_data.make_float = make_float;
  init_data.mask = mask_input;

  IMB_processor_apply_threaded(ibuf->y,
                               sizeof(ColorBalanceThread),
                               &init_data,
                               color_balance_init_handle,
                               color_balance_do_thread);

  /* color balance either happens on float buffer or byte buffer, but never on both,
   * free byte buffer if there's float buffer since float buffer would be used for
   * color balance in favor of byte buffer
   */
  if (ibuf->rect_float && ibuf->rect) {
    imb_freerectImBuf(ibuf);
  }
}

/*
 * input preprocessing for SEQ_TYPE_IMAGE, SEQ_TYPE_MOVIE, SEQ_TYPE_MOVIECLIP and SEQ_TYPE_SCENE
 *
 * Do all the things you can't really do afterwards using sequence effects
 * (read: before rescaling to render resolution has been done)
 *
 * Order is important!
 *
 * - Deinterlace
 * - Crop and transform in image source coordinate space
 * - Flip X + Flip Y (could be done afterwards, backward compatibility)
 * - Promote image to float data (affects pipeline operations afterwards)
 * - Color balance (is most efficient in the byte -> float
 *   (future: half -> float should also work fine!)
 *   case, if done on load, since we can use lookup tables)
 * - Premultiply
 */

bool BKE_sequencer_input_have_to_preprocess(const SeqRenderData *context,
                                            Sequence *seq,
                                            float UNUSED(cfra))
{
  float mul;

  if (context && context->is_proxy_render) {
    return false;
  }

  if (seq->flag &
      (SEQ_FILTERY | SEQ_USE_CROP | SEQ_USE_TRANSFORM | SEQ_FLIPX | SEQ_FLIPY | SEQ_MAKE_FLOAT)) {
    return true;
  }

  mul = seq->mul;

  if (seq->blend_mode == SEQ_BLEND_REPLACE) {
    mul *= seq->blend_opacity / 100.0f;
  }

  if (mul != 1.0f) {
    return true;
  }

  if (seq->sat != 1.0f) {
    return true;
  }

  if (seq->modifiers.first) {
    return true;
  }

  return false;
}

static ImBuf *input_preprocess(const SeqRenderData *context,
                               Sequence *seq,
                               float cfra,
                               ImBuf *ibuf,
                               const bool is_proxy_image,
                               const bool is_preprocessed)
{
  Scene *scene = context->scene;
  float mul;

  ibuf = IMB_makeSingleUser(ibuf);

  if ((seq->flag & SEQ_FILTERY) && !ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_MOVIECLIP)) {
    IMB_filtery(ibuf);
  }

  if (seq->flag & (SEQ_USE_CROP | SEQ_USE_TRANSFORM)) {
    StripCrop c = {0};
    StripTransform t = {0};
    int sx, sy, dx, dy;

    if (is_proxy_image) {
      double f = seq_rendersize_to_scale_factor(context->preview_render_size);

      if (f != 1.0) {
        IMB_scalefastImBuf(ibuf, ibuf->x / f, ibuf->y / f);
      }
    }

    if (seq->flag & SEQ_USE_CROP && seq->strip->crop) {
      c = *seq->strip->crop;
    }
    if (seq->flag & SEQ_USE_TRANSFORM && seq->strip->transform) {
      t = *seq->strip->transform;
    }

    if (is_preprocessed) {
      double xscale = scene->r.xsch ? ((double)context->rectx / (double)scene->r.xsch) : 1.0;
      double yscale = scene->r.ysch ? ((double)context->recty / (double)scene->r.ysch) : 1.0;
      if (seq->flag & SEQ_USE_TRANSFORM) {
        t.xofs *= xscale;
        t.yofs *= yscale;
      }
      if (seq->flag & SEQ_USE_CROP) {
        c.left *= xscale;
        c.right *= xscale;
        c.top *= yscale;
        c.bottom *= yscale;
      }
    }

    sx = ibuf->x - c.left - c.right;
    sy = ibuf->y - c.top - c.bottom;

    if (seq->flag & SEQ_USE_TRANSFORM) {
      if (is_preprocessed) {
        dx = context->rectx;
        dy = context->recty;
      }
      else {
        dx = scene->r.xsch;
        dy = scene->r.ysch;
      }
    }
    else {
      dx = sx;
      dy = sy;
    }

    if (c.top + c.bottom >= ibuf->y || c.left + c.right >= ibuf->x || t.xofs >= dx ||
        t.yofs >= dy) {
      make_black_ibuf(ibuf);
    }
    else {
      ImBuf *i = IMB_allocImBuf(dx, dy, 32, ibuf->rect_float ? IB_rectfloat : IB_rect);

      IMB_rectcpy(i, ibuf, t.xofs, t.yofs, c.left, c.bottom, sx, sy);
      sequencer_imbuf_assign_spaces(scene, i);

      IMB_metadata_copy(i, ibuf);
      IMB_freeImBuf(ibuf);

      ibuf = i;
    }
  }

  if (seq->flag & SEQ_FLIPX) {
    IMB_flipx(ibuf);
  }

  if (seq->flag & SEQ_FLIPY) {
    IMB_flipy(ibuf);
  }

  if (seq->sat != 1.0f) {
    IMB_saturation(ibuf, seq->sat);
  }

  mul = seq->mul;

  if (seq->blend_mode == SEQ_BLEND_REPLACE) {
    mul *= seq->blend_opacity / 100.0f;
  }

  if (seq->flag & SEQ_MAKE_FLOAT) {
    if (!ibuf->rect_float) {
      BKE_sequencer_imbuf_to_sequencer_space(scene, ibuf, true);
    }

    if (ibuf->rect) {
      imb_freerectImBuf(ibuf);
    }
  }

  if (mul != 1.0f) {
    multibuf(ibuf, mul);
  }

  if (ibuf->x != context->rectx || ibuf->y != context->recty) {
    if (scene->r.mode & R_OSA) {
      IMB_scaleImBuf(ibuf, (short)context->rectx, (short)context->recty);
    }
    else {
      IMB_scalefastImBuf(ibuf, (short)context->rectx, (short)context->recty);
    }
  }

  if (seq->modifiers.first) {
    ImBuf *ibuf_new = BKE_sequence_modifier_apply_stack(context, seq, ibuf, cfra);

    if (ibuf_new != ibuf) {
      IMB_metadata_copy(ibuf_new, ibuf);
      IMB_freeImBuf(ibuf);
      ibuf = ibuf_new;
    }
  }

  return ibuf;
}

/*********************** strip rendering functions  *************************/

typedef struct RenderEffectInitData {
  struct SeqEffectHandle *sh;
  const SeqRenderData *context;
  Sequence *seq;
  float cfra, facf0, facf1;
  ImBuf *ibuf1, *ibuf2, *ibuf3;

  ImBuf *out;
} RenderEffectInitData;

typedef struct RenderEffectThread {
  struct SeqEffectHandle *sh;
  const SeqRenderData *context;
  Sequence *seq;
  float cfra, facf0, facf1;
  ImBuf *ibuf1, *ibuf2, *ibuf3;

  ImBuf *out;
  int start_line, tot_line;
} RenderEffectThread;

static void render_effect_execute_init_handle(void *handle_v,
                                              int start_line,
                                              int tot_line,
                                              void *init_data_v)
{
  RenderEffectThread *handle = (RenderEffectThread *)handle_v;
  RenderEffectInitData *init_data = (RenderEffectInitData *)init_data_v;

  handle->sh = init_data->sh;
  handle->context = init_data->context;
  handle->seq = init_data->seq;
  handle->cfra = init_data->cfra;
  handle->facf0 = init_data->facf0;
  handle->facf1 = init_data->facf1;
  handle->ibuf1 = init_data->ibuf1;
  handle->ibuf2 = init_data->ibuf2;
  handle->ibuf3 = init_data->ibuf3;
  handle->out = init_data->out;

  handle->start_line = start_line;
  handle->tot_line = tot_line;
}

static void *render_effect_execute_do_thread(void *thread_data_v)
{
  RenderEffectThread *thread_data = (RenderEffectThread *)thread_data_v;

  thread_data->sh->execute_slice(thread_data->context,
                                 thread_data->seq,
                                 thread_data->cfra,
                                 thread_data->facf0,
                                 thread_data->facf1,
                                 thread_data->ibuf1,
                                 thread_data->ibuf2,
                                 thread_data->ibuf3,
                                 thread_data->start_line,
                                 thread_data->tot_line,
                                 thread_data->out);

  return NULL;
}

static ImBuf *seq_render_effect_execute_threaded(struct SeqEffectHandle *sh,
                                                 const SeqRenderData *context,
                                                 Sequence *seq,
                                                 float cfra,
                                                 float facf0,
                                                 float facf1,
                                                 ImBuf *ibuf1,
                                                 ImBuf *ibuf2,
                                                 ImBuf *ibuf3)
{
  RenderEffectInitData init_data;
  ImBuf *out = sh->init_execution(context, ibuf1, ibuf2, ibuf3);

  init_data.sh = sh;
  init_data.context = context;
  init_data.seq = seq;
  init_data.cfra = cfra;
  init_data.facf0 = facf0;
  init_data.facf1 = facf1;
  init_data.ibuf1 = ibuf1;
  init_data.ibuf2 = ibuf2;
  init_data.ibuf3 = ibuf3;
  init_data.out = out;

  IMB_processor_apply_threaded(out->y,
                               sizeof(RenderEffectThread),
                               &init_data,
                               render_effect_execute_init_handle,
                               render_effect_execute_do_thread);

  return out;
}

static ImBuf *seq_render_effect_strip_impl(const SeqRenderData *context,
                                           SeqRenderState *state,
                                           Sequence *seq,
                                           float cfra)
{
  Scene *scene = context->scene;
  float fac, facf;
  int early_out;
  int i;
  struct SeqEffectHandle sh = BKE_sequence_get_effect(seq);
  FCurve *fcu = NULL;
  ImBuf *ibuf[3];
  Sequence *input[3];
  ImBuf *out = NULL;

  ibuf[0] = ibuf[1] = ibuf[2] = NULL;

  input[0] = seq->seq1;
  input[1] = seq->seq2;
  input[2] = seq->seq3;

  if (!sh.execute && !(sh.execute_slice && sh.init_execution)) {
    /* effect not supported in this version... */
    out = IMB_allocImBuf(context->rectx, context->recty, 32, IB_rect);
    return out;
  }

  if (seq->flag & SEQ_USE_EFFECT_DEFAULT_FADE) {
    sh.get_default_fac(seq, cfra, &fac, &facf);
    facf = fac;
  }
  else {
    fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "effect_fader", 0, NULL);
    if (fcu) {
      fac = facf = evaluate_fcurve(fcu, cfra);
    }
    else {
      fac = facf = seq->effect_fader;
    }
  }

  early_out = sh.early_out(seq, fac, facf);

  switch (early_out) {
    case EARLY_NO_INPUT:
      out = sh.execute(context, seq, cfra, fac, facf, NULL, NULL, NULL);
      break;
    case EARLY_DO_EFFECT:
      for (i = 0; i < 3; i++) {
        if (input[i]) {
          ibuf[i] = seq_render_strip(context, state, input[i], cfra);
        }
      }

      if (ibuf[0] && ibuf[1]) {
        if (sh.multithreaded) {
          out = seq_render_effect_execute_threaded(
              &sh, context, seq, cfra, fac, facf, ibuf[0], ibuf[1], ibuf[2]);
        }
        else {
          out = sh.execute(context, seq, cfra, fac, facf, ibuf[0], ibuf[1], ibuf[2]);
        }
      }
      break;
    case EARLY_USE_INPUT_1:
      if (input[0]) {
        ibuf[0] = seq_render_strip(context, state, input[0], cfra);
      }
      if (ibuf[0]) {
        if (BKE_sequencer_input_have_to_preprocess(context, seq, cfra)) {
          out = IMB_dupImBuf(ibuf[0]);
        }
        else {
          out = ibuf[0];
          IMB_refImBuf(out);
        }
      }
      break;
    case EARLY_USE_INPUT_2:
      if (input[1]) {
        ibuf[1] = seq_render_strip(context, state, input[1], cfra);
      }
      if (ibuf[1]) {
        if (BKE_sequencer_input_have_to_preprocess(context, seq, cfra)) {
          out = IMB_dupImBuf(ibuf[1]);
        }
        else {
          out = ibuf[1];
          IMB_refImBuf(out);
        }
      }
      break;
  }

  for (i = 0; i < 3; i++) {
    IMB_freeImBuf(ibuf[i]);
  }

  if (out == NULL) {
    out = IMB_allocImBuf(context->rectx, context->recty, 32, IB_rect);
  }

  return out;
}

static ImBuf *seq_render_image_strip(const SeqRenderData *context,
                                     Sequence *seq,
                                     float UNUSED(nr),
                                     float cfra)
{
  ImBuf *ibuf = NULL;
  char name[FILE_MAX];
  bool is_multiview = (seq->flag & SEQ_USE_VIEWS) != 0 &&
                      (context->scene->r.scemode & R_MULTIVIEW) != 0;
  StripElem *s_elem = BKE_sequencer_give_stripelem(seq, cfra);
  int flag;

  if (s_elem) {
    BLI_join_dirfile(name, sizeof(name), seq->strip->dir, s_elem->name);
    BLI_path_abs(name, BKE_main_blendfile_path_from_global());
  }

  flag = IB_rect | IB_metadata;
  if (seq->alpha_mode == SEQ_ALPHA_PREMUL) {
    flag |= IB_alphamode_premul;
  }

  if (!s_elem) {
    /* don't do anything */
  }
  else if (is_multiview) {
    const int totfiles = seq_num_files(context->scene, seq->views_format, true);
    int totviews;
    struct ImBuf **ibufs_arr;
    char prefix[FILE_MAX];
    const char *ext = NULL;
    int i;

    if (totfiles > 1) {
      BKE_scene_multiview_view_prefix_get(context->scene, name, prefix, &ext);
      if (prefix[0] == '\0') {
        goto monoview_image;
      }
    }
    else {
      prefix[0] = '\0';
    }

    totviews = BKE_scene_multiview_num_views_get(&context->scene->r);
    ibufs_arr = MEM_callocN(sizeof(ImBuf *) * totviews, "Sequence Image Views Imbufs");

    for (i = 0; i < totfiles; i++) {

      if (prefix[0] == '\0') {
        ibufs_arr[i] = IMB_loadiffname(name, flag, seq->strip->colorspace_settings.name);
      }
      else {
        char str[FILE_MAX];
        seq_multiview_name(context->scene, i, prefix, ext, str, FILE_MAX);
        ibufs_arr[i] = IMB_loadiffname(str, flag, seq->strip->colorspace_settings.name);
      }

      if (ibufs_arr[i]) {
        /* we don't need both (speed reasons)! */
        if (ibufs_arr[i]->rect_float && ibufs_arr[i]->rect) {
          imb_freerectImBuf(ibufs_arr[i]);
        }
      }
    }

    if (seq->views_format == R_IMF_VIEWS_STEREO_3D && ibufs_arr[0]) {
      IMB_ImBufFromStereo3d(seq->stereo3d_format, ibufs_arr[0], &ibufs_arr[0], &ibufs_arr[1]);
    }

    for (i = 0; i < totviews; i++) {
      if (ibufs_arr[i]) {
        SeqRenderData localcontext = *context;
        localcontext.view_id = i;

        /* all sequencer color is done in SRGB space, linear gives odd crossfades */
        BKE_sequencer_imbuf_to_sequencer_space(context->scene, ibufs_arr[i], false);

        if (i != context->view_id) {
          BKE_sequencer_cache_put(
              &localcontext, seq, cfra, SEQ_CACHE_STORE_PREPROCESSED, ibufs_arr[i], 0);
        }
      }
    }

    /* return the original requested ImBuf */
    ibuf = ibufs_arr[context->view_id];
    if (ibuf) {
      s_elem->orig_width = ibufs_arr[0]->x;
      s_elem->orig_height = ibufs_arr[0]->y;
    }

    /* "remove" the others (decrease their refcount) */
    for (i = 0; i < totviews; i++) {
      if (ibufs_arr[i] != ibuf) {
        IMB_freeImBuf(ibufs_arr[i]);
      }
    }

    MEM_freeN(ibufs_arr);
  }
  else {
  monoview_image:
    if ((ibuf = IMB_loadiffname(name, flag, seq->strip->colorspace_settings.name))) {
      /* we don't need both (speed reasons)! */
      if (ibuf->rect_float && ibuf->rect) {
        imb_freerectImBuf(ibuf);
      }

      /* all sequencer color is done in SRGB space, linear gives odd crossfades */
      BKE_sequencer_imbuf_to_sequencer_space(context->scene, ibuf, false);

      s_elem->orig_width = ibuf->x;
      s_elem->orig_height = ibuf->y;
    }
  }

  return ibuf;
}

static ImBuf *seq_render_movie_strip(const SeqRenderData *context,
                                     Sequence *seq,
                                     float nr,
                                     float cfra)
{
  ImBuf *ibuf = NULL;
  StripAnim *sanim;
  bool is_multiview = (seq->flag & SEQ_USE_VIEWS) != 0 &&
                      (context->scene->r.scemode & R_MULTIVIEW) != 0;

  /* load all the videos */
  seq_open_anim_file(context->scene, seq, false);

  if (is_multiview) {
    ImBuf **ibuf_arr;
    const int totfiles = seq_num_files(context->scene, seq->views_format, true);
    int totviews;
    int i;

    if (totfiles != BLI_listbase_count_at_most(&seq->anims, totfiles + 1)) {
      goto monoview_movie;
    }

    totviews = BKE_scene_multiview_num_views_get(&context->scene->r);
    ibuf_arr = MEM_callocN(sizeof(ImBuf *) * totviews, "Sequence Image Views Imbufs");

    for (i = 0, sanim = seq->anims.first; sanim; sanim = sanim->next, i++) {
      if (sanim->anim) {
        IMB_Proxy_Size proxy_size = seq_rendersize_to_proxysize(context->preview_render_size);
        IMB_anim_set_preseek(sanim->anim, seq->anim_preseek);

        ibuf_arr[i] = IMB_anim_absolute(sanim->anim,
                                        nr + seq->anim_startofs,
                                        seq->strip->proxy ? seq->strip->proxy->tc :
                                                            IMB_TC_RECORD_RUN,
                                        proxy_size);

        /* fetching for requested proxy size failed, try fetching the original instead */
        if (!ibuf_arr[i] && proxy_size != IMB_PROXY_NONE) {
          ibuf_arr[i] = IMB_anim_absolute(sanim->anim,
                                          nr + seq->anim_startofs,
                                          seq->strip->proxy ? seq->strip->proxy->tc :
                                                              IMB_TC_RECORD_RUN,
                                          IMB_PROXY_NONE);
        }
        if (ibuf_arr[i]) {
          /* we don't need both (speed reasons)! */
          if (ibuf_arr[i]->rect_float && ibuf_arr[i]->rect) {
            imb_freerectImBuf(ibuf_arr[i]);
          }
        }
      }
    }

    if (seq->views_format == R_IMF_VIEWS_STEREO_3D) {
      if (ibuf_arr[0]) {
        IMB_ImBufFromStereo3d(seq->stereo3d_format, ibuf_arr[0], &ibuf_arr[0], &ibuf_arr[1]);
      }
      else {
        /* probably proxy hasn't been created yet */
        MEM_freeN(ibuf_arr);
        return NULL;
      }
    }

    for (i = 0; i < totviews; i++) {
      SeqRenderData localcontext = *context;
      localcontext.view_id = i;

      if (ibuf_arr[i]) {
        /* all sequencer color is done in SRGB space, linear gives odd crossfades */
        BKE_sequencer_imbuf_to_sequencer_space(context->scene, ibuf_arr[i], false);
      }
      if (i != context->view_id) {
        BKE_sequencer_cache_put(
            &localcontext, seq, cfra, SEQ_CACHE_STORE_PREPROCESSED, ibuf_arr[i], 0);
      }
    }

    /* return the original requested ImBuf */
    ibuf = ibuf_arr[context->view_id];
    if (ibuf) {
      seq->strip->stripdata->orig_width = ibuf->x;
      seq->strip->stripdata->orig_height = ibuf->y;
    }

    /* "remove" the others (decrease their refcount) */
    for (i = 0; i < totviews; i++) {
      if (ibuf_arr[i] != ibuf) {
        IMB_freeImBuf(ibuf_arr[i]);
      }
    }

    MEM_freeN(ibuf_arr);
  }
  else {
  monoview_movie:
    sanim = seq->anims.first;
    if (sanim && sanim->anim) {
      IMB_Proxy_Size proxy_size = seq_rendersize_to_proxysize(context->preview_render_size);
      IMB_anim_set_preseek(sanim->anim, seq->anim_preseek);

      ibuf = IMB_anim_absolute(sanim->anim,
                               nr + seq->anim_startofs,
                               seq->strip->proxy ? seq->strip->proxy->tc : IMB_TC_RECORD_RUN,
                               proxy_size);

      /* fetching for requested proxy size failed, try fetching the original instead */
      if (!ibuf && proxy_size != IMB_PROXY_NONE) {
        ibuf = IMB_anim_absolute(sanim->anim,
                                 nr + seq->anim_startofs,
                                 seq->strip->proxy ? seq->strip->proxy->tc : IMB_TC_RECORD_RUN,
                                 IMB_PROXY_NONE);
      }
      if (ibuf) {
        BKE_sequencer_imbuf_to_sequencer_space(context->scene, ibuf, false);

        /* we don't need both (speed reasons)! */
        if (ibuf->rect_float && ibuf->rect) {
          imb_freerectImBuf(ibuf);
        }

        seq->strip->stripdata->orig_width = ibuf->x;
        seq->strip->stripdata->orig_height = ibuf->y;
      }
    }
  }
  return ibuf;
}

static ImBuf *seq_render_movieclip_strip(const SeqRenderData *context, Sequence *seq, float nr)
{
  ImBuf *ibuf = NULL;
  MovieClipUser user;
  float tloc[2], tscale, tangle;

  if (!seq->clip) {
    return NULL;
  }

  memset(&user, 0, sizeof(MovieClipUser));

  BKE_movieclip_user_set_frame(&user, nr + seq->anim_startofs + seq->clip->start_frame);

  user.render_flag |= MCLIP_PROXY_RENDER_USE_FALLBACK_RENDER;

  user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
  switch (seq_rendersize_to_proxysize(context->preview_render_size)) {
    case IMB_PROXY_NONE:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
      break;
    case IMB_PROXY_100:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_100;
      break;
    case IMB_PROXY_75:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_75;
      break;
    case IMB_PROXY_50:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_50;
      break;
    case IMB_PROXY_25:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_25;
      break;
  }

  if (seq->clip_flag & SEQ_MOVIECLIP_RENDER_UNDISTORTED) {
    user.render_flag |= MCLIP_PROXY_RENDER_UNDISTORT;
  }

  if (seq->clip_flag & SEQ_MOVIECLIP_RENDER_STABILIZED) {
    ibuf = BKE_movieclip_get_stable_ibuf(seq->clip, &user, tloc, &tscale, &tangle, 0);
  }
  else {
    ibuf = BKE_movieclip_get_ibuf_flag(seq->clip, &user, seq->clip->flag, MOVIECLIP_CACHE_SKIP);
  }

  return ibuf;
}

static ImBuf *seq_render_mask(const SeqRenderData *context, Mask *mask, float nr, bool make_float)
{
  /* TODO - add option to rasterize to alpha imbuf? */
  ImBuf *ibuf = NULL;
  float *maskbuf;
  int i;

  if (!mask) {
    return NULL;
  }
  else {
    AnimData *adt;
    Mask *mask_temp;
    MaskRasterHandle *mr_handle;

    mask_temp = BKE_mask_copy_nolib(mask);

    BKE_mask_evaluate(mask_temp, mask->sfra + nr, true);

    /* anim-data */
    adt = BKE_animdata_from_id(&mask->id);
    BKE_animsys_evaluate_animdata(
        context->depsgraph, context->scene, &mask_temp->id, adt, nr, ADT_RECALC_ANIM);

    maskbuf = MEM_mallocN(sizeof(float) * context->rectx * context->recty, __func__);

    mr_handle = BKE_maskrasterize_handle_new();

    BKE_maskrasterize_handle_init(
        mr_handle, mask_temp, context->rectx, context->recty, true, true, true);

    BKE_mask_free(mask_temp);
    MEM_freeN(mask_temp);

    BKE_maskrasterize_buffer(mr_handle, context->rectx, context->recty, maskbuf);

    BKE_maskrasterize_handle_free(mr_handle);
  }

  if (make_float) {
    /* pixels */
    const float *fp_src;
    float *fp_dst;

    ibuf = IMB_allocImBuf(context->rectx, context->recty, 32, IB_rectfloat);

    fp_src = maskbuf;
    fp_dst = ibuf->rect_float;
    i = context->rectx * context->recty;
    while (--i) {
      fp_dst[0] = fp_dst[1] = fp_dst[2] = *fp_src;
      fp_dst[3] = 1.0f;

      fp_src += 1;
      fp_dst += 4;
    }
  }
  else {
    /* pixels */
    const float *fp_src;
    unsigned char *ub_dst;

    ibuf = IMB_allocImBuf(context->rectx, context->recty, 32, IB_rect);

    fp_src = maskbuf;
    ub_dst = (unsigned char *)ibuf->rect;
    i = context->rectx * context->recty;
    while (--i) {
      ub_dst[0] = ub_dst[1] = ub_dst[2] = (unsigned char)(*fp_src * 255.0f); /* already clamped */
      ub_dst[3] = 255;

      fp_src += 1;
      ub_dst += 4;
    }
  }

  MEM_freeN(maskbuf);

  return ibuf;
}

static ImBuf *seq_render_mask_strip(const SeqRenderData *context, Sequence *seq, float nr)
{
  bool make_float = (seq->flag & SEQ_MAKE_FLOAT) != 0;

  return seq_render_mask(context, seq->mask, nr, make_float);
}

static ImBuf *seq_render_scene_strip(const SeqRenderData *context,
                                     Sequence *seq,
                                     float nr,
                                     float cfra)
{
  ImBuf *ibuf = NULL;
  double frame;
  Object *camera;

  struct {
    int scemode;
    int cfra;
    float subframe;

#ifdef DURIAN_CAMERA_SWITCH
    int mode;
#endif
  } orig_data;

  /* Old info:
   * Hack! This function can be called from do_render_seq(), in that case
   * the seq->scene can already have a Render initialized with same name,
   * so we have to use a default name. (compositor uses scene name to
   * find render).
   * However, when called from within the UI (image preview in sequencer)
   * we do want to use scene Render, that way the render result is defined
   * for display in render/imagewindow
   *
   * Hmm, don't see, why we can't do that all the time,
   * and since G.is_rendering is uhm, gone... (Peter)
   */

  /* New info:
   * Using the same name for the renders works just fine as the do_render_seq()
   * render is not used while the scene strips are rendered.
   *
   * However rendering from UI (through sequencer_preview_area_draw) can crash in
   * very many cases since other renders (material preview, an actual render etc.)
   * can be started while this sequence preview render is running. The only proper
   * solution is to make the sequencer preview render a proper job, which can be
   * stopped when needed. This would also give a nice progress bar for the preview
   * space so that users know there's something happening.
   *
   * As a result the active scene now only uses OpenGL rendering for the sequencer
   * preview. This is far from nice, but is the only way to prevent crashes at this
   * time.
   *
   * -jahka
   */

  const bool is_rendering = G.is_rendering;
  const bool is_background = G.background;
  const bool do_seq_gl = is_rendering ? 0 : (context->scene->r.seq_prev_type) != OB_RENDER;
  bool have_comp = false;
  bool use_gpencil = true;
  /* do we need to re-evaluate the frame after rendering? */
  bool is_frame_update = false;
  Scene *scene;
  int is_thread_main = BLI_thread_is_main();

  /* don't refer to seq->scene above this point!, it can be NULL */
  if (seq->scene == NULL) {
    return NULL;
  }

  scene = seq->scene;
  frame = (double)scene->r.sfra + (double)nr + (double)seq->anim_startofs;

#if 0 /* UNUSED */
  have_seq = (scene->r.scemode & R_DOSEQ) && scene->ed && scene->ed->seqbase.first);
#endif
  have_comp = (scene->r.scemode & R_DOCOMP) && scene->use_nodes && scene->nodetree;

  /* Get view layer for the strip. */
  ViewLayer *view_layer = BKE_view_layer_default_render(scene);
  /* Depsgraph will be NULL when doing rendering. */
  Depsgraph *depsgraph = NULL;

  orig_data.scemode = scene->r.scemode;
  orig_data.cfra = scene->r.cfra;
  orig_data.subframe = scene->r.subframe;
#ifdef DURIAN_CAMERA_SWITCH
  orig_data.mode = scene->r.mode;
#endif

  BKE_scene_frame_set(scene, frame);

  if (seq->scene_camera) {
    camera = seq->scene_camera;
  }
  else {
    BKE_scene_camera_switch_update(scene);
    camera = scene->camera;
  }

  if (have_comp == false && camera == NULL) {
    goto finally;
  }

  if (seq->flag & SEQ_SCENE_NO_GPENCIL) {
    use_gpencil = false;
  }

  /* prevent eternal loop */
  scene->r.scemode &= ~R_DOSEQ;

#ifdef DURIAN_CAMERA_SWITCH
  /* stooping to new low's in hackyness :( */
  scene->r.mode |= R_NO_CAMERA_SWITCH;
#endif

  is_frame_update = (orig_data.cfra != scene->r.cfra) || (orig_data.subframe != scene->r.subframe);

  if ((sequencer_view3d_cb && do_seq_gl && camera) && is_thread_main) {
    char err_out[256] = "unknown";
    const int width = (scene->r.xsch * scene->r.size) / 100;
    const int height = (scene->r.ysch * scene->r.size) / 100;
    const char *viewname = BKE_scene_multiview_render_view_name_get(&scene->r, context->view_id);

    unsigned int draw_flags = V3D_OFSDRAW_NONE;
    draw_flags |= (use_gpencil) ? V3D_OFSDRAW_SHOW_ANNOTATION : 0;
    draw_flags |= (context->gpu_full_samples) ? V3D_OFSDRAW_USE_FULL_SAMPLE : 0;
    draw_flags |= (context->scene->r.seq_flag & R_SEQ_OVERRIDE_SCENE_SETTINGS) ?
                      V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS :
                      0;

    /* for old scene this can be uninitialized,
     * should probably be added to do_versions at some point if the functionality stays */
    if (context->scene->r.seq_prev_type == 0) {
      context->scene->r.seq_prev_type = 3 /* == OB_SOLID */;
    }

    /* opengl offscreen render */
    depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
    BKE_scene_graph_update_for_newframe(depsgraph, context->bmain);
    ibuf = sequencer_view3d_cb(
        /* set for OpenGL render (NULL when scrubbing) */
        depsgraph,
        scene,
        &context->scene->display.shading,
        context->scene->r.seq_prev_type,
        camera,
        width,
        height,
        IB_rect,
        draw_flags,
        scene->r.alphamode,
        context->gpu_samples,
        viewname,
        context->gpu_offscreen,
        err_out);
    if (ibuf == NULL) {
      fprintf(stderr, "seq_render_scene_strip failed to get opengl buffer: %s\n", err_out);
    }
  }
  else {
    Render *re = RE_GetSceneRender(scene);
    const int totviews = BKE_scene_multiview_num_views_get(&scene->r);
    int i;
    ImBuf **ibufs_arr;

    ibufs_arr = MEM_callocN(sizeof(ImBuf *) * totviews, "Sequence Image Views Imbufs");

    /* XXX: this if can be removed when sequence preview rendering uses the job system
     *
     * disable rendered preview for sequencer while rendering -- it's very much possible
     * that preview render will went into conflict with final render
     *
     * When rendering from command line renderer is called from main thread, in this
     * case it's always safe to render scene here
     */
    if (!is_thread_main || is_rendering == false || is_background || context->for_render) {
      if (re == NULL) {
        re = RE_NewSceneRender(scene);
      }

      RE_BlenderFrame(re, context->bmain, scene, view_layer, camera, frame, false);

      /* restore previous state after it was toggled on & off by RE_BlenderFrame */
      G.is_rendering = is_rendering;
    }

    for (i = 0; i < totviews; i++) {
      SeqRenderData localcontext = *context;
      RenderResult rres;

      localcontext.view_id = i;

      RE_AcquireResultImage(re, &rres, i);

      if (rres.rectf) {
        ibufs_arr[i] = IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_rectfloat);
        memcpy(ibufs_arr[i]->rect_float, rres.rectf, 4 * sizeof(float) * rres.rectx * rres.recty);

        if (rres.rectz) {
          addzbuffloatImBuf(ibufs_arr[i]);
          memcpy(ibufs_arr[i]->zbuf_float, rres.rectz, sizeof(float) * rres.rectx * rres.recty);
        }

        /* float buffers in the sequencer are not linear */
        BKE_sequencer_imbuf_to_sequencer_space(context->scene, ibufs_arr[i], false);
      }
      else if (rres.rect32) {
        ibufs_arr[i] = IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_rect);
        memcpy(ibufs_arr[i]->rect, rres.rect32, 4 * rres.rectx * rres.recty);
      }

      if (i != context->view_id) {
        BKE_sequencer_cache_put(&localcontext, seq, cfra, SEQ_CACHE_STORE_RAW, ibufs_arr[i], 0);
      }

      RE_ReleaseResultImage(re);
    }

    /* return the original requested ImBuf */
    ibuf = ibufs_arr[context->view_id];

    /* "remove" the others (decrease their refcount) */
    for (i = 0; i < totviews; i++) {
      if (ibufs_arr[i] != ibuf) {
        IMB_freeImBuf(ibufs_arr[i]);
      }
    }
    MEM_freeN(ibufs_arr);
  }

finally:
  /* restore */
  scene->r.scemode = orig_data.scemode;
  scene->r.cfra = orig_data.cfra;
  scene->r.subframe = orig_data.subframe;

  if (is_frame_update && (depsgraph != NULL)) {
    BKE_scene_graph_update_for_newframe(depsgraph, context->bmain);
  }

#ifdef DURIAN_CAMERA_SWITCH
  /* stooping to new low's in hackyness :( */
  scene->r.mode &= ~(orig_data.mode & R_NO_CAMERA_SWITCH);
#endif

  return ibuf;
}

/**
 * Used for meta-strips & scenes with #SEQ_SCENE_STRIPS flag set.
 */
static ImBuf *do_render_strip_seqbase(const SeqRenderData *context,
                                      SeqRenderState *state,
                                      Sequence *seq,
                                      float nr,
                                      bool use_preprocess)
{
  ImBuf *meta_ibuf = NULL, *ibuf = NULL;
  ListBase *seqbase = NULL;
  int offset;

  seqbase = BKE_sequence_seqbase_get(seq, &offset);

  if (seqbase && !BLI_listbase_is_empty(seqbase)) {
    meta_ibuf = seq_render_strip_stack(context,
                                       state,
                                       seqbase,
                                       /* scene strips don't have their start taken into account */
                                       nr + offset,
                                       0);
  }

  if (meta_ibuf) {
    ibuf = meta_ibuf;
    if (ibuf && use_preprocess) {
      ImBuf *i = IMB_dupImBuf(ibuf);

      IMB_freeImBuf(ibuf);

      ibuf = i;
    }
  }

  return ibuf;
}

static ImBuf *do_render_strip_uncached(const SeqRenderData *context,
                                       SeqRenderState *state,
                                       Sequence *seq,
                                       float cfra)
{
  ImBuf *ibuf = NULL;
  float nr = give_stripelem_index(seq, cfra);
  int type = (seq->type & SEQ_TYPE_EFFECT && seq->type != SEQ_TYPE_SPEED) ? SEQ_TYPE_EFFECT :
                                                                            seq->type;
  bool use_preprocess = BKE_sequencer_input_have_to_preprocess(context, seq, cfra);

  switch (type) {
    case SEQ_TYPE_META: {
      ibuf = do_render_strip_seqbase(context, state, seq, nr, use_preprocess);
      break;
    }

    case SEQ_TYPE_SCENE: {
      if (seq->flag & SEQ_SCENE_STRIPS) {
        if (seq->scene && (context->scene != seq->scene)) {
          /* recusrive check */
          if (BLI_linklist_index(state->scene_parents, seq->scene) != -1) {
            break;
          }
          LinkNode scene_parent = {
              .next = state->scene_parents,
              .link = seq->scene,
          };
          state->scene_parents = &scene_parent;
          /* end check */

          /* Use the Scene Seq's scene for the context when rendering the scene's sequences
           * (necessary for Multicam Selector among others).
           */
          SeqRenderData local_context = *context;
          local_context.scene = seq->scene;
          local_context.skip_cache = true;

          ibuf = do_render_strip_seqbase(&local_context, state, seq, nr, use_preprocess);

          /* step back in the list */
          state->scene_parents = state->scene_parents->next;
        }
      }
      else {
        /* scene can be NULL after deletions */
        ibuf = seq_render_scene_strip(context, seq, nr, cfra);

        /* Scene strips update all animation, so we need to restore original state.*/
        BKE_animsys_evaluate_all_animation(
            context->bmain, context->depsgraph, context->scene, cfra);
      }
      break;
    }

    case SEQ_TYPE_SPEED: {
      ImBuf *child_ibuf = NULL;

      float f_cfra;
      SpeedControlVars *s = (SpeedControlVars *)seq->effectdata;

      BKE_sequence_effect_speed_rebuild_map(context->scene, seq, false);

      /* weeek! */
      f_cfra = seq->start + s->frameMap[(int)nr];

      child_ibuf = seq_render_strip(context, state, seq->seq1, f_cfra);

      if (child_ibuf) {
        ibuf = child_ibuf;
        if (ibuf && use_preprocess) {
          ImBuf *i = IMB_dupImBuf(ibuf);

          IMB_freeImBuf(ibuf);

          ibuf = i;
        }
      }
      break;
    }

    case SEQ_TYPE_EFFECT: {
      ibuf = seq_render_effect_strip_impl(context, state, seq, seq->start + nr);
      break;
    }

    case SEQ_TYPE_IMAGE: {
      ibuf = seq_render_image_strip(context, seq, nr, cfra);
      break;
    }

    case SEQ_TYPE_MOVIE: {
      ibuf = seq_render_movie_strip(context, seq, nr, cfra);
      break;
    }

    case SEQ_TYPE_MOVIECLIP: {
      ibuf = seq_render_movieclip_strip(context, seq, nr);

      if (ibuf) {
        /* duplicate frame so movie cache wouldn't be confused by sequencer's stuff */
        ImBuf *i = IMB_dupImBuf(ibuf);
        IMB_freeImBuf(ibuf);
        ibuf = i;

        if (ibuf->rect_float) {
          BKE_sequencer_imbuf_to_sequencer_space(context->scene, ibuf, false);
        }
      }

      break;
    }

    case SEQ_TYPE_MASK: {
      /* ibuf is always new */
      ibuf = seq_render_mask_strip(context, seq, nr);
      break;
    }
  }

  if (ibuf) {
    sequencer_imbuf_assign_spaces(context->scene, ibuf);
  }

  return ibuf;
}

/* Estimate time spent by the program rendering the strip */
static clock_t seq_estimate_render_cost_begin(void)
{
  return clock();
}

static float seq_estimate_render_cost_end(Scene *scene, clock_t begin)
{
  clock_t end = clock();
  float time_spent = (float)(end - begin);
  float time_max = (1.0f / scene->r.frs_sec) * CLOCKS_PER_SEC;

  if (time_max != 0) {
    return time_spent / time_max;
  }
  else {
    return 1;
  }
}

static ImBuf *seq_render_strip(const SeqRenderData *context,
                               SeqRenderState *state,
                               Sequence *seq,
                               float cfra)
{
  ImBuf *ibuf = NULL;
  bool use_preprocess = false;
  bool is_proxy_image = false;
  /* all effects are handled similarly with the exception of speed effect */
  int type = (seq->type & SEQ_TYPE_EFFECT && seq->type != SEQ_TYPE_SPEED) ? SEQ_TYPE_EFFECT :
                                                                            seq->type;
  bool is_preprocessed = !ELEM(
      type, SEQ_TYPE_IMAGE, SEQ_TYPE_MOVIE, SEQ_TYPE_SCENE, SEQ_TYPE_MOVIECLIP);

  clock_t begin = seq_estimate_render_cost_begin();

  ibuf = BKE_sequencer_cache_get(context, seq, cfra, SEQ_CACHE_STORE_PREPROCESSED);

  if (ibuf == NULL) {
    ibuf = BKE_sequencer_cache_get(context, seq, cfra, SEQ_CACHE_STORE_RAW);
    if (ibuf == NULL) {
      /* MOVIECLIPs have their own proxy management */
      if (seq->type != SEQ_TYPE_MOVIECLIP) {
        ibuf = seq_proxy_fetch(context, seq, cfra);
        is_proxy_image = (ibuf != NULL);
      }

      if (ibuf == NULL) {
        ibuf = do_render_strip_uncached(context, state, seq, cfra);
      }

      if (ibuf) {
        if (ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_MOVIECLIP)) {
          is_proxy_image = (context->preview_render_size != 100);
        }
      }
    }

    if (ibuf) {
      use_preprocess = BKE_sequencer_input_have_to_preprocess(context, seq, cfra);
    }

    if (ibuf == NULL) {
      ibuf = IMB_allocImBuf(context->rectx, context->recty, 32, IB_rect);
      sequencer_imbuf_assign_spaces(context->scene, ibuf);
    }

    if (context->is_proxy_render == false &&
        (ibuf->x != context->rectx || ibuf->y != context->recty)) {
      use_preprocess = true;
    }

    if (use_preprocess) {
      float cost = seq_estimate_render_cost_end(context->scene, begin);
      BKE_sequencer_cache_put(context, seq, cfra, SEQ_CACHE_STORE_RAW, ibuf, cost);

      /* reset timer so we can get partial render time */
      begin = seq_estimate_render_cost_begin();
      ibuf = input_preprocess(context, seq, cfra, ibuf, is_proxy_image, is_preprocessed);
    }

    float cost = seq_estimate_render_cost_end(context->scene, begin);
    BKE_sequencer_cache_put(context, seq, cfra, SEQ_CACHE_STORE_PREPROCESSED, ibuf, cost);
  }
  return ibuf;
}

/*********************** strip stack rendering functions *************************/

static bool seq_must_swap_input_in_blend_mode(Sequence *seq)
{
  bool swap_input = false;

  /* bad hack, to fix crazy input ordering of
   * those two effects */

  if (ELEM(seq->blend_mode, SEQ_TYPE_ALPHAOVER, SEQ_TYPE_ALPHAUNDER, SEQ_TYPE_OVERDROP)) {
    swap_input = true;
  }

  return swap_input;
}

static int seq_get_early_out_for_blend_mode(Sequence *seq)
{
  struct SeqEffectHandle sh = BKE_sequence_get_blend(seq);
  float facf = seq->blend_opacity / 100.0f;
  int early_out = sh.early_out(seq, facf, facf);

  if (ELEM(early_out, EARLY_DO_EFFECT, EARLY_NO_INPUT)) {
    return early_out;
  }

  if (seq_must_swap_input_in_blend_mode(seq)) {
    if (early_out == EARLY_USE_INPUT_2) {
      return EARLY_USE_INPUT_1;
    }
    else if (early_out == EARLY_USE_INPUT_1) {
      return EARLY_USE_INPUT_2;
    }
  }
  return early_out;
}

static ImBuf *seq_render_strip_stack_apply_effect(
    const SeqRenderData *context, Sequence *seq, float cfra, ImBuf *ibuf1, ImBuf *ibuf2)
{
  ImBuf *out;
  struct SeqEffectHandle sh = BKE_sequence_get_blend(seq);
  float facf = seq->blend_opacity / 100.0f;
  int swap_input = seq_must_swap_input_in_blend_mode(seq);

  if (swap_input) {
    if (sh.multithreaded) {
      out = seq_render_effect_execute_threaded(
          &sh, context, seq, cfra, facf, facf, ibuf2, ibuf1, NULL);
    }
    else {
      out = sh.execute(context, seq, cfra, facf, facf, ibuf2, ibuf1, NULL);
    }
  }
  else {
    if (sh.multithreaded) {
      out = seq_render_effect_execute_threaded(
          &sh, context, seq, cfra, facf, facf, ibuf1, ibuf2, NULL);
    }
    else {
      out = sh.execute(context, seq, cfra, facf, facf, ibuf1, ibuf2, NULL);
    }
  }

  return out;
}

static ImBuf *seq_render_strip_stack(const SeqRenderData *context,
                                     SeqRenderState *state,
                                     ListBase *seqbasep,
                                     float cfra,
                                     int chanshown)
{
  Sequence *seq_arr[MAXSEQ + 1];
  int count;
  int i;
  ImBuf *out = NULL;
  clock_t begin;

  count = get_shown_sequences(seqbasep, cfra, chanshown, (Sequence **)&seq_arr);

  if (count == 0) {
    return NULL;
  }

  for (i = count - 1; i >= 0; i--) {
    int early_out;
    Sequence *seq = seq_arr[i];

    out = BKE_sequencer_cache_get(context, seq, cfra, SEQ_CACHE_STORE_COMPOSITE);

    if (out) {
      break;
    }
    if (seq->blend_mode == SEQ_BLEND_REPLACE) {
      out = seq_render_strip(context, state, seq, cfra);
      break;
    }

    early_out = seq_get_early_out_for_blend_mode(seq);

    switch (early_out) {
      case EARLY_NO_INPUT:
      case EARLY_USE_INPUT_2:
        out = seq_render_strip(context, state, seq, cfra);
        break;
      case EARLY_USE_INPUT_1:
        if (i == 0) {
          out = IMB_allocImBuf(context->rectx, context->recty, 32, IB_rect);
        }
        break;
      case EARLY_DO_EFFECT:
        if (i == 0) {
          begin = seq_estimate_render_cost_begin();

          ImBuf *ibuf1 = IMB_allocImBuf(context->rectx, context->recty, 32, IB_rect);
          ImBuf *ibuf2 = seq_render_strip(context, state, seq, cfra);

          out = seq_render_strip_stack_apply_effect(context, seq, cfra, ibuf1, ibuf2);

          float cost = seq_estimate_render_cost_end(context->scene, begin);
          BKE_sequencer_cache_put(context, seq_arr[i], cfra, SEQ_CACHE_STORE_COMPOSITE, out, cost);

          IMB_freeImBuf(ibuf1);
          IMB_freeImBuf(ibuf2);
        }
        break;
    }
    if (out) {
      break;
    }
  }

  i++;
  for (; i < count; i++) {
    begin = seq_estimate_render_cost_begin();
    Sequence *seq = seq_arr[i];

    if (seq_get_early_out_for_blend_mode(seq) == EARLY_DO_EFFECT) {
      ImBuf *ibuf1 = out;
      ImBuf *ibuf2 = seq_render_strip(context, state, seq, cfra);

      out = seq_render_strip_stack_apply_effect(context, seq, cfra, ibuf1, ibuf2);

      IMB_freeImBuf(ibuf1);
      IMB_freeImBuf(ibuf2);
    }

    float cost = seq_estimate_render_cost_end(context->scene, begin);
    BKE_sequencer_cache_put(context, seq_arr[i], cfra, SEQ_CACHE_STORE_COMPOSITE, out, cost);
  }

  return out;
}

/*
 * returned ImBuf is refed!
 * you have to free after usage!
 */

ImBuf *BKE_sequencer_give_ibuf(const SeqRenderData *context, float cfra, int chanshown)
{
  Scene *scene = context->scene;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  ListBase *seqbasep;

  if (ed == NULL) {
    return NULL;
  }

  if ((chanshown < 0) && !BLI_listbase_is_empty(&ed->metastack)) {
    int count = BLI_listbase_count(&ed->metastack);
    count = max_ii(count + chanshown, 0);
    seqbasep = ((MetaStack *)BLI_findlink(&ed->metastack, count))->oldbasep;
  }
  else {
    seqbasep = ed->seqbasep;
  }

  SeqRenderState state;
  sequencer_state_init(&state);
  ImBuf *out = NULL;
  Sequence *seq_arr[MAXSEQ + 1];
  int count;

  count = get_shown_sequences(seqbasep, cfra, chanshown, seq_arr);

  if (count) {
    out = BKE_sequencer_cache_get(context, seq_arr[count - 1], cfra, SEQ_CACHE_STORE_FINAL_OUT);
  }

  BKE_sequencer_cache_free_temp_cache(context->scene, 0, cfra);

  clock_t begin = seq_estimate_render_cost_begin();
  float cost = 0;

  if (count && !out) {
    out = seq_render_strip_stack(context, &state, seqbasep, cfra, chanshown);
    cost = seq_estimate_render_cost_end(context->scene, begin);
    BKE_sequencer_cache_put_if_possible(
        context, seq_arr[count - 1], cfra, SEQ_CACHE_STORE_FINAL_OUT, out, cost);
  }

  return out;
}

ImBuf *BKE_sequencer_give_ibuf_seqbase(const SeqRenderData *context,
                                       float cfra,
                                       int chanshown,
                                       ListBase *seqbasep)
{
  SeqRenderState state;
  sequencer_state_init(&state);

  return seq_render_strip_stack(context, &state, seqbasep, cfra, chanshown);
}

ImBuf *BKE_sequencer_give_ibuf_direct(const SeqRenderData *context, float cfra, Sequence *seq)
{
  SeqRenderState state;
  sequencer_state_init(&state);

  ImBuf *ibuf = seq_render_strip(context, &state, seq, cfra);

  return ibuf;
}

/* *********************** threading api ******************* */

static ListBase running_threads;
static ListBase prefetch_wait;
static ListBase prefetch_done;

static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wakeup_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wakeup_cond = PTHREAD_COND_INITIALIZER;

// static pthread_mutex_t prefetch_ready_lock = PTHREAD_MUTEX_INITIALIZER;
// static pthread_cond_t  prefetch_ready_cond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t frame_done_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t frame_done_cond = PTHREAD_COND_INITIALIZER;

static volatile bool seq_thread_shutdown = true;
static volatile int seq_last_given_monoton_cfra = 0;
static int monoton_cfra = 0;

typedef struct PrefetchThread {
  struct PrefetchThread *next, *prev;

  Scene *scene;
  struct PrefetchQueueElem *current;
  pthread_t pthread;
  int running;

} PrefetchThread;

typedef struct PrefetchQueueElem {
  struct PrefetchQueueElem *next, *prev;

  int rectx;
  int recty;
  float cfra;
  int chanshown;
  int preview_render_size;

  int monoton_cfra;

  ImBuf *ibuf;
} PrefetchQueueElem;

void BKE_sequencer_give_ibuf_prefetch_request(const SeqRenderData *context,
                                              float cfra,
                                              int chanshown)
{
  PrefetchQueueElem *e;
  if (seq_thread_shutdown) {
    return;
  }

  e = MEM_callocN(sizeof(PrefetchQueueElem), "prefetch_queue_elem");
  e->rectx = context->rectx;
  e->recty = context->recty;
  e->cfra = cfra;
  e->chanshown = chanshown;
  e->preview_render_size = context->preview_render_size;
  e->monoton_cfra = monoton_cfra++;

  pthread_mutex_lock(&queue_lock);
  BLI_addtail(&prefetch_wait, e);
  pthread_mutex_unlock(&queue_lock);

  pthread_mutex_lock(&wakeup_lock);
  pthread_cond_signal(&wakeup_cond);
  pthread_mutex_unlock(&wakeup_lock);
}

ImBuf *BKE_sequencer_give_ibuf_threaded(const SeqRenderData *context, float cfra, int chanshown)
{
  PrefetchQueueElem *e = NULL;
  bool found_something = false;

  if (seq_thread_shutdown) {
    return BKE_sequencer_give_ibuf(context, cfra, chanshown);
  }

  while (!e) {
    bool success = false;
    pthread_mutex_lock(&queue_lock);

    for (e = prefetch_done.first; e; e = e->next) {
      if (cfra == e->cfra && chanshown == e->chanshown && context->rectx == e->rectx &&
          context->recty == e->recty && context->preview_render_size == e->preview_render_size) {
        success = true;
        found_something = true;
        break;
      }
    }

    if (!e) {
      for (e = prefetch_wait.first; e; e = e->next) {
        if (cfra == e->cfra && chanshown == e->chanshown && context->rectx == e->rectx &&
            context->recty == e->recty && context->preview_render_size == e->preview_render_size) {
          found_something = true;
          break;
        }
      }
    }

    if (!e) {
      PrefetchThread *tslot;

      for (tslot = running_threads.first; tslot; tslot = tslot->next) {
        if (tslot->current && cfra == tslot->current->cfra &&
            chanshown == tslot->current->chanshown && context->rectx == tslot->current->rectx &&
            context->recty == tslot->current->recty &&
            context->preview_render_size == tslot->current->preview_render_size) {
          found_something = true;
          break;
        }
      }
    }

    /* e->ibuf is unrefed by render thread on next round. */

    if (e) {
      seq_last_given_monoton_cfra = e->monoton_cfra;
    }

    pthread_mutex_unlock(&queue_lock);

    if (!success) {
      e = NULL;

      if (!found_something) {
        fprintf(stderr, "SEQ-THREAD: Requested frame not in queue ???\n");
        break;
      }
      pthread_mutex_lock(&frame_done_lock);
      pthread_cond_wait(&frame_done_cond, &frame_done_lock);
      pthread_mutex_unlock(&frame_done_lock);
    }
  }

  return e ? e->ibuf : NULL;
}

/* check whether sequence cur depends on seq */
bool BKE_sequence_check_depend(Sequence *seq, Sequence *cur)
{
  if (cur->seq1 == seq || cur->seq2 == seq || cur->seq3 == seq) {
    return true;
  }

  /* sequences are not intersecting in time, assume no dependency exists between them */
  if (cur->enddisp < seq->startdisp || cur->startdisp > seq->enddisp) {
    return false;
  }

  /* checking sequence is below reference one, not dependent on it */
  if (cur->machine < seq->machine) {
    return false;
  }

  /* sequence is not blending with lower machines, no dependency here occurs
   * check for non-effects only since effect could use lower machines as input
   */
  if ((cur->type & SEQ_TYPE_EFFECT) == 0 &&
      ((cur->blend_mode == SEQ_BLEND_REPLACE) ||
       (cur->blend_mode == SEQ_TYPE_CROSS && cur->blend_opacity == 100.0f))) {
    return false;
  }

  return true;
}

static void sequence_do_invalidate_dependent(Scene *scene, Sequence *seq, ListBase *seqbase)
{
  Sequence *cur;

  for (cur = seqbase->first; cur; cur = cur->next) {
    if (cur == seq) {
      continue;
    }

    if (BKE_sequence_check_depend(seq, cur)) {
      BKE_sequencer_cache_cleanup_sequence(scene, cur);
    }

    if (cur->seqbase.first) {
      sequence_do_invalidate_dependent(scene, seq, &cur->seqbase);
    }
  }
}

static void sequence_invalidate_cache(Scene *scene,
                                      Sequence *seq,
                                      bool invalidate_self,
                                      bool UNUSED(invalidate_preprocess))
{
  Editing *ed = scene->ed;

  /* invalidate cache for current sequence */
  if (invalidate_self) {
    /* Animation structure holds some buffers inside,
     * so for proper cache invalidation we need to
     * re-open the animation.
     */
    BKE_sequence_free_anim(seq);
    BKE_sequencer_cache_cleanup_sequence(scene, seq);
  }

  /* if invalidation is invoked from sequence free routine, effectdata would be NULL here */
  if (seq->effectdata && seq->type == SEQ_TYPE_SPEED) {
    BKE_sequence_effect_speed_rebuild_map(scene, seq, true);
  }

  /* invalidate cache for all dependent sequences */

  /* NOTE: can not use SEQ_BEGIN/SEQ_END here because that macro will change sequence's depth,
   *       which makes transformation routines work incorrect
   */
  sequence_do_invalidate_dependent(scene, seq, &ed->seqbase);
}

void BKE_sequence_invalidate_cache(Scene *scene, Sequence *seq)
{
  sequence_invalidate_cache(scene, seq, true, true);
}

void BKE_sequence_invalidate_dependent(Scene *scene, Sequence *seq)
{
  sequence_invalidate_cache(scene, seq, false, true);
}

void BKE_sequence_invalidate_cache_for_modifier(Scene *scene, Sequence *seq)
{
  sequence_invalidate_cache(scene, seq, true, false);
}

void BKE_sequencer_free_imbuf(Scene *scene, ListBase *seqbase, bool for_render)
{
  Sequence *seq;

  BKE_sequencer_cache_cleanup(scene);

  for (seq = seqbase->first; seq; seq = seq->next) {
    if (for_render && CFRA >= seq->startdisp && CFRA <= seq->enddisp) {
      continue;
    }

    if (seq->strip) {
      if (seq->type == SEQ_TYPE_MOVIE) {
        BKE_sequence_free_anim(seq);
      }
      if (seq->type == SEQ_TYPE_SPEED) {
        BKE_sequence_effect_speed_rebuild_map(scene, seq, true);
      }
    }
    if (seq->type == SEQ_TYPE_META) {
      BKE_sequencer_free_imbuf(scene, &seq->seqbase, for_render);
    }
    if (seq->type == SEQ_TYPE_SCENE) {
      /* FIXME: recurs downwards,
       * but do recurs protection somehow! */
    }
  }
}

static bool update_changed_seq_recurs(
    Scene *scene, Sequence *seq, Sequence *changed_seq, int len_change, int ibuf_change)
{
  Sequence *subseq;
  bool free_imbuf = false;

  /* recurs downwards to see if this seq depends on the changed seq */

  if (seq == NULL) {
    return false;
  }

  if (seq == changed_seq) {
    free_imbuf = true;
  }

  for (subseq = seq->seqbase.first; subseq; subseq = subseq->next) {
    if (update_changed_seq_recurs(scene, subseq, changed_seq, len_change, ibuf_change)) {
      free_imbuf = true;
    }
  }

  if (seq->seq1) {
    if (update_changed_seq_recurs(scene, seq->seq1, changed_seq, len_change, ibuf_change)) {
      free_imbuf = true;
    }
  }
  if (seq->seq2 && (seq->seq2 != seq->seq1)) {
    if (update_changed_seq_recurs(scene, seq->seq2, changed_seq, len_change, ibuf_change)) {
      free_imbuf = true;
    }
  }
  if (seq->seq3 && (seq->seq3 != seq->seq1) && (seq->seq3 != seq->seq2)) {
    if (update_changed_seq_recurs(scene, seq->seq3, changed_seq, len_change, ibuf_change)) {
      free_imbuf = true;
    }
  }

  if (free_imbuf) {
    if (ibuf_change) {
      if (seq->type == SEQ_TYPE_MOVIE) {
        BKE_sequence_free_anim(seq);
      }
      else if (seq->type == SEQ_TYPE_SPEED) {
        BKE_sequence_effect_speed_rebuild_map(scene, seq, true);
      }
    }

    if (len_change) {
      BKE_sequence_calc(scene, seq);
    }
  }

  return free_imbuf;
}

void BKE_sequencer_update_changed_seq_and_deps(Scene *scene,
                                               Sequence *changed_seq,
                                               int len_change,
                                               int ibuf_change)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;

  if (ed == NULL) {
    return;
  }

  for (seq = ed->seqbase.first; seq; seq = seq->next) {
    update_changed_seq_recurs(scene, seq, changed_seq, len_change, ibuf_change);
  }
}

/* seq funcs's for transforming internally
 * notice the difference between start/end and left/right.
 *
 * left and right are the bounds at which the sequence is rendered,
 * start and end are from the start and fixed length of the sequence.
 */
static int seq_tx_get_start(Sequence *seq)
{
  return seq->start;
}
static int seq_tx_get_end(Sequence *seq)
{
  return seq->start + seq->len;
}

int BKE_sequence_tx_get_final_left(Sequence *seq, bool metaclip)
{
  if (metaclip && seq->tmp) {
    /* return the range clipped by the parents range */
    return max_ii(BKE_sequence_tx_get_final_left(seq, false),
                  BKE_sequence_tx_get_final_left((Sequence *)seq->tmp, true));
  }
  else {
    return (seq->start - seq->startstill) + seq->startofs;
  }
}
int BKE_sequence_tx_get_final_right(Sequence *seq, bool metaclip)
{
  if (metaclip && seq->tmp) {
    /* return the range clipped by the parents range */
    return min_ii(BKE_sequence_tx_get_final_right(seq, false),
                  BKE_sequence_tx_get_final_right((Sequence *)seq->tmp, true));
  }
  else {
    return ((seq->start + seq->len) + seq->endstill) - seq->endofs;
  }
}

void BKE_sequence_tx_set_final_left(Sequence *seq, int val)
{
  if (val < (seq)->start) {
    seq->startstill = abs(val - (seq)->start);
    seq->startofs = 0;
  }
  else {
    seq->startofs = abs(val - (seq)->start);
    seq->startstill = 0;
  }
}

void BKE_sequence_tx_set_final_right(Sequence *seq, int val)
{
  if (val > (seq)->start + (seq)->len) {
    seq->endstill = abs(val - (seq->start + (seq)->len));
    seq->endofs = 0;
  }
  else {
    seq->endofs = abs(val - ((seq)->start + (seq)->len));
    seq->endstill = 0;
  }
}

/* used so we can do a quick check for single image seq
 * since they work a bit differently to normal image seq's (during transform) */
bool BKE_sequence_single_check(Sequence *seq)
{
  return ((seq->len == 1) &&
          (seq->type == SEQ_TYPE_IMAGE ||
           ((seq->type & SEQ_TYPE_EFFECT) && BKE_sequence_effect_get_num_inputs(seq->type) == 0)));
}

/* check if the selected seq's reference unselected seq's */
bool BKE_sequence_base_isolated_sel_check(ListBase *seqbase)
{
  Sequence *seq;
  /* is there more than 1 select */
  bool ok = false;

  for (seq = seqbase->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      ok = true;
      break;
    }
  }

  if (ok == false) {
    return false;
  }

  /* test relationships */
  for (seq = seqbase->first; seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0) {
      continue;
    }

    if (seq->flag & SELECT) {
      if ((seq->seq1 && (seq->seq1->flag & SELECT) == 0) ||
          (seq->seq2 && (seq->seq2->flag & SELECT) == 0) ||
          (seq->seq3 && (seq->seq3->flag & SELECT) == 0)) {
        return false;
      }
    }
    else {
      if ((seq->seq1 && (seq->seq1->flag & SELECT)) || (seq->seq2 && (seq->seq2->flag & SELECT)) ||
          (seq->seq3 && (seq->seq3->flag & SELECT))) {
        return false;
      }
    }
  }

  return true;
}

/* use to impose limits when dragging/extending - so impossible situations don't happen
 * Cant use the SEQ_LEFTSEL and SEQ_LEFTSEL directly because the strip may be in a metastrip */
void BKE_sequence_tx_handle_xlimits(Sequence *seq, int leftflag, int rightflag)
{
  if (leftflag) {
    if (BKE_sequence_tx_get_final_left(seq, false) >=
        BKE_sequence_tx_get_final_right(seq, false)) {
      BKE_sequence_tx_set_final_left(seq, BKE_sequence_tx_get_final_right(seq, false) - 1);
    }

    if (BKE_sequence_single_check(seq) == 0) {
      if (BKE_sequence_tx_get_final_left(seq, false) >= seq_tx_get_end(seq)) {
        BKE_sequence_tx_set_final_left(seq, seq_tx_get_end(seq) - 1);
      }

      /* dosnt work now - TODO */
#if 0
      if (seq_tx_get_start(seq) >= seq_tx_get_final_right(seq, 0)) {
        int ofs;
        ofs = seq_tx_get_start(seq) - seq_tx_get_final_right(seq, 0);
        seq->start -= ofs;
        seq_tx_set_final_left(seq, seq_tx_get_final_left(seq, 0) + ofs);
      }
#endif
    }
  }

  if (rightflag) {
    if (BKE_sequence_tx_get_final_right(seq, false) <=
        BKE_sequence_tx_get_final_left(seq, false)) {
      BKE_sequence_tx_set_final_right(seq, BKE_sequence_tx_get_final_left(seq, false) + 1);
    }

    if (BKE_sequence_single_check(seq) == 0) {
      if (BKE_sequence_tx_get_final_right(seq, false) <= seq_tx_get_start(seq)) {
        BKE_sequence_tx_set_final_right(seq, seq_tx_get_start(seq) + 1);
      }
    }
  }

  /* sounds cannot be extended past their endpoints */
  if (seq->type == SEQ_TYPE_SOUND_RAM) {
    seq->startstill = 0;
    seq->endstill = 0;
  }
}

void BKE_sequence_single_fix(Sequence *seq)
{
  int left, start, offset;
  if (!BKE_sequence_single_check(seq)) {
    return;
  }

  /* make sure the image is always at the start since there is only one,
   * adjusting its start should be ok */
  left = BKE_sequence_tx_get_final_left(seq, false);
  start = seq->start;
  if (start != left) {
    offset = left - start;
    BKE_sequence_tx_set_final_left(seq, BKE_sequence_tx_get_final_left(seq, false) - offset);
    BKE_sequence_tx_set_final_right(seq, BKE_sequence_tx_get_final_right(seq, false) - offset);
    seq->start += offset;
  }
}

bool BKE_sequence_tx_test(Sequence *seq)
{
  return !(seq->type & SEQ_TYPE_EFFECT) || (BKE_sequence_effect_get_num_inputs(seq->type) == 0);
}

/**
 * Return \a true if given \a seq needs a complete cleanup of its cache when it is transformed.
 *
 * Some (effect) strip types need a complete recache of themselves when they are transformed,
 * because they do not 'contain' anything and do not have any explicit relations to other strips.
 */
bool BKE_sequence_tx_fullupdate_test(Sequence *seq)
{
  return BKE_sequence_tx_test(seq) && ELEM(seq->type, SEQ_TYPE_ADJUSTMENT, SEQ_TYPE_MULTICAM);
}

static bool seq_overlap(Sequence *seq1, Sequence *seq2)
{
  return (seq1 != seq2 && seq1->machine == seq2->machine &&
          ((seq1->enddisp <= seq2->startdisp) || (seq1->startdisp >= seq2->enddisp)) == 0);
}

bool BKE_sequence_test_overlap(ListBase *seqbasep, Sequence *test)
{
  Sequence *seq;

  seq = seqbasep->first;
  while (seq) {
    if (seq_overlap(test, seq)) {
      return true;
    }

    seq = seq->next;
  }
  return false;
}

void BKE_sequence_translate(Scene *evil_scene, Sequence *seq, int delta)
{
  BKE_sequencer_offset_animdata(evil_scene, seq, delta);
  seq->start += delta;

  if (seq->type == SEQ_TYPE_META) {
    Sequence *seq_child;
    for (seq_child = seq->seqbase.first; seq_child; seq_child = seq_child->next) {
      BKE_sequence_translate(evil_scene, seq_child, delta);
    }
  }

  BKE_sequence_calc_disp(evil_scene, seq);
}

void BKE_sequence_sound_init(Scene *scene, Sequence *seq)
{
  if (seq->type == SEQ_TYPE_META) {
    Sequence *seq_child;
    for (seq_child = seq->seqbase.first; seq_child; seq_child = seq_child->next) {
      BKE_sequence_sound_init(scene, seq_child);
    }
  }
  else {
    if (seq->sound) {
      seq->scene_sound = BKE_sound_add_scene_sound_defaults(scene, seq);
    }
    if (seq->scene) {
      seq->scene_sound = BKE_sound_scene_add_scene_sound_defaults(scene, seq);
    }
  }
}

Sequence *BKE_sequencer_foreground_frame_get(Scene *scene, int frame)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq, *best_seq = NULL;
  int best_machine = -1;

  if (!ed) {
    return NULL;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SEQ_MUTE || seq->startdisp > frame || seq->enddisp <= frame) {
      continue;
    }
    /* Only use strips that generate an image, not ones that combine
     * other strips or apply some effect. */
    if (ELEM(seq->type,
             SEQ_TYPE_IMAGE,
             SEQ_TYPE_META,
             SEQ_TYPE_SCENE,
             SEQ_TYPE_MOVIE,
             SEQ_TYPE_COLOR,
             SEQ_TYPE_TEXT)) {
      if (seq->machine > best_machine) {
        best_seq = seq;
        best_machine = seq->machine;
      }
    }
  }
  return best_seq;
}

/* return 0 if there weren't enough space */
bool BKE_sequence_base_shuffle_ex(ListBase *seqbasep,
                                  Sequence *test,
                                  Scene *evil_scene,
                                  int channel_delta)
{
  const int orig_machine = test->machine;
  BLI_assert(ELEM(channel_delta, -1, 1));

  test->machine += channel_delta;
  BKE_sequence_calc(evil_scene, test);
  while (BKE_sequence_test_overlap(seqbasep, test)) {
    if ((channel_delta > 0) ? (test->machine >= MAXSEQ) : (test->machine < 1)) {
      break;
    }

    test->machine += channel_delta;
    BKE_sequence_calc(
        evil_scene,
        test);  // XXX - I don't think this is needed since were only moving vertically, Campbell.
  }

  if ((test->machine < 1) || (test->machine > MAXSEQ)) {
    /* Blender 2.4x would remove the strip.
     * nicer to move it to the end */

    Sequence *seq;
    int new_frame = test->enddisp;

    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->machine == orig_machine) {
        new_frame = max_ii(new_frame, seq->enddisp);
      }
    }

    test->machine = orig_machine;
    new_frame = new_frame + (test->start - test->startdisp); /* adjust by the startdisp */
    BKE_sequence_translate(evil_scene, test, new_frame - test->start);

    BKE_sequence_calc(evil_scene, test);
    return false;
  }
  else {
    return true;
  }
}

bool BKE_sequence_base_shuffle(ListBase *seqbasep, Sequence *test, Scene *evil_scene)
{
  return BKE_sequence_base_shuffle_ex(seqbasep, test, evil_scene, 1);
}

static int shuffle_seq_time_offset_test(ListBase *seqbasep, char dir)
{
  int offset = 0;
  Sequence *seq, *seq_other;

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (seq->tmp) {
      for (seq_other = seqbasep->first; seq_other; seq_other = seq_other->next) {
        if (!seq_other->tmp && seq_overlap(seq, seq_other)) {
          if (dir == 'L') {
            offset = min_ii(offset, seq_other->startdisp - seq->enddisp);
          }
          else {
            offset = max_ii(offset, seq_other->enddisp - seq->startdisp);
          }
        }
      }
    }
  }
  return offset;
}

static int shuffle_seq_time_offset(Scene *scene, ListBase *seqbasep, char dir)
{
  int ofs = 0;
  int tot_ofs = 0;
  Sequence *seq;
  while ((ofs = shuffle_seq_time_offset_test(seqbasep, dir))) {
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->tmp) {
        /* seq_test_overlap only tests display values */
        seq->startdisp += ofs;
        seq->enddisp += ofs;
      }
    }

    tot_ofs += ofs;
  }

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (seq->tmp) {
      BKE_sequence_calc_disp(scene, seq); /* corrects dummy startdisp/enddisp values */
    }
  }

  return tot_ofs;
}

bool BKE_sequence_base_shuffle_time(ListBase *seqbasep, Scene *evil_scene)
{
  /* note: seq->tmp is used to tag strips to move */

  Sequence *seq;

  int offset_l = shuffle_seq_time_offset(evil_scene, seqbasep, 'L');
  int offset_r = shuffle_seq_time_offset(evil_scene, seqbasep, 'R');
  int offset = (-offset_l < offset_r) ? offset_l : offset_r;

  if (offset) {
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->tmp) {
        BKE_sequence_translate(evil_scene, seq, offset);
        seq->flag &= ~SEQ_OVERLAP;
      }
    }
  }

  return offset ? false : true;
}

/* Unlike _update_sound_ funcs, these ones take info from audaspace to update sequence length! */
#ifdef WITH_AUDASPACE
static bool sequencer_refresh_sound_length_recursive(Scene *scene, ListBase *seqbase)
{
  Sequence *seq;
  bool changed = false;

  for (seq = seqbase->first; seq; seq = seq->next) {
    if (seq->type == SEQ_TYPE_META) {
      if (sequencer_refresh_sound_length_recursive(scene, &seq->seqbase)) {
        BKE_sequence_calc(scene, seq);
        changed = true;
      }
    }
    else if (seq->type == SEQ_TYPE_SOUND_RAM) {
      AUD_SoundInfo info = AUD_getInfo(seq->sound->playback_handle);
      int old = seq->len;
      float fac;

      seq->len = (int)ceil((double)info.length * FPS);
      fac = (float)seq->len / (float)old;
      old = seq->startofs;
      seq->startofs *= fac;
      seq->endofs *= fac;
      seq->start += (old - seq->startofs); /* So that visual/"real" start frame does not change! */

      BKE_sequence_calc(scene, seq);
      changed = true;
    }
  }
  return changed;
}
#endif

void BKE_sequencer_refresh_sound_length(Scene *scene)
{
#ifdef WITH_AUDASPACE
  if (scene->ed) {
    sequencer_refresh_sound_length_recursive(scene, &scene->ed->seqbase);
  }
#else
  (void)scene;
#endif
}

void BKE_sequencer_update_sound_bounds_all(Scene *scene)
{
  Editing *ed = scene->ed;

  if (ed) {
    Sequence *seq;

    for (seq = ed->seqbase.first; seq; seq = seq->next) {
      if (seq->type == SEQ_TYPE_META) {
        seq_update_sound_bounds_recursive(scene, seq);
      }
      else if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
        BKE_sequencer_update_sound_bounds(scene, seq);
      }
    }
  }
}

void BKE_sequencer_update_sound_bounds(Scene *scene, Sequence *seq)
{
  if (seq->type == SEQ_TYPE_SCENE) {
    if (seq->scene_sound) {
      /* We have to take into account start frame of the sequence's scene! */
      int startofs = seq->startofs + seq->anim_startofs + seq->scene->r.sfra;

      BKE_sound_move_scene_sound(scene, seq->scene_sound, seq->startdisp, seq->enddisp, startofs);
    }
  }
  else {
    BKE_sound_move_scene_sound_defaults(scene, seq);
  }
  /* mute is set in seq_update_muting_recursive */
}

static void seq_update_muting_recursive(ListBase *seqbasep, Sequence *metaseq, int mute)
{
  Sequence *seq;
  int seqmute;

  /* for sound we go over full meta tree to update muted state,
   * since sound is played outside of evaluating the imbufs, */
  for (seq = seqbasep->first; seq; seq = seq->next) {
    seqmute = (mute || (seq->flag & SEQ_MUTE));

    if (seq->type == SEQ_TYPE_META) {
      /* if this is the current meta sequence, unmute because
       * all sequences above this were set to mute */
      if (seq == metaseq) {
        seqmute = 0;
      }

      seq_update_muting_recursive(&seq->seqbase, metaseq, seqmute);
    }
    else if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
      if (seq->scene_sound) {
        BKE_sound_mute_scene_sound(seq->scene_sound, seqmute);
      }
    }
  }
}

void BKE_sequencer_update_muting(Editing *ed)
{
  if (ed) {
    /* mute all sounds up to current metastack list */
    MetaStack *ms = ed->metastack.last;

    if (ms) {
      seq_update_muting_recursive(&ed->seqbase, ms->parseq, 1);
    }
    else {
      seq_update_muting_recursive(&ed->seqbase, NULL, 0);
    }
  }
}

static void seq_update_sound_recursive(Scene *scene, ListBase *seqbasep, bSound *sound)
{
  Sequence *seq;

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (seq->type == SEQ_TYPE_META) {
      seq_update_sound_recursive(scene, &seq->seqbase, sound);
    }
    else if (seq->type == SEQ_TYPE_SOUND_RAM) {
      if (seq->scene_sound && sound == seq->sound) {
        BKE_sound_update_scene_sound(seq->scene_sound, sound);
      }
    }
  }
}

void BKE_sequencer_update_sound(Scene *scene, bSound *sound)
{
  if (scene->ed) {
    seq_update_sound_recursive(scene, &scene->ed->seqbase, sound);
  }
}

/* in cases where we done know the sequence's listbase */
ListBase *BKE_sequence_seqbase(ListBase *seqbase, Sequence *seq)
{
  Sequence *iseq;
  ListBase *lb = NULL;

  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    if (seq == iseq) {
      return seqbase;
    }
    else if (iseq->seqbase.first && (lb = BKE_sequence_seqbase(&iseq->seqbase, seq))) {
      return lb;
    }
  }

  return NULL;
}

Sequence *BKE_sequence_metastrip(ListBase *seqbase, Sequence *meta, Sequence *seq)
{
  Sequence *iseq;

  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    Sequence *rval;

    if (seq == iseq) {
      return meta;
    }
    else if (iseq->seqbase.first && (rval = BKE_sequence_metastrip(&iseq->seqbase, iseq, seq))) {
      return rval;
    }
  }

  return NULL;
}

int BKE_sequence_swap(Sequence *seq_a, Sequence *seq_b, const char **error_str)
{
  char name[sizeof(seq_a->name)];

  if (seq_a->len != seq_b->len) {
    *error_str = N_("Strips must be the same length");
    return 0;
  }

  /* type checking, could be more advanced but disallow sound vs non-sound copy */
  if (seq_a->type != seq_b->type) {
    if (seq_a->type == SEQ_TYPE_SOUND_RAM || seq_b->type == SEQ_TYPE_SOUND_RAM) {
      *error_str = N_("Strips were not compatible");
      return 0;
    }

    /* disallow effects to swap with non-effects strips */
    if ((seq_a->type & SEQ_TYPE_EFFECT) != (seq_b->type & SEQ_TYPE_EFFECT)) {
      *error_str = N_("Strips were not compatible");
      return 0;
    }

    if ((seq_a->type & SEQ_TYPE_EFFECT) && (seq_b->type & SEQ_TYPE_EFFECT)) {
      if (BKE_sequence_effect_get_num_inputs(seq_a->type) !=
          BKE_sequence_effect_get_num_inputs(seq_b->type)) {
        *error_str = N_("Strips must have the same number of inputs");
        return 0;
      }
    }
  }

  SWAP(Sequence, *seq_a, *seq_b);

  /* swap back names so animation fcurves don't get swapped */
  BLI_strncpy(name, seq_a->name + 2, sizeof(name));
  BLI_strncpy(seq_a->name + 2, seq_b->name + 2, sizeof(seq_b->name) - 2);
  BLI_strncpy(seq_b->name + 2, name, sizeof(seq_b->name) - 2);

  /* swap back opacity, and overlay mode */
  SWAP(int, seq_a->blend_mode, seq_b->blend_mode);
  SWAP(float, seq_a->blend_opacity, seq_b->blend_opacity);

  SWAP(Sequence *, seq_a->prev, seq_b->prev);
  SWAP(Sequence *, seq_a->next, seq_b->next);
  SWAP(int, seq_a->start, seq_b->start);
  SWAP(int, seq_a->startofs, seq_b->startofs);
  SWAP(int, seq_a->endofs, seq_b->endofs);
  SWAP(int, seq_a->startstill, seq_b->startstill);
  SWAP(int, seq_a->endstill, seq_b->endstill);
  SWAP(int, seq_a->machine, seq_b->machine);
  SWAP(int, seq_a->startdisp, seq_b->startdisp);
  SWAP(int, seq_a->enddisp, seq_b->enddisp);

  return 1;
}

/* prefix + [" + escaped_name + "] + \0 */
#define SEQ_RNAPATH_MAXSTR ((30 + 2 + (SEQ_NAME_MAXSTR * 2) + 2) + 1)

static size_t sequencer_rna_path_prefix(char str[SEQ_RNAPATH_MAXSTR], const char *name)
{
  char name_esc[SEQ_NAME_MAXSTR * 2];

  BLI_strescape(name_esc, name, sizeof(name_esc));
  return BLI_snprintf_rlen(
      str, SEQ_RNAPATH_MAXSTR, "sequence_editor.sequences_all[\"%s\"]", name_esc);
}

/* XXX - hackish function needed for transforming strips! TODO - have some better solution */
void BKE_sequencer_offset_animdata(Scene *scene, Sequence *seq, int ofs)
{
  char str[SEQ_RNAPATH_MAXSTR];
  size_t str_len;
  FCurve *fcu;

  if (scene->adt == NULL || ofs == 0 || scene->adt->action == NULL) {
    return;
  }

  str_len = sequencer_rna_path_prefix(str, seq->name + 2);

  for (fcu = scene->adt->action->curves.first; fcu; fcu = fcu->next) {
    if (STREQLEN(fcu->rna_path, str, str_len)) {
      unsigned int i;
      if (fcu->bezt) {
        for (i = 0; i < fcu->totvert; i++) {
          BezTriple *bezt = &fcu->bezt[i];
          bezt->vec[0][0] += ofs;
          bezt->vec[1][0] += ofs;
          bezt->vec[2][0] += ofs;
        }
      }
      if (fcu->fpt) {
        for (i = 0; i < fcu->totvert; i++) {
          FPoint *fpt = &fcu->fpt[i];
          fpt->vec[0] += ofs;
        }
      }
    }
  }
}

void BKE_sequencer_dupe_animdata(Scene *scene, const char *name_src, const char *name_dst)
{
  char str_from[SEQ_RNAPATH_MAXSTR];
  size_t str_from_len;
  FCurve *fcu;
  FCurve *fcu_last;
  FCurve *fcu_cpy;
  ListBase lb = {NULL, NULL};

  if (scene->adt == NULL || scene->adt->action == NULL) {
    return;
  }

  str_from_len = sequencer_rna_path_prefix(str_from, name_src);

  fcu_last = scene->adt->action->curves.last;

  for (fcu = scene->adt->action->curves.first; fcu && fcu->prev != fcu_last; fcu = fcu->next) {
    if (STREQLEN(fcu->rna_path, str_from, str_from_len)) {
      fcu_cpy = copy_fcurve(fcu);
      BLI_addtail(&lb, fcu_cpy);
    }
  }

  /* notice validate is 0, keep this because the seq may not be added to the scene yet */
  BKE_animdata_fix_paths_rename(
      &scene->id, scene->adt, NULL, "sequence_editor.sequences_all", name_src, name_dst, 0, 0, 0);

  /* add the original fcurves back */
  BLI_movelisttolist(&scene->adt->action->curves, &lb);
}

/* XXX - hackish function needed to remove all fcurves belonging to a sequencer strip */
static void seq_free_animdata(Scene *scene, Sequence *seq)
{
  char str[SEQ_RNAPATH_MAXSTR];
  size_t str_len;
  FCurve *fcu;

  if (scene->adt == NULL || scene->adt->action == NULL) {
    return;
  }

  str_len = sequencer_rna_path_prefix(str, seq->name + 2);

  fcu = scene->adt->action->curves.first;

  while (fcu) {
    if (STREQLEN(fcu->rna_path, str, str_len)) {
      FCurve *next_fcu = fcu->next;

      BLI_remlink(&scene->adt->action->curves, fcu);
      free_fcurve(fcu);

      fcu = next_fcu;
    }
    else {
      fcu = fcu->next;
    }
  }
}

#undef SEQ_RNAPATH_MAXSTR

Sequence *BKE_sequence_get_by_name(ListBase *seqbase, const char *name, bool recursive)
{
  Sequence *iseq = NULL;
  Sequence *rseq = NULL;

  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    if (STREQ(name, iseq->name + 2)) {
      return iseq;
    }
    else if (recursive && (iseq->seqbase.first) &&
             (rseq = BKE_sequence_get_by_name(&iseq->seqbase, name, 1))) {
      return rseq;
    }
  }

  return NULL;
}

/**
 * Only use as last resort when the StripElem is available but no the Sequence.
 * (needed for RNA)
 */
Sequence *BKE_sequencer_from_elem(ListBase *seqbase, StripElem *se)
{
  Sequence *iseq;

  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    Sequence *seq_found;
    if ((iseq->strip && iseq->strip->stripdata) &&
        (ARRAY_HAS_ITEM(se, iseq->strip->stripdata, iseq->len))) {
      break;
    }
    else if ((seq_found = BKE_sequencer_from_elem(&iseq->seqbase, se))) {
      iseq = seq_found;
      break;
    }
  }

  return iseq;
}

Sequence *BKE_sequencer_active_get(Scene *scene)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed == NULL) {
    return NULL;
  }

  return ed->act_seq;
}

void BKE_sequencer_active_set(Scene *scene, Sequence *seq)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed == NULL) {
    return;
  }

  ed->act_seq = seq;
}

int BKE_sequencer_active_get_pair(Scene *scene, Sequence **seq_act, Sequence **seq_other)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  *seq_act = BKE_sequencer_active_get(scene);

  if (*seq_act == NULL) {
    return 0;
  }
  else {
    Sequence *seq;

    *seq_other = NULL;

    for (seq = ed->seqbasep->first; seq; seq = seq->next) {
      if (seq->flag & SELECT && (seq != (*seq_act))) {
        if (*seq_other) {
          return 0;
        }
        else {
          *seq_other = seq;
        }
      }
    }

    return (*seq_other != NULL);
  }
}

Mask *BKE_sequencer_mask_get(Scene *scene)
{
  Sequence *seq_act = BKE_sequencer_active_get(scene);

  if (seq_act && seq_act->type == SEQ_TYPE_MASK) {
    return seq_act->mask;
  }
  else {
    return NULL;
  }
}

/* api like funcs for adding */

static void seq_load_apply(Main *bmain, Scene *scene, Sequence *seq, SeqLoadInfo *seq_load)
{
  if (seq) {
    BLI_strncpy_utf8(seq->name + 2, seq_load->name, sizeof(seq->name) - 2);
    BLI_utf8_invalid_strip(seq->name + 2, strlen(seq->name + 2));
    BKE_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);

    if (seq_load->flag & SEQ_LOAD_FRAME_ADVANCE) {
      seq_load->start_frame += (seq->enddisp - seq->startdisp);
    }

    if (seq_load->flag & SEQ_LOAD_REPLACE_SEL) {
      seq_load->flag |= SELECT;
      BKE_sequencer_active_set(scene, seq);
    }

    if (seq_load->flag & SEQ_LOAD_SOUND_MONO) {
      seq->sound->flags |= SOUND_FLAGS_MONO;
      BKE_sound_load(bmain, seq->sound);
    }

    if (seq_load->flag & SEQ_LOAD_SOUND_CACHE) {
      if (seq->sound) {
        BKE_sound_cache(seq->sound);
      }
    }

    seq_load->tot_success++;
  }
  else {
    seq_load->tot_error++;
  }
}

Sequence *BKE_sequence_alloc(ListBase *lb, int cfra, int machine)
{
  Sequence *seq;

  seq = MEM_callocN(sizeof(Sequence), "addseq");
  BLI_addtail(lb, seq);

  *((short *)seq->name) = ID_SEQ;
  seq->name[2] = 0;

  seq->flag = SELECT;
  seq->start = cfra;
  seq->machine = machine;
  seq->sat = 1.0;
  seq->mul = 1.0;
  seq->blend_opacity = 100.0;
  seq->volume = 1.0f;
  seq->pitch = 1.0f;
  seq->scene_sound = NULL;

  seq->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Sequence Stereo Format");
  seq->cache_flag = SEQ_CACHE_ALL_TYPES;

  return seq;
}

void BKE_sequence_alpha_mode_from_extension(Sequence *seq)
{
  if (seq->strip && seq->strip->stripdata) {
    const char *filename = seq->strip->stripdata->name;
    seq->alpha_mode = BKE_image_alpha_mode_from_extension_ex(filename);
  }
}

void BKE_sequence_init_colorspace(Sequence *seq)
{
  if (seq->strip && seq->strip->stripdata) {
    char name[FILE_MAX];
    ImBuf *ibuf;

    BLI_join_dirfile(name, sizeof(name), seq->strip->dir, seq->strip->stripdata->name);
    BLI_path_abs(name, BKE_main_blendfile_path_from_global());

    /* initialize input color space */
    if (seq->type == SEQ_TYPE_IMAGE) {
      ibuf = IMB_loadiffname(
          name, IB_test | IB_alphamode_detect, seq->strip->colorspace_settings.name);

      /* byte images are default to straight alpha, however sequencer
       * works in premul space, so mark strip to be premultiplied first
       */
      seq->alpha_mode = SEQ_ALPHA_STRAIGHT;
      if (ibuf) {
        if (ibuf->flags & IB_alphamode_premul) {
          seq->alpha_mode = IMA_ALPHA_PREMUL;
        }

        IMB_freeImBuf(ibuf);
      }
    }
  }
}

float BKE_sequence_get_fps(Scene *scene, Sequence *seq)
{
  switch (seq->type) {
    case SEQ_TYPE_MOVIE: {
      seq_open_anim_file(scene, seq, true);
      if (BLI_listbase_is_empty(&seq->anims)) {
        return 0.0f;
      }
      StripAnim *strip_anim = seq->anims.first;
      if (strip_anim->anim == NULL) {
        return 0.0f;
      }
      short frs_sec;
      float frs_sec_base;
      if (IMB_anim_get_fps(strip_anim->anim, &frs_sec, &frs_sec_base, true)) {
        return (float)frs_sec / frs_sec_base;
      }
      break;
    }
    case SEQ_TYPE_MOVIECLIP:
      if (seq->clip != NULL) {
        return BKE_movieclip_get_fps(seq->clip);
      }
      break;
    case SEQ_TYPE_SCENE:
      if (seq->scene != NULL) {
        return (float)seq->scene->r.frs_sec / seq->scene->r.frs_sec_base;
      }
      break;
  }
  return 0.0f;
}

/* NOTE: this function doesn't fill in image names */
Sequence *BKE_sequencer_add_image_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
  Scene *scene = CTX_data_scene(C); /* only for active seq */
  Sequence *seq;
  Strip *strip;

  seq = BKE_sequence_alloc(seqbasep, seq_load->start_frame, seq_load->channel);
  seq->type = SEQ_TYPE_IMAGE;
  seq->blend_mode = SEQ_TYPE_ALPHAOVER;

  /* basic defaults */
  seq->strip = strip = MEM_callocN(sizeof(Strip), "strip");

  seq->len = seq_load->len ? seq_load->len : 1;
  strip->us = 1;
  strip->stripdata = MEM_callocN(seq->len * sizeof(StripElem), "stripelem");
  BLI_strncpy(strip->dir, seq_load->path, sizeof(strip->dir));

  if (seq_load->stereo3d_format) {
    *seq->stereo3d_format = *seq_load->stereo3d_format;
  }

  seq->views_format = seq_load->views_format;
  seq->flag |= seq_load->flag & SEQ_USE_VIEWS;

  seq_load_apply(CTX_data_main(C), scene, seq, seq_load);

  return seq;
}

#ifdef WITH_AUDASPACE
Sequence *BKE_sequencer_add_sound_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C); /* only for sound */
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  bSound *sound;

  Sequence *seq; /* generic strip vars */
  Strip *strip;
  StripElem *se;

  AUD_SoundInfo info;

  sound = BKE_sound_new_file(bmain, seq_load->path); /* handles relative paths */

  if (sound->playback_handle == NULL) {
    BKE_id_free(bmain, sound);
    return NULL;
  }

  info = AUD_getInfo(sound->playback_handle);

  if (info.specs.channels == AUD_CHANNELS_INVALID) {
    BKE_id_free(bmain, sound);
    return NULL;
  }

  seq = BKE_sequence_alloc(seqbasep, seq_load->start_frame, seq_load->channel);

  seq->type = SEQ_TYPE_SOUND_RAM;
  seq->sound = sound;
  BLI_strncpy(seq->name + 2, "Sound", SEQ_NAME_MAXSTR - 2);
  BKE_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);

  /* basic defaults */
  seq->strip = strip = MEM_callocN(sizeof(Strip), "strip");
  /* We add a very small negative offset here, because
   * ceil(132.0) == 133.0, not nice with videos, see T47135. */
  seq->len = (int)ceil((double)info.length * FPS - 1e-4);
  strip->us = 1;

  /* we only need 1 element to store the filename */
  strip->stripdata = se = MEM_callocN(sizeof(StripElem), "stripelem");

  BLI_split_dirfile(seq_load->path, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

  seq->scene_sound = BKE_sound_add_scene_sound(
      scene, seq, seq_load->start_frame, seq_load->start_frame + seq->len, 0);

  BKE_sequence_calc_disp(scene, seq);

  /* last active name */
  BLI_strncpy(ed->act_sounddir, strip->dir, FILE_MAXDIR);

  seq_load_apply(bmain, scene, seq, seq_load);

  return seq;
}
#else   // WITH_AUDASPACE
Sequence *BKE_sequencer_add_sound_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
  (void)C;
  (void)seqbasep;
  (void)seq_load;
  return NULL;
}
#endif  // WITH_AUDASPACE

static void seq_anim_add_suffix(Scene *scene, struct anim *anim, const int view_id)
{
  const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, view_id);
  IMB_suffix_anim(anim, suffix);
}

Sequence *BKE_sequencer_add_movie_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C); /* only for sound */
  char path[sizeof(seq_load->path)];

  Sequence *seq; /* generic strip vars */
  Strip *strip;
  StripElem *se;
  char colorspace[64] = "\0"; /* MAX_COLORSPACE_NAME */
  bool is_multiview_loaded = false;
  const bool is_multiview = (seq_load->flag & SEQ_USE_VIEWS) != 0;
  const int totfiles = seq_num_files(scene, seq_load->views_format, is_multiview);
  struct anim **anim_arr;
  int i;

  BLI_strncpy(path, seq_load->path, sizeof(path));
  BLI_path_abs(path, BKE_main_blendfile_path(bmain));

  anim_arr = MEM_callocN(sizeof(struct anim *) * totfiles, "Video files");

  if (is_multiview && (seq_load->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
    char prefix[FILE_MAX];
    const char *ext = NULL;
    size_t j = 0;

    BKE_scene_multiview_view_prefix_get(scene, path, prefix, &ext);

    if (prefix[0] != '\0') {
      for (i = 0; i < totfiles; i++) {
        char str[FILE_MAX];

        seq_multiview_name(scene, i, prefix, ext, str, FILE_MAX);
        anim_arr[j] = openanim(str, IB_rect, 0, colorspace);

        if (anim_arr[j]) {
          seq_anim_add_suffix(scene, anim_arr[j], i);
          j++;
        }
      }

      if (j == 0) {
        MEM_freeN(anim_arr);
        return NULL;
      }
      is_multiview_loaded = true;
    }
  }

  if (is_multiview_loaded == false) {
    anim_arr[0] = openanim(path, IB_rect, 0, colorspace);

    if (anim_arr[0] == NULL) {
      MEM_freeN(anim_arr);
      return NULL;
    }
  }

  seq = BKE_sequence_alloc(seqbasep, seq_load->start_frame, seq_load->channel);

  /* multiview settings */
  if (seq_load->stereo3d_format) {
    *seq->stereo3d_format = *seq_load->stereo3d_format;
    seq->views_format = seq_load->views_format;
  }
  seq->flag |= seq_load->flag & SEQ_USE_VIEWS;

  seq->type = SEQ_TYPE_MOVIE;
  seq->blend_mode = SEQ_TYPE_ALPHAOVER;

  for (i = 0; i < totfiles; i++) {
    if (anim_arr[i]) {
      StripAnim *sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");
      BLI_addtail(&seq->anims, sanim);
      sanim->anim = anim_arr[i];
    }
    else {
      break;
    }
  }

  IMB_anim_load_metadata(anim_arr[0]);

  seq->anim_preseek = IMB_anim_get_preseek(anim_arr[0]);
  BLI_strncpy(seq->name + 2, "Movie", SEQ_NAME_MAXSTR - 2);
  BKE_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);

  /* adjust scene's frame rate settings to match */
  if (seq_load->flag & SEQ_LOAD_SYNC_FPS) {
    IMB_anim_get_fps(anim_arr[0], &scene->r.frs_sec, &scene->r.frs_sec_base, true);
  }

  /* basic defaults */
  seq->strip = strip = MEM_callocN(sizeof(Strip), "strip");
  seq->len = IMB_anim_get_duration(anim_arr[0], IMB_TC_RECORD_RUN);
  strip->us = 1;

  BLI_strncpy(seq->strip->colorspace_settings.name,
              colorspace,
              sizeof(seq->strip->colorspace_settings.name));

  /* we only need 1 element for MOVIE strips */
  strip->stripdata = se = MEM_callocN(sizeof(StripElem), "stripelem");

  BLI_split_dirfile(seq_load->path, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

  BKE_sequence_calc_disp(scene, seq);

  if (seq_load->name[0] == '\0') {
    BLI_strncpy(seq_load->name, se->name, sizeof(seq_load->name));
  }

  if (seq_load->flag & SEQ_LOAD_MOVIE_SOUND) {
    int start_frame_back = seq_load->start_frame;
    seq_load->channel++;

    seq_load->seq_sound = BKE_sequencer_add_sound_strip(C, seqbasep, seq_load);

    seq_load->start_frame = start_frame_back;
    seq_load->channel--;
  }

  /* can be NULL */
  seq_load_apply(CTX_data_main(C), scene, seq, seq_load);

  MEM_freeN(anim_arr);
  return seq;
}

static Sequence *seq_dupli(const Scene *scene_src,
                           Scene *scene_dst,
                           ListBase *new_seq_list,
                           Sequence *seq,
                           int dupe_flag,
                           const int flag)
{
  Sequence *seqn = MEM_dupallocN(seq);

  seq->tmp = seqn;
  seqn->strip = MEM_dupallocN(seq->strip);

  seqn->stereo3d_format = MEM_dupallocN(seq->stereo3d_format);

  /* XXX: add F-Curve duplication stuff? */

  if (seq->strip->crop) {
    seqn->strip->crop = MEM_dupallocN(seq->strip->crop);
  }

  if (seq->strip->transform) {
    seqn->strip->transform = MEM_dupallocN(seq->strip->transform);
  }

  if (seq->strip->proxy) {
    seqn->strip->proxy = MEM_dupallocN(seq->strip->proxy);
    seqn->strip->proxy->anim = NULL;
  }

  if (seq->prop) {
    seqn->prop = IDP_CopyProperty_ex(seq->prop, flag);
  }

  if (seqn->modifiers.first) {
    BLI_listbase_clear(&seqn->modifiers);

    BKE_sequence_modifier_list_copy(seqn, seq);
  }

  if (seq->type == SEQ_TYPE_META) {
    seqn->strip->stripdata = NULL;

    BLI_listbase_clear(&seqn->seqbase);
    /* WATCH OUT!!! - This metastrip is not recursively duplicated here - do this after!!! */
    /* - seq_dupli_recursive(&seq->seqbase, &seqn->seqbase);*/
  }
  else if (seq->type == SEQ_TYPE_SCENE) {
    seqn->strip->stripdata = NULL;
    if (seq->scene_sound) {
      seqn->scene_sound = BKE_sound_scene_add_scene_sound_defaults(scene_dst, seqn);
    }
  }
  else if (seq->type == SEQ_TYPE_MOVIECLIP) {
    /* avoid assert */
  }
  else if (seq->type == SEQ_TYPE_MASK) {
    /* avoid assert */
  }
  else if (seq->type == SEQ_TYPE_MOVIE) {
    seqn->strip->stripdata = MEM_dupallocN(seq->strip->stripdata);
    BLI_listbase_clear(&seqn->anims);
  }
  else if (seq->type == SEQ_TYPE_SOUND_RAM) {
    seqn->strip->stripdata = MEM_dupallocN(seq->strip->stripdata);
    if (seq->scene_sound) {
      seqn->scene_sound = BKE_sound_add_scene_sound_defaults(scene_dst, seqn);
    }

    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus((ID *)seqn->sound);
    }
  }
  else if (seq->type == SEQ_TYPE_IMAGE) {
    seqn->strip->stripdata = MEM_dupallocN(seq->strip->stripdata);
  }
  else if (seq->type & SEQ_TYPE_EFFECT) {
    struct SeqEffectHandle sh;
    sh = BKE_sequence_get_effect(seq);
    if (sh.copy) {
      sh.copy(seq, seqn, flag);
    }

    seqn->strip->stripdata = NULL;
  }
  else {
    /* sequence type not handled in duplicate! Expect a crash now... */
    BLI_assert(0);
  }

  /* When using SEQ_DUPE_UNIQUE_NAME, it is mandatory to add new sequences in relevant container
   * (scene or meta's one), *before* checking for unique names. Otherwise the meta's list is empty
   * and hence we miss all seqs in that meta that have already been duplicated (see T55668).
   * Note that unique name check itslef could be done at a later step in calling code, once all
   * seqs have bee duplicated (that was first, simpler solution), but then handling of animation
   * data will be broken (see T60194). */
  if (new_seq_list != NULL) {
    BLI_addtail(new_seq_list, seqn);
  }

  if (scene_src == scene_dst) {
    if (dupe_flag & SEQ_DUPE_UNIQUE_NAME) {
      BKE_sequence_base_unique_name_recursive(&scene_dst->ed->seqbase, seqn);
    }

    if (dupe_flag & SEQ_DUPE_ANIM) {
      BKE_sequencer_dupe_animdata(scene_dst, seq->name + 2, seqn->name + 2);
    }
  }

  return seqn;
}

static void seq_new_fix_links_recursive(Sequence *seq)
{
  SequenceModifierData *smd;

  if (seq->type & SEQ_TYPE_EFFECT) {
    if (seq->seq1 && seq->seq1->tmp) {
      seq->seq1 = seq->seq1->tmp;
    }
    if (seq->seq2 && seq->seq2->tmp) {
      seq->seq2 = seq->seq2->tmp;
    }
    if (seq->seq3 && seq->seq3->tmp) {
      seq->seq3 = seq->seq3->tmp;
    }
  }
  else if (seq->type == SEQ_TYPE_META) {
    Sequence *seqn;
    for (seqn = seq->seqbase.first; seqn; seqn = seqn->next) {
      seq_new_fix_links_recursive(seqn);
    }
  }

  for (smd = seq->modifiers.first; smd; smd = smd->next) {
    if (smd->mask_sequence && smd->mask_sequence->tmp) {
      smd->mask_sequence = smd->mask_sequence->tmp;
    }
  }
}

static Sequence *sequence_dupli_recursive_do(const Scene *scene_src,
                                             Scene *scene_dst,
                                             ListBase *new_seq_list,
                                             Sequence *seq,
                                             const int dupe_flag)
{
  Sequence *seqn;

  seq->tmp = NULL;
  seqn = seq_dupli(scene_src, scene_dst, new_seq_list, seq, dupe_flag, 0);
  if (seq->type == SEQ_TYPE_META) {
    Sequence *s;
    for (s = seq->seqbase.first; s; s = s->next) {
      sequence_dupli_recursive_do(scene_src, scene_dst, &seqn->seqbase, s, dupe_flag);
    }
  }

  return seqn;
}

Sequence *BKE_sequence_dupli_recursive(
    const Scene *scene_src, Scene *scene_dst, ListBase *new_seq_list, Sequence *seq, int dupe_flag)
{
  Sequence *seqn = sequence_dupli_recursive_do(scene_src, scene_dst, new_seq_list, seq, dupe_flag);

  /* This does not need to be in recursive call itself, since it is already recursive... */
  seq_new_fix_links_recursive(seqn);

  return seqn;
}

void BKE_sequence_base_dupli_recursive(const Scene *scene_src,
                                       Scene *scene_dst,
                                       ListBase *nseqbase,
                                       const ListBase *seqbase,
                                       int dupe_flag,
                                       const int flag)
{
  Sequence *seq;
  Sequence *seqn = NULL;
  Sequence *last_seq = BKE_sequencer_active_get((Scene *)scene_src);
  /* always include meta's strips */
  int dupe_flag_recursive = dupe_flag | SEQ_DUPE_ALL;

  for (seq = seqbase->first; seq; seq = seq->next) {
    seq->tmp = NULL;
    if ((seq->flag & SELECT) || (dupe_flag & SEQ_DUPE_ALL)) {
      seqn = seq_dupli(scene_src, scene_dst, nseqbase, seq, dupe_flag, flag);
      if (seqn) { /*should never fail */
        if (dupe_flag & SEQ_DUPE_CONTEXT) {
          seq->flag &= ~SEQ_ALLSEL;
          seqn->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL + SEQ_LOCK);
        }

        if (seq->type == SEQ_TYPE_META) {
          BKE_sequence_base_dupli_recursive(
              scene_src, scene_dst, &seqn->seqbase, &seq->seqbase, dupe_flag_recursive, flag);
        }

        if (dupe_flag & SEQ_DUPE_CONTEXT) {
          if (seq == last_seq) {
            BKE_sequencer_active_set(scene_dst, seqn);
          }
        }
      }
    }
  }

  /* fix modifier linking */
  for (seq = nseqbase->first; seq; seq = seq->next) {
    seq_new_fix_links_recursive(seq);
  }
}

/* called on draw, needs to be fast,
 * we could cache and use a flag if we want to make checks for file paths resolving for eg. */
bool BKE_sequence_is_valid_check(Sequence *seq)
{
  switch (seq->type) {
    case SEQ_TYPE_MASK:
      return (seq->mask != NULL);
    case SEQ_TYPE_MOVIECLIP:
      return (seq->clip != NULL);
    case SEQ_TYPE_SCENE:
      return (seq->scene != NULL);
    case SEQ_TYPE_SOUND_RAM:
      return (seq->sound != NULL);
  }

  return true;
}

int BKE_sequencer_find_next_prev_edit(Scene *scene,
                                      int cfra,
                                      const short side,
                                      const bool do_skip_mute,
                                      const bool do_center,
                                      const bool do_unselected)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;

  int dist, best_dist, best_frame = cfra;
  int seq_frames[2], seq_frames_tot;

  /* In case where both is passed,
   * frame just finds the nearest end while frame_left the nearest start. */

  best_dist = MAXFRAME * 2;

  if (ed == NULL) {
    return cfra;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    int i;

    if (do_skip_mute && (seq->flag & SEQ_MUTE)) {
      continue;
    }

    if (do_unselected && (seq->flag & SELECT)) {
      continue;
    }

    if (do_center) {
      seq_frames[0] = (seq->startdisp + seq->enddisp) / 2;
      seq_frames_tot = 1;
    }
    else {
      seq_frames[0] = seq->startdisp;
      seq_frames[1] = seq->enddisp;

      seq_frames_tot = 2;
    }

    for (i = 0; i < seq_frames_tot; i++) {
      const int seq_frame = seq_frames[i];

      dist = MAXFRAME * 2;

      switch (side) {
        case SEQ_SIDE_LEFT:
          if (seq_frame < cfra) {
            dist = cfra - seq_frame;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (seq_frame > cfra) {
            dist = seq_frame - cfra;
          }
          break;
        case SEQ_SIDE_BOTH:
          dist = abs(seq_frame - cfra);
          break;
      }

      if (dist < best_dist) {
        best_frame = seq_frame;
        best_dist = dist;
      }
    }
  }

  return best_frame;
}

static void sequencer_all_free_anim_ibufs(ListBase *seqbase, int cfra)
{
  for (Sequence *seq = seqbase->first; seq != NULL; seq = seq->next) {
    if (seq->enddisp < cfra || seq->startdisp > cfra) {
      BKE_sequence_free_anim(seq);
    }
    if (seq->type == SEQ_TYPE_META) {
      sequencer_all_free_anim_ibufs(&seq->seqbase, cfra);
    }
  }
}

void BKE_sequencer_all_free_anim_ibufs(Scene *scene, int cfra)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  if (ed == NULL) {
    return;
  }
  sequencer_all_free_anim_ibufs(&ed->seqbase, cfra);
  BKE_sequencer_cache_cleanup(scene);
}
