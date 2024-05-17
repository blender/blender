/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"
#include "COM_defines.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

namespace blender::compositor {

/**
 * \brief ImageNode
 * \ingroup Node
 */
class ImageNode : public Node {
 private:
  NodeOperation *do_multilayer_check(NodeConverter &converter,
                                     const CompositorContext &context,
                                     const char *layer_name,
                                     const char *pass_name,
                                     Image *image,
                                     ImageUser *user,
                                     int framenumber,
                                     int outputsocket_index,
                                     DataType datatype) const;

 public:
  ImageNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
