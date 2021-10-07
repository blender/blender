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

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_blenlib.h"

#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "SEQ_edit.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_utils.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "multiview.h"
#include "proxy.h"
#include "utils.h"

/**
 * Sort strips in provided seqbase. Effect strips are trailing the list and they are sorted by
 * channel position as well.
 * This is important for SEQ_time_update_sequence to work properly
 *
 * \param seqbase: ListBase with strips
 */
void SEQ_sort(ListBase *seqbase)
{
  if (seqbase == NULL) {
    return;
  }

  /* all strips together per kind, and in order of y location ("machine") */
  ListBase inputbase, effbase;
  Sequence *seq, *seqt;

  BLI_listbase_clear(&inputbase);
  BLI_listbase_clear(&effbase);

  while ((seq = BLI_pophead(seqbase))) {

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
      seqt = inputbase.first;
      while (seqt) {
        if (seqt->machine >= seq->machine) {
          BLI_insertlinkbefore(&inputbase, seqt, seq);
          break;
        }
        seqt = seqt->next;
      }
      if (seqt == NULL) {
        BLI_addtail(&inputbase, seq);
      }
    }
  }

  BLI_movelisttolist(seqbase, &inputbase);
  BLI_movelisttolist(seqbase, &effbase);
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
      /* SEQ_NAME_MAXSTR -4 for the number, -1 for \0, - 2 for r_prefix */
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

static bool seqbase_unique_name_recursive_fn(Sequence *seq, void *arg_pt)
{
  if (seq->seqbase.first) {
    seqbase_unique_name(&seq->seqbase, (SeqUniqueInfo *)arg_pt);
  }
  return true;
}

void SEQ_sequence_base_unique_name_recursive(struct Scene *scene,
                                             ListBase *seqbasep,
                                             Sequence *seq)
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
    SEQ_for_each_callback(seqbasep, seqbase_unique_name_recursive_fn, &sui);
  }

  SEQ_edit_sequence_name_set(scene, seq, sui.name_dest);
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

const char *SEQ_sequence_give_name(Sequence *seq)
{
  const char *name = give_seqname_by_type(seq->type);

  if (!name) {
    if (!(seq->type & SEQ_TYPE_EFFECT)) {
      return seq->strip->dir;
    }

    return "Effect";
  }
  return name;
}

ListBase *SEQ_get_seqbase_from_sequence(Sequence *seq, int *r_offset)
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
        Editing *ed = SEQ_editing_get(seq->scene);
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

void seq_open_anim_file(Scene *scene, Sequence *seq, bool openfile)
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
  SEQ_relations_sequence_free_anim(seq);

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

          /* No individual view files - monoscopic, stereo 3d or EXR multi-view. */
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

const Sequence *SEQ_get_topmost_sequence(const Scene *scene, int frame)
{
  const Editing *ed = scene->ed;
  const Sequence *seq, *best_seq = NULL;
  int best_machine = -1;

  if (!ed) {
    return NULL;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SEQ_MUTE || !SEQ_time_strip_intersects_frame(seq, frame)) {
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

/* in cases where we don't know the sequence's listbase */
ListBase *SEQ_get_seqbase_by_seq(ListBase *seqbase, Sequence *seq)
{
  Sequence *iseq;
  ListBase *lb = NULL;

  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    if (seq == iseq) {
      return seqbase;
    }
    if (iseq->seqbase.first && (lb = SEQ_get_seqbase_by_seq(&iseq->seqbase, seq))) {
      return lb;
    }
  }

  return NULL;
}

Sequence *SEQ_get_meta_by_seqbase(ListBase *seqbase_main, ListBase *meta_seqbase)
{
  SeqCollection *strips = SEQ_query_all_strips_recursive(seqbase_main);

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, strips) {
    if (seq->type == SEQ_TYPE_META && &seq->seqbase == meta_seqbase) {
      break;
    }
  }

  SEQ_collection_free(strips);
  return seq;
}

/**
 * Only use as last resort when the StripElem is available but no the Sequence.
 * (needed for RNA)
 */
Sequence *SEQ_sequence_from_strip_elem(ListBase *seqbase, StripElem *se)
{
  Sequence *iseq;

  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    Sequence *seq_found;
    if ((iseq->strip && iseq->strip->stripdata) &&
        (ARRAY_HAS_ITEM(se, iseq->strip->stripdata, iseq->len))) {
      break;
    }
    if ((seq_found = SEQ_sequence_from_strip_elem(&iseq->seqbase, se))) {
      iseq = seq_found;
      break;
    }
  }

  return iseq;
}

Sequence *SEQ_get_sequence_by_name(ListBase *seqbase, const char *name, bool recursive)
{
  Sequence *iseq = NULL;
  Sequence *rseq = NULL;

  for (iseq = seqbase->first; iseq; iseq = iseq->next) {
    if (STREQ(name, iseq->name + 2)) {
      return iseq;
    }
    if (recursive && (iseq->seqbase.first) &&
        (rseq = SEQ_get_sequence_by_name(&iseq->seqbase, name, 1))) {
      return rseq;
    }
  }

  return NULL;
}

Mask *SEQ_active_mask_get(Scene *scene)
{
  Sequence *seq_act = SEQ_select_active_get(scene);

  if (seq_act && seq_act->type == SEQ_TYPE_MASK) {
    return seq_act->mask;
  }

  return NULL;
}

void SEQ_alpha_mode_from_file_extension(Sequence *seq)
{
  if (seq->strip && seq->strip->stripdata) {
    const char *filename = seq->strip->stripdata->name;
    seq->alpha_mode = BKE_image_alpha_mode_from_extension_ex(filename);
  }
}

/* called on draw, needs to be fast,
 * we could cache and use a flag if we want to make checks for file paths resolving for eg. */
bool SEQ_sequence_has_source(const Sequence *seq)
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

bool sequencer_seq_generates_image(Sequence *seq)
{
  switch (seq->type) {
    case SEQ_TYPE_IMAGE:
    case SEQ_TYPE_SCENE:
    case SEQ_TYPE_MOVIE:
    case SEQ_TYPE_MOVIECLIP:
    case SEQ_TYPE_MASK:
    case SEQ_TYPE_COLOR:
    case SEQ_TYPE_TEXT:
      return true;
  }
  return false;
}

void SEQ_set_scale_to_fit(const Sequence *seq,
                          const int image_width,
                          const int image_height,
                          const int preview_width,
                          const int preview_height,
                          const eSeqImageFitMethod fit_method)
{
  StripTransform *transform = seq->strip->transform;

  switch (fit_method) {
    case SEQ_SCALE_TO_FIT:
      transform->scale_x = transform->scale_y = MIN2((float)preview_width / (float)image_width,
                                                     (float)preview_height / (float)image_height);

      break;
    case SEQ_SCALE_TO_FILL:

      transform->scale_x = transform->scale_y = MAX2((float)preview_width / (float)image_width,
                                                     (float)preview_height / (float)image_height);
      break;
    case SEQ_STRETCH_TO_FILL:
      transform->scale_x = (float)preview_width / (float)image_width;
      transform->scale_y = (float)preview_height / (float)image_height;
      break;
    case SEQ_USE_ORIGINAL_SIZE:
      transform->scale_x = 1.0f;
      transform->scale_y = 1.0f;
      break;
  }
}

/**
 * Ensure, that provided Sequence has unique name. If animation data exists for this Sequence, it
 * will be duplicated and mapped onto new name
 *
 * \param seq: Sequence which name will be ensured to be unique
 * \param scene: Scene in which name must be unique
 */
void SEQ_ensure_unique_name(Sequence *seq, Scene *scene)
{
  char name[SEQ_NAME_MAXSTR];

  BLI_strncpy_utf8(name, seq->name + 2, sizeof(name));
  SEQ_sequence_base_unique_name_recursive(scene, &scene->ed->seqbase, seq);
  SEQ_dupe_animdata(scene, name, seq->name + 2);

  if (seq->type == SEQ_TYPE_META) {
    LISTBASE_FOREACH (Sequence *, seq_child, &seq->seqbase) {
      SEQ_ensure_unique_name(seq_child, scene);
    }
  }
}
