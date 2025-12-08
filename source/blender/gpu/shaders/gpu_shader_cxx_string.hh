/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * C++ stubs for shading language.
 *
 * IMPORTANT: Please ask the module team if you need some feature that are not listed in this file.
 */

#pragma once

/* For uint declaration. */
#include "gpu_shader_cxx_vector.hh"  // IWYU pragma: export

/**
 * Placeholder type for the actual shading language type.
 * This string type is much like the OSL string.
 * It is merely a hash of the actual string and it immutable.
 * Named `string_t` to avoid name collision with `std::string`.
 */
struct string_t {
  string_t(const char * /*str*/) {}
};

bool equal(string_t, string_t)
{
  return false;
}

uint as_uint(string_t)
{
  return 1;
}
