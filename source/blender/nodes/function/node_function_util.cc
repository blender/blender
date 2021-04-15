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

#include "node_function_util.hh"
#include "node_util.h"

static bool fn_node_poll_default(bNodeType *UNUSED(ntype),
                                 bNodeTree *ntree,
                                 const char **r_disabled_hint)
{
  /* Function nodes are only supported in simulation node trees so far. */
  if (!STREQ(ntree->idname, "GeometryNodeTree")) {
    *r_disabled_hint = "Not a geometry node tree";
    return false;
  }
  return true;
}

void fn_node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
  node_type_base(ntype, type, name, nclass, flag);
  ntype->poll = fn_node_poll_default;
  ntype->update_internal_links = node_update_internal_links_default;
  ntype->insert_link = node_insert_link_default;
}
