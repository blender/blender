/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_global.h"
#include "BKE_image.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** SPLIT VIEWER ******************** */

namespace blender::nodes::node_composite_split_viewer_cc {

static void cmp_node_split_viewer_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image"));
  b.add_input<decl::Color>(N_("Image"), "Image_001");
}

static void node_composit_init_splitviewer(bNodeTree * /*ntree*/, bNode *node)
{
  ImageUser *iuser = MEM_cnew<ImageUser>(__func__);
  node->storage = iuser;
  iuser->sfra = 1;
  node->custom1 = 50; /* default 50% split */

  node->id = (ID *)BKE_image_ensure_viewer(G.main, IMA_TYPE_COMPOSITE, "Viewer Node");
}

static void node_composit_buts_splitviewer(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *row, *col;

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, false);
  uiItemR(row, ptr, "axis", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(col, ptr, "factor", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ViewerOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    GPUShader *shader = get_split_viewer_shader();
    GPU_shader_bind(shader);

    /* The compositing space might be limited to a subset of the output texture, so only write into
     * that compositing region. */
    const rcti compositing_region = context().get_compositing_region();
    const int2 lower_bound = int2(compositing_region.xmin, compositing_region.ymin);
    GPU_shader_uniform_2iv(shader, "compositing_region_lower_bound", lower_bound);

    GPU_shader_uniform_1f(shader, "split_ratio", get_split_ratio());

    const int2 compositing_region_size = context().get_compositing_region_size();
    GPU_shader_uniform_2iv(shader, "view_size", compositing_region_size);

    const Result &first_image = get_input("Image");
    first_image.bind_as_texture(shader, "first_image_tx");
    const Result &second_image = get_input("Image_001");
    second_image.bind_as_texture(shader, "second_image_tx");

    GPUTexture *output_texture = context().get_output_texture();
    const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
    GPU_texture_image_bind(output_texture, image_unit);

    compute_dispatch_threads_at_least(shader, compositing_region_size);

    first_image.unbind_as_texture();
    second_image.unbind_as_texture();
    GPU_texture_image_unbind(output_texture);
    GPU_shader_unbind();
  }

  /* The operation domain has the same size as the compositing region without any transformations
   * applied. */
  Domain compute_domain() override
  {
    return Domain(context().get_compositing_region_size());
  }

  GPUShader *get_split_viewer_shader()
  {
    if (get_split_axis() == CMP_NODE_SPLIT_VIEWER_HORIZONTAL) {
      return shader_manager().get("compositor_split_viewer_horizontal");
    }

    return shader_manager().get("compositor_split_viewer_vertical");
  }

  CMPNodeSplitViewerAxis get_split_axis()
  {
    return (CMPNodeSplitViewerAxis)bnode().custom2;
  }

  float get_split_ratio()
  {
    return bnode().custom1 / 100.0f;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ViewerOperation(context, node);
}

}  // namespace blender::nodes::node_composite_split_viewer_cc

void register_node_type_cmp_splitviewer()
{
  namespace file_ns = blender::nodes::node_composite_split_viewer_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SPLITVIEWER, "Split Viewer", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::cmp_node_split_viewer_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_splitviewer;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_splitviewer;
  node_type_storage(&ntype, "ImageUser", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}
