#ifndef BLENDER_PLY_IMPORT_BINARY_HH
#define BLENDER_PLY_IMPORT_BINARY_HH

#include "DNA_mesh_types.h"
#include "ply_data.hh"
#include "BKE_mesh.h"

namespace blender::io::ply {
/**
 * The function that gets called from the importer
 * @param file The PLY file that was opened
 * @param header The information in the PLY header
 * @return The mesh that can be used inside blender
 */
Mesh *import_ply_binary(std::ifstream &file, PlyHeader *header, Mesh* mesh);

/**
 * Loads the information from the PLY file in Big_Endian format to the PlyData datastructure
 * @param file The PLY file that was opened
 * @param header The information in the PLY header
 * @return The PlyData datastructure that can be used for conversion to a Mesh
 */
PlyData load_ply_binary(std::ifstream &file, PlyHeader *header);

void check_file_errors(std::ifstream& file);

template<typename T> T swap_bits(T input)
{
  // In big endian, the most-significant byte is first
  // So, we need to swap the byte order

  //      0            1                            1          0
  // 0b0000_0101 0b0010_0010 in LE would become 0b0010_0010 0b0000_0101
  if (sizeof(T) == 1) {  // This is the easy part
    return input;
  }
  if (sizeof(T) == 2) {
    uint16_t newInput = (uint16_t)input;
    return (T)(((newInput & 0xFF) << 8) | ((newInput >> 8) & 0xFF));
  }
  if (sizeof(T) == 4) {
    uint32_t newInput = *(uint32_t *)&input;  // Cursed pointer magic
    uint32_t first = (newInput & 0xFF) << 24;
    uint32_t second = ((newInput >> 8) & 0xFF) << 16;
    uint32_t third = ((newInput >> 16) & 0xFF) << 8;
    uint32_t fourth = newInput >> 24;
    uint32_t output = first | second | third | fourth;
    T value = *(T *)&output;
    return value;  // Reinterpret the bytes of output as a T value
  }

  if (sizeof(T) == 8) {
    uint64_t newInput = *(uint64_t *)&input;  // Cursed pointer magic
    uint64_t output = 0;
    for (int i = 0; i < 8; i++) {
      output |= ((newInput >> i * 8) & 0xFF) << (56 - i * 8);
    }
    T value = *(T *)&output;
    return value;  // Reinterpret the bytes of output as a T value
  }
}

template<typename T> T read(std::ifstream& file, bool isBigEndian);

}  // namespace blender::io::ply

#endif  // BLENDER_PLY_IMPORT_BINARY_HH
