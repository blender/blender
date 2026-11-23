/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Internal C++ functions to deal with Actions, Slots, and their runtime data.
 */

#pragma once

#include "BLI_vector.hh"

namespace blender {

struct ID;
struct Main;

namespace animrig {

/**
 * Not placed in the 'internal' namespace, as this type is forward-declared in
 * DNA_action_types.h, and that shouldn't reference the internal namespace.
 */
class SlotRuntime {
 public:
  /**
   * Cache of pointers to the IDs that are animated by this slot.
   *
   * Note that this is a vector for simplicity, as the majority of the slots
   * will have zero or one user. Semantically it's treated as a set: order
   * doesn't matter, and it has no duplicate entries.
   *
   * \note This is NOT thread-safe.
   */
  Vector<ID *> users;

  /**
   * Vector that has the same number of elements as #users, and tracks the owner ID of those users.
   *
   * This is normally the same value, but for embedded IDs, this is the real ID that owns it.
   *
   * This is a temporary hack until #167041 is implemented. That's also why this is tracked in a
   * secondary vector: it allows me to add this now to fix #166948, without changing the public API
   * too much: it is now still possible to return `users` as span or vector.
   */
  Vector<ID *> owners;
};

namespace internal {

/**
 * Rebuild the #SlotRuntime::users cache of all Slots in all Action for a specific `bmain`.
 *
 * The reason that all slot users are re-cached at once is two-fold:
 *
 * 1. Regardless of how many slot caches are rebuilt, this function will need
 *    to loop over all IDs anyway.
 * 2. Deletion of IDs may be hard to detect otherwise. This is a bit of a weak
 *    argument, as if this is not implemented properly (i.e. not un-assigning
 *    the Action first), the 'dirty' flag will also not be set, and thus a
 *    rebuild will not be triggered. In any case, because the rebuild is global,
 *    any subsequent call at least ensures correctness even with such bugs.
 */
void rebuild_slot_user_cache(Main &bmain);

}  // namespace internal

}  // namespace animrig
}  // namespace blender
