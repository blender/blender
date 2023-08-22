/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ExecutionModel.h"
#include "COM_CompositorContext.h"

namespace blender::compositor {

ExecutionModel::ExecutionModel(CompositorContext &context, Span<NodeOperation *> operations)
    : context_(context), operations_(operations)
{
  const bNodeTree *node_tree = context_.get_bnodetree();

  const rctf *viewer_border = &node_tree->viewer_border;
  border_.use_viewer_border = (node_tree->flag & NTREE_VIEWER_BORDER) &&
                              viewer_border->xmin < viewer_border->xmax &&
                              viewer_border->ymin < viewer_border->ymax;
  border_.viewer_border = viewer_border;

  const RenderData *rd = context_.get_render_data();
  /* Case when cropping to render border happens is handled in
   * compositor output and render layer nodes. */
  border_.use_render_border = context.is_rendering() && (rd->mode & R_BORDER) &&
                              !(rd->mode & R_CROP);
  border_.render_border = &rd->border;
}

}  // namespace blender::compositor
