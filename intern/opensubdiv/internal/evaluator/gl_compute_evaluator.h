/* SPDX-FileCopyrightText: 2015 Pixar
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef OPENSUBDIV_GL_COMPUTE_EVALUATOR_H_
#define OPENSUBDIV_GL_COMPUTE_EVALUATOR_H_

#include <opensubdiv/osd/bufferDescriptor.h>
#include <opensubdiv/osd/opengl.h>
#include <opensubdiv/osd/types.h>
#include <opensubdiv/version.h>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {
namespace Far {
class LimitStencilTable;
class StencilTable;
}  // namespace Far
}  // namespace OPENSUBDIV_VERSION
}  // namespace OpenSubdiv

namespace blender::opensubdiv {

/// \brief GL stencil table (Shader Storage buffer)
///
/// This class is a GLSL SSBO representation of OpenSubdiv::Far::StencilTable.
///
/// GLSLComputeKernel consumes this table to apply stencils
///
class GLStencilTableSSBO {
 public:
  static GLStencilTableSSBO *Create(OpenSubdiv::Far::StencilTable const *stencilTable,
                                    void *deviceContext = NULL)
  {
    (void)deviceContext;  // unused
    return new GLStencilTableSSBO(stencilTable);
  }
  static GLStencilTableSSBO *Create(OpenSubdiv::Far::LimitStencilTable const *limitStencilTable,
                                    void *deviceContext = NULL)
  {
    (void)deviceContext;  // unused
    return new GLStencilTableSSBO(limitStencilTable);
  }

  explicit GLStencilTableSSBO(OpenSubdiv::Far::StencilTable const *stencilTable);
  explicit GLStencilTableSSBO(OpenSubdiv::Far::LimitStencilTable const *limitStencilTable);
  ~GLStencilTableSSBO();

  // interfaces needed for GLSLComputeKernel
  GLuint GetSizesBuffer() const
  {
    return _sizes;
  }
  GLuint GetOffsetsBuffer() const
  {
    return _offsets;
  }
  GLuint GetIndicesBuffer() const
  {
    return _indices;
  }
  GLuint GetWeightsBuffer() const
  {
    return _weights;
  }
  GLuint GetDuWeightsBuffer() const
  {
    return _duWeights;
  }
  GLuint GetDvWeightsBuffer() const
  {
    return _dvWeights;
  }
  GLuint GetDuuWeightsBuffer() const
  {
    return _duuWeights;
  }
  GLuint GetDuvWeightsBuffer() const
  {
    return _duvWeights;
  }
  GLuint GetDvvWeightsBuffer() const
  {
    return _dvvWeights;
  }
  int GetNumStencils() const
  {
    return _numStencils;
  }

 private:
  GLuint _sizes;
  GLuint _offsets;
  GLuint _indices;
  GLuint _weights;
  GLuint _duWeights;
  GLuint _dvWeights;
  GLuint _duuWeights;
  GLuint _duvWeights;
  GLuint _dvvWeights;
  int _numStencils;
};

// ---------------------------------------------------------------------------

class GLComputeEvaluator {
 public:
  typedef bool Instantiatable;
  static GLComputeEvaluator *Create(OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                                    void *deviceContext = NULL)
  {
    return Create(srcDesc,
                  dstDesc,
                  duDesc,
                  dvDesc,
                  OpenSubdiv::Osd::BufferDescriptor(),
                  OpenSubdiv::Osd::BufferDescriptor(),
                  OpenSubdiv::Osd::BufferDescriptor(),
                  deviceContext);
  }

  static GLComputeEvaluator *Create(OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                                    OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                                    void *deviceContext = NULL)
  {
    (void)deviceContext;  // not used
    GLComputeEvaluator *instance = new GLComputeEvaluator();
    if (instance->Compile(srcDesc, dstDesc, duDesc, dvDesc, duuDesc, duvDesc, dvvDesc)) {
      return instance;
    }
    delete instance;
    return NULL;
  }

  /// Constructor.
  GLComputeEvaluator();

  /// Destructor. note that the GL context must be made current.
  ~GLComputeEvaluator();

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
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
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
                           GLComputeEvaluator const *instance,
                           void *deviceContext = NULL)
  {

    if (instance) {
      return instance->EvalStencils(srcBuffer, srcDesc, dstBuffer, dstDesc, stencilTable);
    }
    else {
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
  }

  /// \brief Generic static stencil function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        transparently from OsdMesh template interface.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the dstBuffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
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
                           GLComputeEvaluator const *instance,
                           void *deviceContext = NULL)
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
    else {
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
  }

  /// \brief Generic static stencil function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        transparently from OsdMesh template interface.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the dstBuffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param duuBuffer      Output buffer 2nd derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duuDesc        vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer      Output buffer 2nd derivative wrt u and v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duvDesc        vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer      Output buffer 2nd derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvvDesc        vertex buffer descriptor for the dvvBuffer
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
                           DST_BUFFER *duuBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                           DST_BUFFER *duvBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                           DST_BUFFER *dvvBuffer,
                           OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                           STENCIL_TABLE const *stencilTable,
                           GLComputeEvaluator const *instance,
                           void *deviceContext = NULL)
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
                                    duuBuffer,
                                    duuDesc,
                                    duvBuffer,
                                    duvDesc,
                                    dvvBuffer,
                                    dvvDesc,
                                    stencilTable);
    }
    else {
      // Create an instance on demand (slow)
      (void)deviceContext;  // unused
      instance = Create(srcDesc, dstDesc, duDesc, dvDesc, duuDesc, duvDesc, dvvDesc);
      if (instance) {
        bool r = instance->EvalStencils(srcBuffer,
                                        srcDesc,
                                        dstBuffer,
                                        dstDesc,
                                        duBuffer,
                                        duDesc,
                                        dvBuffer,
                                        dvDesc,
                                        duuBuffer,
                                        duuDesc,
                                        duvBuffer,
                                        duvDesc,
                                        dvvBuffer,
                                        dvvDesc,
                                        stencilTable);
        delete instance;
        return r;
      }
      return false;
    }
  }

  /// \brief Generic stencil function.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
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
    return EvalStencils(srcBuffer->BindVBO(),
                        srcDesc,
                        dstBuffer->BindVBO(),
                        dstDesc,
                        0,
                        OpenSubdiv::Osd::BufferDescriptor(),
                        0,
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
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the dstBuffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
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
    return EvalStencils(srcBuffer->BindVBO(),
                        srcDesc,
                        dstBuffer->BindVBO(),
                        dstDesc,
                        duBuffer->BindVBO(),
                        duDesc,
                        dvBuffer->BindVBO(),
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

  /// \brief Generic stencil function.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the dstBuffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param duuBuffer      Output buffer 2nd derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duuDesc        vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer      Output buffer 2nd derivative wrt u and v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duvDesc        vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer      Output buffer 2nd derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvvDesc        vertex buffer descriptor for the dvvBuffer
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
                    DST_BUFFER *duuBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                    DST_BUFFER *duvBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                    DST_BUFFER *dvvBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                    STENCIL_TABLE const *stencilTable) const
  {
    return EvalStencils(srcBuffer->BindVBO(),
                        srcDesc,
                        dstBuffer->BindVBO(),
                        dstDesc,
                        duBuffer->BindVBO(),
                        duDesc,
                        dvBuffer->BindVBO(),
                        dvDesc,
                        duuBuffer->BindVBO(),
                        duuDesc,
                        duvBuffer->BindVBO(),
                        duvDesc,
                        dvvBuffer->BindVBO(),
                        dvvDesc,
                        stencilTable->GetSizesBuffer(),
                        stencilTable->GetOffsetsBuffer(),
                        stencilTable->GetIndicesBuffer(),
                        stencilTable->GetWeightsBuffer(),
                        stencilTable->GetDuWeightsBuffer(),
                        stencilTable->GetDvWeightsBuffer(),
                        stencilTable->GetDuuWeightsBuffer(),
                        stencilTable->GetDuvWeightsBuffer(),
                        stencilTable->GetDvvWeightsBuffer(),
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
  bool EvalStencils(GLuint srcBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                    GLuint dstBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                    GLuint duBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                    GLuint dvBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                    GLuint sizesBuffer,
                    GLuint offsetsBuffer,
                    GLuint indicesBuffer,
                    GLuint weightsBuffer,
                    GLuint duWeightsBuffer,
                    GLuint dvWeightsBuffer,
                    int start,
                    int end) const;

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
  /// @param duuBuffer        GL buffer of output 2nd derivative wrt u
  ///
  /// @param duuDesc          vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer        GL buffer of output 2nd derivative wrt u and v
  ///
  /// @param duvDesc          vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer        GL buffer of output 2nd derivative wrt v
  ///
  /// @param dvvDesc          vertex buffer descriptor for the dvvBuffer
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
  /// @param duuWeightsBuffer GL buffer of the duu weights in the stencil table
  ///
  /// @param duvWeightsBuffer GL buffer of the duv weights in the stencil table
  ///
  /// @param dvvWeightsBuffer GL buffer of the dvv weights in the stencil table
  ///
  /// @param start            start index of stencil table
  ///
  /// @param end              end index of stencil table
  ///
  bool EvalStencils(GLuint srcBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                    GLuint dstBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                    GLuint duBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                    GLuint dvBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                    GLuint duuBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                    GLuint duvBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                    GLuint dvvBuffer,
                    OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                    GLuint sizesBuffer,
                    GLuint offsetsBuffer,
                    GLuint indicesBuffer,
                    GLuint weightsBuffer,
                    GLuint duWeightsBuffer,
                    GLuint dvWeightsBuffer,
                    GLuint duuWeightsBuffer,
                    GLuint duvWeightsBuffer,
                    GLuint dvvWeightsBuffer,
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
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
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
                          GLComputeEvaluator const *instance,
                          void *deviceContext = NULL)
  {

    if (instance) {
      return instance->EvalPatches(
          srcBuffer, srcDesc, dstBuffer, dstDesc, numPatchCoords, patchCoords, patchTable);
    }
    else {
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
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
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
                          GLComputeEvaluator const *instance,
                          void *deviceContext = NULL)
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
    else {
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
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param duuBuffer      Output buffer 2nd derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duuDesc        vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer      Output buffer 2nd derivative wrt u and v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duvDesc        vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer      Output buffer 2nd derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvvDesc        vertex buffer descriptor for the dvvBuffer
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
                          DST_BUFFER *duuBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                          DST_BUFFER *duvBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                          DST_BUFFER *dvvBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                          int numPatchCoords,
                          PATCHCOORD_BUFFER *patchCoords,
                          PATCH_TABLE *patchTable,
                          GLComputeEvaluator const *instance,
                          void *deviceContext = NULL)
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
                                   duuBuffer,
                                   duuDesc,
                                   duvBuffer,
                                   duvDesc,
                                   dvvBuffer,
                                   dvvDesc,
                                   numPatchCoords,
                                   patchCoords,
                                   patchTable);
    }
    else {
      // Create an instance on demand (slow)
      (void)deviceContext;  // unused
      instance = Create(srcDesc, dstDesc, duDesc, dvDesc, duuDesc, duvDesc, dvvDesc);
      if (instance) {
        bool r = instance->EvalPatches(srcBuffer,
                                       srcDesc,
                                       dstBuffer,
                                       dstDesc,
                                       duBuffer,
                                       duDesc,
                                       dvBuffer,
                                       dvDesc,
                                       duuBuffer,
                                       duuDesc,
                                       duvBuffer,
                                       duvDesc,
                                       dvvBuffer,
                                       dvvDesc,
                                       numPatchCoords,
                                       patchCoords,
                                       patchTable);
        delete instance;
        return r;
      }
      return false;
    }
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
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
                   PATCH_TABLE *patchTable) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       0,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       0,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       numPatchCoords,
                       patchCoords->BindVBO(),
                       patchTable->GetPatchArrays(),
                       patchTable->GetPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  /// \brief Generic limit eval function with derivatives. This function has
  ///        a same signature as other device kernels have so that it can be
  ///        called in the same way.
  ///
  /// @param srcBuffer        Input primvar buffer.
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of source data
  ///
  /// @param srcDesc          vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer        Output primvar buffer
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of destination data
  ///
  /// @param dstDesc          vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer         Output buffer derivative wrt u
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of destination data
  ///
  /// @param duDesc           vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer         Output buffer derivative wrt v
  ///                         must have BindVBO() method returning a GL
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
                   PATCH_TABLE *patchTable) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       duBuffer->BindVBO(),
                       duDesc,
                       dvBuffer->BindVBO(),
                       dvDesc,
                       numPatchCoords,
                       patchCoords->BindVBO(),
                       patchTable->GetPatchArrays(),
                       patchTable->GetPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  /// \brief Generic limit eval function with derivatives. This function has
  ///        a same signature as other device kernels have so that it can be
  ///        called in the same way.
  ///
  /// @param srcBuffer        Input primvar buffer.
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of source data
  ///
  /// @param srcDesc          vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer        Output primvar buffer
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of destination data
  ///
  /// @param dstDesc          vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer         Output buffer derivative wrt u
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of destination data
  ///
  /// @param duDesc           vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer         Output buffer derivative wrt v
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of destination data
  ///
  /// @param dvDesc           vertex buffer descriptor for the dvBuffer
  ///
  /// @param duuBuffer        Output buffer 2nd derivative wrt u
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of destination data
  ///
  /// @param duuDesc          vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer        Output buffer 2nd derivative wrt u and v
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of destination data
  ///
  /// @param duvDesc          vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer        Output buffer 2nd derivative wrt v
  ///                         must have BindVBO() method returning a GL
  ///                         buffer object of destination data
  ///
  /// @param dvvDesc          vertex buffer descriptor for the dvvBuffer
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
                   DST_BUFFER *duuBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                   DST_BUFFER *duvBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                   DST_BUFFER *dvvBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                   int numPatchCoords,
                   PATCHCOORD_BUFFER *patchCoords,
                   PATCH_TABLE *patchTable) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       duBuffer->BindVBO(),
                       duDesc,
                       dvBuffer->BindVBO(),
                       dvDesc,
                       duuBuffer->BindVBO(),
                       duuDesc,
                       duvBuffer->BindVBO(),
                       duvDesc,
                       dvvBuffer->BindVBO(),
                       dvvDesc,
                       numPatchCoords,
                       patchCoords->BindVBO(),
                       patchTable->GetPatchArrays(),
                       patchTable->GetPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  bool EvalPatches(GLuint srcBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                   GLuint dstBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                   GLuint duBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                   GLuint dvBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                   int numPatchCoords,
                   GLuint patchCoordsBuffer,
                   const OpenSubdiv::Osd::PatchArrayVector &patchArrays,
                   GLuint patchIndexBuffer,
                   GLuint patchParamsBuffer) const;

  bool EvalPatches(GLuint srcBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                   GLuint dstBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                   GLuint duBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                   GLuint dvBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                   GLuint duuBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                   GLuint duvBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                   GLuint dvvBuffer,
                   OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                   int numPatchCoords,
                   GLuint patchCoordsBuffer,
                   const OpenSubdiv::Osd::PatchArrayVector &patchArrays,
                   GLuint patchIndexBuffer,
                   GLuint patchParamsBuffer) const;

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
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
                                 GLComputeEvaluator const *instance,
                                 void *deviceContext = NULL)
  {

    if (instance) {
      return instance->EvalPatchesVarying(
          srcBuffer, srcDesc, dstBuffer, dstDesc, numPatchCoords, patchCoords, patchTable);
    }
    else {
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
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
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
                          PATCH_TABLE *patchTable) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       0,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       0,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       numPatchCoords,
                       patchCoords->BindVBO(),
                       patchTable->GetVaryingPatchArrays(),
                       patchTable->GetVaryingPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
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
                                 GLComputeEvaluator const *instance,
                                 void *deviceContext = NULL)
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
    else {
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
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
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
                          PATCH_TABLE *patchTable) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       duBuffer->BindVBO(),
                       duDesc,
                       dvBuffer->BindVBO(),
                       dvDesc,
                       numPatchCoords,
                       patchCoords->BindVBO(),
                       patchTable->GetVaryingPatchArrays(),
                       patchTable->GetVaryingPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param duuBuffer      Output buffer 2nd derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duuDesc        vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer      Output buffer 2nd derivative wrt u and v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duvDesc        vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer      Output buffer 2nd derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvvDesc        vertex buffer descriptor for the dvvBuffer
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
                                 DST_BUFFER *duuBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                                 DST_BUFFER *duvBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                                 DST_BUFFER *dvvBuffer,
                                 OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                                 int numPatchCoords,
                                 PATCHCOORD_BUFFER *patchCoords,
                                 PATCH_TABLE *patchTable,
                                 GLComputeEvaluator const *instance,
                                 void *deviceContext = NULL)
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
                                          duuBuffer,
                                          duuDesc,
                                          duvBuffer,
                                          duvDesc,
                                          dvvBuffer,
                                          dvvDesc,
                                          numPatchCoords,
                                          patchCoords,
                                          patchTable);
    }
    else {
      // Create an instance on demand (slow)
      (void)deviceContext;  // unused
      instance = Create(srcDesc, dstDesc, duDesc, dvDesc, duuDesc, duvDesc, dvvDesc);
      if (instance) {
        bool r = instance->EvalPatchesVarying(srcBuffer,
                                              srcDesc,
                                              dstBuffer,
                                              dstDesc,
                                              duBuffer,
                                              duDesc,
                                              dvBuffer,
                                              dvDesc,
                                              duuBuffer,
                                              duuDesc,
                                              duvBuffer,
                                              duvDesc,
                                              dvvBuffer,
                                              dvvDesc,
                                              numPatchCoords,
                                              patchCoords,
                                              patchTable);
        delete instance;
        return r;
      }
      return false;
    }
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param duuBuffer      Output buffer 2nd derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duuDesc        vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer      Output buffer 2nd derivative wrt u and v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duvDesc        vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer      Output buffer 2nd derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvvDesc        vertex buffer descriptor for the dvvBuffer
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
                          DST_BUFFER *duuBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                          DST_BUFFER *duvBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                          DST_BUFFER *dvvBuffer,
                          OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                          int numPatchCoords,
                          PATCHCOORD_BUFFER *patchCoords,
                          PATCH_TABLE *patchTable) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       duBuffer->BindVBO(),
                       duDesc,
                       dvBuffer->BindVBO(),
                       dvDesc,
                       duuBuffer->BindVBO(),
                       duuDesc,
                       duvBuffer->BindVBO(),
                       duvDesc,
                       dvvBuffer->BindVBO(),
                       dvvDesc,
                       numPatchCoords,
                       patchCoords->BindVBO(),
                       patchTable->GetVaryingPatchArrays(),
                       patchTable->GetVaryingPatchIndexBuffer(),
                       patchTable->GetPatchParamBuffer());
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
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
                                     GLComputeEvaluator const *instance,
                                     void *deviceContext = NULL)
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
    else {
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
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
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
                              int fvarChannel = 0) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       0,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       0,
                       OpenSubdiv::Osd::BufferDescriptor(),
                       numPatchCoords,
                       patchCoords->BindVBO(),
                       patchTable->GetFVarPatchArrays(fvarChannel),
                       patchTable->GetFVarPatchIndexBuffer(fvarChannel),
                       patchTable->GetFVarPatchParamBuffer(fvarChannel));
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
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
                                     GLComputeEvaluator const *instance,
                                     void *deviceContext = NULL)
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
    else {
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
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
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
                              int fvarChannel = 0) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       duBuffer->BindVBO(),
                       duDesc,
                       dvBuffer->BindVBO(),
                       dvDesc,
                       numPatchCoords,
                       patchCoords->BindVBO(),
                       patchTable->GetFVarPatchArrays(fvarChannel),
                       patchTable->GetFVarPatchIndexBuffer(fvarChannel),
                       patchTable->GetFVarPatchParamBuffer(fvarChannel));
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param duuBuffer      Output buffer 2nd derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duuDesc        vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer      Output buffer 2nd derivative wrt u and v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duvDesc        vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer      Output buffer 2nd derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvvDesc        vertex buffer descriptor for the dvvBuffer
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
                                     DST_BUFFER *duuBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                                     DST_BUFFER *duvBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                                     DST_BUFFER *dvvBuffer,
                                     OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                                     int numPatchCoords,
                                     PATCHCOORD_BUFFER *patchCoords,
                                     PATCH_TABLE *patchTable,
                                     int fvarChannel,
                                     GLComputeEvaluator const *instance,
                                     void *deviceContext = NULL)
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
                                              duuBuffer,
                                              duuDesc,
                                              duvBuffer,
                                              duvDesc,
                                              dvvBuffer,
                                              dvvDesc,
                                              numPatchCoords,
                                              patchCoords,
                                              patchTable,
                                              fvarChannel);
    }
    else {
      // Create an instance on demand (slow)
      (void)deviceContext;  // unused
      instance = Create(srcDesc, dstDesc, duDesc, dvDesc, duuDesc, duvDesc, dvvDesc);
      if (instance) {
        bool r = instance->EvalPatchesFaceVarying(srcBuffer,
                                                  srcDesc,
                                                  dstBuffer,
                                                  dstDesc,
                                                  duBuffer,
                                                  duDesc,
                                                  dvBuffer,
                                                  dvDesc,
                                                  duuBuffer,
                                                  duuDesc,
                                                  duvBuffer,
                                                  duvDesc,
                                                  dvvBuffer,
                                                  dvvDesc,
                                                  numPatchCoords,
                                                  patchCoords,
                                                  patchTable,
                                                  fvarChannel);
        delete instance;
        return r;
      }
      return false;
    }
  }

  /// \brief Generic limit eval function. This function has a same
  ///        signature as other device kernels have so that it can be called
  ///        in the same way.
  ///
  /// @param srcBuffer      Input primvar buffer.
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of source data
  ///
  /// @param srcDesc        vertex buffer descriptor for the input buffer
  ///
  /// @param dstBuffer      Output primvar buffer
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dstDesc        vertex buffer descriptor for the output buffer
  ///
  /// @param duBuffer       Output buffer derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duDesc         vertex buffer descriptor for the duBuffer
  ///
  /// @param dvBuffer       Output buffer derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvDesc         vertex buffer descriptor for the dvBuffer
  ///
  /// @param duuBuffer      Output buffer 2nd derivative wrt u
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duuDesc        vertex buffer descriptor for the duuBuffer
  ///
  /// @param duvBuffer      Output buffer 2nd derivative wrt u and v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param duvDesc        vertex buffer descriptor for the duvBuffer
  ///
  /// @param dvvBuffer      Output buffer 2nd derivative wrt v
  ///                       must have BindVBO() method returning a GL
  ///                       buffer object of destination data
  ///
  /// @param dvvDesc        vertex buffer descriptor for the dvvBuffer
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
                              DST_BUFFER *duuBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                              DST_BUFFER *duvBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                              DST_BUFFER *dvvBuffer,
                              OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                              int numPatchCoords,
                              PATCHCOORD_BUFFER *patchCoords,
                              PATCH_TABLE *patchTable,
                              int fvarChannel = 0) const
  {

    return EvalPatches(srcBuffer->BindVBO(),
                       srcDesc,
                       dstBuffer->BindVBO(),
                       dstDesc,
                       duBuffer->BindVBO(),
                       duDesc,
                       dvBuffer->BindVBO(),
                       dvDesc,
                       duuBuffer->BindVBO(),
                       duuDesc,
                       duvBuffer->BindVBO(),
                       duvDesc,
                       dvvBuffer->BindVBO(),
                       dvvDesc,
                       numPatchCoords,
                       patchCoords->BindVBO(),
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
      OpenSubdiv::Osd::BufferDescriptor const &dvDesc = OpenSubdiv::Osd::BufferDescriptor(),
      OpenSubdiv::Osd::BufferDescriptor const &duuDesc = OpenSubdiv::Osd::BufferDescriptor(),
      OpenSubdiv::Osd::BufferDescriptor const &duvDesc = OpenSubdiv::Osd::BufferDescriptor(),
      OpenSubdiv::Osd::BufferDescriptor const &dvvDesc = OpenSubdiv::Osd::BufferDescriptor());

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
                 OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                 int workGroupSize);
    GLuint program;
    GLuint uniformStart;
    GLuint uniformEnd;
    GLuint uniformSrcOffset;
    GLuint uniformDstOffset;
    GLuint uniformDuDesc;
    GLuint uniformDvDesc;
    GLuint uniformDuuDesc;
    GLuint uniformDuvDesc;
    GLuint uniformDvvDesc;
  } _stencilKernel;

  struct _PatchKernel {
    _PatchKernel();
    ~_PatchKernel();
    bool Compile(OpenSubdiv::Osd::BufferDescriptor const &srcDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &dstDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &duDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &dvDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &duuDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &duvDesc,
                 OpenSubdiv::Osd::BufferDescriptor const &dvvDesc,
                 int workGroupSize);
    GLuint program;
    GLuint uniformSrcOffset;
    GLuint uniformDstOffset;
    GLuint uniformPatchArray;
    GLuint uniformDuDesc;
    GLuint uniformDvDesc;
    GLuint uniformDuuDesc;
    GLuint uniformDuvDesc;
    GLuint uniformDvvDesc;
  } _patchKernel;

  int _workGroupSize;
  GLuint _patchArraysSSBO;

  int GetDispatchSize(int count) const;

  void DispatchCompute(int totalDispatchSize) const;
};

}  // namespace blender::opensubdiv

#endif  // OPENSUBDIV_GL_COMPUTE_EVALUATOR_H_
