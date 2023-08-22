/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_sunbeams_cc {

NODE_STORAGE_FUNCS(NodeSunBeams)

static void cmp_node_sunbeams_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeSunBeams *data = MEM_cnew<NodeSunBeams>(__func__);

  data->source[0] = 0.5f;
  data->source[1] = 0.5f;
  node->storage = data;
}

static void node_composit_buts_sunbeams(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "source", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, "", ICON_NONE);
  uiItemR(layout,
          ptr,
          "ray_length",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
}

using namespace blender::realtime_compositor;

class SunBeamsOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    GPUShader *shader = shader_manager().get("compositor_sun_beams");
    GPU_shader_bind(shader);

    GPU_shader_uniform_2fv(shader, "source", node_storage(bnode()).source);
    GPU_shader_uniform_1f(shader, "max_ray_length", node_storage(bnode()).ray_length);

    const Result &input_image = get_input("Image");
    GPU_texture_filter_mode(input_image.texture(), true);
    GPU_texture_extend_mode(input_image.texture(), GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SunBeamsOperation(context, node);
}

}  // namespace blender::nodes::node_composite_sunbeams_cc

void register_node_type_cmp_sunbeams()
{
  namespace file_ns = blender::nodes::node_composite_sunbeams_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SUNBEAMS, "Sun Beams", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_sunbeams_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_sunbeams;
  ntype.initfunc = file_ns::init;
  node_type_storage(
      &ntype, "NodeSunBeams", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
