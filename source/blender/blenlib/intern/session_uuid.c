/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bli
 */

#include "BLI_session_uuid.h"

#include "BLI_utildefines.h"

#include "atomic_ops.h"

/* Special value which indicates the UUID has not been assigned yet. */
#define BLI_SESSION_UUID_NONE 0

static const SessionUUID global_session_uuid_none = {BLI_SESSION_UUID_NONE};

/* Denotes last used UUID.
 * It might eventually overflow, and easiest is to add more bits to it. */
static SessionUUID global_session_uuid = {BLI_SESSION_UUID_NONE};

SessionUUID BLI_session_uuid_generate(void)
{
  SessionUUID result;
  result.uuid_ = atomic_add_and_fetch_uint64(&global_session_uuid.uuid_, 1);
  if (!BLI_session_uuid_is_generated(&result)) {
    /* Happens when the UUID overflows.
     *
     * Just request the UUID once again, hoping that there are not a lot of high-priority threads
     * which will overflow the counter once again between the previous call and this one.
     *
     * NOTE: It is possible to have collisions after such overflow. */
    result.uuid_ = atomic_add_and_fetch_uint64(&global_session_uuid.uuid_, 1);
  }
  return result;
}

bool BLI_session_uuid_is_generated(const SessionUUID *uuid)
{
  return !BLI_session_uuid_is_equal(uuid, &global_session_uuid_none);
}

bool BLI_session_uuid_is_equal(const SessionUUID *lhs, const SessionUUID *rhs)
{
  return lhs->uuid_ == rhs->uuid_;
}

uint64_t BLI_session_uuid_hash_uint64(const SessionUUID *uuid)
{
  return uuid->uuid_;
}

uint BLI_session_uuid_ghash_hash(const void *uuid_v)
{
  const SessionUUID *uuid = (const SessionUUID *)uuid_v;
  return uuid->uuid_ & 0xffffffff;
}

bool BLI_session_uuid_ghash_compare(const void *lhs_v, const void *rhs_v)
{
  const SessionUUID *lhs = (const SessionUUID *)lhs_v;
  const SessionUUID *rhs = (const SessionUUID *)rhs_v;
  return BLI_session_uuid_is_equal(lhs, rhs);
}
