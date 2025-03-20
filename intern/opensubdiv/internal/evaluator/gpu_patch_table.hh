/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_storage_buffer.hh"

#include <opensubdiv/version.h>

#include <opensubdiv/osd/nonCopyable.h>
#include <opensubdiv/osd/types.h>

using OpenSubdiv::Far::PatchTable;
using OpenSubdiv::Osd::NonCopyable;
using OpenSubdiv::Osd::PatchArrayVector;

namespace blender::opensubdiv {

// TODO: use Blenlib NonCopyable.
class GPUPatchTable : private NonCopyable<GPUPatchTable> {
 public:
  ~GPUPatchTable();

  static GPUPatchTable *Create(PatchTable const *farPatchTable, void *deviceContext = NULL);

  /// Returns the patch arrays for vertex index buffer data
  PatchArrayVector const &GetPatchArrays() const
  {
    return _patchArrays;
  }

  /// Returns the GL index buffer containing the patch control vertices
  GPUStorageBuf *GetPatchIndexBuffer() const
  {
    return _patchIndexBuffer;
  }

  /// Returns the GL index buffer containing the patch parameter
  GPUStorageBuf *GetPatchParamBuffer() const
  {
    return _patchParamBuffer;
  }

  /// Returns the patch arrays for varying index buffer data
  PatchArrayVector const &GetVaryingPatchArrays() const
  {
    return _varyingPatchArrays;
  }

  /// Returns the GL index buffer containing the varying control vertices
  GPUStorageBuf *GetVaryingPatchIndexBuffer() const
  {
    return _varyingIndexBuffer;
  }

  /// Returns the number of face-varying channel buffers
  int GetNumFVarChannels() const
  {
    return (int)_fvarPatchArrays.size();
  }

  /// Returns the patch arrays for face-varying index buffer data
  PatchArrayVector const &GetFVarPatchArrays(int fvarChannel = 0) const
  {
    return _fvarPatchArrays[fvarChannel];
  }

  /// Returns the GL index buffer containing face-varying control vertices
  GPUStorageBuf *GetFVarPatchIndexBuffer(int fvarChannel = 0) const
  {
    return _fvarIndexBuffers[fvarChannel];
  }

  /// Returns the GL index buffer containing face-varying patch params
  GPUStorageBuf *GetFVarPatchParamBuffer(int fvarChannel = 0) const
  {
    return _fvarParamBuffers[fvarChannel];
  }

 protected:
  GPUPatchTable() {}

  // allocate buffers from patchTable
  bool allocate(PatchTable const *farPatchTable);

  PatchArrayVector _patchArrays;

  GPUStorageBuf *_patchIndexBuffer = nullptr;
  GPUStorageBuf *_patchParamBuffer = nullptr;

  PatchArrayVector _varyingPatchArrays;
  GPUStorageBuf *_varyingIndexBuffer = nullptr;

  std::vector<PatchArrayVector> _fvarPatchArrays;
  std::vector<GPUStorageBuf *> _fvarIndexBuffers;
  std::vector<GPUStorageBuf *> _fvarParamBuffers;
};

}  // namespace blender::opensubdiv
