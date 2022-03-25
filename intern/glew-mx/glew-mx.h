/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup intern_glew-mx
 *
 * Support for GLEW Multiple rendering conteXts (MX)
 * Maintained as a Blender Library.
 *
 * Different rendering contexts may have different entry points
 * to extension functions of the same name.  So it can cause
 * problems if, for example, a second context uses a pointer to
 * say, glActiveTextureARB, that was queried from the first context.
 *
 * GLEW has basic support for multiple contexts by enabling WITH_GLEW_MX,
 * but it does not provide a full implementation.  This is because
 * there are too many questions about thread safety and memory
 * allocation that are up to the user of GLEW.
 *
 * This implementation is very basic and isn't thread safe.
 * For a single context the overhead should be
 * no more than using GLEW without WITH_GLEW_MX enabled.
 */

#ifndef __GLEW_MX_H__
#define __GLEW_MX_H__

#include <GL/glew.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "intern/symbol-binding.h"

/* If compiling only for OpenGL 3.2 Core Profile then we should make sure
 * no legacy API entries or symbolic constants are used.
 */
#if (!defined(WITH_LEGACY_OPENGL)) || defined(WITH_GL_PROFILE_CORE) && \
                                          !defined(WITH_GL_PROFILE_COMPAT) && \
                                          !defined(WITH_GL_PROFILE_ES20)
#  include "intern/gl-deprecated.h"
#endif

GLenum glew_chk(GLenum error, const char *file, int line, const char *text);

#ifndef NDEBUG
#  define GLEW_CHK(x) glew_chk((x), __FILE__, __LINE__, #  x)
#else
#  define GLEW_CHK(x) glew_chk((x), NULL, 0, NULL)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __GLEW_MX_H__ */
