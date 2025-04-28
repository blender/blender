/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/string.h"
#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

struct KernelLightTreeNode;
class LightTree;
struct LightTreeNode;
class Scene;

void light_tree_plot_to_file(const Scene &scene,
                             const LightTree &tree,
                             const LightTreeNode &root_node,
                             const string &filename);

void klight_tree_plot_to_file(uint root_index,
                              const KernelLightTreeNode *knodes,
                              const string &filename);

CCL_NAMESPACE_END
