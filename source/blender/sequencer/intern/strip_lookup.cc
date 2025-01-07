/* SPDX-FileCopyrightText: 2021-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "SEQ_sequencer.hh"
#include "sequencer.hh"

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_vector_set.hh"

#include <cstring>
#include <mutex>

#include "MEM_guardedalloc.h"

static std::mutex lookup_lock;

struct StripLookup {
  blender::Map<std::string, Strip *> strip_by_name;
  blender::Map<const Strip *, Strip *> meta_by_strip;
  blender::Map<const Strip *, blender::VectorSet<Strip *>> effects_by_strip;
  blender::Map<const SeqTimelineChannel *, Strip *> owner_by_channel;
  bool is_valid = false;
};

static void strip_lookup_append_effect(const Strip *input, Strip *effect, StripLookup *lookup)
{
  if (input == nullptr) {
    return;
  }

  blender::VectorSet<Strip *> &effects = lookup->effects_by_strip.lookup_or_add_default(input);

  effects.add(effect);
}

static void strip_lookup_build_effect(Strip *strip, StripLookup *lookup)
{
  if ((strip->type & STRIP_TYPE_EFFECT) == 0) {
    return;
  }

  strip_lookup_append_effect(strip->seq1, strip, lookup);
  strip_lookup_append_effect(strip->seq2, strip, lookup);
}

static void strip_lookup_build_from_seqbase(Strip *parent_meta,
                                            const ListBase *seqbase,
                                            StripLookup *lookup)
{
  if (parent_meta != nullptr) {
    LISTBASE_FOREACH (SeqTimelineChannel *, channel, &parent_meta->channels) {
      lookup->owner_by_channel.add(channel, parent_meta);
    }
  }

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    lookup->strip_by_name.add(strip->name + 2, strip);
    lookup->meta_by_strip.add(strip, parent_meta);
    strip_lookup_build_effect(strip, lookup);

    if (strip->type == STRIP_TYPE_META) {
      strip_lookup_build_from_seqbase(strip, &strip->seqbase, lookup);
    }
  }
}

static void strip_lookup_build(const Scene *scene, StripLookup *lookup)
{
  Editing *ed = SEQ_editing_get(scene);
  strip_lookup_build_from_seqbase(nullptr, &ed->seqbase, lookup);
  lookup->is_valid = true;
}

static StripLookup *strip_lookup_new()
{
  StripLookup *lookup = MEM_new<StripLookup>(__func__);
  return lookup;
}

static void strip_lookup_free(StripLookup **lookup)
{
  MEM_delete(*lookup);
  *lookup = nullptr;
}

static void strip_lookup_rebuild(const Scene *scene, StripLookup **lookup)
{
  strip_lookup_free(lookup);
  *lookup = strip_lookup_new();
  strip_lookup_build(scene, *lookup);
}

static void strip_lookup_update_if_needed(const Scene *scene, StripLookup **lookup)
{
  if (!scene->ed) {
    return;
  }
  if (*lookup && (*lookup)->is_valid) {
    return;
  }

  strip_lookup_rebuild(scene, lookup);
}

void SEQ_strip_lookup_free(const Scene *scene)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  StripLookup *lookup = scene->ed->runtime.strip_lookup;
  strip_lookup_free(&lookup);
}

Strip *SEQ_lookup_strip_by_name(const Scene *scene, const char *key)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(scene, &scene->ed->runtime.strip_lookup);
  StripLookup *lookup = scene->ed->runtime.strip_lookup;
  return lookup->strip_by_name.lookup_default(key, nullptr);
}

Strip *SEQ_lookup_meta_by_strip(const Scene *scene, const Strip *key)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(scene, &scene->ed->runtime.strip_lookup);
  StripLookup *lookup = scene->ed->runtime.strip_lookup;
  return lookup->meta_by_strip.lookup_default(key, nullptr);
}

blender::Span<Strip *> SEQ_lookup_effects_by_strip(const Scene *scene, const Strip *key)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(scene, &scene->ed->runtime.strip_lookup);
  StripLookup *lookup = scene->ed->runtime.strip_lookup;
  blender::VectorSet<Strip *> &effects = lookup->effects_by_strip.lookup_or_add_default(key);
  return effects.as_span();
}

Strip *SEQ_lookup_strip_by_channel_owner(const Scene *scene, const SeqTimelineChannel *channel)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(scene, &scene->ed->runtime.strip_lookup);
  StripLookup *lookup = scene->ed->runtime.strip_lookup;
  return lookup->owner_by_channel.lookup_default(channel, nullptr);
}

void SEQ_strip_lookup_invalidate(const Scene *scene)
{
  if (scene == nullptr || scene->ed == nullptr) {
    return;
  }

  std::lock_guard lock(lookup_lock);
  StripLookup *lookup = scene->ed->runtime.strip_lookup;
  if (lookup != nullptr) {
    lookup->is_valid = false;
  }
}
