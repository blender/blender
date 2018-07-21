// Copyright 2018 Blender Foundation. All rights reserved.
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

#ifndef OPENSUBDIV_GL_MESH_INTERNAL_H_
#define OPENSUBDIV_GL_MESH_INTERNAL_H_

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <opensubdiv/osd/glMesh.h>

#include "opensubdiv_capi_type.h"

namespace opensubdiv_capi {
class GLMeshFVarData;
}  // namespace opensubdiv_capi

typedef struct OpenSubdiv_GLMeshInternal {
  OpenSubdiv_GLMeshInternal();
  ~OpenSubdiv_GLMeshInternal();

  eOpenSubdivEvaluator evaluator_type;
  OpenSubdiv::Osd::GLMeshInterface* mesh_interface;
  opensubdiv_capi::GLMeshFVarData* fvar_data;
} OpenSubdiv_GLMeshInternal;

#endif  // OPENSUBDIV_GL_MESH_INTERNAL_H_
