/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Functions for generating and handling "Session UUIDs".
 *
 * Note that these are not true universally-unique identifiers, but only unique during the current
 * Blender session.
 *
 * For true UUIDs, see `BLI_uuid.h`.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_session_uuid_types.h"

/** Generate new UUID which is unique throughout the Blender session. */
SessionUUID BLI_session_uuid_generate(void);

/** Check whether the UUID is properly generated. */
bool BLI_session_uuid_is_generated(const SessionUUID *uuid);

/** Check whether two UUIDs are identical. */
bool BLI_session_uuid_is_equal(const SessionUUID *lhs, const SessionUUID *rhs);

uint64_t BLI_session_uuid_hash_uint64(const SessionUUID *uuid);

/* Utility functions to make it possible to create GHash/GSet with UUID as a key. */

uint BLI_session_uuid_ghash_hash(const void *uuid_v);
bool BLI_session_uuid_ghash_compare(const void *lhs_v, const void *rhs_v);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender {

inline const bool operator==(const SessionUUID &lhs, const SessionUUID &rhs)
{
  return BLI_session_uuid_is_equal(&lhs, &rhs);
}

template<typename T> struct DefaultHash;

template<> struct DefaultHash<SessionUUID> {
  uint64_t operator()(const SessionUUID &value) const
  {
    return BLI_session_uuid_hash_uint64(&value);
  }
};

}  // namespace blender

#endif
