/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "BKE_node.hh"

#include "NOD_composite.hh"

#include "COM_ConvertOperation.h"
#include "COM_CryptomatteNode.h"
#include "COM_MultilayerImageOperation.h"
#include "COM_RenderLayersProg.h"
#include "COM_SetAlphaMultiplyOperation.h"
#include "COM_SetAlphaReplaceOperation.h"
#include "COM_SetColorOperation.h"

namespace blender::compositor {

/* -------------------------------------------------------------------- */
/** \name Cryptomatte Base
 * \{ */

void CryptomatteBaseNode::convert_to_operations(NodeConverter &converter,
                                                const CompositorContext &context) const
{
  NodeOutput *output_image_socket = this->get_output_socket(0);

  const bNode *node = this->get_bnode();
  const NodeCryptomatte *cryptomatte_settings = static_cast<const NodeCryptomatte *>(
      node->storage);

  CryptomatteOperation *cryptomatte_operation = create_cryptomatte_operation(
      converter, context, *node, cryptomatte_settings);
  converter.add_operation(cryptomatte_operation);

  NodeOutput *output_matte_socket = this->get_output_socket(1);
  SeparateChannelOperation *extract_mask_operation = new SeparateChannelOperation;
  extract_mask_operation->set_channel(3);
  converter.add_operation(extract_mask_operation);
  converter.add_link(cryptomatte_operation->get_output_socket(0),
                     extract_mask_operation->get_input_socket(0));
  converter.map_output_socket(output_matte_socket, extract_mask_operation->get_output_socket(0));

  NodeInput *input_image_socket = this->get_input_socket(0);
  SetAlphaMultiplyOperation *apply_mask_operation = new SetAlphaMultiplyOperation();
  converter.map_input_socket(input_image_socket, apply_mask_operation->get_input_socket(0));
  converter.add_operation(apply_mask_operation);
  converter.add_link(extract_mask_operation->get_output_socket(0),
                     apply_mask_operation->get_input_socket(1));
  converter.map_output_socket(output_image_socket, apply_mask_operation->get_output_socket(0));

  NodeOutput *output_pick_socket = this->get_output_socket(2);
  SetAlphaReplaceOperation *extract_pick_operation = new SetAlphaReplaceOperation();
  converter.add_operation(extract_pick_operation);
  converter.add_input_value(extract_pick_operation->get_input_socket(1), 1.0f);
  converter.add_link(cryptomatte_operation->get_output_socket(0),
                     extract_pick_operation->get_input_socket(0));
  converter.map_output_socket(output_pick_socket, extract_pick_operation->get_output_socket(0));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte V2
 * \{ */

static std::string prefix_from_node(const CompositorContext &context, const bNode &node)
{
  char prefix[MAX_NAME];
  ntreeCompositCryptomatteLayerPrefix(context.get_scene(), &node, prefix, sizeof(prefix));
  return std::string(prefix, BLI_strnlen(prefix, sizeof(prefix)));
}

static std::string combined_layer_pass_name(RenderLayer *render_layer, RenderPass *render_pass)
{
  if (render_layer->name[0] == '\0') {
    return std::string(render_pass->name,
                       BLI_strnlen(render_pass->name, sizeof(render_pass->name)));
  }

  std::string combined_name =
      blender::StringRef(render_layer->name,
                         BLI_strnlen(render_layer->name, sizeof(render_layer->name))) +
      "." +
      blender::StringRef(render_pass->name,
                         BLI_strnlen(render_pass->name, sizeof(render_pass->name)));
  return combined_name;
}

void CryptomatteNode::input_operations_from_render_source(
    const CompositorContext &context,
    const bNode &node,
    Vector<NodeOperation *> &r_input_operations)
{
  Scene *scene = (Scene *)node.id;
  if (!scene) {
    return;
  }

  BLI_assert(GS(scene->id.name) == ID_SCE);
  Render *render = RE_GetSceneRender(scene);
  RenderResult *render_result = render ? RE_AcquireResultRead(render) : nullptr;

  if (!render_result) {
    if (render) {
      RE_ReleaseResult(render);
    }
    return;
  }

  short view_layer_id = 0;
  const std::string prefix = prefix_from_node(context, node);
  LISTBASE_FOREACH_INDEX (ViewLayer *, view_layer, &scene->view_layers, view_layer_id) {
    RenderLayer *render_layer = RE_GetRenderLayer(render_result, view_layer->name);
    if (render_layer) {
      LISTBASE_FOREACH (RenderPass *, render_pass, &render_layer->passes) {
        if (context.has_explicit_view() && !STREQ(render_pass->view, context.get_view_name())) {
          continue;
        }

        const std::string combined_name = combined_layer_pass_name(render_layer, render_pass);
        if (combined_name != prefix && blender::StringRef(combined_name).startswith(prefix)) {
          RenderLayersProg *op = new RenderLayersProg(
              render_pass->name, DataType::Color, render_pass->channels);
          op->set_scene(scene);
          op->set_layer_id(view_layer_id);
          op->set_render_data(context.get_render_data());
          op->set_view_name(context.get_view_name());
          r_input_operations.append(op);
        }
      }
    }
  }
  RE_ReleaseResult(render);
}

void CryptomatteNode::input_operations_from_image_source(
    const CompositorContext &context,
    const bNode &node,
    Vector<NodeOperation *> &r_input_operations)
{
  NodeCryptomatte *cryptomatte_settings = (NodeCryptomatte *)node.storage;
  Image *image = (Image *)node.id;
  if (!image) {
    return;
  }

  BLI_assert(GS(image->id.name) == ID_IM);
  if (image->type != IMA_TYPE_MULTILAYER) {
    return;
  }

  ImageUser *iuser = &cryptomatte_settings->iuser;
  BKE_image_user_frame_calc(image, iuser, context.get_framenumber());
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, iuser, nullptr);

  if (image->rr) {
    const std::string prefix = prefix_from_node(context, node);
    int layer_index;
    LISTBASE_FOREACH_INDEX (RenderLayer *, render_layer, &image->rr->layers, layer_index) {
      if (!blender::StringRef(prefix).startswith(blender::StringRef(
              render_layer->name, BLI_strnlen(render_layer->name, sizeof(render_layer->name)))))
      {
        continue;
      }
      LISTBASE_FOREACH (RenderPass *, render_pass, &render_layer->passes) {
        const std::string combined_name = combined_layer_pass_name(render_layer, render_pass);
        if (combined_name != prefix && blender::StringRef(combined_name).startswith(prefix)) {
          MultilayerColorOperation *op = new MultilayerColorOperation();
          iuser->layer = layer_index;
          op->set_image(image);
          op->set_image_user(*iuser);
          op->set_framenumber(context.get_framenumber());
          op->set_render_data(context.get_render_data());
          op->set_view_name(context.get_view_name());
          op->set_layer_name(render_layer->name);
          op->set_pass_name(render_pass->name);
          r_input_operations.append(op);
        }
      }
      break;
    }
  }
  BKE_image_release_ibuf(image, ibuf, nullptr);
}

Vector<NodeOperation *> CryptomatteNode::create_input_operations(const CompositorContext &context,
                                                                 const bNode &node)
{
  Vector<NodeOperation *> input_operations;
  switch (node.custom1) {
    case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER:
      input_operations_from_render_source(context, node, input_operations);
      break;
    case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE:
      input_operations_from_image_source(context, node, input_operations);
      break;
  }

  if (input_operations.is_empty()) {
    SetColorOperation *op = new SetColorOperation();
    op->set_channel1(0.0f);
    op->set_channel2(1.0f);
    op->set_channel3(0.0f);
    op->set_channel4(0.0f);
    input_operations.append(op);
  }
  return input_operations;
}
CryptomatteOperation *CryptomatteNode::create_cryptomatte_operation(
    NodeConverter &converter,
    const CompositorContext &context,
    const bNode &node,
    const NodeCryptomatte *cryptomatte_settings) const
{
  Vector<NodeOperation *> input_operations = create_input_operations(context, node);
  CryptomatteOperation *operation = new CryptomatteOperation(input_operations.size());
  LISTBASE_FOREACH (CryptomatteEntry *, cryptomatte_entry, &cryptomatte_settings->entries) {
    operation->add_object_index(cryptomatte_entry->encoded_hash);
  }
  for (int i = 0; i < input_operations.size(); ++i) {
    converter.add_operation(input_operations[i]);
    converter.add_link(input_operations[i]->get_output_socket(), operation->get_input_socket(i));
  }
  return operation;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte Legacy
 * \{ */

CryptomatteOperation *CryptomatteLegacyNode::create_cryptomatte_operation(
    NodeConverter &converter,
    const CompositorContext & /*context*/,
    const bNode & /*node*/,
    const NodeCryptomatte *cryptomatte_settings) const
{
  const int num_inputs = inputs_.size() - 1;
  CryptomatteOperation *operation = new CryptomatteOperation(num_inputs);
  if (cryptomatte_settings) {
    LISTBASE_FOREACH (CryptomatteEntry *, cryptomatte_entry, &cryptomatte_settings->entries) {
      operation->add_object_index(cryptomatte_entry->encoded_hash);
    }
  }

  for (int i = 0; i < num_inputs; i++) {
    converter.map_input_socket(this->get_input_socket(i + 1), operation->get_input_socket(i));
  }

  return operation;
}

/** \} */

}  // namespace blender::compositor
