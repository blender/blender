/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "node_function_util.hh"
#include "node_shader_util.hh"

namespace blender::nodes::node_fn_rotate_rotation_cc {

enum class RotationSpace {
  Global = 0,
  Local = 1,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.is_function_node();
  b.add_input<decl::Rotation>("Rotation"_ustr);
  b.add_output<decl::Rotation>("Rotation"_ustr).align_with_previous();
  b.add_input<decl::Rotation>("Rotate By"_ustr);
};

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "rotation_space", ui::ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

static int node_gpu_material(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  const char *name = nullptr;
  switch (RotationSpace(node->custom1)) {
    case RotationSpace::Global:
      name = "rotate_rotation_global";
      break;
    case RotationSpace::Local:
      name = "rotate_rotation_local";
      break;
  }

  if (name != nullptr) {
    return GPU_stack_link(mat, node, name, in, out);
  }
  return 0;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  switch (RotationSpace(builder.node().custom1)) {
    case RotationSpace::Global: {
      static auto fn = mf::build::SI2_SO<math::Quaternion, math::Quaternion, math::Quaternion>(
          "Rotate Rotation Global", [](math::Quaternion a, math::Quaternion b) { return b * a; });
      builder.set_matching_fn(fn);
      break;
    }
    case RotationSpace::Local: {
      static auto fn = mf::build::SI2_SO<math::Quaternion, math::Quaternion, math::Quaternion>(
          "Rotate Rotation Local", [](math::Quaternion a, math::Quaternion b) { return a * b; });
      builder.set_matching_fn(fn);
      break;
    }
  }
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem space_items[] = {
      {int(RotationSpace::Global),
       "GLOBAL",
       ICON_NONE,
       "Global",
       "Rotate the input rotation in global space"},
      {int(RotationSpace::Local),
       "LOCAL",
       ICON_NONE,
       "Local",
       "Rotate the input rotation in its local space"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "rotation_space",
                    "Space",
                    "Base orientation for the rotation",
                    space_items,
                    NOD_inline_enum_accessors(custom1));
}

static void node_register()
{
  static bke::bNodeType ntype;
  fn_cmp_node_type_base(&ntype, "FunctionNodeRotateRotation"_ustr, FN_NODE_ROTATE_ROTATION);
  ntype.ui_name = "Rotate Rotation";
  ntype.ui_description = "Apply a secondary rotation to a given rotation value";
  ntype.enum_name_legacy = "ROTATE_ROTATION";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.gpu_fn = node_gpu_material;
  ntype.build_multi_function = node_build_multi_function;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_rotate_rotation_cc
