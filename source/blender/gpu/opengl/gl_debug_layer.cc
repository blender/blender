/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Implement our own subset of KHR_debug extension.
 * We override the functions pointers by our own implementation that just checks glGetError.
 */

#include "BLI_utildefines.h"

#include "glew-mx.h"

#include "gl_debug.hh"

typedef void *GPUvoidptr;

#define GPUvoidptr_set void *ret =
#define GPUvoidptr_ret return ret

#define GLboolean_set GLboolean ret =
#define GLboolean_ret return ret

#define void_set
#define void_ret

#define DEBUG_FUNC_DECLARE(pfn, rtn_type, fn, ...) \
  pfn real_##fn; \
  static rtn_type GLAPIENTRY debug_##fn(ARG_LIST(__VA_ARGS__)) \
  { \
    debug::check_gl_error("generated before " #fn); \
    rtn_type##_set real_##fn(ARG_LIST_CALL(__VA_ARGS__)); \
    debug::check_gl_error("" #fn); \
    rtn_type##_ret; \
  }

namespace blender::gpu::debug {

/* List of wrapped functions. We dont have to support all of them.
 * Some functions might be declared as extern in GLEW. We cannot override them in this case.
 * Keep the list in alphabetical order. */

/* Avoid very long declarations. */
/* clang-format off */
DEBUG_FUNC_DECLARE(PFNGLBEGINQUERYPROC, void, glBeginQuery, GLenum, target, GLuint, id);
DEBUG_FUNC_DECLARE(PFNGLBEGINTRANSFORMFEEDBACKPROC, void, glBeginTransformFeedback, GLenum, primitiveMode);
DEBUG_FUNC_DECLARE(PFNGLBINDBUFFERBASEPROC, void, glBindBufferBase, GLenum, target, GLuint, index, GLuint, buffer);
DEBUG_FUNC_DECLARE(PFNGLBINDBUFFERPROC, void, glBindBuffer, GLenum, target, GLuint, buffer);
DEBUG_FUNC_DECLARE(PFNGLBINDFRAMEBUFFERPROC, void, glBindFramebuffer, GLenum, target, GLuint, framebuffer);
DEBUG_FUNC_DECLARE(PFNGLBINDSAMPLERPROC, void, glBindSampler, GLuint, unit, GLuint, sampler);
DEBUG_FUNC_DECLARE(PFNGLBINDVERTEXARRAYPROC, void, glBindVertexArray, GLuint, array);
DEBUG_FUNC_DECLARE(PFNGLBLITFRAMEBUFFERPROC, void, glBlitFramebuffer, GLint, srcX0, GLint, srcY0, GLint, srcX1, GLint, srcY1, GLint, dstX0, GLint, dstY0, GLint, dstX1, GLint, dstY1, GLbitfield, mask, GLenum, filter);
DEBUG_FUNC_DECLARE(PFNGLBUFFERDATAPROC, void, glBufferData, GLenum, target, GLsizeiptr, size, const void *, data, GLenum, usage);
DEBUG_FUNC_DECLARE(PFNGLBUFFERSUBDATAPROC, void, glBufferSubData, GLenum, target, GLintptr, offset, GLsizeiptr, size, const void *, data);
DEBUG_FUNC_DECLARE(PFNGLDELETEBUFFERSPROC, void, glDeleteBuffers, GLsizei, n, const GLuint *, buffers);
DEBUG_FUNC_DECLARE(PFNGLDELETEFRAMEBUFFERSPROC, void, glDeleteFramebuffers, GLsizei, n, const GLuint*, framebuffers);
DEBUG_FUNC_DECLARE(PFNGLDELETEPROGRAMPROC, void, glDeleteProgram, GLuint, program);
DEBUG_FUNC_DECLARE(PFNGLDELETEQUERIESPROC, void, glDeleteQueries, GLsizei, n, const GLuint *, ids);
DEBUG_FUNC_DECLARE(PFNGLDELETESAMPLERSPROC, void, glDeleteSamplers, GLsizei, count, const GLuint *, samplers);
DEBUG_FUNC_DECLARE(PFNGLDELETESHADERPROC, void, glDeleteShader, GLuint, shader);
DEBUG_FUNC_DECLARE(PFNGLDELETEVERTEXARRAYSPROC, void, glDeleteVertexArrays, GLsizei, n, const GLuint *, arrays);
DEBUG_FUNC_DECLARE(PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC, void, glDrawArraysInstancedBaseInstance, GLenum, mode, GLint, first, GLsizei, count, GLsizei, primcount, GLuint, baseinstance);
DEBUG_FUNC_DECLARE(PFNGLDRAWARRAYSINSTANCEDPROC, void, glDrawArraysInstanced, GLenum, mode, GLint, first, GLsizei, count, GLsizei, primcount);
DEBUG_FUNC_DECLARE(PFNGLDRAWBUFFERSPROC, void, glDrawBuffers, GLsizei, n, const GLenum*, bufs);
DEBUG_FUNC_DECLARE(PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC, void, glDrawElementsInstancedBaseVertexBaseInstance, GLenum, mode, GLsizei, count, GLenum, type, const void *, indices, GLsizei, primcount, GLint, basevertex, GLuint, baseinstance);
DEBUG_FUNC_DECLARE(PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC, void, glDrawElementsInstancedBaseVertex, GLenum, mode, GLsizei, count, GLenum, type, const void *, indices, GLsizei, instancecount, GLint, basevertex);
DEBUG_FUNC_DECLARE(PFNGLENDQUERYPROC, void, glEndQuery, GLenum, target);
DEBUG_FUNC_DECLARE(PFNGLENDTRANSFORMFEEDBACKPROC, void, glEndTransformFeedback, void);
DEBUG_FUNC_DECLARE(PFNGLFRAMEBUFFERTEXTURE2DPROC, void, glFramebufferTexture2D, GLenum, target, GLenum, attachment, GLenum, textarget, GLuint, texture, GLint, level);
DEBUG_FUNC_DECLARE(PFNGLFRAMEBUFFERTEXTURELAYERPROC, void, glFramebufferTextureLayer, GLenum, target, GLenum, attachment, GLuint, texture, GLint, level, GLint, layer);
DEBUG_FUNC_DECLARE(PFNGLFRAMEBUFFERTEXTUREPROC, void, glFramebufferTexture, GLenum, target, GLenum, attachment, GLuint, texture, GLint, level);
DEBUG_FUNC_DECLARE(PFNGLGENBUFFERSPROC, void, glGenBuffers, GLsizei, n, GLuint *, buffers);
DEBUG_FUNC_DECLARE(PFNGLGENERATEMIPMAPPROC, void, glGenerateMipmap, GLenum, target);
DEBUG_FUNC_DECLARE(PFNGLGENERATETEXTUREMIPMAPPROC, void, glGenerateTextureMipmap, GLuint, texture);
DEBUG_FUNC_DECLARE(PFNGLGENFRAMEBUFFERSPROC, void, glGenFramebuffers, GLsizei, n, GLuint *, framebuffers);
DEBUG_FUNC_DECLARE(PFNGLGENQUERIESPROC, void, glGenQueries, GLsizei, n, GLuint *, ids);
DEBUG_FUNC_DECLARE(PFNGLGENSAMPLERSPROC, void, glGenSamplers, GLsizei, n, GLuint *, samplers);
DEBUG_FUNC_DECLARE(PFNGLGENVERTEXARRAYSPROC, void, glGenVertexArrays, GLsizei, n, GLuint *, arrays);
DEBUG_FUNC_DECLARE(PFNGLLINKPROGRAMPROC, void, glLinkProgram, GLuint, program);
DEBUG_FUNC_DECLARE(PFNGLMAPBUFFERRANGEPROC, GPUvoidptr, glMapBufferRange, GLenum, target, GLintptr, offset, GLsizeiptr, length, GLbitfield, access);
DEBUG_FUNC_DECLARE(PFNGLTEXBUFFERPROC, void, glTexBuffer, GLenum, target, GLenum, internalFormat, GLuint, buffer);
DEBUG_FUNC_DECLARE(PFNGLTEXIMAGE3DPROC, void, glTexImage3D, GLenum, target, GLint, level, GLint, internalFormat, GLsizei, width, GLsizei, height, GLsizei, depth, GLint, border, GLenum, format, GLenum, type, const GLvoid *,pixels);
DEBUG_FUNC_DECLARE(PFNGLTEXSUBIMAGE3DPROC, void, glTexSubImage3D, GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLint, zoffset, GLsizei, width, GLsizei, height, GLsizei, depth, GLenum, format, GLenum, type, const GLvoid *, pixels);
DEBUG_FUNC_DECLARE(PFNGLTEXTUREBUFFERPROC, void, glTextureBuffer, GLuint, texture, GLenum, internalformat, GLuint, buffer);
DEBUG_FUNC_DECLARE(PFNGLUNMAPBUFFERPROC, GLboolean, glUnmapBuffer, GLenum, target);
DEBUG_FUNC_DECLARE(PFNGLUSEPROGRAMPROC, void, glUseProgram, GLuint, program);
/* clang-format on */

#undef DEBUG_FUNC_DECLARE

/* Init a fallback layer (to KHR_debug) that covers only some functions.
 * We override the functions pointers by our own implementation that just checks glGetError.
 * Some additional functions (not overridable) are covered inside the header using wrappers. */
void init_debug_layer()
{
#define DEBUG_WRAP(function) \
  do { \
    real_##function = ::function; \
    ::function = &debug_##function; \
  } while (0)

  DEBUG_WRAP(glBeginQuery);
  DEBUG_WRAP(glBeginTransformFeedback);
  DEBUG_WRAP(glBindBuffer);
  DEBUG_WRAP(glBindBufferBase);
  DEBUG_WRAP(glBindFramebuffer);
  DEBUG_WRAP(glBindSampler);
  DEBUG_WRAP(glBindVertexArray);
  DEBUG_WRAP(glBlitFramebuffer);
  DEBUG_WRAP(glBufferData);
  DEBUG_WRAP(glBufferSubData);
  DEBUG_WRAP(glDeleteBuffers);
  DEBUG_WRAP(glDeleteFramebuffers);
  DEBUG_WRAP(glDeleteProgram);
  DEBUG_WRAP(glDeleteQueries);
  DEBUG_WRAP(glDeleteSamplers);
  DEBUG_WRAP(glDeleteShader);
  DEBUG_WRAP(glDeleteVertexArrays);
  DEBUG_WRAP(glDrawArraysInstanced);
  DEBUG_WRAP(glDrawArraysInstancedBaseInstance);
  DEBUG_WRAP(glDrawBuffers);
  DEBUG_WRAP(glDrawElementsInstancedBaseVertex);
  DEBUG_WRAP(glDrawElementsInstancedBaseVertexBaseInstance);
  DEBUG_WRAP(glEndQuery);
  DEBUG_WRAP(glEndTransformFeedback);
  DEBUG_WRAP(glFramebufferTexture);
  DEBUG_WRAP(glFramebufferTexture2D);
  DEBUG_WRAP(glFramebufferTextureLayer);
  DEBUG_WRAP(glGenBuffers);
  DEBUG_WRAP(glGenerateMipmap);
  DEBUG_WRAP(glGenerateTextureMipmap);
  DEBUG_WRAP(glGenFramebuffers);
  DEBUG_WRAP(glGenQueries);
  DEBUG_WRAP(glGenSamplers);
  DEBUG_WRAP(glGenVertexArrays);
  DEBUG_WRAP(glLinkProgram);
  DEBUG_WRAP(glMapBufferRange);
  DEBUG_WRAP(glTexBuffer);
  DEBUG_WRAP(glTexImage3D);
  DEBUG_WRAP(glTexSubImage3D);
  DEBUG_WRAP(glTextureBuffer);
  DEBUG_WRAP(glUnmapBuffer);
  DEBUG_WRAP(glUseProgram);

#undef DEBUG_WRAP
}

}  // namespace blender::gpu::debug
