/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_sys_types.h"

namespace blender {

/**
 * \brief Universally Unique Identifier according to RFC4122.
 *
 * Cannot be named simply `UUID`, because Windows already defines that type.
 */
struct bUUID {
  uint32_t time_low = 0;
  uint16_t time_mid = 0;
  uint16_t time_hi_and_version = 0;
  uint8_t clock_seq_hi_and_reserved = 0;
  uint8_t clock_seq_low = 0;
  uint8_t node[6] = {};
};

/**
 * Memory required for a string representation of a UUID according to RFC4122.
 * This is 36 characters for the string + a trailing zero byte.
 */
#define UUID_STRING_SIZE 37

}  // namespace blender
