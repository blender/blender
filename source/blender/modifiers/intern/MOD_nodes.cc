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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include <cstring>
#include <iostream>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
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

#include "BKE_customdata.h"
#include "BKE_geometry_set_instances.hh"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_node_ui_storage.hh"
#include "BKE_object.h"
#include "BKE_pointcloud.h"
#include "BKE_screen.h"
#include "BKE_simulation.h"
#include "BKE_workspace.h"

#include "BLO_read_write.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_nodes.h"
#include "MOD_nodes_evaluator.hh"
#include "MOD_ui_common.h"

#include "ED_spreadsheet.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry.h"
#include "NOD_node_tree_multi_function.hh"

using blender::float3;
using blender::FunctionRef;
using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::Span;
using blender::StringRef;
using blender::StringRefNull;
using blender::Vector;
using blender::bke::PersistentCollectionHandle;
using blender::bke::PersistentDataHandleMap;
using blender::bke::PersistentObjectHandle;
using blender::fn::GMutablePointer;
using blender::fn::GPointer;
using blender::nodes::GeoNodeExecParams;
using namespace blender::fn::multi_function_types;
using namespace blender::nodes::derived_node_tree_types;

static void initData(ModifierData *md)
{
  NodesModifierData *nmd = (NodesModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(nmd, modifier));

  MEMCPY_STRUCT_AFTER(nmd, DNA_struct_default_get(NodesModifierData), modifier);
}

static void addIdsUsedBySocket(const ListBase *sockets, Set<ID *> &ids)
{
  LISTBASE_FOREACH (const bNodeSocket *, socket, sockets) {
    if (socket->type == SOCK_OBJECT) {
      Object *object = ((bNodeSocketValueObject *)socket->default_value)->value;
      if (object != nullptr) {
        ids.add(&object->id);
      }
    }
    else if (socket->type == SOCK_COLLECTION) {
      Collection *collection = ((bNodeSocketValueCollection *)socket->default_value)->value;
      if (collection != nullptr) {
        ids.add(&collection->id);
      }
    }
  }
}

static void find_used_ids_from_nodes(const bNodeTree &tree, Set<ID *> &ids)
{
  Set<const bNodeTree *> handled_groups;

  LISTBASE_FOREACH (const bNode *, node, &tree.nodes) {
    addIdsUsedBySocket(&node->inputs, ids);
    addIdsUsedBySocket(&node->outputs, ids);

    if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
      const bNodeTree *group = (bNodeTree *)node->id;
      if (group != nullptr && handled_groups.add(group)) {
        find_used_ids_from_nodes(*group, ids);
      }
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
    else if (ELEM(object.type, OB_MESH, OB_POINTCLOUD, OB_VOLUME)) {
      DEG_add_object_relation(ctx->node, &object, DEG_OB_COMP_GEOMETRY, "Nodes Modifier");
      DEG_add_customdata_mask(ctx->node, &object, &dependency_data_mask);
    }
  }
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  DEG_add_modifier_to_transform_relation(ctx->node, "Nodes Modifier");
  if (nmd->node_group != nullptr) {
    DEG_add_node_tree_relation(ctx->node, nmd->node_group, "Nodes Modifier");

    Set<ID *> used_ids;
    find_used_ids_from_settings(nmd->settings, used_ids);
    find_used_ids_from_nodes(*nmd->node_group, used_ids);
    for (ID *id : used_ids) {
      if (GS(id->name) == ID_OB) {
        Object *object = reinterpret_cast<Object *>(id);
        add_object_relation(ctx, *object);
      }
      if (GS(id->name) == ID_GR) {
        Collection *collection = reinterpret_cast<Collection *>(id);
        add_collection_relation(ctx, *collection);
      }
    }
  }

  /* TODO: Add dependency for adding and removing objects in collections. */
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

/**
 * This code is responsible for creating the new property and also creating the group of
 * properties in the prop_ui_container group for the UI info, the mapping for which is
 * scattered about in RNA_access.c.
 *
 * TODO(Hans): Codify this with some sort of table or refactor IDProperty use in RNA_access.c.
 */
struct SocketPropertyType {
  /* Create the actual property used to store the data for the modifier. */
  IDProperty *(*create_prop)(const bNodeSocket &socket, const char *name);
  /* Reused to build the "soft_min" property too. */
  IDProperty *(*create_min_ui_prop)(const bNodeSocket &socket, const char *name);
  /* Reused to build the "soft_max" property too. */
  IDProperty *(*create_max_ui_prop)(const bNodeSocket &socket, const char *name);
  /* This uses the same values as #create_prop, but sometimes the type is different, so it can't
   * be the same function. */
  IDProperty *(*create_default_ui_prop)(const bNodeSocket &socket, const char *name);
  PropertyType (*rna_subtype_get)(const bNodeSocket &socket);
  bool (*is_correct_type)(const IDProperty &property);
  void (*init_cpp_value)(const IDProperty &property,
                         const PersistentDataHandleMap &handles,
                         void *r_value);
};

static IDProperty *socket_add_property(IDProperty *settings_prop_group,
                                       IDProperty *ui_container,
                                       const SocketPropertyType &property_type,
                                       const bNodeSocket &socket)
{
  const char *new_prop_name = socket.identifier;
  /* Add the property actually storing the data to the modifier's group. */
  IDProperty *prop = property_type.create_prop(socket, new_prop_name);
  IDP_AddToGroup(settings_prop_group, prop);

  prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY;

  /* Make the group in the UI container group to hold the property's UI settings. */
  IDProperty *prop_ui_group;
  {
    IDPropertyTemplate idprop = {0};
    prop_ui_group = IDP_New(IDP_GROUP, &idprop, new_prop_name);
    IDP_AddToGroup(ui_container, prop_ui_group);
  }

  /* Set property description (tooltip). */
  IDPropertyTemplate property_description_template;
  property_description_template.string.str = socket.description;
  property_description_template.string.len = BLI_strnlen(socket.description, MAX_NAME) + 1;
  property_description_template.string.subtype = IDP_STRING_SUB_UTF8;
  IDProperty *description = IDP_New(IDP_STRING, &property_description_template, "description");
  IDP_AddToGroup(prop_ui_group, description);

  /* Create the properties for the socket's UI settings. */
  if (property_type.create_min_ui_prop != nullptr) {
    IDP_AddToGroup(prop_ui_group, property_type.create_min_ui_prop(socket, "min"));
    IDP_AddToGroup(prop_ui_group, property_type.create_min_ui_prop(socket, "soft_min"));
  }
  if (property_type.create_max_ui_prop != nullptr) {
    IDP_AddToGroup(prop_ui_group, property_type.create_max_ui_prop(socket, "max"));
    IDP_AddToGroup(prop_ui_group, property_type.create_max_ui_prop(socket, "soft_max"));
  }
  if (property_type.create_default_ui_prop != nullptr) {
    IDP_AddToGroup(prop_ui_group, property_type.create_default_ui_prop(socket, "default"));
  }
  if (property_type.rna_subtype_get != nullptr) {
    const char *subtype_identifier = nullptr;
    RNA_enum_identifier(rna_enum_property_subtype_items,
                        property_type.rna_subtype_get(socket),
                        &subtype_identifier);

    if (subtype_identifier != nullptr) {
      IDPropertyTemplate idprop = {0};
      idprop.string.str = subtype_identifier;
      idprop.string.len = BLI_strnlen(subtype_identifier, MAX_NAME) + 1;
      IDP_AddToGroup(prop_ui_group, IDP_New(IDP_STRING, &idprop, "subtype"));
    }
  }

  return prop;
}

static const SocketPropertyType *get_socket_property_type(const bNodeSocket &bsocket)
{
  switch (bsocket.type) {
    case SOCK_FLOAT: {
      static const SocketPropertyType float_type = {
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueFloat *value = (bNodeSocketValueFloat *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.f = value->value;
            return IDP_New(IDP_FLOAT, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueFloat *value = (bNodeSocketValueFloat *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.d = value->min;
            return IDP_New(IDP_DOUBLE, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueFloat *value = (bNodeSocketValueFloat *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.d = value->max;
            return IDP_New(IDP_DOUBLE, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueFloat *value = (bNodeSocketValueFloat *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.d = value->value;
            return IDP_New(IDP_DOUBLE, &idprop, name);
          },
          [](const bNodeSocket &socket) {
            return (PropertyType)((bNodeSocketValueFloat *)socket.default_value)->subtype;
          },
          [](const IDProperty &property) { return ELEM(property.type, IDP_FLOAT, IDP_DOUBLE); },
          [](const IDProperty &property,
             const PersistentDataHandleMap &UNUSED(handles),
             void *r_value) {
            if (property.type == IDP_FLOAT) {
              *(float *)r_value = IDP_Float(&property);
            }
            else if (property.type == IDP_DOUBLE) {
              *(float *)r_value = (float)IDP_Double(&property);
            }
          },
      };
      return &float_type;
    }
    case SOCK_INT: {
      static const SocketPropertyType int_type = {
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueInt *value = (bNodeSocketValueInt *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.i = value->value;
            return IDP_New(IDP_INT, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueInt *value = (bNodeSocketValueInt *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.i = value->min;
            return IDP_New(IDP_INT, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueInt *value = (bNodeSocketValueInt *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.i = value->max;
            return IDP_New(IDP_INT, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueInt *value = (bNodeSocketValueInt *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.i = value->value;
            return IDP_New(IDP_INT, &idprop, name);
          },
          [](const bNodeSocket &socket) {
            return (PropertyType)((bNodeSocketValueInt *)socket.default_value)->subtype;
          },
          [](const IDProperty &property) { return property.type == IDP_INT; },
          [](const IDProperty &property,
             const PersistentDataHandleMap &UNUSED(handles),
             void *r_value) { *(int *)r_value = IDP_Int(&property); },
      };
      return &int_type;
    }
    case SOCK_VECTOR: {
      static const SocketPropertyType vector_type = {
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueVector *value = (bNodeSocketValueVector *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.array.len = 3;
            idprop.array.type = IDP_FLOAT;
            IDProperty *property = IDP_New(IDP_ARRAY, &idprop, name);
            copy_v3_v3((float *)IDP_Array(property), value->value);
            return property;
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueVector *value = (bNodeSocketValueVector *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.d = value->min;
            return IDP_New(IDP_DOUBLE, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueVector *value = (bNodeSocketValueVector *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.d = value->max;
            return IDP_New(IDP_DOUBLE, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueVector *value = (bNodeSocketValueVector *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.array.len = 3;
            idprop.array.type = IDP_FLOAT;
            IDProperty *property = IDP_New(IDP_ARRAY, &idprop, name);
            copy_v3_v3((float *)IDP_Array(property), value->value);
            return property;
          },
          [](const bNodeSocket &socket) {
            return (PropertyType)((bNodeSocketValueVector *)socket.default_value)->subtype;
          },
          [](const IDProperty &property) {
            return property.type == IDP_ARRAY && property.subtype == IDP_FLOAT &&
                   property.len == 3;
          },
          [](const IDProperty &property,
             const PersistentDataHandleMap &UNUSED(handles),
             void *r_value) { copy_v3_v3((float *)r_value, (const float *)IDP_Array(&property)); },
      };
      return &vector_type;
    }
    case SOCK_BOOLEAN: {
      static const SocketPropertyType boolean_type = {
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueBoolean *value = (bNodeSocketValueBoolean *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.i = value->value != 0;
            return IDP_New(IDP_INT, &idprop, name);
          },
          [](const bNodeSocket &UNUSED(socket), const char *name) {
            IDPropertyTemplate idprop = {0};
            idprop.i = 0;
            return IDP_New(IDP_INT, &idprop, name);
          },
          [](const bNodeSocket &UNUSED(socket), const char *name) {
            IDPropertyTemplate idprop = {0};
            idprop.i = 1;
            return IDP_New(IDP_INT, &idprop, name);
          },
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueBoolean *value = (bNodeSocketValueBoolean *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.i = value->value != 0;
            return IDP_New(IDP_INT, &idprop, name);
          },
          nullptr,
          [](const IDProperty &property) { return property.type == IDP_INT; },
          [](const IDProperty &property,
             const PersistentDataHandleMap &UNUSED(handles),
             void *r_value) { *(bool *)r_value = IDP_Int(&property) != 0; },
      };
      return &boolean_type;
    }
    case SOCK_STRING: {
      static const SocketPropertyType string_type = {
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueString *value = (bNodeSocketValueString *)socket.default_value;
            return IDP_NewString(
                value->value, name, BLI_strnlen(value->value, sizeof(value->value)) + 1);
          },
          nullptr,
          nullptr,
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueString *value = (bNodeSocketValueString *)socket.default_value;
            return IDP_NewString(
                value->value, name, BLI_strnlen(value->value, sizeof(value->value)) + 1);
          },
          nullptr,
          [](const IDProperty &property) { return property.type == IDP_STRING; },
          [](const IDProperty &property,
             const PersistentDataHandleMap &UNUSED(handles),
             void *r_value) { new (r_value) std::string(IDP_String(&property)); },
      };
      return &string_type;
    }
    case SOCK_OBJECT: {
      static const SocketPropertyType object_type = {
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueObject *value = (bNodeSocketValueObject *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.id = (ID *)value->value;
            return IDP_New(IDP_ID, &idprop, name);
          },
          nullptr,
          nullptr,
          nullptr,
          nullptr,
          [](const IDProperty &property) { return property.type == IDP_ID; },
          [](const IDProperty &property, const PersistentDataHandleMap &handles, void *r_value) {
            ID *id = IDP_Id(&property);
            Object *object = (id && GS(id->name) == ID_OB) ? (Object *)id : nullptr;
            new (r_value) PersistentObjectHandle(handles.lookup(object));
          },
      };
      return &object_type;
    }
    case SOCK_COLLECTION: {
      static const SocketPropertyType collection_type = {
          [](const bNodeSocket &socket, const char *name) {
            bNodeSocketValueCollection *value = (bNodeSocketValueCollection *)socket.default_value;
            IDPropertyTemplate idprop = {0};
            idprop.id = (ID *)value->value;
            return IDP_New(IDP_ID, &idprop, name);
          },
          nullptr,
          nullptr,
          nullptr,
          nullptr,
          [](const IDProperty &property) { return property.type == IDP_ID; },
          [](const IDProperty &property, const PersistentDataHandleMap &handles, void *r_value) {
            ID *id = IDP_Id(&property);
            Collection *collection = (id && GS(id->name) == ID_GR) ? (Collection *)id : nullptr;
            new (r_value) PersistentCollectionHandle(handles.lookup(collection));
          },
      };
      return &collection_type;
    }
    default: {
      return nullptr;
    }
  }
}

/**
 * Rebuild the list of properties based on the sockets exposed as the modifier's node group
 * inputs. If any properties correspond to the old properties by name and type, carry over
 * the values.
 */
void MOD_nodes_update_interface(Object *object, NodesModifierData *nmd)
{
  if (nmd->node_group == nullptr) {
    return;
  }

  IDProperty *old_properties = nmd->settings.properties;

  {
    IDPropertyTemplate idprop = {0};
    nmd->settings.properties = IDP_New(IDP_GROUP, &idprop, "Nodes Modifier Settings");
  }

  IDProperty *ui_container_group;
  {
    IDPropertyTemplate idprop = {0};
    ui_container_group = IDP_New(IDP_GROUP, &idprop, "_RNA_UI");
    IDP_AddToGroup(nmd->settings.properties, ui_container_group);
  }

  LISTBASE_FOREACH (bNodeSocket *, socket, &nmd->node_group->inputs) {
    const SocketPropertyType *property_type = get_socket_property_type(*socket);
    if (property_type == nullptr) {
      continue;
    }

    IDProperty *new_prop = socket_add_property(
        nmd->settings.properties, ui_container_group, *property_type, *socket);

    if (old_properties != nullptr) {
      IDProperty *old_prop = IDP_GetPropertyFromGroup(old_properties, socket->identifier);
      if (old_prop != nullptr && property_type->is_correct_type(*old_prop)) {
        IDP_CopyPropertyContent(new_prop, old_prop);
      }
    }
  }

  if (old_properties != nullptr) {
    IDP_FreeProperty(old_properties);
  }

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
}

void MOD_nodes_init(Main *bmain, NodesModifierData *nmd)
{
  bNodeTree *ntree = ntreeAddTree(bmain, "Geometry Nodes", ntreeType_Geometry->idname);
  nmd->node_group = ntree;

  ntreeAddSocketInterface(ntree, SOCK_IN, "NodeSocketGeometry", "Geometry");
  ntreeAddSocketInterface(ntree, SOCK_OUT, "NodeSocketGeometry", "Geometry");

  bNode *group_input_node = nodeAddStaticNode(nullptr, ntree, NODE_GROUP_INPUT);
  bNode *group_output_node = nodeAddStaticNode(nullptr, ntree, NODE_GROUP_OUTPUT);

  nodeSetSelected(group_input_node, false);
  nodeSetSelected(group_output_node, false);

  group_input_node->locx = -200 - group_input_node->width;
  group_output_node->locx = 200;
  group_output_node->flag |= NODE_DO_OUTPUT;

  nodeAddLink(ntree,
              group_output_node,
              (bNodeSocket *)group_output_node->inputs.first,
              group_input_node,
              (bNodeSocket *)group_input_node->outputs.first);

  ntreeUpdateTree(bmain, ntree);
}

static void initialize_group_input(NodesModifierData &nmd,
                                   const PersistentDataHandleMap &handle_map,
                                   const bNodeSocket &socket,
                                   const CPPType &cpp_type,
                                   void *r_value)
{
  const SocketPropertyType *property_type = get_socket_property_type(socket);
  if (property_type == nullptr) {
    cpp_type.copy_to_uninitialized(cpp_type.default_value(), r_value);
    return;
  }
  if (nmd.settings.properties == nullptr) {
    blender::nodes::socket_cpp_value_get(socket, r_value);
    return;
  }
  const IDProperty *property = IDP_GetPropertyFromGroup(nmd.settings.properties,
                                                        socket.identifier);
  if (property == nullptr) {
    blender::nodes::socket_cpp_value_get(socket, r_value);
    return;
  }
  if (!property_type->is_correct_type(*property)) {
    blender::nodes::socket_cpp_value_get(socket, r_value);
    return;
  }
  property_type->init_cpp_value(*property, handle_map, r_value);
}

static void fill_data_handle_map(const NodesModifierSettings &settings,
                                 const DerivedNodeTree &tree,
                                 PersistentDataHandleMap &handle_map)
{
  Set<ID *> used_ids;
  find_used_ids_from_settings(settings, used_ids);
  find_used_ids_from_nodes(*tree.root_context().tree().btree(), used_ids);

  int current_handle = 0;
  for (ID *id : used_ids) {
    handle_map.add(current_handle, *id);
    current_handle++;
  }
}

static void reset_tree_ui_storage(Span<const blender::nodes::NodeTreeRef *> trees,
                                  const Object &object,
                                  const ModifierData &modifier)
{
  const NodeTreeEvaluationContext context = {object, modifier};

  for (const blender::nodes::NodeTreeRef *tree : trees) {
    bNodeTree *btree_cow = tree->btree();
    bNodeTree *btree_original = (bNodeTree *)DEG_get_original_id((ID *)btree_cow);
    BKE_nodetree_ui_storage_free_for_context(*btree_original, context);
  }
}

static Vector<SpaceSpreadsheet *> find_spreadsheet_editors(Main *bmain)
{
  Vector<SpaceSpreadsheet *> spreadsheets;
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
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

using PreviewSocketMap = blender::MultiValueMap<DSocket, uint64_t>;

static DSocket try_find_preview_socket_in_node(const DNode node)
{
  for (const SocketRef *socket : node->outputs()) {
    if (socket->bsocket()->type == SOCK_GEOMETRY) {
      return {node.context(), socket};
    }
  }
  for (const SocketRef *socket : node->inputs()) {
    if (socket->bsocket()->type == SOCK_GEOMETRY &&
        (socket->bsocket()->flag & SOCK_MULTI_INPUT) == 0) {
      return {node.context(), socket};
    }
  }
  return {};
}

static DSocket try_get_socket_to_preview_for_spreadsheet(SpaceSpreadsheet *sspreadsheet,
                                                         NodesModifierData *nmd,
                                                         const ModifierEvalContext *ctx,
                                                         const DerivedNodeTree &tree)
{
  Vector<SpreadsheetContext *> context_path = sspreadsheet->context_path;
  if (context_path.size() < 3) {
    return {};
  }
  if (context_path[0]->type != SPREADSHEET_CONTEXT_OBJECT) {
    return {};
  }
  if (context_path[1]->type != SPREADSHEET_CONTEXT_MODIFIER) {
    return {};
  }
  SpreadsheetContextObject *object_context = (SpreadsheetContextObject *)context_path[0];
  if (object_context->object != DEG_get_original_object(ctx->object)) {
    return {};
  }
  SpreadsheetContextModifier *modifier_context = (SpreadsheetContextModifier *)context_path[1];
  if (StringRef(modifier_context->modifier_name) != nmd->modifier.name) {
    return {};
  }
  for (SpreadsheetContext *context : context_path.as_span().drop_front(2)) {
    if (context->type != SPREADSHEET_CONTEXT_NODE) {
      return {};
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
      return {};
    }
    context = context->child_context(*found_node);
    if (context == nullptr) {
      return {};
    }
  }

  const NodeTreeRef &tree_ref = context->tree();
  for (const NodeRef *node_ref : tree_ref.nodes()) {
    if (node_ref->name() == last_context->node_name) {
      return try_find_preview_socket_in_node({context, node_ref});
    }
  }
  return {};
}

static void find_sockets_to_preview(NodesModifierData *nmd,
                                    const ModifierEvalContext *ctx,
                                    const DerivedNodeTree &tree,
                                    PreviewSocketMap &r_sockets_to_preview)
{
  Main *bmain = DEG_get_bmain(ctx->depsgraph);

  /* Based on every visible spreadsheet context path, get a list of sockets that need to have their
   * intermediate geometries cached for display. */
  Vector<SpaceSpreadsheet *> spreadsheets = find_spreadsheet_editors(bmain);
  for (SpaceSpreadsheet *sspreadsheet : spreadsheets) {
    const DSocket socket = try_get_socket_to_preview_for_spreadsheet(sspreadsheet, nmd, ctx, tree);
    if (socket) {
      const uint64_t key = ED_spreadsheet_context_path_hash(sspreadsheet);
      r_sockets_to_preview.add_non_duplicates(socket, key);
    }
  }
}

static void log_preview_socket_value(const Span<GPointer> values,
                                     Object *object,
                                     Span<uint64_t> keys)
{
  GeometrySet geometry_set = *(const GeometrySet *)values[0].get();
  geometry_set.ensure_owns_direct_data();
  for (uint64_t key : keys) {
    BKE_object_preview_geometry_set_add(object, key, new GeometrySet(geometry_set));
  }
}

static void log_ui_hints(const DSocket socket,
                         const Span<GPointer> values,
                         Object *self_object,
                         NodesModifierData *nmd)
{
  const DNode node = socket.node();
  if (node->is_reroute_node() || socket->typeinfo()->type != SOCK_GEOMETRY) {
    return;
  }
  bNodeTree *btree_cow = node->btree();
  bNodeTree *btree_original = (bNodeTree *)DEG_get_original_id((ID *)btree_cow);
  const NodeTreeEvaluationContext context{*self_object, nmd->modifier};
  for (const GPointer &data : values) {
    if (data.type() == &CPPType::get<GeometrySet>()) {
      const GeometrySet &geometry_set = *(const GeometrySet *)data.get();
      blender::bke::geometry_set_instances_attribute_foreach(
          geometry_set,
          [&](StringRefNull attribute_name, const AttributeMetaData &meta_data) {
            BKE_nodetree_attribute_hint_add(*btree_original,
                                            context,
                                            *node->bnode(),
                                            attribute_name,
                                            meta_data.domain,
                                            meta_data.data_type);
            return true;
          },
          8);
    }
  }
}

/**
 * Evaluate a node group to compute the output geometry.
 * Currently, this uses a fairly basic and inefficient algorithm that might compute things more
 * often than necessary. It's going to be replaced soon.
 */
static GeometrySet compute_geometry(const DerivedNodeTree &tree,
                                    Span<const NodeRef *> group_input_nodes,
                                    const InputSocketRef &socket_to_compute,
                                    GeometrySet input_geometry_set,
                                    NodesModifierData *nmd,
                                    const ModifierEvalContext *ctx)
{
  blender::ResourceScope scope;
  blender::LinearAllocator<> &allocator = scope.linear_allocator();
  blender::nodes::MultiFunctionByNode mf_by_node = get_multi_function_per_node(tree, scope);

  PersistentDataHandleMap handle_map;
  fill_data_handle_map(nmd->settings, tree, handle_map);

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
      const CPPType &cpp_type = *blender::nodes::socket_cpp_type_get(*socket->typeinfo());
      void *value_in = allocator.allocate(cpp_type.size(), cpp_type.alignment());
      initialize_group_input(*nmd, handle_map, *socket->bsocket(), cpp_type, value_in);
      group_inputs.add_new({root_context, socket}, {cpp_type, value_in});
    }
  }

  /* Don't keep a reference to the input geometry components to avoid copies during evaluation. */
  input_geometry_set.clear();

  Vector<DInputSocket> group_outputs;
  group_outputs.append({root_context, &socket_to_compute});

  PreviewSocketMap preview_sockets;
  find_sockets_to_preview(nmd, ctx, tree, preview_sockets);

  auto log_socket_value = [&](const DSocket socket, const Span<GPointer> values) {
    if (!logging_enabled(ctx)) {
      return;
    }
    Span<uint64_t> keys = preview_sockets.lookup(socket);
    if (!keys.is_empty()) {
      log_preview_socket_value(values, ctx->object, keys);
    }
    log_ui_hints(socket, values, ctx->object, nmd);
  };

  blender::modifiers::geometry_nodes::GeometryNodesEvaluationParams eval_params;
  eval_params.input_values = group_inputs;
  eval_params.output_sockets = group_outputs;
  eval_params.mf_by_node = &mf_by_node;
  eval_params.handle_map = &handle_map;
  eval_params.modifier_ = nmd;
  eval_params.depsgraph = ctx->depsgraph;
  eval_params.self_object = ctx->object;
  eval_params.log_socket_value_fn = log_socket_value;
  blender::modifiers::geometry_nodes::evaluate_geometry_nodes(eval_params);

  BLI_assert(eval_params.r_output_values.size() == 1);
  GMutablePointer result = eval_params.r_output_values[0];
  return result.relocate_out<GeometrySet>();
}

/**
 * \note This could be done in #initialize_group_input, though that would require adding the
 * the object as a parameter, so it's likely better to this check as a separate step.
 */
static void check_property_socket_sync(const Object *ob, ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);

  int i = 0;
  LISTBASE_FOREACH_INDEX (const bNodeSocket *, socket, &nmd->node_group->inputs, i) {
    /* The first socket is the special geometry socket for the modifier object. */
    if (i == 0 && socket->type == SOCK_GEOMETRY) {
      continue;
    }

    IDProperty *property = IDP_GetPropertyFromGroup(nmd->settings.properties, socket->identifier);
    if (property == nullptr) {
      if (socket->type == SOCK_GEOMETRY) {
        BKE_modifier_set_error(ob, md, "Node group can only have one geometry input");
      }
      else {
        BKE_modifier_set_error(ob, md, "Missing property for input socket \"%s\"", socket->name);
      }
      continue;
    }

    const SocketPropertyType *property_type = get_socket_property_type(*socket);
    if (!property_type->is_correct_type(*property)) {
      BKE_modifier_set_error(
          ob, md, "Property type does not match input socket \"(%s)\"", socket->name);
      continue;
    }
  }

  bool has_geometry_output = false;
  LISTBASE_FOREACH (const bNodeSocket *, socket, &nmd->node_group->outputs) {
    if (socket->type == SOCK_GEOMETRY) {
      has_geometry_output = true;
    }
  }

  if (!has_geometry_output) {
    BKE_modifier_set_error(ob, md, "Node group must have a geometry output");
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
    return;
  }

  const NodeTreeRef &root_tree_ref = tree.root_context().tree();
  Span<const NodeRef *> input_nodes = root_tree_ref.nodes_by_type("NodeGroupInput");
  Span<const NodeRef *> output_nodes = root_tree_ref.nodes_by_type("NodeGroupOutput");

  if (output_nodes.size() != 1) {
    return;
  }

  Span<const InputSocketRef *> group_outputs = output_nodes[0]->inputs().drop_back(1);

  if (group_outputs.size() == 0) {
    return;
  }

  const InputSocketRef *group_output = group_outputs[0];
  if (group_output->idname() != "NodeSocketGeometry") {
    return;
  }

  if (logging_enabled(ctx)) {
    reset_tree_ui_storage(tree.used_node_tree_refs(), *ctx->object, *md);
  }

  geometry_set = compute_geometry(
      tree, input_nodes, *group_outputs[0], std::move(geometry_set), nmd, ctx);
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  GeometrySet geometry_set = GeometrySet::create_with_mesh(mesh, GeometryOwnershipType::Editable);
  geometry_set.get_component_for_write<MeshComponent>().copy_vertex_group_names_from_object(
      *ctx->object);
  modifyGeometry(md, ctx, geometry_set);

  /* This function is only called when applying modifiers. In this case it makes sense to realize
   * instances, otherwise in some cases there might be no results when applying the modifier. */
  geometry_set = blender::bke::geometry_set_realize_mesh_for_modifier(geometry_set);

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
  modifyGeometry(md, ctx, *geometry_set);
}

/* Drawing the properties manually with #uiItemR instead of #uiDefAutoButsRNA allows using
 * the node socket identifier for the property names, since they are unique, but also having
 * the correct label displayed in the UI.  */
static void draw_property_for_socket(uiLayout *layout,
                                     PointerRNA *bmain_ptr,
                                     PointerRNA *md_ptr,
                                     const IDProperty *modifier_props,
                                     const bNodeSocket &socket)
{
  const SocketPropertyType *property_type = get_socket_property_type(socket);
  if (property_type == nullptr) {
    return;
  }

  /* The property should be created in #MOD_nodes_update_interface with the correct type. */
  IDProperty *property = IDP_GetPropertyFromGroup(modifier_props, socket.identifier);

  /* IDProperties can be removed with python, so there could be a situation where
   * there isn't a property for a socket or it doesn't have the correct type. */
  if (property == nullptr || !property_type->is_correct_type(*property)) {
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
    default:
      uiItemR(layout, md_ptr, rna_path, 0, socket.name, ICON_NONE);
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Main *bmain = CTX_data_main(C);

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, true);

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

    LISTBASE_FOREACH (bNodeSocket *, socket, &nmd->node_group->inputs) {
      draw_property_for_socket(layout, &bmain_ptr, ptr, nmd->settings.properties, *socket);
    }
  }

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Nodes, panel_draw);
}

static void blendWrite(BlendWriter *writer, const ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
  if (nmd->settings.properties != nullptr) {
    /* Note that the property settings are based on the socket type info
     * and don't necessarily need to be written, but we can't just free them. */
    IDP_BlendWrite(writer, nmd->settings.properties);
  }
}

static void blendRead(BlendDataReader *reader, ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  BLO_read_data_address(reader, &nmd->settings.properties);
  IDP_BlendDataRead(reader, &nmd->settings.properties);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
  NodesModifierData *tnmd = reinterpret_cast<NodesModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);

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
        eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping),
    /* icon */ ICON_NODETREE,

    /* copyData */ copyData,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ nullptr,
    /* modifyGeometrySet */ modifyGeometrySet,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ nullptr,
    /* dependsOnNormals */ nullptr,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ foreachTexLink,
    /* freeRuntimeData */ nullptr,
    /* panelRegister */ panelRegister,
    /* blendWrite */ blendWrite,
    /* blendRead */ blendRead,
};
