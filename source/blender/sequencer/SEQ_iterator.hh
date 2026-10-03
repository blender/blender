/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_enum_flags.hh"
#include "BLI_function_ref.hh"
#include "BLI_vector_set.hh"
#include "DNA_listBase.h"

namespace blender {

struct Scene;
struct SeqTimelineChannel;
struct Strip;
struct Editing;

namespace seq {

/**
 * Callback format for the for_each function below.
 */
using ForEachFunc = bool (*)(Strip *strip, void *user_data);

/**
 * Recursively iterate through all strips in \a seqbase, calling \a callback on each.
 *
 * \param callback: Return false to stop iterating.
 */
void foreach_strip(ListBaseT<Strip> *seqbase, ForEachFunc callback, void *user_data);

/** Same as above, but using a more modern FunctionRef as callback. */
void foreach_strip(ListBaseT<Strip> *seqbase, FunctionRef<bool(Strip *)> callback);

/* -------------------------------------------------------------------- */
/** \name Expand Strips
 * \{ */

/**
 * Ways in which one strip can relate to another.
 * Used by #expand_strips to decide which relations to follow.
 */
enum class StripRelation {
  None = 0,

  /** Effects of this strip. */
  Effects = (1 << 0),
  /** If the strip is an effect, the inputs of this strip. */
  Inputs = (1 << 1),
  /** If the strip is a meta, the strips nested inside this strip. */
  MetaContents = (1 << 2),
  /** Strips connected to this strip. */
  Connected = (1 << 3),

  /** The entire effect chain a strip participates in, followed in both directions. */
  EffectChain = Effects | Inputs,
  /** As above, plus the connections of every strip in that chain. */
  ConnectedEffectChain = EffectChain | Connected,
};
ENUM_OPERATORS(StripRelation);

/**
 * Grow a #VectorSet of \a strips to also contain every strip reachable from it by repeatedly
 * following the relations in \a include.
 */
void expand_strips(Editing *ed, VectorSet<Strip *> &strips, StripRelation include);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Query Strips
 * \{ */

/**
 * Query all selected strips in seqbase.
 */
VectorSet<Strip *> query_selected_strips(ListBaseT<Strip> *seqbase);
/**
 * Query all unselected strips in seqbase.
 */
VectorSet<Strip *> query_unselected_strips(ListBaseT<Strip> *seqbase);
/**
 * Query all strips in seqbase. This does not include strips nested in meta strips.
 */
VectorSet<Strip *> query_all_strips(ListBaseT<Strip> *seqbase);
/**
 * Query all strips in seqbase and nested meta strips.
 */
VectorSet<Strip *> query_all_strips_recursive(const ListBaseT<Strip> *seqbase);

/**
 * Query strips at \a timeline_frame in seqbase and nested meta strips.
 */
VectorSet<Strip *> query_strips_recursive_at_frame(const Scene *scene,
                                                   const ListBaseT<Strip> *seqbase,
                                                   int timeline_frame);

/**
 * Query strips that will be rendered at \a timeline_frame on all channels less than
 * or equal to \a displayed_channel. This does not recurse into meta-strips or sequencer-type scene
 * strips.
 *
 * \note This only returns strips that are directly rendered in the strip stack. Other strips'
 * content may still be indirectly rendered, such as effect inputs, even though they are not
 * included in the returned `VectorSet`. See #must_render_strip.
 * \note Pass \a displayed_channel of 0 to consider all channels.
 */
VectorSet<Strip *> query_rendered_strips(const Scene *scene,
                                         ListBaseT<SeqTimelineChannel> *channels,
                                         ListBaseT<Strip> *seqbase,
                                         int timeline_frame,
                                         int displayed_channel);

/**
 * Strips are sorted from lowest to highest channel.
 * \copydoc #query_rendered_strips
 */
Vector<Strip *> query_rendered_strips_sorted(const Scene *scene,
                                             ListBaseT<SeqTimelineChannel> *channels,
                                             ListBaseT<Strip> *seqbase,
                                             int timeline_frame,
                                             int chanshown);

/**
 * Check to see whether we cannot skip rendering this strip.
 * Some strips do not need to be directly rendered since they are already indirectly rendered as
 * part of other strips' renders (such as effect strip inputs). These should be skipped to avoid
 * unnecessary re-rendering.
 *
 * \note Take care when changing the logic of this function since order matters.
 */
bool must_render_strip(const VectorSet<Strip *> &strip_stack, Strip *target_strip);

/** \} */

}  // namespace seq
}  // namespace blender
