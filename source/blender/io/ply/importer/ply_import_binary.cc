#include "ply_import_binary.hh"
#include "BKE_customdata.h"
#include "BLI_math_vector.h"
#include "ply_import_mesh.hh"
#include <fstream>

namespace blender::io::ply {
Mesh *import_ply_binary(std::ifstream &file, PlyHeader *header, Mesh* mesh)
{
  PlyData data = load_ply_binary(file, header);
  if (data.vertices.size() != 0) {
    return convert_ply_to_mesh(data, mesh);
  }
  return nullptr;
}

template<typename T> T read(std::ifstream& file, bool isBigEndian){
  T returnVal;
  file.read((char *)&returnVal, sizeof(returnVal));
  check_file_errors(file);
  if (isBigEndian) {
    returnVal = swap_bits<T>(returnVal);
  }
  return returnVal;
}

template uint8_t read<uint8_t>(std::ifstream& file, bool isBigEndian);
template int8_t read<int8_t>(std::ifstream& file, bool isBigEndian);
template uint16_t read<uint16_t>(std::ifstream& file, bool isBigEndian);
template int16_t read<int16_t>(std::ifstream& file, bool isBigEndian);
template uint32_t read<uint32_t>(std::ifstream& file, bool isBigEndian);
template int32_t read<int32_t>(std::ifstream& file, bool isBigEndian);
template float read<float>(std::ifstream& file, bool isBigEndian);
template double read<double>(std::ifstream& file, bool isBigEndian);

void check_file_errors(std::ifstream& file) {
  if (file.bad()) {
    printf("Read/Write error on io operation");
  } else if (file.fail()) {
    printf("Logical error on io operation");
  } else if (file.eof()) {
    printf("Reached end of the file");
  }
}

PlyData load_ply_binary(std::ifstream &file, PlyHeader *header)
{
  PlyData data;

  bool isBigEndian = header->type == PlyFormatType::BINARY_BE;
  bool hasNormal = false;
  bool hasColor = false;
  for (int i = 0; i < header->vertex_count; i++) {
    float3 coord {0};
    float3 normal {0};
    float4 color {1};

    for (auto prop : header->properties) {
      if (prop.first == "x") {
        coord.x = read<float>(file, isBigEndian);
      } else if (prop.first == "y") {
        coord.y = read<float>(file, isBigEndian);
      } else if (prop.first == "z") {
        coord.z = read<float>(file, isBigEndian);
      } else if (prop.first == "nx") {
        normal.x = read<float>(file, isBigEndian);
        hasNormal = true;
      } else if (prop.first == "ny") {
        normal.y = read<float>(file, isBigEndian);
      } else if (prop.first == "nz") {
        normal.z = read<float>(file, isBigEndian);
      } else if (prop.first == "red") {
        color.x = read<uint8_t>(file, isBigEndian) / 255.0f;
        hasColor = true;
      } else if (prop.first == "green") {
        color.y = read<uint8_t>(file, isBigEndian) / 255.0f;
      } else if (prop.first == "blue") {
        color.z = read<uint8_t>(file, isBigEndian) / 255.0f;
      } else if (prop.first == "alpha") {
        color.w = read<uint8_t>(file, isBigEndian) / 255.0f;
      } else {
        // We don't support any other properties yet
        switch (prop.second) {
          case CHAR:
            read<int8_t>(file, isBigEndian);
            break;
          case UCHAR:
            read<uint8_t>(file, isBigEndian);
            break;
          case SHORT:
            read<int16_t>(file, isBigEndian);
            break;
          case USHORT:
            read<uint16_t>(file, isBigEndian);
            break;
          case INT:
            read<int32_t>(file, isBigEndian);
            break;
          case UINT:
            read<uint32_t>(file, isBigEndian);
            break;
          case FLOAT:
            read<float>(file, isBigEndian);
            break;
          case DOUBLE:
            read<double>(file, isBigEndian);
            break;
        }
      }
    }

    data.vertices.append(coord);
    if (hasNormal) {
      data.vertex_normals.append(normal);
	}
  	if (hasColor) {
      data.vertex_colors.append(color);
    }
  }

  for (int i = 0; i < header->face_count; i++) {
    // Assume vertex_index_count_type is uchar
    uchar count;
    Vector<uint> vertex_indices;
    file.read((char*)&count, sizeof(count));
    check_file_errors(file);
    for (uchar j = 0; j < count; j++) {
      uint32_t index;
      file.read((char*)&index, sizeof(index));
      check_file_errors(file);
      if (header->type == PlyFormatType::BINARY_BE){
        index = swap_bits<uint32_t>(index);
      }
      vertex_indices.append(index);
    }
    data.faces.append(vertex_indices);
  }

  std::cout << std::endl;

  /*std::cout << "Vertex count: " << data.vertices.size() << std::endl;
  std::cout << "\tFirst: " << data.vertices.first() << std::endl;
  std::cout << "\tLast: " << data.vertices.last() << std::endl;
  std::cout << "Normals count: " << data.vertex_normals.size() << std::endl;
  std::cout << "\tFirst: " << data.vertex_normals.first() << std::endl;
  std::cout << "\tLast: " << data.vertex_normals.last() << std::endl;
  std::cout << "Colours count: " << data.vertex_colors.size() << std::endl;
  std::cout << "\tFirst: " << data.vertex_colors.first() << std::endl;
  std::cout << "\tLast: " << data.vertex_colors.last() << std::endl;*/

  return data;
}

}  // namespace blender::io::ply
