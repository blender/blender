/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BKE_main.hh"
#include "BLI_fileops.h"
#include "BLI_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "SEQ_utils.hh"

namespace blender::seq {

static ThreadMutex presence_lock = BLI_MUTEX_INITIALIZER;

static const char *get_seq_base_path(const Strip *strip)
{
  return strip->scene ? ID_BLEND_PATH_FROM_GLOBAL(&strip->scene->id) :
                        BKE_main_blendfile_path_from_global();
}

static bool check_sound_media_missing(const bSound *sound, const Strip *strip)
{
  if (sound == nullptr) {
    return false;
  }

  char filepath[FILE_MAX];
  STRNCPY(filepath, sound->filepath);
  const char *basepath = get_seq_base_path(strip);
  BLI_path_abs(filepath, basepath);
  return !BLI_exists(filepath);
}

static bool check_media_missing(const Strip *strip)
{
  if (strip == nullptr || strip->data == nullptr) {
    return false;
  }

  /* Images or movies. */
  if (ELEM((strip)->type, STRIP_TYPE_MOVIE, STRIP_TYPE_IMAGE)) {
    const StripElem *elem = strip->data->stripdata;
    if (elem != nullptr) {
      int paths_count = 1;
      if (strip->type == STRIP_TYPE_IMAGE) {
        /* Image strip has array of file names. */
        paths_count = int(MEM_allocN_len(elem) / sizeof(*elem));
      }
      char filepath[FILE_MAX];
      const char *basepath = get_seq_base_path(strip);
      for (int i = 0; i < paths_count; i++, elem++) {
        BLI_path_join(filepath, sizeof(filepath), strip->data->dirpath, elem->filename);
        BLI_path_abs(filepath, basepath);
        if (!BLI_exists(filepath)) {
          return true;
        }
      }
    }
  }

  /* Recurse into meta strips. */
  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, seqn, &strip->seqbase) {
      if (check_media_missing(seqn)) {
        return true;
      }
    }
  }

  /* Nothing is missing. */
  return false;
}

struct MediaPresence {
  Map<const void *, bool> map_seq;
  Map<const bSound *, bool> map_sound;
};

static MediaPresence *get_media_presence_cache(Scene *scene)
{
  MediaPresence **presence = &scene->ed->runtime.media_presence;
  if (*presence == nullptr) {
    *presence = MEM_new<MediaPresence>(__func__);
  }
  return *presence;
}

bool media_presence_is_missing(Scene *scene, const Strip *strip)
{
  if (strip == nullptr || scene == nullptr || scene->ed == nullptr) {
    return false;
  }

  BLI_mutex_lock(&presence_lock);

  MediaPresence *presence = get_media_presence_cache(scene);

  bool missing = false;

  /* Strips that reference another data block that has path to media
   * (e.g. sound strips) need to key the presence cache on that data
   * block. Since it can be used by multiple strips. */
  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    const bSound *sound = strip->sound;
    const bool *val = presence->map_sound.lookup_ptr(sound);
    if (val != nullptr) {
      missing = *val;
    }
    else {
      missing = check_sound_media_missing(sound, strip);
      presence->map_sound.add_new(sound, missing);
    }
  }
  else {
    /* Regular strips that point to media directly. */
    const bool *val = presence->map_seq.lookup_ptr(strip);
    if (val != nullptr) {
      missing = *val;
    }
    else {
      missing = check_media_missing(strip);
      presence->map_seq.add_new(strip, missing);
    }
  }

  BLI_mutex_unlock(&presence_lock);
  return missing;
}

void media_presence_set_missing(Scene *scene, const Strip *strip, bool missing)
{
  if (strip == nullptr || scene == nullptr || scene->ed == nullptr) {
    return;
  }

  BLI_mutex_lock(&presence_lock);

  MediaPresence *presence = get_media_presence_cache(scene);

  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    const bSound *sound = strip->sound;
    presence->map_sound.add_overwrite(sound, missing);
  }
  else {
    presence->map_seq.add_overwrite(strip, missing);
  }

  BLI_mutex_unlock(&presence_lock);
}

void media_presence_invalidate_strip(Scene *scene, const Strip *strip)
{
  BLI_mutex_lock(&presence_lock);
  if (scene != nullptr && scene->ed != nullptr && scene->ed->runtime.media_presence != nullptr) {
    scene->ed->runtime.media_presence->map_seq.remove(strip);
  }
  BLI_mutex_unlock(&presence_lock);
}

void media_presence_invalidate_sound(Scene *scene, const bSound *sound)
{
  BLI_mutex_lock(&presence_lock);
  if (scene != nullptr && scene->ed != nullptr && scene->ed->runtime.media_presence != nullptr) {
    scene->ed->runtime.media_presence->map_sound.remove(sound);
  }
  BLI_mutex_unlock(&presence_lock);
}

void media_presence_free(Scene *scene)
{
  BLI_mutex_lock(&presence_lock);
  if (scene != nullptr && scene->ed != nullptr && scene->ed->runtime.media_presence != nullptr) {
    MEM_delete(scene->ed->runtime.media_presence);
    scene->ed->runtime.media_presence = nullptr;
  }
  BLI_mutex_unlock(&presence_lock);
}

}  // namespace blender::seq
