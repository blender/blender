/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_node_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "WM_api.hh"

const EnumPropertyItem rna_enum_node_socket_type_items[] = {
    {SOCK_CUSTOM, "CUSTOM", 0, "Custom", ""},
    {SOCK_FLOAT, "VALUE", 0, "Value", ""},
    {SOCK_INT, "INT", 0, "Integer", ""},
    {SOCK_BOOLEAN, "BOOLEAN", 0, "Boolean", ""},
    {SOCK_VECTOR, "VECTOR", 0, "Vector", ""},
    {SOCK_ROTATION, "ROTATION", 0, "Rotation", ""},
    {SOCK_STRING, "STRING", 0, "String", ""},
    {SOCK_RGBA, "RGBA", 0, "RGBA", ""},
    {SOCK_SHADER, "SHADER", 0, "Shader", ""},
    {SOCK_OBJECT, "OBJECT", 0, "Object", ""},
    {SOCK_IMAGE, "IMAGE", 0, "Image", ""},
    {SOCK_GEOMETRY, "GEOMETRY", 0, "Geometry", ""},
    {SOCK_COLLECTION, "COLLECTION", 0, "Collection", ""},
    {SOCK_TEXTURE, "TEXTURE", 0, "Texture", ""},
    {SOCK_MATERIAL, "MATERIAL", 0, "Material", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "DNA_material_types.h"

#  include "BLI_math.h"

#  include "BKE_node.h"
#  include "BKE_node_tree_update.h"

#  include "DEG_depsgraph_build.h"

#  include "ED_node.hh"

extern "C" {
extern FunctionRNA rna_NodeSocket_draw_func;
extern FunctionRNA rna_NodeSocket_draw_color_func;
extern FunctionRNA rna_NodeSocketInterface_draw_func;
extern FunctionRNA rna_NodeSocketInterface_draw_color_func;
extern FunctionRNA rna_NodeSocketInterface_init_socket_func;
extern FunctionRNA rna_NodeSocketInterface_from_socket_func;
}

/* ******** Node Socket ******** */

static void rna_NodeSocket_draw(
    bContext *C, uiLayout *layout, PointerRNA *ptr, PointerRNA *node_ptr, const char *text)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  ParameterList list;
  FunctionRNA *func;

  func = &rna_NodeSocket_draw_func; /* RNA_struct_find_function(&ptr, "draw"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  RNA_parameter_set_lookup(&list, "node", node_ptr);
  RNA_parameter_set_lookup(&list, "text", &text);
  sock->typeinfo->ext_socket.call(C, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeSocket_draw_color(bContext *C,
                                      PointerRNA *ptr,
                                      PointerRNA *node_ptr,
                                      float *r_color)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  ParameterList list;
  FunctionRNA *func;
  void *ret;

  func = &rna_NodeSocket_draw_color_func; /* RNA_struct_find_function(&ptr, "draw_color"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "node", node_ptr);
  sock->typeinfo->ext_socket.call(C, ptr, func, &list);

  RNA_parameter_get_lookup(&list, "color", &ret);
  copy_v4_v4(r_color, static_cast<float *>(ret));

  RNA_parameter_list_free(&list);
}

static bool rna_NodeSocket_unregister(Main * /*bmain*/, StructRNA *type)
{
  bNodeSocketType *st = static_cast<bNodeSocketType *>(RNA_struct_blender_type_get(type));
  if (!st) {
    return false;
  }

  RNA_struct_free_extension(type, &st->ext_socket);
  RNA_struct_free(&BLENDER_RNA, type);

  nodeUnregisterSocketType(st);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  return true;
}

static StructRNA *rna_NodeSocket_register(Main * /*bmain*/,
                                          ReportList *reports,
                                          void *data,
                                          const char *identifier,
                                          StructValidateFunc validate,
                                          StructCallbackFunc call,
                                          StructFreeFunc free)
{
  bNodeSocketType *st, dummy_st;
  bNodeSocket dummy_sock;
  PointerRNA dummy_sock_ptr;
  bool have_function[2];

  /* setup dummy socket & socket type to store static properties in */
  memset(&dummy_st, 0, sizeof(bNodeSocketType));
  dummy_st.type = SOCK_CUSTOM;

  memset(&dummy_sock, 0, sizeof(bNodeSocket));
  dummy_sock.typeinfo = &dummy_st;
  RNA_pointer_create(nullptr, &RNA_NodeSocket, &dummy_sock, &dummy_sock_ptr);

  /* validate the python class */
  if (validate(&dummy_sock_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_st.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering node socket class: '%s' is too long, maximum length is %d",
                identifier,
                int(sizeof(dummy_st.idname)));
    return nullptr;
  }

  /* check if we have registered this socket type before */
  st = nodeSocketTypeFind(dummy_st.idname);
  if (!st) {
    /* create a new node socket type */
    st = static_cast<bNodeSocketType *>(MEM_mallocN(sizeof(bNodeSocketType), "node socket type"));
    memcpy(st, &dummy_st, sizeof(dummy_st));

    nodeRegisterSocketType(st);
  }

  st->free_self = (void (*)(bNodeSocketType * stype)) MEM_freeN;

  /* if RNA type is already registered, unregister first */
  if (st->ext_socket.srna) {
    StructRNA *srna = st->ext_socket.srna;
    RNA_struct_free_extension(srna, &st->ext_socket);
    RNA_struct_free(&BLENDER_RNA, srna);
  }
  st->ext_socket.srna = RNA_def_struct_ptr(&BLENDER_RNA, st->idname, &RNA_NodeSocket);
  st->ext_socket.data = data;
  st->ext_socket.call = call;
  st->ext_socket.free = free;
  RNA_struct_blender_type_set(st->ext_socket.srna, st);

  /* XXX bad level call! needed to initialize the basic draw functions ... */
  ED_init_custom_node_socket_type(st);

  st->draw = (have_function[0]) ? rna_NodeSocket_draw : nullptr;
  st->draw_color = (have_function[1]) ? rna_NodeSocket_draw_color : nullptr;

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return st->ext_socket.srna;
}

static StructRNA *rna_NodeSocket_refine(PointerRNA *ptr)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);

  if (sock->typeinfo->ext_socket.srna) {
    return sock->typeinfo->ext_socket.srna;
  }
  else {
    return &RNA_NodeSocket;
  }
}

static char *rna_NodeSocket_path(const PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  bNode *node;
  int socketindex;
  char name_esc[sizeof(node->name) * 2];

  nodeFindNode(ntree, sock, &node, &socketindex);

  BLI_str_escape(name_esc, node->name, sizeof(name_esc));

  if (sock->in_out == SOCK_IN) {
    return BLI_sprintfN("nodes[\"%s\"].inputs[%d]", name_esc, socketindex);
  }
  else {
    return BLI_sprintfN("nodes[\"%s\"].outputs[%d]", name_esc, socketindex);
  }
}

static IDProperty **rna_NodeSocket_idprops(PointerRNA *ptr)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  return &sock->prop;
}

static PointerRNA rna_NodeSocket_node_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  bNode *node;
  PointerRNA r_ptr;

  nodeFindNode(ntree, sock, &node, nullptr);

  RNA_pointer_create(&ntree->id, &RNA_Node, node, &r_ptr);
  return r_ptr;
}

static void rna_NodeSocket_type_set(PointerRNA *ptr, int value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  bNode *node;
  nodeFindNode(ntree, sock, &node, nullptr);
  nodeModifySocketTypeStatic(ntree, node, sock, value, 0);
}

static void rna_NodeSocket_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);

  BKE_ntree_update_tag_socket_property(ntree, sock);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

static bool rna_NodeSocket_is_output_get(PointerRNA *ptr)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  return sock->in_out == SOCK_OUT;
}

static void rna_NodeSocket_link_limit_set(PointerRNA *ptr, int value)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  sock->limit = (value == 0 ? 0xFFF : value);
}

static void rna_NodeSocket_hide_set(PointerRNA *ptr, bool value)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);

  /* don't hide linked sockets */
  if (sock->flag & SOCK_IS_LINKED) {
    return;
  }

  if (value) {
    sock->flag |= SOCK_HIDDEN;
  }
  else {
    sock->flag &= ~SOCK_HIDDEN;
  }
}

static void rna_NodeSocketInterface_draw(bContext *C, uiLayout *layout, PointerRNA *ptr)
{
  bNodeSocket *stemp = static_cast<bNodeSocket *>(ptr->data);
  ParameterList list;
  FunctionRNA *func;

  if (!stemp->typeinfo) {
    return;
  }

  func = &rna_NodeSocketInterface_draw_func; /* RNA_struct_find_function(&ptr, "draw"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  stemp->typeinfo->ext_interface.call(C, ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeSocketInterface_draw_color(bContext *C, PointerRNA *ptr, float *r_color)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  ParameterList list;
  FunctionRNA *func;
  void *ret;

  if (!sock->typeinfo) {
    return;
  }

  func =
      &rna_NodeSocketInterface_draw_color_func; /* RNA_struct_find_function(&ptr, "draw_color"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  sock->typeinfo->ext_interface.call(C, ptr, func, &list);

  RNA_parameter_get_lookup(&list, "color", &ret);
  copy_v4_v4(r_color, static_cast<float *>(ret));

  RNA_parameter_list_free(&list);
}

static void rna_NodeSocketInterface_init_socket(bNodeTree *ntree,
                                                const bNodeSocket *interface_socket,
                                                bNode *node,
                                                bNodeSocket *sock,
                                                const char *data_path)
{
  PointerRNA ptr, node_ptr, sock_ptr;
  ParameterList list;
  FunctionRNA *func;

  if (!interface_socket->typeinfo) {
    return;
  }

  RNA_pointer_create(
      &ntree->id, &RNA_NodeSocketInterface, const_cast<bNodeSocket *>(interface_socket), &ptr);
  RNA_pointer_create(&ntree->id, &RNA_Node, node, &node_ptr);
  RNA_pointer_create(&ntree->id, &RNA_NodeSocket, sock, &sock_ptr);
  // RNA_struct_find_function(&ptr, "init_socket");
  func = &rna_NodeSocketInterface_init_socket_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node", &node_ptr);
  RNA_parameter_set_lookup(&list, "socket", &sock_ptr);
  RNA_parameter_set_lookup(&list, "data_path", &data_path);
  interface_socket->typeinfo->ext_interface.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeSocketInterface_from_socket(bNodeTree *ntree,
                                                bNodeSocket *interface_socket,
                                                const bNode *node,
                                                const bNodeSocket *sock)
{
  PointerRNA ptr, node_ptr, sock_ptr;
  ParameterList list;
  FunctionRNA *func;

  if (!interface_socket->typeinfo) {
    return;
  }

  RNA_pointer_create(&ntree->id, &RNA_NodeSocketInterface, interface_socket, &ptr);
  RNA_pointer_create(&ntree->id, &RNA_Node, const_cast<bNode *>(node), &node_ptr);
  RNA_pointer_create(&ntree->id, &RNA_NodeSocket, const_cast<bNodeSocket *>(sock), &sock_ptr);
  // RNA_struct_find_function(&ptr, "from_socket");
  func = &rna_NodeSocketInterface_from_socket_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node", &node_ptr);
  RNA_parameter_set_lookup(&list, "socket", &sock_ptr);
  interface_socket->typeinfo->ext_interface.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static bool rna_NodeSocketInterface_unregister(Main * /*bmain*/, StructRNA *type)
{
  bNodeSocketType *st = static_cast<bNodeSocketType *>(RNA_struct_blender_type_get(type));
  if (!st) {
    return false;
  }

  RNA_struct_free_extension(type, &st->ext_interface);

  RNA_struct_free(&BLENDER_RNA, type);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  return true;
}

static StructRNA *rna_NodeSocketInterface_register(Main * /*bmain*/,
                                                   ReportList * /*reports*/,
                                                   void *data,
                                                   const char *identifier,
                                                   StructValidateFunc validate,
                                                   StructCallbackFunc call,
                                                   StructFreeFunc free)
{
  bNodeSocketType *st, dummy_st;
  bNodeSocket dummy_sock;
  PointerRNA dummy_sock_ptr;
  bool have_function[4];

  /* setup dummy socket & socket type to store static properties in */
  memset(&dummy_st, 0, sizeof(bNodeSocketType));

  memset(&dummy_sock, 0, sizeof(bNodeSocket));
  dummy_sock.typeinfo = &dummy_st;
  RNA_pointer_create(nullptr, &RNA_NodeSocketInterface, &dummy_sock, &dummy_sock_ptr);

  /* validate the python class */
  if (validate(&dummy_sock_ptr, data, have_function) != 0) {
    return nullptr;
  }

  /* check if we have registered this socket type before */
  st = nodeSocketTypeFind(dummy_st.idname);
  if (st) {
    /* basic socket type registered by a socket class before. */
  }
  else {
    /* create a new node socket type */
    st = static_cast<bNodeSocketType *>(MEM_mallocN(sizeof(bNodeSocketType), "node socket type"));
    memcpy(st, &dummy_st, sizeof(dummy_st));

    nodeRegisterSocketType(st);
  }

  st->free_self = (void (*)(bNodeSocketType * stype)) MEM_freeN;

  /* if RNA type is already registered, unregister first */
  if (st->ext_interface.srna) {
    StructRNA *srna = st->ext_interface.srna;
    RNA_struct_free_extension(srna, &st->ext_interface);
    RNA_struct_free(&BLENDER_RNA, srna);
  }
  st->ext_interface.srna = RNA_def_struct_ptr(&BLENDER_RNA, identifier, &RNA_NodeSocketInterface);
  st->ext_interface.data = data;
  st->ext_interface.call = call;
  st->ext_interface.free = free;
  RNA_struct_blender_type_set(st->ext_interface.srna, st);

  st->interface_draw = (have_function[0]) ? rna_NodeSocketInterface_draw : nullptr;
  st->interface_draw_color = (have_function[1]) ? rna_NodeSocketInterface_draw_color : nullptr;
  st->interface_init_socket = (have_function[2]) ? rna_NodeSocketInterface_init_socket : nullptr;
  st->interface_from_socket = (have_function[3]) ? rna_NodeSocketInterface_from_socket : nullptr;

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return st->ext_interface.srna;
}

static StructRNA *rna_NodeSocketInterface_refine(PointerRNA *ptr)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);

  if (sock->typeinfo && sock->typeinfo->ext_interface.srna) {
    return sock->typeinfo->ext_interface.srna;
  }
  else {
    return &RNA_NodeSocketInterface;
  }
}

static char *rna_NodeSocketInterface_path(const PointerRNA *ptr)
{
  const bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  const bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  int socketindex;

  socketindex = BLI_findindex(&ntree->inputs, sock);
  if (socketindex != -1) {
    return BLI_sprintfN("inputs[%d]", socketindex);
  }

  socketindex = BLI_findindex(&ntree->outputs, sock);
  if (socketindex != -1) {
    return BLI_sprintfN("outputs[%d]", socketindex);
  }

  return nullptr;
}

static IDProperty **rna_NodeSocketInterface_idprops(PointerRNA *ptr)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  return &sock->prop;
}

static void rna_NodeSocketInterface_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *stemp = static_cast<bNodeSocket *>(ptr->data);

  if (!stemp->typeinfo) {
    return;
  }

  BKE_ntree_update_tag_interface(ntree);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

/* ******** Standard Node Socket Base Types ******** */

static void rna_NodeSocketStandard_draw(ID *id,
                                        bNodeSocket *sock,
                                        bContext *C,
                                        uiLayout *layout,
                                        PointerRNA *nodeptr,
                                        const char *text)
{
  PointerRNA ptr;
  RNA_pointer_create(id, &RNA_NodeSocket, sock, &ptr);
  sock->typeinfo->draw(C, layout, &ptr, nodeptr, text);
}

static void rna_NodeSocketStandard_draw_color(
    ID *id, bNodeSocket *sock, bContext *C, PointerRNA *nodeptr, float r_color[4])
{
  PointerRNA ptr;
  RNA_pointer_create(id, &RNA_NodeSocket, sock, &ptr);
  sock->typeinfo->draw_color(C, &ptr, nodeptr, r_color);
}

static void rna_NodeSocketInterfaceStandard_draw(ID *id,
                                                 bNodeSocket *sock,
                                                 bContext *C,
                                                 uiLayout *layout)
{
  PointerRNA ptr;
  RNA_pointer_create(id, &RNA_NodeSocketInterface, sock, &ptr);
  sock->typeinfo->interface_draw(C, layout, &ptr);
}

static void rna_NodeSocketInterfaceStandard_draw_color(ID *id,
                                                       bNodeSocket *sock,
                                                       bContext *C,
                                                       float r_color[4])
{
  PointerRNA ptr;
  RNA_pointer_create(id, &RNA_NodeSocketInterface, sock, &ptr);
  sock->typeinfo->interface_draw_color(C, &ptr, r_color);
}

static void rna_NodeSocketStandard_float_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  bNodeSocketValueFloat *dval = static_cast<bNodeSocketValueFloat *>(sock->default_value);
  int subtype = sock->typeinfo->subtype;

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = (subtype == PROP_UNSIGNED ? 0.0f : -FLT_MAX);
  *max = FLT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

static void rna_NodeSocketStandard_int_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  bNodeSocketValueInt *dval = static_cast<bNodeSocketValueInt *>(sock->default_value);
  int subtype = sock->typeinfo->subtype;

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = (subtype == PROP_UNSIGNED ? 0 : INT_MIN);
  *max = INT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

static void rna_NodeSocketStandard_vector_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  bNodeSocketValueVector *dval = static_cast<bNodeSocketValueVector *>(sock->default_value);

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = -FLT_MAX;
  *max = FLT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

/* using a context update function here, to avoid searching the node if possible */
static void rna_NodeSocketStandard_value_update(bContext *C, PointerRNA *ptr)
{
  /* default update */
  rna_NodeSocket_update(CTX_data_main(C), CTX_data_scene(C), ptr);
}

static void rna_NodeSocketStandard_value_and_relation_update(bContext *C, PointerRNA *ptr)
{
  rna_NodeSocketStandard_value_update(C, ptr);
  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
}

/* ******** Node Socket Subtypes ******** */

bool rna_NodeSocketMaterial_default_value_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  /* Do not show grease pencil materials for now. */
  Material *ma = static_cast<Material *>(value.data);
  return ma->gp_style == nullptr;
}

#else

static void rna_def_node_socket(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  static const EnumPropertyItem rna_enum_node_socket_display_shape_items[] = {
      {SOCK_DISPLAY_SHAPE_CIRCLE, "CIRCLE", 0, "Circle", ""},
      {SOCK_DISPLAY_SHAPE_SQUARE, "SQUARE", 0, "Square", ""},
      {SOCK_DISPLAY_SHAPE_DIAMOND, "DIAMOND", 0, "Diamond", ""},
      {SOCK_DISPLAY_SHAPE_CIRCLE_DOT, "CIRCLE_DOT", 0, "Circle with inner dot", ""},
      {SOCK_DISPLAY_SHAPE_SQUARE_DOT, "SQUARE_DOT", 0, "Square with inner dot", ""},
      {SOCK_DISPLAY_SHAPE_DIAMOND_DOT, "DIAMOND_DOT", 0, "Diamond with inner dot", ""},
      {0, nullptr, 0, nullptr, nullptr}};

  static float default_draw_color[] = {0.0f, 0.0f, 0.0f, 1.0f};

  srna = RNA_def_struct(brna, "NodeSocket", nullptr);
  RNA_def_struct_ui_text(srna, "Node Socket", "Input or output socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");
  RNA_def_struct_refine_func(srna, "rna_NodeSocket_refine");
  RNA_def_struct_ui_icon(srna, ICON_NONE);
  RNA_def_struct_path_func(srna, "rna_NodeSocket_path");
  RNA_def_struct_register_funcs(
      srna, "rna_NodeSocket_register", "rna_NodeSocket_unregister", nullptr);
  RNA_def_struct_idprops_func(srna, "rna_NodeSocket_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Socket name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

  prop = RNA_def_property(srna, "label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "label");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Label", "Custom dynamic defined socket label");

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "identifier");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Identifier", "Unique identifier for mapping sockets");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_ui_text(prop, "Tooltip", "Socket tooltip");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

  prop = RNA_def_property(srna, "is_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_NodeSocket_is_output_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Is Output", "True if the socket is an output, otherwise input");

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_HIDDEN);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_NodeSocket_hide_set");
  RNA_def_property_ui_text(prop, "Hide", "Hide the socket");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SOCK_UNAVAIL);
  RNA_def_property_ui_text(prop, "Enabled", "Enable the socket");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "link_limit", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "limit");
  RNA_def_property_int_funcs(prop, nullptr, "rna_NodeSocket_link_limit_set", nullptr);
  RNA_def_property_range(prop, 1, 0xFFF);
  RNA_def_property_ui_text(prop, "Link Limit", "Max number of links allowed for this socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "is_linked", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_IS_LINKED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Linked", "True if the socket is connected");

  prop = RNA_def_property(srna, "is_unavailable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_UNAVAIL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Unavailable", "True if the socket is unavailable");

  prop = RNA_def_property(srna, "is_multi_input", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_MULTI_INPUT);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Multi Input", "True if the socket can accept multiple ordered input links");

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SOCK_COLLAPSED);
  RNA_def_property_ui_text(prop, "Expanded", "Socket links are expanded in the user interface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "hide_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_HIDE_VALUE);
  RNA_def_property_ui_text(prop, "Hide Value", "Hide the socket input value");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "node", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop, "rna_NodeSocket_node_get", nullptr, nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Node");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "Node", "Node owning this socket");

  /* NOTE: The type property is used by standard sockets.
   * Ideally should be defined only for the registered subclass,
   * but to use the existing DNA is added in the base type here.
   * Future socket types can ignore or override this if needed. */

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, rna_enum_node_socket_type_items);
  RNA_def_property_enum_default(prop, SOCK_FLOAT);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_NodeSocket_type_set", nullptr);
  RNA_def_property_ui_text(prop, "Type", "Data type");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

  prop = RNA_def_property(srna, "display_shape", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "display_shape");
  RNA_def_property_enum_items(prop, rna_enum_node_socket_display_shape_items);
  RNA_def_property_enum_default(prop, SOCK_DISPLAY_SHAPE_CIRCLE);
  RNA_def_property_ui_text(prop, "Shape", "Socket shape");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

  /* registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", "");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->label");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Type Label", "Label to display for the socket type in the UI");

  prop = RNA_def_property(srna, "bl_subtype_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->subtype_label");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop, "Subtype Label", "Label to display for the socket subtype in the UI");

  /* draw socket */
  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "Draw socket");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "Node");
  RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_property(func, "text", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(parm, "Text", "Text label to draw alongside properties");
  // RNA_def_property_string_default(parm, "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "draw_color", nullptr);
  RNA_def_function_ui_description(func, "Color of the socket icon");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "Node");
  RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_float_array(
      func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
  RNA_def_function_output(func, parm);
}

static void rna_def_node_socket_interface(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  static float default_draw_color[] = {0.0f, 0.0f, 0.0f, 1.0f};

  srna = RNA_def_struct(brna, "NodeSocketInterface", nullptr);
  RNA_def_struct_ui_text(srna, "Node Socket Template", "Parameters to define node sockets");
  /* XXX Using bNodeSocket DNA for templates is a compatibility hack.
   * This allows to keep the inputs/outputs lists in bNodeTree working for earlier versions
   * and at the same time use them for socket templates in groups.
   */
  RNA_def_struct_sdna(srna, "bNodeSocket");
  RNA_def_struct_refine_func(srna, "rna_NodeSocketInterface_refine");
  RNA_def_struct_path_func(srna, "rna_NodeSocketInterface_path");
  RNA_def_struct_idprops_func(srna, "rna_NodeSocketInterface_idprops");
  RNA_def_struct_register_funcs(
      srna, "rna_NodeSocketInterface_register", "rna_NodeSocketInterface_unregister", nullptr);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Socket name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "identifier");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Identifier", "Unique identifier for mapping sockets");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_ui_text(prop, "Tooltip", "Socket tooltip");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "is_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_NodeSocket_is_output_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Is Output", "True if the socket is an output, otherwise input");

  prop = RNA_def_property(srna, "hide_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_HIDE_VALUE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Hide Value", "Hide the socket input value even when the socket is not connected");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "hide_in_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_HIDE_IN_MODIFIER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Hide in Modifier",
                           "Don't show the input value in the geometry nodes modifier interface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "attribute_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_ui_text(
      prop,
      "Attribute Domain",
      "Attribute domain used by the geometry nodes modifier to create an attribute output");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "default_attribute_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "default_attribute_name");
  RNA_def_property_ui_text(prop,
                           "Default Attribute",
                           "The attribute name used by default when the node group is used by a "
                           "geometry nodes modifier");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  /* registration */
  prop = RNA_def_property(srna, "bl_socket_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", "");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->label");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Type Label", "Label to display for the socket type in the UI");

  prop = RNA_def_property(srna, "bl_subtype_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "typeinfo->subtype_label");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop, "Subtype Label", "Label to display for the socket subtype in the UI");

  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "Draw template settings");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "draw_color", nullptr);
  RNA_def_function_ui_description(func, "Color of the socket icon");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_float_array(
      func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "init_socket", nullptr);
  RNA_def_function_ui_description(func, "Initialize a node socket instance");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the socket to initialize");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Socket to initialize");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func, "data_path", nullptr, 0, "Data Path", "Path to specialized socket data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "from_socket", nullptr);
  RNA_def_function_ui_description(func, "Setup template parameters from an existing socket");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the original socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Original socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
}

static void rna_def_node_socket_float(BlenderRNA *brna,
                                      const char *idname,
                                      const char *interface_idname,
                                      PropertySubType subtype)
{
  StructRNA *srna;
  PropertyRNA *prop;
  float value_default;

  /* choose sensible common default based on subtype */
  switch (subtype) {
    case PROP_FACTOR:
      value_default = 1.0f;
      break;
    case PROP_PERCENTAGE:
      value_default = 100.0f;
      break;
    default:
      value_default = 0.0f;
      break;
  }

  srna = RNA_def_struct(brna, idname, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Float Node Socket", "Floating-point number socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueFloat", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_float_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(
      srna, "Float Node Socket Interface", "Floating-point number socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueFloat", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_float_default(prop, value_default);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_float_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "min_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "min");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "max_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_int(BlenderRNA *brna,
                                    const char *identifier,
                                    const char *interface_idname,
                                    PropertySubType subtype)
{
  StructRNA *srna;
  PropertyRNA *prop;
  int value_default;

  /* choose sensible common default based on subtype */
  switch (subtype) {
    case PROP_FACTOR:
      value_default = 1;
      break;
    case PROP_PERCENTAGE:
      value_default = 100;
      break;
    default:
      value_default = 0;
      break;
  }

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Integer Node Socket", "Integer number socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueInt", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_INT, subtype);
  RNA_def_property_int_sdna(prop, nullptr, "value");
  RNA_def_property_int_default(prop, value_default);
  RNA_def_property_int_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_int_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Integer Node Socket Interface", "Integer number socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueInt", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_INT, subtype);
  RNA_def_property_int_sdna(prop, nullptr, "value");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_int_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "min_value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "min");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "max_value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "max");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_bool(BlenderRNA *brna,
                                     const char *identifier,
                                     const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Boolean Node Socket", "Boolean value socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueBoolean", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "value", 1);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Boolean Node Socket Interface", "Boolean value socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueBoolean", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "value", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_rotation(BlenderRNA *brna,
                                         const char *identifier,
                                         const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Rotation Node Socket", "Rotation value socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueRotation", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "value_euler");
  // RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(
      srna, "Rotation Node Socket Interface", "Rotation value socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueRotation", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "value_euler");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_vector(BlenderRNA *brna,
                                       const char *identifier,
                                       const char *interface_idname,
                                       PropertySubType subtype)
{
  StructRNA *srna;
  PropertyRNA *prop;
  const float *value_default;

  /* choose sensible common default based on subtype */
  switch (subtype) {
    case PROP_DIRECTION: {
      static const float default_direction[3] = {0.0f, 0.0f, 1.0f};
      value_default = default_direction;
      break;
    }
    default: {
      static const float default_vector[3] = {0.0f, 0.0f, 0.0f};
      value_default = default_vector;
      break;
    }
  }

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Vector Node Socket", "3D vector socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueVector", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_float_array_default(prop, value_default);
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_vector_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Vector Node Socket Interface", "3D vector socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueVector", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_vector_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "min_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "min");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  prop = RNA_def_property(srna, "max_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_color(BlenderRNA *brna,
                                      const char *identifier,
                                      const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Color Node Socket", "RGBA color socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueRGBA", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Color Node Socket Interface", "RGBA color socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueRGBA", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_string(BlenderRNA *brna,
                                       const char *identifier,
                                       const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "String Node Socket", "String socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueString", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "value");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "String Node Socket Interface", "String socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueString", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "value");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_shader(BlenderRNA *brna,
                                       const char *identifier,
                                       const char *interface_idname)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Shader Node Socket", "Shader socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Shader Node Socket Interface", "Shader socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_virtual(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Virtual Node Socket", "Virtual socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_object(BlenderRNA *brna,
                                       const char *identifier,
                                       const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Object Node Socket", "Object socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueObject", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Object Node Socket Interface", "Object socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueObject", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_image(BlenderRNA *brna,
                                      const char *identifier,
                                      const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Image Node Socket", "Image socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueImage", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Image Node Socket Interface", "Image socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueImage", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_geometry(BlenderRNA *brna,
                                         const char *identifier,
                                         const char *interface_idname)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Geometry Node Socket", "Geometry socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Geometry Node Socket Interface", "Geometry socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_collection(BlenderRNA *brna,
                                           const char *identifier,
                                           const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Collection Node Socket", "Collection socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueCollection", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Collection Node Socket Interface", "Collection socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueCollection", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_texture(BlenderRNA *brna,
                                        const char *identifier,
                                        const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Texture Node Socket", "Texture socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueTexture", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Texture Node Socket Interface", "Texture socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueTexture", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_material(BlenderRNA *brna,
                                         const char *identifier,
                                         const char *interface_idname)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Material Node Socket", "Material socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueMaterial", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_NodeSocketMaterial_default_value_poll");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* socket interface */
  srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
  RNA_def_struct_ui_text(srna, "Material Node Socket Interface", "Material socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueMaterial", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_NodeSocketMaterial_default_value_poll");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_standard_types(BlenderRNA *brna)
{
  /* XXX Workaround: Registered functions are not exposed in python by bpy,
   * it expects them to be registered from python and use the native implementation.
   * However, the standard socket types below are not registering these functions from python,
   * so in order to call them in py scripts we need to overload and
   * replace them with plain C callbacks.
   * These types provide a usable basis for socket types defined in C.
   */

  StructRNA *srna;
  PropertyRNA *parm, *prop;
  FunctionRNA *func;

  static float default_draw_color[] = {0.0f, 0.0f, 0.0f, 1.0f};

  srna = RNA_def_struct(brna, "NodeSocketStandard", "NodeSocket");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  /* draw socket */
  func = RNA_def_function(srna, "draw", "rna_NodeSocketStandard_draw");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Draw socket");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "Node");
  RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_property(func, "text", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(parm, "Text", "Text label to draw alongside properties");
  // RNA_def_property_string_default(parm, "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "draw_color", "rna_NodeSocketStandard_draw_color");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Color of the socket icon");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "Node");
  RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_float_array(
      func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
  RNA_def_function_output(func, parm);

  srna = RNA_def_struct(brna, "NodeSocketInterfaceStandard", "NodeSocketInterface");
  RNA_def_struct_sdna(srna, "bNodeSocket");

  /* for easier type comparison in python */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "typeinfo->type");
  RNA_def_property_enum_items(prop, rna_enum_node_socket_type_items);
  RNA_def_property_enum_default(prop, SOCK_FLOAT);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Type", "Data type");

  func = RNA_def_function(srna, "draw", "rna_NodeSocketInterfaceStandard_draw");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Draw template settings");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "draw_color", "rna_NodeSocketInterfaceStandard_draw_color");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Color of the socket icon");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_float_array(
      func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
  RNA_def_function_output(func, parm);

  /* XXX These types should eventually be registered at runtime.
   * Then use the nodeStaticSocketType and nodeStaticSocketInterfaceType functions
   * to get the idname strings from int type and subtype
   * (see node_socket.cc, register_standard_node_socket_types).
   */

  rna_def_node_socket_float(brna, "NodeSocketFloat", "NodeSocketInterfaceFloat", PROP_NONE);
  rna_def_node_socket_float(
      brna, "NodeSocketFloatUnsigned", "NodeSocketInterfaceFloatUnsigned", PROP_UNSIGNED);
  rna_def_node_socket_float(
      brna, "NodeSocketFloatPercentage", "NodeSocketInterfaceFloatPercentage", PROP_PERCENTAGE);
  rna_def_node_socket_float(
      brna, "NodeSocketFloatFactor", "NodeSocketInterfaceFloatFactor", PROP_FACTOR);
  rna_def_node_socket_float(
      brna, "NodeSocketFloatAngle", "NodeSocketInterfaceFloatAngle", PROP_ANGLE);
  rna_def_node_socket_float(
      brna, "NodeSocketFloatTime", "NodeSocketInterfaceFloatTime", PROP_TIME);
  rna_def_node_socket_float(brna,
                            "NodeSocketFloatTimeAbsolute",
                            "NodeSocketInterfaceFloatTimeAbsolute",
                            PROP_TIME_ABSOLUTE);
  rna_def_node_socket_float(
      brna, "NodeSocketFloatDistance", "NodeSocketInterfaceFloatDistance", PROP_DISTANCE);

  rna_def_node_socket_int(brna, "NodeSocketInt", "NodeSocketInterfaceInt", PROP_NONE);
  rna_def_node_socket_int(
      brna, "NodeSocketIntUnsigned", "NodeSocketInterfaceIntUnsigned", PROP_UNSIGNED);
  rna_def_node_socket_int(
      brna, "NodeSocketIntPercentage", "NodeSocketInterfaceIntPercentage", PROP_PERCENTAGE);
  rna_def_node_socket_int(
      brna, "NodeSocketIntFactor", "NodeSocketInterfaceIntFactor", PROP_FACTOR);

  rna_def_node_socket_bool(brna, "NodeSocketBool", "NodeSocketInterfaceBool");
  rna_def_node_socket_rotation(brna, "NodeSocketRotation", "NodeSocketInterfaceRotation");

  rna_def_node_socket_vector(brna, "NodeSocketVector", "NodeSocketInterfaceVector", PROP_NONE);
  rna_def_node_socket_vector(brna,
                             "NodeSocketVectorTranslation",
                             "NodeSocketInterfaceVectorTranslation",
                             PROP_TRANSLATION);
  rna_def_node_socket_vector(
      brna, "NodeSocketVectorDirection", "NodeSocketInterfaceVectorDirection", PROP_DIRECTION);
  rna_def_node_socket_vector(
      brna, "NodeSocketVectorVelocity", "NodeSocketInterfaceVectorVelocity", PROP_VELOCITY);
  rna_def_node_socket_vector(brna,
                             "NodeSocketVectorAcceleration",
                             "NodeSocketInterfaceVectorAcceleration",
                             PROP_ACCELERATION);
  rna_def_node_socket_vector(
      brna, "NodeSocketVectorEuler", "NodeSocketInterfaceVectorEuler", PROP_EULER);
  rna_def_node_socket_vector(
      brna, "NodeSocketVectorXYZ", "NodeSocketInterfaceVectorXYZ", PROP_XYZ);

  rna_def_node_socket_color(brna, "NodeSocketColor", "NodeSocketInterfaceColor");

  rna_def_node_socket_string(brna, "NodeSocketString", "NodeSocketInterfaceString");

  rna_def_node_socket_shader(brna, "NodeSocketShader", "NodeSocketInterfaceShader");

  rna_def_node_socket_virtual(brna, "NodeSocketVirtual");

  rna_def_node_socket_object(brna, "NodeSocketObject", "NodeSocketInterfaceObject");

  rna_def_node_socket_image(brna, "NodeSocketImage", "NodeSocketInterfaceImage");

  rna_def_node_socket_geometry(brna, "NodeSocketGeometry", "NodeSocketInterfaceGeometry");

  rna_def_node_socket_collection(brna, "NodeSocketCollection", "NodeSocketInterfaceCollection");

  rna_def_node_socket_texture(brna, "NodeSocketTexture", "NodeSocketInterfaceTexture");

  rna_def_node_socket_material(brna, "NodeSocketMaterial", "NodeSocketInterfaceMaterial");
}

void RNA_def_node_socket_subtypes(BlenderRNA *brna)
{
  rna_def_node_socket(brna);
  rna_def_node_socket_interface(brna);

  rna_def_node_socket_standard_types(brna);
}

#endif
