/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "COM_node_operation.hh"
#include "COM_realize_on_domain_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_relative_to_pixel_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Value", "Vector Value")
      .subtype(PROP_FACTOR)
      .dimensions(2)
      .default_value({0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .structure_type(StructureType::Dynamic)
      .description(
          "A value that is relative to the image size and needs to be converted to be in pixels");
  b.add_input<decl::Float>("Value", "Float Value")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .structure_type(StructureType::Dynamic)
      .description(
          "A value that is relative to the image size and needs to be converted to be in pixels");
  b.add_input<decl::Color>("Image")
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Float>("Value", "Float Value").structure_type(StructureType::Dynamic);
  b.add_output<decl::Vector>("Value", "Vector Value")
      .dimensions(2)
      .structure_type(StructureType::Dynamic);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT;
  node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const auto data_type = CMPNodeRelativeToPixelDataType(node->custom1);
  const auto reference_dimension = CMPNodeRelativeToPixelReferenceDimension(node->custom2);

  bNodeSocket *float_input = bke::node_find_socket(*node, SOCK_IN, "Float Value");
  bke::node_set_socket_availability(
      *ntree, *float_input, data_type == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT);

  bNodeSocket *vector_input = bke::node_find_socket(*node, SOCK_IN, "Vector Value");
  bke::node_set_socket_availability(
      *ntree, *vector_input, data_type == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR);

  /* The float output doesn't exist if the reference is per dimension, since each dimension can be
   * different. */
  const bool is_per_dimension = reference_dimension ==
                                CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION;
  bNodeSocket *float_output = bke::node_find_socket(*node, SOCK_OUT, "Float Value");
  bke::node_set_socket_availability(*ntree,
                                    *float_output,
                                    data_type == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT &&
                                        !is_per_dimension);

  /* The vector output exist if the reference is per dimension even if the data type is float,
   * since each dimension can be different. */
  bNodeSocket *vector_output = bke::node_find_socket(*node, SOCK_OUT, "Vector Value");
  bke::node_set_socket_availability(*ntree,
                                    *vector_output,
                                    data_type == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR ||
                                        is_per_dimension);
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem data_type_items[] = {
      {CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT, "FLOAT", ICON_NONE, "Float", "Float value"},
      {CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR, "VECTOR", ICON_NONE, "Vector", "Vector value"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "The type of data",
                    data_type_items,
                    NOD_inline_enum_accessors(custom1),
                    CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT,
                    nullptr,
                    true);

  static const EnumPropertyItem reference_dimension_items[] = {
      {CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION,
       "PER_DIMENSION",
       ICON_NONE,
       "Per Dimension",
       "The value is relative to each of the dimensions of the image independently"},
      {CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X,
       "X",
       ICON_NONE,
       "X",
       "The value is relative to the X dimension of the image"},
      {CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_Y,
       "Y",
       ICON_NONE,
       "Y",
       "The value is relative to the Y dimension of the image"},
      {CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_GREATER,
       "Greater",
       ICON_NONE,
       "Greater",
       "The value is relative to the greater dimension of the image"},
      {CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_SMALLER,
       "Smaller",
       ICON_NONE,
       "Smaller",
       "The value is relative to the smaller dimension of the image"},
      {CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_DIAGONAL,
       "Diagonal",
       ICON_NONE,
       "Diagonal",
       "The value is relative to the diagonal of the image"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(
      srna,
      "reference_dimension",
      "Reference Dimension",
      "Defines the dimension of the image that the relative value is in reference to",
      reference_dimension_items,
      NOD_inline_enum_accessors(custom2),
      CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X,
      nullptr,
      true);
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(ptr, "reference_dimension", UI_ITEM_NONE, "", ICON_NONE);
}

using namespace blender::compositor;

class RelativeToPixelOperation : public NodeOperation {
 public:
  RelativeToPixelOperation(Context &context, const bNode &node) : NodeOperation(context, node)
  {
    InputDescriptor &image_descriptor = this->get_input_descriptor("Image");
    image_descriptor.skip_type_conversion = true;
  }

  void execute() override
  {
    if (this->get_data_type() == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT) {
      const Result &input = this->get_input("Float Value");
      if (this->get_reference_dimension() !=
          CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION)
      {
        if (input.is_single_value()) {
          this->execute_float_single();
        }
        else {
          this->execute_float();
        }
      }
      else {
        if (input.is_single_value()) {
          this->execute_float_per_dimension_single();
        }
        else {
          this->execute_float_per_dimension();
        }
      }
    }
    else {
      const Result &input = this->get_input("Vector Value");
      if (input.is_single_value()) {
        this->execute_vector_single();
      }
      else {
        this->execute_vector();
      }
    }
  }

  void execute_float_single()
  {
    const float input_value = this->get_input("Float Value").get_single_value_default<float>();
    const float2 reference_size = this->compute_reference_size();

    const float value_in_pixels = input_value * reference_size.x;

    Result &output = this->get_result("Float Value");
    output.allocate_single_value();
    output.set_single_value(value_in_pixels);
  }

  void execute_float()
  {
    if (this->context().use_gpu()) {
      this->execute_float_gpu();
    }
    else {
      this->execute_float_cpu();
    }
  }

  void execute_float_gpu()
  {
    gpu::Shader *shader = context().get_shader("compositor_relative_to_pixel_float");
    GPU_shader_bind(shader);

    const float reference_size = this->compute_reference_size().x;
    GPU_shader_uniform_1f(shader, "reference_size", reference_size);

    const Result &input = this->get_input("Float Value");
    input.bind_as_texture(shader, "input_tx");

    Result &output = this->get_result("Float Value");
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().data_size);

    input.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_float_cpu()
  {
    const Result &input = this->get_input("Float Value");
    const float reference_size = this->compute_reference_size().x;

    Result &output = this->get_result("Float Value");
    output.allocate_texture(input.domain());
    parallel_for(input.domain().data_size, [&](const int2 texel) {
      output.store_pixel(texel, input.load_pixel<float>(texel) * reference_size);
    });
  }

  void execute_float_per_dimension_single()
  {
    const float input_value = this->get_input("Float Value").get_single_value_default<float>();
    const float2 reference_size = this->compute_reference_size();

    const float2 value_in_pixels = input_value * reference_size;

    Result &output = this->get_result("Vector Value");
    output.allocate_single_value();
    output.set_single_value(value_in_pixels);
  }

  void execute_float_per_dimension()
  {
    if (this->context().use_gpu()) {
      this->execute_float_per_dimension_gpu();
    }
    else {
      this->execute_float_per_dimension_cpu();
    }
  }

  void execute_float_per_dimension_gpu()
  {
    gpu::Shader *shader = context().get_shader("compositor_relative_to_pixel_float_per_dimension");
    GPU_shader_bind(shader);

    const float2 reference_size = this->compute_reference_size();
    GPU_shader_uniform_2fv(shader, "reference_size", reference_size);

    const Result &input = this->get_input("Float Value");
    input.bind_as_texture(shader, "input_tx");

    Result &output = this->get_result("Vector Value");
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().data_size);

    input.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_float_per_dimension_cpu()
  {
    const Result &input = this->get_input("Float Value");
    const float2 reference_size = this->compute_reference_size();

    Result &output = this->get_result("Vector Value");
    output.allocate_texture(input.domain());
    parallel_for(input.domain().data_size, [&](const int2 texel) {
      output.store_pixel(texel, input.load_pixel<float>(texel) * reference_size);
    });
  }

  void execute_vector_single()
  {
    const float2 input_value = this->get_input("Vector Value").get_single_value_default<float2>();
    const float2 reference_size = this->compute_reference_size();

    const float2 value_in_pixels = input_value * reference_size;

    Result &output = this->get_result("Vector Value");
    output.allocate_single_value();
    output.set_single_value(value_in_pixels);
  }

  void execute_vector()
  {
    if (this->context().use_gpu()) {
      this->execute_vector_gpu();
    }
    else {
      this->execute_vector_cpu();
    }
  }

  void execute_vector_gpu()
  {
    gpu::Shader *shader = context().get_shader("compositor_relative_to_pixel_vector");
    GPU_shader_bind(shader);

    const float2 reference_size = this->compute_reference_size();
    GPU_shader_uniform_2fv(shader, "reference_size", reference_size);

    const Result &input = this->get_input("Vector Value");
    input.bind_as_texture(shader, "input_tx");

    Result &output = this->get_result("Vector Value");
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().data_size);

    input.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_vector_cpu()
  {
    const Result &input = this->get_input("Vector Value");
    const float2 reference_size = this->compute_reference_size();

    Result &output = this->get_result("Vector Value");
    output.allocate_texture(input.domain());
    parallel_for(input.domain().data_size, [&](const int2 texel) {
      output.store_pixel(texel, input.load_pixel<float2>(texel) * reference_size);
    });
  }

  float2 compute_reference_size()
  {
    const Result &input_image = this->get_input("Image");
    if (input_image.is_single_value()) {
      return float2(1.0f);
    }

    const Domain domain = RealizeOnDomainOperation::compute_realized_transformation_domain(
        this->context(), input_image.domain());
    const float2 image_size = float2(domain.display_size);
    switch (this->get_reference_dimension()) {
      case CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION:
        return image_size;
      case CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X:
        return float2(image_size.x);
      case CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_Y:
        return float2(image_size.y);
      case CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_GREATER:
        return float2(math::reduce_max(image_size));
      case CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_SMALLER:
        return float2(math::reduce_min(image_size));
      case CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_DIAGONAL:
        return float2(math::length(image_size));
    }

    BLI_assert_unreachable();
    return float2(1.0f);
  }

  CMPNodeRelativeToPixelDataType get_data_type()
  {
    return CMPNodeRelativeToPixelDataType(this->node().custom1);
  }

  CMPNodeRelativeToPixelReferenceDimension get_reference_dimension()
  {
    return CMPNodeRelativeToPixelReferenceDimension(this->node().custom2);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new RelativeToPixelOperation(context, node);
}

static void register_node()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeRelativeToPixel");
  ntype.ui_name = "Relative To Pixel";
  ntype.ui_description =
      "Converts values that are relative to the image size to be in terms of pixels";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.updatefunc = node_update;
  ntype.draw_buttons = node_layout;
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_composite_relative_to_pixel_cc
