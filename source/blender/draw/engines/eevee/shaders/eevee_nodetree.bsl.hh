/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "eevee_nodetree_type.bsl.hh"

struct NodeTreeRes {
  [[uniform(0 /*GPU_NODE_TREE_UBO_SLOT*/), frequency(BATCH)]] NodeTree &node_tree;
};
