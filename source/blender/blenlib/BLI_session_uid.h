/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Functions for generating and handling "Session UIDs".
 *
 * Note that these are not true universally-unique identifiers, but only unique during the current
 * Blender session.
 *
 * For true UUIDs, see `BLI_uuid.h`.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_session_uid_types.h"

/** Generate new UID which is unique throughout the Blender session. */
SessionUID BLI_session_uid_generate(void);

/** Check whether the UID is properly generated. */
bool BLI_session_uid_is_generated(const SessionUID *uid);

/** Check whether two UIDs are identical. */
bool BLI_session_uid_is_equal(const SessionUID *lhs, const SessionUID *rhs);

uint64_t BLI_session_uid_hash_uint64(const SessionUID *uid);

/* Utility functions to make it possible to create GHash/GSet with UID as a key. */

uint BLI_session_uid_ghash_hash(const void *uid_v);
bool BLI_session_uid_ghash_compare(const void *lhs_v, const void *rhs_v);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender {

inline const bool operator==(const SessionUID &lhs, const SessionUID &rhs)
{
  return BLI_session_uid_is_equal(&lhs, &rhs);
}

template<typename T> struct DefaultHash;

template<> struct DefaultHash<SessionUID> {
  uint64_t operator()(const SessionUID &value) const
  {
    return BLI_session_uid_hash_uint64(&value);
  }
};

}  // namespace blender

#endif
