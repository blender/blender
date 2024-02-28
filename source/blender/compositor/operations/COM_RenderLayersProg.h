/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "COM_MultiThreadedOperation.h"
#include "DNA_scene_types.h"
#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"

namespace blender::compositor {

/**
 * Base class for all renderlayeroperations
 *
 * \todo Rename to operation.
 */
class RenderLayersProg : public MultiThreadedOperation {
 protected:
  /**
   * Reference to the scene object.
   */
  Scene *scene_;

  /**
   * layer_id of the layer where this operation needs to get its data from
   */
  short layer_id_;

  /**
   * view_name of the view to use (unless another view is specified by the node
   */
  const char *view_name_;

  const MemoryBuffer *layer_buffer_;

  /**
   * Cached instance to the float buffer inside the layer.
   * TODO: To be removed with tiled implementation.
   */
  float *input_buffer_;

  /**
   * Render-pass where this operation needs to get its data from.
   */
  std::string pass_name_;

  int elementsize_;

  /**
   * \brief render data used for active rendering
   */
  const RenderData *rd_;

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  /**
   * retrieve the reference to the float buffer of the renderer.
   */
  inline float *get_input_buffer()
  {
    return input_buffer_;
  }

  void do_interpolation(float output[4], float x, float y, PixelSampler sampler);

 public:
  /**
   * Constructor
   */
  RenderLayersProg(const char *pass_name, DataType type, int elementsize);
  /**
   * setter for the scene field. Will be called from
   * \see RenderLayerNode to set the actual scene where
   * the data will be retrieved from.
   */
  void set_scene(Scene *scene)
  {
    scene_ = scene;
  }
  Scene *get_scene() const
  {
    return scene_;
  }
  void set_render_data(const RenderData *rd)
  {
    rd_ = rd;
  }
  void set_layer_id(short layer_id)
  {
    layer_id_ = layer_id;
  }
  short get_layer_id() const
  {
    return layer_id_;
  }
  void set_view_name(const char *view_name)
  {
    view_name_ = view_name;
  }
  const char *get_view_name()
  {
    return view_name_;
  }
  void init_execution() override;
  void deinit_execution() override;

  std::unique_ptr<MetaData> get_meta_data() override;

  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

class RenderLayersAOOperation : public RenderLayersProg {
 public:
  RenderLayersAOOperation(const char *pass_name, DataType type, int elementsize)
      : RenderLayersProg(pass_name, type, elementsize)
  {
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class RenderLayersAlphaProg : public RenderLayersProg {
 public:
  RenderLayersAlphaProg(const char *pass_name, DataType type, int elementsize)
      : RenderLayersProg(pass_name, type, elementsize)
  {
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class RenderLayersDepthProg : public RenderLayersProg {
 public:
  RenderLayersDepthProg(const char *pass_name, DataType type, int elementsize)
      : RenderLayersProg(pass_name, type, elementsize)
  {
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
