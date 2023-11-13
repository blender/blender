/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
bool BLI_memory_is_zero(const void *arr, size_t size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MEMORY_UTILS_H__ */
