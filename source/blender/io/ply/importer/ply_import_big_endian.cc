#include "ply_import_big_endian.hh"
#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BLI_math_vector.h"
#include <fstream>

namespace blender::io::ply {
Mesh *import_ply_big_endian(std::ifstream &file, PlyHeader *header, Mesh* mesh)
{
  PlyData data = load_ply_big_endian(file, header);
  if (data.vertices.size() != 0) {
    return convert_ply_to_mesh(data, mesh);
  }
  return nullptr;
}

Mesh *convert_ply_to_mesh(PlyData& data, Mesh* mesh)
{
  mesh->totvert = data.vertices.size();
  CustomData_add_layer(&mesh->vdata, CD_MVERT, CD_SET_DEFAULT, nullptr, mesh->totvert);
  MutableSpan<MVert> verts = mesh->verts_for_write();
  for (int i = 0; i < mesh->totvert; i++) {
    float vert[3] = {data.vertices[i].x, data.vertices[i].y, data.vertices[i].z};
    copy_v3_v3(verts[i].co, vert);
  }

  mesh->totpoly = data.faces.size();
  mesh->totloop = data.faces.size() * data.faces[0].size(); // Todo make more generic, using data.edges
  CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_SET_DEFAULT, nullptr, mesh->totpoly);
  CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_SET_DEFAULT, nullptr, mesh->totloop);
  MutableSpan<MPoly> polys = mesh->polys_for_write();
  MutableSpan<MLoop> loops = mesh->loops_for_write();
  for (int i = 0; i < mesh->totpoly; i++) {
    int size = data.faces[i].size();
    polys[i].loopstart = size * i;
    polys[i].totloop = size;

    for (int j = 0; j < size; j++) {
      loops[size * i + j].v = data.faces[i][j];
    }
  }

  // Vertex colours
  if (data.vertex_colors.size() > 0) {
    // Create a data layer for vertex colours and set them
    CustomDataLayer *color_layer = BKE_id_attribute_new(
        &mesh->id, "Color", CD_PROP_COLOR, ATTR_DOMAIN_POINT, nullptr);
    float4 *colors = (float4 *)color_layer->data;
    for (int i = 0; i < data.vertex_colors.size(); i++) {
      float3 c = data.vertex_colors[i];
      colors[i] = float4(c.x, c.y, c.z, 1.0f);
    }
  }

  // Calculate mesh from edges
  BKE_mesh_calc_edges(mesh, false, false);

  return mesh;
}

float3 read_float3(std::ifstream &file) {
  float3 currFloat3;

  for (int i = 0; i < 3; i++) {
    float temp;
    file.read((char *)&temp, sizeof(temp));
    if (file.bad()) {
      printf("Read/Write error on io operation\n");
    } else if (file.fail()) {
      printf("Logical error on io operation\n");
    } else if (file.eof()) {
      printf("Reached end of the file\n");
    }
    temp = swap_bits<float>(temp);
    currFloat3[i] = temp;
  }

  return currFloat3;
}

uchar3 read_uchar3(std::ifstream& file) {
  uchar3 currUchar3;

  for (int i = 0; i < 3; i++) {
    uchar temp;
    file.read((char*)&temp, sizeof(temp));
    if (file.bad()) {
      printf("Read/Write error on io operation");
    } else if (file.fail()) {
      printf("Logical error on io operation");
    } else if (file.eof()) {
      printf("Reached end of the file");
    }
    // No swapping of bytes necessary as uchar is only 1 byte
    currUchar3[i] = temp;
  }

  return currUchar3;
}

float3 convert_uchar3_float3(uchar3 input) {
  float3 returnVal;
  for (int i = 0; i < 3; i++) {
    returnVal[i] = input[i] / 255.0f;
  }
  return returnVal;
}

PlyData load_ply_big_endian(std::ifstream &file, PlyHeader *header)
{
  PlyData data;

  for (int i = 0; i < header->vertex_count; i++) {
    float3 currFloat3;

    for (auto prop : header->properties) {
      if (prop.first == "x") {
        currFloat3 = read_float3(file);
      } else if (prop.first == "z") {
        data.vertices.append(currFloat3);
      } else if (prop.first == "nx") {
        currFloat3 = read_float3(file);
      } else if (prop.first == "nz") {
        data.vertex_normals.append(currFloat3);
      } else if (prop.first == "red") {
        currFloat3 = convert_uchar3_float3(read_uchar3(file));
      } else if (prop.first == "blue") {
        data.vertex_colors.append(currFloat3);
      }
    }
  }

  for (int i = 0; i < header->face_count; i++) {
    // Assume vertex_index_count_type is uchar
    uchar count;
    Vector<uint> vertex_indices;
    file.read((char*)&count, sizeof(count));
    if (!file.good()) {
      printf("Error reading data");
      break;
    }
    for (uchar j = 0; j < count; j++) {
      uint32_t index;
      file.read((char*)&index, sizeof(index));
      if (!file.good()) {
        printf("Error reading data");
        break;
      }
      index = swap_bits<uint32_t>(index);
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
