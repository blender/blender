/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_combine_matrix_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();

  b.add_output<decl::Matrix>("Matrix");

  PanelDeclarationBuilder &column_a = b.add_panel("Column 1").default_closed(true);
  column_a.add_input<decl::Float>("Column 1 Row 1").default_value(1.0f);
  column_a.add_input<decl::Float>("Column 1 Row 2");
  column_a.add_input<decl::Float>("Column 1 Row 3");
  column_a.add_input<decl::Float>("Column 1 Row 4");

  PanelDeclarationBuilder &column_b = b.add_panel("Column 2").default_closed(true);
  column_b.add_input<decl::Float>("Column 2 Row 1");
  column_b.add_input<decl::Float>("Column 2 Row 2").default_value(1.0f);
  column_b.add_input<decl::Float>("Column 2 Row 3");
  column_b.add_input<decl::Float>("Column 2 Row 4");

  PanelDeclarationBuilder &column_c = b.add_panel("Column 3").default_closed(true);
  column_c.add_input<decl::Float>("Column 3 Row 1");
  column_c.add_input<decl::Float>("Column 3 Row 2");
  column_c.add_input<decl::Float>("Column 3 Row 3").default_value(1.0f);
  column_c.add_input<decl::Float>("Column 3 Row 4");

  PanelDeclarationBuilder &column_d = b.add_panel("Column 4").default_closed(true);
  column_d.add_input<decl::Float>("Column 4 Row 1");
  column_d.add_input<decl::Float>("Column 4 Row 2");
  column_d.add_input<decl::Float>("Column 4 Row 3");
  column_d.add_input<decl::Float>("Column 4 Row 4").default_value(1.0f);
}

static void copy_with_stride(const IndexMask &mask,
                             const VArray<float> &src,
                             const int64_t src_step,
                             const int64_t src_begin,
                             const int64_t dst_step,
                             const int64_t dst_begin,
                             MutableSpan<float> dst)
{
  BLI_assert(src_begin < src_step);
  BLI_assert(dst_begin < dst_step);
  devirtualize_varray(src, [&](const auto src) {
    mask.foreach_index_optimized<int>([&](const int64_t index) {
      dst[dst_begin + dst_step * index] = src[src_begin + src_step * index];
    });
  });
}

class CombineMatrixFunction : public mf::MultiFunction {
 public:
  CombineMatrixFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Combine Matrix", signature};
      builder.single_input<float>("Column 1 Row 1");
      builder.single_input<float>("Column 1 Row 2");
      builder.single_input<float>("Column 1 Row 3");
      builder.single_input<float>("Column 1 Row 4");

      builder.single_input<float>("Column 2 Row 1");
      builder.single_input<float>("Column 2 Row 2");
      builder.single_input<float>("Column 2 Row 3");
      builder.single_input<float>("Column 2 Row 4");

      builder.single_input<float>("Column 3 Row 1");
      builder.single_input<float>("Column 3 Row 2");
      builder.single_input<float>("Column 3 Row 3");
      builder.single_input<float>("Column 3 Row 4");

      builder.single_input<float>("Column 4 Row 1");
      builder.single_input<float>("Column 4 Row 2");
      builder.single_input<float>("Column 4 Row 3");
      builder.single_input<float>("Column 4 Row 4");

      builder.single_output<float4x4>("Matrix");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float> &column_1_row_1 = params.readonly_single_input<float>(0, "Column 1 Row 1");
    const VArray<float> &column_1_row_2 = params.readonly_single_input<float>(1, "Column 1 Row 2");
    const VArray<float> &column_1_row_3 = params.readonly_single_input<float>(2, "Column 1 Row 3");
    const VArray<float> &column_1_row_4 = params.readonly_single_input<float>(3, "Column 1 Row 4");

    const VArray<float> &column_2_row_1 = params.readonly_single_input<float>(4, "Column 2 Row 1");
    const VArray<float> &column_2_row_2 = params.readonly_single_input<float>(5, "Column 2 Row 2");
    const VArray<float> &column_2_row_3 = params.readonly_single_input<float>(6, "Column 2 Row 3");
    const VArray<float> &column_2_row_4 = params.readonly_single_input<float>(7, "Column 2 Row 4");

    const VArray<float> &column_3_row_1 = params.readonly_single_input<float>(8, "Column 3 Row 1");
    const VArray<float> &column_3_row_2 = params.readonly_single_input<float>(9, "Column 3 Row 2");
    const VArray<float> &column_3_row_3 = params.readonly_single_input<float>(10,
                                                                              "Column 3 Row 3");
    const VArray<float> &column_3_row_4 = params.readonly_single_input<float>(11,
                                                                              "Column 3 Row 4");

    const VArray<float> &column_4_row_1 = params.readonly_single_input<float>(12,
                                                                              "Column 4 Row 1");
    const VArray<float> &column_4_row_2 = params.readonly_single_input<float>(13,
                                                                              "Column 4 Row 2");
    const VArray<float> &column_4_row_3 = params.readonly_single_input<float>(14,
                                                                              "Column 4 Row 3");
    const VArray<float> &column_4_row_4 = params.readonly_single_input<float>(15,
                                                                              "Column 4 Row 4");

    MutableSpan<float4x4> matrices = params.uninitialized_single_output<float4x4>(16, "Matrix");
    MutableSpan<float> components = matrices.cast<float>();

    copy_with_stride(mask, column_1_row_1, 1, 0, 16, 0, components);
    copy_with_stride(mask, column_1_row_2, 1, 0, 16, 1, components);
    copy_with_stride(mask, column_1_row_3, 1, 0, 16, 2, components);
    copy_with_stride(mask, column_1_row_4, 1, 0, 16, 3, components);

    copy_with_stride(mask, column_2_row_1, 1, 0, 16, 4, components);
    copy_with_stride(mask, column_2_row_2, 1, 0, 16, 5, components);
    copy_with_stride(mask, column_2_row_3, 1, 0, 16, 6, components);
    copy_with_stride(mask, column_2_row_4, 1, 0, 16, 7, components);

    copy_with_stride(mask, column_3_row_1, 1, 0, 16, 8, components);
    copy_with_stride(mask, column_3_row_2, 1, 0, 16, 9, components);
    copy_with_stride(mask, column_3_row_3, 1, 0, 16, 10, components);
    copy_with_stride(mask, column_3_row_4, 1, 0, 16, 11, components);

    copy_with_stride(mask, column_4_row_1, 1, 0, 16, 12, components);
    copy_with_stride(mask, column_4_row_2, 1, 0, 16, 13, components);
    copy_with_stride(mask, column_4_row_3, 1, 0, 16, 14, components);
    copy_with_stride(mask, column_4_row_4, 1, 0, 16, 15, components);
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const static CombineMatrixFunction fn;
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_COMBINE_MATRIX, "Combine Matrix", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_combine_matrix_cc
