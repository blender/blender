/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_import_ascii.hh"
#include "ply_functions.hh"

#include <algorithm>
#include <fstream>

namespace blender::io::ply {

std::unique_ptr<PlyData> import_ply_ascii(std::ifstream &file, PlyHeader *header)
{
  std::unique_ptr<PlyData> data = std::make_unique<PlyData>();
  *data = load_ply_ascii(file, header);
  return data;
}

PlyData load_ply_ascii(std::ifstream &file, const PlyHeader *header)
{
  PlyData data;
  /* Check if header contains alpha. */
  std::pair<std::string, PlyDataTypes> alpha = {"alpha", PlyDataTypes::UCHAR};
  bool hasAlpha = std::find(header->properties[0].begin(), header->properties[0].end(), alpha) !=
                  header->properties[0].end();

  /* Check if header contains colors. */
  std::pair<std::string, PlyDataTypes> red = {"red", PlyDataTypes::UCHAR};
  bool hasColor = std::find(header->properties[0].begin(), header->properties[0].end(), red) !=
                  header->properties[0].end();

  /* Check if header contains normals. */
  std::pair<std::string, PlyDataTypes> normalx = {"nx", PlyDataTypes::FLOAT};
  bool hasNormals = std::find(header->properties[0].begin(),
                              header->properties[0].end(),
                              normalx) != header->properties[0].end();

  /* Check if header contains uv data. */
  std::pair<std::string, PlyDataTypes> uv = {"s", PlyDataTypes::FLOAT};
  bool hasUv = std::find(header->properties[0].begin(), header->properties[0].end(), uv) !=
               header->properties[0].end();

  int3 vertexIndex = get_vertex_index(header);
  int alphaIndex;
  int3 colorIndex;
  int3 normalIndex;
  int2 uvIndex;

  if (hasAlpha) {
    alphaIndex = get_index(header, "alpha", PlyDataTypes::UCHAR);
  }

  if (hasColor) {
    /* x=red, y=green, z=blue */
    colorIndex = get_color_index(header);
  }

  if (hasNormals) {
    normalIndex = get_normal_index(header);
  }

  if (hasUv) {
    uvIndex = get_uv_index(header);
  }

  for (int i = 0; i < header->vertex_count; i++) {
    std::string line;
    safe_getline(file, line);
    Vector<std::string> value_vec = explode(line, ' ');

    /* Vertex coords */
    float3 vertex3;
    vertex3.x = std::stof(value_vec[vertexIndex.x]);
    vertex3.y = std::stof(value_vec[vertexIndex.y]);
    vertex3.z = std::stof(value_vec[vertexIndex.z]);

    data.vertices.append(vertex3);

    /* Vertex colors */
    if (hasColor) {
      float4 colors4;
      colors4.x = std::stof(value_vec[colorIndex.x]) / 255.0f;
      colors4.y = std::stof(value_vec[colorIndex.y]) / 255.0f;
      colors4.z = std::stof(value_vec[colorIndex.z]) / 255.0f;
      if (hasAlpha) {
        colors4.w = std::stof(value_vec[alphaIndex]) / 255.0f;
      }
      else {
        colors4.w = 1.0f;
      }

      data.vertex_colors.append(colors4);
    }

    /* If normals */
    if (hasNormals) {
      float3 normals3;
      normals3.x = std::stof(value_vec[normalIndex.x]);
      normals3.y = std::stof(value_vec[normalIndex.y]);
      normals3.z = std::stof(value_vec[normalIndex.z]);

      data.vertex_normals.append(normals3);
    }

    /* If uv */
    if (hasUv) {
      float2 uvmap;
      uvmap.x = std::stof(value_vec[uvIndex.x]);
      uvmap.y = std::stof(value_vec[uvIndex.y]);

      data.UV_coordinates.append(uvmap);
    }
  }
  for (int i = 0; i < header->face_count; i++) {
    std::string line;
    getline(file, line);
    Vector<std::string> value_vec = explode(line, ' ');
    Vector<uint> vertex_indices;

    for (int j = 1; j <= std::stoi(value_vec[0]); j++) {
      vertex_indices.append(std::stoi(value_vec[j]));
    }
    data.faces.append(vertex_indices);
  }

  for (int i = 0; i < header->edge_count; i++) {
    std::string line;
    getline(file, line);
    Vector<std::string> value_vec = explode(line, ' ');

    std::pair<int, int> edge = std::make_pair(stoi(value_vec[0]), stoi(value_vec[1]));
    data.edges.append(edge);
  }

  return data;
}

int3 get_vertex_index(const PlyHeader *header)
{
  int3 vertexPos;
  vertexPos.x = get_index(header, "x", PlyDataTypes::FLOAT);
  vertexPos.y = get_index(header, "y", PlyDataTypes::FLOAT);
  vertexPos.z = get_index(header, "z", PlyDataTypes::FLOAT);

  return vertexPos;
}

int3 get_color_index(const PlyHeader *header)
{
  int3 vertexPos;
  vertexPos.x = get_index(header, "red", PlyDataTypes::UCHAR);
  vertexPos.y = get_index(header, "green", PlyDataTypes::UCHAR);
  vertexPos.z = get_index(header, "blue", PlyDataTypes::UCHAR);

  return vertexPos;
}

int3 get_normal_index(const PlyHeader *header)
{
  int3 vertexPos;
  vertexPos.x = get_index(header, "nx", PlyDataTypes::FLOAT);
  vertexPos.y = get_index(header, "ny", PlyDataTypes::FLOAT);
  vertexPos.z = get_index(header, "nz", PlyDataTypes::FLOAT);

  return vertexPos;
}

int2 get_uv_index(const PlyHeader *header)
{
  int2 uvPos;
  uvPos.x = get_index(header, "s", PlyDataTypes::FLOAT);
  uvPos.y = get_index(header, "t", PlyDataTypes::FLOAT);

  return uvPos;
}

int get_index(const PlyHeader *header, std::string property, PlyDataTypes datatype)
{
  std::pair<std::string, PlyDataTypes> pair = {property, datatype};
  auto it = std::find(header->properties[0].begin(), header->properties[0].end(), pair);
  return (int)(it - header->properties[0].begin());
}

Vector<std::string> explode(const StringRef &str, const char &ch)
{
  std::string next;
  Vector<std::string> result;

  /* For each character in the string. */
  for (auto c : str) {
    /* If we've hit the terminal character. */
    if (c == ch) {
      /* If we have some characters accumulated. */
      if (!next.empty()) {
        /* Add them to the result vector. */
        result.append(next);
        next.clear();
      }
    }
    else {
      /* Accumulate the next character into the sequence. */
      next += c;
    }
  }

  if (!next.empty()) {
    result.append(next);
  }

  return result;
}

}  // namespace blender::io::ply
