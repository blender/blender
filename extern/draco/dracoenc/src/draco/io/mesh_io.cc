// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "draco/io/mesh_io.h"

#include <fstream>

#include "draco/io/obj_decoder.h"
#include "draco/io/parser_utils.h"
#include "draco/io/ply_decoder.h"

namespace draco {

namespace {

// Returns the file extension in lowercase if present, else ""
inline std::string LowercaseFileExtension(const std::string &filename) {
  size_t pos = filename.find_last_of('.');
  if (pos == std::string::npos || pos >= filename.length() - 1)
    return "";
  return parser::ToLower(filename.substr(pos + 1));
}

}  // namespace

StatusOr<std::unique_ptr<Mesh>> ReadMeshFromFile(const std::string &file_name) {
  return ReadMeshFromFile(file_name, false);
}

StatusOr<std::unique_ptr<Mesh>> ReadMeshFromFile(const std::string &file_name,
                                                 bool use_metadata) {
  std::unique_ptr<Mesh> mesh(new Mesh());
  // Analyze file extension.
  const std::string extension = LowercaseFileExtension(file_name);
  if (extension == "obj") {
    // Wavefront OBJ file format.
    ObjDecoder obj_decoder;
    obj_decoder.set_use_metadata(use_metadata);
    const Status obj_status = obj_decoder.DecodeFromFile(file_name, mesh.get());
    if (!obj_status.ok())
      return obj_status;
    return std::move(mesh);
  }
  if (extension == "ply") {
    // Wavefront PLY file format.
    PlyDecoder ply_decoder;
    if (!ply_decoder.DecodeFromFile(file_name, mesh.get()))
      return Status(Status::ERROR, "Unknown error.");
    return std::move(mesh);
  }

  // Otherwise not an obj file. Assume the file was encoded with one of the
  // draco encoding methods.
  std::ifstream is(file_name.c_str(), std::ios::binary);
  if (!is)
    return Status(Status::ERROR, "Invalid input stream.");
  if (!ReadMeshFromStream(&mesh, is).good())
    return Status(Status::ERROR,
                  "Unknown error.");  // Error reading the stream.
  return std::move(mesh);
}

}  // namespace draco
