/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_vector_set.hh"

struct Sequence;

/**
 * Callback format for the for_each function below.
 */
using SeqForEachFunc = bool (*)(Sequence *seq, void *user_data);

/**
 * Utility function to recursively iterate through all sequence strips in a `seqbase` list.
 * Uses callback to do operations on each sequence element.
 * The callback can stop the iteration if needed.
 *
 * \param seqbase: #ListBase of sequences to be iterated over.
 * \param callback: query function callback, returns false if iteration should stop.
 * \param user_data: pointer to user data that can be used in the callback function.
 */
void SEQ_for_each_callback(ListBase *seqbase, SeqForEachFunc callback, void *user_data);

/**
 * Expand set by running `seq_query_func()` for each strip, which will be used as reference.
 * Results of these queries will be merged into provided collection.
 *
 * \param seqbase: ListBase in which strips are queried
 * \param strips: set of strips to be expanded
 * \param seq_query_func: query function callback
 */
void SEQ_iterator_set_expand(const Scene *scene,
                             ListBase *seqbase,
                             blender::VectorSet<Sequence *> &strips,
                             void seq_query_func(const Scene *scene,
                                                 Sequence *seq_reference,
                                                 ListBase *seqbase,
                                                 blender::VectorSet<Sequence *> &strips));
/**
 * Query strips from seqbase. seq_reference is used by query function as filter condition.
 *
 * \param seq_reference: reference strip for query function
 * \param seqbase: ListBase in which strips are queried
 * \param seq_query_func: query function callback
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_by_reference(
    Sequence *seq_reference,
    const Scene *scene,
    ListBase *seqbase,
    void seq_query_func(const Scene *scene,
                        Sequence *seq_reference,
                        ListBase *seqbase,
                        blender::VectorSet<Sequence *> &strips));
/**
 * Query all selected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_selected_strips(ListBase *seqbase);
/**
 * Query all unselected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_unselected_strips(ListBase *seqbase);
/**
 * Query all strips in seqbase. This does not include strips nested in meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_all_strips(ListBase *seqbase);
/**
 * Query all strips in seqbase and nested meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_all_strips_recursive(const ListBase *seqbase);
/**
 * Query all meta strips in seqbase and nested meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of meta strips
 */
blender::VectorSet<Sequence *> SEQ_query_all_meta_strips_recursive(const ListBase *seqbase);

/**
 * Query all effect strips that are directly or indirectly connected to seq_reference.
 * This includes all effects of seq_reference, strips used by another inputs and their effects, so
 * that whole chain is fully independent of other strips.
 *
 * \param seq_reference: reference strip
 * \param seqbase: ListBase in which strips are queried
 * \param strips: set of strips to be filled
 */
void SEQ_query_strip_effect_chain(const Scene *scene,
                                  Sequence *seq_reference,
                                  ListBase *seqbase,
                                  blender::VectorSet<Sequence *> &strips);

/**
 * Query strips that are rendered at \a timeline_frame when \a displayed channel is viewed
 *
 * \param seqbase: ListBase in which strips are queried
 * \param timeline_frame: viewed frame
 * \param displayed_channel: viewed channel. when set to 0, no channel filter is applied
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_rendered_strips(const Scene *scene,
                                                         ListBase *channels,
                                                         ListBase *seqbase,
                                                         int timeline_frame,
                                                         int displayed_channel);
