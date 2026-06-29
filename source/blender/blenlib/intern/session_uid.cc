/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_session_uid.hh"

#include "atomic_ops.h"

namespace blender {

/* Denotes last used UID.
 * It might eventually overflow, and easiest is to add more bits to it. */
static SessionUID global_session_uid = detail::SESSION_UID_NONE;

SessionUID BLI_session_uid_generate()
{
  SessionUID result;
  result.uid_ = atomic_add_and_fetch_uint64(&global_session_uid.uid_, 1);
  if (!result.is_generated()) {
    /* Happens when the UID overflows.
     *
     * Just request the UID once again, hoping that there are not a lot of high-priority threads
     * which will overflow the counter once again between the previous call and this one.
     *
     * NOTE: It is possible to have collisions after such overflow. */
    result.uid_ = atomic_add_and_fetch_uint64(&global_session_uid.uid_, 1);
  }
  return result;
}

}  // namespace blender
