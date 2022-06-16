/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include <cstring>
#include <iostream>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_listbase.h"
#include "BLI_math_vec_types.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_search.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
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
#include "DNA_windowmanager_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_customdata.h"
#include "BKE_geometry_cache.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_object.h"
#include "BKE_pointcloud.h"
#include "BKE_screen.h"
#include "BKE_simulation.h"
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

#include "MOD_modifiertypes.h"
#include "MOD_nodes.h"
#include "MOD_nodes_evaluator.hh"
#include "MOD_ui_common.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_spreadsheet.h"
#include "ED_undo.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry.h"
#include "NOD_geometry_nodes_eval_log.hh"
#include "NOD_node_declaration.hh"

#include "FN_field.hh"
#include "FN_field_cpp_type.hh"
#include "FN_multi_function.hh"

using blender::Array;
using blender::ColorGeometry4f;
using blender::CPPType;
using blender::destruct_ptr;
using blender::float3;
using blender::FunctionRef;
using blender::GMutablePointer;
using blender::GMutableSpan;
using blender::GPointer;
using blender::GVArray;
using blender::IndexRange;
using blender::Map;
using blender::MultiValueMap;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::StringRef;
using blender::StringRefNull;
using blender::Vector;
using blender::bke::OutputAttribute;
using blender::fn::Field;
using blender::fn::GField;
using blender::fn::ValueOrField;
using blender::fn::ValueOrFieldCPPType;
using blender::nodes::FieldInferencingInterface;
using blender::nodes::GeoNodeExecParams;
using blender::nodes::InputSocketFieldType;
using blender::threading::EnumerableThreadSpecific;
using namespace blender::fn::multi_function_types;
using namespace blender::nodes::derived_node_tree_types;
using geo_log::eNamedAttrUsage;
using geo_log::GeometryAttributeInfo;

static bool isIterativeModifier(ModifierData *md)
{
  NodesModifierData *nmd = (NodesModifierData *)md;
  return nmd->node_group && nmd->node_group->type == NTREE_PARTICLES;
}

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
        if (Collection *collection =
                ((bNodeSocketValueCollection *)socket->default_value)->value) {
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

  return false;
}

static void process_nodes_for_depsgraph(const bNodeTree &tree,
                                        Set<ID *> &ids,
                                        bool &needs_own_transform_relation,
                                        bool &needs_rigid_body_sim)
{
  Set<const bNodeTree *> handled_groups;

  LISTBASE_FOREACH (const bNode *, node, &tree.nodes) {
    add_used_ids_from_sockets(node->inputs, ids);
    add_used_ids_from_sockets(node->outputs, ids);

    if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
      const bNodeTree *group = (bNodeTree *)node->id;
      if (group != nullptr && handled_groups.add(group)) {
        process_nodes_for_depsgraph(
            *group, ids, needs_own_transform_relation, needs_rigid_body_sim);
      }
    }
    needs_own_transform_relation |= node_needs_own_transform_relation(*node);
  }

  /* XXX dummy */
  needs_rigid_body_sim = true;
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
  bool needs_rigid_body_sim = false;
  Set<ID *> used_ids;
  find_used_ids_from_settings(nmd->settings, used_ids);
  process_nodes_for_depsgraph(
      *nmd->node_group, used_ids, needs_own_transform_relation, needs_rigid_body_sim);
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
      }
      default: {
        /* Purposefully don't add relations for materials. While there are material sockets,
         * the pointers are only passed around as handles rather than dereferenced. */
        break;
      }
    }
  }

  if (needs_own_transform_relation) {
    DEG_add_modifier_to_transform_relation(ctx->node, "Nodes Modifier");
  }

  if (needs_rigid_body_sim) {
    DEG_add_
  }
}

static bool check_tree_for_time_node(const bNodeTree &tree,
                                     Set<const bNodeTree *> &r_checked_trees)
{
  if (!r_checked_trees.add(&tree)) {
    return false;
  }
  LISTBASE_FOREACH (const bNode *, node, &tree.nodes) {
    if (node->type == GEO_NODE_INPUT_SCENE_TIME) {
      return true;
    }
    if (node->type == NODE_GROUP) {
      const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id);
      if (sub_tree && check_tree_for_time_node(*sub_tree, r_checked_trees)) {
        return true;
      }
    }
  }
  return false;
}

static bool dependsOnTime(struct Scene *UNUSED(scene), ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  const bNodeTree *tree = nmd->node_group;
  if (tree == nullptr) {
    return false;
  }
  if (tree->type == NTREE_PARTICLES) {
    return true;
  }
  else {
    Set<const bNodeTree *> checked_trees;
    return check_tree_for_time_node(*tree, checked_trees);
  }
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

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
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
  const FieldInferencingInterface &field_interface =
      *node_tree.runtime->field_inferencing_interface;
  return field_interface.inputs[socket_index] != InputSocketFieldType::None;
}

static IDProperty *id_property_create_from_socket(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *value = (bNodeSocketValueFloat *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.f = value->value;
      IDProperty *property = IDP_New(IDP_FLOAT, &idprop, socket.identifier);
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property);
      ui_data->base.rna_subtype = value->subtype;
      ui_data->min = ui_data->soft_min = (double)value->min;
      ui_data->max = ui_data->soft_max = (double)value->max;
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_INT: {
      bNodeSocketValueInt *value = (bNodeSocketValueInt *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.i = value->value;
      IDProperty *property = IDP_New(IDP_INT, &idprop, socket.identifier);
      IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)IDP_ui_data_ensure(property);
      ui_data->base.rna_subtype = value->subtype;
      ui_data->min = ui_data->soft_min = value->min;
      ui_data->max = ui_data->soft_max = value->max;
      ui_data->default_value = value->value;
      return property;
    }
    case SOCK_VECTOR: {
      bNodeSocketValueVector *value = (bNodeSocketValueVector *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.array.len = 3;
      idprop.array.type = IDP_FLOAT;
      IDProperty *property = IDP_New(IDP_ARRAY, &idprop, socket.identifier);
      copy_v3_v3((float *)IDP_Array(property), value->value);
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property);
      ui_data->base.rna_subtype = value->subtype;
      ui_data->min = ui_data->soft_min = (double)value->min;
      ui_data->max = ui_data->soft_max = (double)value->max;
      ui_data->default_array = (double *)MEM_mallocN(sizeof(double[3]), "mod_prop_default");
      ui_data->default_array_len = 3;
      for (const int i : IndexRange(3)) {
        ui_data->default_array[i] = double(value->value[i]);
      }
      return property;
    }
    case SOCK_RGBA: {
      bNodeSocketValueRGBA *value = (bNodeSocketValueRGBA *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.array.len = 4;
      idprop.array.type = IDP_FLOAT;
      IDProperty *property = IDP_New(IDP_ARRAY, &idprop, socket.identifier);
      copy_v4_v4((float *)IDP_Array(property), value->value);
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(property);
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
      bNodeSocketValueBoolean *value = (bNodeSocketValueBoolean *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.i = value->value != 0;
      IDProperty *property = IDP_New(IDP_INT, &idprop, socket.identifier);
      IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)IDP_ui_data_ensure(property);
      ui_data->min = ui_data->soft_min = 0;
      ui_data->max = ui_data->soft_max = 1;
      ui_data->default_value = value->value != 0;
      return property;
    }
    case SOCK_STRING: {
      bNodeSocketValueString *value = (bNodeSocketValueString *)socket.default_value;
      IDProperty *property = IDP_NewString(
          value->value, socket.identifier, BLI_strnlen(value->value, sizeof(value->value)) + 1);
      IDPropertyUIDataString *ui_data = (IDPropertyUIDataString *)IDP_ui_data_ensure(property);
      ui_data->default_value = BLI_strdup(value->value);
      return property;
    }
    case SOCK_OBJECT: {
      bNodeSocketValueObject *value = (bNodeSocketValueObject *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.id = (ID *)value->value;
      return IDP_New(IDP_ID, &idprop, socket.identifier);
    }
    case SOCK_COLLECTION: {
      bNodeSocketValueCollection *value = (bNodeSocketValueCollection *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.id = (ID *)value->value;
      return IDP_New(IDP_ID, &idprop, socket.identifier);
    }
    case SOCK_TEXTURE: {
      bNodeSocketValueTexture *value = (bNodeSocketValueTexture *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.id = (ID *)value->value;
      return IDP_New(IDP_ID, &idprop, socket.identifier);
    }
    case SOCK_IMAGE: {
      bNodeSocketValueImage *value = (bNodeSocketValueImage *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.id = (ID *)value->value;
      return IDP_New(IDP_ID, &idprop, socket.identifier);
    }
    case SOCK_MATERIAL: {
      bNodeSocketValueMaterial *value = (bNodeSocketValueMaterial *)socket.default_value;
      IDPropertyTemplate idprop = {0};
      idprop.id = (ID *)value->value;
      return IDP_New(IDP_ID, &idprop, socket.identifier);
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
      return property.type == IDP_INT;
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
        value = (float)IDP_Double(&property);
      }
      new (r_value) ValueOrField<float>(value);
      break;
    }
    case SOCK_INT: {
      int value = IDP_Int(&property);
      new (r_value) ValueOrField<int>(value);
      break;
    }
    case SOCK_VECTOR: {
      float3 value;
      copy_v3_v3(value, (const float *)IDP_Array(&property));
      new (r_value) ValueOrField<float3>(value);
      break;
    }
    case SOCK_RGBA: {
      blender::ColorGeometry4f value;
      copy_v4_v4((float *)value, (const float *)IDP_Array(&property));
      new (r_value) ValueOrField<ColorGeometry4f>(value);
      break;
    }
    case SOCK_BOOLEAN: {
      bool value = IDP_Int(&property) != 0;
      new (r_value) ValueOrField<bool>(value);
      break;
    }
    case SOCK_STRING: {
      std::string value = IDP_String(&property);
      new (r_value) ValueOrField<std::string>(std::move(value));
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

void MOD_nodes_update_interface(Object *object, NodesModifierData *nmd)
{
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

  int socket_index;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &nmd->node_group->inputs, socket_index) {
    IDProperty *new_prop = id_property_create_from_socket(*socket);
    if (new_prop == nullptr) {
      /* Out of the set of supported input sockets, only
       * geometry sockets aren't added to the modifier. */
      BLI_assert(socket->type == SOCK_GEOMETRY);
      continue;
    }

    new_prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY;
    if (socket->description[0] != '\0') {
      IDPropertyUIData *ui_data = IDP_ui_data_ensure(new_prop);
      ui_data->description = BLI_strdup(socket->description);
    }
    IDP_AddToGroup(nmd->settings.properties, new_prop);

    if (old_properties != nullptr) {
      IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties, socket->identifier);
      if (old_prop != nullptr && id_property_type_matches_socket(*socket, *old_prop)) {
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

    if (socket_type_has_attribute_toggle(*socket)) {
      const std::string use_attribute_id = socket->identifier + use_attribute_suffix;
      const std::string attribute_name_id = socket->identifier + attribute_name_suffix;

      IDPropertyTemplate idprop = {0};
      IDProperty *use_attribute_prop = IDP_New(IDP_INT, &idprop, use_attribute_id.c_str());
      IDP_AddToGroup(nmd->settings.properties, use_attribute_prop);

      IDProperty *attribute_prop = IDP_New(IDP_STRING, &idprop, attribute_name_id.c_str());
      IDP_AddToGroup(nmd->settings.properties, attribute_prop);

      if (old_properties == nullptr) {
        if (socket->default_attribute_name && socket->default_attribute_name[0] != '\0') {
          IDP_AssignString(attribute_prop, socket->default_attribute_name, MAX_NAME);
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

  LISTBASE_FOREACH (bNodeSocket *, socket, &nmd->node_group->outputs) {
    if (!socket_type_has_attribute_toggle(*socket)) {
      continue;
    }

    const std::string idprop_name = socket->identifier + attribute_name_suffix;
    IDProperty *new_prop = IDP_NewString("", idprop_name.c_str(), MAX_NAME);
    if (socket->description[0] != '\0') {
      IDPropertyUIData *ui_data = IDP_ui_data_ensure(new_prop);
      ui_data->description = BLI_strdup(socket->description);
    }
    IDP_AddToGroup(nmd->settings.properties, new_prop);

    if (old_properties == nullptr) {
      if (socket->default_attribute_name && socket->default_attribute_name[0] != '\0') {
        IDP_AssignString(new_prop, socket->default_attribute_name, MAX_NAME);
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

  if (old_properties != nullptr) {
    IDP_FreeProperty(old_properties);
  }

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
}

static void initialize_group_input(NodesModifierData &nmd,
                                   const OutputSocketRef &socket,
                                   void *r_value)
{
  const bNodeSocketType &socket_type = *socket.typeinfo();
  const bNodeSocket &bsocket = *socket.bsocket();
  const eNodeSocketDatatype socket_data_type = static_cast<eNodeSocketDatatype>(bsocket.type);
  if (nmd.settings.properties == nullptr) {
    socket_type.get_geometry_nodes_cpp_value(bsocket, r_value);
    return;
  }
  const IDProperty *property = IDP_GetPropertyFromGroup(nmd.settings.properties,
                                                        socket.identifier().c_str());
  if (property == nullptr) {
    socket_type.get_geometry_nodes_cpp_value(bsocket, r_value);
    return;
  }
  if (!id_property_type_matches_socket(bsocket, *property)) {
    socket_type.get_geometry_nodes_cpp_value(bsocket, r_value);
    return;
  }

  if (!input_has_attribute_toggle(*nmd.node_group, socket.index())) {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
    return;
  }

  const IDProperty *property_use_attribute = IDP_GetPropertyFromGroup(
      nmd.settings.properties, (socket.identifier() + use_attribute_suffix).c_str());
  const IDProperty *property_attribute_name = IDP_GetPropertyFromGroup(
      nmd.settings.properties, (socket.identifier() + attribute_name_suffix).c_str());
  if (property_use_attribute == nullptr || property_attribute_name == nullptr) {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
    return;
  }

  const bool use_attribute = IDP_Int(property_use_attribute) != 0;
  if (use_attribute) {
    const StringRef attribute_name{IDP_String(property_attribute_name)};
    if (!blender::bke::allow_procedural_attribute_access(attribute_name)) {
      init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
      return;
    }
    auto attribute_input = std::make_shared<blender::bke::AttributeFieldInput>(
        attribute_name, *socket_type.base_cpp_type);
    GField attribute_field{std::move(attribute_input), 0};
    const blender::fn::ValueOrFieldCPPType *cpp_type =
        dynamic_cast<const blender::fn::ValueOrFieldCPPType *>(
            socket_type.geometry_nodes_cpp_type);
    BLI_assert(cpp_type != nullptr);
    cpp_type->construct_from_field(r_value, std::move(attribute_field));
  }
  else {
    init_socket_cpp_value_from_property(*property, socket_data_type, r_value);
  }
}

static Vector<SpaceSpreadsheet *> find_spreadsheet_editors(Main *bmain)
{
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  if (wm == nullptr) {
    return {};
  }
  Vector<SpaceSpreadsheet *> spreadsheets;
  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *sl = (SpaceLink *)area->spacedata.first;
      if (sl->spacetype == SPACE_SPREADSHEET) {
        spreadsheets.append((SpaceSpreadsheet *)sl);
      }
    }
  }
  return spreadsheets;
}

static void find_sockets_to_preview_for_spreadsheet(SpaceSpreadsheet *sspreadsheet,
                                                    NodesModifierData *nmd,
                                                    const ModifierEvalContext *ctx,
                                                    const DerivedNodeTree &tree,
                                                    Set<DSocket> &r_sockets_to_preview)
{
  Vector<SpreadsheetContext *> context_path = sspreadsheet->context_path;
  if (context_path.size() < 3) {
    return;
  }
  if (context_path[0]->type != SPREADSHEET_CONTEXT_OBJECT) {
    return;
  }
  if (context_path[1]->type != SPREADSHEET_CONTEXT_MODIFIER) {
    return;
  }
  SpreadsheetContextObject *object_context = (SpreadsheetContextObject *)context_path[0];
  if (object_context->object != DEG_get_original_object(ctx->object)) {
    return;
  }
  SpreadsheetContextModifier *modifier_context = (SpreadsheetContextModifier *)context_path[1];
  if (StringRef(modifier_context->modifier_name) != nmd->modifier.name) {
    return;
  }
  for (SpreadsheetContext *context : context_path.as_span().drop_front(2)) {
    if (context->type != SPREADSHEET_CONTEXT_NODE) {
      return;
    }
  }

  Span<SpreadsheetContextNode *> nested_group_contexts =
      context_path.as_span().drop_front(2).drop_back(1).cast<SpreadsheetContextNode *>();
  SpreadsheetContextNode *last_context = (SpreadsheetContextNode *)context_path.last();

  const DTreeContext *context = &tree.root_context();
  for (SpreadsheetContextNode *node_context : nested_group_contexts) {
    const NodeTreeRef &tree_ref = context->tree();
    const NodeRef *found_node = nullptr;
    for (const NodeRef *node_ref : tree_ref.nodes()) {
      if (node_ref->name() == node_context->node_name) {
        found_node = node_ref;
        break;
      }
    }
    if (found_node == nullptr) {
      return;
    }
    context = context->child_context(*found_node);
    if (context == nullptr) {
      return;
    }
  }

  const NodeTreeRef &tree_ref = context->tree();
  for (const NodeRef *node_ref : tree_ref.nodes_by_type("GeometryNodeViewer")) {
    if (node_ref->name() == last_context->node_name) {
      const DNode viewer_node{context, node_ref};
      for (const InputSocketRef *input_socket : node_ref->inputs()) {
        if (input_socket->is_available() && input_socket->is_logically_linked()) {
          r_sockets_to_preview.add(DSocket{context, input_socket});
        }
      }
    }
  }
}

static void find_sockets_to_preview(NodesModifierData *nmd,
                                    const ModifierEvalContext *ctx,
                                    const DerivedNodeTree &tree,
                                    Set<DSocket> &r_sockets_to_preview)
{
  Main *bmain = DEG_get_bmain(ctx->depsgraph);

  /* Based on every visible spreadsheet context path, get a list of sockets that need to have their
   * intermediate geometries cached for display. */
  Vector<SpaceSpreadsheet *> spreadsheets = find_spreadsheet_editors(bmain);
  for (SpaceSpreadsheet *sspreadsheet : spreadsheets) {
    find_sockets_to_preview_for_spreadsheet(sspreadsheet, nmd, ctx, tree, r_sockets_to_preview);
  }
}

static void clear_runtime_data(NodesModifierData *nmd)
{
  if (nmd->runtime_eval_log != nullptr) {
    delete (geo_log::ModifierLog *)nmd->runtime_eval_log;
    nmd->runtime_eval_log = nullptr;
  }
}

struct OutputAttributeInfo {
  GField field;
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
    const NodesModifierData &nmd, const NodeRef &output_node, Span<GMutablePointer> output_values)
{
  MultiValueMap<eAttrDomain, OutputAttributeInfo> outputs_by_domain;
  for (const InputSocketRef *socket : output_node.inputs().drop_front(1).drop_back(1)) {
    if (!socket_type_has_attribute_toggle(*socket->bsocket())) {
      continue;
    }

    const std::string prop_name = socket->identifier() + attribute_name_suffix;
    const IDProperty *prop = IDP_GetPropertyFromGroup(nmd.settings.properties, prop_name.c_str());
    if (prop == nullptr) {
      continue;
    }
    const StringRefNull attribute_name = IDP_String(prop);
    if (attribute_name.is_empty()) {
      continue;
    }
    if (!blender::bke::allow_procedural_attribute_access(attribute_name)) {
      continue;
    }

    const int index = socket->index();
    const GPointer value = output_values[index];
    const ValueOrFieldCPPType *cpp_type = dynamic_cast<const ValueOrFieldCPPType *>(value.type());
    BLI_assert(cpp_type != nullptr);
    const GField field = cpp_type->as_field(value.get());

    const bNodeSocket *interface_socket = (const bNodeSocket *)BLI_findlink(
        &nmd.node_group->outputs, socket->index());
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
                                                     GEO_COMPONENT_TYPE_INSTANCES}) {
    if (!geometry.has(component_type)) {
      continue;
    }
    const GeometryComponent &component = *geometry.get_component_for_read(component_type);
    for (const auto item : outputs_by_domain.items()) {
      const eAttrDomain domain = item.key;
      const Span<OutputAttributeInfo> outputs_info = item.value;
      if (!component.attribute_domain_supported(domain)) {
        continue;
      }
      const int domain_num = component.attribute_domain_num(domain);
      blender::bke::GeometryComponentFieldContext field_context{component, domain};
      blender::fn::FieldEvaluator field_evaluator{field_context, domain_num};
      for (const OutputAttributeInfo &output_info : outputs_info) {
        const CPPType &type = output_info.field.cpp_type();
        OutputAttributeToStore store{
            component_type,
            domain,
            output_info.name,
            GMutableSpan{type, MEM_malloc_arrayN(domain_num, type.size(), __func__), domain_num}};
        field_evaluator.add_with_destination(output_info.field, store.data);
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
    const eCustomDataType data_type = blender::bke::cpp_type_to_custom_data_type(
        store.data.type());
    const std::optional<AttributeMetaData> meta_data = component.attribute_get_meta_data(
        store.name);

    /* Attempt to remove the attribute if it already exists but the domain and type don't match.
     * Removing the attribute won't succeed if it is built in and non-removable. */
    if (meta_data.has_value() &&
        (meta_data->domain != store.domain || meta_data->data_type != data_type)) {
      component.attribute_try_delete(store.name);
    }

    /* Try to create the attribute reusing the stored buffer. This will only succeed if the
     * attribute didn't exist before, or if it existed but was removed above. */
    if (component.attribute_try_create(
            store.name,
            store.domain,
            blender::bke::cpp_type_to_custom_data_type(store.data.type()),
            AttributeInitMove(store.data.data()))) {
      continue;
    }

    OutputAttribute attribute = component.attribute_try_get_for_output_only(
        store.name, store.domain, data_type);
    if (attribute) {
      attribute.varray().set_all(store.data.data());
      attribute.save();
    }

    /* We were unable to reuse the data, so it must be destructed and freed. */
    store.data.type().destruct_n(store.data.data(), store.data.size());
    MEM_freeN(store.data.data());
  }
}

static void store_output_attributes(GeometrySet &geometry,
                                    const NodesModifierData &nmd,
                                    const NodeRef &output_node,
                                    Span<GMutablePointer> output_values)
{
  /* All new attribute values have to be computed before the geometry is actually changed. This is
   * necessary because some fields might depend on attributes that are overwritten. */
  MultiValueMap<eAttrDomain, OutputAttributeInfo> outputs_by_domain =
      find_output_attributes_to_store(nmd, output_node, output_values);
  Vector<OutputAttributeToStore> attributes_to_store = compute_attributes_to_store(
      geometry, outputs_by_domain);
  store_computed_output_attributes(geometry, attributes_to_store);
}

/**
 * Evaluate a node group to compute the output geometry.
 */
static GeometrySet compute_geometry(const DerivedNodeTree &tree,
                                    Span<const NodeRef *> group_input_nodes,
                                    const NodeRef &output_node,
                                    GeometrySet input_geometry_set,
                                    NodesModifierData *nmd,
                                    const ModifierEvalContext *ctx)
{
  blender::ResourceScope scope;
  blender::LinearAllocator<> &allocator = scope.linear_allocator();
  blender::nodes::NodeMultiFunctions mf_by_node{tree};

  Map<DOutputSocket, GMutablePointer> group_inputs;

  const DTreeContext *root_context = &tree.root_context();
  for (const NodeRef *group_input_node : group_input_nodes) {
    Span<const OutputSocketRef *> group_input_sockets = group_input_node->outputs().drop_back(1);
    if (group_input_sockets.is_empty()) {
      continue;
    }

    Span<const OutputSocketRef *> remaining_input_sockets = group_input_sockets;

    /* If the group expects a geometry as first input, use the geometry that has been passed to
     * modifier. */
    const OutputSocketRef *first_input_socket = group_input_sockets[0];
    if (first_input_socket->bsocket()->type == SOCK_GEOMETRY) {
      GeometrySet *geometry_set_in =
          allocator.construct<GeometrySet>(input_geometry_set).release();
      group_inputs.add_new({root_context, first_input_socket}, geometry_set_in);
      remaining_input_sockets = remaining_input_sockets.drop_front(1);
    }

    /* Initialize remaining group inputs. */
    for (const OutputSocketRef *socket : remaining_input_sockets) {
      const CPPType &cpp_type = *socket->typeinfo()->geometry_nodes_cpp_type;
      void *value_in = allocator.allocate(cpp_type.size(), cpp_type.alignment());
      initialize_group_input(*nmd, *socket, value_in);
      group_inputs.add_new({root_context, socket}, {cpp_type, value_in});
    }
  }

  Vector<DInputSocket> group_outputs;
  for (const InputSocketRef *socket_ref : output_node.inputs().drop_back(1)) {
    group_outputs.append({root_context, socket_ref});
  }

  std::optional<geo_log::GeoLogger> geo_logger;

  blender::modifiers::geometry_nodes::GeometryNodesEvaluationParams eval_params;

  if (logging_enabled(ctx)) {
    Set<DSocket> preview_sockets;
    find_sockets_to_preview(nmd, ctx, tree, preview_sockets);
    eval_params.force_compute_sockets.extend(preview_sockets.begin(), preview_sockets.end());
    geo_logger.emplace(std::move(preview_sockets));

    geo_logger->log_input_geometry(input_geometry_set);
  }

  /* Don't keep a reference to the input geometry components to avoid copies during evaluation. */
  input_geometry_set.clear();

  eval_params.input_values = group_inputs;
  eval_params.output_sockets = group_outputs;
  eval_params.mf_by_node = &mf_by_node;
  eval_params.modifier_ = nmd;
  eval_params.depsgraph = ctx->depsgraph;
  eval_params.self_object = ctx->object;
  eval_params.geo_logger = geo_logger.has_value() ? &*geo_logger : nullptr;
  blender::modifiers::geometry_nodes::evaluate_geometry_nodes(eval_params);

  GeometrySet output_geometry_set = std::move(*eval_params.r_output_values[0].get<GeometrySet>());

  if (geo_logger.has_value()) {
    geo_logger->log_output_geometry(output_geometry_set);
    NodesModifierData *nmd_orig = (NodesModifierData *)BKE_modifier_get_original(ctx->object,
                                                                                 &nmd->modifier);
    clear_runtime_data(nmd_orig);
    nmd_orig->runtime_eval_log = new geo_log::ModifierLog(*geo_logger);
  }

  store_output_attributes(output_geometry_set, *nmd, output_node, eval_params.r_output_values);

  for (GMutablePointer value : eval_params.r_output_values) {
    value.destruct();
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
  else if (geometry_socket_count > 1) {
    BKE_modifier_set_error(ob, md, "Node group can only have one geometry input");
  }
}

static void modifyGeometry(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           GeometrySet &geometry_set)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group == nullptr) {
    return;
  }

  check_property_socket_sync(ctx->object, md);

  NodeTreeRefMap tree_refs;
  DerivedNodeTree tree{*nmd->node_group, tree_refs};

  if (tree.has_link_cycles()) {
    BKE_modifier_set_error(ctx->object, md, "Node group has cycles");
    geometry_set.clear();
    return;
  }

  const NodeTreeRef &root_tree_ref = tree.root_context().tree();
  Span<const NodeRef *> input_nodes = root_tree_ref.nodes_by_type("NodeGroupInput");
  Span<const NodeRef *> output_nodes = root_tree_ref.nodes_by_type("NodeGroupOutput");
  if (output_nodes.size() != 1) {
    BKE_modifier_set_error(ctx->object, md, "Node group must have a single output node");
    geometry_set.clear();
    return;
  }

  const NodeRef &output_node = *output_nodes[0];
  Span<const InputSocketRef *> group_outputs = output_node.inputs().drop_back(1);
  if (group_outputs.is_empty()) {
    BKE_modifier_set_error(ctx->object, md, "Node group must have an output socket");
    geometry_set.clear();
    return;
  }

  const InputSocketRef *first_output_socket = group_outputs[0];
  if (first_output_socket->idname() != "NodeSocketGeometry") {
    BKE_modifier_set_error(ctx->object, md, "Node group's first output must be a geometry");
    geometry_set.clear();
    return;
  }

  bool use_orig_index_verts = false;
  bool use_orig_index_edges = false;
  bool use_orig_index_polys = false;
  if (geometry_set.has_mesh()) {
    const Mesh &mesh = *geometry_set.get_mesh_for_read();
    use_orig_index_verts = CustomData_has_layer(&mesh.vdata, CD_ORIGINDEX);
    use_orig_index_edges = CustomData_has_layer(&mesh.edata, CD_ORIGINDEX);
    use_orig_index_polys = CustomData_has_layer(&mesh.pdata, CD_ORIGINDEX);
  }

  geometry_set = compute_geometry(
      tree, input_nodes, output_node, std::move(geometry_set), nmd, ctx);

  if (geometry_set.has_mesh()) {
    /* Add #CD_ORIGINDEX layers if they don't exist already. This is required because the
     * #eModifierTypeFlag_SupportsMapping flag is set. If the layers did not exist before, it is
     * assumed that the output mesh does not have a mapping to the original mesh. */
    Mesh &mesh = *geometry_set.get_mesh_for_write();
    if (use_orig_index_verts) {
      CustomData_add_layer(&mesh.vdata, CD_ORIGINDEX, CD_DEFAULT, nullptr, mesh.totvert);
    }
    if (use_orig_index_edges) {
      CustomData_add_layer(&mesh.edata, CD_ORIGINDEX, CD_DEFAULT, nullptr, mesh.totedge);
    }
    if (use_orig_index_polys) {
      CustomData_add_layer(&mesh.pdata, CD_ORIGINDEX, CD_DEFAULT, nullptr, mesh.totpoly);
    }
  }
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  GeometrySet geometry_set = GeometrySet::create_with_mesh(mesh, GeometryOwnershipType::Editable);

  modifyGeometry(md, ctx, geometry_set);

  Mesh *new_mesh = geometry_set.get_component_for_write<MeshComponent>().release();
  if (new_mesh == nullptr) {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }
  return new_mesh;
}

static void modifyGeometrySet(ModifierData *md,
                              const ModifierEvalContext *ctx,
                              GeometrySet *geometry_set)
{
  const Scene *scene = DEG_get_input_scene(ctx->depsgraph);
  GeometryCache::Timestamp timestamp{scene->r.cfra};

  if (isIterativeModifier(md)) {
    BLI_assert(ctx->object->id.orig_id);
    Object *orig_ob = (Object *)ctx->object->id.orig_id;
    if (GeometryCache *cache = orig_ob->runtime.geometry_cache) {
      GeometrySet *cached_geometry_set = cache->get_before(timestamp);
      if (cached_geometry_set) {
        *geometry_set = *cached_geometry_set;
      }
    }
  }

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

static void attribute_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  AttributeSearchData &data = *static_cast<AttributeSearchData *>(arg);
  const NodesModifierData *nmd = get_modifier_data(*CTX_data_main(C), *CTX_wm_manager(C), data);
  if (nmd == nullptr) {
    return;
  }
  const geo_log::ModifierLog *modifier_log = static_cast<const geo_log::ModifierLog *>(
      nmd->runtime_eval_log);
  if (modifier_log == nullptr) {
    return;
  }
  const geo_log::GeometryValueLog *geometry_log = data.is_output ?
                                                      modifier_log->output_geometry_log() :
                                                      modifier_log->input_geometry_log();
  if (geometry_log == nullptr) {
    return;
  }

  Span<GeometryAttributeInfo> infos = geometry_log->attributes();

  /* The shared attribute search code expects a span of pointers, so convert to that. */
  Array<const GeometryAttributeInfo *> info_ptrs(infos.size());
  for (const int i : infos.index_range()) {
    info_ptrs[i] = &infos[i];
  }
  blender::ui::attribute_search_add_items(
      str, data.is_output, info_ptrs.as_span(), items, is_first);
}

static void attribute_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  if (item_v == nullptr) {
    return;
  }
  AttributeSearchData &data = *static_cast<AttributeSearchData *>(data_v);
  const GeometryAttributeInfo &item = *static_cast<const GeometryAttributeInfo *>(item_v);
  const NodesModifierData *nmd = get_modifier_data(*CTX_data_main(C), *CTX_wm_manager(C), data);
  if (nmd == nullptr) {
    return;
  }

  const std::string attribute_prop_name = data.socket_identifier + attribute_name_suffix;
  IDProperty &name_property = *IDP_GetPropertyFromGroup(nmd->settings.properties,
                                                        attribute_prop_name.c_str());
  IDP_AssignString(&name_property, item.name.c_str(), 0);

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
  const geo_log::ModifierLog *log = static_cast<geo_log::ModifierLog *>(nmd.runtime_eval_log);
  if (log == nullptr) {
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
  const bool access_allowed = blender::bke::allow_procedural_attribute_access(attribute_name);
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

  uiLayout *split = uiLayoutSplit(layout, 0.4f, false);
  uiLayout *name_row = uiLayoutRow(split, false);
  uiLayoutSetAlignment(name_row, UI_LAYOUT_ALIGN_RIGHT);
  uiItemL(name_row, socket.name, ICON_NONE);

  uiLayout *row = uiLayoutRow(split, true);

  PointerRNA props;
  uiItemFullO(row,
              "object.geometry_nodes_input_attribute_toggle",
              "",
              ICON_SPREADSHEET,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              0,
              &props);
  RNA_string_set(&props, "modifier_name", nmd.modifier.name);
  RNA_string_set(&props, "prop_path", rna_path_use_attribute.c_str());

  const int use_attribute = RNA_int_get(md_ptr, rna_path_use_attribute.c_str()) != 0;
  if (use_attribute) {
    add_attribute_search_button(C, row, nmd, md_ptr, rna_path_attribute_name, socket, false);
    uiItemL(row, "", ICON_BLANK1);
  }
  else {
    uiItemR(row, md_ptr, rna_path.c_str(), 0, "", ICON_NONE);
    uiItemDecoratorR(row, md_ptr, rna_path.c_str(), -1);
  }
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
  BLI_snprintf(rna_path, ARRAY_SIZE(rna_path), "[\"%s\"]", socket_id_esc);

  /* Use #uiItemPointerR to draw pointer properties because #uiItemR would not have enough
   * information about what type of ID to select for editing the values. This is because
   * pointer IDProperties contain no information about their type. */
  switch (socket.type) {
    case SOCK_OBJECT: {
      uiItemPointerR(
          layout, md_ptr, rna_path, bmain_ptr, "objects", socket.name, ICON_OBJECT_DATA);
      break;
    }
    case SOCK_COLLECTION: {
      uiItemPointerR(layout,
                     md_ptr,
                     rna_path,
                     bmain_ptr,
                     "collections",
                     socket.name,
                     ICON_OUTLINER_COLLECTION);
      break;
    }
    case SOCK_MATERIAL: {
      uiItemPointerR(layout, md_ptr, rna_path, bmain_ptr, "materials", socket.name, ICON_MATERIAL);
      break;
    }
    case SOCK_TEXTURE: {
      uiItemPointerR(layout, md_ptr, rna_path, bmain_ptr, "textures", socket.name, ICON_TEXTURE);
      break;
    }
    case SOCK_IMAGE: {
      uiItemPointerR(layout, md_ptr, rna_path, bmain_ptr, "images", socket.name, ICON_IMAGE);
      break;
    }
    default: {
      if (input_has_attribute_toggle(*nmd->node_group, socket_index)) {
        add_attribute_search_or_value_buttons(C, layout, *nmd, md_ptr, socket);
      }
      else {
        uiLayout *row = uiLayoutRow(layout, false);
        uiLayoutSetPropDecorate(row, true);
        uiItemR(row, md_ptr, rna_path, 0, socket.name, ICON_NONE);
      }
    }
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
      draw_property_for_socket(*C, layout, nmd, &bmain_ptr, ptr, *socket, socket_index);
    }
  }

  /* Draw node warnings. */
  if (nmd->runtime_eval_log != nullptr) {
    const geo_log::ModifierLog &log = *static_cast<geo_log::ModifierLog *>(nmd->runtime_eval_log);
    log.foreach_node_log([&](const geo_log::NodeLog &node_log) {
      for (const geo_log::NodeWarning &warning : node_log.warnings()) {
        if (warning.type != geo_log::NodeWarningType::Info) {
          uiItemL(layout, warning.message.c_str(), ICON_ERROR);
        }
      }
    });
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

static void internal_dependencies_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  if (nmd->runtime_eval_log == nullptr) {
    return;
  }
  const geo_log::ModifierLog &log = *static_cast<geo_log::ModifierLog *>(nmd->runtime_eval_log);
  Map<std::string, eNamedAttrUsage> usage_by_attribute;
  log.foreach_node_log([&](const geo_log::NodeLog &node_log) {
    for (const geo_log::UsedNamedAttribute &used_attribute : node_log.used_named_attributes()) {
      usage_by_attribute.lookup_or_add_as(used_attribute.name,
                                          used_attribute.usage) |= used_attribute.usage;
    }
  });

  if (usage_by_attribute.is_empty()) {
    uiItemL(layout, IFACE_("No named attributes used"), ICON_INFO);
    return;
  }

  struct NameWithUsage {
    StringRefNull name;
    eNamedAttrUsage usage;
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
    const eNamedAttrUsage usage = attribute.usage;

    /* #uiLayoutRowWithHeading doesn't seem to work in this case. */
    uiLayout *split = uiLayoutSplit(layout, 0.4f, false);

    std::stringstream ss;
    Vector<std::string> usages;
    if ((usage & eNamedAttrUsage::Read) != eNamedAttrUsage::None) {
      usages.append(TIP_("Read"));
    }
    if ((usage & eNamedAttrUsage::Write) != eNamedAttrUsage::None) {
      usages.append(TIP_("Write"));
    }
    if ((usage & eNamedAttrUsage::Remove) != eNamedAttrUsage::None) {
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

static void blendWrite(BlendWriter *writer, const ID *UNUSED(id_owner), const ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);

  BLO_write_struct(writer, NodesModifierData, nmd);

  if (nmd->settings.properties != nullptr) {
    /* Note that the property settings are based on the socket type info
     * and don't necessarily need to be written, but we can't just free them. */
    IDP_BlendWrite(writer, nmd->settings.properties);
  }
}

static void blendRead(BlendDataReader *reader, ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group == nullptr) {
    nmd->settings.properties = nullptr;
  }
  else {
    BLO_read_data_address(reader, &nmd->settings.properties);
    IDP_BlendDataRead(reader, &nmd->settings.properties);
  }
  nmd->runtime_eval_log = nullptr;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
  NodesModifierData *tnmd = reinterpret_cast<NodesModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);

  tnmd->runtime_eval_log = nullptr;

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

  clear_runtime_data(nmd);
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
{
  /* We don't know what the node tree will need. If there are vertex groups, it is likely that the
   * node tree wants to access them. */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  r_cddata_masks->vmask |= CD_MASK_PROP_ALL;
}

ModifierTypeInfo modifierType_Nodes = {
    /* name */ "GeometryNodes",
    /* structName */ "NodesModifierData",
    /* structSize */ sizeof(NodesModifierData),
    /* srna */ &RNA_NodesModifier,
    /* type */ eModifierTypeType_Constructive,
    /* flags */
    static_cast<ModifierTypeFlag>(
        eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_NeedCaching),
    /* icon */ ICON_GEOMETRY_NODES,

    /* copyData */ copyData,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ modifyMesh,
    /* modifyGeometrySet */ modifyGeometrySet,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ nullptr,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ foreachTexLink,
    /* freeRuntimeData */ nullptr,
    /* panelRegister */ panelRegister,
    /* blendWrite */ blendWrite,
    /* blendRead */ blendRead,
};
