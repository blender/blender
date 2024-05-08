/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_node_extra_info.hh"
#include "NOD_rna_define.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_axes_to_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Primary Axis")).default_value(float3(0, 0, 1));
  b.add_input<decl::Vector>(N_("Secondary Axis")).default_value(float3(1, 0, 0));
  b.add_output<decl::Rotation>(N_("Rotation"));
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int(math::Axis::Z);
  node->custom2 = int(math::Axis::X);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "primary_axis", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "secondary_axis", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static float3 get_orthogonal_of_non_zero_vector(const float3 &v)
{
  BLI_assert(!math::is_zero(v));
  if (v.x != -v.y) {
    return float3{-v.y, v.x, 0.0f};
  }
  if (v.x != -v.z) {
    return float3(-v.z, 0.0f, v.x);
  }
  return {0.0f, -v.z, v.y};
}

class AxesToRotationFunction : public mf::MultiFunction {
 private:
  math::Axis primary_axis_;
  math::Axis secondary_axis_;
  math::Axis tertiary_axis_;

 public:
  AxesToRotationFunction(const math::Axis primary_axis, const math::Axis secondary_axis)
      : primary_axis_(primary_axis), secondary_axis_(secondary_axis)
  {
    BLI_assert(primary_axis_ != secondary_axis_);

    /* Through cancellation this will set the last axis to be the one that's neither the primary
     * nor secondary axis. */
    tertiary_axis_ = math::Axis::from_int((0 + 1 + 2) - primary_axis.as_int() -
                                          secondary_axis.as_int());

    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Axes to Rotation", signature};
      builder.single_input<float3>("Primary");
      builder.single_input<float3>("Secondary");
      builder.single_output<math::Quaternion>("Rotation");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> primaries = params.readonly_single_input<float3>(0, "Primary");
    const VArray<float3> secondaries = params.readonly_single_input<float3>(1, "Secondary");
    MutableSpan r_rotations = params.uninitialized_single_output<math::Quaternion>(2, "Rotation");

    /* Might have to invert the axis to make sure that the created matrix has determinant 1. */
    const bool invert_tertiary = (secondary_axis_.as_int() + 1) % 3 == primary_axis_.as_int();
    const float tertiary_factor = invert_tertiary ? -1.0f : 1.0f;

    mask.foreach_index([&](const int64_t i) {
      float3 primary = math::normalize(primaries[i]);
      float3 secondary = secondaries[i];
      float3 tertiary;

      const bool primary_is_non_zero = !math::is_zero(primary);
      const bool secondary_is_non_zero = !math::is_zero(secondary);
      if (primary_is_non_zero && secondary_is_non_zero) {
        tertiary = math::cross(primary, secondary);
        if (math::is_zero(tertiary)) {
          tertiary = get_orthogonal_of_non_zero_vector(primary);
        }
        tertiary = math::normalize(tertiary);
        secondary = math::cross(tertiary, primary);
      }
      else if (primary_is_non_zero) {
        secondary = get_orthogonal_of_non_zero_vector(primary);
        secondary = math::normalize(secondary);
        tertiary = math::cross(primary, secondary);
      }
      else if (secondary_is_non_zero) {
        secondary = math::normalize(secondary);
        primary = get_orthogonal_of_non_zero_vector(secondary);
        primary = math::normalize(primary);
        tertiary = math::cross(primary, secondary);
      }
      else {
        r_rotations[i] = math::Quaternion::identity();
        return;
      }

      float3x3 mat;
      mat[primary_axis_.as_int()] = primary;
      mat[secondary_axis_.as_int()] = secondary;
      mat[tertiary_axis_.as_int()] = tertiary_factor * tertiary;
      BLI_assert(math::is_orthonormal(mat));
      BLI_assert(std::abs(math::determinant(mat) - 1.0f) < 0.0001f);

      r_rotations[i] = math::to_quaternion(mat);
    });
  };
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  if (node.custom1 == node.custom2) {
    return;
  }
  builder.construct_and_set_matching_fn<AxesToRotationFunction>(
      math::Axis::from_int(node.custom1), math::Axis::from_int(node.custom2));
}

static void node_extra_info(NodeExtraInfoParams &params)
{
  if (params.node.custom1 == params.node.custom2) {
    NodeExtraInfoRow row;
    row.text = RPT_("Equal Axes");
    row.tooltip = TIP_("The primary and secondary axis have to be different");
    row.icon = ICON_ERROR;
    params.rows.append(std::move(row));
  }
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem axis_items[] = {
      {int(math::Axis::X), "X", ICON_NONE, "X", ""},
      {int(math::Axis::Y), "Y", ICON_NONE, "Y", ""},
      {int(math::Axis::Z), "Z", ICON_NONE, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "primary_axis",
                    "Primary Axis",
                    "Axis that is aligned exactly to the provided primary direction",
                    axis_items,
                    NOD_inline_enum_accessors(custom1));
  RNA_def_node_enum(
      srna,
      "secondary_axis",
      "Secondary Axis",
      "Axis that is aligned as well as possible given the alignment of the primary axis",
      axis_items,
      NOD_inline_enum_accessors(custom2));
}

static void node_register()
{
  static bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_AXES_TO_ROTATION, "Axes to Rotation", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;
  ntype.get_extra_info = node_extra_info;
  node_rna(ntype.rna_ext.srna);
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_axes_to_rotation_cc
