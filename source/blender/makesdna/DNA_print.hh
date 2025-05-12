/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <iosfwd>

#include "DNA_sdna_type_ids.hh"

struct SDNA;
struct SDNA_Struct;

namespace blender::dna {

/**
 * Print all members of the struct assuming that the data has the given address. This is mainly
 * useful for observing what data is written to a .blend file.
 *
 * \param sdna: Contains reflection information about DNA structs.
 * \param struct_id: The type the data points to. Used to index into `sdna.structs`.
 * \param data: Where the data is stored.
 * \param address: The address that should be printed. Often it's the same as `data`.
 * \param element_num: The number of elements in the array, or 1 if there is only one struct.
 * \param stream: Where to print the output.
 */
void print_structs_at_address(const SDNA &sdna,
                              int struct_id,
                              const void *data,
                              const void *address,
                              int64_t element_num,
                              std::ostream &stream);

/**
 * Prints all members of the struct to stdout.
 */
void print_struct_by_id(int struct_id, const void *data);

}  // namespace blender::dna

/**
 * Prints all members of the struct to stdout.
 *
 * Usage:
 *   DNA_print_struct(bNode, node);
 */
#define DNA_print_struct(struct_name, data_ptr) \
  blender::dna::print_struct_by_id(blender::dna::sdna_struct_id_get<struct_name>(), data_ptr)
