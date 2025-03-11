/* SPDX-FileCopyrightText: 2015 Pixar
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef OPENSUBDIV_GPU_COMPUTE_EVALUATOR_H_
#define OPENSUBDIV_GPU_COMPUTE_EVALUATOR_H_

#include <opensubdiv/osd/bufferDescriptor.h>
#include <opensubdiv/osd/types.h>
#include <opensubdiv/version.h>

#include "GPU_storage_buffer.hh"

namespace OpenSubdiv::OPENSUBDIV_VERSION::Far {
class LimitStencilTable;
class StencilTable;
}  // namespace OpenSubdiv::OPENSUBDIV_VERSION::Far
   // namespace OPENSUBDIV_VERSION

namespace blender::opensubdiv {

/// \brief GL stencil table (Shader Storage buffer)
///
/// This class is a GLSL SSBO representation of OpenSubdiv::Far::StencilTable.
///
/// GLSLComputeKernel consumes this table to apply stencils
///
class GPUStencilTableSSBO {
 public:
  static GPUStencilTableSSBO *Create(OpenSubdiv::Far::StencilTable const *stencilTable,
                                     void *deviceContext = nullptr)
  {
    (void)deviceContext;  // unused
    return new GPUStencilTableSSBO(stencilTable);
  }
  static GPUStencilTableSSBO *Create(OpenSubdiv::Far::LimitStencilTable const *limitStencilTable,
                                     void *deviceContext = nullptr)
  {
    (void)deviceContext;  // unused
    return new GPUStencilTableSSBO(limitStencilTable);
  }

  explicit GPUStencilTableSSBO(OpenSubdiv::Far::StencilTable const *stencilTable);
  explicit GPUStencilTableSSBO(OpenSubdiv::Far::LimitStencilTable const *limitStencilTable);
  ~GPUStencilTableSSBO();

  // interfaces needed for GLSLComputeKernel
  GPUStorageBuf *GetSizesBuffer() const
  {
    return sizes_buf;
  }
  GPUStorageBuf *GetOffsetsBuffer() const
  {
    return offsets_buf;
  }
  GPUStorageBuf *GetIndicesBuffer() const
  {
    return indices_buf;
  }
  GPUStorageBuf *GetWeightsBuffer() const
  {
    return weights_buf;
  }
  GPUStorageBuf *GetDuWeightsBuffer() const
  {
    return du_weights_buf;
  }
  GPUStorageBuf *GetDvWeightsBuffer() const
  {
    return dv_weights_buf;
  }
  GPUStorageBuf *GetDuuWeightsBuffer() const
  {
    return duu_weights_buf;
  }
  GPUStorageBuf *GetDuvWeightsBuffer() const
  {
    return duv_weights_buf;
  }
  GPUStorageBuf *GetDvvWeightsBuffer() const
  {
    return dvv_weights_buf;
  }
  int GetNumStencils() const
  {
    return _numStencils;
  }

 private:
  GPUStorageBuf *sizes_buf = nullptr;
  GPUStorageBuf *offsets_buf = nullptr;
  GPUStorageBuf *indices_buf = nullptr;
  GPUStorageBuf *weights_buf = nullptr;
  GPUStorageBuf *du_weights_buf = nullptr;
  GPUStorageBuf *dv_weights_buf = nullptr;
  GPUStorageBuf *duu_weights_buf = nullptr;
  GPUStorageBuf *duv_weights_buf = nullptr;
  GPUStorageBuf *dvv_weights_buf = nullptr;
  int _numStencils;
};

// ---------------------------------------------------------------------------

class GPUComputeEvaluator {
 public:
  using Instantiatable = bool;
  /**
   * Blender doesn't use 2nd derivatives, but the OSD evaluator cache does expect this constructor
   * to be present.
   */
  static GPUComputeEvaluator *Create(OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                                     OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                                     OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                                     OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                                     OpenSubdiv::Osd::BufferDescriptor const & /*duuDesc*/,
                                     OpenSubdiv::Osd::BufferDescriptor const & /*duvDesc*/,
                                     OpenSubdiv::Osd::BufferDescriptor const & /*dvvDesc*/,
                                     void *deviceContext = nullptr)
  {
    return Create(srcDesc, dstDesc, duDesc, dvDesc, deviceContext);
  }

  static GPUComputeEvaluator *Create(OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                                     OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                                     OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                                     OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                                     void * /*deviceContext*/ = nullptr)
  {
    GPUComputeEvaluator *instance = new GPUComputeEvaluator();
    if (instance->Compile(srcDesc, dstDesc, duDesc, dvDesc)) {
      return instance;
    }
    delete instance;
    return nullptr;
  }

  /// Constructor.
  GPUComputeEvaluator();

  /// Destructor. note that the GL context must be made current.
  ~GPUComputeEvaluator();

  /// ----------------------------------------------------------------------
  ///
  ///   Stencil evaluations with StencilTable
  ///
  /// ----------------------------------------------------------------------

  /// \brief Generic static stencil function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        transparently from OsdMesh template interface.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param stencilTable   stencil table to be applied. The table must have
  ///                       SSBO interfaces.
  ///
  /// @param instance       cached compiled instance. Clients are supposed to
  ///                       pre-compile an instance of this class and provide
  ///                       to this function. If it's null the kernel still
  ///                       compute by instantiating on-demand kernel although
  ///                       it may cause a performance problem.
  ///
  /// @param deviceContext  not used in the GLSL kernel
  ///
  template<typename SRC_BUFFER, typename DST_BUFFER, typename STENCIL_TABLE>
  static bool EvalStencils(SRC_BUFFER *srcBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                           DST_BUFFER *dstBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                           STENCIL_TABLE const *stencilTable,
                           GPUComputeEvaluator *instance,
                           void *deviceContext = nullptr)
  {

    if (instance) {
      return instance->EvalStencils(srcBuffer, srcDesc, dstBuffer, dstDesc, stencilTable);
    }

    // Create an instance on demand (slow)
    (void)deviceContext;  // unused
    instance = Create(srcDesc,
                      dstDesc,
                      OpenSubdiv::Osd::BufferDescriptor(),
                      OpenSubdiv::Osd::BufferDescriptor());
    if (instance) {
      bool r = instance->EvalStencils(srcBuffer, srcDesc, dstBuffer, dstDesc, stencilTable);
      delete instance;
      return r;
    }
    return false;
  }

  /// \brief Generic static stencil function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        transparently from OsdMesh template interface.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the dstBuffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param stencilTable   stencil table to be applied. The table must have
  ///                       SSBO interfaces.
  ///
  /// @param instance       cached compiled instance. Clients are supposed to
  ///                       pre-compile an instance of this class and provide
  ///                       to this function. If it's null the kernel still
  ///                       compute by instantiating on-demand kernel although
  ///                       it may cause a performance problem.
  ///
  /// @param deviceContext  not used in the GLSL kernel
  ///
  template<typename SRC_BUFFER, typename DST_BUFFER, typename STENCIL_TABLE>
  static bool EvalStencils(SRC_BUFFER *srcBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                           DST_BUFFER *dstBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                           DST_BUFFER *duBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                           DST_BUFFER *dvBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                           STENCIL_TABLE const *stencilTable,
                           GPUComputeEvaluator *instance,
                           void *deviceContext = nullptr)
  {

    if (instance) {
      return instance->EvalStencils(srcBuffer,
                                    srcDesc,
                                    dstBuffer,
                                    dstDesc,
                                    duBuffer,
                                    duDesc,
                                    dvBuffer,
                                    dvDesc,
                                    stencilTable);
    }

    // Create an instance on demand (slow)
    (void)deviceContext;  // unused
    instance = Create(srcDesc, dstDesc, duDesc, dvDesc);
    if (instance) {
      bool r = instance->EvalStencils(srcBuffer,
                                      srcDesc,
                                      dstBuffer,
                                      dstDesc,
                                      duBuffer,
                                      duDesc,
                                      dvBuffer,
                                      dvDesc,
                                      stencilTable);
      delete instance;
      return r;
    }
    return false;
  }

  /// \brief Generic stencil function.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param stencilTable   stencil table to be applied. The table must have
  ///                       SSBO interfaces.
  ///
  template<typename SRC_BUFFER, typename DST_BUFFER, typename STENCIL_TABLE>
  bool EvalStencils(SRC_BUFFER *srcBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                    DST_BUFFER *dstBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                    STENCIL_TABLE const *stencilTable) const
  {
    return EvalStencils(srcBuffer->get_vertex_buffer(),
                        srcDesc,
                        dstBuffer->get_vertex_buffer(),
                        dstDesc,
                        nullptr,
                        OpenSubdiv::Osd::BufferDescriptor(),
                        nullptr,
                        OpenSubdiv::Osd::BufferDescriptor(),
                        stencilTable->GetSizesBuffer(),
                        stencilTable->GetOffsetsBuffer(),
                        stencilTable->GetIndicesBuffer(),
                        stencilTable->GetWeightsBuffer(),
                        0,
                        0,
                        /* start = */ 0,
                        /* end   = */ stencilTable->GetNumStencils());
  }

  /// \brief Generic stencil function.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the dstBuffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param stencilTable   stencil table to be applied. The table must have
  ///                       SSBO interfaces.
  ///
  template<typename SRC_BUFFER, typename DST_BUFFER, typename STENCIL_TABLE>
  bool EvalStencils(SRC_BUFFER *srcBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                    DST_BUFFER *dstBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                    DST_BUFFER *duBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                    DST_BUFFER *dvBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                    STENCIL_TABLE const *stencilTable) const
  {
    return EvalStencils(srcBuffer->get_vertex_buffer(),
                        srcDesc,
                        dstBuffer->get_vertex_buffer(),
                        dstDesc,
                        duBuffer->get_vertex_buffer(),
                        duDesc,
                        dvBuffer->get_vertex_buffer(),
                        dvDesc,
                        stencilTable->GetSizesBuffer(),
                        stencilTable->GetOffsetsBuffer(),
                        stencilTable->GetIndicesBuffer(),
                        stencilTable->GetWeightsBuffer(),
                        stencilTable->GetDuWeightsBuffer(),
                        stencilTable->GetDvWeightsBuffer(),
                        /* start = */ 0,
                        /* end   = */ stencilTable->GetNumStencils());
  }

  /// \brief Dispatch the GLSL compute kernel on GPU asynchronously
  /// returns false if the kernel hasn't been compiled yet.
  ///
  /// @param srcBuffer        GL buffer of input primvar source data
  ///
  /// @param srcDesc          vertex buffer descriptor for the srcBuffer
  ///
  /// @param dstBuffer        GL buffer of output primvar destination data
  ///
  /// @param dstDesc          vertex buffer descriptor for the dstBuffer
  ///
  /// @param duBuffer         GL buffer of output derivative wrt u
  ///
  /// @param duDesc           vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer         GL buffer of output derivative wrt v
  ///
  /// @param dvDesc           vertex buffer descriptor for the dvBuffer
  ///
  /// @param sizesBuffer      GL buffer of the sizes in the stencil table
  ///
  /// @param offsetsBuffer    GL buffer of the offsets in the stencil table
  ///
  /// @param indicesBuffer    GL buffer of the indices in the stencil table
  ///
  /// @param weightsBuffer    GL buffer of the weights in the stencil table
  ///
  /// @param duWeightsBuffer  GL buffer of the du weights in the stencil table
  ///
  /// @param dvWeightsBuffer  GL buffer of the dv weights in the stencil table
  ///
  /// @param start            start index of stencil table
  ///
  /// @param end              end index of stencil table
  ///
  bool EvalStencils(gpu::VertBuf *srcBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                    gpu::VertBuf *dstBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                    gpu::VertBuf *duBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                    gpu::VertBuf *dvBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                    GPUStorageBuf *sizesBuffer,
                    GPUStorageBuf *offsetsBuffer,
                    GPUStorageBuf *indicesBuffer,
                    GPUStorageBuf *weightsBuffer,
                    GPUStorageBuf *duWeightsBuffer,
                    GPUStorageBuf *dvWeightsBuffer,
                    int start,
                    int end) const;

  /// ----------------------------------------------------------------------
  ///
  ///   Limit evaluations with PatchTable
  ///
  /// ----------------------------------------------------------------------

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  /// @param instance       cached compiled instance. Clients are supposed to
  ///                       pre-compile an instance of this class and provide
  ///                       to this function. If it's null the kernel still
  ///                       compute by instantiating on-demand kernel although
  ///                       it may cause a performance problem.
  ///
  /// @param deviceContext  not used in the GLXFB evaluator
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  static bool EvalPatches(SRC_BUFFER *srcBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                          DST_BUFFER *dstBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                          int numPatchCoords,
                          PATCHCOORD_BUFFER *patchCoords,
                          PATCH_TABLE *patchTable,
                          GPUComputeEvaluator *instance,
                          void *deviceContext = nullptr)
  {

    if (instance) {
      return instance->EvalPatches(
          srcBuffer, srcDesc, dstBuffer, dstDesc, numPatchCoords, patchCoords, patchTable);
    }
    // Create an instance on demand (slow)
    (void)deviceContext;  // unused
    instance = Create(srcDesc,
                      dstDesc,
                      OpenSubdiv::Osd::BufferDescriptor(),
                      OpenSubdiv::Osd::BufferDescriptor());
    if (instance) {
      bool r = instance->EvalPatches(
          srcBuffer, srcDesc, dstBuffer, dstDesc, numPatchCoords, patchCoords, patchTable);
      delete instance;
      return r;
    }
    return false;
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  /// @param instance       cached compiled instance. Clients are supposed to
  ///                       pre-compile an instance of this class and provide
  ///                       to this function. If it's null the kernel still
  ///                       compute by instantiating on-demand kernel although
  ///                       it may cause a performance problem.
  ///
  /// @param deviceContext  not used in the GLXFB evaluator
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  static bool EvalPatches(SRC_BUFFER *srcBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                          DST_BUFFER *dstBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                          DST_BUFFER *duBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                          DST_BUFFER *dvBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                          int numPatchCoords,
                          PATCHCOORD_BUFFER *patchCoords,
                          PATCH_TABLE *patchTable,
                          GPUComputeEvaluator *instance,
                          void *deviceContext = nullptr)
  {
    if (instance) {
      return instance->EvalPatches(srcBuffer,
                                   srcDesc,
                                   dstBuffer,
                                   dstDesc,
                                   duBuffer,
                                   duDesc,
                                   dvBuffer,
                                   dvDesc,
                                   numPatchCoords,
                                   patchCoords,
                                   patchTable);
    }

    // Create an instance on demand (slow)
    (void)deviceContext;  // unused
    instance = Create(srcDesc, dstDesc, duDesc, dvDesc);
    if (instance) {
      bool r = instance->EvalPatches(srcBuffer,
                                     srcDesc,
                                     dstBuffer,
                                     dstDesc,
                                     duBuffer,
                                     duDesc,
                                     dvBuffer,
                                     dvDesc,
                                     numPatchCoords,
                                     patchCoords,
                                     patchTable);
      delete instance;
      return r;
    }
    return false;
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  bool EvalPatches(SRC_BUFFER *srcBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                   DST_BUFFER *dstBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                   int numPatchCoords,
                   PATCHCOORD_BUFFER *patchCoords,
                   PATCH_TABLE *patchTable)
  {

    return EvalPatches(srcBuffer->get_vertex_buffer(),
                       srcDesc,
                       dstBuffer->get_vertex_buffer(),
                       dstDesc,
                       nullptr,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       nullptr,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       numPatchCoords,
                       patchCoords->get_vertex_buffer(),
                       patchTable->GetPatchArrays(),
                       patchTable->GetPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  /// \brief Generic limit eval function with derivatives. This function has
  ///        a same signature as other device kernels have so that it can be
  ///        called in the same way.
  ///
  /// @param srcBuffer        Input primvar buffer.
  ///                         Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                         buffer object of source data
  ///
  /// @param srcDesc          vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer        Output primvar buffer
  ///                         Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                         buffer object of destination data
  ///
  /// @param dstDesc          vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer         Output buffer derivative wrt u
  ///                         Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                         buffer object of destination data
  ///
  /// @param duDesc           vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer         Output buffer derivative wrt v
  ///                         Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                         buffer object of destination data
  ///
  /// @param dvDesc           vertex buffer descriptor for the dvBuffer
  ///
  /// @param numPatchCoords   number of patchCoords.
  ///
  /// @param patchCoords      array of locations to be evaluated.
  ///
  /// @param patchTable       GLPatchTable or equivalent
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  bool EvalPatches(SRC_BUFFER *srcBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                   DST_BUFFER *dstBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                   DST_BUFFER *duBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                   DST_BUFFER *dvBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                   int numPatchCoords,
                   PATCHCOORD_BUFFER *patchCoords,
                   PATCH_TABLE *patchTable)
  {

    return EvalPatches(srcBuffer->get_vertex_buffer(),
                       srcDesc,
                       dstBuffer->get_vertex_buffer(),
                       dstDesc,
                       duBuffer->get_vertex_buffer(),
                       duDesc,
                       dvBuffer->get_vertex_buffer(),
                       dvDesc,
                       numPatchCoords,
                       patchCoords->get_vertex_buffer(),
                       patchTable->GetPatchArrays(),
                       patchTable->GetPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  bool EvalPatches(gpu::VertBuf *srcBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                   gpu::VertBuf *dstBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                   gpu::VertBuf *duBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                   gpu::VertBuf *dvBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                   int numPatchCoords,
                   gpu::VertBuf *patchCoordsBuffer,
                   const OpenSubdiv::Osd::PatchArrayVector &patchArrays,
                   GPUStorageBuf *patchIndexBuffer,
                   GPUStorageBuf *patchParamsBuffer);

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  /// @param instance       cached compiled instance. Clients are supposed to
  ///                       pre-compile an instance of this class and provide
  ///                       to this function. If it's null the kernel still
  ///                       compute by instantiating on-demand kernel although
  ///                       it may cause a performance problem.
  ///
  /// @param deviceContext  not used in the GLXFB evaluator
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  static bool EvalPatchesVarying(SRC_BUFFER *srcBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                                 DST_BUFFER *dstBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                                 int numPatchCoords,
                                 PATCHCOORD_BUFFER *patchCoords,
                                 PATCH_TABLE *patchTable,
                                 GPUComputeEvaluator *instance,
                                 void *deviceContext = nullptr)
  {
    if (instance) {
      return instance->EvalPatchesVarying(
          srcBuffer, srcDesc, dstBuffer, dstDesc, numPatchCoords, patchCoords, patchTable);
    }

    // Create an instance on demand (slow)
    (void)deviceContext;  // unused
    instance = Create(srcDesc,
                      dstDesc,
                      OpenSubdiv::Osd::BufferDescriptor(),
                      OpenSubdiv::Osd::BufferDescriptor());
    if (instance) {
      bool r = instance->EvalPatchesVarying(
          srcBuffer, srcDesc, dstBuffer, dstDesc, numPatchCoords, patchCoords, patchTable);
      delete instance;
      return r;
    }
    return false;
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  bool EvalPatchesVarying(SRC_BUFFER *srcBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                          DST_BUFFER *dstBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                          int numPatchCoords,
                          PATCHCOORD_BUFFER *patchCoords,
                          PATCH_TABLE *patchTable)
  {

    return EvalPatches(srcBuffer->get_vertex_buffer(),
                       srcDesc,
                       dstBuffer->get_vertex_buffer(),
                       dstDesc,
                       nullptr,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       nullptr,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       numPatchCoords,
                       patchCoords->get_vertex_buffer(),
                       patchTable->GetVaryingPatchArrays(),
                       patchTable->GetVaryingPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  /// @param instance       cached compiled instance. Clients are supposed to
  ///                       pre-compile an instance of this class and provide
  ///                       to this function. If it's null the kernel still
  ///                       compute by instantiating on-demand kernel although
  ///                       it may cause a performance problem.
  ///
  /// @param deviceContext  not used in the GLXFB evaluator
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  static bool EvalPatchesVarying(SRC_BUFFER *srcBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                                 DST_BUFFER *dstBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                                 DST_BUFFER *duBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                                 DST_BUFFER *dvBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                                 int numPatchCoords,
                                 PATCHCOORD_BUFFER *patchCoords,
                                 PATCH_TABLE *patchTable,
                                 GPUComputeEvaluator *instance,
                                 void *deviceContext = nullptr)
  {
    if (instance) {
      return instance->EvalPatchesVarying(srcBuffer,
                                          srcDesc,
                                          dstBuffer,
                                          dstDesc,
                                          duBuffer,
                                          duDesc,
                                          dvBuffer,
                                          dvDesc,
                                          numPatchCoords,
                                          patchCoords,
                                          patchTable);
    }

    // Create an instance on demand (slow)
    (void)deviceContext;  // unused
    instance = Create(srcDesc, dstDesc, duDesc, dvDesc);
    if (instance) {
      bool r = instance->EvalPatchesVarying(srcBuffer,
                                            srcDesc,
                                            dstBuffer,
                                            dstDesc,
                                            duBuffer,
                                            duDesc,
                                            dvBuffer,
                                            dvDesc,
                                            numPatchCoords,
                                            patchCoords,
                                            patchTable);
      delete instance;
      return r;
    }
    return false;
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  bool EvalPatchesVarying(SRC_BUFFER *srcBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                          DST_BUFFER *dstBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                          DST_BUFFER *duBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                          DST_BUFFER *dvBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                          int numPatchCoords,
                          PATCHCOORD_BUFFER *patchCoords,
                          PATCH_TABLE *patchTable)
  {

    return EvalPatches(srcBuffer->get_vertex_buffer(),
                       srcDesc,
                       dstBuffer->get_vertex_buffer(),
                       dstDesc,
                       duBuffer->get_vertex_buffer(),
                       duDesc,
                       dvBuffer->get_vertex_buffer(),
                       dvDesc,
                       numPatchCoords,
                       patchCoords->get_vertex_buffer(),
                       patchTable->GetVaryingPatchArrays(),
                       patchTable->GetVaryingPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  /// @param fvarChannel    face-varying channel
  ///
  /// @param instance       cached compiled instance. Clients are supposed to
  ///                       pre-compile an instance of this class and provide
  ///                       to this function. If it's null the kernel still
  ///                       compute by instantiating on-demand kernel although
  ///                       it may cause a performance problem.
  ///
  /// @param deviceContext  not used in the GLXFB evaluator
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  static bool EvalPatchesFaceVarying(SRC_BUFFER *srcBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                                     DST_BUFFER *dstBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                                     int numPatchCoords,
                                     PATCHCOORD_BUFFER *patchCoords,
                                     PATCH_TABLE *patchTable,
                                     int fvarChannel,
                                     GPUComputeEvaluator *instance,
                                     void *deviceContext = nullptr)
  {
    if (instance) {
      return instance->EvalPatchesFaceVarying(srcBuffer,
                                              srcDesc,
                                              dstBuffer,
                                              dstDesc,
                                              numPatchCoords,
                                              patchCoords,
                                              patchTable,
                                              fvarChannel);
    }

    // Create an instance on demand (slow)
    (void)deviceContext;  // unused
    instance = Create(srcDesc,
                      dstDesc,
                      OpenSubdiv::Osd::BufferDescriptor(),
                      OpenSubdiv::Osd::BufferDescriptor());
    if (instance) {
      bool r = instance->EvalPatchesFaceVarying(srcBuffer,
                                                srcDesc,
                                                dstBuffer,
                                                dstDesc,
                                                numPatchCoords,
                                                patchCoords,
                                                patchTable,
                                                fvarChannel);
      delete instance;
      return r;
    }
    return false;
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  /// @param fvarChannel    face-varying channel
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  bool EvalPatchesFaceVarying(SRC_BUFFER *srcBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                              DST_BUFFER *dstBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                              int numPatchCoords,
                              PATCHCOORD_BUFFER *patchCoords,
                              PATCH_TABLE *patchTable,
                              int fvarChannel = 0)
  {

    return EvalPatches(srcBuffer->get_vertex_buffer(),
                       srcDesc,
                       dstBuffer->get_vertex_buffer(),
                       dstDesc,
                       0,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       0,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       numPatchCoords,
                       patchCoords->get_vertex_buffer(),
                       patchTable->GetFVarPatchArrays(fvarChannel),
                       patchTable->GetFVarPatchIndexBuffer(fvarChannel),
                       patchTable->GetFVarPatchParamBuffer(fvarChannel));
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  /// @param fvarChannel    face-varying channel
  ///
  /// @param instance       cached compiled instance. Clients are supposed to
  ///                       pre-compile an instance of this class and provide
  ///                       to this function. If it's null the kernel still
  ///                       compute by instantiating on-demand kernel although
  ///                       it may cause a performance problem.
  ///
  /// @param deviceContext  not used in the GLXFB evaluator
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  static bool EvalPatchesFaceVarying(SRC_BUFFER *srcBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                                     DST_BUFFER *dstBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                                     DST_BUFFER *duBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                                     DST_BUFFER *dvBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                                     int numPatchCoords,
                                     PATCHCOORD_BUFFER *patchCoords,
                                     PATCH_TABLE *patchTable,
                                     int fvarChannel,
                                     GPUComputeEvaluator *instance,
                                     void *deviceContext = nullptr)
  {
    if (instance) {
      return instance->EvalPatchesFaceVarying(srcBuffer,
                                              srcDesc,
                                              dstBuffer,
                                              dstDesc,
                                              duBuffer,
                                              duDesc,
                                              dvBuffer,
                                              dvDesc,
                                              numPatchCoords,
                                              patchCoords,
                                              patchTable,
                                              fvarChannel);
    }

    // Create an instance on demand (slow)
    (void)deviceContext;  // unused
    instance = Create(srcDesc, dstDesc, duDesc, dvDesc);
    if (instance) {
      bool r = instance->EvalPatchesFaceVarying(srcBuffer,
                                                srcDesc,
                                                dstBuffer,
                                                dstDesc,
                                                duBuffer,
                                                duDesc,
                                                dvBuffer,
                                                dvDesc,
                                                numPatchCoords,
                                                patchCoords,
                                                patchTable,
                                                fvarChannel);
      delete instance;
      return r;
    }
    return false;
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       Must have `get_vertex_buffer()` returning a `gpu::VertBuf`
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param numPatchCoords number of patchCoords.
  ///
  /// @param patchCoords    array of locations to be evaluated.
  ///                       must have BindVBO() method returning an
  ///                       array of PatchCoord struct in VBO.
  ///
  /// @param patchTable     GLPatchTable or equivalent
  ///
  /// @param fvarChannel    face-varying channel
  ///
  template<typename SRC_BUFFER,
           typename DST_BUFFER,
           typename PATCHCOORD_BUFFER,
           typename PATCH_TABLE>
  bool EvalPatchesFaceVarying(SRC_BUFFER *srcBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                              DST_BUFFER *dstBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                              DST_BUFFER *duBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                              DST_BUFFER *dvBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                              int numPatchCoords,
                              PATCHCOORD_BUFFER *patchCoords,
                              PATCH_TABLE *patchTable,
                              int fvarChannel = 0)
  {

    return EvalPatches(srcBuffer->get_vertex_buffer(),
                       srcDesc,
                       dstBuffer->get_vertex_buffer(),
                       dstDesc,
                       duBuffer->get_vertex_buffer(),
                       duDesc,
                       dvBuffer->get_vertex_buffer(),
                       dvDesc,
                       numPatchCoords,
                       patchCoords->get_vertex_buffer(),
                       patchTable->GetFVarPatchArrays(fvarChannel),
                       patchTable->GetFVarPatchIndexBuffer(fvarChannel),
                       patchTable->GetFVarPatchParamBuffer(fvarChannel));
  }

  /// ----------------------------------------------------------------------
  ///
  ///   Other methods
  ///
  /// ----------------------------------------------------------------------

  /// Configure GLSL kernel. A valid GL context must be made current before
  /// calling this function. Returns false if it fails to compile the kernel.
  bool Compile(
      OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
      OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
      OpenSubdiv::Osd::BufferDescriptor const &duDesc = OpenSubdiv::Osd::BufferDescriptor(),
      OpenSubdiv::Osd::BufferDescriptor const &dvDesc = OpenSubdiv::Osd::BufferDescriptor());

  /// Wait the dispatched kernel finishes.
  static void Synchronize(void *deviceContext);

 private:
  struct _StencilKernel {
    _StencilKernel();
    ~_StencilKernel();
    bool Compile(OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                 int workGroupSize);
    GPUShader *shader = nullptr;
    int uniformStart = 0;
    int uniformEnd = 0;
    int uniformSrcOffset = 0;
    int uniformDstOffset = 0;
    int uniformDuDesc = 0;
    int uniformDvDesc = 0;
  } _stencilKernel;

  struct _PatchKernel {
    _PatchKernel();
    ~_PatchKernel();
    bool Compile(OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                 int workGroupSize);
    GPUShader *shader = nullptr;
    int uniformSrcOffset = 0;
    int uniformDstOffset = 0;
    int uniformDuDesc = 0;
    int uniformDvDesc = 0;
  } _patchKernel;

  int _workGroupSize;
  GPUStorageBuf *_patchArraysSSBO = nullptr;

  int GetDispatchSize(int count) const;

  void DispatchCompute(GPUShader *shader, int totalDispatchSize) const;
};

}  // namespace blender::opensubdiv

#endif  // OPENSUBDIV_GPU_COMPUTE_EVALUATOR_H_
