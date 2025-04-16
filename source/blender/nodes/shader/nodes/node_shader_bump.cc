/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

/* **************** BUMP ******************** */

namespace blender::nodes::node_shader_bump_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
#define SOCK_STRENGTH_ID 0
  b.add_input<decl::Float>("Strength")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Strength of the bump mapping effect, interpolating between "
          "no bump mapping and full bump mapping")
      .translation_context(BLT_I18NCONTEXT_AMOUNT);
#define SOCK_DISTANCE_ID 1
  b.add_input<decl::Float>("Distance")
      .default_value(1.0f)
      .min(0.0f)
      .max(1000.0f)
      .description(
          "Multiplier for the height value to control the overall distance for bump mapping");
#define SOCK_FILTER_WIDTH_ID 2
  b.add_input<decl::Float>("Filter Width")
      .default_value(0.1f)
      .min(0.001)
      .max(10.0f)
      .subtype(PROP_PIXEL)
      .description(
          "Filter width in pixels, used to compute the bump mapping direction. For most textures "
          "the default value of 0.1 enables subpixel filtering for stable results. For stepwise "
          "textures a larger filter width can be used to get a bevel like effect on the edges");
#define SOCK_HEIGHT_ID 3
  b.add_input<decl::Float>("Height")
      .default_value(1.0f)
      .min(-1000.0f)
      .max(1000.0f)
      .hide_value()
      .description("Height above surface. Connect the height map texture to this input");
#define SOCK_NORMAL_ID 4
  b.add_input<decl::Vector>("Normal").min(-1.0f).max(1.0f).hide_value();
  b.add_output<decl::Vector>("Normal");
}

static void node_shader_buts_bump(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "invert", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

static int gpu_shader_bump(GPUMaterial *mat,
                           bNode *node,
                           bNodeExecData * /*execdata*/,
                           GPUNodeStack *in,
                           GPUNodeStack *out)
{
  /* If there is no Height input, the node becomes a no-op. */
  if (!in[SOCK_HEIGHT_ID].link) {
    if (!in[SOCK_NORMAL_ID].link) {
      return GPU_link(mat, "world_normals_get", &out[0].link);
    }
    /* Actually running the bump code would normalize, but Cycles handles it as total no-op. */
    return GPU_link(mat, "vector_copy", in[SOCK_NORMAL_ID].link, &out[0].link);
  }

  if (!in[SOCK_NORMAL_ID].link) {
    GPU_link(mat, "world_normals_get", &in[SOCK_NORMAL_ID].link);
  }

  const float filter_width = in[SOCK_FILTER_WIDTH_ID].vec[0];
  const char *height_function = GPU_material_split_sub_function(
      mat, GPU_FLOAT, &in[SOCK_HEIGHT_ID].link);

  /* TODO (Miguel Pozo):
   * Currently, this doesn't compute the actual differentials, just the height at dX and dY
   * offsets. The actual differentials are computed inside the GLSL node_bump function by
   * subtracting the height input. This avoids redundant computations when the height input is
   * also needed by regular nodes as part in the main function (See #103903 for context).
   * A better option would be to add a "value" input socket (in this case the height) to the
   * differentiate node, but currently this kind of intermediate nodes are pruned in the
   * code generation process (see #104265), so we need to fix that first. */
  GPUNodeLink *dheight = GPU_differentiate_float_function(height_function, filter_width);

  float invert = (node->custom1) ? -1.0 : 1.0;

  return GPU_stack_link(mat, node, "node_bump", in, out, dheight, GPU_constant(&invert));
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem height = get_input_link("Height", NodeItem::Type::Float);
  NodeItem normal = get_input_link("Normal", NodeItem::Type::Vector3);

  if (!height) {
    if (!normal) {
      return create_node(
          "normal", NodeItem::Type::Vector3, {{"space", val(std::string("world"))}});
    }
    return normal;
  }

  NodeItem strength = get_input_value("Strength", NodeItem::Type::Float);
  NodeItem distance = get_input_value("Distance", NodeItem::Type::Float);
  NodeItem height_normal = create_node(
      "heighttonormal", NodeItem::Type::Vector3, {{"in", height}, {"scale", strength}});

  return create_node("normalmap",
                     NodeItem::Type::Vector3,
                     {{"in", height_normal},
                      {"scale", node_->custom1 ? distance * val(-1.0f) : distance},
                      {"normal", normal}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_bump_cc

/* node type definition */
void register_node_type_sh_bump()
{
  namespace file_ns = blender::nodes::node_shader_bump_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBump", SH_NODE_BUMP);
  ntype.ui_name = "Bump";
  ntype.ui_description =
      "Generate a perturbed normal from a height texture for bump mapping. Typically used for "
      "faking highly detailed surfaces";
  ntype.enum_name_legacy = "BUMP";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_bump;
  ntype.gpu_fn = file_ns::gpu_shader_bump;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(&ntype);
}
