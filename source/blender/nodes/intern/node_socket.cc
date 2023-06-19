/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <climits>

#include "DNA_node_types.h"

#include "BLI_color.hh"
#include "BLI_listbase.h"
#include "BLI_math_euler.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"

#include "DNA_collection_types.h"
#include "DNA_material_types.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "MEM_guardedalloc.h"

#include "NOD_node_declaration.hh"
#include "NOD_socket.h"

#include "FN_field.hh"

using namespace blender;
using blender::fn::ValueOrField;
using blender::nodes::SocketDeclarationPtr;

bNodeSocket *node_add_socket_from_template(bNodeTree *ntree,
                                           bNode *node,
                                           bNodeSocketTemplate *stemp,
                                           eNodeSocketInOut in_out)
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
      dval->value = int(stemp->val1);
      dval->min = int(stemp->min);
      dval->max = int(stemp->max);
      break;
    }
    case SOCK_BOOLEAN: {
      bNodeSocketValueBoolean *dval = (bNodeSocketValueBoolean *)sock->default_value;
      dval->value = int(stemp->val1);
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

static bNodeSocket *verify_socket_template(bNodeTree *ntree,
                                           bNode *node,
                                           eNodeSocketInOut in_out,
                                           ListBase *socklist,
                                           bNodeSocketTemplate *stemp)
{
  bNodeSocket *sock;

  for (sock = (bNodeSocket *)socklist->first; sock; sock = sock->next) {
    if (STREQLEN(sock->name, stemp->name, NODE_MAXSTR)) {
      break;
    }
  }
  if (sock) {
    if (sock->type != stemp->type) {
      nodeModifySocketTypeStatic(ntree, node, sock, stemp->type, stemp->subtype);
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
                                        eNodeSocketInOut in_out,
                                        ListBase *socklist,
                                        bNodeSocketTemplate *stemp_first)
{
  bNodeSocket *sock, *nextsock;
  bNodeSocketTemplate *stemp;

  /* no inputs anymore? */
  if (stemp_first == nullptr) {
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

namespace blender::nodes {

static void refresh_socket_list(bNodeTree &ntree,
                                bNode &node,
                                ListBase &sockets,
                                Span<SocketDeclarationPtr> socket_decls,
                                const bool do_id_user)
{
  Vector<bNodeSocket *> old_sockets = sockets;
  VectorSet<bNodeSocket *> new_sockets;
  for (const SocketDeclarationPtr &socket_decl : socket_decls) {
    /* Try to find a socket that corresponds to the declaration. */
    bNodeSocket *old_socket_with_same_identifier = nullptr;
    for (const int i : old_sockets.index_range()) {
      bNodeSocket &old_socket = *old_sockets[i];
      if (old_socket.identifier == socket_decl->identifier) {
        old_sockets.remove_and_reorder(i);
        old_socket_with_same_identifier = &old_socket;
        break;
      }
    }
    bNodeSocket *new_socket = nullptr;
    if (old_socket_with_same_identifier == nullptr) {
      /* Create a completely new socket. */
      new_socket = &socket_decl->build(ntree, node);
    }
    else {
      STRNCPY(old_socket_with_same_identifier->name, socket_decl->name.c_str());
      if (socket_decl->matches(*old_socket_with_same_identifier)) {
        /* The existing socket matches exactly, just use it. */
        new_socket = old_socket_with_same_identifier;
      }
      else {
        /* Clear out identifier to avoid name collisions when a new socket is created. */
        old_socket_with_same_identifier->identifier[0] = '\0';
        new_socket = &socket_decl->update_or_build(ntree, node, *old_socket_with_same_identifier);

        if (new_socket == old_socket_with_same_identifier) {
          /* The existing socket has been updated, set the correct identifier again. */
          STRNCPY(new_socket->identifier, socket_decl->identifier.c_str());
        }
        else {
          /* Move links to new socket with same identifier. */
          LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
            if (link->fromsock == old_socket_with_same_identifier) {
              link->fromsock = new_socket;
            }
            else if (link->tosock == old_socket_with_same_identifier) {
              link->tosock = new_socket;
            }
          }
          for (bNodeLink &internal_link : node.runtime->internal_links) {
            if (internal_link.fromsock == old_socket_with_same_identifier) {
              internal_link.fromsock = new_socket;
            }
            else if (internal_link.tosock == old_socket_with_same_identifier) {
              internal_link.tosock = new_socket;
            }
          }
        }
      }
    }
    new_sockets.add_new(new_socket);
    BKE_ntree_update_tag_socket_new(&ntree, new_socket);
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, old_socket, &sockets) {
    if (!new_sockets.contains(old_socket)) {
      blender::bke::nodeRemoveSocketEx(&ntree, &node, old_socket, do_id_user);
    }
  }
  BLI_listbase_clear(&sockets);
  for (bNodeSocket *socket : new_sockets) {
    BLI_addtail(&sockets, socket);
  }
}

static void refresh_node(bNodeTree &ntree,
                         bNode &node,
                         blender::nodes::NodeDeclaration &node_decl,
                         bool do_id_user)
{
  if (node_decl.skip_updating_sockets) {
    return;
  }
  if (!node_decl.matches(node)) {
    refresh_socket_list(ntree, node, node.inputs, node_decl.inputs, do_id_user);
    refresh_socket_list(ntree, node, node.outputs, node_decl.outputs, do_id_user);
  }
  blender::bke::nodeSocketDeclarationsUpdate(&node);
}

void update_node_declaration_and_sockets(bNodeTree &ntree, bNode &node)
{
  if (node.typeinfo->declare_dynamic) {
    if (!node.runtime->declaration) {
      node.runtime->declaration = new NodeDeclaration();
    }
    build_node_declaration_dynamic(ntree, node, *node.runtime->declaration);
  }
  refresh_node(ntree, node, *node.runtime->declaration, true);
}

}  // namespace blender::nodes

void node_verify_sockets(bNodeTree *ntree, bNode *node, bool do_id_user)
{
  bNodeType *ntype = node->typeinfo;
  if (ntype == nullptr) {
    return;
  }
  if (ntype->declare || ntype->declare_dynamic) {
    blender::bke::nodeDeclarationEnsureOnOutdatedNode(ntree, node);
    refresh_node(*ntree, *node, *node->runtime->declaration, do_id_user);
    return;
  }
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
      bNodeSocketValueFloat *dval = MEM_cnew<bNodeSocketValueFloat>("node socket value float");
      dval->subtype = subtype;
      dval->value = 0.0f;
      dval->min = -FLT_MAX;
      dval->max = FLT_MAX;

      sock->default_value = dval;
      break;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *dval = MEM_cnew<bNodeSocketValueInt>("node socket value int");
      dval->subtype = subtype;
      dval->value = 0;
      dval->min = INT_MIN;
      dval->max = INT_MAX;

      sock->default_value = dval;
      break;
    }
    case SOCK_BOOLEAN: {
      bNodeSocketValueBoolean *dval = MEM_cnew<bNodeSocketValueBoolean>("node socket value bool");
      dval->value = false;

      sock->default_value = dval;
      break;
    }
    case SOCK_ROTATION: {
      bNodeSocketValueRotation *dval = MEM_cnew<bNodeSocketValueRotation>(__func__);
      sock->default_value = dval;
      break;
    }
    case SOCK_VECTOR: {
      static float default_value[] = {0.0f, 0.0f, 0.0f};
      bNodeSocketValueVector *dval = MEM_cnew<bNodeSocketValueVector>("node socket value vector");
      dval->subtype = subtype;
      copy_v3_v3(dval->value, default_value);
      dval->min = -FLT_MAX;
      dval->max = FLT_MAX;

      sock->default_value = dval;
      break;
    }
    case SOCK_RGBA: {
      static float default_value[] = {0.0f, 0.0f, 0.0f, 1.0f};
      bNodeSocketValueRGBA *dval = MEM_cnew<bNodeSocketValueRGBA>("node socket value color");
      copy_v4_v4(dval->value, default_value);

      sock->default_value = dval;
      break;
    }
    case SOCK_STRING: {
      bNodeSocketValueString *dval = MEM_cnew<bNodeSocketValueString>("node socket value string");
      dval->subtype = subtype;
      dval->value[0] = '\0';

      sock->default_value = dval;
      break;
    }
    case SOCK_OBJECT: {
      bNodeSocketValueObject *dval = MEM_cnew<bNodeSocketValueObject>("node socket value object");
      dval->value = nullptr;

      sock->default_value = dval;
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *dval = MEM_cnew<bNodeSocketValueImage>("node socket value image");
      dval->value = nullptr;

      sock->default_value = dval;
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *dval = MEM_cnew<bNodeSocketValueCollection>(
          "node socket value object");
      dval->value = nullptr;

      sock->default_value = dval;
      break;
    }
    case SOCK_TEXTURE: {
      bNodeSocketValueTexture *dval = MEM_cnew<bNodeSocketValueTexture>(
          "node socket value texture");
      dval->value = nullptr;

      sock->default_value = dval;
      break;
    }
    case SOCK_MATERIAL: {
      bNodeSocketValueMaterial *dval = MEM_cnew<bNodeSocketValueMaterial>(
          "node socket value material");
      dval->value = nullptr;

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
    STRNCPY(to->name, from->label);
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
    case SOCK_ROTATION: {
      bNodeSocketValueRotation *toval = (bNodeSocketValueRotation *)to->default_value;
      bNodeSocketValueRotation *fromval = (bNodeSocketValueRotation *)from->default_value;
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
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *toval = (bNodeSocketValueCollection *)to->default_value;
      bNodeSocketValueCollection *fromval = (bNodeSocketValueCollection *)from->default_value;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
    case SOCK_TEXTURE: {
      bNodeSocketValueTexture *toval = (bNodeSocketValueTexture *)to->default_value;
      bNodeSocketValueTexture *fromval = (bNodeSocketValueTexture *)from->default_value;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
    case SOCK_MATERIAL: {
      bNodeSocketValueMaterial *toval = (bNodeSocketValueMaterial *)to->default_value;
      bNodeSocketValueMaterial *fromval = (bNodeSocketValueMaterial *)from->default_value;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
  }

  to->flag |= (from->flag & SOCK_HIDE_VALUE);
}

static void standard_node_socket_interface_init_socket(bNodeTree * /*ntree*/,
                                                       const bNodeSocket *interface_socket,
                                                       bNode * /*node*/,
                                                       bNodeSocket *sock,
                                                       const char * /*data_path*/)
{
  /* initialize the type value */
  sock->type = sock->typeinfo->type;

  /* XXX socket interface 'type' value is not used really,
   * but has to match or the copy function will bail out
   */
  const_cast<bNodeSocket *>(interface_socket)->type = interface_socket->typeinfo->type;
  /* copy default_value settings */
  node_socket_copy_default_value(sock, interface_socket);
}

static void standard_node_socket_interface_from_socket(bNodeTree * /*ntree*/,
                                                       bNodeSocket *stemp,
                                                       const bNode * /*node*/,
                                                       const bNodeSocket *sock)
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
  const char *socket_label = nodeStaticSocketLabel(type, subtype);
  const char *socket_subtype_label = blender::bke::nodeSocketSubTypeLabel(subtype);
  bNodeSocketType *stype;
  StructRNA *srna;

  stype = MEM_cnew<bNodeSocketType>("node socket C type");
  stype->free_self = (void (*)(bNodeSocketType * stype)) MEM_freeN;
  STRNCPY(stype->idname, socket_idname);
  STRNCPY(stype->label, socket_label);
  STRNCPY(stype->subtype_label, socket_subtype_label);

  /* set the RNA type
   * uses the exact same identifier as the socket type idname */
  srna = stype->ext_socket.srna = RNA_struct_find(socket_idname);
  BLI_assert(srna != nullptr);
  /* associate the RNA type with the socket type */
  RNA_struct_blender_type_set(srna, stype);

  /* set the interface RNA type */
  srna = stype->ext_interface.srna = RNA_struct_find(interface_idname);
  BLI_assert(srna != nullptr);
  /* associate the RNA type with the socket type */
  RNA_struct_blender_type_set(srna, stype);

  /* extra type info for standard socket types */
  stype->type = type;
  stype->subtype = subtype;

  /* XXX bad-level call! needed for setting draw callbacks */
  ED_init_standard_node_socket_type(stype);

  stype->interface_init_socket = standard_node_socket_interface_init_socket;
  stype->interface_from_socket = standard_node_socket_interface_from_socket;

  stype->use_link_limits_of_type = true;
  stype->input_link_limit = 1;
  stype->output_link_limit = 0xFFF;

  return stype;
}

extern "C" void ED_init_node_socket_type_virtual(bNodeSocketType *);

static bNodeSocketType *make_socket_type_virtual()
{
  const char *socket_idname = "NodeSocketVirtual";
  bNodeSocketType *stype;
  StructRNA *srna;

  stype = MEM_cnew<bNodeSocketType>("node socket C type");
  stype->free_self = (void (*)(bNodeSocketType * stype)) MEM_freeN;
  STRNCPY(stype->idname, socket_idname);

  /* set the RNA type
   * uses the exact same identifier as the socket type idname */
  srna = stype->ext_socket.srna = RNA_struct_find(socket_idname);
  BLI_assert(srna != nullptr);
  /* associate the RNA type with the socket type */
  RNA_struct_blender_type_set(srna, stype);

  /* extra type info for standard socket types */
  stype->type = SOCK_CUSTOM;

  ED_init_node_socket_type_virtual(stype);

  stype->use_link_limits_of_type = true;
  stype->input_link_limit = 0xFFF;
  stype->output_link_limit = 0xFFF;

  return stype;
}

static bNodeSocketType *make_socket_type_bool()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_BOOLEAN, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<bool>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(bool *)r_value = ((bNodeSocketValueBoolean *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<ValueOrField<bool>>();
  socktype->get_geometry_nodes_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    bool value;
    socket.typeinfo->get_base_cpp_value(socket, &value);
    new (r_value) ValueOrField<bool>(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_rotation()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_ROTATION, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<math::Quaternion>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    const auto &value = *socket.default_value_typed<bNodeSocketValueRotation>();
    const math::EulerXYZ euler(float3(value.value_euler));
    *static_cast<math::Quaternion *>(r_value) = math::to_quaternion(euler);
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<ValueOrField<math::Quaternion>>();
  socktype->get_geometry_nodes_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    math::Quaternion value;
    socket.typeinfo->get_base_cpp_value(socket, &value);
    new (r_value) ValueOrField<math::Quaternion>(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_float(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_FLOAT, subtype);
  socktype->base_cpp_type = &blender::CPPType::get<float>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(float *)r_value = ((bNodeSocketValueFloat *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<ValueOrField<float>>();
  socktype->get_geometry_nodes_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    float value;
    socket.typeinfo->get_base_cpp_value(socket, &value);
    new (r_value) ValueOrField<float>(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_int(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_INT, subtype);
  socktype->base_cpp_type = &blender::CPPType::get<int>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(int *)r_value = ((bNodeSocketValueInt *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<ValueOrField<int>>();
  socktype->get_geometry_nodes_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    int value;
    socket.typeinfo->get_base_cpp_value(socket, &value);
    new (r_value) ValueOrField<int>(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_vector(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_VECTOR, subtype);
  socktype->base_cpp_type = &blender::CPPType::get<blender::float3>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(blender::float3 *)r_value = ((bNodeSocketValueVector *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<ValueOrField<blender::float3>>();
  socktype->get_geometry_nodes_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    blender::float3 value;
    socket.typeinfo->get_base_cpp_value(socket, &value);
    new (r_value) ValueOrField<blender::float3>(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_rgba()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_RGBA, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<blender::ColorGeometry4f>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(blender::ColorGeometry4f *)r_value = ((bNodeSocketValueRGBA *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type =
      &blender::CPPType::get<ValueOrField<blender::ColorGeometry4f>>();
  socktype->get_geometry_nodes_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    blender::ColorGeometry4f value;
    socket.typeinfo->get_base_cpp_value(socket, &value);
    new (r_value) ValueOrField<blender::ColorGeometry4f>(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_string()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_STRING, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<std::string>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    new (r_value) std::string(((bNodeSocketValueString *)socket.default_value)->value);
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<ValueOrField<std::string>>();
  socktype->get_geometry_nodes_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    std::string value;
    value.~basic_string();
    socket.typeinfo->get_base_cpp_value(socket, &value);
    new (r_value) ValueOrField<std::string>(value);
  };
  return socktype;
}

static bNodeSocketType *make_socket_type_object()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_OBJECT, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Object *>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(Object **)r_value = ((bNodeSocketValueObject *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_geometry()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_GEOMETRY, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<blender::bke::GeometrySet>();
  socktype->get_base_cpp_value = [](const bNodeSocket & /*socket*/, void *r_value) {
    new (r_value) blender::bke::GeometrySet();
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_collection()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_COLLECTION, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Collection *>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(Collection **)r_value = ((bNodeSocketValueCollection *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_texture()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_TEXTURE, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Tex *>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(Tex **)r_value = ((bNodeSocketValueTexture *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_image()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_IMAGE, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Image *>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(Image **)r_value = ((bNodeSocketValueImage *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_material()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_MATERIAL, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Material *>();
  socktype->get_base_cpp_value = [](const bNodeSocket &socket, void *r_value) {
    *(Material **)r_value = ((bNodeSocketValueMaterial *)socket.default_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

void register_standard_node_socket_types()
{
  /* Draw callbacks are set in `drawnode.c` to avoid bad-level calls. */

  nodeRegisterSocketType(make_socket_type_float(PROP_NONE));
  nodeRegisterSocketType(make_socket_type_float(PROP_UNSIGNED));
  nodeRegisterSocketType(make_socket_type_float(PROP_PERCENTAGE));
  nodeRegisterSocketType(make_socket_type_float(PROP_FACTOR));
  nodeRegisterSocketType(make_socket_type_float(PROP_ANGLE));
  nodeRegisterSocketType(make_socket_type_float(PROP_TIME));
  nodeRegisterSocketType(make_socket_type_float(PROP_TIME_ABSOLUTE));
  nodeRegisterSocketType(make_socket_type_float(PROP_DISTANCE));

  nodeRegisterSocketType(make_socket_type_int(PROP_NONE));
  nodeRegisterSocketType(make_socket_type_int(PROP_UNSIGNED));
  nodeRegisterSocketType(make_socket_type_int(PROP_PERCENTAGE));
  nodeRegisterSocketType(make_socket_type_int(PROP_FACTOR));

  nodeRegisterSocketType(make_socket_type_bool());
  nodeRegisterSocketType(make_socket_type_rotation());

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

  nodeRegisterSocketType(make_socket_type_geometry());

  nodeRegisterSocketType(make_socket_type_collection());

  nodeRegisterSocketType(make_socket_type_texture());

  nodeRegisterSocketType(make_socket_type_image());

  nodeRegisterSocketType(make_socket_type_material());

  nodeRegisterSocketType(make_socket_type_virtual());
}
