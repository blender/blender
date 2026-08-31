/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Implementation of basic shading language functions (types, builtin functions, etc...).
 *
 * Each backend will replace it with its own implementation at runtime.
 */

#pragma once
/* This file must replaced at runtime. The following content is only a possible implementation. */
#pragma runtime_generated

#include "gpu_shader_compat_cxx.hh"  // IWYU pragma: export
/* Other possible implementation. */
// #include "gpu_shader_compat_glsl.hh"
// #include "gpu_shader_compat_msl.hh"
