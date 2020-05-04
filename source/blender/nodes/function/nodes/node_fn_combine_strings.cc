#include "node_function_util.h"

static bNodeSocketTemplate fn_node_combine_strings_in[] = {
    {SOCK_STRING, N_("A")},
    {SOCK_STRING, N_("B")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_combine_strings_out[] = {
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

void register_node_type_fn_combine_strings()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_COMBINE_STRINGS, "Combine Strings", 0, 0);
  node_type_socket_templates(&ntype, fn_node_combine_strings_in, fn_node_combine_strings_out);
  nodeRegisterType(&ntype);
}
