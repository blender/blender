/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_eval.glsl"

/* The compute shader that will be dispatched by the compositor ShaderOperation. It just calls the
 * evaluate function that will be dynamically generated and appended to this shader in the
 * ShaderOperation::generate_code method. */
void main()
{
  evaluate();
}
