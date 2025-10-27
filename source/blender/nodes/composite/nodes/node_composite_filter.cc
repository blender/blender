/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_types.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_filter_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_FILTER_SOFT, "SOFTEN", 0, N_("Soften"), ""},
    {CMP_NODE_FILTER_SHARP_BOX,
     "SHARPEN",
     0,
     N_("Box Sharpen"),
     N_("An aggressive sharpening filter")},
    {CMP_NODE_FILTER_SHARP_DIAMOND,
     "SHARPEN_DIAMOND",
     0,
     N_("Diamond Sharpen"),
     N_("A moderate sharpening filter")},
    {CMP_NODE_FILTER_LAPLACE, "LAPLACE", 0, N_("Laplace"), ""},
    {CMP_NODE_FILTER_SOBEL, "SOBEL", 0, N_("Sobel"), ""},
    {CMP_NODE_FILTER_PREWITT, "PREWITT", 0, N_("Prewitt"), ""},
    {CMP_NODE_FILTER_KIRSCH, "KIRSCH", 0, N_("Kirsch"), ""},
    {CMP_NODE_FILTER_SHADOW, "SHADOW", 0, N_("Shadow"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_filter_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_FILTER_SOFT)
      .static_items(type_items)
      .optional_label();
}

class SocketSearchOp {
 public:
  CMPNodeFilterMethod filter_type = CMP_NODE_FILTER_SOFT;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("CompositorNodeFilter");
    bNodeSocket &type_socket = *blender::bke::node_find_socket(node, SOCK_IN, "Type");
    type_socket.default_value_typed<bNodeSocketValueMenu>()->value = this->filter_type;
    params.update_and_connect_available_socket(node, "Image");
  }
};

static void gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype from_socket_type = eNodeSocketDatatype(params.other_socket().type);
  if (!params.node_tree().typeinfo->validate_link(from_socket_type, SOCK_RGBA)) {
    return;
  }

  params.add_item(IFACE_("Soften"), SocketSearchOp{CMP_NODE_FILTER_SOFT});
  params.add_item(IFACE_("Box Sharpen"), SocketSearchOp{CMP_NODE_FILTER_SHARP_BOX});
  params.add_item(IFACE_("Laplace"), SocketSearchOp{CMP_NODE_FILTER_LAPLACE});
  params.add_item(IFACE_("Sobel"), SocketSearchOp{CMP_NODE_FILTER_SOBEL});
  params.add_item(IFACE_("Prewitt"), SocketSearchOp{CMP_NODE_FILTER_PREWITT});
  params.add_item(IFACE_("Kirsch"), SocketSearchOp{CMP_NODE_FILTER_KIRSCH});
  params.add_item(IFACE_("Shadow"), SocketSearchOp{CMP_NODE_FILTER_SHADOW});
  params.add_item(IFACE_("Diamond Sharpen"), SocketSearchOp{CMP_NODE_FILTER_SHARP_DIAMOND});
}

using namespace blender::compositor;

class FilterOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Image");
    if (input_image.is_single_value()) {
      Result &output_image = this->get_result("Image");
      output_image.share_data(input_image);
      return;
    }

    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
    else {
      this->execute_cpu();
    }
  }

  void execute_gpu()
  {
    gpu::Shader *shader = context().get_shader(get_shader_name());
    GPU_shader_bind(shader);

    GPU_shader_uniform_mat3_as_mat4(shader, "ukernel", get_filter_kernel().ptr());

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Result &factor = get_input("Fac");
    factor.bind_as_texture(shader, "factor_tx");

    const Domain domain = compute_domain();

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    factor.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  const char *get_shader_name()
  {
    if (this->is_edge_filter()) {
      return "compositor_edge_filter";
    }
    return "compositor_filter";
  }

  void execute_cpu()
  {
    const float3x3 kernel = this->get_filter_kernel();

    const Result &input = get_input("Image");
    const Result &factor = get_input("Fac");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    if (this->is_edge_filter()) {
      parallel_for(domain.size, [&](const int2 texel) {
        /* Compute the dot product between the 3x3 window around the pixel and the edge detection
         * kernel in the X direction and Y direction. The Y direction kernel is computed by
         * transposing the given X direction kernel. */
        float3 color_x = float3(0.0f);
        float3 color_y = float3(0.0f);
        for (int j = 0; j < 3; j++) {
          for (int i = 0; i < 3; i++) {
            float3 color =
                float4(input.load_pixel_extended<Color>(texel + int2(i - 1, j - 1))).xyz();
            color_x += color * kernel[j][i];
            color_y += color * kernel[i][j];
          }
        }

        /* Compute the channel-wise magnitude of the 2D vector composed from the X and Y edge
         * detection filter results. */
        float3 magnitude = math::sqrt(color_x * color_x + color_y * color_y);

        /* Mix the channel-wise magnitude with the original color at the center of the kernel using
         * the input factor. */
        float4 color = float4(input.load_pixel<Color>(texel));
        magnitude = math::interpolate(
            color.xyz(), magnitude, factor.load_pixel<float, true>(texel));

        /* Store the channel-wise magnitude with the original alpha of the input. */
        output.store_pixel(texel, Color(float4(magnitude, color.w)));
      });
    }
    else {
      parallel_for(domain.size, [&](const int2 texel) {
        /* Compute the dot product between the 3x3 window around the pixel and the kernel. */
        float4 color = float4(0.0f);
        for (int j = 0; j < 3; j++) {
          for (int i = 0; i < 3; i++) {
            color += float4(input.load_pixel_extended<Color>(texel + int2(i - 1, j - 1))) *
                     kernel[j][i];
          }
        }

        /* Mix with the original color at the center of the kernel using the input factor. */
        color = math::interpolate(
            float4(input.load_pixel<Color>(texel)), color, factor.load_pixel<float, true>(texel));

        /* Store the color making sure it is not negative. */
        output.store_pixel(texel, Color(math::max(color, float4(0.0f))));
      });
    }
  }

  bool is_edge_filter()
  {
    switch (this->get_type()) {
      case CMP_NODE_FILTER_LAPLACE:
      case CMP_NODE_FILTER_SOBEL:
      case CMP_NODE_FILTER_PREWITT:
      case CMP_NODE_FILTER_KIRSCH:
        return true;
      case CMP_NODE_FILTER_SOFT:
      case CMP_NODE_FILTER_SHARP_BOX:
      case CMP_NODE_FILTER_SHADOW:
      case CMP_NODE_FILTER_SHARP_DIAMOND:
        return false;
    }
    return false;
  }

  float3x3 get_filter_kernel()
  {
    /* Initialize the kernels as arrays of rows with the top row first. Edge detection kernels
     * return the kernel in the X direction, while the kernel in the Y direction will be computed
     * inside the shader by transposing the kernel in the X direction. */
    switch (this->get_type()) {
      case CMP_NODE_FILTER_SOFT: {
        const float kernel[3][3] = {{1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f},
                                    {2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f},
                                    {1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_SHARP_BOX: {
        const float kernel[3][3] = {
            {-1.0f, -1.0f, -1.0f}, {-1.0f, 9.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_LAPLACE: {
        const float kernel[3][3] = {{-1.0f / 8.0f, -1.0f / 8.0f, -1.0f / 8.0f},
                                    {-1.0f / 8.0f, 1.0f, -1.0f / 8.0f},
                                    {-1.0f / 8.0f, -1.0f / 8.0f, -1.0f / 8.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_SOBEL: {
        const float kernel[3][3] = {{1.0f, 0.0f, -1.0f}, {2.0f, 0.0f, -2.0f}, {1.0f, 0.0f, -1.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_PREWITT: {
        const float kernel[3][3] = {{1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_KIRSCH: {
        const float kernel[3][3] = {
            {5.0f, -3.0f, -2.0f}, {5.0f, -3.0f, -2.0f}, {5.0f, -3.0f, -2.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_SHADOW: {
        const float kernel[3][3] = {{1.0f, 2.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, -2.0f, -1.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_SHARP_DIAMOND: {
        const float kernel[3][3] = {
            {0.0f, -1.0f, 0.0f}, {-1.0f, 5.0f, -1.0f}, {0.0f, -1.0f, 0.0f}};
        return float3x3(kernel);
      }
    }

    const float kernel[3][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    return float3x3(kernel);
  }

  CMPNodeFilterMethod get_type()
  {
    const Result &input = this->get_input("Type");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_FILTER_SOFT);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeFilterMethod>(menu_value.value);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new FilterOperation(context, node);
}

}  // namespace blender::nodes::node_composite_filter_cc

static void register_node_type_cmp_filter()
{
  namespace file_ns = blender::nodes::node_composite_filter_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeFilter", CMP_NODE_FILTER);
  ntype.ui_name = "Filter";
  ntype.ui_description = "Apply common image enhancement filters";
  ntype.enum_name_legacy = "FILTER";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_filter_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.gather_link_search_ops = file_ns::gather_link_searches;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_filter)
