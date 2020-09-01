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
 * Debug features of OpenGL.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "glew-mx.h"

#include "gl_context.hh"
#include "gl_uniform_buffer.hh"

#include "gl_debug.hh"

#include <stdio.h>

namespace blender::gpu::debug {

/* -------------------------------------------------------------------- */
/** \name Debug Callbacks
 *
 * Hooks up debug callbacks to a debug OpenGL context using extensions or 4.3 core debug
 * capabiliities.
 * \{ */

/* Debug callbacks need the same calling convention as OpenGL functions. */
#if defined(_WIN32)
#  define APIENTRY __stdcall
#else
#  define APIENTRY
#endif

#define VERBOSE 1

static void APIENTRY debug_callback(GLenum UNUSED(source),
                                    GLenum type,
                                    GLuint UNUSED(id),
                                    GLenum severity,
                                    GLsizei UNUSED(length),
                                    const GLchar *message,
                                    const GLvoid *UNUSED(userParm))
{
  const char format[] = "GPUDebug: %s%s\e[0m\n";

  if (ELEM(severity, GL_DEBUG_SEVERITY_LOW, GL_DEBUG_SEVERITY_NOTIFICATION)) {
    if (VERBOSE) {
      fprintf(stderr, format, "\e[2m", message);
    }
  }
  else {
    switch (type) {
      case GL_DEBUG_TYPE_ERROR:
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        fprintf(stderr, format, "\e[31;1mError\e[39m: ", message);
        break;
      case GL_DEBUG_TYPE_PORTABILITY:
      case GL_DEBUG_TYPE_PERFORMANCE:
      case GL_DEBUG_TYPE_OTHER:
      case GL_DEBUG_TYPE_MARKER: /* KHR has this, ARB does not */
      default:
        fprintf(stderr, format, "\e[33;1mWarning\e[39m: ", message);
        break;
    }

    if (VERBOSE && severity == GL_DEBUG_SEVERITY_HIGH) {
      /* Focus on error message. */
      fprintf(stderr, "\e[2m");
      BLI_system_backtrace(stderr);
      fprintf(stderr, "\e[0m\n");
      fflush(stderr);
    }
  }
}

#undef APIENTRY

void init_gl_callbacks(void)
{
#ifdef __APPLE__
  fprintf(stderr, "GPUDebug: OpenGL debug callback is not available on Apple\n");
  return;
#endif /* not Apple */

  char msg[256] = "";
  const char format[] = "Successfully hooked OpenGL debug callback using %s";

  if (GLEW_VERSION_4_3 || GLEW_KHR_debug) {
    SNPRINTF(msg, format, GLEW_VERSION_4_3 ? "OpenGL 4.3" : "KHR_debug extension");
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback((GLDEBUGPROC)debug_callback, NULL);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                         GL_DEBUG_TYPE_MARKER,
                         0,
                         GL_DEBUG_SEVERITY_NOTIFICATION,
                         -1,
                         msg);
  }
  else if (GLEW_ARB_debug_output) {
    SNPRINTF(msg, format, "ARB_debug_output");
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallbackARB((GLDEBUGPROCARB)debug_callback, NULL);
    glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    glDebugMessageInsertARB(GL_DEBUG_SOURCE_APPLICATION_ARB,
                            GL_DEBUG_TYPE_OTHER_ARB,
                            0,
                            GL_DEBUG_SEVERITY_LOW_ARB,
                            -1,
                            msg);
  }
  else {
    fprintf(stderr, "GPUDebug: Failed to hook OpenGL debug callback\n");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Error Checking
 *
 * This is only useful for implementation that does not support the KHR_debug extension OR when the
 * implementations do not report any errors even when clearly doing shady things.
 * \{ */

void check_gl_error(const char *info)
{
  GLenum error = glGetError();

#define ERROR_CASE(err) \
  case err: { \
    char msg[256]; \
    SNPRINTF(msg, "%s : %s", #err, info); \
    debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, msg, NULL); \
    break; \
  }

  switch (error) {
    ERROR_CASE(GL_INVALID_ENUM)
    ERROR_CASE(GL_INVALID_VALUE)
    ERROR_CASE(GL_INVALID_OPERATION)
    ERROR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION)
    ERROR_CASE(GL_OUT_OF_MEMORY)
    ERROR_CASE(GL_STACK_UNDERFLOW)
    ERROR_CASE(GL_STACK_OVERFLOW)
    case GL_NO_ERROR:
      break;
    default:
      char msg[256];
      SNPRINTF(msg, "Unknown GL error: %x : %s", error, info);
      debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, msg, NULL);
      break;
  }
}

void check_gl_resources(const char *info)
{
  GLContext *ctx = static_cast<GLContext *>(GPU_context_active_get());
  ShaderInterface *interface = ctx->shader->interface;
  /* NOTE: This only check binding. To be valid, the bound ubo needs to
   * be big enough to feed the data range the shader awaits. */
  uint16_t ubo_needed = interface->enabled_ubo_mask_;
  ubo_needed &= ~ctx->bound_ubo_slots;

  if (ubo_needed == 0) {
    return;
  }

  for (int i = 0; ubo_needed != 0; i++, ubo_needed >>= 1) {
    if ((ubo_needed & 1) != 0) {
      const ShaderInput *ubo_input = interface->ubo_get(i);
      const char *ubo_name = interface->input_name_get(ubo_input);
      const char *sh_name = ctx->shader->name_get();
      char msg[256];
      SNPRINTF(msg, "Missing UBO bind at slot %d : %s > %s : %s", i, sh_name, ubo_name, info);
      debug_callback(0, GL_DEBUG_TYPE_ERROR, 0, GL_DEBUG_SEVERITY_HIGH, 0, msg, NULL);
    }
  }
}

/** \} */

}  // namespace blender::gpu::debug