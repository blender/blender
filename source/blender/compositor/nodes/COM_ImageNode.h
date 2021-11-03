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
                                     RenderLayer *render_layer,
                                     RenderPass *render_pass,
                                     Image *image,
                                     ImageUser *user,
                                     int framenumber,
                                     int outputsocket_index,
                                     int view,
                                     DataType datatype) const;

 public:
  ImageNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
