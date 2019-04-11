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
#ifndef DRACO_COMPRESSION_MESH_MESH_ENCODER_HELPERS_H_
#define DRACO_COMPRESSION_MESH_MESH_ENCODER_HELPERS_H_

#include "draco/compression/mesh/mesh_encoder.h"
#include "draco/mesh/triangle_soup_mesh_builder.h"

namespace draco {

// Helper class for encoding data supplied in a stream of single precision
// floating point numbers formatted as XYZ|UV. The stream must contain three
// XYZ|UV values for every face of the mesh.
// The encoded data is written into the output stream "os".
// In case of error, the stream is set to an invalid state (ios_base::bad_bit).
template <typename OStreamT>
OStreamT EncodePos3Tex2DataToStream(
    const float *data, int num_faces, CompressionMethod method,
    const MeshCompressionOptions &compression_options,
    const MeshAttributeCompressionOptions &pos_options,
    const MeshAttributeCompressionOptions &tex_options, OStreamT &&os) {
  // Build the mesh.
  TriangleSoupMeshBuilder mb;
  mb.Start(num_faces);
  const int pos_att_id =
      mb.AddAttribute(GeometryAttribute::POSITION, 3, DT_FLOAT32);
  const int tex_att_id =
      mb.AddAttribute(GeometryAttribute::TEX_COORD_0, 2, DT_FLOAT32);
  constexpr int data_stride = 5;
  constexpr int tex_offset = 3;
  for (int f = 0; f < num_faces; ++f) {
    int offset = 3 * f * data_stride;
    // Add position data for the face.
    mb.SetAttributeValuesForFace(pos_att_id, f, data + offset,
                                 data + offset + data_stride,
                                 data + offset + 2 * data_stride);
    // Add texture data for the face.
    offset += tex_offset;
    mb.SetAttributeValuesForFace(tex_att_id, f, data + offset,
                                 data + offset + data_stride,
                                 data + offset + 2 * data_stride);
  }
  std::unique_ptr<Mesh> mesh = mb.Finalize();
  if (mesh == nullptr) {
    os.setstate(ios_base::badbit);
    return os;
  }

  // Set up the encoder.
  std::unique_ptr<MeshEncoder> encoder =
      MeshEncoder::CreateEncoderForMethod(method);
  encoder->SetGlobalOptions(compression_options);
  encoder->SetAttributeOptions(GeometryAttribute::POSITION, pos_options);
  encoder->SetAttributeOptions(GeometryAttribute::TEX_COORD_0, tex_options);

  if (!encoder->EncodeMesh(mesh.get())) {
    os.setstate(ios_base::badbit);
    return os;
  }

  // Write the encoded data into the stream.
  os.write(static_cast<const char *>(encoder->buffer()->data()),
           encoder->buffer()->size());
  return os;
}

}  // namespace draco

#endif  // DRACO_COMPRESSION_MESH_MESH_ENCODER_HELPERS_H_
