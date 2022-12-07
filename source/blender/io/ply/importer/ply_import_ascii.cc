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
  // check if has alpha
  std::pair<std::string, PlyDataTypes> alpha = {"alpha", PlyDataTypes::UCHAR};
  bool hasAlpha = std::find(header->properties.begin(), header->properties.end(), alpha) !=
                  header->properties.end();
  std::cout << "Has alpha: " << hasAlpha << std::endl;

  //check if has colours
  std::pair<std::string, PlyDataTypes> red = {"red", PlyDataTypes::UCHAR};
  bool hasColor = std::find(header->properties.begin(), header->properties.end(), red) !=
                  header->properties.end();
  std::cout << "Has color: " << hasColor << std::endl;

  //check if has normals
  std::pair<std::string, PlyDataTypes> normalx = {"nx", PlyDataTypes::FLOAT};
  bool hasNormals = std::find(header->properties.begin(), header->properties.end(), normalx) !=
                  header->properties.end();
  std::cout << "Has normals: " << hasNormals << std::endl;

  int3 vertexpos = get_vertex_pos(header);
  int alphapos;
  int3 colorpos;
  int3 normalpos;

  if (hasAlpha)
  {
    alphapos = get_index(header, "alpha", PlyDataTypes::UCHAR);
  }

  if (hasColor)
  {
    // x=red,y=green,z=blue
    //xyz = rgb
    colorpos = get_color_pos(header);
  }

  if (hasNormals){
    normalpos = get_normal_pos(header);
  }
  

  for (int i = 0; i < header->vertex_count; i++) {
    std::string line;
    getline(file, line);
    std::vector<std::string> value_arr = explode(line, ' ');
    
    //vertex coords
    float3 vertex3;
    //get pos of x in properties, grab that value from file line 
    vertex3.x = std::stof(value_arr.at(vertexpos.x));
    //get pos of y in properties, grab that value from file line 
    vertex3.y = std::stof(value_arr.at(vertexpos.y));
    //get pos of z in properties, grab that value from file line 
    vertex3.z = std::stof(value_arr.at(vertexpos.z));

    data.vertices.append(vertex3);
    
    // vertex colours
    // if colours 
    if (hasColor)
    {
      float4 colors4;
      //get pos of red in properties, grab that value from file line and convert from uchar?
      colors4.x = std::stof(value_arr.at(colorpos.x))/255.0f; 
      //get pos of green in properties, grab that value from file line 
      colors4.y = std::stof(value_arr.at(colorpos.y))/255.0f;
      //get pos of blue in properties, grab that value from file line 
      colors4.z = std::stof(value_arr.at(colorpos.z))/255.0f;
      //if alpha get pos of alpha in properties, grab that value from file line else alpha 1.0f
      if (hasAlpha)
      {
        colors4.w = std::stof(value_arr.at(alphapos))/255.0f;
      } else {
        colors4.w = 1.0f;
      }
      
      data.vertex_colors.append(colors4);
    }

    // if normals
    if (hasNormals)
    {
      float3 normals3;
      //get pos of nx in properties, grab that value from file line 
      vertex3.x = std::stof(value_arr.at(normalpos.x));
      
      //genormals3t pos of ny in properties, grab that value from file line 
      normals3.y = std::stof(value_arr.at(normalpos.y));

      //get pos of nz in properties, grab that value from file line 
      normals3.z = std::stof(value_arr.at(normalpos.z));

      data.vertex_normals.append(normals3);
    }
    
  }
  for (int i = 0; i < header->face_count; i++) {
    std::string line;
    getline(file, line);
    std::vector<std::string> value_arr = explode(line, ' ');
    Vector<uint> vertex_indices;

    for (int j = 1; j <= std::stoi(value_arr.at(0)); j++)
      {
        vertex_indices.append(std::stoi(value_arr.at(j)));
      }
    data.faces.append(vertex_indices);
      
  }

  return data;
}

int3 get_vertex_pos(PlyHeader *header){
  int3 vertexPos;
  vertexPos.x = get_index(header, "x", PlyDataTypes::FLOAT);
  vertexPos.y = get_index(header, "y", PlyDataTypes::FLOAT);
  vertexPos.z = get_index(header, "z", PlyDataTypes::FLOAT);

  return vertexPos;
}

int3 get_color_pos(PlyHeader *header){
  int3 vertexPos;
  vertexPos.x = get_index(header, "red", PlyDataTypes::UCHAR);
  vertexPos.y = get_index(header, "green", PlyDataTypes::UCHAR);
  vertexPos.z = get_index(header, "blue", PlyDataTypes::UCHAR);

  return vertexPos;
}

int3 get_normal_pos(PlyHeader *header){
  int3 vertexPos;
  vertexPos.x = get_index(header, "nx", PlyDataTypes::FLOAT);
  vertexPos.y = get_index(header, "ny", PlyDataTypes::FLOAT);
  vertexPos.z = get_index(header, "nz", PlyDataTypes::FLOAT);

  return vertexPos;
}

int get_index(PlyHeader *header, std::string property, PlyDataTypes datatype){
  std::pair<std::string, PlyDataTypes> pair = {property, datatype};
  auto it = std::find(header->properties.begin(), header->properties.end(), pair);
  return it - header->properties.begin();
}

std::vector<std::string> explode(const std::string& str, const char& ch) {
    std::string next;
    std::vector<std::string> result;

    // For each character in the string
    for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
        // If we've hit the terminal character
        if (*it == ch) {
            // If we have some characters accumulated
            if (!next.empty()) {
                // Add them to the result vector
                result.push_back(next);
                next.clear();
            }
        } else {
            // Accumulate the next character into the sequence
            next += *it;
        }
    }
    if (!next.empty()){
      result.push_back(next);
    }
         
    return result;
}


}  // namespace blender::io::ply