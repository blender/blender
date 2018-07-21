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

#ifndef OPENSUBDIV_GL_MESH_FVAR_H_
#define OPENSUBDIV_GL_MESH_FVAR_H_

// NOTE: This is a [sane(er)] port of previous ground work for getting UVs to
// work. Still needs a lot of work to make it easy, correct and have proper
// data ownership.

#include <opensubdiv/far/topologyRefiner.h>
#include <opensubdiv/far/patchTable.h>

#include <vector>

namespace opensubdiv_capi {

// The buffer which holds GPU resources for face-varying elements.
class GLMeshFVarData {
 public:
  GLMeshFVarData();
  ~GLMeshFVarData();

  void release();
  void create(const OpenSubdiv::Far::TopologyRefiner* refiner,
              const OpenSubdiv::Far::PatchTable* patch_table,
              int fvar_width,
              const float* fvar_src_data);

  unsigned int texture_buffer;
  unsigned int offset_buffer;
  std::vector<int> channel_offsets;
  int fvar_width;
};

void interpolateFVarData(const OpenSubdiv::Far::TopologyRefiner& refiner,
                         const std::vector<float>& uvs,
                         std::vector<float>* fvar_data);

}  // namespace opensubdiv_capi

#endif  // OPENSUBDIV_GL_MESH_FVAR_H_
