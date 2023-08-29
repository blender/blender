/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_RenderLayersNode.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"

namespace blender::compositor {

RenderLayersNode::RenderLayersNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void RenderLayersNode::test_socket_link(NodeConverter &converter,
                                        const CompositorContext &context,
                                        NodeOutput *output,
                                        RenderLayersProg *operation,
                                        Scene *scene,
                                        int layer_id,
                                        bool is_preview) const
{
  operation->set_scene(scene);
  operation->set_layer_id(layer_id);
  operation->set_render_data(context.get_render_data());
  operation->set_view_name(context.get_view_name());

  converter.map_output_socket(output, operation->get_output_socket());
  converter.add_operation(operation);

  if (is_preview) { /* only for image socket */
    converter.add_preview(operation->get_output_socket());
  }
}

void RenderLayersNode::test_render_link(NodeConverter &converter,
                                        const CompositorContext &context,
                                        Render *re) const
{
  Scene *scene = (Scene *)this->get_bnode()->id;
  const short layer_id = this->get_bnode()->custom1;
  RenderResult *rr = RE_AcquireResultRead(re);
  if (rr == nullptr) {
    missing_render_link(converter);
    return;
  }
  ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, layer_id);
  if (view_layer == nullptr) {
    missing_render_link(converter);
    return;
  }
  RenderLayer *rl = RE_GetRenderLayer(rr, view_layer->name);
  if (rl == nullptr) {
    missing_render_link(converter);
    return;
  }

  for (NodeOutput *output : get_output_sockets()) {
    NodeImageLayer *storage = (NodeImageLayer *)output->get_bnode_socket()->storage;
    RenderPass *rpass = (RenderPass *)BLI_findstring(
        &rl->passes, storage->pass_name, offsetof(RenderPass, name));
    if (rpass == nullptr) {
      missing_socket_link(converter, output);
      continue;
    }
    RenderLayersProg *operation;
    bool is_preview;
    if (STREQ(rpass->name, RE_PASSNAME_COMBINED) &&
        STREQ(output->get_bnode_socket()->name, "Alpha")) {
      operation = new RenderLayersAlphaProg(rpass->name, DataType::Value, rpass->channels);
      is_preview = false;
    }
    else if (STREQ(rpass->name, RE_PASSNAME_Z)) {
      operation = new RenderLayersDepthProg(rpass->name, DataType::Value, rpass->channels);
      is_preview = false;
    }
    else {
      DataType type;
      switch (rpass->channels) {
        case 4:
          type = DataType::Color;
          break;
        case 3:
          type = DataType::Vector;
          break;
        case 1:
          type = DataType::Value;
          break;
        default:
          BLI_assert_msg(0, "Unexpected number of channels for pass");
          type = DataType::Value;
          break;
      }
      operation = new RenderLayersProg(rpass->name, type, rpass->channels);
      is_preview = STREQ(output->get_bnode_socket()->name, "Image");
    }
    test_socket_link(converter, context, output, operation, scene, layer_id, is_preview);
  }
}

void RenderLayersNode::missing_socket_link(NodeConverter &converter, NodeOutput *output) const
{
  NodeOperation *operation;
  switch (output->get_data_type()) {
    case DataType::Color: {
      const float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      SetColorOperation *color_operation = new SetColorOperation();
      color_operation->set_channels(color);
      operation = color_operation;
      break;
    }
    case DataType::Vector: {
      const float vector[3] = {0.0f, 0.0f, 0.0f};
      SetVectorOperation *vector_operation = new SetVectorOperation();
      vector_operation->set_vector(vector);
      operation = vector_operation;
      break;
    }
    case DataType::Value: {
      SetValueOperation *value_operation = new SetValueOperation();
      value_operation->set_value(0.0f);
      operation = value_operation;
      break;
    }
    default: {
      BLI_assert_msg(0, "Unexpected data type");
      return;
    }
  }

  converter.map_output_socket(output, operation->get_output_socket());
  converter.add_operation(operation);
}

void RenderLayersNode::missing_render_link(NodeConverter &converter) const
{
  for (NodeOutput *output : outputs_) {
    missing_socket_link(converter, output);
  }
}

void RenderLayersNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext &context) const
{
  Scene *scene = (Scene *)this->get_bnode()->id;
  Render *re = (scene) ? RE_GetSceneRender(scene) : nullptr;

  if (re != nullptr) {
    test_render_link(converter, context, re);
    RE_ReleaseResult(re);
  }
  else {
    missing_render_link(converter);
  }
}

}  // namespace blender::compositor
