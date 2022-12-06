#ifndef BLENDER_PLY_IMPORT_ASCII_HH
#define BLENDER_PLY_IMPORT_ASCII_HH

#endif  // BLENDER_PLY_IMPORT_ASCII_HH

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
Mesh *import_ply_ascii(std::ifstream &file, PlyHeader *header, Mesh *mesh);

/**
 * Loads the information from the PLY file in Big_Endian format to the PlyData datastructure
 * @param file The PLY file that was opened
 * @param header The information in the PLY header
 * @return The PlyData datastructure that can be used for conversion to a Mesh
 */
PlyData load_ply_ascii(std::ifstream &file, PlyHeader *header);

float3 read_float3_ascii(std::ifstream &file);

uchar3 read_uchar3_ascii(std::ifstream& file);
uchar4 read_uchar4_ascii(std::ifstream& file);

float3 convert_uchar3_float3_ascii(uchar3);
float4 convert_uchar4_float4_ascii(uchar4);
void check_file_errors_ascii(std::ifstream& file);

template<typename T> T read(std::ifstream& file);
}