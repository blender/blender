/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_memory_counter.hh"

namespace blender {

MemoryCounter::~MemoryCounter()
{
  for (const ImplicitSharingInfo *sharing_info : counted_shared_data_) {
    sharing_info->remove_weak_user_and_delete_if_last();
  }
}

void MemoryCounter::add_shared(const ImplicitSharingInfo *sharing_info,
                               const FunctionRef<void(MemoryCounter &memory)> count_fn)
{
  if (!sharing_info) {
    /* Data is not actually shared. */
    count_fn(*this);
    return;
  }
  if (!counted_shared_data_.add(sharing_info)) {
    /* Data was counted before, avoid counting it again. */
    return;
  }
  sharing_info->add_weak_user();
  /* Count into the `this` for now. In the future we could pass in a separate #MemoryCounter here
   * if we needed to know the amount of memory used by each shared data. */
  count_fn(*this);
}

void MemoryCounter::add_shared(const ImplicitSharingInfo *sharing_info, const int64_t bytes)
{
  this->add_shared(sharing_info, [&](MemoryCounter &shared_memory) { shared_memory.add(bytes); });
}

}  // namespace blender
