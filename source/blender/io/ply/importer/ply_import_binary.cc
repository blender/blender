/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */
#include "BLI_array.hh"

#include "ply_import_binary.hh"

#include <fstream>

namespace blender::io::ply {
std::unique_ptr<PlyData> import_ply_binary(fstream &file, const PlyHeader *header)
{
  std::unique_ptr<PlyData> data = std::make_unique<PlyData>(load_ply_binary(file, header));
  return data;
}

template<typename T> T read(fstream &file, bool isBigEndian)
{
  T returnVal;
  file.read((char *)&returnVal, sizeof(returnVal));
  check_file_errors(file);
  if (isBigEndian) {
    returnVal = swap_bytes<T>(returnVal);
  }
  return returnVal;
}

template uint8_t read<uint8_t>(fstream &file, bool isBigEndian);
template int8_t read<int8_t>(fstream &file, bool isBigEndian);
template uint16_t read<uint16_t>(fstream &file, bool isBigEndian);
template int16_t read<int16_t>(fstream &file, bool isBigEndian);
template uint32_t read<uint32_t>(fstream &file, bool isBigEndian);
template int32_t read<int32_t>(fstream &file, bool isBigEndian);
template float read<float>(fstream &file, bool isBigEndian);
template double read<double>(fstream &file, bool isBigEndian);

void check_file_errors(const fstream &file)
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

void discard_value(fstream &file, const PlyDataTypes type)
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

PlyData load_ply_binary(fstream &file, const PlyHeader *header)
{
  PlyData data;
  bool isBigEndian = header->type == PlyFormatType::BINARY_BE;

  for (int i = 0; i < header->elements.size(); i++) {
    if (header->elements[i].first == "vertex") {
      /* Import vertices. */
      load_vertex_data(file, header, &data, i);
    }
    else if (header->elements[i].first == "edge") {
      /* Import edges. */
      for (int j = 0; j < header->elements[i].second; j++) {
        std::pair<int, int> vertex_indices;
        for (auto [name, type] : header->properties[i]) {
          if (name == "vertex1") {
            vertex_indices.first = int(read<int32_t>(file, isBigEndian));
          }
          else if (name == "vertex2") {
            vertex_indices.second = int(read<int32_t>(file, isBigEndian));
          }
          else {
            discard_value(file, type);
          }
        }
        data.edges.append(vertex_indices);
      }
    }
    else if (header->elements[i].first == "face") {

      /* Import faces. */
      for (int j = 0; j < header->elements[i].second; j++) {
        /* Assume vertex_index_count_type is uchar. */
        uint8_t count = read<uint8_t>(file, isBigEndian);
        Array<uint> vertex_indices(count);

        /* Loop over the amount of vertex indices in this face. */
        for (uint8_t k = 0; k < count; k++) {
          uint32_t index = read<uint32_t>(file, isBigEndian);
          /* If the face has a vertex index that is outside the range. */
          if (index >= data.vertices.size()) {
            throw std::runtime_error("Vertex index out of bounds");
          }
          vertex_indices[k] = index;
        }
        data.faces.append(vertex_indices);
      }
    }
    else {
      /* Nothing else is supported. */
      for (int j = 0; j < header->elements[i].second; j++) {
        for (auto [name, type] : header->properties[i]) {
          discard_value(file, type);
        }
      }
    }
  }

  return data;
}

void load_vertex_data(fstream &file, const PlyHeader *header, PlyData *r_data, int index)
{
  bool hasNormal = false;
  bool hasColor = false;
  bool hasUv = false;
  bool isBigEndian = header->type == PlyFormatType::BINARY_BE;

  for (int i = 0; i < header->vertex_count; i++) {
    float3 coord{0};
    float3 normal{0};
    float4 color{1};
    float2 uv{0};

    for (auto [name, type] : header->properties[index]) {
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
      else if (name == "s") {
        uv.x = read<float>(file, isBigEndian);
        hasUv = true;
      }
      else if (name == "t") {
        uv.y = read<float>(file, isBigEndian);
      }
      else {
        /* No other properties are supported yet. */
        discard_value(file, type);
      }
    }

    r_data->vertices.append(coord);
    if (hasNormal) {
      r_data->vertex_normals.append(normal);
    }
    if (hasColor) {
      r_data->vertex_colors.append(color);
    }
    if (hasUv) {
      r_data->UV_coordinates.append(uv);
    }
  }
}

}  // namespace blender::io::ply
