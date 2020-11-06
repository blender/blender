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

#include "BKE_node.h"

#include "NOD_simulation.h"

#include "NOD_common.h"
#include "node_common.h"
#include "node_simulation_util.h"

void register_node_type_sim_group(void)
{
  static bNodeType ntype;

  node_type_base_custom(&ntype, "SimulationNodeGroup", "Group", 0, 0);
  ntype.type = NODE_GROUP;
  ntype.poll = sim_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.update_internal_links = node_update_internal_links_default;
  ntype.rna_ext.srna = RNA_struct_find("SimulationNodeGroup");
  BLI_assert(ntype.rna_ext.srna != nullptr);
  RNA_struct_blender_type_set(ntype.rna_ext.srna, &ntype);

  node_type_socket_templates(&ntype, nullptr, nullptr);
  node_type_size(&ntype, 140, 60, 400);
  node_type_label(&ntype, node_group_label);
  node_type_group_update(&ntype, node_group_update);

  nodeRegisterType(&ntype);
}
