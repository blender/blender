/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_image_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image").compositor_domain_priority(0).compositor_realization_mode(
      CompositorInputRealizationMode::None);

  b.add_output<decl::Vector>("Texture Coordinates");
  b.add_output<decl::Vector>("Pixel Coordinates");
  b.add_output<decl::Vector>("Resolution");
  b.add_output<decl::Vector>("Location");
  b.add_output<decl::Float>("Rotation");
  b.add_output<decl::Vector>("Scale");
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

    Result &texture_coordinates_result = this->get_result("Texture Coordinates");
    if (texture_coordinates_result.should_compute()) {
      const Result &texture_coordinates = this->context().cache_manager().texture_coordinates.get(
          this->context(), domain.size);
      texture_coordinates_result.wrap_external(texture_coordinates);
      texture_coordinates_result.transform(domain.transformation);
    }

    Result &pixel_coordinates_result = this->get_result("Pixel Coordinates");
    if (pixel_coordinates_result.should_compute()) {
      const Result &pixel_coordinates = this->context().cache_manager().pixel_coordinates.get(
          this->context(), domain.size);
      pixel_coordinates_result.wrap_external(pixel_coordinates);
      pixel_coordinates_result.transform(domain.transformation);
    }

    Result &resolution_result = this->get_result("Resolution");
    if (resolution_result.should_compute()) {
      resolution_result.allocate_single_value();
      resolution_result.set_single_value(float3(domain.size, 0.0f));
    }

    math::AngleRadian rotation;
    float2 location, scale;
    math::to_loc_rot_scale(domain.transformation, location, rotation, scale);

    Result &location_result = this->get_result("Location");
    if (location_result.should_compute()) {
      location_result.allocate_single_value();
      location_result.set_single_value(float3(location, 0.0f));
    }

    Result &rotation_result = this->get_result("Rotation");
    if (rotation_result.should_compute()) {
      rotation_result.allocate_single_value();
      rotation_result.set_single_value(float(rotation));
    }

    Result &scale_result = this->get_result("Scale");
    if (scale_result.should_compute()) {
      scale_result.allocate_single_value();
      scale_result.set_single_value(float3(scale, 0.0f));
    }
  }

  void execute_invalid()
  {
    Result &texture_coordinates_result = this->get_result("Texture Coordinates");
    if (texture_coordinates_result.should_compute()) {
      texture_coordinates_result.allocate_invalid();
    }

    Result &pixel_coordinates_result = this->get_result("Pixel Coordinates");
    if (pixel_coordinates_result.should_compute()) {
      pixel_coordinates_result.allocate_invalid();
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

void register_node_type_cmp_image_info()
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
