/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_import_ascii.hh"
#include "ply_functions.hh"

#include <algorithm>
#include <fstream>

namespace blender::io::ply {

std::unique_ptr<PlyData> import_ply_ascii(fstream &file, PlyHeader *header)
{
  std::unique_ptr<PlyData> data = std::make_unique<PlyData>(load_ply_ascii(file, header));
  return data;
}

PlyData load_ply_ascii(fstream &file, const PlyHeader *header)
{
  PlyData data;
  /* Check if header contains alpha. */
  std::pair<std::string, PlyDataTypes> alpha = {"alpha", PlyDataTypes::UCHAR};
  bool has_alpha = std::find(header->properties[0].begin(), header->properties[0].end(), alpha) !=
                   header->properties[0].end();

  /* Check if header contains colors. */
  std::pair<std::string, PlyDataTypes> red = {"red", PlyDataTypes::UCHAR};
  bool has_color = std::find(header->properties[0].begin(), header->properties[0].end(), red) !=
                   header->properties[0].end();

  /* Check if header contains normals. */
  std::pair<std::string, PlyDataTypes> normalx = {"nx", PlyDataTypes::FLOAT};
  bool has_normals = std::find(header->properties[0].begin(),
                               header->properties[0].end(),
                               normalx) != header->properties[0].end();

  /* Check if header contains uv data. */
  std::pair<std::string, PlyDataTypes> uv = {"s", PlyDataTypes::FLOAT};
  const bool has_uv = std::find(header->properties[0].begin(), header->properties[0].end(), uv) !=
                      header->properties[0].end();

  int3 vertex_index = get_vertex_index(header);
  int alpha_index;
  int3 color_index;
  int3 normal_index;
  int2 uv_index;

  if (has_alpha) {
    alpha_index = get_index(header, "alpha", PlyDataTypes::UCHAR);
  }

  if (has_color) {
    /* x=red, y=green, z=blue */
    color_index = get_color_index(header);
  }

  if (has_normals) {
    normal_index = get_normal_index(header);
  }

  if (has_uv) {
    uv_index = get_uv_index(header);
  }

  for (int i = 0; i < header->vertex_count; i++) {
    std::string line;
    safe_getline(file, line);
    Vector<std::string> value_vec = explode(line, ' ');

    /* Vertex coords */
    float3 vertex3;
    vertex3.x = std::stof(value_vec[vertex_index.x]);
    vertex3.y = std::stof(value_vec[vertex_index.y]);
    vertex3.z = std::stof(value_vec[vertex_index.z]);

    data.vertices.append(vertex3);

    /* Vertex colors */
    if (has_color) {
      float4 colors4;
      colors4.x = std::stof(value_vec[color_index.x]) / 255.0f;
      colors4.y = std::stof(value_vec[color_index.y]) / 255.0f;
      colors4.z = std::stof(value_vec[color_index.z]) / 255.0f;
      if (has_alpha) {
        colors4.w = std::stof(value_vec[alpha_index]) / 255.0f;
      }
      else {
        colors4.w = 1.0f;
      }

      data.vertex_colors.append(colors4);
    }

    /* If normals */
    if (has_normals) {
      float3 normals3;
      normals3.x = std::stof(value_vec[normal_index.x]);
      normals3.y = std::stof(value_vec[normal_index.y]);
      normals3.z = std::stof(value_vec[normal_index.z]);

      data.vertex_normals.append(normals3);
    }

    /* If uv */
    if (has_uv) {
      float2 uvmap;
      uvmap.x = std::stof(value_vec[uv_index.x]);
      uvmap.y = std::stof(value_vec[uv_index.y]);

      data.uv_coordinates.append(uvmap);
    }
  }
  for (int i = 0; i < header->face_count; i++) {
    std::string line;
    getline(file, line);
    Vector<std::string> value_vec = explode(line, ' ');
    int count = std::stoi(value_vec[0]);
    Array<uint> vertex_indices(count);

    for (int j = 1; j <= count; j++) {
      int index = std::stoi(value_vec[j]);
      /* If the face has a vertex index that is outside the range. */
      if (index >= data.vertices.size()) {
        throw std::runtime_error("Vertex index out of bounds");
      }
      vertex_indices[j - 1] = index;
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
  int3 vertex_index;
  vertex_index.x = get_index(header, "x", PlyDataTypes::FLOAT);
  vertex_index.y = get_index(header, "y", PlyDataTypes::FLOAT);
  vertex_index.z = get_index(header, "z", PlyDataTypes::FLOAT);

  return vertex_index;
}

int3 get_color_index(const PlyHeader *header)
{
  int3 color_index;
  color_index.x = get_index(header, "red", PlyDataTypes::UCHAR);
  color_index.y = get_index(header, "green", PlyDataTypes::UCHAR);
  color_index.z = get_index(header, "blue", PlyDataTypes::UCHAR);

  return color_index;
}

int3 get_normal_index(const PlyHeader *header)
{
  int3 normal_index;
  normal_index.x = get_index(header, "nx", PlyDataTypes::FLOAT);
  normal_index.y = get_index(header, "ny", PlyDataTypes::FLOAT);
  normal_index.z = get_index(header, "nz", PlyDataTypes::FLOAT);

  return normal_index;
}

int2 get_uv_index(const PlyHeader *header)
{
  int2 uv_index;
  uv_index.x = get_index(header, "s", PlyDataTypes::FLOAT);
  uv_index.y = get_index(header, "t", PlyDataTypes::FLOAT);

  return uv_index;
}

int get_index(const PlyHeader *header, std::string property, PlyDataTypes datatype)
{
  std::pair<std::string, PlyDataTypes> pair = {property, datatype};
  const std::pair<std::string, blender::io::ply::PlyDataTypes> *it = std::find(
      header->properties[0].begin(), header->properties[0].end(), pair);
  return int(it - header->properties[0].begin());
}

Vector<std::string> explode(const StringRef str, const char &ch)
{
  std::string next;
  Vector<std::string> result;

  /* For each character in the string. */
  for (char c : str) {
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
