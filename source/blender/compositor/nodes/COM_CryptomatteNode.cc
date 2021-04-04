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
 * Copyright 2018, Blender Foundation.
 */

#include "COM_CryptomatteNode.h"
#include "BKE_node.h"
#include "BLI_assert.h"
#include "BLI_hash_mm3.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "COM_ConvertOperation.h"
#include "COM_CryptomatteOperation.h"
#include "COM_MultilayerImageOperation.h"
#include "COM_RenderLayersProg.h"
#include "COM_SetAlphaMultiplyOperation.h"
#include "COM_SetColorOperation.h"
#include <iterator>
#include <string>

namespace blender::compositor {

/** \name Cryptomatte base
 * \{ */

void CryptomatteBaseNode::convertToOperations(NodeConverter &converter,
                                              const CompositorContext &context) const
{
  NodeOutput *output_image_socket = this->getOutputSocket(0);

  bNode *node = this->getbNode();
  NodeCryptomatte *cryptomatte_settings = static_cast<NodeCryptomatte *>(node->storage);

  CryptomatteOperation *cryptomatte_operation = create_cryptomatte_operation(
      converter, context, *node, cryptomatte_settings);
  converter.addOperation(cryptomatte_operation);

  NodeOutput *output_matte_socket = this->getOutputSocket(1);
  SeparateChannelOperation *extract_mask_operation = new SeparateChannelOperation;
  extract_mask_operation->setChannel(3);
  converter.addOperation(extract_mask_operation);
  converter.addLink(cryptomatte_operation->getOutputSocket(0),
                    extract_mask_operation->getInputSocket(0));
  converter.mapOutputSocket(output_matte_socket, extract_mask_operation->getOutputSocket(0));

  NodeInput *input_image_socket = this->getInputSocket(0);
  SetAlphaMultiplyOperation *apply_mask_operation = new SetAlphaMultiplyOperation();
  converter.mapInputSocket(input_image_socket, apply_mask_operation->getInputSocket(0));
  converter.addOperation(apply_mask_operation);
  converter.addLink(extract_mask_operation->getOutputSocket(0),
                    apply_mask_operation->getInputSocket(1));
  converter.mapOutputSocket(output_image_socket, apply_mask_operation->getOutputSocket(0));

  NodeOutput *output_pick_socket = this->getOutputSocket(2);
  SetAlphaMultiplyOperation *extract_pick_operation = new SetAlphaMultiplyOperation();
  converter.addOperation(extract_pick_operation);
  converter.addInputValue(extract_pick_operation->getInputSocket(1), 1.0f);
  converter.addLink(cryptomatte_operation->getOutputSocket(0),
                    extract_pick_operation->getInputSocket(0));
  converter.mapOutputSocket(output_pick_socket, extract_pick_operation->getOutputSocket(0));
}

/* \} */

/** \name Cryptomatte V2
 * \{ */
static std::string prefix_from_node(const bNode &node)
{
  char prefix[MAX_NAME];
  ntreeCompositCryptomatteLayerPrefix(&node, prefix, sizeof(prefix));
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
    return;
  }

  const short cryptomatte_layer_id = 0;
  const std::string prefix = prefix_from_node(node);
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
    RenderLayer *render_layer = RE_GetRenderLayer(render_result, view_layer->name);
    if (render_layer) {
      LISTBASE_FOREACH (RenderPass *, render_pass, &render_layer->passes) {
        const std::string combined_name = combined_layer_pass_name(render_layer, render_pass);
        if (blender::StringRef(combined_name).startswith(prefix)) {
          RenderLayersProg *op = new RenderLayersProg(
              render_pass->name, DataType::Color, render_pass->channels);
          op->setScene(scene);
          op->setLayerId(cryptomatte_layer_id);
          op->setRenderData(context.getRenderData());
          op->setViewName(context.getViewName());
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
  BKE_image_user_frame_calc(image, iuser, context.getFramenumber());
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, iuser, nullptr);

  if (image->rr) {
    int view = 0;
    if (BLI_listbase_count_at_most(&image->rr->views, 2) > 1) {
      if (iuser->view == 0) {
        /* Heuristic to match image name with scene names, check if the view name exists in the
         * image. */
        view = BLI_findstringindex(
            &image->rr->views, context.getViewName(), offsetof(RenderView, name));
        if (view == -1) {
          view = 0;
        }
      }
      else {
        view = iuser->view - 1;
      }
    }

    const std::string prefix = prefix_from_node(node);
    int layer_index;
    LISTBASE_FOREACH_INDEX (RenderLayer *, render_layer, &image->rr->layers, layer_index) {
      if (!blender::StringRef(prefix).startswith(blender::StringRef(
              render_layer->name, BLI_strnlen(render_layer->name, sizeof(render_layer->name))))) {
        continue;
      }
      LISTBASE_FOREACH (RenderPass *, render_pass, &render_layer->passes) {
        const std::string combined_name = combined_layer_pass_name(render_layer, render_pass);
        if (blender::StringRef(combined_name).startswith(prefix)) {
          MultilayerColorOperation *op = new MultilayerColorOperation(
              render_layer, render_pass, view);
          op->setImage(image);
          op->setImageUser(iuser);
          iuser->layer = layer_index;
          op->setFramenumber(context.getFramenumber());
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
    case CMP_CRYPTOMATTE_SRC_RENDER:
      input_operations_from_render_source(context, node, input_operations);
      break;
    case CMP_CRYPTOMATTE_SRC_IMAGE:
      input_operations_from_image_source(context, node, input_operations);
      break;
  }

  if (input_operations.is_empty()) {
    SetColorOperation *op = new SetColorOperation();
    op->setChannel1(0.0f);
    op->setChannel2(1.0f);
    op->setChannel3(0.0f);
    op->setChannel4(0.0f);
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
    operation->addObjectIndex(cryptomatte_entry->encoded_hash);
  }
  for (int i = 0; i < input_operations.size(); ++i) {
    converter.addOperation(input_operations[i]);
    converter.addLink(input_operations[i]->getOutputSocket(), operation->getInputSocket(i));
  }
  return operation;
}

/* \} */

/** \name Cryptomatte legacy
 * \{ */

CryptomatteOperation *CryptomatteLegacyNode::create_cryptomatte_operation(
    NodeConverter &converter,
    const CompositorContext &UNUSED(context),
    const bNode &UNUSED(node),
    const NodeCryptomatte *cryptomatte_settings) const
{
  const int num_inputs = inputs.size() - 1;
  CryptomatteOperation *operation = new CryptomatteOperation(num_inputs);
  if (cryptomatte_settings) {
    LISTBASE_FOREACH (CryptomatteEntry *, cryptomatte_entry, &cryptomatte_settings->entries) {
      operation->addObjectIndex(cryptomatte_entry->encoded_hash);
    }
  }

  for (int i = 0; i < num_inputs; i++) {
    converter.mapInputSocket(this->getInputSocket(i + 1), operation->getInputSocket(i));
  }

  return operation;
}

/* \} */

}  // namespace blender::compositor
