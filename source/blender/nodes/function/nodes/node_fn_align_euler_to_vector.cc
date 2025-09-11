/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_align_euler_to_vector_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Rotation").subtype(PROP_EULER).hide_value();
  b.add_input<decl::Float>("Factor").default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Vector>("Vector").default_value({0.0, 0.0, 1.0});
  b.add_output<decl::Vector>("Rotation").subtype(PROP_EULER);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "axis", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  layout->prop(ptr, "pivot_axis", UI_ITEM_NONE, IFACE_("Pivot"), ICON_NONE);
}

static void align_rotations_auto_pivot(const IndexMask &mask,
                                       const VArray<float3> &input_rotations,
                                       const VArray<float3> &vectors,
                                       const VArray<float> &factors,
                                       const float3 local_main_axis,
                                       MutableSpan<float3> output_rotations)
{
  mask.foreach_index([&](const int64_t i) {
    const float3 vector = vectors[i];
    if (math::is_zero(vector)) {
      output_rotations[i] = input_rotations[i];
      return;
    }

    float old_rotation[3][3];
    eul_to_mat3(old_rotation, input_rotations[i]);
    float3 old_axis;
    mul_v3_m3v3(old_axis, old_rotation, local_main_axis);

    const float3 new_axis = math::normalize(vector);
    float3 rotation_axis = math::cross_high_precision(old_axis, new_axis);
    if (math::is_zero(rotation_axis)) {
      /* The vectors are linearly dependent, so we fall back to another axis. */
      rotation_axis = math::cross_high_precision(old_axis, float3(1, 0, 0));
      if (math::is_zero(rotation_axis)) {
        /* This is now guaranteed to not be zero. */
        rotation_axis = math::cross_high_precision(old_axis, float3(0, 1, 0));
      }
    }

    const float full_angle = angle_normalized_v3v3(old_axis, new_axis);
    const float angle = factors[i] * full_angle;

    float rotation[3][3];
    axis_angle_to_mat3(rotation, rotation_axis, angle);

    float new_rotation_matrix[3][3];
    mul_m3_m3m3(new_rotation_matrix, rotation, old_rotation);

    float3 new_rotation;
    mat3_to_eul(new_rotation, new_rotation_matrix);

    output_rotations[i] = new_rotation;
  });
}

static void align_rotations_fixed_pivot(const IndexMask &mask,
                                        const VArray<float3> &input_rotations,
                                        const VArray<float3> &vectors,
                                        const VArray<float> &factors,
                                        const float3 local_main_axis,
                                        const float3 local_pivot_axis,
                                        MutableSpan<float3> output_rotations)
{
  mask.foreach_index([&](const int64_t i) {
    if (local_main_axis == local_pivot_axis) {
      /* Can't compute any meaningful rotation angle in this case. */
      output_rotations[i] = input_rotations[i];
      return;
    }

    const float3 vector = vectors[i];
    if (math::is_zero(vector)) {
      output_rotations[i] = input_rotations[i];
      return;
    }

    float old_rotation[3][3];
    eul_to_mat3(old_rotation, input_rotations[i]);
    float3 old_axis;
    mul_v3_m3v3(old_axis, old_rotation, local_main_axis);
    float3 pivot_axis;
    mul_v3_m3v3(pivot_axis, old_rotation, local_pivot_axis);

    float full_angle = angle_signed_on_axis_v3v3_v3(vector, old_axis, pivot_axis);
    if (full_angle > M_PI) {
      /* Make sure the point is rotated as little as possible. */
      full_angle -= 2.0f * M_PI;
    }
    const float angle = factors[i] * full_angle;

    float rotation[3][3];
    axis_angle_to_mat3(rotation, pivot_axis, angle);

    float new_rotation_matrix[3][3];
    mul_m3_m3m3(new_rotation_matrix, rotation, old_rotation);

    float3 new_rotation;
    mat3_to_eul(new_rotation, new_rotation_matrix);

    output_rotations[i] = new_rotation;
  });
}

class MF_AlignEulerToVector : public mf::MultiFunction {
 private:
  int main_axis_mode_;
  int pivot_axis_mode_;

 public:
  MF_AlignEulerToVector(int main_axis_mode, int pivot_axis_mode)
      : main_axis_mode_(main_axis_mode), pivot_axis_mode_(pivot_axis_mode)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Align Euler to Vector", signature};
      builder.single_input<float3>("Rotation");
      builder.single_input<float>("Factor");
      builder.single_input<float3>("Vector");
      builder.single_output<float3>("Rotation");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &input_rotations = params.readonly_single_input<float3>(0, "Rotation");
    const VArray<float> &factors = params.readonly_single_input<float>(1, "Factor");
    const VArray<float3> &vectors = params.readonly_single_input<float3>(2, "Vector");

    auto output_rotations = params.uninitialized_single_output<float3>(3, "Rotation");

    float3 local_main_axis = {0.0f, 0.0f, 0.0f};
    local_main_axis[main_axis_mode_] = 1;

    if (pivot_axis_mode_ == FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_AUTO) {
      align_rotations_auto_pivot(
          mask, input_rotations, vectors, factors, local_main_axis, output_rotations);
    }
    else {
      float3 local_pivot_axis = {0.0f, 0.0f, 0.0f};
      local_pivot_axis[pivot_axis_mode_ - 1] = 1;
      align_rotations_fixed_pivot(mask,
                                  input_rotations,
                                  vectors,
                                  factors,
                                  local_main_axis,
                                  local_pivot_axis,
                                  output_rotations);
    }
  }

  ExecutionHints get_execution_hints() const override
  {
    ExecutionHints hints;
    hints.min_grain_size = 512;
    return hints;
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  builder.construct_and_set_matching_fn<MF_AlignEulerToVector>(node.custom1, node.custom2);
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem axis_items[] = {
      {FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_X,
       "X",
       ICON_NONE,
       "X",
       "Align the X axis with the vector"},
      {FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_Y,
       "Y",
       ICON_NONE,
       "Y",
       "Align the Y axis with the vector"},
      {FN_NODE_ALIGN_EULER_TO_VECTOR_AXIS_Z,
       "Z",
       ICON_NONE,
       "Z",
       "Align the Z axis with the vector"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem pivot_axis_items[] = {
      {FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_AUTO,
       "AUTO",
       ICON_NONE,
       "Auto",
       "Automatically detect the best rotation axis to rotate towards the vector"},
      {FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_X,
       "X",
       ICON_NONE,
       "X",
       "Rotate around the local X axis"},
      {FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_Y,
       "Y",
       ICON_NONE,
       "Y",
       "Rotate around the local Y axis"},
      {FN_NODE_ALIGN_EULER_TO_VECTOR_PIVOT_AXIS_Z,
       "Z",
       ICON_NONE,
       "Z",
       "Rotate around the local Z axis"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "axis",
                    "Axis",
                    "Axis to align to the vector",
                    axis_items,
                    NOD_inline_enum_accessors(custom1),
                    std::nullopt,
                    nullptr,
                    true);

  RNA_def_node_enum(srna,
                    "pivot_axis",
                    "Pivot Axis",
                    "Axis to rotate around",
                    pivot_axis_items,
                    NOD_inline_enum_accessors(custom2),
                    std::nullopt,
                    nullptr,
                    true);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeAlignEulerToVector", FN_NODE_ALIGN_EULER_TO_VECTOR);
  ntype.ui_name = "Align Euler to Vector";
  ntype.ui_description = "Orient an Euler rotation along the given direction";
  ntype.enum_name_legacy = "ALIGN_EULER_TO_VECTOR";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.build_multi_function = node_build_multi_function;
  ntype.deprecation_notice = N_("Use the \"Align Rotation to Vector\" node instead");
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_align_euler_to_vector_cc
