/* SPDX-FileCopyrightText: 2021 Blender Foundation
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
class ConvertColorSpaceNode : public Node {
 public:
  ConvertColorSpaceNode(bNode *editorNode);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;

 private:
  /** \brief check if the given settings changes color space. */
  bool performs_conversion(NodeConvertColorSpace &settings) const;
};

}  // namespace blender::compositor
