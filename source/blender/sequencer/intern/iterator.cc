/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2024 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <cstring>

#include "DNA_sequence_types.h"

#include "BLI_listbase.h"

#include "SEQ_connect.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"

namespace blender::seq {

static bool strip_for_each_recursive(ListBaseT<Strip> *seqbase,
                                     ForEachFunc callback,
                                     void *user_data)
{
  for (Strip &strip : *seqbase) {
    if (!callback(&strip, user_data)) {
      /* Callback signaled stop, return. */
      return false;
    }
    if (strip.type == STRIP_TYPE_META) {
      if (!strip_for_each_recursive(&strip.seqbase, callback, user_data)) {
        return false;
      }
    }
  }
  return true;
}

static bool strip_for_each_recursive(ListBaseT<Strip> *seqbase,
                                     FunctionRef<bool(Strip *)> callback)
{
  for (Strip &strip : *seqbase) {
    if (!callback(&strip)) {
      /* Callback signaled stop, return. */
      return false;
    }
    if (strip.type == STRIP_TYPE_META) {
      if (!strip_for_each_recursive(&strip.seqbase, callback)) {
        return false;
      }
    }
  }
  return true;
}

void foreach_strip(ListBaseT<Strip> *seqbase, ForEachFunc callback, void *user_data)
{
  strip_for_each_recursive(seqbase, callback, user_data);
}

void foreach_strip(ListBaseT<Strip> *seqbase, FunctionRef<bool(Strip *)> callback)
{
  strip_for_each_recursive(seqbase, callback);
}

VectorSet<Strip *> query_by_reference(Strip *strip_reference,
                                      const Scene *scene,
                                      ListBaseT<Strip> *seqbase,
                                      void strip_query_func(const Scene *scene,
                                                            Strip *strip_reference,
                                                            ListBaseT<Strip> *seqbase,
                                                            VectorSet<Strip *> &strips))
{
  VectorSet<Strip *> strips;
  strip_query_func(scene, strip_reference, seqbase, strips);
  return strips;
}

void iterator_set_expand(const Scene *scene,
                         ListBaseT<Strip> *seqbase,
                         VectorSet<Strip *> &strips,
                         void strip_query_func(const Scene *scene,
                                               Strip *strip_reference,
                                               ListBaseT<Strip> *seqbase,
                                               VectorSet<Strip *> &strips))
{
  /* Collect expanded results for each sequence in provided VectorSet. */
  VectorSet<Strip *> query_matches;

  for (Strip *strip : strips) {
    query_matches.add_multiple(query_by_reference(strip, scene, seqbase, strip_query_func));
  }

  /* Merge all expanded results in provided VectorSet. */
  strips.add_multiple(query_matches);
}

static void query_all_strips_recursive(const ListBaseT<Strip> *seqbase, VectorSet<Strip *> &strips)
{
  for (Strip &strip : *seqbase) {
    if (strip.type == STRIP_TYPE_META) {
      query_all_strips_recursive(&strip.seqbase, strips);
    }
    strips.add(&strip);
  }
}

VectorSet<Strip *> query_all_strips_recursive(const ListBaseT<Strip> *seqbase)
{
  VectorSet<Strip *> strips;
  query_all_strips_recursive(seqbase, strips);
  return strips;
}

static void query_strips_recursive_at_frame(const Scene *scene,
                                            const ListBaseT<Strip> *seqbase,
                                            const int timeline_frame,
                                            VectorSet<Strip *> &strips)
{
  for (Strip &strip : *seqbase) {
    if (!strip.intersects_frame(scene, timeline_frame)) {
      continue;
    }
    if (strip.type == STRIP_TYPE_META) {
      query_strips_recursive_at_frame(scene, &strip.seqbase, timeline_frame, strips);
    }
    strips.add(&strip);
  }
}

VectorSet<Strip *> query_strips_recursive_at_frame(const Scene *scene,
                                                   const ListBaseT<Strip> *seqbase,
                                                   const int timeline_frame)
{
  VectorSet<Strip *> strips;
  query_strips_recursive_at_frame(scene, seqbase, timeline_frame, strips);
  return strips;
}

VectorSet<Strip *> query_all_strips(ListBaseT<Strip> *seqbase)
{
  VectorSet<Strip *> strips;
  for (Strip &strip : *seqbase) {
    strips.add(&strip);
  }
  return strips;
}

VectorSet<Strip *> query_selected_strips(ListBaseT<Strip> *seqbase)
{
  VectorSet<Strip *> strips;
  for (Strip &strip : *seqbase) {
    if ((strip.flag & SEQ_SELECT) != 0) {
      strips.add(&strip);
    }
  }
  return strips;
}

static VectorSet<Strip *> query_strips_at_frame(const Scene *scene,
                                                ListBaseT<Strip> *seqbase,
                                                const int timeline_frame)
{
  VectorSet<Strip *> strips;

  for (Strip &strip : *seqbase) {
    if (strip.intersects_frame(scene, timeline_frame)) {
      strips.add(&strip);
    }
  }
  return strips;
}

static void collection_filter_channel_up_to_incl(VectorSet<Strip *> &strip_stack,
                                                 const int channel)
{
  strip_stack.remove_if([&](Strip *strip) { return strip->channel > channel; });
}

bool must_render_strip(const VectorSet<Strip *> &strip_stack, Strip *target_strip)
{
  bool strip_have_effect_in_stack = false;
  for (Strip *strip : strip_stack) {
    /* Strips below another strip with replace blending are never directly rendered. */
    if (strip->blend_mode == STRIP_BLEND_REPLACE && target_strip->channel < strip->channel) {
      return false;
    }
    if (strip->is_effect() && relation_is_effect_of_strip(strip, target_strip)) {
      /* Strips at the same channel or above their effect are rendered. */
      if (target_strip->channel >= strip->channel) {
        return true;
      }
      /* Mark that this strip has an effect in the stack that is above the strip. */
      strip_have_effect_in_stack = true;
    }
  }

  /* All effects with inputs are rendered assuming they pass the above checks. */
  if (target_strip->is_effect() && effect_get_num_inputs(target_strip->type) != 0) {
    return true;
  }

  /* If strip has effects in stack, and all effects are above this strip, it is not rendered. */
  if (strip_have_effect_in_stack) {
    return false;
  }

  return true;
}

/* Remove strips we don't want to render from VectorSet. */
static void collection_filter_rendered_strips(VectorSet<Strip *> &strip_stack,
                                              ListBaseT<SeqTimelineChannel> *channels)
{
  /* Remove sound strips and muted strips from VectorSet, because these are not rendered.
   * Function #must_render_strip() don't have to check for these strips anymore. */
  strip_stack.remove_if([&](Strip *strip) {
    return strip->type == STRIP_TYPE_SOUND || render_is_muted(channels, strip);
  });

  strip_stack.remove_if([&](Strip *strip) { return !must_render_strip(strip_stack, strip); });
}

VectorSet<Strip *> query_rendered_strips(const Scene *scene,
                                         ListBaseT<SeqTimelineChannel> *channels,
                                         ListBaseT<Strip> *seqbase,
                                         const int timeline_frame,
                                         const int displayed_channel)
{
  VectorSet strips = query_strips_at_frame(scene, seqbase, timeline_frame);
  if (displayed_channel != 0) {
    collection_filter_channel_up_to_incl(strips, displayed_channel);
  }
  collection_filter_rendered_strips(strips, channels);
  return strips;
}

Vector<Strip *> query_rendered_strips_sorted(const Scene *scene,
                                             ListBaseT<SeqTimelineChannel> *channels,
                                             ListBaseT<Strip> *seqbase,
                                             const int timeline_frame,
                                             const int chanshown)
{
  VectorSet strips = query_rendered_strips(scene, channels, seqbase, timeline_frame, chanshown);

  Vector<Strip *> strips_vec = strips.extract_vector();
  /* Sort strips by channel. */
  std::ranges::sort(strips_vec,
                    [](const Strip *a, const Strip *b) { return a->channel < b->channel; });
  return strips_vec;
}

VectorSet<Strip *> query_unselected_strips(ListBaseT<Strip> *seqbase)
{
  VectorSet<Strip *> strips;
  for (Strip &strip : *seqbase) {
    if ((strip.flag & SEQ_SELECT) != 0) {
      continue;
    }
    strips.add(&strip);
  }
  return strips;
}

void query_strip_effect_chain(const Scene *scene,
                              Strip *reference_strip,
                              ListBaseT<Strip> *seqbase,
                              VectorSet<Strip *> &r_strips)
{
  if (r_strips.contains(reference_strip)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }

  r_strips.add(reference_strip);

  /* Find all input strips for `reference_strip`. */
  if (reference_strip->is_effect()) {
    if (reference_strip->input1) {
      query_strip_effect_chain(scene, reference_strip->input1, seqbase, r_strips);
    }
    if (reference_strip->input2) {
      query_strip_effect_chain(scene, reference_strip->input2, seqbase, r_strips);
    }
  }

  /* Find all effect strips that have `reference_strip` as an input. */
  for (Strip &strip_test : *seqbase) {
    if (strip_test.input1 == reference_strip || strip_test.input2 == reference_strip) {
      query_strip_effect_chain(scene, &strip_test, seqbase, r_strips);
    }
  }
}

void query_strip_connected_and_effect_chain(const Scene *scene,
                                            Strip *reference_strip,
                                            ListBaseT<Strip> *seqbase,
                                            VectorSet<Strip *> &r_strips)
{

  Vector<Strip *> pending;
  pending.append(reference_strip);

  while (!pending.is_empty()) {
    Strip *current = pending.pop_last();

    if (r_strips.contains(current)) {
      continue;
    }

    r_strips.add(current);

    VectorSet<Strip *> connections = connected_strips_get(current);
    for (Strip *connection : connections) {
      if (!r_strips.contains(connection)) {
        pending.append(connection);
      }
    }

    VectorSet<Strip *> effect_chain;
    query_strip_effect_chain(scene, current, seqbase, effect_chain);
    for (Strip *effect_strip : effect_chain) {
      if (!r_strips.contains(effect_strip)) {
        pending.append(effect_strip);
      }
    }
  }
}

}  // namespace blender::seq
