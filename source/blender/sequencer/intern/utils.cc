/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_blenlib.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "SEQ_animation.h"
#include "SEQ_channels.h"
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
#include "sequencer.h"
#include "utils.h"

struct SeqUniqueInfo {
  Sequence *seq;
  char name_src[SEQ_NAME_MAXSTR];
  char name_dest[SEQ_NAME_MAXSTR];
  int count;
  int match;
};

static void seqbase_unique_name(ListBase *seqbasep, SeqUniqueInfo *sui)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbasep) {
    if ((sui->seq != seq) && STREQ(sui->name_dest, seq->name + 2)) {
      /* SEQ_NAME_MAXSTR -4 for the number, -1 for \0, - 2 for r_prefix */
      SNPRINTF(
          sui->name_dest, "%.*s.%03d", SEQ_NAME_MAXSTR - 4 - 1 - 2, sui->name_src, sui->count++);
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

void SEQ_sequence_base_unique_name_recursive(Scene *scene, ListBase *seqbasep, Sequence *seq)
{
  SeqUniqueInfo sui;
  char *dot;
  sui.seq = seq;
  STRNCPY(sui.name_src, seq->name + 2);
  STRNCPY(sui.name_dest, seq->name + 2);

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
      return DATA_("Meta");
    case SEQ_TYPE_IMAGE:
      return DATA_("Image");
    case SEQ_TYPE_SCENE:
      return DATA_("Scene");
    case SEQ_TYPE_MOVIE:
      return DATA_("Movie");
    case SEQ_TYPE_MOVIECLIP:
      return DATA_("Clip");
    case SEQ_TYPE_MASK:
      return DATA_("Mask");
    case SEQ_TYPE_SOUND_RAM:
      return DATA_("Audio");
    case SEQ_TYPE_CROSS:
      return DATA_("Cross");
    case SEQ_TYPE_GAMCROSS:
      return DATA_("Gamma Cross");
    case SEQ_TYPE_ADD:
      return DATA_("Add");
    case SEQ_TYPE_SUB:
      return DATA_("Sub");
    case SEQ_TYPE_MUL:
      return DATA_("Mul");
    case SEQ_TYPE_ALPHAOVER:
      return DATA_("Alpha Over");
    case SEQ_TYPE_ALPHAUNDER:
      return DATA_("Alpha Under");
    case SEQ_TYPE_OVERDROP:
      return DATA_("Over Drop");
    case SEQ_TYPE_COLORMIX:
      return DATA_("Color Mix");
    case SEQ_TYPE_WIPE:
      return DATA_("Wipe");
    case SEQ_TYPE_GLOW:
      return DATA_("Glow");
    case SEQ_TYPE_TRANSFORM:
      return DATA_("Transform");
    case SEQ_TYPE_COLOR:
      return DATA_("Color");
    case SEQ_TYPE_MULTICAM:
      return DATA_("Multicam");
    case SEQ_TYPE_ADJUSTMENT:
      return DATA_("Adjustment");
    case SEQ_TYPE_SPEED:
      return DATA_("Speed");
    case SEQ_TYPE_GAUSSIAN_BLUR:
      return DATA_("Gaussian Blur");
    case SEQ_TYPE_TEXT:
      return DATA_("Text");
    default:
      return nullptr;
  }
}

const char *SEQ_sequence_give_name(Sequence *seq)
{
  const char *name = give_seqname_by_type(seq->type);

  if (!name) {
    if (!(seq->type & SEQ_TYPE_EFFECT)) {
      return seq->strip->dirpath;
    }

    return DATA_("Effect");
  }
  return name;
}

ListBase *SEQ_get_seqbase_from_sequence(Sequence *seq, ListBase **r_channels, int *r_offset)
{
  ListBase *seqbase = nullptr;

  switch (seq->type) {
    case SEQ_TYPE_META: {
      seqbase = &seq->seqbase;
      *r_channels = &seq->channels;
      *r_offset = SEQ_time_start_frame_get(seq);
      break;
    }
    case SEQ_TYPE_SCENE: {
      if (seq->flag & SEQ_SCENE_STRIPS && seq->scene) {
        Editing *ed = SEQ_editing_get(seq->scene);
        if (ed) {
          seqbase = &ed->seqbase;
          *r_channels = &ed->channels;
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
  char dirpath[FILE_MAX];
  char filepath[FILE_MAX];
  StripProxy *proxy;
  bool use_proxy;
  bool is_multiview_loaded = false;
  Editing *ed = scene->ed;
  const bool is_multiview = (seq->flag & SEQ_USE_VIEWS) != 0 &&
                            (scene->r.scemode & R_MULTIVIEW) != 0;

  if ((seq->anims.first != nullptr) && (((StripAnim *)seq->anims.first)->anim != nullptr) &&
      !openfile)
  {
    return;
  }

  /* reset all the previously created anims */
  SEQ_relations_sequence_free_anim(seq);

  BLI_path_join(filepath, sizeof(filepath), seq->strip->dirpath, seq->strip->stripdata->filename);
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&scene->id));

  proxy = seq->strip->proxy;

  use_proxy = proxy && ((proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_DIR) != 0 ||
                        (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE));

  if (use_proxy) {
    if (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE) {
      if (ed->proxy_dir[0] == 0) {
        STRNCPY(dirpath, "//BL_proxy");
      }
      else {
        STRNCPY(dirpath, ed->proxy_dir);
      }
    }
    else {
      STRNCPY(dirpath, seq->strip->proxy->dirpath);
    }
    BLI_path_abs(dirpath, BKE_main_blendfile_path_from_global());
  }

  if (is_multiview && seq->views_format == R_IMF_VIEWS_INDIVIDUAL) {
    int totfiles = seq_num_files(scene, seq->views_format, true);
    char prefix[FILE_MAX];
    const char *ext = nullptr;
    int i;

    BKE_scene_multiview_view_prefix_get(scene, filepath, prefix, &ext);

    if (prefix[0] != '\0') {
      for (i = 0; i < totfiles; i++) {
        const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, i);
        char filepath_view[FILE_MAX];
        StripAnim *sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));

        BLI_addtail(&seq->anims, sanim);

        SNPRINTF(filepath_view, "%s%s%s", prefix, suffix, ext);

        if (openfile) {
          sanim->anim = openanim(filepath_view,
                                 IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                 seq->streamindex,
                                 seq->strip->colorspace_settings.name);
        }
        else {
          sanim->anim = openanim_noload(filepath_view,
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
            sanim->anim = openanim(filepath,
                                   IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                   seq->streamindex,
                                   seq->strip->colorspace_settings.name);
          }
          else {
            sanim->anim = openanim_noload(filepath,
                                          IB_rect |
                                              ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                          seq->streamindex,
                                          seq->strip->colorspace_settings.name);
          }

          /* No individual view files - monoscopic, stereo 3d or EXR multi-view. */
          totfiles = 1;
        }

        if (sanim->anim && use_proxy) {
          seq_proxy_index_dir_set(sanim->anim, dirpath);
        }
      }
      is_multiview_loaded = true;
    }
  }

  if (is_multiview_loaded == false) {
    StripAnim *sanim;

    sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
    BLI_addtail(&seq->anims, sanim);

    if (openfile) {
      sanim->anim = openanim(filepath,
                             IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                             seq->streamindex,
                             seq->strip->colorspace_settings.name);
    }
    else {
      sanim->anim = openanim_noload(filepath,
                                    IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                    seq->streamindex,
                                    seq->strip->colorspace_settings.name);
    }

    if (sanim->anim && use_proxy) {
      seq_proxy_index_dir_set(sanim->anim, dirpath);
    }
  }
}

const Sequence *SEQ_get_topmost_sequence(const Scene *scene, int frame)
{
  Editing *ed = scene->ed;

  if (!ed) {
    return nullptr;
  }

  ListBase *channels = SEQ_channels_displayed_get(ed);
  const Sequence *best_seq = nullptr;
  int best_machine = -1;

  LISTBASE_FOREACH (const Sequence *, seq, ed->seqbasep) {
    if (SEQ_render_is_muted(channels, seq) || !SEQ_time_strip_intersects_frame(scene, seq, frame))
    {
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
             SEQ_TYPE_TEXT))
    {
      if (seq->machine > best_machine) {
        best_seq = seq;
        best_machine = seq->machine;
      }
    }
  }
  return best_seq;
}

ListBase *SEQ_get_seqbase_by_seq(const Scene *scene, Sequence *seq)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *main_seqbase = &ed->seqbase;
  Sequence *seq_meta = seq_sequence_lookup_meta_by_seq(scene, seq);

  if (seq_meta != nullptr) {
    return &seq_meta->seqbase;
  }
  if (BLI_findindex(main_seqbase, seq) != -1) {
    return main_seqbase;
  }
  return nullptr;
}

Sequence *SEQ_get_meta_by_seqbase(ListBase *seqbase_main, ListBase *meta_seqbase)
{
  SeqCollection *strips = SEQ_query_all_strips_recursive(seqbase_main);

  Sequence *seq = nullptr;
  SEQ_ITERATOR_FOREACH (seq, strips) {
    if (seq->type == SEQ_TYPE_META && &seq->seqbase == meta_seqbase) {
      break;
    }
  }

  SEQ_collection_free(strips);
  return seq;
}

Sequence *SEQ_sequence_from_strip_elem(ListBase *seqbase, StripElem *se)
{
  Sequence *iseq;

  for (iseq = static_cast<Sequence *>(seqbase->first); iseq; iseq = iseq->next) {
    Sequence *seq_found;
    if ((iseq->strip && iseq->strip->stripdata) &&
        ARRAY_HAS_ITEM(se, iseq->strip->stripdata, iseq->len))
    {
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
  LISTBASE_FOREACH (Sequence *, iseq, seqbase) {
    if (STREQ(name, iseq->name + 2)) {
      return iseq;
    }
    if (recursive && !BLI_listbase_is_empty(&iseq->seqbase)) {
      Sequence *rseq = SEQ_get_sequence_by_name(&iseq->seqbase, name, true);
      if (rseq != nullptr) {
        return rseq;
      }
    }
  }

  return nullptr;
}

Mask *SEQ_active_mask_get(Scene *scene)
{
  Sequence *seq_act = SEQ_select_active_get(scene);

  if (seq_act && seq_act->type == SEQ_TYPE_MASK) {
    return seq_act->mask;
  }

  return nullptr;
}

void SEQ_alpha_mode_from_file_extension(Sequence *seq)
{
  if (seq->strip && seq->strip->stripdata) {
    const char *filename = seq->strip->stripdata->filename;
    seq->alpha_mode = BKE_image_alpha_mode_from_extension_ex(filename);
  }
}

bool SEQ_sequence_has_source(const Sequence *seq)
{
  /* Called on draw, needs to be fast,
   * we could cache and use a flag if we want to make checks for file paths resolving for eg. */
  switch (seq->type) {
    case SEQ_TYPE_MASK:
      return (seq->mask != nullptr);
    case SEQ_TYPE_MOVIECLIP:
      return (seq->clip != nullptr);
    case SEQ_TYPE_SCENE:
      return (seq->scene != nullptr);
    case SEQ_TYPE_SOUND_RAM:
      return (seq->sound != nullptr);
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
      transform->scale_x = transform->scale_y = MIN2(float(preview_width) / float(image_width),
                                                     float(preview_height) / float(image_height));

      break;
    case SEQ_SCALE_TO_FILL:

      transform->scale_x = transform->scale_y = MAX2(float(preview_width) / float(image_width),
                                                     float(preview_height) / float(image_height));
      break;
    case SEQ_STRETCH_TO_FILL:
      transform->scale_x = float(preview_width) / float(image_width);
      transform->scale_y = float(preview_height) / float(image_height);
      break;
    case SEQ_USE_ORIGINAL_SIZE:
      transform->scale_x = 1.0f;
      transform->scale_y = 1.0f;
      break;
  }
}

void SEQ_ensure_unique_name(Sequence *seq, Scene *scene)
{
  char name[SEQ_NAME_MAXSTR];

  STRNCPY_UTF8(name, seq->name + 2);
  SEQ_sequence_base_unique_name_recursive(scene, &scene->ed->seqbase, seq);
  BKE_animdata_fix_paths_rename(&scene->id,
                                scene->adt,
                                nullptr,
                                "sequence_editor.sequences_all",
                                name,
                                seq->name + 2,
                                0,
                                0,
                                false);

  if (seq->type == SEQ_TYPE_META) {
    LISTBASE_FOREACH (Sequence *, seq_child, &seq->seqbase) {
      SEQ_ensure_unique_name(seq_child, scene);
    }
  }
}
