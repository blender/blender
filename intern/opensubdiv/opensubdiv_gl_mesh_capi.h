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

#ifndef OPENSUBDIV_CAPI_GL_MESH_CAPI_H_
#define OPENSUBDIV_CAPI_GL_MESH_CAPI_H_

#include <stdint.h>  // for bool

#include "opensubdiv_capi_type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenSubdiv_GLMeshInternal;

// Mesh which is displayable in OpenGL context.
typedef struct OpenSubdiv_GLMesh {
  //////////////////////////////////////////////////////////////////////////////
  // Subdivision/topology part.

  // Returns the GL index buffer containing the patch control vertices.
  unsigned int (*getPatchIndexBuffer)(struct OpenSubdiv_GLMesh* gl_mesh);

  // Bind GL buffer which contains vertices (VBO).
  // TODO(sergey): Is this a coarse vertices?
  void (*bindVertexBuffer)(struct OpenSubdiv_GLMesh* gl_mesh);

  // Set coarse positions from a continuous array of coordinates.
  void (*setCoarsePositions)(struct OpenSubdiv_GLMesh* gl_mesh,
                             const float* positions,
                             const int start_vertex,
                             const int num_vertices);
  // TODO(sergey): setCoarsePositionsFromBuffer().

  // Refine after coarse positions update.
  void (*refine)(struct OpenSubdiv_GLMesh* gl_mesh);

  // Synchronize after coarse positions update and refine.
  void (*synchronize)(struct OpenSubdiv_GLMesh* gl_mesh);

  //////////////////////////////////////////////////////////////////////////////
  // Drawing part.

  // Prepare mesh for display.
  void (*prepareDraw)(struct OpenSubdiv_GLMesh* gl_mesh,
                      const bool use_osd_glsl,
                      const int active_uv_index);

  // Draw given range of patches.
  //
  // If fill_quads is false, then patches are drawn in wireframe.
  void (*drawPatches)(struct OpenSubdiv_GLMesh *gl_mesh,
                      const bool fill_quads,
                      const int start_patch, const int num_patches);

  // Internal storage for the use in this module only.
  //
  // Tease: This contains an actual OpenSubdiv's Mesh object.
  struct OpenSubdiv_GLMeshInternal* internal;
} OpenSubdiv_GLMesh;

OpenSubdiv_GLMesh* openSubdiv_createOsdGLMeshFromTopologyRefiner(
    struct OpenSubdiv_TopologyRefiner* topology_refiner,
    eOpenSubdivEvaluator evaluator_type);

void openSubdiv_deleteOsdGLMesh(OpenSubdiv_GLMesh *gl_mesh);

// Global resources needed for GL mesh drawing.
bool openSubdiv_initGLMeshDrawingResources(void);
void openSubdiv_deinitGLMeshDrawingResources(void);

#ifdef __cplusplus
}
#endif

#endif  // OPENSUBDIV_CAPI_GL_MESH_CAPI_H_
