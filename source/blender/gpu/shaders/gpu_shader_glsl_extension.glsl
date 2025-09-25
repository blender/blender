/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Always start a shader with #version directive and the required #extension.
 * The extensions are different depending on the backend and implementation support.
 * For this reason this file is generated at runtime.
 * The present implementation is just for reference and documentation.
 */

#pragma once

#pragma runtime_generated

#if defined(GPU_OPENGL)
/* We only require OpenGL 4.3. */
#  version 430
/* Required: For draw call batching. */
#  extension GL_ARB_shader_draw_parameters : enable
/* Optional: Avoid geometry shader for layered rendering. */
#  extension GL_ARB_shader_viewport_layer_array : enable
/* Optional: Avoid geometry shader for barycentric coordinates. */
#  extension GL_AMD_shader_explicit_vertex_parameter : enable
/* Optional: For subpass input emulation. */
#  extension GL_EXT_shader_framebuffer_fetch : enable
/* Optional: For faster EEVEE GBuffer classification. */
#  extension GL_ARB_shader_stencil_export : enable

#elif defined(GPU_VULKAN)
#  version 450
/* Required: For draw call batching. */
#  extension GL_ARB_shader_draw_parameters : enable
/* Optional: Avoid geometry shader for layered rendering. */
#  extension GL_ARB_shader_viewport_layer_array : enable
/* Optional: Avoid geometry shader for barycentric coordinates. */
#  extension GL_EXT_fragment_shader_barycentric : require
/* Optional: For faster EEVEE GBuffer classification. */
#  extension GL_ARB_shader_stencil_export : enable
#endif
