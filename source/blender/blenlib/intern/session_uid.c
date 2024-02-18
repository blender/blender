/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_session_uid.h"

#include "atomic_ops.h"

/* Special value which indicates the UID has not been assigned yet. */
#define BLI_session_uid_NONE 0

static const SessionUID global_session_uid_none = {BLI_session_uid_NONE};

/* Denotes last used UID.
 * It might eventually overflow, and easiest is to add more bits to it. */
static SessionUID global_session_uid = {BLI_session_uid_NONE};

SessionUID BLI_session_uid_generate(void)
{
  SessionUID result;
  result.uid_ = atomic_add_and_fetch_uint64(&global_session_uid.uid_, 1);
  if (!BLI_session_uid_is_generated(&result)) {
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

bool BLI_session_uid_is_generated(const SessionUID *uid)
{
  return !BLI_session_uid_is_equal(uid, &global_session_uid_none);
}

bool BLI_session_uid_is_equal(const SessionUID *lhs, const SessionUID *rhs)
{
  return lhs->uid_ == rhs->uid_;
}

uint64_t BLI_session_uid_hash_uint64(const SessionUID *uid)
{
  return uid->uid_;
}

uint BLI_session_uid_ghash_hash(const void *uid_v)
{
  const SessionUID *uid = (const SessionUID *)uid_v;
  return uid->uid_ & 0xffffffff;
}

bool BLI_session_uid_ghash_compare(const void *lhs_v, const void *rhs_v)
{
  const SessionUID *lhs = (const SessionUID *)lhs_v;
  const SessionUID *rhs = (const SessionUID *)rhs_v;
  return !BLI_session_uid_is_equal(lhs, rhs);
}
