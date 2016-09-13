
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

#define TRUST_NO_ONE 1

#include <GL/glew.h>
#include <stdbool.h>

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

#define PRIM_NONE 0xF
