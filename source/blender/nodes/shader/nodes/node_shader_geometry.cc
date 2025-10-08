/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_geometry_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Position");
  b.add_output<decl::Vector>("Normal");
  b.add_output<decl::Vector>("Tangent");
  b.add_output<decl::Vector>("True Normal");
  b.add_output<decl::Vector>("Incoming");
  b.add_output<decl::Vector>("Parametric");
  b.add_output<decl::Float>("Backfacing");
  b.add_output<decl::Float>("Pointiness");
  b.add_output<decl::Float>("Random Per Island");
}

static int node_shader_gpu_geometry(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData * /*execdata*/,
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  /* HACK: Don't request GPU_MATFLAG_BARYCENTRIC if not used because it will
   * trigger the use of geometry shader (and the performance penalty it implies). */
  if (out[5].hasoutput) {
    GPU_material_flag_set(mat, GPU_MATFLAG_BARYCENTRIC);
  }
  /* Optimization: don't request orco if not needed. */
  const float val[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GPUNodeLink *orco_link = out[2].hasoutput ? GPU_attribute(mat, CD_ORCO, "") : GPU_constant(val);

  const bool success = GPU_stack_link(mat, node, "node_geometry", in, out, orco_link);

  int i;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->outputs, i) {
    node_shader_gpu_bump_tex_coord(mat, node, &out[i].link);
    /* Normalize some vectors after dFdx/dFdy offsets.
     * This is the case for interpolated, non linear functions.
     * The resulting vector can still be a bit wrong but not as much.
     * (see #70644) */
    if (ELEM(i, 1, 2, 4)) {
      GPU_link(mat,
               "vector_math_normalize",
               out[i].link,
               out[i].link,
               out[i].link,
               out[i].link,
               &out[i].link,
               nullptr);
    }
  }

  return success;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: Some outputs aren't supported by MaterialX. */
  NodeItem res = empty();
  std::string name = socket_out_->identifier;

  if (name == "Position") {
    res = create_node("position", NodeItem::Type::Vector3, {{"space", val(std::string("world"))}});
  }
  else if (name == "Normal") {
    res = create_node("normal", NodeItem::Type::Vector3, {{"space", val(std::string("world"))}});
  }
  else if (ELEM(name, "Tangent", "True Normal")) {
    res = create_node("tangent", NodeItem::Type::Vector3, {{"space", val(std::string("world"))}});
  }
  else {
    res = get_output_default(name, NodeItem::Type::Any);
  }
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_geometry_cc

/* node type definition */
void register_node_type_sh_geometry()
{
  namespace file_ns = blender::nodes::node_shader_geometry_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeNewGeometry", SH_NODE_NEW_GEOMETRY);
  ntype.ui_name = "Geometry";
  ntype.ui_description = "Retrieve geometric information about the current shading point";
  ntype.enum_name_legacy = "NEW_GEOMETRY";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_geometry;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
