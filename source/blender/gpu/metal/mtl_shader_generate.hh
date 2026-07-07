/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_create_info.hh"
#include "gpu_shader_private.hh"

#include "mtl_capabilities.hh"

#include <sstream>

/**
 * In Metal UBO, SSBO and Push Constants all share the same binding space with a maximum of 31
 * bindings per stage. To avoid bind location clash, we associate different ranges to different
 * usage. Given that vertex and index buffers are not present in the shader code, we try to pack
 * them in the remaining unused slots. This is done inside the PSO description building (inside the
 * Batch API).
 *
 * +-----------------------------+--------+------------+
 * | Type                        | Count  | Slot Range |
 * +-----------------------------+--------+------------+
 * | Vertex Buffers              |     16 |    0..30   |
 * | Storage Buffers             |     16 |    0..15   |
 * | Uniform Buffers             |     13 |   16..28   |
 * | Push Constant Buffer        |      1 |   29..29   |
 * | Sampler Argument Buffer     |      1 |   30..30   |
 * +-----------------------------+--------+------------+
 */
#define MTL_MAX_SSBO 16
#define MTL_MAX_UBO 13
#define MTL_SSBO_SLOT_OFFSET 0
#define MTL_UBO_SLOT_OFFSET MTL_MAX_SSBO
#define MTL_PUSH_CONSTANT_BUFFER_SLOT (MTL_MAX_SSBO + MTL_MAX_UBO)
#define MTL_SAMPLER_ARGUMENT_BUFFER_SLOT (MTL_PUSH_CONSTANT_BUFFER_SLOT + 1)

/**
 * Whether they are used for arbitrary load/store or sampling, all textures share a binding space
 * per stage (up to 128 slots on our target devices). However, we keep the same combined
 * texture+sampler semantic as GLSL. The sampler binding space is much more limited (16 on target
 * hardware) which limits the maximum texture we can bind for sampling. We lift this limit by using
 * an Argument Buffer to store the samplers. So we reserve the first 16 slots to images and the
 * remaining ones for sampler textures.
 *
 * +-----------------------------+--------+------------+
 * | Type                        | Count  | Slot Range |
 * +-----------------------------+--------+------------+
 * | Image Textures              |      8 |    0..7    |
 * | Sampler Textures            |     64 |    8..71   |
 * +-----------------------------+--------+------------+
 */
#define MTL_IMAGE_SLOT_OFFSET 0
#define MTL_SAMPLER_SLOT_OFFSET MTL_MAX_IMAGE_SLOTS

/* Other parts of the backend also use specialization constants. */
#define MTL_SPECIALIZATION_CONSTANT_OFFSET 30

namespace blender::gpu {

struct PatchedShaderCreateInfo;

uint32_t available_buffer_slots(const shader::ShaderCreateInfo &info);

std::pair<std::string, std::string> generate_entry_point(const shader::ShaderCreateInfo &info,
                                                         const ShaderStage stage,
                                                         const StringRefNull entry_point_name);

void patch_create_info_atomic_workaround(std::unique_ptr<PatchedShaderCreateInfo> &patched_info,
                                         const shader::ShaderCreateInfo &original_info);

}  // namespace blender::gpu
