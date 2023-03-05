/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_register.hh"
#include "NOD_socket.h"

#include "BKE_node.h"

#include "ED_node.hh"

#include "BLT_translation.h"

#include "RNA_access.h"

static bool node_undefined_poll(const bNodeType * /*ntype*/,
                                const bNodeTree * /*nodetree*/,
                                const char ** /*r_disabled_hint*/)
{
  /* this type can not be added deliberately, it's just a placeholder */
  return false;
}

/* register fallback types used for undefined tree, nodes, sockets */
static void register_undefined_types()
{
  /* NOTE: these types are not registered in the type hashes,
   * they are just used as placeholders in case the actual types are not registered.
   */

  NodeTreeTypeUndefined.type = NTREE_UNDEFINED;
  strcpy(NodeTreeTypeUndefined.idname, "NodeTreeUndefined");
  strcpy(NodeTreeTypeUndefined.ui_name, N_("Undefined"));
  strcpy(NodeTreeTypeUndefined.ui_description, N_("Undefined Node Tree Type"));

  node_type_base_custom(&NodeTypeUndefined, "NodeUndefined", "Undefined", 0);
  NodeTypeUndefined.poll = node_undefined_poll;

  BLI_strncpy(NodeSocketTypeUndefined.idname,
              "NodeSocketUndefined",
              sizeof(NodeSocketTypeUndefined.idname));
  /* extra type info for standard socket types */
  NodeSocketTypeUndefined.type = SOCK_CUSTOM;
  NodeSocketTypeUndefined.subtype = PROP_NONE;

  NodeSocketTypeUndefined.use_link_limits_of_type = true;
  NodeSocketTypeUndefined.input_link_limit = 0xFFF;
  NodeSocketTypeUndefined.output_link_limit = 0xFFF;
}

void register_nodes()
{
  register_undefined_types();

  register_standard_node_socket_types();

  register_node_type_frame();
  register_node_type_reroute();
  register_node_type_group_input();
  register_node_type_group_output();

  register_composite_nodes();
  register_shader_nodes();
  register_texture_nodes();
  register_geometry_nodes();
  register_function_nodes();
}
