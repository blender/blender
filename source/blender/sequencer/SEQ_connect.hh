/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_vector_set.hh"
#include "DNA_listBase.h"

namespace blender {

struct Strip;
struct StripConnection;

namespace seq {

void connections_duplicate(ListBaseT<StripConnection> *connections_dst,
                           ListBaseT<StripConnection> *connections_src);

/**
 * Completely disconnect the strip(s) from any connections with other strips. This function also
 * frees the allocated memory as necessary. Returns false if any of the strips were not already
 * connected.
 */
bool disconnect(Strip *strip);
bool disconnect(VectorSet<Strip *> &strip_list);

/**
 * Ensure that the \a strip has only bidirectional connections (expected behavior).
 */
void cut_one_way_connections(Strip *strip);
/**
 * Only remove the connections from strips inside \a strip_list to outside the list.
 * Strips within \a strip_list keep their connections if they existed before.
 */
void cut_outside_connections(const VectorSet<Strip *> &strip_list);

/**
 * Connect strips so that they may be selected together. Any connections the
 * strips already have will be severed before reconnection.
 */
void connect(Strip *strip1, Strip *strip2);
void connect(VectorSet<Strip *> &strip_list);

/**
 * Returns a list of strips that the \a strip is connected to.
 * NOTE: This does not include \a strip itself.
 * This list is empty if \a strip is not connected.
 */
VectorSet<Strip *> connected_strips_get(const Strip *strip);

/**
 * Check whether \a strip has any connections.
 */
bool is_strip_connected(const Strip *strip);

/**
 * Check whether the \a strip_list are a single connection "group", that is, they are all
 * connected to each other and there are no outside connections.
 */
bool are_strips_connected_together(VectorSet<Strip *> &strip_list);

}  // namespace seq
}  // namespace blender
