/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_scene_time_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Seconds"));
  b.add_output<decl::Float>(N_("Frame"));
}

static void node_exec(GeoNodeExecParams params)
{
  const Scene *scene = DEG_get_input_scene(params.depsgraph());
  const float scene_ctime = BKE_scene_ctime_get(scene);
  const double frame_rate = (((double)scene->r.frs_sec) / (double)scene->r.frs_sec_base);
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
