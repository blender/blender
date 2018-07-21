// Copyright 2013 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#include "internal/opensubdiv_gl_mesh_fvar.h"

#include <GL/glew.h>
#include <opensubdiv/far/primvarRefiner.h>

namespace opensubdiv_capi {

////////////////////////////////////////////////////////////////////////////////
// GLMeshFVarData

GLMeshFVarData::GLMeshFVarData()
    : texture_buffer(0),
      offset_buffer(0) {
}

GLMeshFVarData::~GLMeshFVarData() {
  release();
}

void GLMeshFVarData::release() {
  if (texture_buffer) {
    glDeleteTextures(1, &texture_buffer);
  }
  if (offset_buffer) {
    glDeleteTextures(1, &offset_buffer);
  }
  texture_buffer = 0;
  offset_buffer = 0;
  fvar_width = 0;
  channel_offsets.clear();
}

void GLMeshFVarData::create(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv::Far::PatchTable* patch_table,
    int fvar_width,
    const float* fvar_src_data) {
  release();
  this->fvar_width = fvar_width;
  /// Expand fvar data to per-patch array.
  const int max_level = topology_refiner->GetMaxLevel();
  const int num_channels = patch_table->GetNumFVarChannels();
  std::vector<float> data;
  int fvar_data_offset = 0;
  channel_offsets.resize(num_channels);
  for (int channel = 0; channel < num_channels; ++channel) {
    OpenSubdiv::Far::ConstIndexArray indices =
        patch_table->GetFVarValues(channel);
    channel_offsets[channel] = data.size();
    data.reserve(data.size() + indices.size() * fvar_width);
    for (int fvert = 0; fvert < indices.size(); ++fvert) {
      int index = indices[fvert] * fvar_width;
      for (int i = 0; i < fvar_width; ++i) {
        data.push_back(fvar_src_data[fvar_data_offset + index++]);
      }
    }
    if (topology_refiner->IsUniform()) {
      const int num_values_max =
          topology_refiner->GetLevel(max_level).GetNumFVarValues(channel);
      fvar_data_offset += num_values_max * fvar_width;
    } else {
      const int num_values_total =
          topology_refiner->GetNumFVarValuesTotal(channel);
      fvar_data_offset += num_values_total * fvar_width;
    }
  }
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER,
               data.size() * sizeof(float), &data[0],
               GL_STATIC_DRAW);
  glGenTextures(1, &texture_buffer);
  glBindTexture(GL_TEXTURE_BUFFER, texture_buffer);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, buffer);
  glDeleteBuffers(1, &buffer);
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER,
               channel_offsets.size() * sizeof(int),
               &channel_offsets[0],
               GL_STATIC_DRAW);
  glGenTextures(1, &offset_buffer);
  glBindTexture(GL_TEXTURE_BUFFER, offset_buffer);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, buffer);
  glBindTexture(GL_TEXTURE_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Helper functions.

struct FVarVertex {
  float u, v;

  void Clear() {
    u = v = 0.0f;
  }

  void AddWithWeight(FVarVertex const& src, float weight) {
    u += weight * src.u;
    v += weight * src.v;
  }
};

void interpolateFVarData(const OpenSubdiv::Far::TopologyRefiner& refiner,
                         const std::vector<float>& uvs,
                         std::vector<float>* fvar_data) {
  const int fvar_width = 2;
  const int max_level = refiner.GetMaxLevel();
  size_t fvar_data_offset = 0, values_offset = 0;
  for (int channel = 0; channel < refiner.GetNumFVarChannels(); ++channel) {
    const int num_values = refiner.GetLevel(0).GetNumFVarValues(channel) * 2;
    const int num_values_max =
        refiner.GetLevel(max_level).GetNumFVarValues(channel);
    const int num_values_total = refiner.GetNumFVarValuesTotal(channel);
    if (num_values_total <= 0) {
      continue;
    }
    OpenSubdiv::Far::PrimvarRefiner primvar_refiner(refiner);
    if (refiner.IsUniform()) {
      // For uniform we only keep the highest level of refinement.
      fvar_data->resize(fvar_data->size() + num_values_max * fvar_width);
      std::vector<FVarVertex> buffer(num_values_total - num_values_max);
      FVarVertex* src = &buffer[0];
      memcpy(src, &uvs[values_offset], num_values * sizeof(float));
      // Defer the last level to treat separately with its alternate
      // destination.
      for (int level = 1; level < max_level; ++level) {
        FVarVertex* dst =
            src + refiner.GetLevel(level - 1).GetNumFVarValues(channel);
        primvar_refiner.InterpolateFaceVarying(level, src, dst, channel);
        src = dst;
      }
      FVarVertex* dst =
          reinterpret_cast<FVarVertex*>(&(*fvar_data)[fvar_data_offset]);
      primvar_refiner.InterpolateFaceVarying(max_level, src, dst, channel);
      fvar_data_offset += num_values_max * fvar_width;
    } else {
      // For adaptive we keep all levels.
      fvar_data->resize(fvar_data->size() + num_values_total * fvar_width);
      FVarVertex* src =
          reinterpret_cast<FVarVertex*>(&(*fvar_data)[fvar_data_offset]);
      memcpy(src, &uvs[values_offset], num_values * sizeof(float));
      for (int level = 1; level <= max_level; ++level) {
        FVarVertex* dst =
            src + refiner.GetLevel(level - 1).GetNumFVarValues(channel);
        primvar_refiner.InterpolateFaceVarying(level, src, dst, channel);
        src = dst;
      }
      fvar_data_offset += num_values_total * fvar_width;
    }
    values_offset += num_values;
  }
}

}  // namespace opensubdiv_capi
