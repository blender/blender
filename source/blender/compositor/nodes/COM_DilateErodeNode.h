/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief DilateErodeNode
 * \ingroup Node
 */
class DilateErodeNode : public Node {
  /** only used for blurring alpha, since the dilate/erode node doesn't have this. */
  NodeBlurData alpha_blur_;

 public:
  DilateErodeNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
