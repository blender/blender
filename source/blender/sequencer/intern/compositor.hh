/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#pragma once

#include "COM_context.hh"
#include "COM_node_group_operation.hh"
#include "SEQ_render.hh"

namespace blender::seq {

class CompositorContext : public compositor::Context {
 protected:
  const RenderData &render_data_;
  const Strip *strip_ = nullptr;
  float2 result_translation_ = float2(0, 0);
  /* Identifies if the output of the viewer was written. */
  bool viewer_was_written_ = false;

 public:
  CompositorContext(compositor::StaticCacheManager &cache_manager,
                    const RenderData &render_data,
                    const Strip &strip)
      : compositor::Context(cache_manager), render_data_(render_data), strip_(&strip)
  {
  }
  const Scene &get_scene() const override
  {
    return *render_data_.scene;
  }
  bool treat_viewer_as_group_output() const override
  {
    return true;
  }
  const Strip *get_strip() const override
  {
    return strip_;
  }

  bool use_gpu() const override
  {
    return this->render_data_.scene->r.compositor_device == SCE_COMPOSITOR_DEVICE_GPU;
  }

  compositor::ResultPrecision get_precision() const override;

  float2 get_result_translation() const
  {
    return result_translation_;
  }

 protected:
  compositor::NodeGroupOutputTypes needed_outputs() const
  {
    compositor::NodeGroupOutputTypes needed_outputs =
        compositor::NodeGroupOutputTypes::GroupOutputNode;
    if (!render_data_.render) {
      needed_outputs |= compositor::NodeGroupOutputTypes::ViewerNode;
    }
    return needed_outputs;
  }

  void create_result_from_input(compositor::Result &result, const ImBuf &input) const;
  void write_output(const compositor::Result &result, ImBuf &image);
  void write_outputs(const bNodeTree &node_group,
                     compositor::NodeGroupOperation &node_group_operation,
                     ImBuf &output_image);
  void set_output_refcount(const bNodeTree &node_group,
                           compositor::NodeGroupOperation &node_group_operation);
};

}  // namespace blender::seq
