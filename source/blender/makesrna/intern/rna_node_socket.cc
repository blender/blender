/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "BLT_translation.hh"

#include "DNA_node_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "rna_internal.hh"

#include "WM_api.hh"

const EnumPropertyItem rna_enum_node_socket_type_items[] = {
    {SOCK_CUSTOM, "CUSTOM", 0, "Custom", ""},
    {SOCK_FLOAT, "VALUE", ICON_NODE_SOCKET_FLOAT, "Value", ""},
    {SOCK_INT, "INT", ICON_NODE_SOCKET_INT, "Integer", ""},
    {SOCK_BOOLEAN, "BOOLEAN", ICON_NODE_SOCKET_BOOLEAN, "Boolean", ""},
    {SOCK_VECTOR, "VECTOR", ICON_NODE_SOCKET_VECTOR, "Vector", ""},
    {SOCK_ROTATION, "ROTATION", ICON_NODE_SOCKET_ROTATION, "Rotation", ""},
    {SOCK_MATRIX, "MATRIX", ICON_NODE_SOCKET_MATRIX, "Matrix", ""},
    {SOCK_STRING, "STRING", ICON_NODE_SOCKET_STRING, "String", ""},
    {SOCK_RGBA, "RGBA", ICON_NODE_SOCKET_RGBA, "RGBA", ""},
    {SOCK_SHADER, "SHADER", ICON_NODE_SOCKET_SHADER, "Shader", ""},
    {SOCK_OBJECT, "OBJECT", ICON_NODE_SOCKET_OBJECT, "Object", ""},
    {SOCK_IMAGE, "IMAGE", ICON_NODE_SOCKET_IMAGE, "Image", ""},
    {SOCK_GEOMETRY, "GEOMETRY", ICON_NODE_SOCKET_GEOMETRY, "Geometry", ""},
    {SOCK_COLLECTION, "COLLECTION", ICON_NODE_SOCKET_COLLECTION, "Collection", ""},
    {SOCK_TEXTURE, "TEXTURE", ICON_NODE_SOCKET_TEXTURE, "Texture", ""},
    {SOCK_MATERIAL, "MATERIAL", ICON_NODE_SOCKET_MATERIAL, "Material", ""},
    {SOCK_MENU, "MENU", ICON_NODE_SOCKET_MENU, "Menu", ""},
    {SOCK_BUNDLE, "BUNDLE", ICON_NODE_SOCKET_BUNDLE, "Bundle", ""},
    {SOCK_CLOSURE, "CLOSURE", ICON_NODE_SOCKET_CLOSURE, "Closure", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "DNA_material_types.h"

#  include "BLI_math_vector.h"
#  include "BLI_string_ref.hh"

#  include "BKE_main_invariants.hh"
#  include "BKE_node.hh"
#  include "BKE_node_enum.hh"
#  include "BKE_node_runtime.hh"
#  include "BKE_node_tree_update.hh"

#  include "DEG_depsgraph_build.hh"

#  include "NOD_socket_declarations.hh"

#  include "ED_node.hh"

extern FunctionRNA rna_NodeSocket_draw_func;
extern FunctionRNA rna_NodeSocket_draw_color_func;
extern FunctionRNA rna_NodeSocket_draw_color_simple_func;

/* ******** Node Socket ******** */

static void rna_NodeSocket_draw(bContext *C,
                                uiLayout *layout,
                                PointerRNA *ptr,
                                PointerRNA *node_ptr,
                                const blender::StringRef text)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  ParameterList list;
  FunctionRNA *func;

  func = &rna_NodeSocket_draw_func; /* RNA_struct_find_function(&ptr, "draw"); */

  RNA_parameter_list_create(&list, ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  RNA_parameter_set_lookup(&list, "node", node_ptr);
  const std::string text_str = text;
  const char *text_c_str = text_str.c_str();
  RNA_parameter_set_lookup(&list, "text", &text_c_str);
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

static void rna_NodeSocket_draw_color_simple(const blender::bke::bNodeSocketType *socket_type,
                                             float *r_color)
{
  ParameterList list;
  FunctionRNA *func;
  void *ret;

  func = &rna_NodeSocket_draw_color_simple_func; /* RNA_struct_find_function(&ptr,
                                                  * "draw_color_simple"); */

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, socket_type->ext_socket.srna, nullptr);
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "type", socket_type);
  socket_type->ext_socket.call(nullptr, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "color", &ret);
  copy_v4_v4(r_color, static_cast<float *>(ret));

  RNA_parameter_list_free(&list);
}

static bool rna_NodeSocket_unregister(Main *bmain, StructRNA *type)
{
  blender::bke::bNodeSocketType *st = static_cast<blender::bke::bNodeSocketType *>(
      RNA_struct_blender_type_get(type));
  if (!st) {
    return false;
  }

  RNA_struct_free_extension(type, &st->ext_socket);
  RNA_struct_free(&BLENDER_RNA, type);

  blender::bke::node_unregister_socket_type(*st);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
  return true;
}

static StructRNA *rna_NodeSocket_register(Main *bmain,
                                          ReportList *reports,
                                          void *data,
                                          const char *identifier,
                                          StructValidateFunc validate,
                                          StructCallbackFunc call,
                                          StructFreeFunc free)
{
  blender::bke::bNodeSocketType *st;
  bNodeSocket dummy_sock = {};
  bool have_function[3];

  /* setup dummy socket & socket type to store static properties in */
  blender::bke::bNodeSocketType dummy_st = {};
  dummy_st.type = SOCK_CUSTOM;

  dummy_sock.typeinfo = &dummy_st;
  PointerRNA dummy_sock_ptr = RNA_pointer_create_discrete(nullptr, &RNA_NodeSocket, &dummy_sock);

  /* validate the python class */
  if (validate(&dummy_sock_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_sock.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering node socket class: '%s' is too long, maximum length is %d",
                identifier,
                int(sizeof(dummy_sock.idname)));
    return nullptr;
  }

  /* check if we have registered this socket type before */
  st = blender::bke::node_socket_type_find(dummy_st.idname);
  if (!st) {
    /* create a new node socket type */
    st = MEM_new<blender::bke::bNodeSocketType>(__func__, dummy_st);
    blender::bke::node_register_socket_type(*st);
  }

  st->free_self = [](blender::bke::bNodeSocketType *stype) { MEM_delete(stype); };

  /* if RNA type is already registered, unregister first */
  if (st->ext_socket.srna) {
    StructRNA *srna = st->ext_socket.srna;
    RNA_struct_free_extension(srna, &st->ext_socket);
    RNA_struct_free(&BLENDER_RNA, srna);
  }
  st->ext_socket.srna = RNA_def_struct_ptr(&BLENDER_RNA, st->idname.c_str(), &RNA_NodeSocket);
  st->ext_socket.data = data;
  st->ext_socket.call = call;
  st->ext_socket.free = free;
  RNA_struct_blender_type_set(st->ext_socket.srna, st);

  /* XXX bad level call! needed to initialize the basic draw functions ... */
  ED_init_custom_node_socket_type(st);

  st->draw = (have_function[0]) ? rna_NodeSocket_draw : nullptr;
  st->draw_color = (have_function[1]) ? rna_NodeSocket_draw_color : nullptr;
  st->draw_color_simple = (have_function[2]) ? rna_NodeSocket_draw_color_simple : nullptr;

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  BKE_main_ensure_invariants(*bmain);
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

static std::optional<std::string> rna_NodeSocket_path(const PointerRNA *ptr)
{
  const bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  const bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);

  const bNode &node = blender::bke::node_find_node(*ntree, *sock);
  const ListBase *sockets = (sock->in_out == SOCK_IN) ? &node.inputs : &node.outputs;
  const int socketindex = BLI_findindex(sockets, sock);

  char name_esc[sizeof(node.name) * 2];
  BLI_str_escape(name_esc, node.name, sizeof(name_esc));

  if (sock->in_out == SOCK_IN) {
    return fmt::format("nodes[\"{}\"].inputs[{}]", name_esc, socketindex);
  }
  return fmt::format("nodes[\"{}\"].outputs[{}]", name_esc, socketindex);
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
  bNode &node = blender::bke::node_find_node(*ntree, *sock);
  return RNA_pointer_create_discrete(&ntree->id, &RNA_Node, &node);
}

static void rna_NodeSocket_type_set(PointerRNA *ptr, int value)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  bNode &node = blender::bke::node_find_node(*ntree, *sock);
  if (node.type_legacy != NODE_CUSTOM) {
    /* Can't change the socket type on built-in nodes like this. */
    return;
  }
  blender::bke::node_modify_socket_type_static(ntree, &node, sock, value, 0);
}

static int rna_NodeSocket_inferred_structure_type_get(PointerRNA *ptr)
{
  bNodeSocket *socket = ptr->data_as<bNodeSocket>();
  return int(socket->runtime->inferred_structure_type);
}

static void rna_NodeSocket_bl_idname_get(PointerRNA *ptr, char *value)
{
  const bNodeSocket *node = static_cast<const bNodeSocket *>(ptr->data);
  const blender::bke::bNodeSocketType *ntype = node->typeinfo;
  blender::StringRef(ntype->idname).copy_unsafe(value);
}

static int rna_NodeSocket_bl_idname_length(PointerRNA *ptr)
{
  const bNodeSocket *node = static_cast<const bNodeSocket *>(ptr->data);
  const blender::bke::bNodeSocketType *ntype = node->typeinfo;
  return ntype->idname.size();
}

static void rna_NodeSocket_bl_idname_set(PointerRNA *ptr, const char *value)
{
  bNodeSocket *node = static_cast<bNodeSocket *>(ptr->data);
  blender::bke::bNodeSocketType *ntype = node->typeinfo;
  ntype->idname = value;
}

static void rna_NodeSocket_bl_label_get(PointerRNA *ptr, char *value)
{
  const bNodeSocket *node = static_cast<const bNodeSocket *>(ptr->data);
  const blender::bke::bNodeSocketType *ntype = node->typeinfo;
  blender::StringRef(ntype->label).copy_unsafe(value);
}

static int rna_NodeSocket_bl_label_length(PointerRNA *ptr)
{
  const bNodeSocket *node = static_cast<const bNodeSocket *>(ptr->data);
  const blender::bke::bNodeSocketType *ntype = node->typeinfo;
  return ntype->label.size();
}

static void rna_NodeSocket_bl_label_set(PointerRNA *ptr, const char *value)
{
  bNodeSocket *node = static_cast<bNodeSocket *>(ptr->data);
  blender::bke::bNodeSocketType *ntype = node->typeinfo;
  ntype->label = value;
}

static void rna_NodeSocket_bl_subtype_label_get(PointerRNA *ptr, char *value)
{
  const bNodeSocket *node = static_cast<const bNodeSocket *>(ptr->data);
  const blender::bke::bNodeSocketType *ntype = node->typeinfo;
  blender::StringRef(ntype->subtype_label).copy_unsafe(value);
}

static int rna_NodeSocket_bl_subtype_label_length(PointerRNA *ptr)
{
  const bNodeSocket *node = static_cast<const bNodeSocket *>(ptr->data);
  const blender::bke::bNodeSocketType *ntype = node->typeinfo;
  return ntype->subtype_label.size();
}

static void rna_NodeSocket_bl_subtype_label_set(PointerRNA *ptr, const char *value)
{
  bNodeSocket *node = static_cast<bNodeSocket *>(ptr->data);
  blender::bke::bNodeSocketType *ntype = node->typeinfo;
  ntype->subtype_label = value;
}

static bool rna_NodeSocket_is_linked_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  ntree->ensure_topology_cache();
  return sock->is_directly_linked();
}

static bool rna_NodeSocket_is_inactive_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = ptr->data_as<bNodeSocket>();
  ntree->ensure_topology_cache();
  return sock->is_inactive();
}

static bool rna_NodeSocket_is_icon_visible_get(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = ptr->data_as<bNodeSocket>();
  ntree->ensure_topology_cache();
  return sock->is_icon_visible();
}

static void rna_NodeSocket_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);

  BKE_ntree_update_tag_socket_property(ntree, sock);
  BKE_main_ensure_invariants(*bmain, ntree->id);
}

static void rna_NodeSocket_enabled_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);

  BKE_ntree_update_tag_socket_availability(ntree, sock);
  BKE_main_ensure_invariants(*bmain, ntree->id);
}

static void rna_NodeSocket_label_get(PointerRNA *ptr, char *value)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  strcpy(value, blender::bke::node_socket_label(*sock).c_str());
}

static int rna_NodeSocket_label_length(PointerRNA *ptr)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  return blender::bke::node_socket_label(*sock).size();
}

static bool rna_NodeSocket_is_output_get(PointerRNA *ptr)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  return sock->in_out == SOCK_OUT;
}

static bool rna_NodeSocket_select_get(PointerRNA *ptr)
{
  const bNodeSocket *socket = ptr->data_as<bNodeSocket>();

  return (socket->flag & SELECT) != 0;
}

static int rna_NodeSocket_link_limit_get(PointerRNA *ptr)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  return blender::bke::node_socket_link_limit(*sock);
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

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = blender::bke::node_find_node(*ntree, *sock);

  /* The Reroute node is the socket itself, do not hide this. */
  if (node.is_reroute()) {
    return;
  }

  if (value) {
    sock->flag |= SOCK_HIDDEN;
  }
  else {
    sock->flag &= ~SOCK_HIDDEN;
  }
}

/* ******** Standard Node Socket Base Types ******** */

static void rna_NodeSocketStandard_draw(ID *id,
                                        bNodeSocket *sock,
                                        bContext *C,
                                        uiLayout *layout,
                                        PointerRNA *nodeptr,
                                        const char *text)
{
  PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_NodeSocket, sock);
  sock->typeinfo->draw(C, layout, &ptr, nodeptr, text);
}

static void rna_NodeSocketStandard_draw_color(
    ID *id, bNodeSocket *sock, bContext *C, PointerRNA *nodeptr, float r_color[4])
{
  PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_NodeSocket, sock);
  sock->typeinfo->draw_color(C, &ptr, nodeptr, r_color);
}

static void rna_NodeSocketStandard_draw_color_simple(StructRNA *type, float r_color[4])
{
  const blender::bke::bNodeSocketType *typeinfo =
      static_cast<const blender::bke::bNodeSocketType *>(RNA_struct_blender_type_get(type));
  typeinfo->draw_color_simple(typeinfo, r_color);
}

static const char *rna_NodeSocketStandard_name_func(const PointerRNA *ptr,
                                                    const PropertyRNA * /*prop*/,
                                                    const bool do_translate)
{
  const bNodeSocket *socket = ptr->data_as<bNodeSocket>();
  if (do_translate) {
    return blender::ed::space_node::node_socket_get_label(socket);
  }
  return socket->name;
}

/* ******** Node Socket Subtypes ******** */

void rna_NodeSocketStandard_float_range(
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

void rna_NodeSocketStandard_int_range(
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

void rna_NodeSocketStandard_vector_range(
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

float rna_NodeSocketStandard_float_default(PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  if (!sock->runtime->declaration) {
    return 0.0f;
  }
  auto *decl = static_cast<const blender::nodes::decl::Float *>(sock->runtime->declaration);
  return decl->default_value;
}

int rna_NodeSocketStandard_int_default(PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  if (!sock->runtime->declaration) {
    return 0;
  }
  auto *decl = static_cast<const blender::nodes::decl::Int *>(sock->runtime->declaration);
  return decl->default_value;
}

bool rna_NodeSocketStandard_boolean_default(PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  if (!sock->runtime->declaration) {
    return false;
  }
  auto *decl = static_cast<const blender::nodes::decl::Bool *>(sock->runtime->declaration);
  return decl->default_value;
}

void rna_NodeSocketStandard_vector_default(PointerRNA *ptr,
                                           PropertyRNA * /*prop*/,
                                           float *r_values)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  if (!sock->runtime->declaration) {
    const int dimensions = sock->default_value_typed<bNodeSocketValueVector>()->dimensions;
    std::fill_n(r_values, dimensions, 0.0f);
    return;
  }
  auto *decl = static_cast<const blender::nodes::decl::Vector *>(sock->runtime->declaration);
  std::copy_n(&decl->default_value[0], decl->dimensions, r_values);
}

void rna_NodeSocketStandard_color_default(PointerRNA *ptr, PropertyRNA * /*prop*/, float *r_values)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  if (!sock->runtime->declaration) {
    std::fill_n(r_values, 4, 0.0f);
    return;
  }
  auto *decl = static_cast<const blender::nodes::decl::Color *>(sock->runtime->declaration);

#  if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Warray-bounds"
#  endif

  std::copy_n(&decl->default_value[0], 4, r_values);

#  if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic pop
#  endif
}

int rna_NodeSocketStandard_menu_default(PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
  if (!sock->runtime->declaration) {
    return 0;
  }
  auto *decl = static_cast<const blender::nodes::decl::Menu *>(sock->runtime->declaration);
  return decl->default_value.value;
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

const EnumPropertyItem *RNA_node_enum_definition_itemf(
    const blender::bke::RuntimeNodeEnumItems &enum_items, bool *r_free)
{
  EnumPropertyItem tmp = {0};
  EnumPropertyItem *result = nullptr;
  int totitem = 0;

  for (const blender::bke::RuntimeNodeEnumItem &item : enum_items.items) {
    tmp.value = item.identifier;
    /* Item name is unique and used as the RNA identifier as well.
     * The integer value is persistent and unique and should be used
     * when storing the enum value. */
    tmp.identifier = item.name.c_str();
    /* TODO support icons in enum definition. */
    tmp.icon = ICON_NONE;
    tmp.name = item.name.c_str();
    tmp.description = item.description.c_str();

    RNA_enum_item_add(&result, &totitem, &tmp);
  }

  if (totitem == 0) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  RNA_enum_item_end(&result, &totitem);
  *r_free = true;

  return result;
}

const EnumPropertyItem *RNA_node_socket_menu_itemf(bContext * /*C*/,
                                                   PointerRNA *ptr,
                                                   PropertyRNA *prop,
                                                   bool *r_free)
{
  const bNodeSocket *socket = static_cast<bNodeSocket *>(ptr->data);
  if (!socket) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }
  const bNodeSocketValueMenu *data = static_cast<bNodeSocketValueMenu *>(socket->default_value);
  if (!data->enum_items) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }
  const char *socket_translation_context = blender::bke::node_socket_translation_context(*socket);
  RNA_def_property_translation_context(prop, socket_translation_context);
  return RNA_node_enum_definition_itemf(*data->enum_items, r_free);
}

std::optional<std::string> rna_NodeSocketString_filepath_filter(const bContext * /*C*/,
                                                                PointerRNA *ptr,
                                                                PropertyRNA * /*prop*/)
{
  bNodeSocket *socket = static_cast<bNodeSocket *>(ptr->data);
  BLI_assert(socket->type == SOCK_STRING);
  if (const auto *decl = dynamic_cast<const blender::nodes::decl::String *>(
          socket->runtime->declaration))
  {
    return decl->path_filter;
  }
  return std::nullopt;
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

  static const float default_draw_color[] = {0.0f, 0.0f, 0.0f, 1.0f};

  srna = RNA_def_struct(brna, "NodeSocket", nullptr);
  RNA_def_struct_ui_text(srna, "Node Socket", "Input or output socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");
  RNA_def_struct_refine_func(srna, "rna_NodeSocket_refine");
  RNA_def_struct_ui_icon(srna, ICON_NONE);
  RNA_def_struct_path_func(srna, "rna_NodeSocket_path");
  RNA_def_struct_register_funcs(
      srna, "rna_NodeSocket_register", "rna_NodeSocket_unregister", nullptr);
  RNA_def_struct_system_idprops_func(srna, "rna_NodeSocket_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Socket name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

  prop = RNA_def_property(srna, "label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_NodeSocket_label_get", "rna_NodeSocket_label_length", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Label",
                           "Custom dynamic defined UI label for the socket. Can be translated if "
                           "translation is enabled in the preferences");

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

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_NodeSocket_select_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Select", "True if the socket is selected");

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_HIDDEN);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_NodeSocket_hide_set");
  RNA_def_property_ui_text(prop, "Hide", "Hide the socket");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SOCK_UNAVAIL);
  RNA_def_property_ui_text(prop, "Enabled", "Enable the socket");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeSocket_enabled_update");

  prop = RNA_def_property(srna, "link_limit", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "limit");
  RNA_def_property_int_funcs(
      prop, "rna_NodeSocket_link_limit_get", "rna_NodeSocket_link_limit_set", nullptr);
  RNA_def_property_range(prop, 1, 0xFFF);
  RNA_def_property_ui_text(prop, "Link Limit", "Max number of links allowed for this socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "is_linked", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_NodeSocket_is_linked_get", nullptr);
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

  prop = RNA_def_property(srna, "is_inactive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_NodeSocket_is_inactive_get", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Inactive",
      "Socket is grayed out because it has been detected to not have any effect on the output");

  prop = RNA_def_property(srna, "is_icon_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_NodeSocket_is_icon_visible_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Icon Visible", "Socket is drawn as interactive icon in the node editor");

  prop = RNA_def_property(srna, "hide_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_HIDE_VALUE);
  RNA_def_property_ui_text(prop, "Hide Value", "Hide the socket input value");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "pin_gizmo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SOCK_GIZMO_PIN);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Pin Gizmo", "Keep gizmo visible even when the node is not selected");
  RNA_def_property_update(prop, NC_NODE | ND_NODE_GIZMO, nullptr);

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

  prop = RNA_def_property(srna, "inferred_structure_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_node_socket_structure_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_funcs(
      prop, "rna_NodeSocket_inferred_structure_type_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop,
                           "Inferred Structure Type",
                           "Best known structure type of the socket. This may not match the "
                           "socket shape, e.g. for unlinked input sockets");

  /* registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeSocket_bl_idname_get",
                                "rna_NodeSocket_bl_idname_length",
                                "rna_NodeSocket_bl_idname_set");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", "");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeSocket_bl_label_get",
                                "rna_NodeSocket_bl_label_length",
                                "rna_NodeSocket_bl_label_set");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Type Label", "Label to display for the socket type in the UI");

  prop = RNA_def_property(srna, "bl_subtype_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeSocket_bl_subtype_label_get",
                                "rna_NodeSocket_bl_subtype_label_length",
                                "rna_NodeSocket_bl_subtype_label_set");
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
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "Node");
  RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_float_array(
      func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "draw_color_simple", nullptr);
  RNA_def_function_ui_description(
      func,
      "Color of the socket icon. Used to draw sockets in places where the socket does not belong "
      "to a node, like the node interface panel. Also used to draw node sockets if draw_color is "
      "not defined.");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_float_array(
      func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
  RNA_def_function_output(func, parm);
}

static void rna_def_node_socket_standard(BlenderRNA *brna)
{
  /* XXX Workaround: Registered functions are not exposed in python by bpy,
   * it expects them to be registered from python and use the native implementation.
   * However, the standard socket types below are not registering these functions from python,
   * so in order to call them in py scripts we need to overload and
   * replace them with plain C callbacks.
   * These types provide a usable basis for socket types defined in C.
   */

  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  static const float default_draw_color[] = {0.0f, 0.0f, 0.0f, 1.0f};

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

  func = RNA_def_function(srna, "draw_color_simple", "rna_NodeSocketStandard_draw_color_simple");
  RNA_def_function_ui_description(func, "Color of the socket icon");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE | FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_float_array(
      func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
  RNA_def_function_output(func, parm);
}

/* Common functions for all builtin socket interface types. */
static void rna_def_node_tree_interface_socket_builtin(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  /* Override for functions, invoking the typeinfo callback directly
   * instead of expecting an existing RNA registered function implementation.
   */

  func = RNA_def_function(srna, "draw", "rna_NodeTreeInterfaceSocket_draw_builtin");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Draw interface socket settings");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "init_socket", "rna_NodeTreeInterfaceSocket_init_socket_builtin");
  RNA_def_function_ui_description(func, "Initialize a node socket instance");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the socket to initialize");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Socket to initialize");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(
      func, "data_path", nullptr, 0, "Data Path", "Path to specialized socket data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "from_socket", "rna_NodeTreeInterfaceSocket_from_socket_builtin");
  RNA_def_function_ui_description(func, "Setup template parameters from an existing socket");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the original socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Original socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_node_socket_float(BlenderRNA *brna,
                                      const char *identifier,
                                      PropertySubType subtype)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Float Node Socket", "Floating-point number socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_FLOAT);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueFloat", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_float_range");
  RNA_def_property_float_default_func(prop, "rna_NodeSocketStandard_float_default");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_float(BlenderRNA *brna,
                                                const char *identifier,
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

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(
      srna, "Float Node Socket Interface", "Floating-point number socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueFloat", "socket_data");

  prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_sdna(prop, nullptr, "subtype");
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocketFloat_subtype_itemf");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Subtype", "Subtype of the default value");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_float_default(prop, value_default);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocketFloat_default_value_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "min_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "min");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "max_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_int(BlenderRNA *brna,
                                    const char *identifier,
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
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_INT);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueInt", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_INT, subtype);
  RNA_def_property_int_sdna(prop, nullptr, "value");
  RNA_def_property_int_default(prop, value_default);
  RNA_def_property_int_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_int_range");
  RNA_def_property_int_default_func(prop, "rna_NodeSocketStandard_int_default");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_int(BlenderRNA *brna,
                                              const char *identifier,
                                              PropertySubType subtype)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Integer Node Socket Interface", "Integer number socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueInt", "socket_data");

  prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_sdna(prop, nullptr, "subtype");
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocketInt_subtype_itemf");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Subtype", "Subtype of the default value");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "default_value", PROP_INT, subtype);
  RNA_def_property_int_sdna(prop, nullptr, "value");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocketInt_default_value_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "min_value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "min");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "max_value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "max");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_bool(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Boolean Node Socket", "Boolean value socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_BOOLEAN);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueBoolean", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "value", 1);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_boolean_default_func(prop, "rna_NodeSocketStandard_boolean_default");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_bool(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Boolean Node Socket Interface", "Boolean value socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueBoolean", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "value", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_rotation(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Rotation Node Socket", "Rotation value socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_ROTATION);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueRotation", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "value_euler");
  // RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_rotation(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(
      srna, "Rotation Node Socket Interface", "Rotation value socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueRotation", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "value_euler");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_matrix(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Matrix Node Socket", "Matrix value socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_MATRIX);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_matrix(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Matrix Node Socket Interface", "Matrix value socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_vector(BlenderRNA *brna,
                                       const char *identifier,
                                       PropertySubType subtype,
                                       int dimensions)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Vector Node Socket", "3D vector socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_VECTOR);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueVector", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_array(prop, dimensions);
  RNA_def_property_float_default_func(prop, "rna_NodeSocketStandard_vector_default");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_NodeSocketStandard_vector_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_vector(BlenderRNA *brna,
                                                 const char *identifier,
                                                 PropertySubType subtype,
                                                 int dimensions)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Vector Node Socket Interface", "3D vector socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueVector", "socket_data");

  prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_sdna(prop, nullptr, "subtype");
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocketVector_subtype_itemf");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Subtype", "Subtype of the default value");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "dimensions", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "dimensions");
  RNA_def_property_range(prop, 2, 4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Dimensions", "Dimensions of the vector socket");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceSocketVector_dimensions_update");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_array(prop, dimensions);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocketVector_default_value_range");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "min_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "min");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "max_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_color(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Color Node Socket", "RGBA color socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_RGBA);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueRGBA", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_float_default_func(prop, "rna_NodeSocketStandard_color_default");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_color(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Color Node Socket Interface", "RGBA color socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueRGBA", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_string(BlenderRNA *brna,
                                       const char *identifier,
                                       PropertySubType subtype)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "String Node Socket", "String socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_STRING);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueString", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_STRING, subtype);
  RNA_def_property_string_sdna(prop, nullptr, "value");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  if (subtype == PROP_FILEPATH) {
    RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
    RNA_def_property_string_filepath_filter_func(prop, "rna_NodeSocketString_filepath_filter");
  }

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_string(BlenderRNA *brna,
                                                 const char *identifier,
                                                 PropertySubType subtype)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "String Node Socket Interface", "String socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueString", "socket_data");

  prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_sdna(prop, nullptr, "subtype");
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_NodeTreeInterfaceSocketString_subtype_itemf");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Subtype", "Subtype of the default value");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "default_value", PROP_STRING, subtype);
  RNA_def_property_string_sdna(prop, nullptr, "value");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  // TODO: Do I need to call RNA_def_property_string_funcs() ?
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_menu(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Menu Node Socket", "Menu socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_MENU);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueMenu", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "value");
  RNA_def_property_enum_items(prop, rna_enum_dummy_NULL_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "RNA_node_socket_menu_itemf");
  RNA_def_property_enum_default_func(prop, "rna_NodeSocketStandard_menu_default");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);

  RNA_def_struct_sdna_from(srna, "bNodeSocket", nullptr);
}

static void rna_def_node_socket_interface_menu(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Menu Node Socket Interface", "Menu socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueMenu", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "value");
  RNA_def_property_enum_items(prop, rna_enum_dummy_NULL_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "RNA_node_tree_interface_socket_menu_itemf");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  RNA_def_struct_sdna_from(srna, "bNodeTreeInterfaceSocket", nullptr);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_shader(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Shader Node Socket", "Shader socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_SHADER);
  RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_interface_shader(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Shader Node Socket Interface", "Shader socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_object(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Object Node Socket", "Object socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_OBJECT);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueObject", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_interface_object(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Object Node Socket Interface", "Object socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueObject", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_image(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Image Node Socket", "Image socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_IMAGE);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueImage", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_interface_image(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Image Node Socket Interface", "Image socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueImage", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_geometry(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Geometry Node Socket", "Geometry socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_GEOMETRY);
  RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_interface_geometry(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Geometry Node Socket Interface", "Geometry socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_bundle(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Bundle Node Socket", "Bundle socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_BUNDLE);
  RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_interface_bundle(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Bundle Node Socket Interface", "Bundle socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_closure(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Closure Node Socket", "Closure socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_CLOSURE);
  RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_interface_closure(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Closure Node Socket Interface", "Closure socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_collection(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Collection Node Socket", "Collection socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_COLLECTION);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueCollection", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_interface_collection(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Collection Node Socket Interface", "Collection socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueCollection", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_texture(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Texture Node Socket", "Texture socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_TEXTURE);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueTexture", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_interface_texture(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Texture Node Socket Interface", "Texture socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueTexture", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_material(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Material Node Socket", "Material socket of a node");
  RNA_def_struct_ui_icon(srna, ICON_NODE_SOCKET_MATERIAL);
  RNA_def_struct_sdna(srna, "bNodeSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueMaterial", "default_value");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_ui_name_func(prop, "rna_NodeSocketStandard_name_func");
  RNA_def_property_update(
      prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_and_relation_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT | PROP_CONTEXT_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
}

static void rna_def_node_socket_interface_material(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, identifier, "NodeTreeInterfaceSocket");
  RNA_def_struct_ui_text(srna, "Material Node Socket Interface", "Material socket of a node");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");

  RNA_def_struct_sdna_from(srna, "bNodeSocketValueMaterial", "socket_data");

  prop = RNA_def_property(srna, "default_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "value");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_NodeTreeInterfaceSocketMaterial_default_value_poll");
  RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  rna_def_node_tree_interface_socket_builtin(srna);
}

static void rna_def_node_socket_virtual(BlenderRNA *brna, const char *identifier)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
  RNA_def_struct_ui_text(srna, "Virtual Node Socket", "Virtual socket of a node");
  RNA_def_struct_sdna(srna, "bNodeSocket");
}

/* Info for generating static subtypes. */
struct bNodeSocketStaticTypeInfo {
  const char *socket_identifier;
  const char *interface_identifier;
  eNodeSocketDatatype type;
  PropertySubType subtype;
  const char *label;
};

/* NOTE: Socket and interface subtypes could be defined from a single central list,
 * but makesrna cannot have a dependency on BKE, so this list would have to live in RNA itself,
 * with BKE etc. accessing the RNA API to get the subtypes info. */
static const bNodeSocketStaticTypeInfo node_socket_subtypes[] = {
    {"NodeSocketFloat", "NodeTreeInterfaceSocketFloat", SOCK_FLOAT, PROP_NONE},
    {"NodeSocketFloatUnsigned", "NodeTreeInterfaceSocketFloatUnsigned", SOCK_FLOAT, PROP_UNSIGNED},
    {"NodeSocketFloatPercentage",
     "NodeTreeInterfaceSocketFloatPercentage",
     SOCK_FLOAT,
     PROP_PERCENTAGE},
    {"NodeSocketFloatFactor", "NodeTreeInterfaceSocketFloatFactor", SOCK_FLOAT, PROP_FACTOR},
    {"NodeSocketFloatAngle", "NodeTreeInterfaceSocketFloatAngle", SOCK_FLOAT, PROP_ANGLE},
    {"NodeSocketFloatTime", "NodeTreeInterfaceSocketFloatTime", SOCK_FLOAT, PROP_TIME},
    {"NodeSocketFloatTimeAbsolute",
     "NodeTreeInterfaceSocketFloatTimeAbsolute",
     SOCK_FLOAT,
     PROP_TIME_ABSOLUTE},
    {"NodeSocketFloatDistance", "NodeTreeInterfaceSocketFloatDistance", SOCK_FLOAT, PROP_DISTANCE},
    {"NodeSocketFloatWavelength",
     "NodeTreeInterfaceSocketFloatWavelength",
     SOCK_FLOAT,
     PROP_WAVELENGTH},
    {"NodeSocketFloatColorTemperature",
     "NodeTreeInterfaceSocketFloatColorTemperature",
     SOCK_FLOAT,
     PROP_COLOR_TEMPERATURE},
    {"NodeSocketFloatFrequency",
     "NodeTreeInterfaceSocketFloatFrequency",
     SOCK_FLOAT,
     PROP_FREQUENCY},
    {"NodeSocketInt", "NodeTreeInterfaceSocketInt", SOCK_INT, PROP_NONE},
    {"NodeSocketIntUnsigned", "NodeTreeInterfaceSocketIntUnsigned", SOCK_INT, PROP_UNSIGNED},
    {"NodeSocketIntPercentage", "NodeTreeInterfaceSocketIntPercentage", SOCK_INT, PROP_PERCENTAGE},
    {"NodeSocketIntFactor", "NodeTreeInterfaceSocketIntFactor", SOCK_INT, PROP_FACTOR},
    {"NodeSocketBool", "NodeTreeInterfaceSocketBool", SOCK_BOOLEAN, PROP_NONE},

    {"NodeSocketVector", "NodeTreeInterfaceSocketVector", SOCK_VECTOR, PROP_NONE},
    {"NodeSocketVectorFactor", "NodeTreeInterfaceSocketVectorFactor", SOCK_VECTOR, PROP_FACTOR},
    {"NodeSocketVectorPercentage",
     "NodeTreeInterfaceSocketVectorPercentage",
     SOCK_VECTOR,
     PROP_PERCENTAGE},
    {"NodeSocketVectorTranslation",
     "NodeTreeInterfaceSocketVectorTranslation",
     SOCK_VECTOR,
     PROP_TRANSLATION},
    {"NodeSocketVectorDirection",
     "NodeTreeInterfaceSocketVectorDirection",
     SOCK_VECTOR,
     PROP_DIRECTION},
    {"NodeSocketVectorVelocity",
     "NodeTreeInterfaceSocketVectorVelocity",
     SOCK_VECTOR,
     PROP_VELOCITY},
    {"NodeSocketVectorAcceleration",
     "NodeTreeInterfaceSocketVectorAcceleration",
     SOCK_VECTOR,
     PROP_ACCELERATION},
    {"NodeSocketVectorEuler", "NodeTreeInterfaceSocketVectorEuler", SOCK_VECTOR, PROP_EULER},
    {"NodeSocketVectorXYZ", "NodeTreeInterfaceSocketVectorXYZ", SOCK_VECTOR, PROP_XYZ},

    {"NodeSocketVector2D", "NodeTreeInterfaceSocketVector2D", SOCK_VECTOR, PROP_NONE},
    {"NodeSocketVectorFactor2D",
     "NodeTreeInterfaceSocketVectorFactor2D",
     SOCK_VECTOR,
     PROP_FACTOR},
    {"NodeSocketVectorPercentage2D",
     "NodeTreeInterfaceSocketVectorPercentage2D",
     SOCK_VECTOR,
     PROP_PERCENTAGE},
    {"NodeSocketVectorTranslation2D",
     "NodeTreeInterfaceSocketVectorTranslation2D",
     SOCK_VECTOR,
     PROP_TRANSLATION},
    {"NodeSocketVectorDirection2D",
     "NodeTreeInterfaceSocketVectorDirection2D",
     SOCK_VECTOR,
     PROP_DIRECTION},
    {"NodeSocketVectorVelocity2D",
     "NodeTreeInterfaceSocketVectorVelocity2D",
     SOCK_VECTOR,
     PROP_VELOCITY},
    {"NodeSocketVectorAcceleration2D",
     "NodeTreeInterfaceSocketVectorAcceleration2D",
     SOCK_VECTOR,
     PROP_ACCELERATION},
    {"NodeSocketVectorEuler2D", "NodeTreeInterfaceSocketVectorEuler2D", SOCK_VECTOR, PROP_EULER},
    {"NodeSocketVectorXYZ2D", "NodeTreeInterfaceSocketVectorXYZ2D", SOCK_VECTOR, PROP_XYZ},

    {"NodeSocketVector4D", "NodeTreeInterfaceSocketVector4D", SOCK_VECTOR, PROP_NONE},
    {"NodeSocketVectorFactor4D",
     "NodeTreeInterfaceSocketVectorFactor4D",
     SOCK_VECTOR,
     PROP_FACTOR},
    {"NodeSocketVectorPercentage4D",
     "NodeTreeInterfaceSocketVectorPercentage4D",
     SOCK_VECTOR,
     PROP_PERCENTAGE},
    {"NodeSocketVectorTranslation4D",
     "NodeTreeInterfaceSocketVectorTranslation4D",
     SOCK_VECTOR,
     PROP_TRANSLATION},
    {"NodeSocketVectorDirection4D",
     "NodeTreeInterfaceSocketVectorDirection4D",
     SOCK_VECTOR,
     PROP_DIRECTION},
    {"NodeSocketVectorVelocity4D",
     "NodeTreeInterfaceSocketVectorVelocity4D",
     SOCK_VECTOR,
     PROP_VELOCITY},
    {"NodeSocketVectorAcceleration4D",
     "NodeTreeInterfaceSocketVectorAcceleration4D",
     SOCK_VECTOR,
     PROP_ACCELERATION},
    {"NodeSocketVectorEuler4D", "NodeTreeInterfaceSocketVectorEuler4D", SOCK_VECTOR, PROP_EULER},
    {"NodeSocketVectorXYZ4D", "NodeTreeInterfaceSocketVectorXYZ4D", SOCK_VECTOR, PROP_XYZ},

    {"NodeSocketRotation", "NodeTreeInterfaceSocketRotation", SOCK_ROTATION, PROP_NONE},
    {"NodeSocketMatrix", "NodeTreeInterfaceSocketMatrix", SOCK_MATRIX, PROP_NONE},

    {"NodeSocketColor", "NodeTreeInterfaceSocketColor", SOCK_RGBA, PROP_NONE},
    {"NodeSocketString", "NodeTreeInterfaceSocketString", SOCK_STRING, PROP_NONE},
    {"NodeSocketStringFilePath",
     "NodeTreeInterfaceSocketStringFilePath",
     SOCK_STRING,
     PROP_FILEPATH},
    {"NodeSocketShader", "NodeTreeInterfaceSocketShader", SOCK_SHADER, PROP_NONE},
    {"NodeSocketObject", "NodeTreeInterfaceSocketObject", SOCK_OBJECT, PROP_NONE},
    {"NodeSocketImage", "NodeTreeInterfaceSocketImage", SOCK_IMAGE, PROP_NONE},
    {"NodeSocketGeometry", "NodeTreeInterfaceSocketGeometry", SOCK_GEOMETRY, PROP_NONE},
    {"NodeSocketCollection", "NodeTreeInterfaceSocketCollection", SOCK_COLLECTION, PROP_NONE},
    {"NodeSocketTexture", "NodeTreeInterfaceSocketTexture", SOCK_TEXTURE, PROP_NONE},
    {"NodeSocketMaterial", "NodeTreeInterfaceSocketMaterial", SOCK_MATERIAL, PROP_NONE},
    {"NodeSocketMenu", "NodeTreeInterfaceSocketMenu", SOCK_MENU, PROP_NONE},
    {"NodeSocketBundle", "NodeTreeInterfaceSocketBundle", SOCK_BUNDLE, PROP_NONE},
    {"NodeSocketClosure", "NodeTreeInterfaceSocketClosure", SOCK_CLOSURE, PROP_NONE},
};

static void rna_def_node_socket_subtypes(BlenderRNA *brna)
{
  for (const bNodeSocketStaticTypeInfo &info : node_socket_subtypes) {
    const char *identifier = info.socket_identifier;

    switch (info.type) {
      case SOCK_FLOAT:
        rna_def_node_socket_float(brna, identifier, info.subtype);
        break;
      case SOCK_INT:
        rna_def_node_socket_int(brna, identifier, info.subtype);
        break;
      case SOCK_BOOLEAN:
        rna_def_node_socket_bool(brna, identifier);
        break;
      case SOCK_ROTATION:
        rna_def_node_socket_rotation(brna, identifier);
        break;
      case SOCK_MATRIX:
        rna_def_node_socket_matrix(brna, identifier);
        break;
      case SOCK_VECTOR:
        if (blender::StringRef(identifier).endswith("2D")) {
          rna_def_node_socket_vector(brna, identifier, info.subtype, 2);
        }
        else if (blender::StringRef(identifier).endswith("4D")) {
          rna_def_node_socket_vector(brna, identifier, info.subtype, 4);
        }
        else {
          rna_def_node_socket_vector(brna, identifier, info.subtype, 3);
        }
        break;
      case SOCK_RGBA:
        rna_def_node_socket_color(brna, identifier);
        break;
      case SOCK_STRING:
        rna_def_node_socket_string(brna, identifier, info.subtype);
        break;
      case SOCK_SHADER:
        rna_def_node_socket_shader(brna, identifier);
        break;
      case SOCK_OBJECT:
        rna_def_node_socket_object(brna, identifier);
        break;
      case SOCK_IMAGE:
        rna_def_node_socket_image(brna, identifier);
        break;
      case SOCK_GEOMETRY:
        rna_def_node_socket_geometry(brna, identifier);
        break;
      case SOCK_COLLECTION:
        rna_def_node_socket_collection(brna, identifier);
        break;
      case SOCK_TEXTURE:
        rna_def_node_socket_texture(brna, identifier);
        break;
      case SOCK_MATERIAL:
        rna_def_node_socket_material(brna, identifier);
        break;
      case SOCK_MENU:
        rna_def_node_socket_menu(brna, identifier);
        break;
      case SOCK_BUNDLE:
        rna_def_node_socket_bundle(brna, identifier);
        break;
      case SOCK_CLOSURE:
        rna_def_node_socket_closure(brna, identifier);
        break;

      case SOCK_CUSTOM:
        break;
    }
  }

  rna_def_node_socket_virtual(brna, "NodeSocketVirtual");
}

void rna_def_node_socket_interface_subtypes(BlenderRNA *brna)
{
  /* NOTE: interface items are defined outside this file.
   * The subtypes must be defined after the base type, so this function
   * is called from the interface rna file to ensure correct order. */

  for (const bNodeSocketStaticTypeInfo &info : node_socket_subtypes) {
    const char *identifier = info.interface_identifier;

    switch (info.type) {
      case SOCK_FLOAT:
        rna_def_node_socket_interface_float(brna, identifier, info.subtype);
        break;
      case SOCK_INT:
        rna_def_node_socket_interface_int(brna, identifier, info.subtype);
        break;
      case SOCK_BOOLEAN:
        rna_def_node_socket_interface_bool(brna, identifier);
        break;
      case SOCK_ROTATION:
        rna_def_node_socket_interface_rotation(brna, identifier);
        break;
      case SOCK_MATRIX:
        rna_def_node_socket_interface_matrix(brna, identifier);
        break;
      case SOCK_VECTOR:
        if (blender::StringRef(identifier).endswith("2D")) {
          rna_def_node_socket_interface_vector(brna, identifier, info.subtype, 2);
        }
        else if (blender::StringRef(identifier).endswith("4D")) {
          rna_def_node_socket_interface_vector(brna, identifier, info.subtype, 4);
        }
        else {
          rna_def_node_socket_interface_vector(brna, identifier, info.subtype, 3);
        }
        break;
      case SOCK_RGBA:
        rna_def_node_socket_interface_color(brna, identifier);
        break;
      case SOCK_STRING:
        rna_def_node_socket_interface_string(brna, identifier, info.subtype);
        break;
      case SOCK_MENU:
        rna_def_node_socket_interface_menu(brna, identifier);
        break;
      case SOCK_SHADER:
        rna_def_node_socket_interface_shader(brna, identifier);
        break;
      case SOCK_OBJECT:
        rna_def_node_socket_interface_object(brna, identifier);
        break;
      case SOCK_IMAGE:
        rna_def_node_socket_interface_image(brna, identifier);
        break;
      case SOCK_GEOMETRY:
        rna_def_node_socket_interface_geometry(brna, identifier);
        break;
      case SOCK_COLLECTION:
        rna_def_node_socket_interface_collection(brna, identifier);
        break;
      case SOCK_TEXTURE:
        rna_def_node_socket_interface_texture(brna, identifier);
        break;
      case SOCK_MATERIAL:
        rna_def_node_socket_interface_material(brna, identifier);
        break;
      case SOCK_BUNDLE:
        rna_def_node_socket_interface_bundle(brna, identifier);
        break;
      case SOCK_CLOSURE:
        rna_def_node_socket_interface_closure(brna, identifier);
        break;

      case SOCK_CUSTOM:
        break;
    }
  }
}

void RNA_def_node_socket_subtypes(BlenderRNA *brna)
{
  rna_def_node_socket(brna);

  rna_def_node_socket_standard(brna);
  rna_def_node_socket_subtypes(brna);
}

#endif
