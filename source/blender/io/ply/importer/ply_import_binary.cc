#include "ply_import_binary.hh"
#include "BKE_customdata.h"
#include "BLI_math_vector.h"
#include "ply_import_mesh.hh"
#include <fstream>

namespace blender::io::ply {
Mesh *import_ply_binary(std::ifstream &file, const PlyHeader *header, Mesh *mesh)
{
  PlyData data = load_ply_binary(file, header);
  if (!data.vertices.is_empty()) {
    return convert_ply_to_mesh(data, mesh);
  }
  return nullptr;
}

template<typename T> T read(std::ifstream &file, bool isBigEndian)
{
  T returnVal;
  file.read((char *)&returnVal, sizeof(returnVal));
  check_file_errors(file);
  if (isBigEndian) {
    returnVal = swap_bytes<T>(returnVal);
  }
  return returnVal;
}

template uint8_t read<uint8_t>(std::ifstream &file, bool isBigEndian);
template int8_t read<int8_t>(std::ifstream &file, bool isBigEndian);
template uint16_t read<uint16_t>(std::ifstream &file, bool isBigEndian);
template int16_t read<int16_t>(std::ifstream &file, bool isBigEndian);
template uint32_t read<uint32_t>(std::ifstream &file, bool isBigEndian);
template int32_t read<int32_t>(std::ifstream &file, bool isBigEndian);
template float read<float>(std::ifstream &file, bool isBigEndian);
template double read<double>(std::ifstream &file, bool isBigEndian);

void check_file_errors(const std::ifstream &file)
{
  if (file.bad()) {
    throw std::ios_base::failure("Read/Write error on io operation");
  }
  if (file.fail()) {
    throw std::ios_base::failure("Logical error on io operation");
  }
  if (file.eof()) {
    throw std::ios_base::failure("Reached end of the file");
  }
}

void discard_value(std::ifstream &file, const PlyDataTypes type)
{
  switch (type) {
    case CHAR:
      read<int8_t>(file, false);
      break;
    case UCHAR:
      read<uint8_t>(file, false);
      break;
    case SHORT:
      read<int16_t>(file, false);
      break;
    case USHORT:
      read<uint16_t>(file, false);
      break;
    case INT:
      read<int32_t>(file, false);
      break;
    case UINT:
      read<uint32_t>(file, false);
      break;
    case FLOAT:
      read<float>(file, false);
      break;
    case DOUBLE:
      read<double>(file, false);
      break;
  }
}

PlyData load_ply_binary(std::ifstream &file, const PlyHeader *header)
{
  PlyData data;

  bool isBigEndian = header->type == PlyFormatType::BINARY_BE;
  bool hasNormal = false;
  bool hasColor = false;
  for (int i = 0; i < header->vertex_count; i++) {
    float3 coord{0};
    float3 normal{0};
    float4 color{1};

    for (auto [name, type] : header->properties) {
      if (name == "x") {
        coord.x = read<float>(file, isBigEndian);
      }
      else if (name == "y") {
        coord.y = read<float>(file, isBigEndian);
      }
      else if (name == "z") {
        coord.z = read<float>(file, isBigEndian);
      }
      else if (name == "nx") {
        normal.x = read<float>(file, isBigEndian);
        hasNormal = true;
      }
      else if (name == "ny") {
        normal.y = read<float>(file, isBigEndian);
      }
      else if (name == "nz") {
        normal.z = read<float>(file, isBigEndian);
      }
      else if (name == "red") {
        color.x = read<uint8_t>(file, isBigEndian) / 255.0f;
        hasColor = true;
      }
      else if (name == "green") {
        color.y = read<uint8_t>(file, isBigEndian) / 255.0f;
      }
      else if (name == "blue") {
        color.z = read<uint8_t>(file, isBigEndian) / 255.0f;
      }
      else if (name == "alpha") {
        color.w = read<uint8_t>(file, isBigEndian) / 255.0f;
      }
      else {
        // We don't support any other properties yet.
        discard_value(file, type);
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
    // Assume vertex_index_count_type is uchar.
    uint8_t count = read<uint8_t>(file, isBigEndian);
    Vector<uint> vertex_indices;

    // Loop over the amount of vertex indices in this face.
    for (uint8_t j = 0; j < count; j++) {
      uint32_t index = read<uint32_t>(file, isBigEndian);
      vertex_indices.append(index);
    }
    data.faces.append(vertex_indices);
  }

  return data;
}

}  // namespace blender::io::ply
