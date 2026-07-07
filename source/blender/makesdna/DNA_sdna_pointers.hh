/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector.hh"

#include "DNA_sdna_types.h"

namespace blender::dna::pointers {

/** Information about a single pointer in a DNA struct. */
struct PointerInfo {
  /** Offset in bytes from the start of the struct. */
  int64_t offset;
  /** Additional information about the pointer which can be useful for debugging. */
  const char *member_type_name = nullptr;
  const char *name = nullptr;
};

/** All pointers within a DNA struct (including nested structs). */
struct StructInfo {
  /** All pointers in that struct. */
  Vector<PointerInfo> pointers;
  /** Size of the struct in bytes. */
  int size_in_bytes = 0;
};

/**
 * Contains information about where pointers are stored in DNA structs.
 */
class PointersInDNA {
 private:
  /** The SDNA that this class belongs to. */
  const SDNA &sdna_;
  /** Pointer information about all structs. */
  Vector<StructInfo> structs_;

 public:
  explicit PointersInDNA(const SDNA &sdna);

  const StructInfo &get_for_struct(const int struct_nr) const
  {
    return structs_[struct_nr];
  }

 private:
  void gather_pointer_members_recursive(const SDNA_Struct &sdna_struct,
                                        int initial_offset,
                                        StructInfo &r_struct_info) const;
};

}  // namespace blender::dna::pointers
