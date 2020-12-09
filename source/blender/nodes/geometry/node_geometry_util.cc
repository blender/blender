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
#include "node_util.h"

namespace blender::nodes {

void update_attribute_input_socket_availabilities(bNode &node,
                                                  const StringRef name,
                                                  const GeometryNodeAttributeInputMode mode)
{
  const GeometryNodeAttributeInputMode mode_ = (GeometryNodeAttributeInputMode)mode;
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (name == socket->name) {
      const bool is_available =
          ((socket->type == SOCK_STRING && mode_ == GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE) ||
           (socket->type == SOCK_FLOAT && mode_ == GEO_NODE_ATTRIBUTE_INPUT_FLOAT) ||
           (socket->type == SOCK_VECTOR && mode_ == GEO_NODE_ATTRIBUTE_INPUT_VECTOR) ||
           (socket->type == SOCK_RGBA && mode_ == GEO_NODE_ATTRIBUTE_INPUT_COLOR));
      nodeSetSocketAvailability(socket, is_available);
    }
  }
}

}  // namespace blender::nodes

bool geo_node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *ntree)
{
  return STREQ(ntree->idname, "GeometryNodeTree");
}

void geo_node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
  node_type_base(ntype, type, name, nclass, flag);
  ntype->poll = geo_node_poll_default;
  ntype->update_internal_links = node_update_internal_links_default;
  ntype->insert_link = node_insert_link_default;
}
