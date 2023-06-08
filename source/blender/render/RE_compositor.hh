/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

struct bNodeTree;
struct Depsgraph;
struct Render;
struct RenderData;
struct Scene;

namespace blender {

namespace realtime_compositor {
class Evaluator;
}

namespace render {
class Context;
class TexturePool;

/* ------------------------------------------------------------------------------------------------
 * Render Realtime Compositor
 *
 * Implementation of the compositor for final rendering, as opposed to the viewport compositor
 * that is part of the draw manager. The input and output of this is pre-existing RenderResult
 * buffers in scenes, that are uploaded to and read back from the GPU. */

class RealtimeCompositor {
 private:
  /* Render instance for GPU context to run compositor in. */
  Render &render_;

  std::unique_ptr<TexturePool> texture_pool_;
  std::unique_ptr<Context> context_;
  std::unique_ptr<realtime_compositor::Evaluator> evaluator_;

 public:
  RealtimeCompositor(Render &render,
                     const Scene &scene,
                     const RenderData &render_data,
                     const bNodeTree &node_tree,
                     const bool use_file_output,
                     const char *view_name);

  ~RealtimeCompositor();

  /* Evaluate the compositor and output to the scene render result. */
  void execute();

  /* If the compositor node tree changed, reset the evaluator. */
  void update(const Depsgraph *depsgraph);
};

}  // namespace render
}  // namespace blender
