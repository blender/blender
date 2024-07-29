/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_memutil
 */

#include "MEM_alloc_string_storage.hh"
#include "MEM_guardedalloc.h"

namespace intern::memutil::internal {

AllocStringStorageContainer &ensure_storage_container()
{
  static thread_local AllocStringStorageContainer &storage =
      MEM_construct_leak_detection_data<AllocStringStorageContainer>();
  return storage;
}

}  // namespace intern::memutil::internal
