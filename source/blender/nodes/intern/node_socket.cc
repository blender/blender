/* SPDX-FileCopyrightText: 2007 Blender Authors
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
#include "BKE_lib_id.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_node_tree_update.hh"

#include "DNA_collection_types.h"
#include "DNA_material_types.h"

#include "RNA_access.hh"

#include "MEM_guardedalloc.h"

#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"
#include "NOD_socket_declarations.hh"

using namespace blender;
using blender::bke::SocketValueVariant;
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

static void refresh_node_socket(bNodeTree &ntree,
                                bNode &node,
                                const SocketDeclaration &socket_decl,
                                Vector<bNodeSocket *> &old_sockets,
                                VectorSet<bNodeSocket *> &new_sockets)
{
  /* Try to find a socket that corresponds to the declaration. */
  bNodeSocket *old_socket_with_same_identifier = nullptr;
  for (const int i : old_sockets.index_range()) {
    bNodeSocket &old_socket = *old_sockets[i];
    if (old_socket.identifier == socket_decl.identifier) {
      old_sockets.remove_and_reorder(i);
      old_socket_with_same_identifier = &old_socket;
      break;
    }
  }
  bNodeSocket *new_socket = nullptr;
  if (old_socket_with_same_identifier == nullptr) {
    /* Create a completely new socket. */
    new_socket = &socket_decl.build(ntree, node);
  }
  else {
    STRNCPY(old_socket_with_same_identifier->name, socket_decl.name.c_str());
    if (socket_decl.matches(*old_socket_with_same_identifier)) {
      /* The existing socket matches exactly, just use it. */
      new_socket = old_socket_with_same_identifier;
    }
    else {
      /* Clear out identifier to avoid name collisions when a new socket is created. */
      old_socket_with_same_identifier->identifier[0] = '\0';
      new_socket = &socket_decl.update_or_build(ntree, node, *old_socket_with_same_identifier);

      if (new_socket == old_socket_with_same_identifier) {
        /* The existing socket has been updated, set the correct identifier again. */
        STRNCPY(new_socket->identifier, socket_decl.identifier.c_str());
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

static void refresh_node_panel(const PanelDeclaration &panel_decl,
                               Vector<bNodePanelState> &old_panels,
                               bNodePanelState &new_panel)
{
  /* Try to find a panel that corresponds to the declaration. */
  bNodePanelState *old_panel_with_same_identifier = nullptr;
  for (const int i : old_panels.index_range()) {
    bNodePanelState &old_panel = old_panels[i];
    if (old_panel.identifier == panel_decl.identifier) {
      /* Panel is removed after copying to #new_panel. */
      old_panel_with_same_identifier = &old_panel;
      break;
    }
  }

  if (old_panel_with_same_identifier == nullptr) {
    /* Create a completely new panel. */
    panel_decl.build(new_panel);
  }
  else {
    if (panel_decl.matches(*old_panel_with_same_identifier)) {
      /* The existing socket matches exactly, just use it. */
      new_panel = *old_panel_with_same_identifier;
    }
    else {
      /* Clear out identifier to avoid name collisions when a new panel is created. */
      old_panel_with_same_identifier->identifier = -1;
      panel_decl.update_or_build(*old_panel_with_same_identifier, new_panel);
    }

    /* Remove from old panels. */
    const int64_t old_panel_index = old_panel_with_same_identifier - old_panels.begin();
    old_panels.remove_and_reorder(old_panel_index);
  }
}

/**
 * Not great to have this here, but this is only for forward compatibility, so this code shouldn't
 * in the `main` branch.
 */
static std::optional<eNodeSocketDatatype> decl_to_data_type(const SocketDeclaration &socket_decl)
{
  if (dynamic_cast<const decl::Float *>(&socket_decl)) {
    return SOCK_FLOAT;
  }
  else if (dynamic_cast<const decl::Int *>(&socket_decl)) {
    return SOCK_INT;
  }
  else if (dynamic_cast<const decl::Bool *>(&socket_decl)) {
    return SOCK_BOOLEAN;
  }
  else if (dynamic_cast<const decl::Vector *>(&socket_decl)) {
    return SOCK_VECTOR;
  }
  else if (dynamic_cast<const decl::Color *>(&socket_decl)) {
    return SOCK_RGBA;
  }
  else if (dynamic_cast<const decl::Rotation *>(&socket_decl)) {
    return SOCK_ROTATION;
  }
  else if (dynamic_cast<const decl::String *>(&socket_decl)) {
    return SOCK_STRING;
  }
  else if (dynamic_cast<const decl::Image *>(&socket_decl)) {
    return SOCK_IMAGE;
  }
  else if (dynamic_cast<const decl::Texture *>(&socket_decl)) {
    return SOCK_TEXTURE;
  }
  else if (dynamic_cast<const decl::Material *>(&socket_decl)) {
    return SOCK_MATERIAL;
  }
  else if (dynamic_cast<const decl::Shader *>(&socket_decl)) {
    return SOCK_SHADER;
  }
  else if (dynamic_cast<const decl::Collection *>(&socket_decl)) {
    return SOCK_COLLECTION;
  }
  else if (dynamic_cast<const decl::Object *>(&socket_decl)) {
    return SOCK_OBJECT;
  }
  return std::nullopt;
}

static const char *get_identifier_from_decl(const char *identifier_prefix,
                                            const bNodeSocket &socket,
                                            const Span<const SocketDeclaration *> socket_decls)
{
  if (!BLI_str_startswith(socket.identifier, identifier_prefix)) {
    return nullptr;
  }
  for (const SocketDeclaration *socket_decl : socket_decls) {
    if (BLI_str_startswith(socket_decl->identifier.c_str(), identifier_prefix)) {
      if (socket.type == decl_to_data_type(*socket_decl)) {
        return socket_decl->identifier.c_str();
      }
    }
  }
  return nullptr;
}

static const char *get_identifier_from_decl(const Span<const char *> identifier_prefixes,
                                            const bNodeSocket &socket,
                                            const Span<const SocketDeclaration *> socket_decls)
{
  for (const char *identifier_prefix : identifier_prefixes) {
    if (const char *identifier = get_identifier_from_decl(identifier_prefix, socket, socket_decls))
    {
      return identifier;
    }
  }
  return nullptr;
}

/**
 * Currently, nodes that support different socket types have sockets for all supported types with
 * different identifiers (e.g. `Attribute`, `Attribute_001`, `Attribute_002`, ...). In the future,
 * we will hopefully have a better way to handle this that does not require all the sockets of
 * different types to exist at the same time. Instead we want that there is only a single socket
 * that can change its type while the identifier stays the same.
 *
 * This function prepares us for that future. It returns the identifier that we use for a socket
 * now based on the "base socket name" (e.g. `Attribute`) and its socket type. It allows us to
 * change the socket identifiers in the future without breaking forward compatibility for the nodes
 * handled here.
 */
static const char *get_current_socket_identifier_for_future_socket(
    const bNode &node,
    const bNodeSocket &socket,
    const Span<const SocketDeclaration *> socket_decls)
{
  switch (node.type) {
    case FN_NODE_RANDOM_VALUE: {
      return get_identifier_from_decl({"Min", "Max", "Value"}, socket, socket_decls);
    }
    case SH_NODE_MIX: {
      return get_identifier_from_decl({"A", "B", "Result"}, socket, socket_decls);
    }
    case FN_NODE_COMPARE: {
      if (STREQ(socket.identifier, "Angle")) {
        return nullptr;
      }
      return get_identifier_from_decl({"A", "B"}, socket, socket_decls);
    }
    case SH_NODE_MAP_RANGE: {
      if (socket.type == SOCK_VECTOR) {
        if (STREQ(socket.identifier, "Value")) {
          return "Vector";
        }
        if (STREQ(socket.identifier, "From Min")) {
          return "From_Min_FLOAT3";
        }
        if (STREQ(socket.identifier, "From Max")) {
          return "From_Max_FLOAT3";
        }
        if (STREQ(socket.identifier, "To Min")) {
          return "To_Min_FLOAT3";
        }
        if (STREQ(socket.identifier, "To Max")) {
          return "To_Max_FLOAT3";
        }
        if (STREQ(socket.identifier, "Steps")) {
          return "Steps_FLOAT3";
        }
        if (STREQ(socket.identifier, "Result")) {
          return "Vector";
        }
      }
      return nullptr;
    }
  }
  return nullptr;
}

/**
 * Try to update identifiers of sockets created in the future to match identifiers that exist now.
 */
static void do_forward_compat_versioning(bNode &node, const NodeDeclaration &node_decl)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (socket->is_available()) {
      if (const char *new_identifier = get_current_socket_identifier_for_future_socket(
              node, *socket, node_decl.inputs))
      {
        STRNCPY(socket->identifier, new_identifier);
      }
    }
  }
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.outputs) {
    if (socket->is_available()) {
      if (const char *new_identifier = get_current_socket_identifier_for_future_socket(
              node, *socket, node_decl.outputs))
      {
        STRNCPY(socket->identifier, new_identifier);
      }
    }
  }
}

static void refresh_node_sockets_and_panels(bNodeTree &ntree,
                                            bNode &node,
                                            const NodeDeclaration &node_decl,
                                            const bool do_id_user)
{
  if (!node.runtime->forward_compatible_versioning_done) {
    do_forward_compat_versioning(node, node_decl);
    node.runtime->forward_compatible_versioning_done = true;
  }

  /* Count panels */
  int new_num_panels = 0;
  for (const ItemDeclarationPtr &item_decl : node_decl.items) {
    if (dynamic_cast<const PanelDeclaration *>(item_decl.get())) {
      ++new_num_panels;
    }
  }

  Vector<bNodeSocket *> old_inputs;
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    old_inputs.append(socket);
  }

  Vector<bNodeSocket *> old_outputs;
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.outputs) {
    old_outputs.append(socket);
  }

  Vector<bNodePanelState> old_panels = Vector<bNodePanelState>(node.panel_states());

  /* New panel states buffer. */
  MEM_SAFE_FREE(node.panel_states_array);
  node.num_panel_states = new_num_panels;
  node.panel_states_array = MEM_cnew_array<bNodePanelState>(new_num_panels, __func__);

  /* Find list of sockets to add, mixture of old and new sockets. */
  VectorSet<bNodeSocket *> new_inputs;
  VectorSet<bNodeSocket *> new_outputs;
  bNodePanelState *new_panel = node.panel_states_array;
  for (const ItemDeclarationPtr &item_decl : node_decl.items) {
    if (const SocketDeclaration *socket_decl = dynamic_cast<const SocketDeclaration *>(
            item_decl.get()))
    {
      if (socket_decl->in_out == SOCK_IN) {
        refresh_node_socket(ntree, node, *socket_decl, old_inputs, new_inputs);
      }
      else {
        refresh_node_socket(ntree, node, *socket_decl, old_outputs, new_outputs);
      }
    }
    else if (const PanelDeclaration *panel_decl = dynamic_cast<const PanelDeclaration *>(
                 item_decl.get()))
    {
      refresh_node_panel(*panel_decl, old_panels, *new_panel);
      ++new_panel;
    }
  }

  /* Destroy any remaining sockets that are no longer in the declaration. */
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, old_socket, &node.inputs) {
    if (!new_inputs.contains(old_socket)) {
      blender::bke::nodeRemoveSocketEx(&ntree, &node, old_socket, do_id_user);
    }
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, old_socket, &node.outputs) {
    if (!new_outputs.contains(old_socket)) {
      blender::bke::nodeRemoveSocketEx(&ntree, &node, old_socket, do_id_user);
    }
  }

  /* Clear and reinsert sockets in the new order. */
  BLI_listbase_clear(&node.inputs);
  BLI_listbase_clear(&node.outputs);
  for (bNodeSocket *socket : new_inputs) {
    BLI_addtail(&node.inputs, socket);
  }
  for (bNodeSocket *socket : new_outputs) {
    BLI_addtail(&node.outputs, socket);
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
    refresh_node_sockets_and_panels(ntree, node, node_decl, do_id_user);
  }
  blender::bke::nodeSocketDeclarationsUpdate(&node);
}

void update_node_declaration_and_sockets(bNodeTree &ntree, bNode &node)
{
  if (node.typeinfo->declare) {
    if (node.typeinfo->static_declaration->is_context_dependent) {
      if (!node.runtime->declaration) {
        node.runtime->declaration = new NodeDeclaration();
      }
      build_node_declaration(*node.typeinfo, *node.runtime->declaration, &ntree, &node);
    }
  }
  refresh_node(ntree, node, *node.runtime->declaration, true);
}

bool socket_type_supports_fields(const eNodeSocketDatatype socket_type)
{
  return ELEM(socket_type,
              SOCK_FLOAT,
              SOCK_VECTOR,
              SOCK_RGBA,
              SOCK_BOOLEAN,
              SOCK_INT,
              SOCK_ROTATION,
              SOCK_MENU);
}

}  // namespace blender::nodes

void node_verify_sockets(bNodeTree *ntree, bNode *node, bool do_id_user)
{
  bNodeType *ntype = node->typeinfo;
  if (ntype == nullptr) {
    return;
  }
  if (ntype->declare) {
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

void node_socket_init_default_value_data(eNodeSocketDatatype datatype, int subtype, void **data)
{
  if (!data) {
    return;
  }

  switch (datatype) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *dval = MEM_cnew<bNodeSocketValueFloat>("node socket value float");
      dval->subtype = subtype;
      dval->value = 0.0f;
      dval->min = -FLT_MAX;
      dval->max = FLT_MAX;

      *data = dval;
      break;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *dval = MEM_cnew<bNodeSocketValueInt>("node socket value int");
      dval->subtype = subtype;
      dval->value = 0;
      dval->min = INT_MIN;
      dval->max = INT_MAX;

      *data = dval;
      break;
    }
    case SOCK_BOOLEAN: {
      bNodeSocketValueBoolean *dval = MEM_cnew<bNodeSocketValueBoolean>("node socket value bool");
      dval->value = false;

      *data = dval;
      break;
    }
    case SOCK_ROTATION: {
      bNodeSocketValueRotation *dval = MEM_cnew<bNodeSocketValueRotation>(__func__);
      *data = dval;
      break;
    }
    case SOCK_VECTOR: {
      static float default_value[] = {0.0f, 0.0f, 0.0f};
      bNodeSocketValueVector *dval = MEM_cnew<bNodeSocketValueVector>("node socket value vector");
      dval->subtype = subtype;
      copy_v3_v3(dval->value, default_value);
      dval->min = -FLT_MAX;
      dval->max = FLT_MAX;

      *data = dval;
      break;
    }
    case SOCK_RGBA: {
      static float default_value[] = {0.0f, 0.0f, 0.0f, 1.0f};
      bNodeSocketValueRGBA *dval = MEM_cnew<bNodeSocketValueRGBA>("node socket value color");
      copy_v4_v4(dval->value, default_value);

      *data = dval;
      break;
    }
    case SOCK_STRING: {
      bNodeSocketValueString *dval = MEM_cnew<bNodeSocketValueString>("node socket value string");
      dval->subtype = subtype;
      dval->value[0] = '\0';

      *data = dval;
      break;
    }
    case SOCK_MENU: {
      bNodeSocketValueMenu *dval = MEM_cnew<bNodeSocketValueMenu>("node socket value menu");
      dval->value = -1;

      *data = dval;
      break;
    }
    case SOCK_OBJECT: {
      bNodeSocketValueObject *dval = MEM_cnew<bNodeSocketValueObject>("node socket value object");
      dval->value = nullptr;

      *data = dval;
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *dval = MEM_cnew<bNodeSocketValueImage>("node socket value image");
      dval->value = nullptr;

      *data = dval;
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *dval = MEM_cnew<bNodeSocketValueCollection>(
          "node socket value object");
      dval->value = nullptr;

      *data = dval;
      break;
    }
    case SOCK_TEXTURE: {
      bNodeSocketValueTexture *dval = MEM_cnew<bNodeSocketValueTexture>(
          "node socket value texture");
      dval->value = nullptr;

      *data = dval;
      break;
    }
    case SOCK_MATERIAL: {
      bNodeSocketValueMaterial *dval = MEM_cnew<bNodeSocketValueMaterial>(
          "node socket value material");
      dval->value = nullptr;

      *data = dval;
      break;
    }

    case SOCK_CUSTOM:
    case SOCK_GEOMETRY:
    case SOCK_SHADER:
      break;
  }
}

void node_socket_copy_default_value_data(eNodeSocketDatatype datatype, void *to, const void *from)
{
  if (!to || !from) {
    return;
  }

  switch (datatype) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *toval = (bNodeSocketValueFloat *)to;
      bNodeSocketValueFloat *fromval = (bNodeSocketValueFloat *)from;
      *toval = *fromval;
      break;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *toval = (bNodeSocketValueInt *)to;
      bNodeSocketValueInt *fromval = (bNodeSocketValueInt *)from;
      *toval = *fromval;
      break;
    }
    case SOCK_BOOLEAN: {
      bNodeSocketValueBoolean *toval = (bNodeSocketValueBoolean *)to;
      bNodeSocketValueBoolean *fromval = (bNodeSocketValueBoolean *)from;
      *toval = *fromval;
      break;
    }
    case SOCK_VECTOR: {
      bNodeSocketValueVector *toval = (bNodeSocketValueVector *)to;
      bNodeSocketValueVector *fromval = (bNodeSocketValueVector *)from;
      *toval = *fromval;
      break;
    }
    case SOCK_RGBA: {
      bNodeSocketValueRGBA *toval = (bNodeSocketValueRGBA *)to;
      bNodeSocketValueRGBA *fromval = (bNodeSocketValueRGBA *)from;
      *toval = *fromval;
      break;
    }
    case SOCK_ROTATION: {
      bNodeSocketValueRotation *toval = (bNodeSocketValueRotation *)to;
      bNodeSocketValueRotation *fromval = (bNodeSocketValueRotation *)from;
      *toval = *fromval;
      break;
    }
    case SOCK_STRING: {
      bNodeSocketValueString *toval = (bNodeSocketValueString *)to;
      bNodeSocketValueString *fromval = (bNodeSocketValueString *)from;
      *toval = *fromval;
      break;
    }
    case SOCK_MENU: {
      bNodeSocketValueMenu *toval = (bNodeSocketValueMenu *)to;
      bNodeSocketValueMenu *fromval = (bNodeSocketValueMenu *)from;
      *toval = *fromval;
      break;
    }
    case SOCK_OBJECT: {
      bNodeSocketValueObject *toval = (bNodeSocketValueObject *)to;
      bNodeSocketValueObject *fromval = (bNodeSocketValueObject *)from;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *toval = (bNodeSocketValueImage *)to;
      bNodeSocketValueImage *fromval = (bNodeSocketValueImage *)from;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *toval = (bNodeSocketValueCollection *)to;
      bNodeSocketValueCollection *fromval = (bNodeSocketValueCollection *)from;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
    case SOCK_TEXTURE: {
      bNodeSocketValueTexture *toval = (bNodeSocketValueTexture *)to;
      bNodeSocketValueTexture *fromval = (bNodeSocketValueTexture *)from;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }
    case SOCK_MATERIAL: {
      bNodeSocketValueMaterial *toval = (bNodeSocketValueMaterial *)to;
      bNodeSocketValueMaterial *fromval = (bNodeSocketValueMaterial *)from;
      *toval = *fromval;
      id_us_plus(&toval->value->id);
      break;
    }

    case SOCK_CUSTOM:
    case SOCK_GEOMETRY:
    case SOCK_SHADER:
      break;
  }
}

void node_socket_init_default_value(bNodeSocket *sock)
{
  if (sock->default_value) {
    return; /* already initialized */
  }

  node_socket_init_default_value_data(eNodeSocketDatatype(sock->typeinfo->type),
                                      PropertySubType(sock->typeinfo->subtype),
                                      &sock->default_value);
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

  node_socket_copy_default_value_data(
      eNodeSocketDatatype(to->typeinfo->type), to->default_value, from->default_value);

  to->flag |= (from->flag & SOCK_HIDE_VALUE);
}

static void standard_node_socket_interface_init_socket(
    ID * /*id*/,
    const bNodeTreeInterfaceSocket *interface_socket,
    bNode * /*node*/,
    bNodeSocket *sock,
    const char * /*data_path*/)
{
  /* initialize the type value */
  sock->type = sock->typeinfo->type;

  node_socket_init_default_value_data(
      eNodeSocketDatatype(sock->type), sock->typeinfo->subtype, &sock->default_value);
  node_socket_copy_default_value_data(
      eNodeSocketDatatype(sock->type), sock->default_value, interface_socket->socket_data);
}

static void standard_node_socket_interface_from_socket(ID * /*id*/,
                                                       bNodeTreeInterfaceSocket *iosock,
                                                       const bNode * /*node*/,
                                                       const bNodeSocket *sock)
{
  /* initialize settings */
  iosock->init_from_socket_instance(sock);
}

void ED_init_standard_node_socket_type(bNodeSocketType *);

static bNodeSocketType *make_standard_socket_type(int type, int subtype)
{
  const char *socket_idname = nodeStaticSocketType(type, subtype);
  const char *interface_idname = nodeStaticSocketInterfaceTypeNew(type, subtype);
  const char *socket_label = nodeStaticSocketLabel(type, subtype);
  const char *socket_subtype_label = blender::bke::nodeSocketSubTypeLabel(subtype);
  bNodeSocketType *stype;
  StructRNA *srna;

  stype = MEM_cnew<bNodeSocketType>("node socket C type");
  stype->free_self = (void (*)(bNodeSocketType *stype))MEM_freeN;
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

void ED_init_node_socket_type_virtual(bNodeSocketType *);

static bNodeSocketType *make_socket_type_virtual()
{
  const char *socket_idname = "NodeSocketVirtual";
  bNodeSocketType *stype;
  StructRNA *srna;

  stype = MEM_cnew<bNodeSocketType>("node socket C type");
  stype->free_self = (void (*)(bNodeSocketType *stype))MEM_freeN;
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
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(bool *)r_value = ((bNodeSocketValueBoolean *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<SocketValueVariant>();
  socktype->get_geometry_nodes_cpp_value = [](const void *socket_value, void *r_value) {
    const bool value = ((bNodeSocketValueBoolean *)socket_value)->value;
    new (r_value) SocketValueVariant(value);
  };
  static SocketValueVariant default_value{false};
  socktype->geometry_nodes_default_cpp_value = &default_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_rotation()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_ROTATION, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<math::Quaternion>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    const auto &typed_value = *(bNodeSocketValueRotation *)socket_value;
    const math::EulerXYZ euler(float3(typed_value.value_euler));
    *static_cast<math::Quaternion *>(r_value) = math::to_quaternion(euler);
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<SocketValueVariant>();
  socktype->get_geometry_nodes_cpp_value = [](const void *socket_value, void *r_value) {
    const auto &typed_value = *(bNodeSocketValueRotation *)socket_value;
    const math::EulerXYZ euler(float3(typed_value.value_euler));
    const math::Quaternion value = math::to_quaternion(euler);
    new (r_value) SocketValueVariant(value);
  };
  static SocketValueVariant default_value{math::Quaternion::identity()};
  socktype->geometry_nodes_default_cpp_value = &default_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_float(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_FLOAT, subtype);
  socktype->base_cpp_type = &blender::CPPType::get<float>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(float *)r_value = ((bNodeSocketValueFloat *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<SocketValueVariant>();
  socktype->get_geometry_nodes_cpp_value = [](const void *socket_value, void *r_value) {
    const float value = ((bNodeSocketValueFloat *)socket_value)->value;
    new (r_value) SocketValueVariant(value);
  };
  static SocketValueVariant default_value{0.0f};
  socktype->geometry_nodes_default_cpp_value = &default_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_int(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_INT, subtype);
  socktype->base_cpp_type = &blender::CPPType::get<int>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(int *)r_value = ((bNodeSocketValueInt *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<SocketValueVariant>();
  socktype->get_geometry_nodes_cpp_value = [](const void *socket_value, void *r_value) {
    const int value = ((bNodeSocketValueInt *)socket_value)->value;
    new (r_value) SocketValueVariant(value);
  };
  static SocketValueVariant default_value{0};
  socktype->geometry_nodes_default_cpp_value = &default_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_vector(PropertySubType subtype)
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_VECTOR, subtype);
  socktype->base_cpp_type = &blender::CPPType::get<blender::float3>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(blender::float3 *)r_value = ((bNodeSocketValueVector *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<SocketValueVariant>();
  socktype->get_geometry_nodes_cpp_value = [](const void *socket_value, void *r_value) {
    const blender::float3 value = ((bNodeSocketValueVector *)socket_value)->value;
    new (r_value) SocketValueVariant(value);
  };
  static SocketValueVariant default_value{blender::float3(0, 0, 0)};
  socktype->geometry_nodes_default_cpp_value = &default_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_rgba()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_RGBA, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<blender::ColorGeometry4f>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(blender::ColorGeometry4f *)r_value = ((bNodeSocketValueRGBA *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<SocketValueVariant>();
  socktype->get_geometry_nodes_cpp_value = [](const void *socket_value, void *r_value) {
    const blender::ColorGeometry4f value = ((bNodeSocketValueRGBA *)socket_value)->value;
    new (r_value) SocketValueVariant(value);
  };
  static SocketValueVariant default_value{blender::ColorGeometry4f(0, 0, 0, 0)};
  socktype->geometry_nodes_default_cpp_value = &default_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_string()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_STRING, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<std::string>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    new (r_value) std::string(((bNodeSocketValueString *)socket_value)->value);
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<SocketValueVariant>();
  socktype->get_geometry_nodes_cpp_value = [](const void *socket_value, void *r_value) {
    std::string value = ((bNodeSocketValueString *)socket_value)->value;
    new (r_value) SocketValueVariant(value);
  };
  static SocketValueVariant default_value{std::string()};
  socktype->geometry_nodes_default_cpp_value = &default_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_menu()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_MENU, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<int>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(int *)r_value = ((bNodeSocketValueMenu *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = &blender::CPPType::get<SocketValueVariant>();
  socktype->get_geometry_nodes_cpp_value = [](const void *socket_value, void *r_value) {
    const int value = ((bNodeSocketValueMenu *)socket_value)->value;
    new (r_value) SocketValueVariant(value);
  };
  static SocketValueVariant default_value{0};
  socktype->geometry_nodes_default_cpp_value = &default_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_object()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_OBJECT, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Object *>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(Object **)r_value = ((bNodeSocketValueObject *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_geometry()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_GEOMETRY, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<blender::bke::GeometrySet>();
  socktype->get_base_cpp_value = [](const void * /*socket_value*/, void *r_value) {
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
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(Collection **)r_value = ((bNodeSocketValueCollection *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_texture()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_TEXTURE, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Tex *>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(Tex **)r_value = ((bNodeSocketValueTexture *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_image()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_IMAGE, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Image *>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(Image **)r_value = ((bNodeSocketValueImage *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

static bNodeSocketType *make_socket_type_material()
{
  bNodeSocketType *socktype = make_standard_socket_type(SOCK_MATERIAL, PROP_NONE);
  socktype->base_cpp_type = &blender::CPPType::get<Material *>();
  socktype->get_base_cpp_value = [](const void *socket_value, void *r_value) {
    *(Material **)r_value = ((bNodeSocketValueMaterial *)socket_value)->value;
  };
  socktype->geometry_nodes_cpp_type = socktype->base_cpp_type;
  socktype->get_geometry_nodes_cpp_value = socktype->get_base_cpp_value;
  return socktype;
}

void register_standard_node_socket_types()
{
  /* Draw callbacks are set in `drawnode.cc` to avoid bad-level calls. */

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

  nodeRegisterSocketType(make_socket_type_menu());

  nodeRegisterSocketType(make_standard_socket_type(SOCK_SHADER, PROP_NONE));

  nodeRegisterSocketType(make_socket_type_object());

  nodeRegisterSocketType(make_socket_type_geometry());

  nodeRegisterSocketType(make_socket_type_collection());

  nodeRegisterSocketType(make_socket_type_texture());

  nodeRegisterSocketType(make_socket_type_image());

  nodeRegisterSocketType(make_socket_type_material());

  nodeRegisterSocketType(make_socket_type_virtual());
}
