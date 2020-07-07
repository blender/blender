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

#include "BLI_listbase.h"
#include "node_function_util.hh"

static bNodeSocketTemplate fn_node_switch_in[] = {
    {SOCK_BOOLEAN, N_("Switch")},

    {SOCK_FLOAT, N_("If False"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_INT, N_("If False"), 0, 0, 0, 0, -10000, 10000},
    {SOCK_BOOLEAN, N_("If False")},
    {SOCK_VECTOR, N_("If False"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_STRING, N_("If False")},
    {SOCK_RGBA, N_("If False"), 0.8f, 0.8f, 0.8f, 1.0f},
    {SOCK_OBJECT, N_("If False")},
    {SOCK_IMAGE, N_("If False")},

    {SOCK_FLOAT, N_("If True"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_INT, N_("If True"), 0, 0, 0, 0, -10000, 10000},
    {SOCK_BOOLEAN, N_("If True")},
    {SOCK_VECTOR, N_("If True"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_STRING, N_("If True")},
    {SOCK_RGBA, N_("If True"), 0.8f, 0.8f, 0.8f, 1.0f},
    {SOCK_OBJECT, N_("If True")},
    {SOCK_IMAGE, N_("If True")},

    {-1, ""},
};

static bNodeSocketTemplate fn_node_switch_out[] = {
    {SOCK_FLOAT, N_("Result")},
    {SOCK_INT, N_("Result")},
    {SOCK_BOOLEAN, N_("Result")},
    {SOCK_VECTOR, N_("Result")},
    {SOCK_STRING, N_("Result")},
    {SOCK_RGBA, N_("Result")},
    {SOCK_OBJECT, N_("Result")},
    {SOCK_IMAGE, N_("Result")},
    {-1, ""},
};

static void fn_node_switch_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  int index = 0;
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    nodeSetSocketAvailability(sock, index == 0 || sock->type == node->custom1);
    index++;
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    nodeSetSocketAvailability(sock, sock->type == node->custom1);
  }
}

void register_node_type_fn_switch()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SWITCH, "Switch", 0, 0);
  node_type_socket_templates(&ntype, fn_node_switch_in, fn_node_switch_out);
  node_type_update(&ntype, fn_node_switch_update);
  nodeRegisterType(&ntype);
}
