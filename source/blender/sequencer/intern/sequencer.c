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

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_animsys.h"
#include "BKE_fcurve.h"
#include "BKE_idprop.h"
#include "BKE_lib_id.h"
#include "BKE_sound.h"

#include "DEG_depsgraph.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_modifier.h"
#include "SEQ_relations.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_utils.h"

#include "image_cache.h"
#include "prefetch.h"
#include "sequencer.h"
#include "utils.h"

static void seq_free_animdata(Scene *scene, Sequence *seq);

/* -------------------------------------------------------------------- */
/** \name Allocate / Free Functions
 * \{ */

static Strip *seq_strip_alloc(int type)
{
  Strip *strip = MEM_callocN(sizeof(Strip), "strip");

  if (ELEM(type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD) == 0) {
    strip->transform = MEM_callocN(sizeof(struct StripTransform), "StripTransform");
    strip->transform->scale_x = 1;
    strip->transform->scale_y = 1;
    strip->crop = MEM_callocN(sizeof(struct StripCrop), "StripCrop");
  }

  strip->us = 1;
  return strip;
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

Sequence *SEQ_sequence_alloc(ListBase *lb, int timeline_frame, int machine, int type)
{
  Sequence *seq;

  seq = MEM_callocN(sizeof(Sequence), "addseq");
  BLI_addtail(lb, seq);

  *((short *)seq->name) = ID_SEQ;
  seq->name[2] = 0;

  seq->flag = SELECT;
  seq->start = timeline_frame;
  seq->machine = machine;
  seq->sat = 1.0;
  seq->mul = 1.0;
  seq->blend_opacity = 100.0;
  seq->volume = 1.0f;
  seq->pitch = 1.0f;
  seq->scene_sound = NULL;
  seq->type = type;

  seq->strip = seq_strip_alloc(type);
  seq->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Sequence Stereo Format");
  seq->cache_flag = SEQ_CACHE_STORE_RAW | SEQ_CACHE_STORE_PREPROCESSED | SEQ_CACHE_STORE_COMPOSITE;

  SEQ_relations_session_uuid_generate(seq);

  return seq;
}

/* only give option to skip cache locally (static func) */
static void seq_sequence_free_ex(Scene *scene,
                                 Sequence *seq,
                                 const bool do_cache,
                                 const bool do_id_user,
                                 const bool do_clean_animdata)
{
  if (seq->strip) {
    seq_free_strip(seq->strip);
  }

  SEQ_relations_sequence_free_anim(seq);

  if (seq->type & SEQ_TYPE_EFFECT) {
    struct SeqEffectHandle sh = SEQ_effect_handle_get(seq);
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

    /* XXX This must not be done in BKE code. */
    if (do_clean_animdata) {
      seq_free_animdata(scene, seq);
    }
  }

  if (seq->prop) {
    IDP_FreePropertyContent_ex(seq->prop, do_id_user);
    MEM_freeN(seq->prop);
  }

  /* free modifiers */
  SEQ_modifier_clear(seq);

  /* free cached data used by this strip,
   * also invalidate cache for all dependent sequences
   *
   * be _very_ careful here, invalidating cache loops over the scene sequences and
   * assumes the listbase is valid for all strips,
   * this may not be the case if lists are being freed.
   * this is optional SEQ_relations_invalidate_cache
   */
  if (do_cache) {
    if (scene) {
      SEQ_relations_invalidate_cache_raw(scene, seq);
    }
  }

  MEM_freeN(seq);
}

void SEQ_sequence_free(Scene *scene, Sequence *seq, const bool do_clean_animdata)
{
  seq_sequence_free_ex(scene, seq, true, true, do_clean_animdata);
}

/* cache must be freed before calling this function
 * since it leaves the seqbase in an invalid state */
void seq_free_sequence_recurse(Scene *scene, Sequence *seq, const bool do_id_user)
{
  Sequence *iseq, *iseq_next;

  for (iseq = seq->seqbase.first; iseq; iseq = iseq_next) {
    iseq_next = iseq->next;
    seq_free_sequence_recurse(scene, iseq, do_id_user);
  }

  seq_sequence_free_ex(scene, seq, false, do_id_user, true);
}

Editing *SEQ_editing_get(Scene *scene, bool alloc)
{
  if (alloc) {
    SEQ_editing_ensure(scene);
  }
  return scene->ed;
}

Editing *SEQ_editing_ensure(Scene *scene)
{
  if (scene->ed == NULL) {
    Editing *ed;

    ed = scene->ed = MEM_callocN(sizeof(Editing), "addseq");
    ed->seqbasep = &ed->seqbase;
    ed->cache = NULL;
    ed->cache_flag = SEQ_CACHE_STORE_FINAL_OUT;
    ed->cache_flag |= SEQ_CACHE_VIEW_FINAL_OUT;
    ed->cache_flag |= SEQ_CACHE_VIEW_ENABLE;
  }

  return scene->ed;
}

void SEQ_editing_free(Scene *scene, const bool do_id_user)
{
  Editing *ed = scene->ed;
  Sequence *seq;

  if (ed == NULL) {
    return;
  }

  seq_prefetch_free(scene);
  seq_cache_destruct(scene);

  SEQ_ALL_BEGIN (ed, seq) {
    /* handle cache freeing above */
    seq_sequence_free_ex(scene, seq, false, do_id_user, false);
  }
  SEQ_ALL_END;

  BLI_freelistN(&ed->metastack);
  MEM_freeN(ed);

  scene->ed = NULL;
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

SequencerToolSettings *SEQ_tool_settings_init(void)
{
  SequencerToolSettings *tool_settings = MEM_callocN(sizeof(SequencerToolSettings),
                                                     "Sequencer tool settings");
  tool_settings->fit_method = SEQ_SCALE_TO_FIT;
  return tool_settings;
}

void SEQ_tool_settings_free(SequencerToolSettings *tool_settings)
{
  MEM_freeN(tool_settings);
}

eSeqImageFitMethod SEQ_tool_settings_fit_method_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = scene->toolsettings->sequencer_tool_settings;
  return tool_settings->fit_method;
}

void SEQ_tool_settings_fit_method_set(Scene *scene, eSeqImageFitMethod fit_method)
{
  SequencerToolSettings *tool_settings = scene->toolsettings->sequencer_tool_settings;
  tool_settings->fit_method = fit_method;
}

/**
 * Get seqbase that is being viewed currently. This can be main seqbase or meta strip seqbase
 *
 * \param ed: sequence editor data
 * \return pointer to active seqbase. returns NULL if ed is NULL
 */
ListBase *SEQ_active_seqbase_get(const Editing *ed)
{
  if (ed == NULL) {
    return NULL;
  }

  return ed->seqbasep;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Functions
 * \{ */
static Sequence *seq_dupli(const Scene *scene_src,
                           Scene *scene_dst,
                           ListBase *new_seq_list,
                           Sequence *seq,
                           int dupe_flag,
                           const int flag)
{
  Sequence *seqn = MEM_dupallocN(seq);

  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    SEQ_relations_session_uuid_generate(seqn);
  }

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

    SEQ_modifier_list_copy(seqn, seq);
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
    seqn->scene_sound = NULL;
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus((ID *)seqn->sound);
    }
  }
  else if (seq->type == SEQ_TYPE_IMAGE) {
    seqn->strip->stripdata = MEM_dupallocN(seq->strip->stripdata);
  }
  else if (seq->type & SEQ_TYPE_EFFECT) {
    struct SeqEffectHandle sh;
    sh = SEQ_effect_handle_get(seq);
    if (sh.copy) {
      sh.copy(seqn, seq, flag);
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
   * Note that unique name check itself could be done at a later step in calling code, once all
   * seqs have bee duplicated (that was first, simpler solution), but then handling of animation
   * data will be broken (see T60194). */
  if (new_seq_list != NULL) {
    BLI_addtail(new_seq_list, seqn);
  }

  if (scene_src == scene_dst) {
    if (dupe_flag & SEQ_DUPE_UNIQUE_NAME) {
      SEQ_sequence_base_unique_name_recursive(&scene_dst->ed->seqbase, seqn);
    }

    if (dupe_flag & SEQ_DUPE_ANIM) {
      SEQ_dupe_animdata(scene_dst, seq->name + 2, seqn->name + 2);
    }
  }

  return seqn;
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

Sequence *SEQ_sequence_dupli_recursive(
    const Scene *scene_src, Scene *scene_dst, ListBase *new_seq_list, Sequence *seq, int dupe_flag)
{
  Sequence *seqn = sequence_dupli_recursive_do(scene_src, scene_dst, new_seq_list, seq, dupe_flag);

  /* This does not need to be in recursive call itself, since it is already recursive... */
  seq_new_fix_links_recursive(seqn);

  return seqn;
}

void SEQ_sequence_base_dupli_recursive(const Scene *scene_src,
                                       Scene *scene_dst,
                                       ListBase *nseqbase,
                                       const ListBase *seqbase,
                                       int dupe_flag,
                                       const int flag)
{
  Sequence *seq;
  Sequence *seqn = NULL;
  Sequence *last_seq = SEQ_select_active_get((Scene *)scene_src);
  /* always include meta's strips */
  int dupe_flag_recursive = dupe_flag | SEQ_DUPE_ALL | SEQ_DUPE_IS_RECURSIVE_CALL;

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
          SEQ_sequence_base_dupli_recursive(
              scene_src, scene_dst, &seqn->seqbase, &seq->seqbase, dupe_flag_recursive, flag);
        }

        if (dupe_flag & SEQ_DUPE_CONTEXT) {
          if (seq == last_seq) {
            SEQ_select_active_set(scene_dst, seqn);
          }
        }
      }
    }
  }

  /* Fix modifier links recursively from the top level only, when all sequences have been
   * copied. */
  if (dupe_flag & SEQ_DUPE_IS_RECURSIVE_CALL) {
    return;
  }

  /* fix modifier linking */
  for (seq = nseqbase->first; seq; seq = seq->next) {
    seq_new_fix_links_recursive(seq);
  }
}
/* r_prefix + [" + escaped_name + "] + \0 */
#define SEQ_RNAPATH_MAXSTR ((30 + 2 + (SEQ_NAME_MAXSTR * 2) + 2) + 1)

static size_t sequencer_rna_path_prefix(char str[SEQ_RNAPATH_MAXSTR], const char *name)
{
  char name_esc[SEQ_NAME_MAXSTR * 2];

  BLI_str_escape(name_esc, name, sizeof(name_esc));
  return BLI_snprintf_rlen(
      str, SEQ_RNAPATH_MAXSTR, "sequence_editor.sequences_all[\"%s\"]", name_esc);
}

/* XXX - hackish function needed for transforming strips! TODO - have some better solution */
void SEQ_offset_animdata(Scene *scene, Sequence *seq, int ofs)
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

  DEG_id_tag_update(&scene->adt->action->id, ID_RECALC_ANIMATION);
}

void SEQ_dupe_animdata(Scene *scene, const char *name_src, const char *name_dst)
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
      fcu_cpy = BKE_fcurve_copy(fcu);
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
      BKE_fcurve_free(fcu);

      fcu = next_fcu;
    }
    else {
      fcu = fcu->next;
    }
  }
}

#undef SEQ_RNAPATH_MAXSTR

SequencerToolSettings *SEQ_tool_settings_copy(SequencerToolSettings *tool_settings)
{
  SequencerToolSettings *tool_settings_copy = MEM_dupallocN(tool_settings);
  return tool_settings_copy;
}

/** \} */
