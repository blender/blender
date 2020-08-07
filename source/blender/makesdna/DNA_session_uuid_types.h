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

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Is a structure because of the following considerations:
 *
 * - It is not possible to use custom types in DNA members: makesdna does not recognize them.
 * - It allows to add more bits, more than standard fixed-size types can store. For example, if
 *   we ever need to go 128 bits, it is as simple as adding extra 64bit field.
 */
typedef struct SessionUUID {
  /* Never access directly, as it might cause a headache when more bits are needed: if the field
   * is used directly it will not be easy to find all places where partial access is used. */
  uint64_t uuid_;
} SessionUUID;

#ifdef __cplusplus
}
#endif
