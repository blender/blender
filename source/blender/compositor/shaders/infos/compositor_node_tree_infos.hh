/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "gpu_shader_compositor_nodetree_type.glsl"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_nodetree)
UNIFORM_BUF(0 /*GPU_NODE_TREE_UBO_SLOT*/, NodeTree, node_tree)
GPU_SHADER_CREATE_END()
