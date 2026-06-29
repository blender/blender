/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "node_geometry_util.hh"
#include "shader/node_shader_util.hh"

#include "node_util.hh"

#include "RNA_access.hh"

namespace blender::nodes::node_geo_input_scene_time_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Seconds"_ustr);
  b.add_output<decl::Float>("Frame"_ustr);
}

static void node_exec(GeoNodeExecParams params)
{
  const Scene *scene = DEG_get_input_scene(params.depsgraph());
  const float scene_ctime = BKE_scene_ctime_get(scene);
  const double frame_rate = double(scene->r.frs_sec) / double(scene->r.frs_sec_base);
  params.set_output("Seconds"_ustr, float(scene_ctime / frame_rate));
  params.set_output("Frame"_ustr, scene_ctime);
}

static int node_shader_gpu(GPUMaterial *mat,
                           bNode *node,
                           bNodeExecData * /*execdata*/,
                           GPUNodeStack *in,
                           GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_SCENE_TIME);
  GPU_stack_link(mat, node, "node_scene_time", in, out);
  return 1;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  return get_output_default(socket_out_->identifier, NodeItem::Type::Any);
}
#endif
NODE_SHADER_MATERIALX_END

static void node_register()
{
  static bke::bNodeType ntype;
  sh_geo_node_type_base(&ntype, "GeometryNodeInputSceneTime"_ustr, GEO_NODE_INPUT_SCENE_TIME);
  ntype.ui_name = "Scene Time";
  ntype.ui_description =
      "Retrieve the current time in the scene's animation in units of seconds or frames";
  ntype.enum_name_legacy = "INPUT_SCENE_TIME";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_exec;
  ntype.declare = node_declare;
  ntype.gpu_fn = node_shader_gpu;
  ntype.materialx_fn = node_shader_materialx;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_scene_time_cc
