/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "RNA_path.hh"

#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"
#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

#include "BKE_animsys.hh"
#include "BKE_idprop.hh"
#include "BKE_image.hh"
#include "BKE_library.hh"
#include "BKE_scene.hh"

#include "SEQ_channels.hh"
#include "SEQ_edit.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_utils.hh"

#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"

#include "cache/movie_reader_cache.hh"
#include "utils.hh"

namespace blender::seq {

struct StripUniqueInfo {
  Strip *strip;
  char name_src[STRIP_NAME_MAXSTR];
  char name_dest[STRIP_NAME_MAXSTR];
  int count;
  int match;
};

static void seqbase_unique_name(ListBaseT<Strip> *seqbasep, StripUniqueInfo *sui)
{
  for (Strip &strip : *seqbasep) {
    if ((sui->strip != &strip) && STREQ(sui->name_dest, strip.name + 2)) {
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
    seqbase_unique_name(&strip->seqbase, static_cast<StripUniqueInfo *>(arg_pt));
  }
  return true;
}

void strip_unique_name_set(Scene *scene, ListBaseT<Strip> *seqbasep, Strip *strip)
{
  StripUniqueInfo sui;
  char *dot;
  sui.strip = strip;
  STRNCPY(sui.name_src, strip->name + 2);
  STRNCPY(sui.name_dest, sui.name_src);
  sui.count = 1;
  sui.match = 1; /* assume the worst to start the loop */

  /* Strip off the suffix only if it is purely numeric. */
  if ((dot = strrchr(sui.name_src, '.'))) {
    char *suffix = dot + 1;
    if (BLI_string_is_decimal(suffix)) {
      *dot = '\0';
      sui.count = atoi(suffix) + 1;
    }
  }

  while (sui.match) {
    sui.match = 0;
    seqbase_unique_name(seqbasep, &sui);
    foreach_strip(seqbasep, seqbase_unique_name_recursive_fn, &sui);
  }

  edit_strip_name_set(scene, strip, sui.name_dest);
}

const char *get_default_stripname_by_type(int type)
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
    case STRIP_TYPE_SOUND:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Audio");
    case STRIP_TYPE_CROSS:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Crossfade");
    case STRIP_TYPE_GAMCROSS:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Gamma Crossfade");
    case STRIP_TYPE_COMPOSITOR:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Compositor");
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
    case STRIP_TYPE_COLORMIX:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Color Mix");
    case STRIP_TYPE_WIPE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Wipe");
    case STRIP_TYPE_GLOW:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, "Glow");
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

const char *strip_give_name(const Strip *strip)
{
  const char *name = get_default_stripname_by_type(strip->type);

  if (!name) {
    if (!strip->is_effect()) {
      return strip->data->dirpath;
    }

    return DATA_("Effect");
  }
  return name;
}

ListBaseT<Strip> *get_seqbase_from_strip(Strip *strip,
                                         ListBaseT<SeqTimelineChannel> **r_channels,
                                         int *r_offset)
{
  ListBaseT<Strip> *seqbase = nullptr;

  switch (strip->type) {
    case STRIP_TYPE_META: {
      seqbase = &strip->seqbase;
      *r_channels = &strip->channels;
      *r_offset = strip->content_start();
      break;
    }
    case STRIP_TYPE_SCENE: {
      if (strip->flag & SEQ_SCENE_STRIPS && strip->scene) {
        Editing *ed = editing_get(strip->scene);
        if (ed) {
          seqbase = &ed->seqbase;
          *r_channels = &ed->channels;
          *r_offset = strip->scene->r.sfra;
        }
      }
      break;
    }
    default:
      break;
  }

  return seqbase;
}

void MovieReaderDeleter::operator()(MovieReader *reader) const
{
  MOV_close(reader);
}

void movie_metadata_invalidate(Strip &strip)
{
  if (strip.runtime->movie_metadata != nullptr) {
    IDP_FreeProperty(strip.runtime->movie_metadata);
  }
  strip.runtime->movie_metadata = nullptr;
  strip.runtime->movie_metadata_is_loaded = false;
}

void movie_metadata_set_from_reader(Strip &strip, MovieReader &reader)
{
  movie_metadata_invalidate(strip);
  const IDProperty *metadata = MOV_load_metadata(&reader);
  strip.runtime->movie_metadata = metadata != nullptr ? IDP_CopyProperty(metadata) : nullptr;
  strip.runtime->movie_metadata_is_loaded = true;
}

IDProperty *movie_metadata_ensure(Scene &scene, Strip &strip)
{
  if (!strip.runtime->movie_metadata_is_loaded) {
    MovieReaderAccessor reader = movie_reader_cache_acquire_any(scene, strip);
    if (reader) {
      movie_metadata_set_from_reader(strip, *reader.reader());
    }
  }
  return strip.runtime->movie_metadata;
}

const Strip *strip_topmost_get(const Scene *scene, int frame)
{
  Editing *ed = scene->ed;

  if (!ed) {
    return nullptr;
  }

  ListBaseT<SeqTimelineChannel> *channels = channels_displayed_get(ed);
  const Strip *best_strip = nullptr;
  int best_channel = -1;

  for (const Strip &strip : *ed->current_strips()) {
    if (render_is_muted(channels, &strip) || !strip.intersects_frame(scene, frame)) {
      continue;
    }
    /* Only use strips that generate an image, not ones that combine
     * other strips or apply some effect. */
    if (ELEM(strip.type,
             STRIP_TYPE_IMAGE,
             STRIP_TYPE_META,
             STRIP_TYPE_SCENE,
             STRIP_TYPE_MOVIE,
             STRIP_TYPE_COLOR,
             STRIP_TYPE_TEXT))
    {
      if (strip.channel > best_channel) {
        best_strip = &strip;
        best_channel = strip.channel;
      }
    }
  }
  return best_strip;
}

ListBaseT<Strip> *get_seqbase_by_strip(const Scene *scene, Strip *strip)
{
  Editing *ed = editing_get(scene);
  ListBaseT<Strip> *main_seqbase = &ed->seqbase;
  Strip *strip_meta = lookup_meta_by_strip(ed, strip);

  if (strip_meta != nullptr) {
    return &strip_meta->seqbase;
  }
  if (BLI_findindex(main_seqbase, strip) != -1) {
    return main_seqbase;
  }
  return nullptr;
}

Strip *strip_from_strip_elem(ListBaseT<Strip> *seqbase, StripElem *se)
{
  Strip *istrip;

  for (istrip = static_cast<Strip *>(seqbase->first); istrip; istrip = istrip->next) {
    Strip *strip_found;
    if ((istrip->data && istrip->data->stripdata) &&
        ARRAY_HAS_ITEM(se, istrip->data->stripdata, istrip->content_length()))
    {
      break;
    }
    if ((strip_found = strip_from_strip_elem(&istrip->seqbase, se))) {
      istrip = strip_found;
      break;
    }
  }

  return istrip;
}

Strip *get_strip_by_name(ListBaseT<Strip> *seqbase, const char *name, bool recursive)
{
  for (Strip &istrip : *seqbase) {
    if (STREQ(name, istrip.name + 2)) {
      return &istrip;
    }
    if (recursive && !istrip.seqbase.is_empty()) {
      Strip *rseq = get_strip_by_name(&istrip.seqbase, name, true);
      if (rseq != nullptr) {
        return rseq;
      }
    }
  }

  return nullptr;
}

Mask *active_mask_get(Scene *scene)
{
  Strip *strip_act = select_active_get(scene);

  if (strip_act && strip_act->type == STRIP_TYPE_MASK) {
    return strip_act->mask;
  }

  return nullptr;
}

void alpha_mode_from_file_extension(Strip *strip)
{
  if (strip->data && strip->data->stripdata) {
    const char *filename = strip->data->stripdata->filename;
    strip->alpha_mode = eStripAlphaMode(BKE_image_alpha_mode_from_extension_ex(filename));
  }
}

bool strip_has_valid_data(const Strip *strip)
{
  switch (strip->type) {
    case STRIP_TYPE_MASK:
      return (strip->mask != nullptr);
    case STRIP_TYPE_MOVIECLIP:
      return (strip->clip != nullptr);
    case STRIP_TYPE_SCENE:
      return (strip->scene != nullptr);
    case STRIP_TYPE_SOUND:
      return (strip->sound != nullptr);
    default:
      return true;
  }
}

bool sequencer_strip_generates_image(Strip *strip)
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
    default:
      return false;
  }
}

void set_scale_to_fit(const Strip *strip,
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

void ensure_unique_name(Main &bmain, Strip *strip, Scene *scene)
{
  char name[STRIP_NAME_MAXSTR];

  STRNCPY_UTF8(name, strip->name + 2);
  strip_unique_name_set(scene, &scene->ed->seqbase, strip);
  BKE_animdata_fix_paths(scene->id,
                         "sequence_editor.strips_all",
                         RNA_path_name_to_infix(name),
                         RNA_path_name_to_infix(strip->name + 2),
                         /*verify_paths=*/false,
                         bmain);

  if (strip->type == STRIP_TYPE_META) {
    for (Strip &strip_child : strip->seqbase) {
      ensure_unique_name(bmain, &strip_child, scene);
    }
  }
}

}  // namespace blender::seq
