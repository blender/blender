/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <string>

namespace blender {

namespace compositor {
class RenderContext;
enum class NodeGroupOutputTypes : uint8_t;
}  // namespace compositor

struct bNodeTree;
struct Render;
struct Main;
struct RenderData;
struct Scene;

/* ------------------------------------------------------------------------------------------------
 * Render Compositor
 *
 * Implementation of the compositor for final rendering, as opposed to the viewport compositor
 * that is part of the draw manager. The input and output of this is pre-existing RenderResult
 * buffers in scenes, that are uploaded to and read back from the GPU. */

namespace render {
class Compositor;

class CompositorInputData {
 public:
  Render &render;
  const Main &main;
  const Scene &scene;
  const RenderData &render_data;
  const bNodeTree &node_tree;
  std::string view_name;
  compositor::RenderContext *render_context;
  compositor::NodeGroupOutputTypes needed_outputs;
  /* Identifies if the compositor is executing due to the user making a modification or if it is
   * executing due to playback or rendering. */
  const bool triggered_by_user = false;
};

}  // namespace render

/* Execute compositor. */
void RE_compositor_execute(render::CompositorInputData input_data);

/* Free compositor caches. */
void RE_compositor_free(Render &render);

}  // namespace blender
