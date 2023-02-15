/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "ply_data.hh"

namespace blender::io::ply {

/**
 * The function that gets called from the importer.
 * \param file: The PLY file that was opened.
 * \param header: The information in the PLY header.
 * \return The PlyData datastructure that can be used for conversion to a Mesh.
 */
std::unique_ptr<PlyData> import_ply_binary(std::ifstream &file, const PlyHeader *header);

/**
 * Loads the information from the PLY file in binary format to the PlyData datastructure.
 * \param file: The PLY file that was opened.
 * \param header: The information in the PLY header.
 * \return The PlyData datastructure that can be used for conversion to a Mesh.
 */
PlyData load_ply_binary(std::ifstream &file, const PlyHeader *header);

void load_vertex_data(std::ifstream &file, const PlyHeader *header, PlyData *r_data, int index);

void check_file_errors(const std::ifstream &file);

void discard_value(std::ifstream &file, const PlyDataTypes type);

template<typename T> T swap_bytes(T input)
{
  /* In big endian, the most-significant byte is first.
   * So, we need to swap the byte order. */

  /* 0xAC in LE should become 0xCA in BE. */
  if (sizeof(T) == 1) {
    return input;
  }

  if (sizeof(T) == 2) {
    uint16_t newInput = uint16_t(input);
    return (T)(((newInput & 0xFF) << 8) | ((newInput >> 8) & 0xFF));
  }

  if (sizeof(T) == 4) {
    /* Reinterpret this data as uint32 for easy rearranging of bytes. */
    uint32_t newInput = *(uint32_t *)&input;
    uint32_t output = 0;
    for (int i = 0; i < 4; i++) {
      output |= ((newInput >> i * 8) & 0xFF) << (24 - i * 8);
    }
    T value = *(T *)&output;
    return value; /* Reinterpret the bytes of output as a T value. */
  }

  if (sizeof(T) == 8) {
    /* Reinterpret this data as uint64 for easy rearranging of bytes. */
    uint64_t newInput = *(uint64_t *)&input;
    uint64_t output = 0;
    for (int i = 0; i < 8; i++) {
      output |= ((newInput >> i * 8) & 0xFF) << (56 - i * 8);
    }
    T value = *(T *)&output;
    return value; /* Reinterpret the bytes of output as a T value. */
  }
}

template<typename T> T read(std::ifstream &file, bool isBigEndian);

}  // namespace blender::io::ply
