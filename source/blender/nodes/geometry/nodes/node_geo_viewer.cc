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

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_viewer_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

void register_node_type_geo_viewer()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_VIEWER, "Viewer", NODE_CLASS_OUTPUT, 0);
  node_type_socket_templates(&ntype, geo_node_viewer_in, nullptr);
  nodeRegisterType(&ntype);
}
