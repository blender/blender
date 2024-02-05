/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Map UV  ******************** */

namespace blender::nodes::node_composite_map_uv_cc {

static void cmp_node_map_uv_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_realization_options(CompositorInputRealizationOptions::None);
  b.add_input<decl::Vector>("UV")
      .default_value({1.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_map_uv(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static void node_composit_init_map_uv(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom2 = CMP_NODE_MAP_UV_FILTERING_ANISOTROPIC;
}

using namespace blender::realtime_compositor;

class MapUVOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (get_input("Image").is_single_value()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }
    bool nearest_neighbour = get_nearest_neighbour();

    GPUShader *shader = context().get_shader(get_shader_name());

    GPU_shader_bind(shader);

    if (!nearest_neighbour) {
      GPU_shader_uniform_1f(
          shader, "gradient_attenuation_factor", get_gradient_attenuation_factor());
    }

    const Result &input_image = get_input("Image");
    if (nearest_neighbour) {
      GPU_texture_mipmap_mode(input_image.texture(), false, false);
      GPU_texture_anisotropic_filter(input_image.texture(), false);
    }
    else {
      GPU_texture_mipmap_mode(input_image.texture(), true, true);
      GPU_texture_anisotropic_filter(input_image.texture(), true);
    }

    GPU_texture_extend_mode(input_image.texture(), GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_uv = get_input("UV");
    input_uv.bind_as_texture(shader, "uv_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    input_uv.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  /* A factor that controls the attenuation of the result at the pixels where the gradients of the
   * UV texture are too high, see the shader for more information. The factor ranges between zero
   * and one, where it has no effect when it is zero and performs full attenuation when it is 1. */
  float get_gradient_attenuation_factor()
  {
    return bnode().custom1 / 100.0f;
  }

  bool get_nearest_neighbour()
  {
    return bnode().custom2 == CMP_NODE_MAP_UV_FILTERING_NEAREST;
  }

  char const *get_shader_name()
  {
    return get_nearest_neighbour() ? "compositor_map_uv_nearest_neighbour" :
                                     "compositor_map_uv_anisotropic";
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MapUVOperation(context, node);
}

}  // namespace blender::nodes::node_composite_map_uv_cc

void register_node_type_cmp_mapuv()
{
  namespace file_ns = blender::nodes::node_composite_map_uv_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MAP_UV, "Map UV", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_map_uv_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_map_uv;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.initfunc = file_ns::node_composit_init_map_uv;

  nodeRegisterType(&ntype);
}
