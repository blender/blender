/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "BLI_threads.h"

#include "BLT_translation.h"

#include "BKE_node.h"
#include "BKE_scene.h"

#include "COM_ExecutionSystem.h"
#include "COM_WorkScheduler.h"
#include "COM_compositor.h"

static struct {
  bool is_initialized = false;
  ThreadMutex mutex;
} g_compositor;

/* Make sure node tree has previews.
 * Don't create previews in advance, this is done when adding preview operations.
 * Reserved preview size is determined by render output for now. */
static void compositor_init_node_previews(const RenderData *render_data, bNodeTree *node_tree)
{
  /* We fit the aspect into COM_PREVIEW_SIZE x COM_PREVIEW_SIZE image to avoid
   * insane preview resolution, which might even overflow preview dimensions. */
  const float aspect = render_data->xsch > 0 ?
                           (float)render_data->ysch / (float)render_data->xsch :
                           1.0f;
  int preview_width, preview_height;
  if (aspect < 1.0f) {
    preview_width = blender::compositor::COM_PREVIEW_SIZE;
    preview_height = (int)(blender::compositor::COM_PREVIEW_SIZE * aspect);
  }
  else {
    preview_width = (int)(blender::compositor::COM_PREVIEW_SIZE / aspect);
    preview_height = blender::compositor::COM_PREVIEW_SIZE;
  }
  BKE_node_preview_init_tree(node_tree, preview_width, preview_height);
}

static void compositor_reset_node_tree_status(bNodeTree *node_tree)
{
  node_tree->progress(node_tree->prh, 0.0);
  node_tree->stats_draw(node_tree->sdh, IFACE_("Compositing"));
}

void COM_execute(RenderData *render_data,
                 Scene *scene,
                 bNodeTree *node_tree,
                 int rendering,
                 const ColorManagedViewSettings *view_settings,
                 const ColorManagedDisplaySettings *display_settings,
                 const char *view_name)
{
  /* Initialize mutex, TODO: this mutex init is actually not thread safe and
   * should be done somewhere as part of blender startup, all the other
   * initializations can be done lazily. */
  if (!g_compositor.is_initialized) {
    BLI_mutex_init(&g_compositor.mutex);
    g_compositor.is_initialized = true;
  }

  BLI_mutex_lock(&g_compositor.mutex);

  if (node_tree->test_break(node_tree->tbh)) {
    /* During editing multiple compositor executions can be triggered.
     * Make sure this is the most recent one. */
    BLI_mutex_unlock(&g_compositor.mutex);
    return;
  }

  compositor_init_node_previews(render_data, node_tree);
  compositor_reset_node_tree_status(node_tree);

  /* Initialize workscheduler. */
  const bool use_opencl = (node_tree->flag & NTREE_COM_OPENCL) != 0;
  blender::compositor::WorkScheduler::initialize(use_opencl, BKE_render_num_threads(render_data));

  /* Execute. */
  const bool twopass = (node_tree->flag & NTREE_TWO_PASS) && !rendering;
  if (twopass) {
    blender::compositor::ExecutionSystem fast_pass(render_data,
                                                   scene,
                                                   node_tree,
                                                   rendering,
                                                   true,
                                                   view_settings,
                                                   display_settings,
                                                   view_name);
    fast_pass.execute();

    if (node_tree->test_break(node_tree->tbh)) {
      BLI_mutex_unlock(&g_compositor.mutex);
      return;
    }
  }

  blender::compositor::ExecutionSystem system(
      render_data, scene, node_tree, rendering, false, view_settings, display_settings, view_name);
  system.execute();

  BLI_mutex_unlock(&g_compositor.mutex);
}

void COM_deinitialize()
{
  if (g_compositor.is_initialized) {
    BLI_mutex_lock(&g_compositor.mutex);
    blender::compositor::WorkScheduler::deinitialize();
    g_compositor.is_initialized = false;
    BLI_mutex_unlock(&g_compositor.mutex);
    BLI_mutex_end(&g_compositor.mutex);
  }
}
