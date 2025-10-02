/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_enum_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_stabilize2d_cc {

static void cmp_node_stabilize2d_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .hide_value()
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_layout([](uiLayout *layout, bContext *context, PointerRNA *node_pointer) {
    uiTemplateID(layout, context, node_pointer, "clip", nullptr, "CLIP_OT_open", nullptr);
  });

  b.add_input<decl::Bool>("Invert").default_value(false).description(
      "Invert stabilization to reintroduce motion to the image");

  PanelDeclarationBuilder &sampling_panel = b.add_panel("Sampling").default_closed(true);
  sampling_panel.add_input<decl::Menu>("Interpolation")
      .default_value(CMP_NODE_INTERPOLATION_BILINEAR)
      .static_items(rna_enum_node_compositor_interpolation_items)
      .optional_label()
      .description("Interpolation method");
  sampling_panel.add_input<decl::Menu>("Extension X")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .optional_label()
      .description("The extension mode applied to the X axis");
  sampling_panel.add_input<decl::Menu>("Extension Y")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .optional_label()
      .description("The extension mode applied to the Y axis");
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  Scene *scene = CTX_data_scene(C);

  node->id = (ID *)scene->clip;
  id_us_plus(node->id);
}

using namespace blender::compositor;

class Stabilize2DOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");

    MovieClip *movie_clip = get_movie_clip();
    if (input.is_single_value() || !movie_clip) {
      output.share_data(input);
      return;
    }

    const int width = input.domain().size.x;
    const int height = input.domain().size.y;
    const int frame_number = BKE_movieclip_remap_scene_to_clip_frame(movie_clip,
                                                                     context().get_frame_number());

    float2 translation;
    float scale, rotation;
    BKE_tracking_stabilization_data_get(
        movie_clip, frame_number, width, height, translation, &scale, &rotation);

    float3x3 transformation = math::from_loc_rot_scale<float3x3>(
        translation, math::AngleRadian(rotation), float2(scale));
    if (do_inverse_stabilization()) {
      transformation = math::invert(transformation);
    }

    output.share_data(input);
    output.transform(transformation);
    output.get_realization_options().interpolation = this->get_interpolation();
    output.get_realization_options().extension_x = this->get_extension_mode_x();
    output.get_realization_options().extension_y = this->get_extension_mode_y();
  }

  Interpolation get_interpolation()
  {
    const Result &input = this->get_input("Interpolation");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_INTERPOLATION_BILINEAR);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPNodeInterpolation interpolation = static_cast<CMPNodeInterpolation>(menu_value.value);
    switch (interpolation) {
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_ANISOTROPIC:
      case CMP_NODE_INTERPOLATION_BICUBIC:
        return Interpolation::Bicubic;
    }

    return Interpolation::Nearest;
  }

  ExtensionMode get_extension_mode_x()
  {
    const Result &input = this->get_input("Extension X");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_EXTENSION_MODE_CLIP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPExtensionMode extension_x = static_cast<CMPExtensionMode>(menu_value.value);
    switch (extension_x) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    return ExtensionMode::Clip;
  }

  ExtensionMode get_extension_mode_y()
  {
    const Result &input = this->get_input("Extension Y");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_EXTENSION_MODE_CLIP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPExtensionMode extension_y = static_cast<CMPExtensionMode>(menu_value.value);
    switch (extension_y) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    return ExtensionMode::Clip;
  }

  bool do_inverse_stabilization()
  {
    return this->get_input("Invert").get_single_value_default(false);
  }

  MovieClip *get_movie_clip()
  {
    return reinterpret_cast<MovieClip *>(bnode().id);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new Stabilize2DOperation(context, node);
}

}  // namespace blender::nodes::node_composite_stabilize2d_cc

static void register_node_type_cmp_stabilize2d()
{
  namespace file_ns = blender::nodes::node_composite_stabilize2d_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeStabilize", CMP_NODE_STABILIZE2D);
  ntype.ui_name = "Stabilize 2D";
  ntype.ui_description = "Stabilize footage using 2D stabilization motion tracking settings";
  ntype.enum_name_legacy = "STABILIZE2D";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_stabilize2d_declare;
  ntype.initfunc_api = file_ns::init;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_stabilize2d)
