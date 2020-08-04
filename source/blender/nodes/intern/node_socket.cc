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
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup nodes
 */

#include <limits.h>

#include "DNA_node_types.h"

#include "BLI_color.hh"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_lib_id.h"
#include "BKE_node.h"
#include "BKE_persistent_data_handle.hh"

#include "RNA_access.h"
#include "RNA_types.h"

#include "MEM_guardedalloc.h"

#include "NOD_node_tree_multi_function.hh"
#include "NOD_socket.h"

struct bNodeSocket *node_add_socket_from_template(struct bNodeTree *ntree,
                                                  struct bNode *node,
                                                  struct bNodeSocketTemplate *stemp,
                                                  int in_out)
{
  bNodeSocket *sock = nodeAddStaticSocket(
      ntree, node, in_out, stemp->type, stemp->subtype, stemp->identifier, stemp->name);

  sock->flag |= stemp->flag;

  /* initialize default_value */
  switch (stemp->type) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *dval = (bNodeSocketValueFloat *)sock->default_value;
      dval->value = stemp->val1;
      dval->min = stemp->min;
      dval->max = stemp->max;
      break;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *dval = (bNodeSocketValueInt *)sock->default_value;
      dval->value = (int)stemp->val1;
      dval->min = (int)stemp->min;
      dval->max = (int)stemp->max;
      break;
    }
    case SOCK_BOOLEAN: {
      bNodeSocketValueBoolean *dval = (bNodeSocketValueBoolean *)sock->default_value;
      dval->value = (int)stemp->val1;
      break;
    }
    case SOCK_VECTOR: {
      bNodeSocketValueVector *dval = (bNodeSocketValueVector *)sock->default_value;
      dval->value[0] = stemp->val1;
      dval->value[1] = stemp->val2;
      dval->value[2] = stemp->val3;
      dval->min = stemp->min;
      dval->max = stemp->max;
      break;
    }
    case SOCK_RGBA: {
      bNodeSocketValueRGBA *dval = (bNodeSocketValueRGBA *)sock->default_value;
      dval->value[0] = stemp->val1;
      dval->value[1] = stemp->val2;
      dval->value[2] = stemp->val3;
      dval->value[3] = stemp->val4;
      break;
    }
  }

  return sock;
}

static bNodeSocket *verify_socket_template(
    bNodeTree *ntree, bNode *node, int in_out, ListBase *socklist, bNodeSocketTemplate *stemp)
{
  bNodeSocket *sock;

  for (sock = (bNodeSocket *)socklist->first; sock; sock = sock->next) {
    if (STREQLEN(sock->name, stemp->name, NODE_MAXSTR)) {
      break;
    }
  }
  if (sock) {
    if (sock->type != stemp->type) {
      nodeModifySocketType(ntree, node, sock, stemp->type, stemp->subtype);
    }
    sock->flag |= stemp->flag;
  }
  else {
    /* no socket for this template found, make a new one */
    sock = node_add_socket_from_template(ntree, node, stemp, in_out);
  }

  /* remove the new socket from the node socket list first,
   * will be added back after verification. */
  BLI_remlink(socklist, sock);

  return sock;
}

static void verify_socket_template_list(bNodeTree *ntree,
                                        bNode *node,
                                        int in_out,
                                        ListBase *socklist,
                                        bNodeSocketTemplate *stemp_first)
{
  bNodeSocket *sock, *nextsock;
  bNodeSocketTemplate *stemp;

  /* no inputs anymore? */
  if (stemp_first == NULL) {
    for (sock = (bNodeSocket *)socklist->first; sock; sock = nextsock) {
      nextsock = sock->next;
      nodeRemoveSocket(ntree, node, sock);
    }
  }
  else {
    /* step by step compare */
    stemp = stemp_first;
    while (stemp->type != -1) {
      stemp->sock = verify_socket_template(ntree, node, in_out, socklist, stemp);
      stemp++;
    }
    /* leftovers are removed */
    for (sock = (bNodeSocket *)socklist->first; sock; sock = nextsock) {
      nextsock = sock->next;
      nodeRemoveSocket(ntree, node, sock);
    }

    /* and we put back the verified sockets */
    stemp = stemp_first;
    if (socklist->first) {
      /* Some dynamic sockets left, store the list start
       * so we can add static sockets in front of it. */
      sock = (bNodeSocket *)socklist->first;
      while (stemp->type != -1) {
        /* Put static sockets in front of dynamic. */
        BLI_insertlinkbefore(socklist, sock, stemp->sock);
        stemp++;
      }
    }
    else {
      while (stemp->type != -1) {
        BLI_addtail(socklist, stemp->sock);
        stemp++;
      }
    }
  }
}

void node_verify_socket_templates(bNodeTree *ntree, bNode *node)
{
  bNodeType *ntype = node->typeinfo;
  /* Don't try to match socket lists when there are no templates.
   * This prevents dynamically generated sockets to be removed, like for
   * group, image or render layer nodes. We have an explicit check for the
   * render layer node since it still has fixed sockets too.
   */
  if (ntype) {
    if (ntype->inputs && ntype->inputs[0].type >= 0) {
      verify_socket_template_list(ntree, node, SOCK_IN, &node->inputs, ntype->inputs);
    }
    if (ntype->outputs && ntype->outputs[0].type >= 0 && node->type != CMP_NODE_R_LAYERS) {
      verify_socket_template_list(ntree, node, SOCK_OUT, &node->outputs, ntype->outputs);
    }
  }
}

void node_socket_init_default_value(bNodeSocket *sock)
{
  int type = sock->typeinfo->type;
  int subtype = sock->typeinfo->subtype;

  if (sock->default_value) {
    return; /* already initialized */
  }

  switch (type) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *dval = (bNodeSocketValueFloat *)MEM_callocN(
          sizeof(bNodeSocketValueFloat), "node socket value float");
      dval->subtype = subtype;
      dval->value = 0.0f;
      dval->min = -FLT_MAX;
      dval->max = FLT_MAX;

      sock->default_value = dval;
      break;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *dval = (bNodeSocketValueInt *)MEM_callocN(sizeof(bNodeSocketValueInt),
                                                                     "node socket value int");
      dval->subtype = subtype;
      dval->value = 0;
      dval->min = INT_MIN;
      dval->max = INT_MAX;

      sock->default_value = dval;
      break;
    }
    case SOCK_BOOLEAN: {
      bNodeSocketValueBoolean *dval = (bNodeSocketValueBoolean *)MEM_callocN(
          sizeof(bNodeSocketValueBoolean), "node socket value bool");
      dval->value = false;

      sock->default_value = dval;
      break;
    }
    case SOCK_VECTOR: {
      static float default_value[] = {0.0f, 0.0f, 0.0f};
      bNodeSocketValueVector *dval = (bNodeSocketValueVector *)MEM_callocN(
          sizeof(bNodeSocketValueVector), "node socket value vector");
      dval->subtype = subtype;
      copy_v3_v3(dval->value, default_value);
      dval->min = -FLT_MAX;
      dval->max = FLT_MAX;

      sock->default_value = dval;
      break;
    }
    case SOCK_RGBA: {
      static float default_value[] = {0.0f, 0.0f, 0.0f, 1.0f};
      bNodeSocketValueRGBA *dval = (bNodeSocketValueRGBA *)MEM_callocN(
          sizeof(bNodeSocketValueRGBA), "node socket value color");
      copy_v4_v4(dval->value, default_value);

      sock->default_value = dval;
      break;
    }
    case SOCK_STRING: {
      bNodeSocketValueString *dval = (bNodeSocketValueString *)MEM_callocN(
          sizeof(bNodeSocketValueString), "node socket value string");
      dval->subtype = subtype;
      dval->value[0] = '\0';

      sock->default_value = dval;
      break;
    }
    case SOCK_OBJECT: {
      bNodeSocketValueObject *dval = (bNodeSocketValueObject *)MEM_callocN(
          sizeof(bNodeSocketValueObject), "node socket value object");
      dval->value = NULL;

      sock->default_value = dval;
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *dval = (bNodeSocketValueImage *)MEM_callocN(
          sizeof(bNodeSocketValueImage), "node socket value image");
      dval->value = NULL;

      sock->default_value = dval;
      break;
    }
  }
}

void node_socket_copy_default_value(bNodeSocket *to, const bNodeSocket *from)
{
  /* sanity check */
  if (to->type != from->type) {
    return;
  }

  /* make sure both exist */
  if (!from->default_value) {
    return;
  }
  node_socket_init_default_value(to);

  /* use label instead of name if it has been set */
  if (from->label[0] != '\0') {
    BLI_strncpy(to->name, from->label, NODE_MAXSTR);
  }

  switch (from->typeinfo->type) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *toval = (bNodeSocketValueFloat *)to->default_value;
      bNodeSocketValueFloat *fromval = (bNodeSocketValueFloat *)from->default_value;
      *toval = *fromval;
      break;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *toval = (bNodeSocketValueInt *)to->default_value;
      bNodeSocketValueInt *fromval = (bNodeSocketValueInt *)from->default_value;
      *toval = *fromval;
      break;
    }
    case SOCK_BOOLEAN: {
      bNodeSocketValueBoolean *toval = (bNodeSocketValueBoolean *)to->default_value;
      bNodeSocketValueBoolean *fromval = (bNodeSocketValueBoolean *)from->default_value;
      *toval = *fromval;
      break;
    }
    case SOCK_VECTOR: {
      bNodeSocketValueVector *toval = (bNodeSocketValueVector *)to->default_value;
      bNodeSocketValueVector *fromval = (bNodeSocketValueVector *)from->default_value;
      *toval = *fromval;
      break;
    }
    case SOCK_RGBA: {
      bNodeSocketValueRGBA *toval = (bNodeSocketValueRGBA *)to->default_value;
      bNodeSocketValueRGBA *fromval = (bNodeSocketValueRGBA *)from->default_value;
      *toval = *fromval;
      break;
    }
    case SOCK_STRING: {
      bNodeSocketValueString *toval = (bNodeSocketValueString *)to->default_value;
      bNodeSocketValueString *fromval = (bNodeSocketValueString *)from->default_value;
      *toval = *fromval;
      break;
    }
    case SOCK_OBJECT: {
      bNodeSocketValueObject *toval = (bNodeSocketValueObject *)to->default_value;
      bNodeSocketValueObject *fromval = (bNodeSocketValueObject *)from->default_value;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *toval = (bNodeSocketValueImage *)to->default_value;
      bNodeSocketValueImage *fromval = (bNodeSocketValueImage *)from->default_value;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
  }

  to->flag |= (from->flag & SOCK_HIDE_VALUE);
}

void node_socket_skip_reroutes(
    ListBase *links, bNode *node, bNodeSocket *socket, bNode **r_node, bNodeSocket **r_socket)
{
  const int loop_limit = 100; /* Limit in case there is a connection cycle. */

  if (socket->in_out == SOCK_IN) {
    bNodeLink *first_link = (bNodeLink *)links->first;

    for (int i = 0; node->type == NODE_REROUTE && i < loop_limit; i++) {
      bNodeLink *link = first_link;

      for (; link; link = link->next) {
        if (link->fromnode == node && link->tonode != node) {
          break;
        }
      }

      if (link) {
        node = link->tonode;
        socket = link->tosock;
      }
      else {
        break;
      }
    }
  }
  else {
    for (int i = 0; node->type == NODE_REROUTE && i < loop_limit; i++) {
      bNodeSocket *input = (bNodeSocket *)node->inputs.first;

      if (input && input->link) {
        node = input->link->fromnode;
        socket = input->link->fromsock;
      }
      else {
        break;
      }
    }
  }

  if (r_node) {
    *r_node = node;
  }
  if (r_socket) {
    *r_socket = socket;
  }
}

static void standard_node_socket_interface_init_socket(bNodeTree *UNUSED(ntree),
                                                       bNodeSocket *stemp,
                                                       bNode *UNUSED(node),
                                                       bNodeSocket *sock,
                                                       const char *UNUSED(data_path))
{
  /* initialize the type value */
  sock->type = sock->typeinfo->type;

  /* XXX socket interface 'type' value is not used really,
   * but has to match or the copy function will bail out
   */
  stemp->type = stemp->typeinfo->type;
  /* copy default_value settings */
  node_socket_copy_default_value(sock, stemp);
}

/* copies settings that are not changed for each socket instance */
static void standard_node_socket_interface_verify_socket(bNodeTree *UNUSED(ntree),
                                                         bNodeSocket *stemp,
                                                         bNode *UNUSED(node),
                                                         bNodeSocket *sock,
                                                         const char *UNUSED(data_path))
{
  /* sanity check */
  if (sock->type != stemp->typeinfo->type) {
    return;
  }

  /* make sure both exist */
  if (!stemp->default_value) {
    return;
  }
  node_socket_init_default_value(sock);

  switch (stemp->typeinfo->type) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *toval = (bNodeSocketValueFloat *)sock->default_value;
      bNodeSocketValueFloat *fromval = (bNodeSocketValueFloat *)stemp->default_value;
      toval->min = fromval->min;
      toval->max = fromval->max;
      break;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *toval = (bNodeSocketValueInt *)sock->default_value;
      bNodeSocketValueInt *fromval = (bNodeSocketValueInt *)stemp->default_value;
      toval->min = fromval->min;
      toval->max = fromval->max;
      break;
    }
    case SOCK_VECTOR: {
      bNodeSocketValueVector *toval = (bNodeSocketValueVector *)sock->default_value;
      bNodeSocketValueVector *fromval = (bNodeSocketValueVector *)stemp->default_value;
      toval->min = fromval->min;
      toval->max = fromval->max;
      break;
    }
  }
}

static void standard_node_socket_interface_from_socket(bNodeTree *UNUSED(ntree),
                                                       bNodeSocket *stemp,
                                                       bNode *UNUSED(node),
                                                       bNodeSocket *sock)
{
  /* initialize settings */
  stemp->type = stemp->typeinfo->type;
  node_socket_copy_default_value(stemp, sock);
}

extern "C" void ED_init_standard_node_socket_type(bNodeSocketType *);

static bNodeSocketType *make_standard_socket_type(int type, int subtype)
{
  const char *socket_idname = nodeStaticSocketType(type, subtype);
  const char *interface_idname = nodeStaticSocketInterfaceType(type, subtype);
  bNodeSocketType *stype;
  StructRNA *srna;

  stype = (bNodeSocketType *)MEM_callocN(sizeof(bNodeSocketType), "node socket C type");
  stype->free_self = (void (*)(bNodeSocketType * stype)) MEM_freeN;
  BLI_strncpy(stype->idname, socket_idname, sizeof(stype->idname));

  /* set the RNA type
   * uses the exact same identifier as the socket type idname */
  srna = stype->ext_socket.srna = RNA_struct_find(socket_idname);
  BLI_assert(srna != NULL);
  /* associate the RNA type with the socket type */
  RNA_struct_blender_type_set(srna, stype);

  /* set the interface RNA type */
  srna = stype->ext_interface.srna = RNA_struct_find(interface_idname);
  BLI_assert(srna != NULL);
  /* associate the RNA type with the socket type */
  RNA_struct_blender_type_set(srna, stype);

  /* extra type info for standard socket types */
  stype->type = type;
  stype->subtype = subtype;

  /* XXX bad-level call! needed for setting draw callbacks */
  ED_init_standard_node_socket_type(stype);

  stype->interface_init_socket = standard_node_socket_interface_init_socket;
  stype->interface_from_socket = standard_node_socket_interface_from_socket;
  stype->interface_verify_socket = standard_node_socket_interface_verify_socket;

  stype->use_link_limits_of_type = true;
  stype->input_link_limit = 1;
  stype->output_link_limit = 0xFFF;

  return stype;
}

extern "C" void ED_init_node_socket_type_virtual(bNodeSocketType *);

static bNodeSocketType *make_socket_type_virtual(void)
{
  const char *socket_idname = "NodeSocketVirtual";
  bNodeSocketType *stype;
  StructRNA *srna;

  stype = (bNodeSocketType *)MEM_callocN(sizeof(bNodeSocketType), "node socket C type");
  stype->free_self = (void (*)(bNodeSocketType * stype)) MEM_freeN;
  BLI_strncpy(stype->idname, socket_idname, sizeof(stype->idname));

  /* set the RNA type
   * uses the exact same identifier as the socket type idname */
  srna = stype->ext_socket.srna = RNA_struct_find(socket_idname);
  BLI_assert(srna != NULL);
  /* associate the RNA type with the socket type */
  RNA_struct_blender_type_set(srna, stype);

  /* extra type info for standard socket types */
  stype->type = SOCK_CUSTOM;

  ED_init_node_socket_type_virtual(stype);

  stype->use_link_limits_of_type = true;
  stype->input_link_limit = 1;
  stype->output_link_limit = 1;

  return stype;
}

static bNodeSocketType *make_socket_type_effector(int type)
{
  bNodeSocketType *stype = make_standard_socket_type(type, PROP_NONE);
  stype->input_link_limit = 0xFFF;
  return stype;
}

static bNodeSocketType *make_socket_type_control_flow(int type)
{
  bNodeSocketType *stype = make_standard_socket_type(type, PROP_NONE);
  return stype;
}

static bNodeSocketType *make_socket_type_bool()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_BOOLEAN, PROP_NONE);
  socktype->get_mf_data_type = []() { return blender::fn::MFDataType::ForSingle<bool>(); };
  socktype->expand_in_mf_network = [](blender::nodes::SocketMFNetworkBuilder &builder) {
    bool value = builder.socket_default_value<bNodeSocketValueBoolean>()->value;
    builder.set_constant_value(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_float(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_FLOAT, subtype);
  socktype->get_mf_data_type = []() { return blender::fn::MFDataType::ForSingle<float>(); };
  socktype->expand_in_mf_network = [](blender::nodes::SocketMFNetworkBuilder &builder) {
    float value = builder.socket_default_value<bNodeSocketValueFloat>()->value;
    builder.set_constant_value(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_int(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_INT, subtype);
  socktype->get_mf_data_type = []() { return blender::fn::MFDataType::ForSingle<int>(); };
  socktype->expand_in_mf_network = [](blender::nodes::SocketMFNetworkBuilder &builder) {
    int value = builder.socket_default_value<bNodeSocketValueInt>()->value;
    builder.set_constant_value(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_vector(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_VECTOR, subtype);
  socktype->get_mf_data_type = []() {
    return blender::fn::MFDataType::ForSingle<blender::float3>();
  };
  socktype->expand_in_mf_network = [](blender::nodes::SocketMFNetworkBuilder &builder) {
    blender::float3 value = builder.socket_default_value<bNodeSocketValueVector>()->value;
    builder.set_constant_value(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_rgba()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_RGBA, PROP_NONE);
  socktype->get_mf_data_type = []() {
    return blender::fn::MFDataType::ForSingle<blender::Color4f>();
  };
  socktype->expand_in_mf_network = [](blender::nodes::SocketMFNetworkBuilder &builder) {
    blender::Color4f value = builder.socket_default_value<bNodeSocketValueRGBA>()->value;
    builder.set_constant_value(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_string()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_STRING, PROP_NONE);
  socktype->get_mf_data_type = []() { return blender::fn::MFDataType::ForSingle<std::string>(); };
  socktype->expand_in_mf_network = [](blender::nodes::SocketMFNetworkBuilder &builder) {
    std::string value = builder.socket_default_value<bNodeSocketValueString>()->value;
    builder.set_constant_value(value);
  };
  return socktype;
}

class ObjectSocketMultiFunction : public blender::fn::MultiFunction {
 private:
  Object *object_;

 public:
  ObjectSocketMultiFunction(Object *object) : object_(object)
  {
    blender::fn::MFSignatureBuilder signature = this->get_builder("Object Socket");
    signature.depends_on_context();
    signature.single_output<blender::bke::PersistentObjectHandle>("Object");
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext context) const override
  {
    blender::MutableSpan output =
        params.uninitialized_single_output<blender::bke::PersistentObjectHandle>(0, "Object");

    /* Try to get a handle map, so that the object can be converted to a handle. */
    const blender::bke::PersistentDataHandleMap *handle_map =
        context.get_global_context<blender::bke::PersistentDataHandleMap>(
            "PersistentDataHandleMap");

    if (handle_map == nullptr) {
      /* Return empty handles when there is no handle map. */
      output.fill_indices(mask, blender::bke::PersistentObjectHandle());
      return;
    }

    blender::bke::PersistentObjectHandle handle = handle_map->lookup(object_);
    for (int64_t i : mask) {
      output[i] = handle;
    }
  }
};

MAKE_CPP_TYPE(PersistentObjectHandle, blender::bke::PersistentObjectHandle);

static bNodeSocketType *make_socket_type_object()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_OBJECT, PROP_NONE);
  socktype->get_mf_data_type = []() {
    /* Objects are not passed along as raw pointers, but as handles. */
    return blender::fn::MFDataType::ForSingle<blender::bke::PersistentObjectHandle>();
  };
  socktype->expand_in_mf_network = [](blender::nodes::SocketMFNetworkBuilder &builder) {
    Object *object = builder.socket_default_value<bNodeSocketValueObject>()->value;
    builder.construct_generator_fn<ObjectSocketMultiFunction>(object);
  };
  return socktype;
}

void register_standard_node_socket_types(void)
{
  /* draw callbacks are set in drawnode.c to avoid bad-level calls */

  nodeRegisterSocketType(make_socket_type_float(PROP_NONE));
  nodeRegisterSocketType(make_socket_type_float(PROP_UNSIGNED));
  nodeRegisterSocketType(make_socket_type_float(PROP_PERCENTAGE));
  nodeRegisterSocketType(make_socket_type_float(PROP_FACTOR));
  nodeRegisterSocketType(make_socket_type_float(PROP_ANGLE));
  nodeRegisterSocketType(make_socket_type_float(PROP_TIME));

  nodeRegisterSocketType(make_socket_type_int(PROP_NONE));
  nodeRegisterSocketType(make_socket_type_int(PROP_UNSIGNED));
  nodeRegisterSocketType(make_socket_type_int(PROP_PERCENTAGE));
  nodeRegisterSocketType(make_socket_type_int(PROP_FACTOR));

  nodeRegisterSocketType(make_socket_type_bool());

  nodeRegisterSocketType(make_socket_type_vector(PROP_NONE));
  nodeRegisterSocketType(make_socket_type_vector(PROP_TRANSLATION));
  nodeRegisterSocketType(make_socket_type_vector(PROP_DIRECTION));
  nodeRegisterSocketType(make_socket_type_vector(PROP_VELOCITY));
  nodeRegisterSocketType(make_socket_type_vector(PROP_ACCELERATION));
  nodeRegisterSocketType(make_socket_type_vector(PROP_EULER));
  nodeRegisterSocketType(make_socket_type_vector(PROP_XYZ));

  nodeRegisterSocketType(make_socket_type_rgba());

  nodeRegisterSocketType(make_socket_type_string());

  nodeRegisterSocketType(make_standard_socket_type(SOCK_SHADER, PROP_NONE));

  nodeRegisterSocketType(make_socket_type_object());

  nodeRegisterSocketType(make_standard_socket_type(SOCK_IMAGE, PROP_NONE));

  nodeRegisterSocketType(make_socket_type_effector(SOCK_EMITTERS));
  nodeRegisterSocketType(make_socket_type_effector(SOCK_EVENTS));
  nodeRegisterSocketType(make_socket_type_effector(SOCK_FORCES));

  nodeRegisterSocketType(make_socket_type_control_flow(SOCK_CONTROL_FLOW));

  nodeRegisterSocketType(make_socket_type_virtual());
}
