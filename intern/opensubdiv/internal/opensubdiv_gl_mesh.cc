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
// Contributor(s): Brecht van Lommel

#include "opensubdiv_gl_mesh_capi.h"

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <opensubdiv/far/stencilTable.h>
#include <opensubdiv/osd/glMesh.h>
#include <opensubdiv/osd/glPatchTable.h>

using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Osd::GLMeshInterface;
using OpenSubdiv::Osd::GLPatchTable;
using OpenSubdiv::Osd::Mesh;
using OpenSubdiv::Osd::MeshBitset;

// CPU backend.
#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/cpuGLVertexBuffer.h>
using OpenSubdiv::Osd::CpuGLVertexBuffer;
using OpenSubdiv::Osd::CpuEvaluator;
typedef Mesh<CpuGLVertexBuffer,
             StencilTable,
             CpuEvaluator,
             GLPatchTable> OsdCpuMesh;
// OpenMP backend.
#ifdef OPENSUBDIV_HAS_OPENMP
#  include <opensubdiv/osd/ompEvaluator.h>
using OpenSubdiv::Osd::OmpEvaluator;
typedef Mesh<CpuGLVertexBuffer,
             StencilTable,
             OmpEvaluator,
             GLPatchTable> OsdOmpMesh;
#endif
// OpenCL backend.
#ifdef OPENSUBDIV_HAS_OPENCL
#  include <opensubdiv/osd/clEvaluator.h>
#  include <opensubdiv/osd/clGLVertexBuffer.h>
#  include "opensubdiv_device_context_opencl.h"
using OpenSubdiv::Osd::CLEvaluator;
using OpenSubdiv::Osd::CLGLVertexBuffer;
using OpenSubdiv::Osd::CLStencilTable;
/* TODO(sergey): Use CLDeviceContext similar to OSD examples? */
typedef Mesh<CLGLVertexBuffer,
             CLStencilTable,
             CLEvaluator,
             GLPatchTable,
             CLDeviceContext> OsdCLMesh;
static CLDeviceContext g_cl_device_context;
#endif
// CUDA backend.
#ifdef OPENSUBDIV_HAS_CUDA
#  include <opensubdiv/osd/cudaEvaluator.h>
#  include <opensubdiv/osd/cudaGLVertexBuffer.h>
#  include "opensubdiv_device_context_cuda.h"
using OpenSubdiv::Osd::CudaEvaluator;
using OpenSubdiv::Osd::CudaGLVertexBuffer;
using OpenSubdiv::Osd::CudaStencilTable;
typedef Mesh<CudaGLVertexBuffer,
             CudaStencilTable,
             CudaEvaluator,
             GLPatchTable> OsdCudaMesh;
static CudaDeviceContext g_cuda_device_context;
#endif
// Transform feedback backend.
#ifdef OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK
#  include <opensubdiv/osd/glVertexBuffer.h>
#  include <opensubdiv/osd/glXFBEvaluator.h>
using OpenSubdiv::Osd::GLXFBEvaluator;
using OpenSubdiv::Osd::GLStencilTableTBO;
using OpenSubdiv::Osd::GLVertexBuffer;
typedef Mesh<GLVertexBuffer,
             GLStencilTableTBO,
             GLXFBEvaluator,
             GLPatchTable> OsdGLSLTransformFeedbackMesh;
#endif
// GLSL compute backend.
#ifdef OPENSUBDIV_HAS_GLSL_COMPUTE
#  include <opensubdiv/osd/glComputeEvaluator.h>
#  include <opensubdiv/osd/glVertexBuffer.h>
using OpenSubdiv::Osd::GLComputeEvaluator;
using OpenSubdiv::Osd::GLStencilTableSSBO;
using OpenSubdiv::Osd::GLVertexBuffer;
typedef Mesh<GLVertexBuffer,
             GLStencilTableSSBO,
             GLComputeEvaluator,
             GLPatchTable> OsdGLSLComputeMesh;
#endif

#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "opensubdiv_topology_refiner_capi.h"
#include "internal/opensubdiv_gl_mesh_draw.h"
#include "internal/opensubdiv_gl_mesh_fvar.h"
#include "internal/opensubdiv_gl_mesh_internal.h"
#include "internal/opensubdiv_topology_refiner_internal.h"

namespace {

GLMeshInterface* createGLMeshInterface(
    OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const MeshBitset& bits,
    const int num_vertex_elements,
    const int num_varying_elements,
    const int level,
    eOpenSubdivEvaluator evaluator_type) {
  GLMeshInterface* mesh = NULL;
  switch (evaluator_type) {
#define CHECK_EVALUATOR_TYPE(type, class)                                \
  case OPENSUBDIV_EVALUATOR_##type:                                      \
    mesh = new class(topology_refiner,                                   \
                     num_vertex_elements,                                \
                     num_varying_elements,                               \
                     level,                                              \
                     bits);                                              \
    break;

#define CHECK_EVALUATOR_TYPE_STUB(type)                                  \
  case OPENSUBDIV_EVALUATOR_##type:                                      \
    mesh = NULL;                                                         \
    break;

    CHECK_EVALUATOR_TYPE(CPU, OsdCpuMesh)
#ifdef OPENSUBDIV_HAS_OPENMP
    CHECK_EVALUATOR_TYPE(OPENMP, OsdOmpMesh)
#else
    CHECK_EVALUATOR_TYPE_STUB(OPENMP)
#endif
#ifdef OPENSUBDIV_HAS_OPENCL
    CHECK_EVALUATOR_TYPE(OPENCL, OsdCLMesh)
#else
    CHECK_EVALUATOR_TYPE_STUB(OPENCL)
#endif
#ifdef OPENSUBDIV_HAS_CUDA
    CHECK_EVALUATOR_TYPE(CUDA, OsdCudaMesh)
#else
    CHECK_EVALUATOR_TYPE_STUB(CUDA)
#endif
#ifdef OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK
    CHECK_EVALUATOR_TYPE(GLSL_TRANSFORM_FEEDBACK, OsdGLSLTransformFeedbackMesh)
#else
    CHECK_EVALUATOR_TYPE_STUB(GLSL_TRANSFORM_FEEDBACK)
#endif
#ifdef OPENSUBDIV_HAS_GLSL_COMPUTE
    CHECK_EVALUATOR_TYPE(GLSL_COMPUTE, OsdGLSLComputeMesh)
#else
    CHECK_EVALUATOR_TYPE_STUB(GLSL_COMPUTE)
#endif

#undef CHECK_EVALUATOR_TYPE
#undef CHECK_EVALUATOR_TYPE_STUB
  }
  return mesh;
}

////////////////////////////////////////////////////////////////////////////////
// GLMesh structure "methods".

opensubdiv_capi::GLMeshFVarData* createFVarData(
    OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    GLMeshInterface* mesh,
    const float *fvar_src_buffer) {
  using opensubdiv_capi::GLMeshFVarData;
  GLMeshFVarData* fvar_data = new GLMeshFVarData();
  fvar_data->create(topology_refiner,
                    mesh->GetFarPatchTable(),
                    2,
                    fvar_src_buffer);
  return fvar_data;
}

unsigned int getPatchIndexBuffer(OpenSubdiv_GLMesh* gl_mesh) {
  return gl_mesh->internal->mesh_interface
             ->GetPatchTable()
             ->GetPatchIndexBuffer();
}

void bindVertexBuffer(OpenSubdiv_GLMesh* gl_mesh) {
  gl_mesh->internal->mesh_interface->BindVertexBuffer();
}

void setCoarsePositions(OpenSubdiv_GLMesh* gl_mesh,
                        const float* positions,
                        const int start_vertex,
                        const int num_vertices) {
  gl_mesh->internal->mesh_interface->UpdateVertexBuffer(positions,
                                                        start_vertex,
                                                        num_vertices);
}

void refine(OpenSubdiv_GLMesh* gl_mesh) {
  gl_mesh->internal->mesh_interface->Refine();
}

void synchronize(struct OpenSubdiv_GLMesh* gl_mesh) {
  gl_mesh->internal->mesh_interface->Synchronize();
}

void assignFunctionPointers(OpenSubdiv_GLMesh* gl_mesh) {
  gl_mesh->getPatchIndexBuffer = getPatchIndexBuffer;
  gl_mesh->bindVertexBuffer = bindVertexBuffer;
  gl_mesh->setCoarsePositions = setCoarsePositions;
  gl_mesh->refine = refine;
  gl_mesh->synchronize = synchronize;

  gl_mesh->prepareDraw = opensubdiv_capi::GLMeshDisplayPrepare;
  gl_mesh->drawPatches = opensubdiv_capi::GLMeshDisplayDrawPatches;
}

}  // namespace

struct OpenSubdiv_GLMesh *openSubdiv_createOsdGLMeshFromTopologyRefiner(
    OpenSubdiv_TopologyRefiner* topology_refiner,
    eOpenSubdivEvaluator evaluator_type) {
  using OpenSubdiv::Far::TopologyRefiner;
  TopologyRefiner* osd_topology_refiner =
      topology_refiner->internal->osd_topology_refiner;
  // TODO(sergey): Query this from refiner.
  const bool is_adaptive = false;
  MeshBitset bits;
  bits.set(OpenSubdiv::Osd::MeshAdaptive, is_adaptive);
  bits.set(OpenSubdiv::Osd::MeshUseSingleCreasePatch, 0);
  bits.set(OpenSubdiv::Osd::MeshInterleaveVarying, 1);
  bits.set(OpenSubdiv::Osd::MeshFVarData, 1);
  bits.set(OpenSubdiv::Osd::MeshEndCapBSplineBasis, 1);
  const int num_vertex_elements = 3;
  const int num_varying_elements = 3;
  GLMeshInterface* mesh = createGLMeshInterface(
      osd_topology_refiner,
      bits,
      num_vertex_elements,
      num_varying_elements,
      osd_topology_refiner->GetMaxLevel(),
      evaluator_type);
  if (mesh == NULL) {
    return NULL;
  }
  OpenSubdiv_GLMesh* gl_mesh = OBJECT_GUARDED_NEW(OpenSubdiv_GLMesh);
  assignFunctionPointers(gl_mesh);
  gl_mesh->internal = new OpenSubdiv_GLMeshInternal();
  gl_mesh->internal->evaluator_type = evaluator_type;
  gl_mesh->internal->mesh_interface = mesh;
  // Face-varying support.
  // TODO(sergey): This part needs to be re-done.
  if (osd_topology_refiner->GetNumFVarChannels() > 0) {
    // TODO(sergey): This is a temporary stub to get things compiled. Need
    // to store base level UVs somewhere else.
    std::vector<float> uvs;
    std::vector<float> fvar_data_buffer;
    opensubdiv_capi::interpolateFVarData(*osd_topology_refiner,
                                         uvs,
                                         &fvar_data_buffer);
    gl_mesh->internal->fvar_data = createFVarData(osd_topology_refiner,
                                                  mesh,
                                                  &fvar_data_buffer[0]);
  } else {
    gl_mesh->internal->fvar_data = NULL;
  }
  return gl_mesh;
}

void openSubdiv_deleteOsdGLMesh(OpenSubdiv_GLMesh* gl_mesh) {
  delete gl_mesh->internal;
  OBJECT_GUARDED_DELETE(gl_mesh, OpenSubdiv_GLMesh);
}
