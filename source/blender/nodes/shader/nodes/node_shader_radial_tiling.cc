/* SPDX-FileCopyrightText: 2024-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLI_radial_tiling.hh"

namespace blender::nodes::node_shader_radial_tiling_cc {

NODE_STORAGE_FUNCS(NodeRadialTiling)

static void sh_node_radial_tiling_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();

  b.add_output<decl::Vector>("Segment Coordinates")
      .no_muted_links()
      .description("Segment coordinates for texture mapping within each angular segment");
  b.add_output<decl::Float>("Segment ID")
      .no_muted_links()
      .description(
          "Unique ID for every angular segment starting at 0 and increasing counterclockwise by "
          "1");
  b.add_output<decl::Float>("Segment Width")
      .no_muted_links()
      .description(
          "Relative width of each angular segment. "
          "May be used to scale textures to fit into each segment");
  b.add_output<decl::Float>("Segment Rotation")
      .no_muted_links()
      .description(
          "Counterclockwise rotation of each segment coordinates system. May be used to align the "
          "rotation of the textures of each segment");

  b.add_input<decl::Vector>("Vector")
      .dimensions(2)
      .default_value(float3{0.0f, 0.0f, 0.0f})
      .description("Input texture coordinates");
  b.add_input<decl::Float>("Sides").min(2.0f).max(1000.0f).default_value(5.0f).description(
      "Number of angular segments for tiling. A non-integer value results in an irregular "
      "segment");
  b.add_input<decl::Float>("Roundness")
      .min(0.0f)
      .max(1.0f)
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .description("Roundness of the segment coordinates systems");
}

static void node_shader_buts_radial_tiling(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "normalize", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

static void node_shader_init_radial_tiling(bNodeTree * /*ntree*/, bNode *node)
{
  NodeRadialTiling *storage = MEM_callocN<NodeRadialTiling>(__func__);
  storage->normalize = false;

  node->storage = storage;
}

static const char *gpu_shader_get_name()
{
  return "node_radial_tiling";
}

static int node_shader_gpu_radial_tiling(GPUMaterial *mat,
                                         bNode *node,
                                         bNodeExecData * /*execdata*/,
                                         GPUNodeStack *in,
                                         GPUNodeStack *out)
{
  const NodeRadialTiling &storage = node_storage(*node);
  float normalize_r_gon_parameter = storage.normalize;
  float calculate_r_gon_parameter_field = out[0].hasoutput;
  float calculate_segment_id = out[1].hasoutput;
  float calculate_max_unit_parameter = out[2].hasoutput;
  float calculate_x_axis_A_angle_bisector = out[3].hasoutput;

  const char *name = gpu_shader_get_name();

  return GPU_stack_link(mat,
                        node,
                        name,
                        in,
                        out,
                        GPU_constant(&normalize_r_gon_parameter),
                        GPU_constant(&calculate_r_gon_parameter_field),
                        GPU_constant(&calculate_segment_id),
                        GPU_constant(&calculate_max_unit_parameter),
                        GPU_constant(&calculate_x_axis_A_angle_bisector));
}

class RoundedPolygonFunction : public mf::MultiFunction {
 private:
  bool normalize_r_gon_parameter_;

  mf::Signature signature_;

 public:
  RoundedPolygonFunction(bool normalize_r_gon_parameter)
      : normalize_r_gon_parameter_(normalize_r_gon_parameter)
  {
    signature_ = create_signature();
    this->set_signature(&signature_);
  }

  static mf::Signature create_signature()
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"radial_tiling", signature};

    builder.single_input<float3>("Vector");

    builder.single_input<float>("Sides");
    builder.single_input<float>("Roundness");

    builder.single_output<float3>("Segment Coordinates", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<float>("Segment ID", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<float>("Segment Width", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<float>("Segment Rotation", mf::ParamFlag::SupportsUnusedOutput);

    return signature;
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    int param = 0;

    const VArray<float3> &coord = params.readonly_single_input<float3>(param++, "Vector");

    const VArray<float> &r_gon_sides = params.readonly_single_input<float>(param++, "Sides");
    const VArray<float> &r_gon_roundness = params.readonly_single_input<float>(param++,
                                                                               "Roundness");

    MutableSpan<float3> r_segment_coordinates =
        params.uninitialized_single_output_if_required<float3>(param++, "Segment Coordinates");
    MutableSpan<float> r_segment_id = params.uninitialized_single_output_if_required<float>(
        param++, "Segment ID");
    MutableSpan<float> r_max_unit_parameter =
        params.uninitialized_single_output_if_required<float>(param++, "Segment Width");
    MutableSpan<float> r_x_axis_A_angle_bisector =
        params.uninitialized_single_output_if_required<float>(param++, "Segment Rotation");

    const bool calculate_r_gon_parameter_field = !r_segment_coordinates.is_empty();
    const bool calculate_segment_id = !r_segment_id.is_empty();
    const bool calculate_max_unit_parameter = !r_max_unit_parameter.is_empty();
    const bool calculate_x_axis_A_angle_bisector = !r_x_axis_A_angle_bisector.is_empty();

    mask.foreach_index([&](const int64_t i) {
      if (calculate_r_gon_parameter_field || calculate_max_unit_parameter ||
          calculate_x_axis_A_angle_bisector)
      {
        float4 out_variables = calculate_out_variables(calculate_r_gon_parameter_field,
                                                       calculate_max_unit_parameter,
                                                       normalize_r_gon_parameter_,
                                                       math::max(r_gon_sides[i], 2.0f),
                                                       math::clamp(r_gon_roundness[i], 0.0f, 1.0f),
                                                       float2(coord[i].x, coord[i].y));

        if (calculate_r_gon_parameter_field) {
          r_segment_coordinates[i] = float3(out_variables.y, out_variables.x, 0.0f);
        }
        if (calculate_max_unit_parameter) {
          r_max_unit_parameter[i] = out_variables.z;
        }
        if (calculate_x_axis_A_angle_bisector) {
          r_x_axis_A_angle_bisector[i] = out_variables.w;
        }
      }

      if (calculate_segment_id) {
        r_segment_id[i] = calculate_out_segment_id(math::max(r_gon_sides[i], 2.0f),
                                                   float2(coord[i].x, coord[i].y));
      }
    });
  }

  ExecutionHints get_execution_hints() const override
  {
    ExecutionHints hints;
    hints.allocates_array = false;
    hints.min_grain_size = 50;
    return hints;
  }
};

static void sh_node_radial_tiling_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeRadialTiling &storage = node_storage(builder.node());
  builder.construct_and_set_matching_fn<RoundedPolygonFunction>(storage.normalize);
}

}  // namespace blender::nodes::node_shader_radial_tiling_cc

void register_node_type_sh_radial_tiling()
{
  namespace file_ns = blender::nodes::node_shader_radial_tiling_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeRadialTiling");
  ntype.ui_name = "Radial Tiling";
  ntype.ui_description = "Transform Coordinate System for Radial Tiling";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::sh_node_radial_tiling_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_radial_tiling;
  ntype.initfunc = file_ns::node_shader_init_radial_tiling;
  blender::bke::node_type_storage(
      ntype, "NodeRadialTiling", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_radial_tiling;
  ntype.build_multi_function = file_ns::sh_node_radial_tiling_build_multi_function;

  blender::bke::node_register_type(ntype);
}
