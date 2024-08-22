/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_vector_set.hh"

struct Sequence;
struct ListBase;

void SEQ_connections_duplicate(ListBase *connections_dst, ListBase *connections_src);

/**
 * Disconnect the strip(s) from any connections with other strips. This function also
 * frees the allocated memory as necessary. Returns false if any of the strips were not already
 * connected.
 */
bool SEQ_disconnect(Sequence *seq);
bool SEQ_disconnect(blender::VectorSet<Sequence *> &seq_list);

/**
 * Ensure that the strip has only bidirectional connections (expected behavior).
 */
void SEQ_cut_one_way_connections(Sequence *seq);

/**
 * Connect strips so that they may be selected together. Any connections the
 * strips already have will be severed before reconnection.
 */
void SEQ_connect(Sequence *seq1, Sequence *seq2);
void SEQ_connect(blender::VectorSet<Sequence *> &seq_list);

/**
 * Returns a list of strips that the `seq` is connected to.
 * NOTE: This does not include `seq` itself.
 * This list is empty if `seq` is not connected.
 */
blender::VectorSet<Sequence *> SEQ_get_connected_strips(const Sequence *seq);

/**
 * Check whether a strip has any connections.
 */
bool SEQ_is_strip_connected(const Sequence *seq);

/**
 * Check whether the list of strips are a single connection "group", that is, they are all
 * connected to each other and there are no outside connections.
 */
bool SEQ_are_strips_connected_together(blender::VectorSet<Sequence *> &seq_list);
