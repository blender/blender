/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_endian_switch.h"
#include "BLI_fileops.hh"

#include "ply_data.hh"

namespace blender::io::ply {

/**
 * The function that gets called from the importer.
 * \param file: The PLY file that was opened.
 * \param header: The information in the PLY header.
 * \return The #PlyData data-structure that can be used for conversion to a #Mesh.
 */
std::unique_ptr<PlyData> import_ply_binary(fstream &file, const PlyHeader *header);

/**
 * Loads the information from the PLY file in binary format to the #PlyData data-structure.
 * \param file: The PLY file that was opened.
 * \param header: The information in the PLY header.
 * \return The #PlyData data-structure that can be used for conversion to a Mesh.
 */
PlyData load_ply_binary(fstream &file, const PlyHeader *header);

void load_vertex_data(fstream &file, const PlyHeader *header, PlyData *r_data, int index);

void check_file_errors(const fstream &file);

void discard_value(fstream &file, const PlyDataTypes type);

template<typename T> T swap_bytes(T input)
{
  /* In big endian, the most-significant byte is first.
   * So, we need to swap the byte order. */

  /* 0xAC in LE should become 0xCA in BE. */
  if (sizeof(T) == 1) {
    return input;
  }

  if constexpr (sizeof(T) == 2) {
    uint16_t value = reinterpret_cast<uint16_t &>(input);
    BLI_endian_switch_uint16(&value);
    return reinterpret_cast<T &>(value);
  }

  if constexpr (sizeof(T) == 4) {
    /* Reinterpret this data as uint32 for easy rearranging of bytes. */
    uint32_t value = reinterpret_cast<uint32_t &>(input);
    BLI_endian_switch_uint32(&value);
    return reinterpret_cast<T &>(value);
  }

  if constexpr (sizeof(T) == 8) {
    /* Reinterpret this data as uint64 for easy rearranging of bytes. */
    uint64_t value = reinterpret_cast<uint64_t &>(input);
    BLI_endian_switch_uint64(&value);
    return reinterpret_cast<T &>(value);
  }
}

template<typename T> T read(fstream &file, bool isBigEndian);

}  // namespace blender::io::ply
