/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_mutex.hh"

#include "BLT_translation.hh"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_compositor.hh"

#include "RE_compositor.hh"

namespace blender {

static Mutex g_compositor_mutex;

static void compositor_reset_node_tree_status(bNodeTree *node_tree)
{
  node_tree->runtime->progress(node_tree->runtime->prh, 0.0);
  node_tree->runtime->stats_draw(node_tree->runtime->sdh, IFACE_("Compositing"));
}

void COM_execute(Render *render,
                 RenderData *render_data,
                 Scene *scene,
                 bNodeTree *node_tree,
                 const char *view_name,
                 compositor::RenderContext *render_context,
                 compositor::Profiler *profiler,
                 compositor::NodeGroupOutputTypes needed_outputs)
{
  std::scoped_lock lock(g_compositor_mutex);

  if (node_tree->runtime->test_break(node_tree->runtime->tbh)) {
    /* During editing multiple compositor executions can be triggered.
     * Make sure this is the most recent one. */
    return;
  }

  compositor_reset_node_tree_status(node_tree);

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
