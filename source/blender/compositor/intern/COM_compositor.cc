/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_compositor.hh"

#include "RE_compositor.hh"

namespace blender {

void COM_execute(Render *render,
                 RenderData *render_data,
                 Scene *scene,
                 bNodeTree *node_tree,
                 const char *view_name,
                 compositor::RenderContext *render_context,
                 compositor::Profiler *profiler,
                 compositor::NodeGroupOutputTypes needed_outputs)
{
  RE_compositor_execute(*render,
                        *scene,
                        *render_data,
                        *node_tree,
                        view_name,
                        render_context,
                        profiler,
                        needed_outputs);
}

}  // namespace blender
