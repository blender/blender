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
#ifndef DRACO_COMPRESSION_MESH_MESH_DECODER_HELPERS_H_
#define DRACO_COMPRESSION_MESH_MESH_DECODER_HELPERS_H_

#include "draco/compression/mesh/mesh_decoder.h"

namespace draco {

// Function for decoding a stream previously encoded by a MeshEncoder. The
// result is stored into a stream of single precision floating point numbers
// in a XYZ|UV format, where one value is stored for every corner of each
// triangle.
// On error, the function sets the input stream "is" to an invalid state.
template <typename InStreamT>
InStreamT &DecodePos3Tex2DataFromStream(InStreamT &&is,
                                        std::vector<float> *out_data) {
  // Determine the size of the encoded data and write it into a vector.
  const auto start_pos = is.tellg();
  is.seekg(0, std::ios::end);
  const std::streampos is_size = is.tellg() - start_pos;
  is.seekg(start_pos);
  std::vector<char> data(is_size);
  is.read(&data[0], is_size);

  // Create a mesh from the data.
  std::unique_ptr<Mesh> mesh = draco::DecodeMesh(&data[0], data.size());

  if (mesh == nullptr) {
    is.setstate(ios_base::badbit);
    return is;
  }

  const PointAttribute *pos_att =
      mesh->GetNamedAttribute(GeometryAttribute::POSITION);
  const PointAttribute *tex_att =
      mesh->GetNamedAttribute(GeometryAttribute::TEX_COORD_0);

  // Both position and texture attributes must be present.
  if (pos_att == nullptr || tex_att == nullptr) {
    is.setstate(ios_base::badbit);
    return is;
  }

  // Copy the mesh data into the provided output.
  constexpr int data_stride = 5;
  // Prepare the output storage for 3 output values per face.
  out_data->resize(mesh->num_faces() * 3 * data_stride);

  std::array<float, 3> pos_val;
  std::array<float, 2> tex_val;
  int out_it = 0;
  for (int f = 0; f < mesh->num_faces(); ++f) {
    const Mesh::Face &face = mesh->face(f);
    for (int p = 0; p < 3; ++p) {
      pos_att->ConvertValue<float, 3>(pos_att->mapped_index(face[p]),
                                      &pos_val[0]);
      memcpy(&out_data->at(0) + out_it, &pos_val[0], sizeof(pos_val));
      out_it += 3;
      tex_att->ConvertValue<float, 2>(tex_att->mapped_index(face[p]),
                                      &tex_val[0]);
      memcpy(&out_data->at(0) + out_it, &tex_val[0], sizeof(tex_val));
      out_it += 2;
    }
  }

  return is;
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_MESH_MESH_DECODER_HELPERS_H_
