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
      .description(
          "A value that is relative to the image size and needs to be converted to be in pixels");
  b.add_input<decl::Float>("Value", "Float Value")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "A value that is relative to the image size and needs to be converted to be in pixels");
  b.add_input<decl::Color>("Image")
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Float>("Value", "Float Value");
  b.add_output<decl::Vector>("Value", "Vector Value").dimensions(2);
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
  blender::bke::node_set_socket_availability(
      *ntree, *float_input, data_type == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT);

  bNodeSocket *vector_input = bke::node_find_socket(*node, SOCK_IN, "Vector Value");
  blender::bke::node_set_socket_availability(
      *ntree, *vector_input, data_type == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR);

  /* The float output doesn't exist if the reference is per dimension, since each dimension can be
   * different. */
  const bool is_per_dimension = reference_dimension ==
                                CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION;
  bNodeSocket *float_output = bke::node_find_socket(*node, SOCK_OUT, "Float Value");
  blender::bke::node_set_socket_availability(
      *ntree,
      *float_output,
      data_type == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT && !is_per_dimension);

  /* The vector output exist if the reference is per dimension even if the data type is float,
   * since each dimension can be different. */
  bNodeSocket *vector_output = bke::node_find_socket(*node, SOCK_OUT, "Vector Value");
  blender::bke::node_set_socket_availability(
      *ntree,
      *vector_output,
      data_type == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR || is_per_dimension);
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

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr, "reference_dimension", UI_ITEM_NONE, "", ICON_NONE);
}

using namespace blender::compositor;

class RelativeToPixelOperation : public NodeOperation {
 public:
  RelativeToPixelOperation(Context &context, DNode node) : NodeOperation(context, node)
  {
    InputDescriptor &image_descriptor = this->get_input_descriptor("Image");
    image_descriptor.skip_type_conversion = true;
  }

  void execute() override
  {
    const float2 input_value = this->get_input_value();
    const float2 reference_size = this->compute_reference_size();

    const float2 value_in_pixels = input_value * reference_size;

    /* The float output doesn't exist if the reference is per dimension, since each dimension can
     * be different. */
    const bool is_per_dimension = this->get_reference_dimension() ==
                                  CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION;
    if (this->get_data_type() == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT && !is_per_dimension) {
      Result &output_float_value = this->get_result("Float Value");
      if (output_float_value.should_compute()) {
        output_float_value.allocate_single_value();
        /* Both values of the float2 are identical in this case, so just set the x component. */
        output_float_value.set_single_value(value_in_pixels.x);
      }
    }

    /* The vector output exist if the reference is per dimension even if the data type is float,
     * since each dimension can be different. */
    if (this->get_data_type() == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR || is_per_dimension) {
      Result &output_vector_value = this->get_result("Vector Value");
      if (output_vector_value.should_compute()) {
        output_vector_value.allocate_single_value();
        output_vector_value.set_single_value(value_in_pixels);
      }
    }
  }

  float2 get_input_value()
  {
    if (this->get_data_type() == CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT) {
      return float2(this->get_input("Float Value").get_single_value_default(0.0f));
    }
    return this->get_input("Vector Value").get_single_value_default(float2(0.0f));
  }

  float2 compute_reference_size()
  {
    const Result &input_image = this->get_input("Image");
    if (input_image.is_single_value()) {
      return float2(1.0f);
    }

    const Domain domain = RealizeOnDomainOperation::compute_realized_transformation_domain(
        this->context(), input_image.domain());
    const float2 image_size = float2(domain.size);
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
    return CMPNodeRelativeToPixelDataType(this->bnode().custom1);
  }

  CMPNodeRelativeToPixelReferenceDimension get_reference_dimension()
  {
    return CMPNodeRelativeToPixelReferenceDimension(this->bnode().custom2);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new RelativeToPixelOperation(context, node);
}

static void register_node()
{
  static blender::bke::bNodeType ntype;

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

  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_composite_relative_to_pixel_cc
