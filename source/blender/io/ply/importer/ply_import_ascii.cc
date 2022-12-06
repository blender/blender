//
// Created by Arjan on 06/12/2022.
//
#include "ply_import_ascii.hh"
#include "BLI_math_vector.h"
#include "ply_import_mesh.hh"
#include <fstream>

namespace blender::io::ply {

Mesh *import_ply_ascii(std::ifstream &file, PlyHeader *header, Mesh *mesh)
{
  PlyData data = load_ply_ascii(file, header);
  if (data.vertices.size() != 0) {
    return convert_ply_to_mesh(data, mesh);
  }
  return nullptr;
}

PlyData load_ply_ascii(std::ifstream &file, PlyHeader *header)
{
  PlyData data;

  std::pair<std::string, PlyDataTypes> alpha = {"alpha", PlyDataTypes::UCHAR};
  bool hasAlpha = std::find(header->properties.begin(), header->properties.end(), alpha) !=
                  header->properties.end();
  std::cout << "Has alpha: " << hasAlpha << std::endl;

  for (int i = 0; i < header->vertex_count; i++) {
    float3 currFloat3;
    float4 currFloat4;

    // switch (prop.second) {}
    for (auto prop : header->properties) {
      if (prop.first == "x") {
        currFloat3 = read_float3_ascii(file);
      } else if (prop.first == "z") {
        data.vertices.append(currFloat3);
      } else if (prop.first == "nx") {
        currFloat3 = read_float3_ascii(file);
      } else if (prop.first == "nz") {
        data.vertex_normals.append(currFloat3);
      } else if (prop.first == "red" && !hasAlpha) {
        currFloat3 = convert_uchar3_float3_ascii(read_uchar3_ascii(file));
      } else if (prop.first == "red" && hasAlpha) {
        currFloat4 = convert_uchar4_float4_ascii(read_uchar4_ascii(file));
      } else if (prop.first == "blue" && !hasAlpha) {
        data.vertex_colors.append(float4(currFloat3.x, currFloat3.y, currFloat3.z, 1.0f));
      } else if (prop.first == "alpha") {
        data.vertex_colors.append(currFloat4);
      } else if (prop.first == "y" || prop.first == "ny" || prop.first == "green" || (prop.first == "blue" && hasAlpha)){
        continue;
      } else {
        // We don't support any other properties yet
        switch (prop.second) {
          case CHAR:
            read<int8_t>(file);
            break;
          case UCHAR:
            read<uint8_t>(file);
            break;
          case SHORT:
            read<int16_t>(file);
            break;
          case USHORT:
            read<uint16_t>(file);
            break;
          case INT:
            read<int32_t>(file);
            break;
          case UINT:
            read<uint32_t>(file);
            break;
          case FLOAT:
            read<float>(file);
            break;
          case DOUBLE:
            read<double>(file);
            break;
        }
      }
    }
  }

  std::cout << "Vertex count: " << data.vertices.size() << std::endl;
  std::cout << "\tFirst: " << data.vertices.first() << std::endl;
  std::cout << "\tLast: " << data.vertices.last() << std::endl;
  std::cout << "Normals count: " << data.vertex_normals.size() << std::endl;
  std::cout << "\tFirst: " << data.vertex_normals.first() << std::endl;
  std::cout << "\tLast: " << data.vertex_normals.last() << std::endl;
  std::cout << "Colours count: " << data.vertex_colors.size() << std::endl;
  std::cout << "\tFirst: " << data.vertex_colors.first() << std::endl;
  std::cout << "\tLast: " << data.vertex_colors.last() << std::endl;

  return data;
}

float3 read_float3_ascii(std::ifstream& file){
  float3 currFloat3;

  for (int i = 0; i < 3; i++) {
    float temp;
    file.read((char *)&temp, sizeof(temp));
    check_file_errors_ascii(file);
    currFloat3[i] = temp;
  }

  return currFloat3;
}

uchar3 read_uchar3_ascii(std::ifstream &file)
{
  uchar3 currUchar3;

  for (int i = 0; i < 3; i++) {
    uchar temp;
    file.read((char *)&temp, sizeof(temp));
    check_file_errors_ascii(file);
    // No swapping of bytes necessary as uchar is only 1 byte
    currUchar3[i] = temp;
  }

  return currUchar3;
}
uchar4 read_uchar4_ascii(std::ifstream &file)
{
  uchar4 currUchar4;

  for (int i = 0; i < 4; i++) {
    uchar temp;
    file.read((char *)&temp, sizeof(temp));
    check_file_errors_ascii(file);
    // No swapping of bytes necessary as uchar is only 1 byte
    currUchar4[i] = temp;
  }

  return currUchar4;
}

float3 convert_uchar3_float3_ascii(uchar3 input) {
  float3 returnVal;
  for (int i = 0; i < 3; i++) {
    returnVal[i] = input[i] / 255.0f;
  }
  return returnVal;
}
float4 convert_uchar4_float4_ascii(uchar4 input)
{
  float4 returnVal;
  for (int i = 0; i < 4; i++) {
    returnVal[i] = input[i] / 255.0f;
  }
  return returnVal;
}

void check_file_errors_ascii(std::ifstream &file)
{
  if (file.bad()) {
    printf("Read/Write error on io operation");
  }
  else if (file.fail()) {
    printf("Logical error on io operation");
  }
  else if (file.eof()) {
    printf("Reached end of the file");
  }
}

template<typename T> T read(std::ifstream &file)
{
  T returnVal;
  file.read((char *)&returnVal, sizeof(returnVal));
  check_file_errors_ascii(file);
  return returnVal;
}

template uint8_t read<uint8_t>(std::ifstream &file);
template int8_t read<int8_t>(std::ifstream &file);
template uint16_t read<uint16_t>(std::ifstream &file);
template int16_t read<int16_t>(std::ifstream &file);
template uint32_t read<uint32_t>(std::ifstream &file);
template int32_t read<int32_t>(std::ifstream &file);
template float read<float>(std::ifstream &file);
template double read<double>(std::ifstream &file);

}  // namespace blender::io::ply