/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_function_ref.hh"
#include "BLI_vector_set.hh"

struct ListBase;
struct Scene;
struct Strip;

namespace blender::seq {

/**
 * Callback format for the for_each function below.
 */
using ForEachFunc = bool (*)(Strip *strip, void *user_data);

/**
 * Utility function to recursively iterate through all sequence strips in a `seqbase` list.
 * Uses callback to do operations on each element.
 * The callback can stop the iteration if needed.
 *
 * \param seqbase: #ListBase of sequences to be iterated over.
 * \param callback: query function callback, returns false if iteration should stop.
 * \param user_data: pointer to user data that can be used in the callback function.
 */
void foreach_strip(ListBase *seqbase, ForEachFunc callback, void *user_data);

/** Same as above, but using a more modern FunctionRef as callback. */
void foreach_strip(ListBase *seqbase, FunctionRef<bool(Strip *)> callback);

/**
 * Expand set by running `strip_query_func()` for each strip, which will be used as reference.
 * Results of these queries will be merged into provided collection.
 *
 * \param seqbase: ListBase in which strips are queried
 * \param strips: set of strips to be expanded
 * \param strip_query_func: query function callback
 */
void iterator_set_expand(const Scene *scene,
                         ListBase *seqbase,
                         VectorSet<Strip *> &strips,
                         void strip_query_func(const Scene *scene,
                                               Strip *strip_reference,
                                               ListBase *seqbase,
                                               VectorSet<Strip *> &strips));
/**
 * Query strips from seqbase. strip_reference is used by query function as filter condition.
 *
 * \param strip_reference: reference strip for query function
 * \param seqbase: ListBase in which strips are queried
 * \param strip_query_func: query function callback
 * \return set of strips
 */
VectorSet<Strip *> query_by_reference(Strip *strip_reference,
                                      const Scene *scene,
                                      ListBase *seqbase,
                                      void strip_query_func(const Scene *scene,
                                                            Strip *strip_reference,
                                                            ListBase *seqbase,
                                                            VectorSet<Strip *> &strips));
/**
 * Query all selected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
VectorSet<Strip *> query_selected_strips(ListBase *seqbase);
/**
 * Query all unselected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
VectorSet<Strip *> query_unselected_strips(ListBase *seqbase);
/**
 * Query all strips in seqbase. This does not include strips nested in meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
VectorSet<Strip *> query_all_strips(ListBase *seqbase);
/**
 * Query all strips in seqbase and nested meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
VectorSet<Strip *> query_all_strips_recursive(const ListBase *seqbase);

/**
 * Query strips at \a timeline_frame in seqbase and nested meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \param timeline_frame: viewed frame
 * \return set of strips
 */
VectorSet<Strip *> query_strips_recursive_at_frame(const Scene *scene,
                                                   const ListBase *seqbase,
                                                   int timeline_frame);

/**
 * Query all effect strips that are directly or indirectly connected to strip_reference.
 * This includes all effects of strip_reference, strips used by another inputs and their effects,
 * so that whole chain is fully independent of other strips.
 *
 * \param strip_reference: reference strip
 * \param seqbase: ListBase in which strips are queried
 * \param strips: set of strips to be filled
 */
void query_strip_effect_chain(const Scene *scene,
                              Strip *reference_strip,
                              ListBase *seqbase,
                              VectorSet<Strip *> &r_strips);

/**
 * Query all connected strips, as well as all effect strips directly or indirectly connected to
 * those connected strips. These steps repeat until there are no new strips to process.
 *
 * \param strip_reference: reference strip
 * \param seqbase: ListBase in which strips are queried
 * \param strips: set of strips to be filled
 */
void query_strip_connected_and_effect_chain(const Scene *scene,
                                            Strip *reference_strip,
                                            ListBase *seqbase,
                                            VectorSet<Strip *> &r_strips);

/**
 * Query strips that will be rendered at \a timeline_frame on all channels less than
 * or equal to \a displayed_channel. This does not recurse into metastrips or sequencer-type scene
 * strips.
 *
 * \note This only returns strips that are directly rendered in the strip stack. Other strips'
 * content may still be indirectly rendered, such as effect inputs, even though they are not
 * included in the returned `VectorSet`. See #must_render_strip.
 * \note Pass \a displayed_channel of 0 to consider all channels.
 */
VectorSet<Strip *> query_rendered_strips(const Scene *scene,
                                         ListBase *channels,
                                         ListBase *seqbase,
                                         int timeline_frame,
                                         int displayed_channel);

/**
 * Strips are sorted from lowest to highest channel.
 * \copydoc #query_rendered_strips
 */
Vector<Strip *> query_rendered_strips_sorted(
    const Scene *scene, ListBase *channels, ListBase *seqbase, int timeline_frame, int chanshown);

/**
 * Check to see whether we cannot skip rendering this strip.
 * Some strips do not need to be directly rendered since they are already indirectly rendered as
 * part of other strips' renders (such as effect strip inputs). These should be skipped to avoid
 * unnecessary re-rendering.
 *
 * \note: Take care when changing the logic of this function since order matters.
 * */
bool must_render_strip(const VectorSet<Strip *> &strip_stack, Strip *target_strip);
}  // namespace blender::seq
