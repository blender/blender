/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <epoxy/gl.h>

#include "gpu_compute_evaluator.h"

#include <opensubdiv/far/error.h>
#include <opensubdiv/far/patchDescriptor.h>
#include <opensubdiv/far/stencilTable.h>

#include <cassert>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_context.hh"
#include "GPU_debug.hh"
#include "GPU_state.hh"
#include "GPU_vertex_buffer.hh"
#include "gpu_shader_create_info.hh"

using OpenSubdiv::Far::LimitStencilTable;
using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Osd::BufferDescriptor;
using OpenSubdiv::Osd::PatchArray;
using OpenSubdiv::Osd::PatchArrayVector;

#define SHADER_SRC_VERTEX_BUFFER_BUF_SLOT 0
#define SHADER_DST_VERTEX_BUFFER_BUF_SLOT 1
#define SHADER_DU_BUFFER_BUF_SLOT 2
#define SHADER_DV_BUFFER_BUF_SLOT 3
#define SHADER_SIZES_BUF_SLOT 4
#define SHADER_OFFSETS_BUF_SLOT 5
#define SHADER_INDICES_BUF_SLOT 6
#define SHADER_WEIGHTS_BUF_SLOT 7
#define SHADER_DU_WEIGHTS_BUF_SLOT 8
#define SHADER_DV_WEIGHTS_BUF_SLOT 9

#define SHADER_PATCH_ARRAY_BUFFER_BUF_SLOT 4
#define SHADER_PATCH_COORDS_BUF_SLOT 5
#define SHADER_PATCH_INDEX_BUFFER_BUF_SLOT 6
#define SHADER_PATCH_PARAM_BUFFER_BUF_SLOT 7

namespace blender::opensubdiv {

template<class T> gpu::StorageBuf *create_buffer(std::vector<T> const &src, const char *name)
{
  if (src.empty()) {
    return nullptr;
  }

  const size_t buffer_size = src.size() * sizeof(T);
  gpu::StorageBuf *storage_buffer = GPU_storagebuf_create_ex(
      buffer_size, &src.at(0), GPU_USAGE_STATIC, name);

  return storage_buffer;
}

GPUStencilTableSSBO::GPUStencilTableSSBO(StencilTable const *stencilTable)
{
  _numStencils = stencilTable->GetNumStencils();
  if (_numStencils > 0) {
    sizes_buf = create_buffer(stencilTable->GetSizes(), "osd_sized");
    offsets_buf = create_buffer(stencilTable->GetOffsets(), "osd_offsets");
    indices_buf = create_buffer(stencilTable->GetControlIndices(), "osd_control_indices");
    weights_buf = create_buffer(stencilTable->GetWeights(), "osd_weights");
  }
}

GPUStencilTableSSBO::GPUStencilTableSSBO(LimitStencilTable const *limitStencilTable)
{
  _numStencils = limitStencilTable->GetNumStencils();
  if (_numStencils > 0) {
    sizes_buf = create_buffer(limitStencilTable->GetSizes(), "osd_sized");
    offsets_buf = create_buffer(limitStencilTable->GetOffsets(), "osd_offsets");
    indices_buf = create_buffer(limitStencilTable->GetControlIndices(), "osd_control_indices");
    weights_buf = create_buffer(limitStencilTable->GetWeights(), "osd_weights");
    du_weights_buf = create_buffer(limitStencilTable->GetDuWeights(), "osd_du_weights");
    dv_weights_buf = create_buffer(limitStencilTable->GetDvWeights(), "osd_dv_weights");
    duu_weights_buf = create_buffer(limitStencilTable->GetDuuWeights(), "osd_duu_weights");
    duv_weights_buf = create_buffer(limitStencilTable->GetDuvWeights(), "osd_duv_weights");
    dvv_weights_buf = create_buffer(limitStencilTable->GetDvvWeights(), "osd_dvv_weights");
  }
}

static void storage_buffer_free(gpu::StorageBuf **buffer)
{
  if (*buffer) {
    GPU_storagebuf_free(*buffer);
    *buffer = nullptr;
  }
}

GPUStencilTableSSBO::~GPUStencilTableSSBO()
{
  storage_buffer_free(&sizes_buf);
  storage_buffer_free(&offsets_buf);
  storage_buffer_free(&indices_buf);
  storage_buffer_free(&weights_buf);
  storage_buffer_free(&du_weights_buf);
  storage_buffer_free(&dv_weights_buf);
  storage_buffer_free(&duu_weights_buf);
  storage_buffer_free(&duv_weights_buf);
  storage_buffer_free(&dvv_weights_buf);
}

// ---------------------------------------------------------------------------

GPUComputeEvaluator::GPUComputeEvaluator() : _workGroupSize(64), _patchArraysSSBO(nullptr)
{
  memset((void *)&_stencilKernel, 0, sizeof(_stencilKernel));
  memset((void *)&_patchKernel, 0, sizeof(_patchKernel));
}

GPUComputeEvaluator::~GPUComputeEvaluator()
{
  if (_patchArraysSSBO) {
    GPU_storagebuf_free(_patchArraysSSBO);
    _patchArraysSSBO = nullptr;
  }
}

bool GPUComputeEvaluator::Compile(BufferDescriptor const &srcDesc,
                                  BufferDescriptor const &dstDesc,
                                  BufferDescriptor const &duDesc,
                                  BufferDescriptor const &dvDesc)
{

  if (!_stencilKernel.Compile(srcDesc, dstDesc, duDesc, dvDesc, _workGroupSize)) {
    return false;
  }

  if (!_patchKernel.Compile(srcDesc, dstDesc, duDesc, dvDesc, _workGroupSize)) {
    return false;
  }

  return true;
}

/* static */
void GPUComputeEvaluator::Synchronize(void * /*kernel*/)
{
  // XXX: this is currently just for the performance measuring purpose.
  // need to be reimplemented by fence and sync.
  GPU_finish();
}

int GPUComputeEvaluator::GetDispatchSize(int count) const
{
  return (count + _workGroupSize - 1) / _workGroupSize;
}

void GPUComputeEvaluator::DispatchCompute(blender::gpu::Shader *shader,
                                          int totalDispatchSize) const
{
  const int dispatchSize = GetDispatchSize(totalDispatchSize);
  int dispatchRX = dispatchSize;
  int dispatchRY = 1u;
  if (dispatchRX > GPU_max_work_group_count(0)) {
    /* Since there are some limitations with regards to the maximum work group size (could be as
     * low as 64k elements per call), we split the number elements into a "2d" number, with the
     * final index being computed as `res_x + res_y * max_work_group_size`. Even with a maximum
     * work group size of 64k, that still leaves us with roughly `64k * 64k = 4` billion elements
     * total, which should be enough. If not, we could also use the 3rd dimension. */
    /* TODO(fclem): We could dispatch fewer groups if we compute the prime factorization and
     * get the smallest rect fitting the requirements. */
    dispatchRX = dispatchRY = std::ceil(std::sqrt(dispatchSize));
    /* Avoid a completely empty dispatch line caused by rounding. */
    if ((dispatchRX * (dispatchRY - 1)) >= dispatchSize) {
      dispatchRY -= 1;
    }
  }

  /* X and Y dimensions may have different limits so the above computation may not be right, but
   * even with the standard 64k minimum on all dimensions we still have a lot of room. Therefore,
   * we presume it all fits. */
  assert(dispatchRY < GPU_max_work_group_count(1));
  GPU_compute_dispatch(shader, dispatchRX, dispatchRY, 1);

  /* Next usage of the src/dst buffers will always be a shader storage. Vertices/normals/attributes
   * are copied over to the final buffers using compute shaders. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
}

bool GPUComputeEvaluator::EvalStencils(gpu::VertBuf *srcBuffer,
                                       BufferDescriptor const &srcDesc,
                                       gpu::VertBuf *dstBuffer,
                                       BufferDescriptor const &dstDesc,
                                       gpu::VertBuf *duBuffer,
                                       BufferDescriptor const &duDesc,
                                       gpu::VertBuf *dvBuffer,
                                       BufferDescriptor const &dvDesc,
                                       gpu::StorageBuf *sizesBuffer,
                                       gpu::StorageBuf *offsetsBuffer,
                                       gpu::StorageBuf *indicesBuffer,
                                       gpu::StorageBuf *weightsBuffer,
                                       gpu::StorageBuf *duWeightsBuffer,
                                       gpu::StorageBuf *dvWeightsBuffer,
                                       int start,
                                       int end) const
{
  if (_stencilKernel.shader == nullptr) {
    return false;
  }
  int count = end - start;
  if (count <= 0) {
    return true;
  }

  GPU_shader_bind(_stencilKernel.shader);
  GPU_vertbuf_bind_as_ssbo(srcBuffer, SHADER_SRC_VERTEX_BUFFER_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(dstBuffer, SHADER_DST_VERTEX_BUFFER_BUF_SLOT);
  if (duBuffer) {
    GPU_vertbuf_bind_as_ssbo(duBuffer, SHADER_DU_BUFFER_BUF_SLOT);
  }
  if (dvBuffer) {
    GPU_vertbuf_bind_as_ssbo(dvBuffer, SHADER_DV_BUFFER_BUF_SLOT);
  }
  GPU_storagebuf_bind(sizesBuffer, SHADER_SIZES_BUF_SLOT);
  GPU_storagebuf_bind(offsetsBuffer, SHADER_OFFSETS_BUF_SLOT);
  GPU_storagebuf_bind(indicesBuffer, SHADER_INDICES_BUF_SLOT);
  GPU_storagebuf_bind(weightsBuffer, SHADER_WEIGHTS_BUF_SLOT);
  if (duWeightsBuffer) {
    GPU_storagebuf_bind(duWeightsBuffer, SHADER_DU_WEIGHTS_BUF_SLOT);
  }
  if (dvWeightsBuffer) {
    GPU_storagebuf_bind(dvWeightsBuffer, SHADER_DV_WEIGHTS_BUF_SLOT);
  }

  GPU_shader_uniform_int_ex(_stencilKernel.shader, _stencilKernel.uniformStart, 1, 1, &start);
  GPU_shader_uniform_int_ex(_stencilKernel.shader, _stencilKernel.uniformEnd, 1, 1, &end);
  GPU_shader_uniform_int_ex(
      _stencilKernel.shader, _stencilKernel.uniformSrcOffset, 1, 1, &srcDesc.offset);
  GPU_shader_uniform_int_ex(
      _stencilKernel.shader, _stencilKernel.uniformDstOffset, 1, 1, &dstDesc.offset);

// TODO init to -1 and check >= 0 to align with GPU module. Currently we assume that the uniform
// location is not zero as there are other uniforms defined as well.
#define BIND_BUF_DESC(uniform, desc) \
  if (_stencilKernel.uniform > 0) { \
    int value[] = {desc.offset, desc.length, desc.stride}; \
    GPU_shader_uniform_int_ex(_stencilKernel.shader, _stencilKernel.uniform, 3, 1, value); \
  }
  BIND_BUF_DESC(uniformDuDesc, duDesc)
  BIND_BUF_DESC(uniformDvDesc, dvDesc)
#undef BIND_BUF_DESC
  DispatchCompute(_stencilKernel.shader, count);
  // GPU_storagebuf_unbind_all();
  GPU_shader_unbind();

  return true;
}

bool GPUComputeEvaluator::EvalPatches(gpu::VertBuf *srcBuffer,
                                      BufferDescriptor const &srcDesc,
                                      gpu::VertBuf *dstBuffer,
                                      BufferDescriptor const &dstDesc,
                                      gpu::VertBuf *duBuffer,
                                      BufferDescriptor const &duDesc,
                                      gpu::VertBuf *dvBuffer,
                                      BufferDescriptor const &dvDesc,
                                      int numPatchCoords,
                                      gpu::VertBuf *patchCoordsBuffer,
                                      const PatchArrayVector &patchArrays,
                                      gpu::StorageBuf *patchIndexBuffer,
                                      gpu::StorageBuf *patchParamsBuffer)
{
  if (_patchKernel.shader == nullptr) {
    return false;
  }

  GPU_shader_bind(_patchKernel.shader);
  GPU_vertbuf_bind_as_ssbo(srcBuffer, SHADER_SRC_VERTEX_BUFFER_BUF_SLOT);
  GPU_vertbuf_bind_as_ssbo(dstBuffer, SHADER_DST_VERTEX_BUFFER_BUF_SLOT);
  if (duBuffer) {
    GPU_vertbuf_bind_as_ssbo(duBuffer, SHADER_DU_BUFFER_BUF_SLOT);
  }
  if (dvBuffer) {
    GPU_vertbuf_bind_as_ssbo(dvBuffer, SHADER_DV_BUFFER_BUF_SLOT);
  }
  GPU_vertbuf_bind_as_ssbo(patchCoordsBuffer, SHADER_PATCH_COORDS_BUF_SLOT);
  GPU_storagebuf_bind(patchIndexBuffer, SHADER_PATCH_INDEX_BUFFER_BUF_SLOT);
  GPU_storagebuf_bind(patchParamsBuffer, SHADER_PATCH_PARAM_BUFFER_BUF_SLOT);
  int patchArraySize = sizeof(PatchArray);
  if (_patchArraysSSBO) {
    GPU_storagebuf_free(_patchArraysSSBO);
    _patchArraysSSBO = nullptr;
  }
  _patchArraysSSBO = GPU_storagebuf_create_ex(patchArrays.size() * patchArraySize,
                                              static_cast<const void *>(&patchArrays[0]),
                                              GPU_USAGE_STATIC,
                                              "osd_patch_array");
  GPU_storagebuf_bind(_patchArraysSSBO, SHADER_PATCH_ARRAY_BUFFER_BUF_SLOT);

  GPU_shader_uniform_int_ex(
      _patchKernel.shader, _patchKernel.uniformSrcOffset, 1, 1, &srcDesc.offset);
  GPU_shader_uniform_int_ex(
      _patchKernel.shader, _patchKernel.uniformDstOffset, 1, 1, &dstDesc.offset);

// TODO init to -1 and check >= 0 to align with GPU module.
#define BIND_BUF_DESC(uniform, desc) \
  if (_stencilKernel.uniform > 0) { \
    int value[] = {desc.offset, desc.length, desc.stride}; \
    GPU_shader_uniform_int_ex(_patchKernel.shader, _patchKernel.uniform, 3, 1, value); \
  }
  BIND_BUF_DESC(uniformDuDesc, duDesc)
  BIND_BUF_DESC(uniformDvDesc, dvDesc)
#undef BIND_BUF_DESC

  DispatchCompute(_patchKernel.shader, numPatchCoords);
  GPU_shader_unbind();

  return true;
}
// ---------------------------------------------------------------------------

GPUComputeEvaluator::_StencilKernel::_StencilKernel() {}
GPUComputeEvaluator::_StencilKernel::~_StencilKernel()
{
  if (shader) {
    GPU_shader_free(shader);
    shader = nullptr;
  }
}
static blender::gpu::Shader *compile_eval_stencil_shader(BufferDescriptor const &srcDesc,
                                                         BufferDescriptor const &dstDesc,
                                                         BufferDescriptor const &duDesc,
                                                         BufferDescriptor const &dvDesc,
                                                         int workGroupSize)
{
  using namespace blender::gpu::shader;
  ShaderCreateInfo info("opensubdiv_compute_eval");
  info.local_group_size(workGroupSize, 1, 1);
  info.builtins(BuiltinBits::GLOBAL_INVOCATION_ID);
  info.builtins(BuiltinBits::NUM_WORK_GROUP);

  /* Ensure the basis code has access to proper backend specification define: it is not guaranteed
   * that the code provided by OpenSubdiv specifies it. For example, it doesn't for GLSL but it
   * does for Metal. Additionally, for Metal OpenSubdiv defines OSD_PATCH_BASIS_METAL as 1, so do
   * the same here to avoid possible warning about value being re-defined. */
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    info.define("OSD_PATCH_BASIS_METAL", "1");
  }
  else {
    info.define("OSD_PATCH_BASIS_GLSL");
  }

  // TODO: use specialization constants for src_stride, dst_stride. Not sure we can use
  // work group size as that requires extensions. This allows us to compile less shaders and
  // improve overall performance. Adding length as specialization constant will not work as it is
  // used to define an array length. This is not supported by Metal.
  std::string length = std::to_string(srcDesc.length);
  std::string src_stride = std::to_string(srcDesc.stride);
  std::string dst_stride = std::to_string(dstDesc.stride);
  std::string work_group_size = std::to_string(workGroupSize);
  info.define("LENGTH", length);
  info.define("SRC_STRIDE", src_stride);
  info.define("DST_STRIDE", dst_stride);
  info.define("WORK_GROUP_SIZE", work_group_size);
  info.typedef_source("osd_patch_basis.glsl");
  info.storage_buf(
      SHADER_SRC_VERTEX_BUFFER_BUF_SLOT, Qualifier::read, "float", "srcVertexBuffer[]");
  info.storage_buf(
      SHADER_DST_VERTEX_BUFFER_BUF_SLOT, Qualifier::write, "float", "dstVertexBuffer[]");
  info.push_constant(Type::int_t, "srcOffset");
  info.push_constant(Type::int_t, "dstOffset");

  bool deriv1 = (duDesc.length > 0 || dvDesc.length > 0);
  if (deriv1) {
    info.define("OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES");
    info.storage_buf(SHADER_DU_BUFFER_BUF_SLOT, Qualifier::read_write, "float", "duBuffer[]");
    info.storage_buf(SHADER_DV_BUFFER_BUF_SLOT, Qualifier::read_write, "float", "dvBuffer[]");
    info.push_constant(Type::int3_t, "duDesc");
    info.push_constant(Type::int3_t, "dvDesc");
  }

  info.storage_buf(SHADER_SIZES_BUF_SLOT, Qualifier::read, "int", "sizes_buf[]");
  info.storage_buf(SHADER_OFFSETS_BUF_SLOT, Qualifier::read, "int", "offsets_buf[]");
  info.storage_buf(SHADER_INDICES_BUF_SLOT, Qualifier::read, "int", "indices_buf[]");
  info.storage_buf(SHADER_WEIGHTS_BUF_SLOT, Qualifier::read, "float", "weights_buf[]");
  if (deriv1) {
    info.storage_buf(
        SHADER_DU_WEIGHTS_BUF_SLOT, Qualifier::read_write, "float", "du_weights_buf[]");
    info.storage_buf(
        SHADER_DV_WEIGHTS_BUF_SLOT, Qualifier::read_write, "float", "dv_weights_buf[]");
  }
  info.push_constant(Type::int_t, "batchStart");
  info.push_constant(Type::int_t, "batchEnd");

  info.compute_source("osd_eval_stencils_comp.glsl");
  blender::gpu::Shader *shader = GPU_shader_create_from_info(
      reinterpret_cast<const GPUShaderCreateInfo *>(&info));
  return shader;
}

bool GPUComputeEvaluator::_StencilKernel::Compile(BufferDescriptor const &srcDesc,
                                                  BufferDescriptor const &dstDesc,
                                                  BufferDescriptor const &duDesc,
                                                  BufferDescriptor const &dvDesc,
                                                  int workGroupSize)
{
  if (shader) {
    GPU_shader_free(shader);
    shader = nullptr;
  }

  shader = compile_eval_stencil_shader(srcDesc, dstDesc, duDesc, dvDesc, workGroupSize);
  if (shader == nullptr) {
    return false;
  }

  // cache uniform locations (TODO: use uniform block)
  uniformStart = GPU_shader_get_uniform(shader, "batchStart");
  uniformEnd = GPU_shader_get_uniform(shader, "batchEnd");
  uniformSrcOffset = GPU_shader_get_uniform(shader, "srcOffset");
  uniformDstOffset = GPU_shader_get_uniform(shader, "dstOffset");
  uniformDuDesc = GPU_shader_get_uniform(shader, "duDesc");
  uniformDvDesc = GPU_shader_get_uniform(shader, "dvDesc");

  return true;
}

// ---------------------------------------------------------------------------

GPUComputeEvaluator::_PatchKernel::_PatchKernel() {}
GPUComputeEvaluator::_PatchKernel::~_PatchKernel()
{
  if (shader) {
    GPU_shader_free(shader);
    shader = nullptr;
  }
}

static blender::gpu::Shader *compile_eval_patches_shader(BufferDescriptor const &srcDesc,
                                                         BufferDescriptor const &dstDesc,
                                                         BufferDescriptor const &duDesc,
                                                         BufferDescriptor const &dvDesc,
                                                         int workGroupSize)
{
  using namespace blender::gpu::shader;
  ShaderCreateInfo info("opensubdiv_compute_eval");
  info.local_group_size(workGroupSize, 1, 1);
  info.builtins(BuiltinBits::GLOBAL_INVOCATION_ID);
  info.builtins(BuiltinBits::NUM_WORK_GROUP);

  /* Ensure the basis code has access to proper backend specification define: it is not guaranteed
   * that the code provided by OpenSubdiv specifies it. For example, it doesn't for GLSL but it
   * does for Metal. Additionally, for Metal OpenSubdiv defines OSD_PATCH_BASIS_METAL as 1, so do
   * the same here to avoid possible warning about value being re-defined. */
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    info.define("OSD_PATCH_BASIS_METAL", "1");
  }
  else {
    info.define("OSD_PATCH_BASIS_GLSL");
  }

  // TODO: use specialization constants for src_stride, dst_stride. Not sure we can use
  // work group size as that requires extensions. This allows us to compile less shaders and
  // improve overall performance. Adding length as specialization constant will not work as it is
  // used to define an array length. This is not supported by Metal.
  std::string length = std::to_string(srcDesc.length);
  std::string src_stride = std::to_string(srcDesc.stride);
  std::string dst_stride = std::to_string(dstDesc.stride);
  std::string work_group_size = std::to_string(workGroupSize);
  info.define("LENGTH", length);
  info.define("SRC_STRIDE", src_stride);
  info.define("DST_STRIDE", dst_stride);
  info.define("WORK_GROUP_SIZE", work_group_size);
  info.typedef_source("osd_patch_basis.glsl");
  info.storage_buf(
      SHADER_SRC_VERTEX_BUFFER_BUF_SLOT, Qualifier::read, "float", "srcVertexBuffer[]");
  info.storage_buf(
      SHADER_DST_VERTEX_BUFFER_BUF_SLOT, Qualifier::write, "float", "dstVertexBuffer[]");
  info.push_constant(Type::int_t, "srcOffset");
  info.push_constant(Type::int_t, "dstOffset");

  bool deriv1 = (duDesc.length > 0 || dvDesc.length > 0);
  if (deriv1) {
    info.define("OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES");
    info.storage_buf(SHADER_DU_BUFFER_BUF_SLOT, Qualifier::read_write, "float", "duBuffer[]");
    info.storage_buf(SHADER_DV_BUFFER_BUF_SLOT, Qualifier::read_write, "float", "dvBuffer[]");
    info.push_constant(Type::int3_t, "duDesc");
    info.push_constant(Type::int3_t, "dvDesc");
  }

  info.storage_buf(
      SHADER_PATCH_ARRAY_BUFFER_BUF_SLOT, Qualifier::read, "OsdPatchArray", "patchArrayBuffer[]");
  info.storage_buf(
      SHADER_PATCH_COORDS_BUF_SLOT, Qualifier::read, "OsdPatchCoord", "patchCoords[]");
  info.storage_buf(
      SHADER_PATCH_INDEX_BUFFER_BUF_SLOT, Qualifier::read, "int", "patchIndexBuffer[]");
  info.storage_buf(
      SHADER_PATCH_PARAM_BUFFER_BUF_SLOT, Qualifier::read, "OsdPatchParam", "patchParamBuffer[]");

  info.compute_source("osd_eval_patches_comp.glsl");
  blender::gpu::Shader *shader = GPU_shader_create_from_info(
      reinterpret_cast<const GPUShaderCreateInfo *>(&info));
  return shader;
}

bool GPUComputeEvaluator::_PatchKernel::Compile(BufferDescriptor const &srcDesc,
                                                BufferDescriptor const &dstDesc,
                                                BufferDescriptor const &duDesc,
                                                BufferDescriptor const &dvDesc,
                                                int workGroupSize)
{
  if (shader) {
    GPU_shader_free(shader);
    shader = nullptr;
  }

  shader = compile_eval_patches_shader(srcDesc, dstDesc, duDesc, dvDesc, workGroupSize);
  if (shader == nullptr) {
    return false;
  }

  // cache uniform locations
  uniformSrcOffset = GPU_shader_get_uniform(shader, "srcOffset");
  uniformDstOffset = GPU_shader_get_uniform(shader, "dstOffset");
  uniformDuDesc = GPU_shader_get_uniform(shader, "duDesc");
  uniformDvDesc = GPU_shader_get_uniform(shader, "dvDesc");

  return true;
}

}  // namespace blender::opensubdiv
