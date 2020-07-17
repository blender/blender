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

static bNodeSocketTemplate fn_node_combine_strings_in[] = {
    {SOCK_STRING, N_("A")},
    {SOCK_STRING, N_("B")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_combine_strings_out[] = {
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

static void fn_node_combine_strings_expand_in_mf_network(
    blender::nodes::NodeMFNetworkBuilder &builder)
{
  static blender::fn::CustomMF_SI_SI_SO<std::string, std::string, std::string> combine_fn{
      "Combine Strings", [](const std::string &a, const std::string &b) { return a + b; }};
  builder.set_matching_fn(combine_fn);
}

void register_node_type_fn_combine_strings()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_COMBINE_STRINGS, "Combine Strings", 0, 0);
  node_type_socket_templates(&ntype, fn_node_combine_strings_in, fn_node_combine_strings_out);
  ntype.expand_in_mf_network = fn_node_combine_strings_expand_in_mf_network;
  nodeRegisterType(&ntype);
}
