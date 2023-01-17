/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BKE_mesh.h"

#include "DNA_mesh_types.h"

#include "IO_ply.h"
#include "ply_data.hh"

namespace blender::io::ply {
/**
 * The function that gets called from the importer.
 * @param file The PLY file that was opened.
 * @param header The information in the PLY header.
 * @return The mesh that can be used inside blender.
 */
Mesh *import_ply_binary(std::ifstream &file, const PlyHeader *header, Mesh *mesh, const PLYImportParams &params);

/**
 * Loads the information from the PLY file in binary format to the PlyData datastructure.
 * @param file The PLY file that was opened.
 * @param header The information in the PLY header.
 * @return The PlyData datastructure that can be used for conversion to a Mesh.
 */
PlyData load_ply_binary(std::ifstream &file, const PlyHeader *header);

void load_vertex_data(std::ifstream &file, const PlyHeader *header, PlyData *r_data, int index);

void check_file_errors(const std::ifstream &file);

void discard_value(std::ifstream &file, const PlyDataTypes type);

template<typename T> T swap_bytes(T input)
{
  // In big endian, the most-significant byte is first.
  // So, we need to swap the byte order.

  //      0            1                            1          0
  // 0b0000_0101 0b0010_0010 in LE would become 0b0010_0010 0b0000_0101
  if (sizeof(T) == 1) {  // This is the easy part.
    return input;
  }
  if (sizeof(T) == 2) {
    uint16_t newInput = uint16_t(input);
    return (T)(((newInput & 0xFF) << 8) | ((newInput >> 8) & 0xFF));
  }
  if (sizeof(T) == 4) {
    // Reinterpret this data as uint32 for easy rearranging of bytes.
    uint32_t newInput = *(uint32_t *)&input;
    uint32_t first = (newInput & 0xFF) << 24;
    uint32_t second = ((newInput >> 8) & 0xFF) << 16;
    uint32_t third = ((newInput >> 16) & 0xFF) << 8;
    uint32_t fourth = newInput >> 24;
    uint32_t output = first | second | third | fourth;
    T value = *(T *)&output;
    return value;  // Reinterpret the bytes of output as a T value.
  }

  if (sizeof(T) == 8) {
    // Reinterpret this data as uint64 for easy rearranging of bytes.
    uint64_t newInput = *(uint64_t *)&input;
    uint64_t output = 0;
    for (int i = 0; i < 8; i++) {
      output |= ((newInput >> i * 8) & 0xFF) << (56 - i * 8);
    }
    T value = *(T *)&output;
    return value;  // Reinterpret the bytes of output as a T value.
  }
}

template<typename T> T read(std::ifstream &file, bool isBigEndian);

}  // namespace blender::io::ply
