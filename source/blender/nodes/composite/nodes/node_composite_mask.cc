/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_string_utf8.h"

#include "DNA_mask_types.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "COM_cached_mask.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_mask_cc {

static const EnumPropertyItem size_source_items[] = {
    {0, "SCENE", 0, "Scene Size", ""},
    {CMP_NODE_MASK_FLAG_SIZE_FIXED, "FIXED", 0, N_("Fixed"), N_("Use pixel size for the buffer")},
    {CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE,
     "FIXED_SCENE",
     0,
     N_("Fixed/Scene"),
     N_("Pixel size scaled by scene percentage")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_mask_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();

  b.add_output<decl::Float>("Mask").structure_type(StructureType::Dynamic);

  b.add_layout([](uiLayout *layout, bContext *C, PointerRNA *ptr) {
    uiTemplateID(layout, C, ptr, "mask", nullptr, nullptr, nullptr);
  });

  b.add_input<decl::Menu>("Size Source")
      .default_value(MenuValue(0))
      .static_items(size_source_items)
      .optional_label()
      .description("The source where the size of the mask is retrieved");
  b.add_input<decl::Int>("Size X")
      .default_value(256)
      .min(1)
      .usage_by_menu("Size Source",
                     {CMP_NODE_MASK_FLAG_SIZE_FIXED, CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE})
      .description("The resolution of the mask along the X direction");
  b.add_input<decl::Int>("Size Y")
      .default_value(256)
      .min(1)
      .usage_by_menu("Size Source",
                     {CMP_NODE_MASK_FLAG_SIZE_FIXED, CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE})
      .description("The resolution of the mask along the Y direction");
  b.add_input<decl::Bool>("Feather").default_value(true).description(
      "Use feather information from the mask");

  PanelDeclarationBuilder &motion_blur_panel = b.add_panel("Motion Blur").default_closed(true);
  motion_blur_panel.add_input<decl::Bool>("Motion Blur")
      .default_value(false)
      .panel_toggle()
      .description("Use multi-sampled motion blur of the mask");
  motion_blur_panel.add_input<decl::Int>("Samples", "Motion Blur Samples")
      .default_value(16)
      .min(1)
      .max(64)
      .description("Number of motion blur samples");
  motion_blur_panel.add_input<decl::Float>("Shutter", "Motion Blur Shutter")
      .default_value(0.5f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description("Exposure for motion blur as a factor of FPS");
}

static void node_mask_label(const bNodeTree * /*ntree*/,
                            const bNode *node,
                            char *label,
                            int label_maxncpy)
{
  BLI_strncpy_utf8(label, node->id ? node->id->name + 2 : IFACE_("Mask"), label_maxncpy);
}

using namespace blender::compositor;

class MaskOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &output_mask = this->get_result("Mask");
    if (!this->get_mask() ||
        (!this->is_fixed_size() && !this->context().is_valid_compositing_region()))
    {
      output_mask.allocate_invalid();
      return;
    }

    const Domain domain = compute_domain();
    Result &cached_mask = context().cache_manager().cached_masks.get(
        this->context(),
        this->get_mask(),
        domain.size,
        this->get_aspect_ratio(),
        this->get_use_feather(),
        this->get_motion_blur_samples(),
        this->get_motion_blur_shutter());

    output_mask.wrap_external(cached_mask);
  }

  Domain compute_domain() override
  {
    return Domain(this->compute_size());
  }

  int2 compute_size()
  {
    if (this->get_flags() & CMP_NODE_MASK_FLAG_SIZE_FIXED) {
      return this->get_size();
    }

    if (this->get_flags() & CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE) {
      return this->get_size() * this->context().get_render_percentage();
    }

    return this->context().get_compositing_region_size();
  }

  int2 get_size()
  {
    return int2(math::max(1, this->get_input("Size X").get_single_value_default(256)),
                math::max(1, this->get_input("Size Y").get_single_value_default(256)));
  }

  float get_aspect_ratio()
  {
    if (this->is_fixed_size()) {
      return 1.0f;
    }

    return this->context().get_render_data().yasp / this->context().get_render_data().xasp;
  }

  bool is_fixed_size()
  {
    return this->get_flags() &
           (CMP_NODE_MASK_FLAG_SIZE_FIXED | CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE);
  }

  bool get_use_feather()
  {
    return this->get_input("Feather").get_single_value_default(true);
  }

  int get_motion_blur_samples()
  {
    const int samples = math::clamp(
        this->get_input("Motion Blur Samples").get_single_value_default(16), 1, 64);
    return this->use_motion_blur() ? samples : 1;
  }

  float get_motion_blur_shutter()
  {
    return math::clamp(
        this->get_input("Motion Blur Shutter").get_single_value_default(0.5f), 0.0f, 1.0f);
  }

  bool use_motion_blur()
  {
    return this->get_input("Motion Blur").get_single_value_default(false);
  }

  CMPNodeMaskFlags get_flags()
  {
    const Result &input = this->get_input("Size Source");
    const MenuValue default_menu_value = MenuValue(0);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeMaskFlags>(menu_value.value);
  }

  Mask *get_mask()
  {
    return reinterpret_cast<Mask *>(this->bnode().id);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_mask_cc

static void register_node_type_cmp_mask()
{
  namespace file_ns = blender::nodes::node_composite_mask_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeMask", CMP_NODE_MASK);
  ntype.ui_name = "Mask";
  ntype.ui_description = "Input mask from a mask data-block, created in the image editor";
  ntype.enum_name_legacy = "MASK";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_mask_declare;
  ntype.labelfunc = file_ns::node_mask_label;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_mask)
