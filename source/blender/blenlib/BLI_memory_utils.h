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

/* Use a define instead of `#pragma once` because of `BLI_utildefines.h` */
#ifndef __BLI_MEMORY_UTILS_H__
#define __BLI_MEMORY_UTILS_H__

/** \file
 * \ingroup bli
 * \brief Generic memory manipulation API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* it may be defined already */
#ifndef __BLI_UTILDEFINES_H__
bool BLI_memory_is_zero(const void *arr, const size_t size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MEMORY_UTILS_H__ */
