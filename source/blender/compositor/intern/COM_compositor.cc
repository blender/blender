/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_mutex.hh"

#include "BLT_translation.hh"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_compositor.hh"

#include "RE_compositor.hh"

static constexpr float COM_PREVIEW_SIZE = 140.0f;

static blender::Mutex g_compositor_mutex;

/* Make sure node tree has previews.
 * Don't create previews in advance, this is done when adding preview operations.
 * Reserved preview size is determined by render output for now. */
static void compositor_init_node_previews(const RenderData *render_data, bNodeTree *node_tree)
{
  /* We fit the aspect into COM_PREVIEW_SIZE x COM_PREVIEW_SIZE image to avoid
   * insane preview resolution, which might even overflow preview dimensions. */
  const float aspect = render_data->xsch > 0 ?
                           float(render_data->ysch) / float(render_data->xsch) :
                           1.0f;
  int preview_width, preview_height;
  if (aspect < 1.0f) {
    preview_width = COM_PREVIEW_SIZE;
    preview_height = int(COM_PREVIEW_SIZE * aspect);
  }
  else {
    preview_width = int(COM_PREVIEW_SIZE / aspect);
    preview_height = COM_PREVIEW_SIZE;
  }
  blender::bke::node_preview_init_tree(node_tree, preview_width, preview_height);
}

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
                 blender::compositor::RenderContext *render_context,
                 blender::compositor::Profiler *profiler,
                 blender::compositor::OutputTypes needed_outputs)
{
  std::scoped_lock lock(g_compositor_mutex);

  if (node_tree->runtime->test_break(node_tree->runtime->tbh)) {
    /* During editing multiple compositor executions can be triggered.
     * Make sure this is the most recent one. */
    return;
  }

  compositor_init_node_previews(render_data, node_tree);
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

void COM_deinitialize() {}
