/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

namespace blender::ocio {

/**
 * Comment out all uniform statements. This avoids double declarations from the backend.
 * This function modifies source in-place without adding extra characters. This means that
 * statement like `uniform vec3 pos;` becomes `//iform vec3 pos;`.
 */
void source_comment_out_uniforms(std::string &source);

}  // namespace blender::ocio
