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
 * Query strips that are rendered at \a timeline_frame when \a displayed channel is viewed
 *
 * \param seqbase: ListBase in which strips are queried
 * \param timeline_frame: viewed frame
 * \param displayed_channel: viewed channel. when set to 0, no channel filter is applied
 * \return set of strips
 */
VectorSet<Strip *> query_rendered_strips(const Scene *scene,
                                         ListBase *channels,
                                         ListBase *seqbase,
                                         int timeline_frame,
                                         int displayed_channel);

bool must_render_strip(const VectorSet<Strip *> &strips, Strip *strip);
}  // namespace blender::seq
