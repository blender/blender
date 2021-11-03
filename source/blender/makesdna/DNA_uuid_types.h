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
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Universally Unique Identifier according to RFC4122.
 *
 * Cannot be named simply `UUID`, because Windows already defines that type.
 */
typedef struct bUUID {
  uint32_t time_low;
  uint16_t time_mid;
  uint16_t time_hi_and_version;
  uint8_t clock_seq_hi_and_reserved;
  uint8_t clock_seq_low;
  uint8_t node[6];
} bUUID;

/**
 * Memory required for a string representation of a UUID according to RFC4122.
 * This is 36 characters for the string + a trailing zero byte.
 */
#define UUID_STRING_LEN 37

#ifdef __cplusplus
}
#endif
