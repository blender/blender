/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gpu_patch_table.hh"

#include "opensubdiv/far/patchTable.h"
#include "opensubdiv/osd/cpuPatchTable.h"

using namespace OpenSubdiv::Osd;

namespace blender::opensubdiv {

GPUPatchTable *GPUPatchTable::Create(PatchTable const *far_patch_table, void * /*deviceContext*/)
{
  GPUPatchTable *instance = new GPUPatchTable();
  if (instance->allocate(far_patch_table))
    return instance;
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

/**
 * Storage buffers sizes needs to be divisable by 16 (float4).
 */
static size_t storage_buffer_size(size_t size)
{
  return (size + 15) & (~0b1111);
}

/**
 * Function to create a storage buffer and upload it with data.
 *
 * - Ensures that allocated size is aligned to 16 byte
 * - WARNING: Can read from not allocated data after `data`.
 */
// TODO: this means that if buffer size is adjusted we need to copy into a temp buffer otherwise we
// could read out of bounds. The performance impact of this is measurable so I would suggest to
// support GPU_storagebuf_update() with a max len to update.
static GPUStorageBuf *storage_buffer_create(size_t size, const void *data, const char *name)
{
  size_t storage_size = storage_buffer_size(size);
  return GPU_storagebuf_create_ex(storage_size, data, GPU_USAGE_STATIC, name);
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
  _patchIndexBuffer = storage_buffer_create(
      index_size * sizeof(int32_t),
      static_cast<const void *>(patch_table.GetPatchIndexBuffer()),
      "osd_patch_index");

  /* Patch param buffer */
  const size_t patch_param_size = patch_table.GetPatchParamSize();
  _patchParamBuffer = storage_buffer_create(
      patch_param_size * sizeof(PatchParam), patch_table.GetPatchParamBuffer(), "osd_patch_param");

  /* Varying patch array */
  _varyingPatchArrays.assign(patch_table.GetVaryingPatchArrayBuffer(),
                             patch_table.GetVaryingPatchArrayBuffer() + num_patch_arrays);

  /* Varying index buffer */
  _varyingIndexBuffer = storage_buffer_create(patch_table.GetVaryingPatchIndexSize() *
                                                  sizeof(uint32_t),
                                              patch_table.GetVaryingPatchIndexBuffer(),
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
    _fvarIndexBuffers[index] = storage_buffer_create(patch_table.GetFVarPatchIndexSize(index) *
                                                         sizeof(int32_t),
                                                     patch_table.GetFVarPatchIndexBuffer(index),
                                                     "osd_face_varying_index");

    /* Face varying patch param buffer */
    _fvarParamBuffers[index] = storage_buffer_create(patch_table.GetFVarPatchParamSize(index) *
                                                         sizeof(PatchParam),
                                                     patch_table.GetFVarPatchParamBuffer(index),
                                                     "osd_face_varying_params");
  }

  return true;
}

}  // namespace blender::opensubdiv
