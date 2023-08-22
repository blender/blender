/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_hash_tables.hh"
#include "BLI_string.h"

#include <iostream>

void blender::HashTableStats::print(StringRef name) const
{
  std::cout << "Hash Table Stats: " << name << "\n";
  std::cout << "  Address: " << address_ << "\n";
  std::cout << "  Total Slots: " << capacity_ << "\n";
  std::cout << "  Occupied Slots:  " << size_ << " (" << load_factor_ * 100.0f << " %)\n";
  std::cout << "  Removed Slots: " << removed_amount_ << " (" << removed_load_factor_ * 100.0f
            << " %)\n";

  char memory_size_str[BLI_STR_FORMAT_INT64_BYTE_UNIT_SIZE];
  BLI_str_format_byte_unit(memory_size_str, size_in_bytes_, true);
  std::cout << "  Size: ~" << memory_size_str << "\n";
  std::cout << "  Size per Slot: " << size_per_element_ << " bytes\n";

  std::cout << "  Average Collisions: " << average_collisions_ << "\n";
  for (int64_t collision_count : keys_by_collision_count_.index_range()) {
    std::cout << "  " << collision_count
              << " Collisions: " << keys_by_collision_count_[collision_count] << "\n";
  }
}
