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
/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

namespace blender::nodes {

static void cmp_node_scene_time_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Seconds"));
  b.add_output<decl::Float>(N_("Frame"));
}

}  // namespace blender::nodes

void register_node_type_cmp_scene_time()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SCENE_TIME, "Scene Time", NODE_CLASS_INPUT);
  ntype.declare = blender::nodes::cmp_node_scene_time_declare;
  nodeRegisterType(&ntype);
}
