/* SPDX-FileCopyrightText: 2021-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "SEQ_sequencer.hh"
#include "sequencer.hh"

#include "DNA_listBase.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_mutex.hh"

#include <cstring>

#include "MEM_guardedalloc.h"

namespace blender::seq {

static Mutex lookup_lock;

struct StripLookup {
  Map<std::string, Strip *> strip_by_name;
  Map<const Scene *, VectorSet<Strip *>> strips_by_scene;
  Map<const bNodeTree *, VectorSet<Strip *>> strips_by_compositor_node_group;
  Map<const Strip *, Strip *> meta_by_strip;
  Map<const Strip *, VectorSet<Strip *>> effects_by_strip;
  Map<const SeqTimelineChannel *, Strip *> owner_by_channel;
  bool is_valid = false;
};

static void strip_lookup_append_effect(const Strip *input, Strip *effect, StripLookup *lookup)
{
  if (input == nullptr) {
    return;
  }

  VectorSet<Strip *> &effects = lookup->effects_by_strip.lookup_or_add_default(input);

  effects.add(effect);
}

static void strip_by_scene_lookup_build(Strip *strip, StripLookup *lookup)
{
  if (strip->scene == nullptr) {
    return;
  }
  VectorSet<Strip *> &strips = lookup->strips_by_scene.lookup_or_add_default(strip->scene);
  strips.add(strip);
}

static void strip_by_compositor_node_group_lookup_build(Strip *strip, StripLookup *lookup)
{
  LISTBASE_FOREACH (StripModifierData *, modifier, &strip->modifiers) {
    if (modifier->type != eSeqModifierType_Compositor) {
      continue;
    }

    const SequencerCompositorModifierData *modifier_data =
        reinterpret_cast<SequencerCompositorModifierData *>(modifier);
    if (!modifier_data->node_group) {
      continue;
    }

    VectorSet<Strip *> &strips = lookup->strips_by_compositor_node_group.lookup_or_add_default(
        modifier_data->node_group);
    strips.add(strip);
  }
}

static void strip_lookup_build_effect(Strip *strip, StripLookup *lookup)
{
  if (!strip->is_effect()) {
    return;
  }

  strip_lookup_append_effect(strip->input1, strip, lookup);
  strip_lookup_append_effect(strip->input2, strip, lookup);
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
    strip_by_scene_lookup_build(strip, lookup);
    strip_by_compositor_node_group_lookup_build(strip, lookup);

    if (strip->type == STRIP_TYPE_META) {
      strip_lookup_build_from_seqbase(strip, &strip->seqbase, lookup);
    }
  }
}

static void strip_lookup_build(const Editing *ed, StripLookup *lookup)
{
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

static void strip_lookup_rebuild(const Editing *ed, StripLookup **lookup)
{
  strip_lookup_free(lookup);
  *lookup = strip_lookup_new();
  strip_lookup_build(ed, *lookup);
}

static void strip_lookup_update_if_needed(const Editing *ed, StripLookup **lookup)
{
  if (!ed) {
    return;
  }
  if (*lookup && (*lookup)->is_valid) {
    return;
  }

  strip_lookup_rebuild(ed, lookup);
}

void strip_lookup_free(Editing *ed)
{
  BLI_assert(ed != nullptr);
  std::lock_guard lock(lookup_lock);
  strip_lookup_free(&ed->runtime.strip_lookup);
}

Strip *lookup_strip_by_name(Editing *ed, const char *key)
{
  BLI_assert(ed != nullptr);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(ed, &ed->runtime.strip_lookup);
  StripLookup *lookup = ed->runtime.strip_lookup;
  return lookup->strip_by_name.lookup_default(key, nullptr);
}

Span<Strip *> lookup_strips_by_scene(Editing *ed, const Scene *key)
{
  BLI_assert(ed != nullptr);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(ed, &ed->runtime.strip_lookup);
  StripLookup *lookup = ed->runtime.strip_lookup;
  VectorSet<Strip *> &strips = lookup->strips_by_scene.lookup_or_add_default(key);
  return strips.as_span();
}

Map<const Scene *, VectorSet<Strip *>> &lookup_strips_by_scene_map_get(Editing *ed)
{
  BLI_assert(ed != nullptr);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(ed, &ed->runtime.strip_lookup);
  StripLookup *lookup = ed->runtime.strip_lookup;
  return lookup->strips_by_scene;
}

Span<Strip *> lookup_strips_by_compositor_node_group(Editing *ed, const bNodeTree *key)
{
  BLI_assert(ed != nullptr);
  BLI_assert(key->type == NTREE_COMPOSIT);

  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(ed, &ed->runtime.strip_lookup);
  StripLookup *lookup = ed->runtime.strip_lookup;
  VectorSet<Strip *> &strips = lookup->strips_by_compositor_node_group.lookup_or_add_default(key);
  return strips.as_span();
}

Strip *lookup_meta_by_strip(Editing *ed, const Strip *key)
{
  BLI_assert(ed != nullptr);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(ed, &ed->runtime.strip_lookup);
  StripLookup *lookup = ed->runtime.strip_lookup;
  return lookup->meta_by_strip.lookup_default(key, nullptr);
}

Span<Strip *> SEQ_lookup_effects_by_strip(Editing *ed, const Strip *key)
{
  BLI_assert(ed != nullptr);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(ed, &ed->runtime.strip_lookup);
  StripLookup *lookup = ed->runtime.strip_lookup;
  VectorSet<Strip *> &effects = lookup->effects_by_strip.lookup_or_add_default(key);
  return effects.as_span();
}

Strip *lookup_strip_by_channel_owner(Editing *ed, const SeqTimelineChannel *channel)
{
  BLI_assert(ed != nullptr);
  std::lock_guard lock(lookup_lock);
  strip_lookup_update_if_needed(ed, &ed->runtime.strip_lookup);
  StripLookup *lookup = ed->runtime.strip_lookup;
  return lookup->owner_by_channel.lookup_default(channel, nullptr);
}

void strip_lookup_invalidate(const Editing *ed)
{
  if (ed == nullptr) {
    return;
  }

  std::lock_guard lock(lookup_lock);
  StripLookup *lookup = ed->runtime.strip_lookup;
  if (lookup != nullptr) {
    lookup->is_valid = false;
  }
}

}  // namespace blender::seq
