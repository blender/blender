/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_patch_table.hh"

#include "opensubdiv/far/patchTable.h"
#include "opensubdiv/osd/cpuPatchTable.h"

using namespace OpenSubdiv::Osd;

namespace blender::opensubdiv {

GPUPatchTable *GPUPatchTable::Create(PatchTable const *far_patch_table, void * /*deviceContext*/)
{
  GPUPatchTable *instance = new GPUPatchTable();
  if (instance->allocate(far_patch_table)) {
    return instance;
  }
  delete instance;
  return nullptr;
}

static void discard_buffer(GPUStorageBuf **buffer)
{
  if (*buffer != nullptr) {
    GPU_storagebuf_free(*buffer);
    *buffer = nullptr;
  }
}

static void discard_list(std::vector<GPUStorageBuf *> &buffers)
{
  while (!buffers.empty()) {
    GPUStorageBuf *buffer = buffers.back();
    buffers.pop_back();
    GPU_storagebuf_free(buffer);
  }
}

GPUPatchTable::~GPUPatchTable()
{
  discard_buffer(&_patchIndexBuffer);
  discard_buffer(&_patchParamBuffer);
  discard_buffer(&_varyingIndexBuffer);
  discard_list(_fvarIndexBuffers);
  discard_list(_fvarParamBuffers);
}

bool GPUPatchTable::allocate(PatchTable const *far_patch_table)
{
  CpuPatchTable patch_table(far_patch_table);

  /* Patch array */
  size_t num_patch_arrays = patch_table.GetNumPatchArrays();
  _patchArrays.assign(patch_table.GetPatchArrayBuffer(),
                      patch_table.GetPatchArrayBuffer() + num_patch_arrays);

  /* Patch index buffer */
  const size_t index_size = patch_table.GetPatchIndexSize();
  _patchIndexBuffer = GPU_storagebuf_create_ex(
      index_size * sizeof(int32_t),
      static_cast<const void *>(patch_table.GetPatchIndexBuffer()),
      GPU_USAGE_STATIC,
      "osd_patch_index");

  /* Patch param buffer */
  const size_t patch_param_size = patch_table.GetPatchParamSize();
  _patchParamBuffer = GPU_storagebuf_create_ex(patch_param_size * sizeof(PatchParam),
                                               patch_table.GetPatchParamBuffer(),
                                               GPU_USAGE_STATIC,
                                               "osd_patch_param");

  /* Varying patch array */
  _varyingPatchArrays.assign(patch_table.GetVaryingPatchArrayBuffer(),
                             patch_table.GetVaryingPatchArrayBuffer() + num_patch_arrays);

  /* Varying index buffer */
  _varyingIndexBuffer = GPU_storagebuf_create_ex(patch_table.GetVaryingPatchIndexSize() *
                                                     sizeof(uint32_t),
                                                 patch_table.GetVaryingPatchIndexBuffer(),
                                                 GPU_USAGE_STATIC,
                                                 "osd_varying_index");

  /* Face varying */
  const int num_face_varying_channels = patch_table.GetNumFVarChannels();
  _fvarPatchArrays.resize(num_face_varying_channels);
  _fvarIndexBuffers.resize(num_face_varying_channels);
  _fvarParamBuffers.resize(num_face_varying_channels);
  for (int index = 0; index < num_face_varying_channels; index++) {
    /* Face varying patch arrays */
    _fvarPatchArrays[index].assign(patch_table.GetFVarPatchArrayBuffer(),
                                   patch_table.GetFVarPatchArrayBuffer() + num_patch_arrays);

    /* Face varying patch index buffer */
    _fvarIndexBuffers[index] = GPU_storagebuf_create_ex(patch_table.GetFVarPatchIndexSize(index) *
                                                            sizeof(int32_t),
                                                        patch_table.GetFVarPatchIndexBuffer(index),
                                                        GPU_USAGE_STATIC,
                                                        "osd_face_varying_index");

    /* Face varying patch param buffer */
    _fvarParamBuffers[index] = GPU_storagebuf_create_ex(patch_table.GetFVarPatchParamSize(index) *
                                                            sizeof(PatchParam),
                                                        patch_table.GetFVarPatchParamBuffer(index),
                                                        GPU_USAGE_STATIC,
                                                        "osd_face_varying_params");
  }

  return true;
}

}  // namespace blender::opensubdiv
