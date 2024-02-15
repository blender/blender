/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BKE_scene.h"

namespace blender::nodes::node_shader_output_material_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>("Surface");
  b.add_input<decl::Shader>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
  b.add_input<decl::Vector>("Displacement").hide_value();
  b.add_input<decl::Float>("Thickness")
      .hide_value()
      .unavailable() /* EEVEE-Next only. Does nothing in 4.1. */;
}

static int node_shader_gpu_output_material(GPUMaterial *mat,
                                           bNode * /*node*/,
                                           bNodeExecData * /*execdata*/,
                                           GPUNodeStack *in,
                                           GPUNodeStack * /*out*/)
{
  GPUNodeLink *outlink_surface, *outlink_volume, *outlink_displacement, *outlink_thickness;
  /* Passthrough node in order to do the right socket conversions (important for displacement). */
  if (in[0].link) {
    GPU_link(mat, "node_output_material_surface", in[0].link, &outlink_surface);
    GPU_material_output_surface(mat, outlink_surface);
  }
  if (in[1].link) {
    GPU_link(mat, "node_output_material_volume", in[1].link, &outlink_volume);
    GPU_material_output_volume(mat, outlink_volume);
  }
  if (in[2].link) {
    GPU_link(mat, "node_output_material_displacement", in[2].link, &outlink_displacement);
    GPU_material_output_displacement(mat, outlink_displacement);
  }
  if (in[3].link) {
    GPU_link(mat, "node_output_material_thickness", in[3].link, &outlink_thickness);
    GPU_material_output_thickness(mat, outlink_thickness);
  }
  return true;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem surface = get_input_link("Surface", NodeItem::Type::SurfaceShader);
  if (!surface) {
    NodeItem bsdf = get_input_link("Surface", NodeItem::Type::BSDF);
    NodeItem edf = get_input_link("Surface", NodeItem::Type::EDF);
    if (bsdf || edf) {
      NodeItem opacity = get_input_link("Surface", NodeItem::Type::SurfaceOpacity);
      surface = create_node("surface",
                            NodeItem::Type::SurfaceShader,
                            {{"bsdf", bsdf}, {"edf", edf}, {"opacity", opacity}});
    }
  }
  return create_node("surfacematerial", NodeItem::Type::Material, {{"surfaceshader", surface}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_output_material_cc

/* node type definition */
void register_node_type_sh_output_material()
{
  namespace file_ns = blender::nodes::node_shader_output_material_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OUTPUT_MATERIAL, "Material Output", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_gpu_output_material;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}
