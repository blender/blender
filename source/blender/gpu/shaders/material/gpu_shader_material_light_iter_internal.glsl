/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER

#else

/**
 * Dummy functions for gpu_shader_dependency.
 */
[[node]]
void LIGHT_ITER_BEGIN(float &light_index) {};
[[node]]
void LIGHT_ITER_END(Closure shader, Closure &r_shader) {};

#endif
