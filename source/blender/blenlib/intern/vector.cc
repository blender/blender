/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"
#include "BLI_vector.hh"

#include <iostream>

void blender::internal::vector_print_stats(const char *name,
                                           void *address,
                                           int64_t size,
                                           int64_t capacity,
                                           int64_t inlineCapacity,
                                           int64_t memorySize)
{
  std::cout << "Vector Stats: " << name << "\n";
  std::cout << "  Address: " << address << "\n";
  std::cout << "  Elements: " << size << "\n";
  std::cout << "  Capacity: " << capacity << "\n";
  std::cout << "  Inline Capacity: " << inlineCapacity << "\n";

  char memory_size_str[BLI_STR_FORMAT_INT64_BYTE_UNIT_SIZE];
  BLI_str_format_byte_unit(memory_size_str, memorySize, true);
  std::cout << "  Size on Stack: " << memory_size_str << "\n";
}
