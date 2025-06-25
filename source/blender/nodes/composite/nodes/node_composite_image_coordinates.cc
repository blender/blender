/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_image_coordinates_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .hide_value()
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Vector>("Uniform")
      .dimensions(2)
      .structure_type(StructureType::Dynamic)
      .description(
          "Zero centered coordinates normalizes along the larger dimension for uniform scaling");
  b.add_output<decl::Vector>("Normalized")
      .dimensions(2)
      .structure_type(StructureType::Dynamic)
      .description("Normalized coordinates with half pixel offsets");
  b.add_output<decl::Vector>("Pixel")
      .dimensions(2)
      .structure_type(StructureType::Dynamic)
      .description("Integer pixel coordinates");
}

using namespace blender::compositor;

class ImageCoordinatesOperation : public NodeOperation {
 public:
  ImageCoordinatesOperation(Context &context, DNode node) : NodeOperation(context, node)
  {
    InputDescriptor &image_descriptor = this->get_input_descriptor("Image");
    image_descriptor.skip_type_conversion = true;
  }

  void execute() override
  {
    const Result &input = this->get_input("Image");
    Result &uniform_coordinates_result = this->get_result("Uniform");
    Result &normalized_coordinates_result = this->get_result("Normalized");
    Result &pixel_coordinates_result = this->get_result("Pixel");
    if (input.is_single_value()) {
      if (uniform_coordinates_result.should_compute()) {
        uniform_coordinates_result.allocate_invalid();
      }
      if (normalized_coordinates_result.should_compute()) {
        normalized_coordinates_result.allocate_invalid();
      }
      if (pixel_coordinates_result.should_compute()) {
        pixel_coordinates_result.allocate_invalid();
      }
      return;
    }

    const Domain domain = input.domain();

    if (uniform_coordinates_result.should_compute()) {
      const Result &uniform_coordinates = this->context().cache_manager().image_coordinates.get(
          this->context(), domain.size, CoordinatesType::Uniform);
      uniform_coordinates_result.wrap_external(uniform_coordinates);
      uniform_coordinates_result.transform(domain.transformation);
    }

    if (normalized_coordinates_result.should_compute()) {
      const Result &normalized_coordinates = this->context().cache_manager().image_coordinates.get(
          this->context(), domain.size, CoordinatesType::Normalized);
      normalized_coordinates_result.wrap_external(normalized_coordinates);
      normalized_coordinates_result.transform(domain.transformation);
    }

    if (pixel_coordinates_result.should_compute()) {
      const Result &pixel_coordinates = this->context().cache_manager().image_coordinates.get(
          this->context(), domain.size, CoordinatesType::Pixel);
      pixel_coordinates_result.wrap_external(pixel_coordinates);
      pixel_coordinates_result.transform(domain.transformation);
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ImageCoordinatesOperation(context, node);
}

static void register_node()
{
  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeImageCoordinates");
  ntype.ui_name = "Image Coordinates";
  ntype.ui_description = "Returns the coordinates of the pixels of an image";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_composite_image_coordinates_cc
