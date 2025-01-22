/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_animsys.h"
#include "BKE_image.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "SEQ_channels.hh"
#include "SEQ_edit.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_utils.hh"

#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"

#include "multiview.hh"
#include "proxy.hh"
#include "sequencer.hh"
#include "utils.hh"

struct SeqUniqueInfo {
  Strip *strip;
  char name_src[STRIP_NAME_MAXSTR];
  char name_dest[STRIP_NAME_MAXSTR];
  int count;
  int match;
};

static void seqbase_unique_name(ListBase *seqbasep, SeqUniqueInfo *sui)
{
  LISTBASE_FOREACH (Strip *, strip, seqbasep) {
    if ((sui->strip != strip) && STREQ(sui->name_dest, strip->name + 2)) {
      /* STRIP_NAME_MAXSTR -4 for the number, -1 for \0, - 2 for r_prefix */
      SNPRINTF(
          sui->name_dest, "%.*s.%03d", STRIP_NAME_MAXSTR - 4 - 1 - 2, sui->name_src, sui->count++);
      sui->match = 1; /* be sure to re-scan */
    }
  }
}

static bool seqbase_unique_name_recursive_fn(Strip *strip, void *arg_pt)
{
  if (strip->seqbase.first) {
    seqbase_unique_name(&strip->seqbase, (SeqUniqueInfo *)arg_pt);
  }
  return true;
}

void SEQ_sequence_base_unique_name_recursive(Scene *scene, ListBase *seqbasep, Strip *strip)
{
  SeqUniqueInfo sui;
  char *dot;
  sui.strip = strip;
  STRNCPY(sui.name_src, strip->name + 2);
  STRNCPY(sui.name_dest, strip->name + 2);

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

  SEQ_edit_sequence_name_set(scene, strip, sui.name_dest);
}

static const char *give_seqname_by_type(int type)
{
  switch (type) {
    case STRIP_TYPE_META:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Meta");
    case STRIP_TYPE_IMAGE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Image");
    case STRIP_TYPE_SCENE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Scene");
    case STRIP_TYPE_MOVIE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Movie");
    case STRIP_TYPE_MOVIECLIP:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Clip");
    case STRIP_TYPE_MASK:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Mask");
    case STRIP_TYPE_SOUND_RAM:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Audio");
    case STRIP_TYPE_CROSS:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Cross");
    case STRIP_TYPE_GAMCROSS:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Gamma Cross");
    case STRIP_TYPE_ADD:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Add");
    case STRIP_TYPE_SUB:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Subtract");
    case STRIP_TYPE_MUL:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Multiply");
    case STRIP_TYPE_ALPHAOVER:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Alpha Over");
    case STRIP_TYPE_ALPHAUNDER:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Alpha Under");
    case STRIP_TYPE_OVERDROP:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Over Drop");
    case STRIP_TYPE_COLORMIX:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Color Mix");
    case STRIP_TYPE_WIPE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Wipe");
    case STRIP_TYPE_GLOW:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Glow");
    case STRIP_TYPE_TRANSFORM:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Transform");
    case STRIP_TYPE_COLOR:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Color");
    case STRIP_TYPE_MULTICAM:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Multicam");
    case STRIP_TYPE_ADJUSTMENT:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Adjustment");
    case STRIP_TYPE_SPEED:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Speed");
    case STRIP_TYPE_GAUSSIAN_BLUR:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Gaussian Blur");
    case STRIP_TYPE_TEXT:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Text");
    default:
      return nullptr;
  }
}

const char *SEQ_sequence_give_name(const Strip *strip)
{
  const char *name = give_seqname_by_type(strip->type);

  if (!name) {
    if (!(strip->type & STRIP_TYPE_EFFECT)) {
      return strip->data->dirpath;
    }

    return DATA_("Effect");
  }
  return name;
}

ListBase *SEQ_get_seqbase_from_sequence(Strip *strip, ListBase **r_channels, int *r_offset)
{
  ListBase *seqbase = nullptr;

  switch (strip->type) {
    case STRIP_TYPE_META: {
      seqbase = &strip->seqbase;
      *r_channels = &strip->channels;
      *r_offset = SEQ_time_start_frame_get(strip);
      break;
    }
    case STRIP_TYPE_SCENE: {
      if (strip->flag & SEQ_SCENE_STRIPS && strip->scene) {
        Editing *ed = SEQ_editing_get(strip->scene);
        if (ed) {
          seqbase = &ed->seqbase;
          *r_channels = &ed->channels;
          *r_offset = strip->scene->r.sfra;
        }
      }
      break;
    }
  }

  return seqbase;
}

static void open_anim_filepath(Strip *strip, StripAnim *sanim, const char *filepath, bool openfile)
{
  if (openfile) {
    sanim->anim = openanim(filepath,
                           IB_rect | ((strip->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                           strip->streamindex,
                           strip->data->colorspace_settings.name);
  }
  else {
    sanim->anim = openanim_noload(filepath,
                                  IB_rect | ((strip->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                                  strip->streamindex,
                                  strip->data->colorspace_settings.name);
  }
}

static bool use_proxy(Editing *ed, Strip *strip)
{
  StripProxy *proxy = strip->data->proxy;
  return proxy && ((proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_DIR) != 0 ||
                   (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE));
}

static void proxy_dir_get(Editing *ed, Strip *strip, size_t str_len, char *r_proxy_dirpath)
{
  if (use_proxy(ed, strip)) {
    if (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE) {
      if (ed->proxy_dir[0] == 0) {
        BLI_strncpy(r_proxy_dirpath, "//BL_proxy", str_len);
      }
      else {
        BLI_strncpy(r_proxy_dirpath, ed->proxy_dir, str_len);
      }
    }
    else {
      BLI_strncpy(r_proxy_dirpath, strip->data->proxy->dirpath, str_len);
    }
    BLI_path_abs(r_proxy_dirpath, BKE_main_blendfile_path_from_global());
  }
}

static void index_dir_set(Editing *ed, Strip *strip, StripAnim *sanim)
{
  if (sanim->anim == nullptr || !use_proxy(ed, strip)) {
    return;
  }

  char proxy_dirpath[FILE_MAX];
  proxy_dir_get(ed, strip, sizeof(proxy_dirpath), proxy_dirpath);
  seq_proxy_index_dir_set(sanim->anim, proxy_dirpath);
}

static bool open_anim_file_multiview(Scene *scene, Strip *strip, const char *filepath)
{
  char prefix[FILE_MAX];
  const char *ext = nullptr;
  BKE_scene_multiview_view_prefix_get(scene, filepath, prefix, &ext);

  if (strip->views_format != R_IMF_VIEWS_INDIVIDUAL || prefix[0] == '\0') {
    return false;
  }

  Editing *ed = scene->ed;
  bool is_multiview_loaded = false;
  int totfiles = seq_num_files(scene, strip->views_format, true);

  for (int i = 0; i < totfiles; i++) {
    const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, i);
    char filepath_view[FILE_MAX];
    SNPRINTF(filepath_view, "%s%s%s", prefix, suffix, ext);

    StripAnim *sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
    /* Multiview files must be loaded, otherwise it is not possible to detect failure. */
    open_anim_filepath(strip, sanim, filepath_view, true);

    if (sanim->anim == nullptr) {
      SEQ_relations_sequence_free_anim(strip);
      return false; /* Multiview render failed. */
    }

    index_dir_set(ed, strip, sanim);
    BLI_addtail(&strip->anims, sanim);
    MOV_set_multiview_suffix(sanim->anim, suffix);
    is_multiview_loaded = true;
  }

  return is_multiview_loaded;
}

void strip_open_anim_file(Scene *scene, Strip *strip, bool openfile)
{
  if ((strip->anims.first != nullptr) && (((StripAnim *)strip->anims.first)->anim != nullptr) &&
      !openfile)
  {
    return;
  }

  /* Reset all the previously created anims. */
  SEQ_relations_sequence_free_anim(strip);

  Editing *ed = scene->ed;
  char filepath[FILE_MAX];
  BLI_path_join(
      filepath, sizeof(filepath), strip->data->dirpath, strip->data->stripdata->filename);
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&scene->id));

  bool is_multiview = (strip->flag & SEQ_USE_VIEWS) != 0 && (scene->r.scemode & R_MULTIVIEW) != 0;
  bool multiview_is_loaded = false;

  if (is_multiview) {
    multiview_is_loaded = open_anim_file_multiview(scene, strip, filepath);
  }

  if (!is_multiview || !multiview_is_loaded) {
    StripAnim *sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
    BLI_addtail(&strip->anims, sanim);
    open_anim_filepath(strip, sanim, filepath, openfile);
    index_dir_set(ed, strip, sanim);
  }
}

const Strip *SEQ_get_topmost_sequence(const Scene *scene, int frame)
{
  Editing *ed = scene->ed;

  if (!ed) {
    return nullptr;
  }

  ListBase *channels = SEQ_channels_displayed_get(ed);
  const Strip *best_seq = nullptr;
  int best_machine = -1;

  LISTBASE_FOREACH (const Strip *, strip, ed->seqbasep) {
    if (SEQ_render_is_muted(channels, strip) ||
        !SEQ_time_strip_intersects_frame(scene, strip, frame))
    {
      continue;
    }
    /* Only use strips that generate an image, not ones that combine
     * other strips or apply some effect. */
    if (ELEM(strip->type,
             STRIP_TYPE_IMAGE,
             STRIP_TYPE_META,
             STRIP_TYPE_SCENE,
             STRIP_TYPE_MOVIE,
             STRIP_TYPE_COLOR,
             STRIP_TYPE_TEXT))
    {
      if (strip->machine > best_machine) {
        best_seq = strip;
        best_machine = strip->machine;
      }
    }
  }
  return best_seq;
}

ListBase *SEQ_get_seqbase_by_seq(const Scene *scene, Strip *strip)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *main_seqbase = &ed->seqbase;
  Strip *strip_meta = SEQ_lookup_meta_by_strip(scene, strip);

  if (strip_meta != nullptr) {
    return &strip_meta->seqbase;
  }
  if (BLI_findindex(main_seqbase, strip) != -1) {
    return main_seqbase;
  }
  return nullptr;
}

Strip *SEQ_sequence_from_strip_elem(ListBase *seqbase, StripElem *se)
{
  Strip *iseq;

  for (iseq = static_cast<Strip *>(seqbase->first); iseq; iseq = iseq->next) {
    Strip *strip_found;
    if ((iseq->data && iseq->data->stripdata) &&
        ARRAY_HAS_ITEM(se, iseq->data->stripdata, iseq->len))
    {
      break;
    }
    if ((strip_found = SEQ_sequence_from_strip_elem(&iseq->seqbase, se))) {
      iseq = strip_found;
      break;
    }
  }

  return iseq;
}

Strip *SEQ_get_sequence_by_name(ListBase *seqbase, const char *name, bool recursive)
{
  LISTBASE_FOREACH (Strip *, iseq, seqbase) {
    if (STREQ(name, iseq->name + 2)) {
      return iseq;
    }
    if (recursive && !BLI_listbase_is_empty(&iseq->seqbase)) {
      Strip *rseq = SEQ_get_sequence_by_name(&iseq->seqbase, name, true);
      if (rseq != nullptr) {
        return rseq;
      }
    }
  }

  return nullptr;
}

Mask *SEQ_active_mask_get(Scene *scene)
{
  Strip *strip_act = SEQ_select_active_get(scene);

  if (strip_act && strip_act->type == STRIP_TYPE_MASK) {
    return strip_act->mask;
  }

  return nullptr;
}

void SEQ_alpha_mode_from_file_extension(Strip *strip)
{
  if (strip->data && strip->data->stripdata) {
    const char *filename = strip->data->stripdata->filename;
    strip->alpha_mode = BKE_image_alpha_mode_from_extension_ex(filename);
  }
}

bool SEQ_sequence_has_valid_data(const Strip *strip)
{
  switch (strip->type) {
    case STRIP_TYPE_MASK:
      return (strip->mask != nullptr);
    case STRIP_TYPE_MOVIECLIP:
      return (strip->clip != nullptr);
    case STRIP_TYPE_SCENE:
      return (strip->scene != nullptr);
    case STRIP_TYPE_SOUND_RAM:
      return (strip->sound != nullptr);
  }

  return true;
}

bool sequencer_seq_generates_image(Strip *strip)
{
  switch (strip->type) {
    case STRIP_TYPE_IMAGE:
    case STRIP_TYPE_SCENE:
    case STRIP_TYPE_MOVIE:
    case STRIP_TYPE_MOVIECLIP:
    case STRIP_TYPE_MASK:
    case STRIP_TYPE_COLOR:
    case STRIP_TYPE_TEXT:
      return true;
  }
  return false;
}

void SEQ_set_scale_to_fit(const Strip *strip,
                          const int image_width,
                          const int image_height,
                          const int preview_width,
                          const int preview_height,
                          const eSeqImageFitMethod fit_method)
{
  StripTransform *transform = strip->data->transform;

  switch (fit_method) {
    case SEQ_SCALE_TO_FIT:
      transform->scale_x = transform->scale_y = std::min(
          float(preview_width) / float(image_width), float(preview_height) / float(image_height));

      break;
    case SEQ_SCALE_TO_FILL:

      transform->scale_x = transform->scale_y = std::max(
          float(preview_width) / float(image_width), float(preview_height) / float(image_height));
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

void SEQ_ensure_unique_name(Strip *strip, Scene *scene)
{
  char name[STRIP_NAME_MAXSTR];

  STRNCPY_UTF8(name, strip->name + 2);
  SEQ_sequence_base_unique_name_recursive(scene, &scene->ed->seqbase, strip);
  BKE_animdata_fix_paths_rename(&scene->id,
                                scene->adt,
                                nullptr,
                                "sequence_editor.strips_all",
                                name,
                                strip->name + 2,
                                0,
                                0,
                                false);

  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, strip_child, &strip->seqbase) {
      SEQ_ensure_unique_name(strip_child, scene);
    }
  }
}
