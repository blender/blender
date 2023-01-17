/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BLI_math_vector.h"

#include "ply_functions.hh"
#include "ply_import_ascii.hh"
#include "ply_import_mesh.hh"

#include <algorithm>
#include <fstream>

namespace blender::io::ply {

Mesh *import_ply_ascii(std::ifstream &file, PlyHeader *header, Mesh *mesh)
{
  PlyData data = load_ply_ascii(file, header);
  if (!data.vertices.is_empty()) {
    return convert_ply_to_mesh(data, mesh);
  }
  return nullptr;
}

PlyData load_ply_ascii(std::ifstream &file, PlyHeader *header)
{
  PlyData data;
  // Check if header contains alpha.
  std::pair<std::string, PlyDataTypes> alpha = {"alpha", PlyDataTypes::UCHAR};
  bool hasAlpha = std::find(header->properties[0].begin(), header->properties[0].end(), alpha) !=
                  header->properties[0].end();

  // Check if header contains colors.
  std::pair<std::string, PlyDataTypes> red = {"red", PlyDataTypes::UCHAR};
  bool hasColor = std::find(header->properties[0].begin(), header->properties[0].end(), red) !=
                  header->properties[0].end();

  // Check if header contains normals.
  std::pair<std::string, PlyDataTypes> normalx = {"nx", PlyDataTypes::FLOAT};
  bool hasNormals = std::find(header->properties[0].begin(),
                              header->properties[0].end(),
                              normalx) != header->properties[0].end();

  int3 vertexpos = get_vertex_pos(header);
  int alphapos;
  int3 colorpos;
  int3 normalpos;

  if (hasAlpha) {
    alphapos = get_index(header, "alpha", PlyDataTypes::UCHAR);
  }

  if (hasColor) {
    // x=red, y=green, z=blue
    colorpos = get_color_pos(header);
  }

  if (hasNormals) {
    normalpos = get_normal_pos(header);
  }

  for (int i = 0; i < header->vertex_count; i++) {
    std::string line;
    safe_getline(file, line);
    std::vector<std::string> value_arr = explode(line, ' ');

    // Vertex coords
    float3 vertex3;
    vertex3.x = std::stof(value_arr.at(vertexpos.x));
    vertex3.y = std::stof(value_arr.at(vertexpos.y));
    vertex3.z = std::stof(value_arr.at(vertexpos.z));

    data.vertices.append(vertex3);

    // Vertex colors
    if (hasColor) {
      float4 colors4;
      colors4.x = std::stof(value_arr.at(colorpos.x)) / 255.0f;
      colors4.y = std::stof(value_arr.at(colorpos.y)) / 255.0f;
      colors4.z = std::stof(value_arr.at(colorpos.z)) / 255.0f;
      if (hasAlpha) {
        colors4.w = std::stof(value_arr.at(alphapos)) / 255.0f;
      }
      else {
        colors4.w = 1.0f;
      }

      data.vertex_colors.append(colors4);
    }

    // If normals
    if (hasNormals) {
      float3 normals3;
      normals3.x = std::stof(value_arr.at(normalpos.x));
      normals3.y = std::stof(value_arr.at(normalpos.y));
      normals3.z = std::stof(value_arr.at(normalpos.z));

      data.vertex_normals.append(normals3);
    }
  }
  for (int i = 0; i < header->face_count; i++) {
    std::string line;
    getline(file, line);
    std::vector<std::string> value_arr = explode(line, ' ');
    Vector<uint> vertex_indices;

    for (int j = 1; j <= std::stoi(value_arr.at(0)); j++) {
      vertex_indices.append(std::stoi(value_arr.at(j)));
    }
    data.faces.append(vertex_indices);
  }

  return data;
}

int3 get_vertex_pos(PlyHeader *header)
{
  int3 vertexPos;
  vertexPos.x = get_index(header, "x", PlyDataTypes::FLOAT);
  vertexPos.y = get_index(header, "y", PlyDataTypes::FLOAT);
  vertexPos.z = get_index(header, "z", PlyDataTypes::FLOAT);

  return vertexPos;
}

int3 get_color_pos(PlyHeader *header)
{
  int3 vertexPos;
  vertexPos.x = get_index(header, "red", PlyDataTypes::UCHAR);
  vertexPos.y = get_index(header, "green", PlyDataTypes::UCHAR);
  vertexPos.z = get_index(header, "blue", PlyDataTypes::UCHAR);

  return vertexPos;
}

int3 get_normal_pos(PlyHeader *header)
{
  int3 vertexPos;
  vertexPos.x = get_index(header, "nx", PlyDataTypes::FLOAT);
  vertexPos.y = get_index(header, "ny", PlyDataTypes::FLOAT);
  vertexPos.z = get_index(header, "nz", PlyDataTypes::FLOAT);

  return vertexPos;
}

int get_index(PlyHeader *header, std::string property, PlyDataTypes datatype)
{
  std::pair<std::string, PlyDataTypes> pair = {property, datatype};
  auto it = std::find(header->properties[0].begin(), header->properties[0].end(), pair);
  return (int)(it - header->properties[0].begin());
}

std::vector<std::string> explode(const std::string_view &str, const char &ch)
{
  std::string next;
  std::vector<std::string> result;

  // For each character in the string...
  for (auto c : str) {
    // If we've hit the terminal character...
    if (c == ch) {
      // If we have some characters accumulated...
      if (!next.empty()) {
        // Add them to the result vector.
        result.push_back(next);
        next.clear();
      }
    }
    else {
      // Accumulate the next character into the sequence.
      next += c;
    }
  }

  if (!next.empty()) {
    result.push_back(next);
  }

  return result;
}

}  // namespace blender::io::ply
