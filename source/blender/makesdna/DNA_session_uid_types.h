/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_sys_types.hh"

namespace blender {

/**
 * Is a structure because of the following considerations:
 *
 * - It is not possible to use custom types in DNA members: `makesdna` does not recognize them.
 * - It allows to add more bits, more than standard fixed-size types can store. For example, if
 *   we ever need to go 128 bits, it is as simple as adding extra 64bit field.
 */
struct SessionUID {
  /**
   * Never access directly, as it might cause a headache when more bits are needed: if the field
   * is used directly it will not be easy to find all places where partial access is used.
   */
  uint64_t uid_ = 0;

#ifdef __cplusplus
  friend bool operator==(const SessionUID &a, const SessionUID &b)
  {
    return a.uid_ == b.uid_;
  }
  uint64_t hash() const
  {
    return uid_;
  }

  /** Check whether the UID is properly generated. */
  bool is_generated() const;
#endif
};

#ifdef __cplusplus
namespace detail {
/* Special value which indicates the UID has not been assigned yet. */
constexpr SessionUID SESSION_UID_NONE = {0};
}  // namespace detail

inline bool SessionUID::is_generated() const
{
  return *this != blender::detail::SESSION_UID_NONE;
}

#endif

}  // namespace blender
