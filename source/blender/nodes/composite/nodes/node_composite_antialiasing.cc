/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Anti-Aliasing (SMAA 1x) ******************** */

namespace blender::nodes::node_composite_antialiasing_cc {

NODE_STORAGE_FUNCS(NodeAntiAliasingData)

static void cmp_node_antialiasing_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_antialiasing(bNodeTree * /*ntree*/, bNode *node)
{
  NodeAntiAliasingData *data = MEM_cnew<NodeAntiAliasingData>(__func__);

  data->threshold = CMP_DEFAULT_SMAA_THRESHOLD;
  data->contrast_limit = CMP_DEFAULT_SMAA_CONTRAST_LIMIT;
  data->corner_rounding = CMP_DEFAULT_SMAA_CORNER_ROUNDING;

  node->storage = data;
}

static void node_composit_buts_antialiasing(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemR(col, ptr, "threshold", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "contrast_limit", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "corner_rounding", UI_ITEM_NONE, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class AntiAliasingOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    smaa(context(),
         get_input("Image"),
         get_result("Image"),
         get_threshold(),
         get_local_contrast_adaptation_factor(),
         get_corner_rounding());
  }

  /* Blender encodes the threshold in the [0, 1] range, while the SMAA algorithm expects it in
   * the [0, 0.5] range. */
  float get_threshold()
  {
    return node_storage(bnode()).threshold / 2.0f;
  }

  /* Blender encodes the local contrast adaptation factor in the [0, 1] range, while the SMAA
   * algorithm expects it in the [0, 10] range. */
  float get_local_contrast_adaptation_factor()
  {
    return node_storage(bnode()).contrast_limit * 10.0f;
  }

  /* Blender encodes the corner rounding factor in the float [0, 1] range, while the SMAA algorithm
   * expects it in the integer [0, 100] range. */
  int get_corner_rounding()
  {
    return int(node_storage(bnode()).corner_rounding * 100.0f);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new AntiAliasingOperation(context, node);
}

}  // namespace blender::nodes::node_composite_antialiasing_cc

void register_node_type_cmp_antialiasing()
{
  namespace file_ns = blender::nodes::node_composite_antialiasing_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ANTIALIASING, "Anti-Aliasing", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_antialiasing_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_antialiasing;
  ntype.flag |= NODE_PREVIEW;
  blender::bke::node_type_size(&ntype, 170, 140, 200);
  ntype.initfunc = file_ns::node_composit_init_antialiasing;
  node_type_storage(
      &ntype, "NodeAntiAliasingData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
