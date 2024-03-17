/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct bNodeTree;
struct Main;
struct Material;

namespace blender::io::obj {

struct MTLMaterial;

bNodeTree *create_mtl_node_tree(Main *bmain,
                                const MTLMaterial &mtl_mat,
                                Material *mat,
                                bool relative_paths);

}  // namespace blender::io::obj
