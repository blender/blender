#include "node_function_util.h"

static bNodeSocketTemplate fn_node_group_instance_id_out[] = {
    {SOCK_STRING, N_("Identifier")},
    {-1, ""},
};

void register_node_type_fn_group_instance_id()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_GROUP_INSTANCE_ID, "Group Instance ID", 0, 0);
  node_type_socket_templates(&ntype, nullptr, fn_node_group_instance_id_out);
  nodeRegisterType(&ntype);
}
