/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mem
 */

#pragma once

namespace mem_guarded::internal {

enum class AllocationType {
  /** Allocation is handled through 'C type' alloc/free calls. */
  ALLOC_FREE,
  /** Allocation is handled through 'C++ type' new/delete calls. */
  NEW_DELETE,
};

/** Internal implementation of #MEM_freeN, exposed because #MEM_delete needs access to it. */
extern void (*mem_freeN_ex)(void *vmemh, AllocationType allocation_type);

/** Internal implementation of #MEM_mallocN_aligned, exposed because #MEM_new needs access to it.
 */
extern void *(*mem_mallocN_aligned_ex)(size_t len,
                                       size_t alignment,
                                       const char *str,
                                       AllocationType allocation_type);

}  // namespace mem_guarded::internal
