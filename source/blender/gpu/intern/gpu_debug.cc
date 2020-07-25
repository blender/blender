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
 */

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_debug.h"
#include "GPU_glew.h"
#include "intern/gpu_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __APPLE__ /* only non-Apple systems implement OpenGL debug callbacks */

/* control whether we use older AMD_debug_output extension
 * some supported GPU + OS combos do not have the newer extensions */
#  define LEGACY_DEBUG 1

/* Debug callbacks need the same calling convention as OpenGL functions. */
#  if defined(_WIN32)
#    define APIENTRY __stdcall
#  else
#    define APIENTRY
#  endif

static const char *source_name(GLenum source)
{
  switch (source) {
    case GL_DEBUG_SOURCE_API:
      return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      return "window system";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      return "shader compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      return "3rd party";
    case GL_DEBUG_SOURCE_APPLICATION:
      return "application";
    case GL_DEBUG_SOURCE_OTHER:
      return "other";
    default:
      return "???";
  }
}

static const char *message_type_name(GLenum message)
{
  switch (message) {
    case GL_DEBUG_TYPE_ERROR:
      return "error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      return "deprecated behavior";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      return "undefined behavior";
    case GL_DEBUG_TYPE_PORTABILITY:
      return "portability";
    case GL_DEBUG_TYPE_PERFORMANCE:
      return "performance";
    case GL_DEBUG_TYPE_OTHER:
      return "other";
    case GL_DEBUG_TYPE_MARKER:
      return "marker"; /* KHR has this, ARB does not */
    default:
      return "???";
  }
}

static void APIENTRY gpu_debug_proc(GLenum source,
                                    GLenum type,
                                    GLuint UNUSED(id),
                                    GLenum severity,
                                    GLsizei UNUSED(length),
                                    const GLchar *message,
                                    const GLvoid *UNUSED(userParm))
{
  bool backtrace = false;

  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      backtrace = true;
      ATTR_FALLTHROUGH;
    case GL_DEBUG_SEVERITY_MEDIUM:
    case GL_DEBUG_SEVERITY_LOW:
    case GL_DEBUG_SEVERITY_NOTIFICATION: /* KHR has this, ARB does not */
      fprintf(stderr, "GL %s %s: %s\n", source_name(source), message_type_name(type), message);
  }

  if (backtrace) {
    BLI_system_backtrace(stderr);
    fflush(stderr);
  }
}

#  if LEGACY_DEBUG

static const char *category_name_amd(GLenum category)
{
  switch (category) {
    case GL_DEBUG_CATEGORY_API_ERROR_AMD:
      return "API error";
    case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD:
      return "window system";
    case GL_DEBUG_CATEGORY_DEPRECATION_AMD:
      return "deprecated behavior";
    case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD:
      return "undefined behavior";
    case GL_DEBUG_CATEGORY_PERFORMANCE_AMD:
      return "performance";
    case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD:
      return "shader compiler";
    case GL_DEBUG_CATEGORY_APPLICATION_AMD:
      return "application";
    case GL_DEBUG_CATEGORY_OTHER_AMD:
      return "other";
    default:
      return "???";
  }
}

static void APIENTRY gpu_debug_proc_amd(GLuint UNUSED(id),
                                        GLenum category,
                                        GLenum severity,
                                        GLsizei UNUSED(length),
                                        const GLchar *message,
                                        GLvoid *UNUSED(userParm))
{
  bool backtrace = false;

  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      backtrace = true;
      ATTR_FALLTHROUGH;
    case GL_DEBUG_SEVERITY_MEDIUM:
    case GL_DEBUG_SEVERITY_LOW:
      fprintf(stderr, "GL %s: %s\n", category_name_amd(category), message);
  }

  if (backtrace) {
    BLI_system_backtrace(stderr);
    fflush(stderr);
  }
}
#  endif /* LEGACY_DEBUG */

#  undef APIENTRY
#endif /* not Apple */

void gpu_debug_init(void)
{
#ifdef __APPLE__
  fprintf(stderr, "OpenGL debug callback is not available on Apple.\n");
#else /* not Apple */
  const char success[] = "Successfully hooked OpenGL debug callback.";

  if (GLEW_VERSION_4_3 || GLEW_KHR_debug) {
    fprintf(stderr,
            "Using %s\n",
            GLEW_VERSION_4_3 ? "OpenGL 4.3 debug facilities" : "KHR_debug extension");
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback((GLDEBUGPROC)gpu_debug_proc, NULL);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    GPU_string_marker(success);
  }
  else if (GLEW_ARB_debug_output) {
    fprintf(stderr, "Using ARB_debug_output extension\n");
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallbackARB((GLDEBUGPROCARB)gpu_debug_proc, NULL);
    glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    GPU_string_marker(success);
  }
#  if LEGACY_DEBUG
  else if (GLEW_AMD_debug_output) {
    fprintf(stderr, "Using AMD_debug_output extension\n");
    glDebugMessageCallbackAMD(gpu_debug_proc_amd, NULL);
    glDebugMessageEnableAMD(GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    GPU_string_marker(success);
  }
#  endif
  else {
    fprintf(stderr, "Failed to hook OpenGL debug callback.\n");
  }
#endif /* not Apple */
}

void gpu_debug_exit(void)
{
#ifndef __APPLE__
  if (GLEW_VERSION_4_3 || GLEW_KHR_debug) {
    glDebugMessageCallback(NULL, NULL);
  }
  else if (GLEW_ARB_debug_output) {
    glDebugMessageCallbackARB(NULL, NULL);
  }
#  if LEGACY_DEBUG
  else if (GLEW_AMD_debug_output) {
    glDebugMessageCallbackAMD(NULL, NULL);
  }
#  endif
#endif
}

void GPU_string_marker(const char *buf)
{
#ifdef __APPLE__
  UNUSED_VARS(buf);
#else /* not Apple */
  if (GLEW_VERSION_4_3 || GLEW_KHR_debug) {
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                         GL_DEBUG_TYPE_MARKER,
                         0,
                         GL_DEBUG_SEVERITY_NOTIFICATION,
                         -1,
                         buf);
  }
  else if (GLEW_ARB_debug_output) {
    glDebugMessageInsertARB(GL_DEBUG_SOURCE_APPLICATION_ARB,
                            GL_DEBUG_TYPE_OTHER_ARB,
                            0,
                            GL_DEBUG_SEVERITY_LOW_ARB,
                            -1,
                            buf);
  }
#  if LEGACY_DEBUG
  else if (GLEW_AMD_debug_output) {
    glDebugMessageInsertAMD(
        GL_DEBUG_CATEGORY_APPLICATION_AMD, GL_DEBUG_SEVERITY_LOW_AMD, 0, 0, buf);
  }
#  endif
#endif /* not Apple */
}

void GPU_print_error_debug(const char *str)
{
  if (G.debug & G_DEBUG) {
    fprintf(stderr, "GPU: %s\n", str);
  }
}
