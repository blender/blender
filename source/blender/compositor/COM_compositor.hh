/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "DNA_node_types.h"

namespace blender {

namespace compositor {
class RenderContext;
class Profiler;
enum class NodeGroupOutputTypes : uint8_t;
}  // namespace compositor

struct Render;

void COM_execute(Render *render,
                 RenderData *render_data,
                 Scene *scene,
                 bNodeTree *node_tree,
                 const char *view_name,
                 compositor::RenderContext *render_context,
                 compositor::Profiler *profiler,
                 compositor::NodeGroupOutputTypes needed_outputs);

}  // namespace blender
