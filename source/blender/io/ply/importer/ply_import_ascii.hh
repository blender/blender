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

int3 get_vertex_pos(PlyHeader *header);
int3 get_color_pos(PlyHeader *header);
int3 get_normal_pos(PlyHeader *header);
int get_index(PlyHeader *header, std::string property, PlyDataTypes datatype);
std::vector<std::string> explode(const std::string& str, const char& ch);
}