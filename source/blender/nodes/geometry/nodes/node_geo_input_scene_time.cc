/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_scene_time_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Seconds");
  b.add_output<decl::Float>("Frame");
}

static void node_exec(GeoNodeExecParams params)
{
  const Scene *scene = DEG_get_input_scene(params.depsgraph());
  const float scene_ctime = BKE_scene_ctime_get(scene);
  const double frame_rate = (double(scene->r.frs_sec) / double(scene->r.frs_sec_base));
  params.set_output("Seconds", float(scene_ctime / frame_rate));
  params.set_output("Frame", scene_ctime);
}

}  // namespace blender::nodes::node_geo_input_scene_time_cc

void register_node_type_geo_input_scene_time()
{
  static bNodeType ntype;
  namespace file_ns = blender::nodes::node_geo_input_scene_time_cc;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_SCENE_TIME, "Scene Time", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
