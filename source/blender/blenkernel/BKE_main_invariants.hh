/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_span.hh"
#include <optional>

struct Main;
struct ID;

/**
 * Makes sure that invariants in original DNA data are maintained after changes.
 *
 * This function has to be idempotent, i.e. after calling it once, additional calls should not
 * modify DNA data further. If it would, it would imply that this function does more than
 * maintaining invariants.
 *
 * This has to be called after any kind of change to original DNA data that may be involved in some
 * of the maintained invariants. It's possible to do multiple changes in a row and then fixing all
 * invariants with a single call in the end. Obviously, the invariants are not maintained in the
 * meantime then and functions relying on them might not work.
 *
 * If this function does not need to do any work to ensure the invariants (that is, no data
 * affecting invariants has been changed), it should not be slower than checking a flag on every
 * data-block in the given bmain.
 *
 * Sometimes, it is known that only a single or very few data-blocks have been changed (e.g. when a
 * node has been inserted in a node tree). Passing in #modified_ids can speed up the function
 * because it may avoid the need to iterate over all data-blocks to find modified data-blocks.
 *
 * Examples of maintained invariants:
 * - Group nodes need to have the correct sockets based on the referenced node group.
 * - The geometry nodes modifier needs to have the correct inputs based on the referenced group.
 */
void BKE_main_ensure_invariants(Main &bmain,
                                std::optional<blender::Span<ID *>> modified_ids = std::nullopt);

/**
 * Same as above but the calling code is less verbose in the common case when only a single
 * data-block has been modified.
 */
void BKE_main_ensure_invariants(Main &bmain, ID &modified_id);
