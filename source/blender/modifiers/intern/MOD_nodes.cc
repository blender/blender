/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup modifiers
 */

#include <cstring>
#include <iostream>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_path_util.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_search.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_curves_types.h"
#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_customdata.h"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_global.h"
#include "BKE_idprop.hh"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_object.h"
#include "BKE_pointcloud.h"
#include "BKE_screen.h"
#include "BKE_simulation.h"
#include "BKE_simulation_state.hh"
#include "BKE_simulation_state_serialize.hh"
#include "BKE_workspace.h"

#include "BLO_read_write.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_resources.h"

#include "BLT_translation.h"

#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.hh"
#include "MOD_nodes.h"
#include "MOD_ui_common.hh"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_spreadsheet.h"
#include "ED_undo.h"
#include "ED_viewer_path.hh"

#include "NOD_geometry.h"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_node_declaration.hh"

#include "FN_field.hh"
#include "FN_field_cpp_type.hh"
#include "FN_lazy_function_execute.hh"
#include "FN_lazy_function_graph_executor.hh"
#include "FN_multi_function.hh"

namespace lf = blender::fn::lazy_function;
namespace geo_log = blender::nodes::geo_eval_log;

namespace blender {

static void initData(ModifierData *md)
{
  NodesModifierData *nmd = (NodesModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(nmd, modifier));

  MEMCPY_STRUCT_AFTER(nmd, DNA_struct_default_get(NodesModifierData), modifier);
}

static void add_used_ids_from_sockets(const ListBase &sockets, Set<ID *> &ids)
{
  LISTBASE_FOREACH (const bNodeSocket *, socket, &sockets) {
    switch (socket->type) {
      case SOCK_OBJECT: {
        if (Object *object = ((bNodeSocketValueObject *)socket->default_value)->value) {
          ids.add(&object->id);
        }
        break;
      }
      case SOCK_COLLECTION: {
        if (Collection *collection = ((bNodeSocketValueCollection *)socket->default_value)->value)
        {
          ids.add(&collection->id);
        }
        break;
      }
      case SOCK_MATERIAL: {
        if (Material *material = ((bNodeSocketValueMaterial *)socket->default_value)->value) {
          ids.add(&material->id);
        }
        break;
      }
      case SOCK_TEXTURE: {
        if (Tex *texture = ((bNodeSocketValueTexture *)socket->default_value)->value) {
          ids.add(&texture->id);
        }
        break;
      }
      case SOCK_IMAGE: {
        if (Image *image = ((bNodeSocketValueImage *)socket->default_value)->value) {
          ids.add(&image->id);
        }
        break;
      }
    }
  }
}

/**
 * \note We can only check properties here that cause the dependency graph to update relations when
 * they are changed, otherwise there may be a missing relation after editing. So this could check
 * more properties like whether the node is muted, but we would have to accept the cost of updating
 * relations when those properties are changed.
 */
static bool node_needs_own_transform_relation(const bNode &node)
{
  if (node.type == GEO_NODE_COLLECTION_INFO) {
    const NodeGeometryCollectionInfo &storage = *static_cast<const NodeGeometryCollectionInfo *>(
        node.storage);
    return storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
  }

  if (node.type == GEO_NODE_OBJECT_INFO) {
    const NodeGeometryObjectInfo &storage = *static_cast<const NodeGeometryObjectInfo *>(
        node.storage);
    return storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
  }

  if (node.type == GEO_NODE_SELF_OBJECT) {
    return true;
  }
  if (node.type == GEO_NODE_DEFORM_CURVES_ON_SURFACE) {
    return true;
  }

  return false;
}

static void process_nodes_for_depsgraph(const bNodeTree &tree,
                                        Set<ID *> &ids,
                                        bool &r_needs_own_transform_relation,
                                        Set<const bNodeTree *> &checked_groups)
{
  if (!checked_groups.add(&tree)) {
    return;
  }

  tree.ensure_topology_cache();
  for (const bNode *node : tree.all_nodes()) {
    add_used_ids_from_sockets(node->inputs, ids);
    add_used_ids_from_sockets(node->outputs, ids);
    r_needs_own_transform_relation |= node_needs_own_transform_relation(*node);
  }

  for (const bNode *node : tree.group_nodes()) {
    if (const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id)) {
      process_nodes_for_depsgraph(*sub_tree, ids, r_needs_own_transform_relation, checked_groups);
    }
  }
}

static void find_used_ids_from_settings(const NodesModifierSettings &settings, Set<ID *> &ids)
{
  IDP_foreach_property(
      settings.properties,
      IDP_TYPE_FILTER_ID,
      [](IDProperty *property, void *user_data) {
        Set<ID *> *ids = (Set<ID *> *)user_data;
        ID *id = IDP_Id(property);
        if (id != nullptr) {
          ids->add(id);
        }
      },
      &ids);
}

/* We don't know exactly what attributes from the other object we will need. */
static const CustomData_MeshMasks dependency_data_mask{CD_MASK_PROP_ALL | CD_MASK_MDEFORMVERT,
                                                       CD_MASK_PROP_ALL,
                                                       CD_MASK_PROP_ALL,
                                                       CD_MASK_PROP_ALL,
                                                       CD_MASK_PROP_ALL};

static void add_collection_relation(const ModifierUpdateDepsgraphContext *ctx,
                                    Collection &collection)
{
  DEG_add_collection_geometry_relation(ctx->node, &collection, "Nodes Modifier");
  DEG_add_collection_geometry_customdata_mask(ctx->node, &collection, &dependency_data_mask);
}

static void add_object_relation(const ModifierUpdateDepsgraphContext *ctx, Object &object)
{
  DEG_add_object_relation(ctx->node, &object, DEG_OB_COMP_TRANSFORM, "Nodes Modifier");
  if (&(ID &)object != &ctx->object->id) {
    if (object.type == OB_EMPTY && object.instance_collection != nullptr) {
      add_collection_relation(ctx, *object.instance_collection);
    }
    else if (DEG_object_has_geometry_component(&object)) {
      DEG_add_object_relation(ctx->node, &object, DEG_OB_COMP_GEOMETRY, "Nodes Modifier");
      DEG_add_customdata_mask(ctx->node, &object, &dependency_data_mask);
    }
  }
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group == nullptr) {
    return;
  }

  DEG_add_node_tree_output_relation(ctx->node, nmd->node_group, "Nodes Modifier");

  bool needs_own_transform_relation = false;
  Set<ID *> used_ids;
  find_used_ids_from_settings(nmd->settings, used_ids);
  Set<const bNodeTree *> checked_groups;
  process_nodes_for_depsgraph(
      *nmd->node_group, used_ids, needs_own_transform_relation, checked_groups);

  if (ctx->object->type == OB_CURVES) {
    Curves *curves_id = static_cast<Curves *>(ctx->object->data);
    if (curves_id->surface != nullptr) {
      used_ids.add(&curves_id->surface->id);
    }
  }

  for (ID *id : used_ids) {
    switch ((ID_Type)GS(id->name)) {
      case ID_OB: {
        Object *object = reinterpret_cast<Object *>(id);
        add_object_relation(ctx, *object);
        break;
      }
      case ID_GR: {
        Collection *collection = reinterpret_cast<Collection *>(id);
        add_collection_relation(ctx, *collection);
        break;
      }
      case ID_IM:
      case ID_TE: {
        DEG_add_generic_id_relation(ctx->node, id, "Nodes Modifier");
        break;
      }
      default: {
        /* Purposefully don't add relations for materials. While there are material sockets,
         * the pointers are only passed around as handles rather than dereferenced. */
        break;
      }
    }
  }

  if (needs_own_transform_relation) {
    DEG_add_depends_on_transform_relation(ctx->node, "Nodes Modifier");
  }
}

static bool check_tree_for_time_node(const bNodeTree &tree, Set<const bNodeTree *> &checked_groups)
{
  if (!checked_groups.add(&tree)) {
    return false;
  }
  tree.ensure_topology_cache();
  if (!tree.nodes_by_type("GeometryNodeInputSceneTime").is_empty()) {
    return true;
  }
  if (!tree.nodes_by_type("GeometryNodeSimulationInput").is_empty()) {
    return true;
  }
  for (const bNode *node : tree.group_nodes()) {
    if (const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id)) {
      if (check_tree_for_time_node(*sub_tree, checked_groups)) {
        return true;
      }
    }
  }
  return false;
}

static bool dependsOnTime(struct Scene * /*scene*/, ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  const bNodeTree *tree = nmd->node_group;
  if (tree == nullptr) {
    return false;
  }
  Set<const bNodeTree *> checked_groups;
  return check_tree_for_time_node(*tree, checked_groups);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  walk(userData, ob, (ID **)&nmd->node_group, IDWALK_CB_USER);

  struct ForeachSettingData {
    IDWalkFunc walk;
    void *userData;
    Object *ob;
  } settings = {walk, userData, ob};

  IDP_foreach_property(
      nmd->settings.properties,
      IDP_TYPE_FILTER_ID,
      [](IDProperty *id_prop, void *user_data) {
        ForeachSettingData *settings = (ForeachSettingData *)user_data;
        settings->walk(
            settings->userData, settings->ob, (ID **)&id_prop->data.pointer, IDWALK_CB_USER);
      },
      &settings);
}

static void foreachTexLink(ModifierData *md, Object *ob, TexWalkFunc walk, void *userData)
{
  walk(userData, ob, md, "texture");
}

static bool isDisabled(const struct Scene * /*scene*/, ModifierData *md, bool /*useRenderParams*/)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);

  if (nmd->node_group == nullptr) {
    return true;
  }

  return false;
}

static bool logging_enabled(const ModifierEvalContext *ctx)
{
  if (!DEG_is_active(ctx->depsgraph)) {
    return false;
  }
  if ((ctx->flag & MOD_APPLY_ORCO) != 0) {
    return false;
  }
  return true;
}

static const std::string use_attribute_suffix = "_use_attribute";
static const std::string attribute_name_suffix = "_attribute_name";

/**
 * \return Whether using an attribute to input values of this type is supported.
 */
static bool socket_type_has_attribute_toggle(const bNodeSocket &socket)
{
  return ELEM(socket.type, SOCK_FLOAT, SOCK_VECTOR, SOCK_BOOLEAN, SOCK_RGBA, SOCK_INT);
}

/**
 * \return Whether using an attribute to input values of this type is supported, and the node
 * group's input for this socket accepts a field rather than just single values.
 */
static bool input_has_attribute_toggle(const bNodeTree &node_tree, const int socket_index)
{
  BLI_assert(node_tree.runtime->field_inferencing_interface);
  const nodes::FieldInferencingInterface &field_interface =
      *node_tree.runtime->field_inferencing_interface;
  return field_interface.inputs[socket_index] != nodes::InputSocketFieldType::None;
}

static std::unique_ptr<IDProperty, bke::idprop::IDPropertyDeleter> id_property_create_from_socket(
    const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT: {
      const bNodeSocketValueFloat *value = static_cast<const bNodeSocketValueFloat *>(
          socket.default_value);
      auto property = bke::idprop::create(socket.identifier, value->value);
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = double(value->min);
      ui_data->soft_max = double(value->max);
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_INT: {
      const bNodeSocketValueInt *value = static_cast<const bNodeSocketValueInt *>(
          socket.default_value);
      auto property = bke::idprop::create(socket.identifier, value->value);
      IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = value->min;
      ui_data->soft_max = value->max;
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_VECTOR: {
      const bNodeSocketValueVector *value = static_cast<const bNodeSocketValueVector *>(
          socket.default_value);
      auto property = bke::idprop::create(
          socket.identifier, Span<float>{value->value[0], value->value[1], value->value[2]});
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = value->subtype;
      ui_data->soft_min = double(value->min);
      ui_data->soft_max = double(value->max);
      ui_data->default_array = (double *)MEM_mallocN(sizeof(double[3]), "mod_prop_default");
      ui_data->default_array_len = 3;
      for (const int i : IndexRange(3)) {
        ui_data->default_array[i] = double(value->value[i]);
      }
      return property;
    }
    case SOCK_RGBA: {
      const bNodeSocketValueRGBA *value = static_cast<const bNodeSocketValueRGBA *>(
          socket.default_value);
      auto property = bke::idprop::create(
          socket.identifier,
          Span<float>{value->value[0], value->value[1], value->value[2], value->value[3]});
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property.get());
      ui_data->base.rna_subtype = PROP_COLOR;
      ui_data->default_array = (double *)MEM_mallocN(sizeof(double[4]), __func__);
      ui_data->default_array_len = 4;
      ui_data->min = 0.0;
      ui_data->max = FLT_MAX;
      ui_data->soft_min = 0.0;
      ui_data->soft_max = 1.0;
      for (const int i : IndexRange(4)) {
        ui_data->default_array[i] = double(value->value[i]);
      }
      return property;
    }
    case SOCK_BOOLEAN: {
      const bNodeSocketValueBoolean *value = static_cast<const bNodeSocketValueBoolean *>(
          socket.default_value);
      auto property = bke::idprop::create_bool(socket.identifier, value->value);
      IDPropertyUIDataBool *ui_data = (IDPropertyUIDataBool *)IDP_ui_data_ensure(property.get());
      ui_data->default_value = value->value != 0;
      return property;
    }
    case SOCK_STRING: {
      const bNodeSocketValueString *value = static_cast<const bNodeSocketValueString *>(
          socket.default_value);
      auto property = bke::idprop::create(socket.identifier, value->value);
      IDPropertyUIDataString *ui_data = (IDPropertyUIDataString *)IDP_ui_data_ensure(
          property.get());
      ui_data->default_value = BLI_strdup(value->value);
      return property;
    }
    case SOCK_OBJECT: {
      const bNodeSocketValueObject *value = static_cast<const bNodeSocketValueObject *>(
          socket.default_value);
      auto property = bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
      IDPropertyUIDataID *ui_data = (IDPropertyUIDataID *)IDP_ui_data_ensure(property.get());
      ui_data->id_type = ID_OB;
      return property;
    }
    case SOCK_COLLECTION: {
      const bNodeSocketValueCollection *value = static_cast<const bNodeSocketValueCollection *>(
          socket.default_value);
      return bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_TEXTURE: {
      const bNodeSocketValueTexture *value = static_cast<const bNodeSocketValueTexture *>(
          socket.default_value);
      return bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_IMAGE: {
      const bNodeSocketValueImage *value = static_cast<const bNodeSocketValueImage *>(
          socket.default_value);
      return bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
    }
    case SOCK_MATERIAL: {
      const bNodeSocketValueMaterial *value = static_cast<const bNodeSocketValueMaterial *>(
          socket.default_value);
      return bke::idprop::create(socket.identifier, reinterpret_cast<ID *>(value->value));
    }
  }
  return nullptr;
}

static bool id_property_type_matches_socket(const bNodeSocket &socket, const IDProperty &property)
{
  switch (socket.type) {
    case SOCK_FLOAT:
      return ELEM(property.type, IDP_FLOAT, IDP_DOUBLE);
    case SOCK_INT:
      return property.type == IDP_INT;
    case SOCK_VECTOR:
      return property.type == IDP_ARRAY && property.subtype == IDP_FLOAT && property.len == 3;
    case SOCK_RGBA:
      return property.type == IDP_ARRAY && property.subtype == IDP_FLOAT && property.len == 4;
    case SOCK_BOOLEAN:
      return property.type == IDP_BOOLEAN;
    case SOCK_STRING:
      return property.type == IDP_STRING;
    case SOCK_OBJECT:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_IMAGE:
    case SOCK_MATERIAL:
      return property.type == IDP_ID;
  }
  BLI_assert_unreachable();
  return false;
}

static void init_socket_cpp_value_from_property(const IDProperty &property,
                                                const eNodeSocketDatatype socket_value_type,
                                                void *r_value)
{
  switch (socket_value_type) {
    case SOCK_FLOAT: {
      float value = 0.0f;
      if (property.type == IDP_FLOAT) {
        value = IDP_Float(&property);
      }
      else if (property.type == IDP_DOUBLE) {
        value = float(IDP_Double(&property));
      }
      new (r_value) fn::ValueOrField<float>(value);
      break;
    }
    case SOCK_INT: {
      int value = IDP_Int(&property);
      new (r_value) fn::ValueOrField<int>(value);
      break;
    }
    case SOCK_VECTOR: {
      float3 value;
      copy_v3_v3(value, (const float *)IDP_Array(&property));
      new (r_value) fn::ValueOrField<float3>(value);
      break;
    }
    case SOCK_RGBA: {
      ColorGeometry4f value;
      copy_v4_v4((float *)value, (const float *)IDP_Array(&property));
      new (r_value) fn::ValueOrField<ColorGeometry4f>(value);
      break;
    }
    case SOCK_BOOLEAN: {
      const bool value = IDP_Bool(&property);
      new (r_value) fn::ValueOrField<bool>(value);
      break;
    }
    case SOCK_STRING: {
      std::string value = IDP_String(&property);
      new (r_value) fn::ValueOrField<std::string>(std::move(value));
      break;
    }
    case SOCK_OBJECT: {
      ID *id = IDP_Id(&property);
      Object *object = (id && GS(id->name) == ID_OB) ? (Object *)id : nullptr;
      *(Object **)r_value = object;
      break;
    }
    case SOCK_COLLECTION: {
      ID *id = IDP_Id(&property);
      Collection *collection = (id && GS(id->name) == ID_GR) ? (Collection *)id : nullptr;
      *(Collection **)r_value = collection;
      break;
    }
    case SOCK_TEXTURE: {
      ID *id = IDP_Id(&property);
      Tex *texture = (id && GS(id->name) == ID_TE) ? (Tex *)id : nullptr;
      *(Tex **)r_value = texture;
      break;
    }
    case SOCK_IMAGE: {
      ID *id = IDP_Id(&property);
      Image *image = (id && GS(id->name) == ID_IM) ? (Image *)id : nullptr;
      *(Image **)r_value = image;
      break;
    }
    case SOCK_MATERIAL: {
      ID *id = IDP_Id(&property);
      Material *material = (id && GS(id->name) == ID_MA) ? (Material *)id : nullptr;
      *(Material **)r_value = material;
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static void update_input_properties_from_node_tree(const bNodeTree &tree,
                                                   const IDProperty *old_properties,
                                                   IDProperty &properties)
{
  tree.ensure_topology_cache();
  const Span<const bNodeSocket *> tree_inputs = tree.interface_inputs();
  for (const int i : tree_inputs.index_range()) {
    const bNodeSocket &socket = *tree_inputs[i];
    IDProperty *new_prop = id_property_create_from_socket(socket).release();
    if (new_prop == nullptr) {
      /* Out of the set of supported input sockets, only
       * geometry sockets aren't added to the modifier. */
      BLI_assert(socket.type == SOCK_GEOMETRY);
      continue;
    }

    new_prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY;
    if (socket.description[0] != '\0') {
      IDPropertyUIData *ui_data = IDP_ui_data_ensure(new_prop);
      ui_data->description = BLI_strdup(socket.description);
    }
    IDP_AddToGroup(&properties, new_prop);

    if (old_properties != nullptr) {
      const IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties, socket.identifier);
      if (old_prop != nullptr) {
        if (id_property_type_matches_socket(socket, *old_prop)) {
          /* #IDP_CopyPropertyContent replaces the UI data as well, which we don't (we only
           * want to replace the values). So release it temporarily and replace it after. */
          IDPropertyUIData *ui_data = new_prop->ui_data;
          new_prop->ui_data = nullptr;
          IDP_CopyPropertyContent(new_prop, old_prop);
          if (new_prop->ui_data != nullptr) {
            IDP_ui_data_free(new_prop);
          }
          new_prop->ui_data = ui_data;
        }
        else if (old_prop->type == IDP_INT && new_prop->type == IDP_BOOLEAN) {
          /* Support versioning from integer to boolean property values. The actual value is stored
           * in the same variable for both types. */
          new_prop->data.val = old_prop->data.val != 0;
        }
      }
    }

    if (socket_type_has_attribute_toggle(socket)) {
      const std::string use_attribute_id = socket.identifier + use_attribute_suffix;
      const std::string attribute_name_id = socket.identifier + attribute_name_suffix;

      IDPropertyTemplate idprop = {0};
      IDProperty *use_attribute_prop = IDP_New(IDP_INT, &idprop, use_attribute_id.c_str());
      IDP_AddToGroup(&properties, use_attribute_prop);

      IDProperty *attribute_prop = IDP_New(IDP_STRING, &idprop, attribute_name_id.c_str());
      IDP_AddToGroup(&properties, attribute_prop);

      if (old_properties == nullptr) {
        if (socket.default_attribute_name && socket.default_attribute_name[0] != '\0') {
          IDP_AssignStringMaxSize(attribute_prop, socket.default_attribute_name, MAX_NAME);
          IDP_Int(use_attribute_prop) = 1;
        }
      }
      else {
        IDProperty *old_prop_use_attribute = IDP_GetPropertyFromGroup(old_properties,
                                                                      use_attribute_id.c_str());
        if (old_prop_use_attribute != nullptr) {
          IDP_CopyPropertyContent(use_attribute_prop, old_prop_use_attribute);
        }

        IDProperty *old_attribute_name_prop = IDP_GetPropertyFromGroup(old_properties,
                                                                       attribute_name_id.c_str());
        if (old_attribute_name_prop != nullptr) {
          IDP_CopyPropertyContent(attribute_prop, old_attribute_name_prop);
        }
      }
    }
  }
}

static void update_output_properties_from_node_tree(const bNodeTree &tree,
                                                    const IDProperty *old_properties,
                                                    IDProperty &properties)
{
  tree.ensure_topology_cache();
  const Span<const bNodeSocket *> tree_outputs = tree.interface_outputs();
  for (const int i : tree_outputs.index_range()) {
    const bNodeSocket &socket = *tree_outputs[i];
    if (!socket_type_has_attribute_toggle(socket)) {
      continue;
    }

    const std::string idprop_name = socket.identifier + attribute_name_suffix;
    IDProperty *new_prop = IDP_NewString("", idprop_name.c_str());
    if (socket.description[0] != '\0') {
      IDPropertyUIData *ui_data = IDP_ui_data_ensure(new_prop);
      ui_data->description = BLI_strdup(socket.description);
    }
    IDP_AddToGroup(&properties, new_prop);

    if (old_properties == nullptr) {
      if (socket.default_attribute_name && socket.default_attribute_name[0] != '\0') {
        IDP_AssignStringMaxSize(new_prop, socket.default_attribute_name, MAX_NAME);
      }
    }
    else {
      IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties, idprop_name.c_str());
      if (old_prop != nullptr) {
        /* #IDP_CopyPropertyContent replaces the UI data as well, which we don't (we only
         * want to replace the values). So release it temporarily and replace it after. */
        IDPropertyUIData *ui_data = new_prop->ui_data;
        new_prop->ui_data = nullptr;
        IDP_CopyPropertyContent(new_prop, old_prop);
        if (new_prop->ui_data != nullptr) {
          IDP_ui_data_free(new_prop);
        }
        new_prop->ui_data = ui_data;
      }
    }
  }
}

}  // namespace blender

void MOD_nodes_update_interface(Object *object, NodesModifierData *nmd)
{
  using namespace blender;
  if (nmd->node_group == nullptr) {
    if (nmd->settings.properties) {
      IDP_FreeProperty(nmd->settings.properties);
      nmd->settings.properties = nullptr;
    }
    return;
  }

  IDProperty *old_properties = nmd->settings.properties;
  {
    IDPropertyTemplate idprop = {0};
    nmd->settings.properties = IDP_New(IDP_GROUP, &idprop, "Nodes Modifier Settings");
  }
  IDProperty *new_properties = nmd->settings.properties;

  update_input_properties_from_node_tree(*nmd->node_group, old_properties, *new_properties);
  update_output_properties_from_node_tree(*nmd->node_group, old_properties, *new_properties);

  if (old_properties != nullptr) {
    IDP_FreeProperty(old_properties);
  }

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
}

namespace blender {

static void initialize_group_input(const bNodeTree &tree,
                                   const IDProperty *properties,
                                   const int input_index,
                                   void *r_value)
{
  const bNodeSocket &io_input = *tree.interface_inputs()[input_index];
  const bNodeSocketType &socket_type = *io_input.typeinfo;
  const eNodeSocketDatatype socket_data_type = static_cast<eNodeSocketDatatype>(io_input.type);
  if (properties == nullptr) {
    socket_type.get_geometry_nodes_cpp_value(io_input, r_value);
    return;
  }
  const IDProperty *property = IDP_GetPropertyFromGroup(properties, io_input.identifier);
  if (property == nullptr) {
    socket_type.get_geometry_nodes_cpp_value(io_input, r_value);
    return;
  }
  if (!id_property_type_matches_socket(io_input, *property)) {
    socket_type.get_geometry_nodes_cpp_value(io_input, r_value);
    return;
  }

  if (!input_has_attribute_toggle(tree, input_index)) {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
    return;
  }

  const IDProperty *property_use_attribute = IDP_GetPropertyFromGroup(
      properties, (io_input.identifier + use_attribute_suffix).c_str());
  const IDProperty *property_attribute_name = IDP_GetPropertyFromGroup(
      properties, (io_input.identifier + attribute_name_suffix).c_str());
  if (property_use_attribute == nullptr || property_attribute_name == nullptr) {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
    return;
  }

  const bool use_attribute = IDP_Int(property_use_attribute) != 0;
  if (use_attribute) {
    const StringRef attribute_name{IDP_String(property_attribute_name)};
    if (!bke::allow_procedural_attribute_access(attribute_name)) {
      init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
      return;
    }
    fn::GField attribute_field = bke::AttributeFieldInput::Create(attribute_name,
                                                                  *socket_type.base_cpp_type);
    const auto *value_or_field_cpp_type = fn::ValueOrFieldCPPType::get_from_self(
        *socket_type.geometry_nodes_cpp_type);
    BLI_assert(value_or_field_cpp_type != nullptr);
    value_or_field_cpp_type->construct_from_field(r_value, std::move(attribute_field));
  }
  else {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
  }
}

static const lf::FunctionNode *find_viewer_lf_node(const bNode &viewer_bnode)
{
  if (const nodes::GeometryNodesLazyFunctionGraphInfo *lf_graph_info =
          nodes::ensure_geometry_nodes_lazy_function_graph(viewer_bnode.owner_tree()))
  {
    return lf_graph_info->mapping.viewer_node_map.lookup_default(&viewer_bnode, nullptr);
  }
  return nullptr;
}
static const lf::FunctionNode *find_group_lf_node(const bNode &group_bnode)
{
  if (const nodes::GeometryNodesLazyFunctionGraphInfo *lf_graph_info =
          nodes::ensure_geometry_nodes_lazy_function_graph(group_bnode.owner_tree()))
  {
    return lf_graph_info->mapping.group_node_map.lookup_default(&group_bnode, nullptr);
  }
  return nullptr;
}

static void find_side_effect_nodes_for_viewer_path(
    const ViewerPath &viewer_path,
    const NodesModifierData &nmd,
    const ModifierEvalContext &ctx,
    MultiValueMap<ComputeContextHash, const lf::FunctionNode *> &r_side_effect_nodes)
{
  const std::optional<ed::viewer_path::ViewerPathForGeometryNodesViewer> parsed_path =
      ed::viewer_path::parse_geometry_nodes_viewer(viewer_path);
  if (!parsed_path.has_value()) {
    return;
  }
  if (parsed_path->object != DEG_get_original_object(ctx.object)) {
    return;
  }
  if (parsed_path->modifier_name != nmd.modifier.name) {
    return;
  }

  ComputeContextBuilder compute_context_builder;
  compute_context_builder.push<bke::ModifierComputeContext>(parsed_path->modifier_name);

  const bNodeTree *group = nmd.node_group;
  Stack<const bNode *> group_node_stack;
  for (const int32_t group_node_id : parsed_path->group_node_ids) {
    const bNode *found_node = group->node_by_id(group_node_id);
    if (found_node == nullptr) {
      return;
    }
    if (found_node->id == nullptr) {
      return;
    }
    if (found_node->is_muted()) {
      return;
    }
    group_node_stack.push(found_node);
    group = reinterpret_cast<bNodeTree *>(found_node->id);
    compute_context_builder.push<bke::NodeGroupComputeContext>(*found_node);
  }

  const bNode *found_viewer_node = group->node_by_id(parsed_path->viewer_node_id);
  if (found_viewer_node == nullptr) {
    return;
  }
  const lf::FunctionNode *lf_viewer_node = find_viewer_lf_node(*found_viewer_node);
  if (lf_viewer_node == nullptr) {
    return;
  }

  /* Not only mark the viewer node as having side effects, but also all group nodes it is contained
   * in. */
  r_side_effect_nodes.add_non_duplicates(compute_context_builder.hash(), lf_viewer_node);
  compute_context_builder.pop();
  while (!compute_context_builder.is_empty()) {
    const lf::FunctionNode *lf_group_node = find_group_lf_node(*group_node_stack.pop());
    if (lf_group_node == nullptr) {
      return;
    }

    r_side_effect_nodes.add_non_duplicates(compute_context_builder.hash(), lf_group_node);
    compute_context_builder.pop();
  }
}

static void find_side_effect_nodes(
    const NodesModifierData &nmd,
    const ModifierEvalContext &ctx,
    MultiValueMap<ComputeContextHash, const lf::FunctionNode *> &r_side_effect_nodes)
{
  Main *bmain = DEG_get_bmain(ctx.depsgraph);
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  if (wm == nullptr) {
    return;
  }
  LISTBASE_FOREACH (const wmWindow *, window, &wm->windows) {
    const bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    const WorkSpace *workspace = BKE_workspace_active_get(window->workspace_hook);
    find_side_effect_nodes_for_viewer_path(workspace->viewer_path, nmd, ctx, r_side_effect_nodes);
    LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
      const SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      if (sl->spacetype == SPACE_SPREADSHEET) {
        const SpaceSpreadsheet &sspreadsheet = *reinterpret_cast<const SpaceSpreadsheet *>(sl);
        find_side_effect_nodes_for_viewer_path(
            sspreadsheet.viewer_path, nmd, ctx, r_side_effect_nodes);
      }
      if (sl->spacetype == SPACE_VIEW3D) {
        const View3D &v3d = *reinterpret_cast<const View3D *>(sl);
        find_side_effect_nodes_for_viewer_path(v3d.viewer_path, nmd, ctx, r_side_effect_nodes);
      }
    }
  }
}

static void find_socket_log_contexts(const NodesModifierData &nmd,
                                     const ModifierEvalContext &ctx,
                                     Set<ComputeContextHash> &r_socket_log_contexts)
{
  Main *bmain = DEG_get_bmain(ctx.depsgraph);
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  if (wm == nullptr) {
    return;
  }
  LISTBASE_FOREACH (const wmWindow *, window, &wm->windows) {
    const bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
      const SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      if (sl->spacetype == SPACE_NODE) {
        const SpaceNode &snode = *reinterpret_cast<const SpaceNode *>(sl);
        if (const std::optional<ComputeContextHash> hash =
                geo_log::GeoModifierLog::get_compute_context_hash_for_node_editor(
                    snode, nmd.modifier.name))
        {
          r_socket_log_contexts.add(*hash);
        }
      }
    }
  }
}

static void clear_runtime_data(NodesModifierData *nmd)
{
  if (nmd->runtime_eval_log != nullptr) {
    delete static_cast<geo_log::GeoModifierLog *>(nmd->runtime_eval_log);
    nmd->runtime_eval_log = nullptr;
  }
}

struct OutputAttributeInfo {
  fn::GField field;
  StringRefNull name;
};

struct OutputAttributeToStore {
  GeometryComponentType component_type;
  eAttrDomain domain;
  StringRefNull name;
  GMutableSpan data;
};

/**
 * The output attributes are organized based on their domain, because attributes on the same domain
 * can be evaluated together.
 */
static MultiValueMap<eAttrDomain, OutputAttributeInfo> find_output_attributes_to_store(
    const bNodeTree &tree,
    const IDProperty *properties,
    const bNode &output_node,
    Span<GMutablePointer> output_values)
{
  MultiValueMap<eAttrDomain, OutputAttributeInfo> outputs_by_domain;
  for (const bNodeSocket *socket : output_node.input_sockets().drop_front(1).drop_back(1)) {
    if (!socket_type_has_attribute_toggle(*socket)) {
      continue;
    }

    const std::string prop_name = socket->identifier + attribute_name_suffix;
    const IDProperty *prop = IDP_GetPropertyFromGroup(properties, prop_name.c_str());
    if (prop == nullptr) {
      continue;
    }
    const StringRefNull attribute_name = IDP_String(prop);
    if (attribute_name.is_empty()) {
      continue;
    }
    if (!bke::allow_procedural_attribute_access(attribute_name)) {
      continue;
    }

    const int index = socket->index();
    const GPointer value = output_values[index];
    const auto *value_or_field_type = fn::ValueOrFieldCPPType::get_from_self(*value.type());
    BLI_assert(value_or_field_type != nullptr);
    const fn::GField field = value_or_field_type->as_field(value.get());

    const bNodeSocket *interface_socket = (const bNodeSocket *)BLI_findlink(&tree.outputs, index);
    const eAttrDomain domain = (eAttrDomain)interface_socket->attribute_domain;
    OutputAttributeInfo output_info;
    output_info.field = std::move(field);
    output_info.name = attribute_name;
    outputs_by_domain.add(domain, std::move(output_info));
  }
  return outputs_by_domain;
}

/**
 * The computed values are stored in newly allocated arrays. They still have to be moved to the
 * actual geometry.
 */
static Vector<OutputAttributeToStore> compute_attributes_to_store(
    const GeometrySet &geometry,
    const MultiValueMap<eAttrDomain, OutputAttributeInfo> &outputs_by_domain)
{
  Vector<OutputAttributeToStore> attributes_to_store;
  for (const GeometryComponentType component_type : {GEO_COMPONENT_TYPE_MESH,
                                                     GEO_COMPONENT_TYPE_POINT_CLOUD,
                                                     GEO_COMPONENT_TYPE_CURVE,
                                                     GEO_COMPONENT_TYPE_INSTANCES})
  {
    if (!geometry.has(component_type)) {
      continue;
    }
    const GeometryComponent &component = *geometry.get_component_for_read(component_type);
    const bke::AttributeAccessor attributes = *component.attributes();
    for (const auto item : outputs_by_domain.items()) {
      const eAttrDomain domain = item.key;
      const Span<OutputAttributeInfo> outputs_info = item.value;
      if (!attributes.domain_supported(domain)) {
        continue;
      }
      const int domain_size = attributes.domain_size(domain);
      bke::GeometryFieldContext field_context{component, domain};
      fn::FieldEvaluator field_evaluator{field_context, domain_size};
      for (const OutputAttributeInfo &output_info : outputs_info) {
        const CPPType &type = output_info.field.cpp_type();
        const bke::AttributeValidator validator = attributes.lookup_validator(output_info.name);
        OutputAttributeToStore store{
            component_type,
            domain,
            output_info.name,
            GMutableSpan{
                type, MEM_malloc_arrayN(domain_size, type.size(), __func__), domain_size}};
        fn::GField field = validator.validate_field_if_necessary(output_info.field);
        field_evaluator.add_with_destination(std::move(field), store.data);
        attributes_to_store.append(store);
      }
      field_evaluator.evaluate();
    }
  }
  return attributes_to_store;
}

static void store_computed_output_attributes(
    GeometrySet &geometry, const Span<OutputAttributeToStore> attributes_to_store)
{
  for (const OutputAttributeToStore &store : attributes_to_store) {
    GeometryComponent &component = geometry.get_component_for_write(store.component_type);
    bke::MutableAttributeAccessor attributes = *component.attributes_for_write();

    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(store.data.type());
    const std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(
        store.name);

    /* Attempt to remove the attribute if it already exists but the domain and type don't match.
     * Removing the attribute won't succeed if it is built in and non-removable. */
    if (meta_data.has_value() &&
        (meta_data->domain != store.domain || meta_data->data_type != data_type))
    {
      attributes.remove(store.name);
    }

    /* Try to create the attribute reusing the stored buffer. This will only succeed if the
     * attribute didn't exist before, or if it existed but was removed above. */
    if (attributes.add(
            store.name, store.domain, data_type, bke::AttributeInitMoveArray(store.data.data())))
    {
      continue;
    }

    bke::GAttributeWriter attribute = attributes.lookup_or_add_for_write(
        store.name, store.domain, data_type);
    if (attribute) {
      attribute.varray.set_all(store.data.data());
      attribute.finish();
    }

    /* We were unable to reuse the data, so it must be destructed and freed. */
    store.data.type().destruct_n(store.data.data(), store.data.size());
    MEM_freeN(store.data.data());
  }
}

static void store_output_attributes(GeometrySet &geometry,
                                    const bNodeTree &tree,
                                    const IDProperty *properties,
                                    const bNode &output_node,
                                    Span<GMutablePointer> output_values)
{
  /* All new attribute values have to be computed before the geometry is actually changed. This is
   * necessary because some fields might depend on attributes that are overwritten. */
  MultiValueMap<eAttrDomain, OutputAttributeInfo> outputs_by_domain =
      find_output_attributes_to_store(tree, properties, output_node, output_values);
  Vector<OutputAttributeToStore> attributes_to_store = compute_attributes_to_store(
      geometry, outputs_by_domain);
  store_computed_output_attributes(geometry, attributes_to_store);
}

static void prepare_simulation_states_for_evaluation(const NodesModifierData &nmd,
                                                     NodesModifierData &nmd_orig,
                                                     const ModifierEvalContext &ctx,
                                                     nodes::GeoNodesModifierData &exec_data)
{
  const Main *bmain = DEG_get_bmain(ctx.depsgraph);
  const SubFrame current_frame = DEG_get_ctime(ctx.depsgraph);
  const Scene *scene = DEG_get_input_scene(ctx.depsgraph);
  const SubFrame start_frame = scene->r.sfra;
  const bool is_start_frame = current_frame == start_frame;

  if (DEG_is_active(ctx.depsgraph)) {
    if (nmd_orig.simulation_cache == nullptr) {
      nmd_orig.simulation_cache = MEM_new<bke::sim::ModifierSimulationCache>(__func__);
    }

    {
      /* Try to use baked data. */
      const StringRefNull bmain_path = BKE_main_blendfile_path(bmain);
      if (nmd_orig.simulation_cache->cache_state() != bke::sim::CacheState::Baked &&
          !bmain_path.is_empty())
      {
        if (!StringRef(nmd.simulation_bake_directory).is_empty()) {
          if (const char *base_path = ID_BLEND_PATH(bmain, &ctx.object->id)) {
            char absolute_bake_dir[FILE_MAX];
            STRNCPY(absolute_bake_dir, nmd.simulation_bake_directory);
            BLI_path_abs(absolute_bake_dir, base_path);
            nmd_orig.simulation_cache->try_discover_bake(absolute_bake_dir);
          }
        }
      }
    }

    {
      /* Invalidate cached data on user edits. */
      if (nmd.modifier.flag & eModifierFlag_UserModified) {
        if (nmd_orig.simulation_cache->cache_state() != bke::sim::CacheState::Baked) {
          nmd_orig.simulation_cache->invalidate();
        }
      }
    }

    {
      /* Reset cached data if necessary. */
      const bke::sim::StatesAroundFrame sim_states =
          nmd_orig.simulation_cache->get_states_around_frame(current_frame);
      if (nmd_orig.simulation_cache->cache_state() == bke::sim::CacheState::Invalid &&
          (current_frame == start_frame ||
           (sim_states.current == nullptr && sim_states.prev == nullptr &&
            sim_states.next != nullptr)))
      {
        nmd_orig.simulation_cache->reset();
      }
    }
    /* Decide if a new simulation state should be created in this evaluation. */
    const bke::sim::StatesAroundFrame sim_states =
        nmd_orig.simulation_cache->get_states_around_frame(current_frame);
    if (nmd_orig.simulation_cache->cache_state() != bke::sim::CacheState::Baked) {
      if (sim_states.current == nullptr) {
        if (is_start_frame || !nmd_orig.simulation_cache->has_states()) {
          bke::sim::ModifierSimulationState &current_sim_state =
              nmd_orig.simulation_cache->get_state_at_frame_for_write(current_frame);
          exec_data.current_simulation_state_for_write = &current_sim_state;
          exec_data.simulation_time_delta = 0.0f;
          if (!is_start_frame) {
            /* When starting a new simulation at another frame than the start frame, it can't match
             * what would be baked, so invalidate it immediately. */
            nmd_orig.simulation_cache->invalidate();
          }
        }
        else if (sim_states.prev != nullptr && sim_states.next == nullptr) {
          const float max_delta_frames = 1.0f;
          const float scene_delta_frames = float(current_frame) - float(sim_states.prev->frame);
          const float delta_frames = std::min(max_delta_frames, scene_delta_frames);
          if (delta_frames != scene_delta_frames) {
            nmd_orig.simulation_cache->invalidate();
          }
          bke::sim::ModifierSimulationState &current_sim_state =
              nmd_orig.simulation_cache->get_state_at_frame_for_write(current_frame);
          exec_data.current_simulation_state_for_write = &current_sim_state;
          const float delta_seconds = delta_frames / FPS;
          exec_data.simulation_time_delta = delta_seconds;
        }
      }
    }
  }

  if (nmd_orig.simulation_cache == nullptr) {
    return;
  }

  /* Load read-only states to give nodes access to cached data. */
  const bke::sim::StatesAroundFrame sim_states =
      nmd_orig.simulation_cache->get_states_around_frame(current_frame);
  if (sim_states.current) {
    sim_states.current->state.ensure_bake_loaded();
    exec_data.current_simulation_state = &sim_states.current->state;
  }
  if (sim_states.prev) {
    sim_states.prev->state.ensure_bake_loaded();
    exec_data.prev_simulation_state = &sim_states.prev->state;
    if (sim_states.next) {
      sim_states.next->state.ensure_bake_loaded();
      exec_data.next_simulation_state = &sim_states.next->state;
      exec_data.simulation_state_mix_factor =
          (float(current_frame) - float(sim_states.prev->frame)) /
          (float(sim_states.next->frame) - float(sim_states.prev->frame));
    }
  }
}

/**
 * Evaluate a node group to compute the output geometry.
 */
static GeometrySet compute_geometry(const bNodeTree &btree,
                                    const nodes::GeometryNodesLazyFunctionGraphInfo &lf_graph_info,
                                    const bNode &output_node,
                                    GeometrySet input_geometry_set,
                                    NodesModifierData *nmd,
                                    const ModifierEvalContext *ctx)
{
  NodesModifierData *nmd_orig = reinterpret_cast<NodesModifierData *>(
      BKE_modifier_get_original(ctx->object, &nmd->modifier));

  const nodes::GeometryNodeLazyFunctionGraphMapping &mapping = lf_graph_info.mapping;

  Vector<const lf::OutputSocket *> graph_inputs = mapping.group_input_sockets;
  graph_inputs.extend(mapping.group_output_used_sockets);
  graph_inputs.extend(mapping.attribute_set_by_geometry_output.values().begin(),
                      mapping.attribute_set_by_geometry_output.values().end());
  Vector<const lf::InputSocket *> graph_outputs = mapping.standard_group_output_sockets;

  Array<GMutablePointer> param_inputs(graph_inputs.size());
  Array<GMutablePointer> param_outputs(graph_outputs.size());
  Array<std::optional<lf::ValueUsage>> param_input_usages(graph_inputs.size());
  Array<lf::ValueUsage> param_output_usages(graph_outputs.size(), lf::ValueUsage::Used);
  Array<bool> param_set_outputs(graph_outputs.size(), false);

  nodes::GeometryNodesLazyFunctionLogger lf_logger(lf_graph_info);
  nodes::GeometryNodesLazyFunctionSideEffectProvider lf_side_effect_provider;

  lf::GraphExecutor graph_executor{
      lf_graph_info.graph, graph_inputs, graph_outputs, &lf_logger, &lf_side_effect_provider};

  nodes::GeoNodesModifierData geo_nodes_modifier_data;
  geo_nodes_modifier_data.depsgraph = ctx->depsgraph;
  geo_nodes_modifier_data.self_object = ctx->object;
  auto eval_log = std::make_unique<geo_log::GeoModifierLog>();

  prepare_simulation_states_for_evaluation(*nmd, *nmd_orig, *ctx, geo_nodes_modifier_data);

  Set<ComputeContextHash> socket_log_contexts;
  if (logging_enabled(ctx)) {
    geo_nodes_modifier_data.eval_log = eval_log.get();

    find_socket_log_contexts(*nmd, *ctx, socket_log_contexts);
    geo_nodes_modifier_data.socket_log_contexts = &socket_log_contexts;
  }
  MultiValueMap<ComputeContextHash, const lf::FunctionNode *> r_side_effect_nodes;
  find_side_effect_nodes(*nmd, *ctx, r_side_effect_nodes);
  geo_nodes_modifier_data.side_effect_nodes = &r_side_effect_nodes;
  nodes::GeoNodesLFUserData user_data;
  user_data.modifier_data = &geo_nodes_modifier_data;
  bke::ModifierComputeContext modifier_compute_context{nullptr, nmd->modifier.name};
  user_data.compute_context = &modifier_compute_context;

  LinearAllocator<> allocator;
  Vector<GMutablePointer> inputs_to_destruct;

  const IDProperty *properties = nmd->settings.properties;
  int input_index = -1;
  for (const int i : btree.interface_inputs().index_range()) {
    input_index++;
    const bNodeSocket &interface_socket = *btree.interface_inputs()[i];
    if (interface_socket.type == SOCK_GEOMETRY && input_index == 0) {
      param_inputs[input_index] = &input_geometry_set;
      continue;
    }

    const CPPType *type = interface_socket.typeinfo->geometry_nodes_cpp_type;
    BLI_assert(type != nullptr);
    void *value = allocator.allocate(type->size(), type->alignment());
    initialize_group_input(btree, properties, i, value);
    param_inputs[input_index] = {type, value};
    inputs_to_destruct.append({type, value});
  }

  Array<bool> output_used_inputs(btree.interface_outputs().size(), true);
  for (const int i : btree.interface_outputs().index_range()) {
    input_index++;
    param_inputs[input_index] = &output_used_inputs[i];
  }

  Array<bke::AnonymousAttributeSet> attributes_to_propagate(
      mapping.attribute_set_by_geometry_output.size());
  for (const int i : attributes_to_propagate.index_range()) {
    input_index++;
    param_inputs[input_index] = &attributes_to_propagate[i];
  }

  for (const int i : graph_outputs.index_range()) {
    const lf::InputSocket &socket = *graph_outputs[i];
    const CPPType &type = socket.type();
    void *buffer = allocator.allocate(type.size(), type.alignment());
    param_outputs[i] = {type, buffer};
  }

  nodes::GeoNodesLFLocalUserData local_user_data(user_data);

  lf::Context lf_context(graph_executor.init_storage(allocator), &user_data, &local_user_data);
  lf::BasicParams lf_params{graph_executor,
                            param_inputs,
                            param_outputs,
                            param_input_usages,
                            param_output_usages,
                            param_set_outputs};
  graph_executor.execute(lf_params, lf_context);
  graph_executor.destruct_storage(lf_context.storage);

  for (GMutablePointer &ptr : inputs_to_destruct) {
    ptr.destruct();
  }

  GeometrySet output_geometry_set = std::move(*static_cast<GeometrySet *>(param_outputs[0].get()));
  store_output_attributes(output_geometry_set, btree, properties, output_node, param_outputs);

  for (GMutablePointer &ptr : param_outputs) {
    ptr.destruct();
  }

  if (logging_enabled(ctx)) {
    delete static_cast<geo_log::GeoModifierLog *>(nmd_orig->runtime_eval_log);
    nmd_orig->runtime_eval_log = eval_log.release();
  }

  if (DEG_is_active(ctx->depsgraph)) {
    /* When caching is turned off, remove all states except the last which was just created in this
     * evaluation. Check if active status to avoid changing original data in other depsgraphs. */
    if (!(ctx->object->flag & OB_FLAG_USE_SIMULATION_CACHE)) {
      nmd_orig->simulation_cache->clear_prev_states();
    }
  }

  return output_geometry_set;
}

/**
 * \note This could be done in #initialize_group_input, though that would require adding the
 * the object as a parameter, so it's likely better to this check as a separate step.
 */
static void check_property_socket_sync(const Object *ob, ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);

  int geometry_socket_count = 0;

  int i;
  LISTBASE_FOREACH_INDEX (const bNodeSocket *, socket, &nmd->node_group->inputs, i) {
    /* The first socket is the special geometry socket for the modifier object. */
    if (i == 0 && socket->type == SOCK_GEOMETRY) {
      geometry_socket_count++;
      continue;
    }

    IDProperty *property = IDP_GetPropertyFromGroup(nmd->settings.properties, socket->identifier);
    if (property == nullptr) {
      if (socket->type == SOCK_GEOMETRY) {
        geometry_socket_count++;
      }
      else {
        BKE_modifier_set_error(ob, md, "Missing property for input socket \"%s\"", socket->name);
      }
      continue;
    }

    if (!id_property_type_matches_socket(*socket, *property)) {
      BKE_modifier_set_error(
          ob, md, "Property type does not match input socket \"(%s)\"", socket->name);
      continue;
    }
  }

  if (geometry_socket_count == 1) {
    if (((bNodeSocket *)nmd->node_group->inputs.first)->type != SOCK_GEOMETRY) {
      BKE_modifier_set_error(ob, md, "Node group's geometry input must be the first");
    }
  }
}

static void modifyGeometry(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           GeometrySet &geometry_set)
{
  using namespace blender;
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group == nullptr) {
    return;
  }

  const bNodeTree &tree = *nmd->node_group;
  tree.ensure_topology_cache();
  check_property_socket_sync(ctx->object, md);

  const bNode *output_node = tree.group_output_node();
  if (output_node == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Node group must have a group output node");
    geometry_set.clear();
    return;
  }

  Span<const bNodeSocket *> group_outputs = output_node->input_sockets().drop_back(1);
  if (group_outputs.is_empty()) {
    BKE_modifier_set_error(ctx->object, md, "Node group must have an output socket");
    geometry_set.clear();
    return;
  }

  const bNodeSocket *first_output_socket = group_outputs[0];
  if (!STREQ(first_output_socket->idname, "NodeSocketGeometry")) {
    BKE_modifier_set_error(ctx->object, md, "Node group's first output must be a geometry");
    geometry_set.clear();
    return;
  }

  const nodes::GeometryNodesLazyFunctionGraphInfo *lf_graph_info =
      nodes::ensure_geometry_nodes_lazy_function_graph(tree);
  if (lf_graph_info == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Cannot evaluate node group");
    geometry_set.clear();
    return;
  }

  bool use_orig_index_verts = false;
  bool use_orig_index_edges = false;
  bool use_orig_index_polys = false;
  if (const Mesh *mesh = geometry_set.get_mesh_for_read()) {
    use_orig_index_verts = CustomData_has_layer(&mesh->vdata, CD_ORIGINDEX);
    use_orig_index_edges = CustomData_has_layer(&mesh->edata, CD_ORIGINDEX);
    use_orig_index_polys = CustomData_has_layer(&mesh->pdata, CD_ORIGINDEX);
  }

  geometry_set = compute_geometry(
      tree, *lf_graph_info, *output_node, std::move(geometry_set), nmd, ctx);

  if (use_orig_index_verts || use_orig_index_edges || use_orig_index_polys) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      /* Add #CD_ORIGINDEX layers if they don't exist already. This is required because the
       * #eModifierTypeFlag_SupportsMapping flag is set. If the layers did not exist before, it is
       * assumed that the output mesh does not have a mapping to the original mesh. */
      if (use_orig_index_verts) {
        CustomData_add_layer(&mesh->vdata, CD_ORIGINDEX, CD_SET_DEFAULT, mesh->totvert);
      }
      if (use_orig_index_edges) {
        CustomData_add_layer(&mesh->edata, CD_ORIGINDEX, CD_SET_DEFAULT, mesh->totedge);
      }
      if (use_orig_index_polys) {
        CustomData_add_layer(&mesh->pdata, CD_ORIGINDEX, CD_SET_DEFAULT, mesh->totpoly);
      }
    }
  }
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  GeometrySet geometry_set = GeometrySet::create_with_mesh(mesh, GeometryOwnershipType::Editable);

  modifyGeometry(md, ctx, geometry_set);

  Mesh *new_mesh = geometry_set.get_component_for_write<MeshComponent>().release();
  if (new_mesh == nullptr) {
    return BKE_mesh_new_nomain(0, 0, 0, 0);
  }
  return new_mesh;
}

static void modifyGeometrySet(ModifierData *md,
                              const ModifierEvalContext *ctx,
                              GeometrySet *geometry_set)
{
  modifyGeometry(md, ctx, *geometry_set);
}

struct AttributeSearchData {
  uint32_t object_session_uid;
  char modifier_name[MAX_NAME];
  char socket_identifier[MAX_NAME];
  bool is_output;
};
/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
BLI_STATIC_ASSERT(std::is_trivially_destructible_v<AttributeSearchData>, "");

static NodesModifierData *get_modifier_data(Main &bmain,
                                            const wmWindowManager &wm,
                                            const AttributeSearchData &data)
{
  if (ED_screen_animation_playing(&wm)) {
    /* Work around an issue where the attribute search exec function has stale pointers when data
     * is reallocated when evaluating the node tree, causing a crash. This would be solved by
     * allowing the UI search data to own arbitrary memory rather than just referencing it. */
    return nullptr;
  }

  const Object *object = (Object *)BKE_libblock_find_session_uuid(
      &bmain, ID_OB, data.object_session_uid);
  if (object == nullptr) {
    return nullptr;
  }
  ModifierData *md = BKE_modifiers_findby_name(object, data.modifier_name);
  if (md == nullptr) {
    return nullptr;
  }
  BLI_assert(md->type == eModifierType_Nodes);
  return reinterpret_cast<NodesModifierData *>(md);
}

static geo_log::GeoTreeLog *get_root_tree_log(const NodesModifierData &nmd)
{
  if (nmd.runtime_eval_log == nullptr) {
    return nullptr;
  }
  auto &modifier_log = *static_cast<geo_log::GeoModifierLog *>(nmd.runtime_eval_log);
  bke::ModifierComputeContext compute_context{nullptr, nmd.modifier.name};
  return &modifier_log.get_tree_log(compute_context.hash());
}

static void attribute_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  AttributeSearchData &data = *static_cast<AttributeSearchData *>(arg);
  const NodesModifierData *nmd = get_modifier_data(*CTX_data_main(C), *CTX_wm_manager(C), data);
  if (nmd == nullptr) {
    return;
  }
  if (nmd->node_group == nullptr) {
    return;
  }
  geo_log::GeoTreeLog *tree_log = get_root_tree_log(*nmd);
  if (tree_log == nullptr) {
    return;
  }
  tree_log->ensure_existing_attributes();
  nmd->node_group->ensure_topology_cache();

  Vector<const bNodeSocket *> sockets_to_check;
  if (data.is_output) {
    for (const bNode *node : nmd->node_group->nodes_by_type("NodeGroupOutput")) {
      for (const bNodeSocket *socket : node->input_sockets()) {
        if (socket->type == SOCK_GEOMETRY) {
          sockets_to_check.append(socket);
        }
      }
    }
  }
  else {
    for (const bNode *node : nmd->node_group->group_input_nodes()) {
      for (const bNodeSocket *socket : node->output_sockets()) {
        if (socket->type == SOCK_GEOMETRY) {
          sockets_to_check.append(socket);
        }
      }
    }
  }
  Set<StringRef> names;
  Vector<const geo_log::GeometryAttributeInfo *> attributes;
  for (const bNodeSocket *socket : sockets_to_check) {
    const geo_log::ValueLog *value_log = tree_log->find_socket_value_log(*socket);
    if (value_log == nullptr) {
      continue;
    }
    if (const auto *geo_log = dynamic_cast<const geo_log::GeometryInfoLog *>(value_log)) {
      for (const geo_log::GeometryAttributeInfo &attribute : geo_log->attributes) {
        if (names.add(attribute.name)) {
          attributes.append(&attribute);
        }
      }
    }
  }
  ui::attribute_search_add_items(str, data.is_output, attributes.as_span(), items, is_first);
}

static void attribute_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  if (item_v == nullptr) {
    return;
  }
  AttributeSearchData &data = *static_cast<AttributeSearchData *>(data_v);
  const auto &item = *static_cast<const geo_log::GeometryAttributeInfo *>(item_v);
  const NodesModifierData *nmd = get_modifier_data(*CTX_data_main(C), *CTX_wm_manager(C), data);
  if (nmd == nullptr) {
    return;
  }

  const std::string attribute_prop_name = data.socket_identifier + attribute_name_suffix;
  IDProperty &name_property = *IDP_GetPropertyFromGroup(nmd->settings.properties,
                                                        attribute_prop_name.c_str());
  IDP_AssignString(&name_property, item.name.c_str());

  ED_undo_push(C, "Assign Attribute Name");
}

static void add_attribute_search_button(const bContext &C,
                                        uiLayout *layout,
                                        const NodesModifierData &nmd,
                                        PointerRNA *md_ptr,
                                        const StringRefNull rna_path_attribute_name,
                                        const bNodeSocket &socket,
                                        const bool is_output)
{
  if (nmd.runtime_eval_log == nullptr) {
    uiItemR(layout, md_ptr, rna_path_attribute_name.c_str(), 0, "", ICON_NONE);
    return;
  }

  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but = uiDefIconTextButR(block,
                                 UI_BTYPE_SEARCH_MENU,
                                 0,
                                 ICON_NONE,
                                 "",
                                 0,
                                 0,
                                 10 * UI_UNIT_X, /* Dummy value, replaced by layout system. */
                                 UI_UNIT_Y,
                                 md_ptr,
                                 rna_path_attribute_name.c_str(),
                                 0,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 socket.description);

  const Object *object = ED_object_context(&C);
  BLI_assert(object != nullptr);
  if (object == nullptr) {
    return;
  }

  AttributeSearchData *data = MEM_new<AttributeSearchData>(__func__);
  data->object_session_uid = object->id.session_uuid;
  STRNCPY(data->modifier_name, nmd.modifier.name);
  STRNCPY(data->socket_identifier, socket.identifier);
  data->is_output = is_output;

  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         attribute_search_update_fn,
                         static_cast<void *>(data),
                         true,
                         nullptr,
                         attribute_search_exec_fn,
                         nullptr);

  char *attribute_name = RNA_string_get_alloc(
      md_ptr, rna_path_attribute_name.c_str(), nullptr, 0, nullptr);
  const bool access_allowed = bke::allow_procedural_attribute_access(attribute_name);
  MEM_freeN(attribute_name);
  if (!access_allowed) {
    UI_but_flag_enable(but, UI_BUT_REDALERT);
  }
}

static void add_attribute_search_or_value_buttons(const bContext &C,
                                                  uiLayout *layout,
                                                  const NodesModifierData &nmd,
                                                  PointerRNA *md_ptr,
                                                  const bNodeSocket &socket)
{
  char socket_id_esc[sizeof(socket.identifier) * 2];
  BLI_str_escape(socket_id_esc, socket.identifier, sizeof(socket_id_esc));
  const std::string rna_path = "[\"" + std::string(socket_id_esc) + "\"]";
  const std::string rna_path_use_attribute = "[\"" + std::string(socket_id_esc) +
                                             use_attribute_suffix + "\"]";
  const std::string rna_path_attribute_name = "[\"" + std::string(socket_id_esc) +
                                              attribute_name_suffix + "\"]";

  /* We're handling this manually in this case. */
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *split = uiLayoutSplit(layout, 0.4f, false);
  uiLayout *name_row = uiLayoutRow(split, false);
  uiLayoutSetAlignment(name_row, UI_LAYOUT_ALIGN_RIGHT);

  const int use_attribute = RNA_int_get(md_ptr, rna_path_use_attribute.c_str()) != 0;
  if (socket.type == SOCK_BOOLEAN && !use_attribute) {
    uiItemL(name_row, "", ICON_NONE);
  }
  else {
    uiItemL(name_row, socket.name, ICON_NONE);
  }

  uiLayout *prop_row = uiLayoutRow(split, true);
  if (socket.type == SOCK_BOOLEAN) {
    uiLayoutSetPropSep(prop_row, false);
    uiLayoutSetAlignment(prop_row, UI_LAYOUT_ALIGN_EXPAND);
  }

  if (use_attribute) {
    add_attribute_search_button(C, prop_row, nmd, md_ptr, rna_path_attribute_name, socket, false);
    uiItemL(layout, "", ICON_BLANK1);
  }
  else {
    const char *name = socket.type == SOCK_BOOLEAN ? socket.name : "";
    uiItemR(prop_row, md_ptr, rna_path.c_str(), 0, name, ICON_NONE);
    uiItemDecoratorR(layout, md_ptr, rna_path.c_str(), -1);
  }

  PointerRNA props;
  uiItemFullO(prop_row,
              "object.geometry_nodes_input_attribute_toggle",
              "",
              ICON_SPREADSHEET,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              0,
              &props);
  RNA_string_set(&props, "modifier_name", nmd.modifier.name);
  RNA_string_set(&props, "prop_path", rna_path_use_attribute.c_str());
}

/* Drawing the properties manually with #uiItemR instead of #uiDefAutoButsRNA allows using
 * the node socket identifier for the property names, since they are unique, but also having
 * the correct label displayed in the UI. */
static void draw_property_for_socket(const bContext &C,
                                     uiLayout *layout,
                                     NodesModifierData *nmd,
                                     PointerRNA *bmain_ptr,
                                     PointerRNA *md_ptr,
                                     const bNodeSocket &socket,
                                     const int socket_index)
{
  /* The property should be created in #MOD_nodes_update_interface with the correct type. */
  IDProperty *property = IDP_GetPropertyFromGroup(nmd->settings.properties, socket.identifier);

  /* IDProperties can be removed with python, so there could be a situation where
   * there isn't a property for a socket or it doesn't have the correct type. */
  if (property == nullptr || !id_property_type_matches_socket(socket, *property)) {
    return;
  }

  char socket_id_esc[sizeof(socket.identifier) * 2];
  BLI_str_escape(socket_id_esc, socket.identifier, sizeof(socket_id_esc));

  char rna_path[sizeof(socket_id_esc) + 4];
  SNPRINTF(rna_path, "[\"%s\"]", socket_id_esc);

  uiLayout *row = uiLayoutRow(layout, true);
  uiLayoutSetPropDecorate(row, true);

  /* Use #uiItemPointerR to draw pointer properties because #uiItemR would not have enough
   * information about what type of ID to select for editing the values. This is because
   * pointer IDProperties contain no information about their type. */
  switch (socket.type) {
    case SOCK_OBJECT: {
      uiItemPointerR(row, md_ptr, rna_path, bmain_ptr, "objects", socket.name, ICON_OBJECT_DATA);
      break;
    }
    case SOCK_COLLECTION: {
      uiItemPointerR(
          row, md_ptr, rna_path, bmain_ptr, "collections", socket.name, ICON_OUTLINER_COLLECTION);
      break;
    }
    case SOCK_MATERIAL: {
      uiItemPointerR(row, md_ptr, rna_path, bmain_ptr, "materials", socket.name, ICON_MATERIAL);
      break;
    }
    case SOCK_TEXTURE: {
      uiItemPointerR(row, md_ptr, rna_path, bmain_ptr, "textures", socket.name, ICON_TEXTURE);
      break;
    }
    case SOCK_IMAGE: {
      uiItemPointerR(row, md_ptr, rna_path, bmain_ptr, "images", socket.name, ICON_IMAGE);
      break;
    }
    default: {
      if (input_has_attribute_toggle(*nmd->node_group, socket_index)) {
        add_attribute_search_or_value_buttons(C, row, *nmd, md_ptr, socket);
      }
      else {
        uiItemR(row, md_ptr, rna_path, 0, socket.name, ICON_NONE);
      }
    }
  }
  if (!input_has_attribute_toggle(*nmd->node_group, socket_index)) {
    uiItemL(row, "", ICON_BLANK1);
  }
}

static void draw_property_for_output_socket(const bContext &C,
                                            uiLayout *layout,
                                            const NodesModifierData &nmd,
                                            PointerRNA *md_ptr,
                                            const bNodeSocket &socket)
{
  char socket_id_esc[sizeof(socket.identifier) * 2];
  BLI_str_escape(socket_id_esc, socket.identifier, sizeof(socket_id_esc));
  const std::string rna_path_attribute_name = "[\"" + StringRef(socket_id_esc) +
                                              attribute_name_suffix + "\"]";

  uiLayout *split = uiLayoutSplit(layout, 0.4f, false);
  uiLayout *name_row = uiLayoutRow(split, false);
  uiLayoutSetAlignment(name_row, UI_LAYOUT_ALIGN_RIGHT);
  uiItemL(name_row, socket.name, ICON_NONE);

  uiLayout *row = uiLayoutRow(split, true);
  add_attribute_search_button(C, row, nmd, md_ptr, rna_path_attribute_name, socket, true);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Main *bmain = CTX_data_main(C);

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);
  /* Decorators are added manually for supported properties because the
   * attribute/value toggle requires a manually built layout anyway. */
  uiLayoutSetPropDecorate(layout, false);

  uiTemplateID(layout,
               C,
               ptr,
               "node_group",
               "node.new_geometry_node_group_assign",
               nullptr,
               nullptr,
               0,
               false,
               nullptr);

  if (nmd->node_group != nullptr && nmd->settings.properties != nullptr) {
    PointerRNA bmain_ptr;
    RNA_main_pointer_create(bmain, &bmain_ptr);

    int socket_index;
    LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &nmd->node_group->inputs, socket_index) {
      if (!(socket->flag & SOCK_HIDE_IN_MODIFIER)) {
        draw_property_for_socket(*C, layout, nmd, &bmain_ptr, ptr, *socket, socket_index);
      }
    }
  }

  /* Draw node warnings. */
  geo_log::GeoTreeLog *tree_log = get_root_tree_log(*nmd);
  if (tree_log != nullptr) {
    tree_log->ensure_node_warnings();
    for (const geo_log::NodeWarning &warning : tree_log->all_warnings) {
      if (warning.type != geo_log::NodeWarningType::Info) {
        uiItemL(layout, warning.message.c_str(), ICON_ERROR);
      }
    }
  }

  modifier_panel_end(layout, ptr);
}

static void output_attribute_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, true);

  bool has_output_attribute = false;
  if (nmd->node_group != nullptr && nmd->settings.properties != nullptr) {
    LISTBASE_FOREACH (bNodeSocket *, socket, &nmd->node_group->outputs) {
      if (socket_type_has_attribute_toggle(*socket)) {
        has_output_attribute = true;
        draw_property_for_output_socket(*C, layout, *nmd, ptr, *socket);
      }
    }
  }
  if (!has_output_attribute) {
    uiItemL(layout, TIP_("No group output attributes connected"), ICON_INFO);
  }
}

static void internal_dependencies_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  geo_log::GeoTreeLog *tree_log = get_root_tree_log(*nmd);
  if (tree_log == nullptr) {
    return;
  }

  tree_log->ensure_used_named_attributes();
  const Map<StringRefNull, geo_log::NamedAttributeUsage> &usage_by_attribute =
      tree_log->used_named_attributes;

  if (usage_by_attribute.is_empty()) {
    uiItemL(layout, IFACE_("No named attributes used"), ICON_INFO);
    return;
  }

  struct NameWithUsage {
    StringRefNull name;
    geo_log::NamedAttributeUsage usage;
  };

  Vector<NameWithUsage> sorted_used_attribute;
  for (auto &&item : usage_by_attribute.items()) {
    sorted_used_attribute.append({item.key, item.value});
  }
  std::sort(sorted_used_attribute.begin(),
            sorted_used_attribute.end(),
            [](const NameWithUsage &a, const NameWithUsage &b) {
              return BLI_strcasecmp_natural(a.name.c_str(), b.name.c_str()) <= 0;
            });

  for (const NameWithUsage &attribute : sorted_used_attribute) {
    const StringRefNull attribute_name = attribute.name;
    const geo_log::NamedAttributeUsage usage = attribute.usage;

    /* #uiLayoutRowWithHeading doesn't seem to work in this case. */
    uiLayout *split = uiLayoutSplit(layout, 0.4f, false);

    std::stringstream ss;
    Vector<std::string> usages;
    if ((usage & geo_log::NamedAttributeUsage::Read) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("Read"));
    }
    if ((usage & geo_log::NamedAttributeUsage::Write) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("Write"));
    }
    if ((usage & geo_log::NamedAttributeUsage::Remove) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("Remove"));
    }
    for (const int i : usages.index_range()) {
      ss << usages[i];
      if (i < usages.size() - 1) {
        ss << ", ";
      }
    }

    uiLayout *row = uiLayoutRow(split, false);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
    uiLayoutSetActive(row, false);
    uiItemL(row, ss.str().c_str(), ICON_NONE);

    row = uiLayoutRow(split, false);
    uiItemL(row, attribute_name.c_str(), ICON_NONE);
  }
}

static void panelRegister(ARegionType *region_type)
{
  using namespace blender;
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Nodes, panel_draw);
  modifier_subpanel_register(region_type,
                             "output_attributes",
                             N_("Output Attributes"),
                             nullptr,
                             output_attribute_panel_draw,
                             panel_type);
  modifier_subpanel_register(region_type,
                             "internal_dependencies",
                             N_("Internal Dependencies"),
                             nullptr,
                             internal_dependencies_panel_draw,
                             panel_type);
}

static void blendWrite(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);

  BLO_write_struct(writer, NodesModifierData, nmd);

  BLO_write_string(writer, nmd->simulation_bake_directory);

  if (nmd->settings.properties != nullptr) {
    Map<IDProperty *, IDPropertyUIDataBool *> boolean_props;
    if (!BLO_write_is_undo(writer)) {
      /* Boolean properties are added automatically for boolean node group inputs. Integer
       * properties are automatically converted to boolean sockets where applicable as well.
       * However, boolean properties will crash old versions of Blender, so convert them to integer
       * properties for writing. The actual value is stored in the same variable for both types */
      LISTBASE_FOREACH (IDProperty *, prop, &nmd->settings.properties->data.group) {
        if (prop->type == IDP_BOOLEAN) {
          boolean_props.add_new(prop, reinterpret_cast<IDPropertyUIDataBool *>(prop->ui_data));
          prop->type = IDP_INT;
          prop->ui_data = nullptr;
        }
      }
    }

    /* Note that the property settings are based on the socket type info
     * and don't necessarily need to be written, but we can't just free them. */
    IDP_BlendWrite(writer, nmd->settings.properties);

    if (!BLO_write_is_undo(writer)) {
      LISTBASE_FOREACH (IDProperty *, prop, &nmd->settings.properties->data.group) {
        if (prop->type == IDP_INT) {
          if (IDPropertyUIDataBool **ui_data = boolean_props.lookup_ptr(prop)) {
            prop->type = IDP_BOOLEAN;
            if (ui_data) {
              prop->ui_data = reinterpret_cast<IDPropertyUIData *>(*ui_data);
            }
          }
        }
      }
    }
  }
}

static void blendRead(BlendDataReader *reader, ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  BLO_read_data_address(reader, &nmd->simulation_bake_directory);
  if (nmd->node_group == nullptr) {
    nmd->settings.properties = nullptr;
  }
  else {
    BLO_read_data_address(reader, &nmd->settings.properties);
    IDP_BlendDataRead(reader, &nmd->settings.properties);
  }
  nmd->runtime_eval_log = nullptr;
  nmd->simulation_cache = nullptr;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
  NodesModifierData *tnmd = reinterpret_cast<NodesModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);

  tnmd->runtime_eval_log = nullptr;
  tnmd->simulation_cache = nullptr;
  tnmd->simulation_bake_directory = nmd->simulation_bake_directory ?
                                        BLI_strdup(nmd->simulation_bake_directory) :
                                        nullptr;

  if (nmd->settings.properties != nullptr) {
    tnmd->settings.properties = IDP_CopyProperty_ex(nmd->settings.properties, flag);
  }
}

static void freeData(ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->settings.properties != nullptr) {
    IDP_FreeProperty_ex(nmd->settings.properties, false);
    nmd->settings.properties = nullptr;
  }

  MEM_delete(nmd->simulation_cache);
  MEM_SAFE_FREE(nmd->simulation_bake_directory);

  clear_runtime_data(nmd);
}

static void requiredDataMask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  /* We don't know what the node tree will need. If there are vertex groups, it is likely that the
   * node tree wants to access them. */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  r_cddata_masks->vmask |= CD_MASK_PROP_ALL;
}

}  // namespace blender

ModifierTypeInfo modifierType_Nodes = {
    /*name*/ N_("GeometryNodes"),
    /*structName*/ "NodesModifierData",
    /*structSize*/ sizeof(NodesModifierData),
    /*srna*/ &RNA_NodesModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/
    static_cast<ModifierTypeFlag>(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
                                  eModifierTypeFlag_SupportsEditmode |
                                  eModifierTypeFlag_EnableInEditmode |
                                  eModifierTypeFlag_SupportsMapping),
    /*icon*/ ICON_GEOMETRY_NODES,

    /*copyData*/ blender::copyData,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ blender::modifyMesh,
    /*modifyGeometrySet*/ blender::modifyGeometrySet,

    /*initData*/ blender::initData,
    /*requiredDataMask*/ blender::requiredDataMask,
    /*freeData*/ blender::freeData,
    /*isDisabled*/ blender::isDisabled,
    /*updateDepsgraph*/ blender::updateDepsgraph,
    /*dependsOnTime*/ blender::dependsOnTime,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ blender::foreachIDLink,
    /*foreachTexLink*/ blender::foreachTexLink,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ blender::panelRegister,
    /*blendWrite*/ blender::blendWrite,
    /*blendRead*/ blender::blendRead,
};
