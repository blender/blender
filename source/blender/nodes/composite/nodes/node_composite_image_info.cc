/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector_types.hh"

#include "COM_node_operation.hh"
#include "COM_realize_on_domain_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_image_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .hide_value()
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Vector>("Dimensions")
      .dimensions(2)
      .description("The dimensions of the image in pixels with transformations applied");
  b.add_output<decl::Vector>("Resolution")
      .dimensions(2)
      .description("The original resolution of the image in pixels before any transformations");
  b.add_output<decl::Vector>("Location").dimensions(2);
  b.add_output<decl::Float>("Rotation");
  b.add_output<decl::Vector>("Scale").dimensions(2);
}

using namespace blender::compositor;

class ImageInfoOperation : public NodeOperation {
 public:
  ImageInfoOperation(Context &context, DNode node) : NodeOperation(context, node)
  {
    InputDescriptor &image_descriptor = this->get_input_descriptor("Image");
    image_descriptor.skip_type_conversion = true;
  }

  void execute() override
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      this->execute_invalid();
      return;
    }

    const Domain domain = input.domain();

    Result &dimensions_result = this->get_result("Dimensions");
    if (dimensions_result.should_compute()) {
      dimensions_result.allocate_single_value();
      const Domain realized_domain =
          RealizeOnDomainOperation::compute_realized_transformation_domain(this->context(),
                                                                           domain);
      dimensions_result.set_single_value(float2(realized_domain.size));
    }

    Result &resolution_result = this->get_result("Resolution");
    if (resolution_result.should_compute()) {
      resolution_result.allocate_single_value();
      resolution_result.set_single_value(float2(domain.size));
    }

    math::AngleRadian rotation;
    float2 location, scale;
    math::to_loc_rot_scale(domain.transformation, location, rotation, scale);

    Result &location_result = this->get_result("Location");
    if (location_result.should_compute()) {
      location_result.allocate_single_value();
      location_result.set_single_value(location);
    }

    Result &rotation_result = this->get_result("Rotation");
    if (rotation_result.should_compute()) {
      rotation_result.allocate_single_value();
      rotation_result.set_single_value(float(rotation));
    }

    Result &scale_result = this->get_result("Scale");
    if (scale_result.should_compute()) {
      scale_result.allocate_single_value();
      scale_result.set_single_value(scale);
    }
  }

  void execute_invalid()
  {
    Result &dimensions_result = this->get_result("Dimensions");
    if (dimensions_result.should_compute()) {
      dimensions_result.allocate_invalid();
    }

    Result &resolution_result = this->get_result("Resolution");
    if (resolution_result.should_compute()) {
      resolution_result.allocate_invalid();
    }

    Result &location_result = this->get_result("Location");
    if (location_result.should_compute()) {
      location_result.allocate_invalid();
    }

    Result &rotation_result = this->get_result("Rotation");
    if (rotation_result.should_compute()) {
      rotation_result.allocate_invalid();
    }

    Result &scale_result = this->get_result("Scale");
    if (scale_result.should_compute()) {
      scale_result.allocate_invalid();
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ImageInfoOperation(context, node);
}

}  // namespace blender::nodes::node_composite_image_info_cc

static void register_node_type_cmp_image_info()
{
  namespace file_ns = blender::nodes::node_composite_image_info_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeImageInfo", CMP_NODE_IMAGE_INFO);
  ntype.ui_name = "Image Info";
  ntype.ui_description = "Returns information about an image";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_image_info)
