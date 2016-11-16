
// Gawain common #defines and #includes
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

// #define TRUST_NO_ONE !defined(NDEBUG)
#define TRUST_NO_ONE 1
// strict error checking, always enabled during early development

#include <GL/glew.h>
#include <stdbool.h>
#include <stdint.h>

#if TRUST_NO_ONE
  #include <assert.h>
#endif

#define PER_THREAD
// #define PER_THREAD __thread
// MSVC uses __declspec(thread) for C code

#define APPLE_LEGACY (defined(__APPLE__) && defined(WITH_GL_PROFILE_COMPAT))

#if APPLE_LEGACY
  #undef glGenVertexArrays
  #define glGenVertexArrays glGenVertexArraysAPPLE

  #undef glDeleteVertexArrays
  #define glDeleteVertexArrays glDeleteVertexArraysAPPLE

  #undef glBindVertexArray
  #define glBindVertexArray glBindVertexArrayAPPLE
#endif

typedef enum {
	PRIM_POINTS = GL_POINTS,
	PRIM_LINES = GL_LINES,
	PRIM_TRIANGLES = GL_TRIANGLES,

#ifdef WITH_GL_PROFILE_COMPAT
	PRIM_QUADS = GL_QUADS, // legacy GL has this, modern GL & Vulkan do not
#endif

	PRIM_LINE_STRIP = GL_LINE_STRIP,
	PRIM_LINE_LOOP = GL_LINE_LOOP, // GL has this, Vulkan does not
	PRIM_TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
	PRIM_TRIANGLE_FAN = GL_TRIANGLE_FAN,

	PRIM_NONE = 0xF
} PrimitiveType;
