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
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_customdata.h"
#include "BKE_idprop.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_pointcloud.h"
#include "BKE_screen.h"
#include "BKE_simulation.h"

#include "BLO_read_write.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_nodes.h"
#include "MOD_ui_common.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry.h"
#include "NOD_geometry_exec.hh"
#include "NOD_node_tree_multi_function.hh"
#include "NOD_type_callbacks.hh"

using blender::float3;
using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::Span;
using blender::StringRef;
using blender::Vector;
using blender::fn::GMutablePointer;
using blender::fn::GValueMap;
using blender::nodes::GeoNodeExecParams;
using namespace blender::nodes::derived_node_tree_types;
using namespace blender::fn::multi_function_types;

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

static void findUsedIds(const bNodeTree &tree, Set<ID *> &ids)
{
  Set<const bNodeTree *> handled_groups;

  LISTBASE_FOREACH (const bNode *, node, &tree.nodes) {
    addIdsUsedBySocket(&node->inputs, ids);
    addIdsUsedBySocket(&node->outputs, ids);

    if (node->type == NODE_GROUP) {
      const bNodeTree *group = (bNodeTree *)node->id;
      if (group != nullptr && handled_groups.add(group)) {
        findUsedIds(*group, ids);
      }
    }
  }
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group != nullptr) {
    DEG_add_node_tree_relation(ctx->node, nmd->node_group, "Nodes Modifier");

    Set<ID *> used_ids;
    findUsedIds(*nmd->node_group, used_ids);
    for (ID *id : used_ids) {
      if (GS(id->name) == ID_OB) {
        Object *object = reinterpret_cast<Object *>(id);
        DEG_add_object_relation(ctx->node, object, DEG_OB_COMP_TRANSFORM, "Nodes Modifier");
        if (id != &ctx->object->id) {
          if (object->type != OB_EMPTY) {
            DEG_add_object_relation(
                ctx->node, (Object *)id, DEG_OB_COMP_GEOMETRY, "Nodes Modifier");
          }
        }
      }
    }
  }

  /* TODO: Add relations for IDs in settings. */
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

class GeometryNodesEvaluator {
 private:
  blender::LinearAllocator<> allocator_;
  Map<const DInputSocket *, GMutablePointer> value_by_input_;
  Vector<const DInputSocket *> group_outputs_;
  blender::nodes::MultiFunctionByNode &mf_by_node_;
  const blender::nodes::DataTypeConversions &conversions_;
  const blender::bke::PersistentDataHandleMap &handle_map_;
  const Object *self_object_;

 public:
  GeometryNodesEvaluator(const Map<const DOutputSocket *, GMutablePointer> &group_input_data,
                         Vector<const DInputSocket *> group_outputs,
                         blender::nodes::MultiFunctionByNode &mf_by_node,
                         const blender::bke::PersistentDataHandleMap &handle_map,
                         const Object *self_object)
      : group_outputs_(std::move(group_outputs)),
        mf_by_node_(mf_by_node),
        conversions_(blender::nodes::get_implicit_type_conversions()),
        handle_map_(handle_map),
        self_object_(self_object)
  {
    for (auto item : group_input_data.items()) {
      this->forward_to_inputs(*item.key, item.value);
    }
  }

  Vector<GMutablePointer> execute()
  {
    Vector<GMutablePointer> results;
    for (const DInputSocket *group_output : group_outputs_) {
      GMutablePointer result = this->get_input_value(*group_output);
      results.append(result);
    }
    for (GMutablePointer value : value_by_input_.values()) {
      value.destruct();
    }
    return results;
  }

 private:
  GMutablePointer get_input_value(const DInputSocket &socket_to_compute)
  {
    std::optional<GMutablePointer> value = value_by_input_.pop_try(&socket_to_compute);
    if (value.has_value()) {
      /* This input has been computed before, return it directly. */
      return *value;
    }

    Span<const DOutputSocket *> from_sockets = socket_to_compute.linked_sockets();
    Span<const DGroupInput *> from_group_inputs = socket_to_compute.linked_group_inputs();
    const int total_inputs = from_sockets.size() + from_group_inputs.size();
    BLI_assert(total_inputs <= 1);

    if (total_inputs == 0) {
      /* The input is not connected, use the value from the socket itself. */
      return get_unlinked_input_value(socket_to_compute);
    }
    if (from_group_inputs.size() == 1) {
      /* The input gets its value from the input of a group that is not further connected. */
      return get_unlinked_input_value(socket_to_compute);
    }

    /* Compute the socket now. */
    const DOutputSocket &from_socket = *from_sockets[0];
    this->compute_output_and_forward(from_socket);
    return value_by_input_.pop(&socket_to_compute);
  }

  void compute_output_and_forward(const DOutputSocket &socket_to_compute)
  {
    const DNode &node = socket_to_compute.node();
    const bNode &bnode = *node.bnode();

    if (!socket_to_compute.is_available()) {
      /* If the output is not available, use a default value. */
      const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_to_compute.typeinfo());
      void *buffer = allocator_.allocate(type.size(), type.alignment());
      type.copy_to_uninitialized(type.default_value(), buffer);
      this->forward_to_inputs(socket_to_compute, {type, buffer});
      return;
    }

    /* Prepare inputs required to execute the node. */
    GValueMap<StringRef> node_inputs_map{allocator_};
    for (const DInputSocket *input_socket : node.inputs()) {
      if (input_socket->is_available()) {
        GMutablePointer value = this->get_input_value(*input_socket);
        node_inputs_map.add_new_direct(input_socket->identifier(), value);
      }
    }

    /* Execute the node. */
    GValueMap<StringRef> node_outputs_map{allocator_};
    GeoNodeExecParams params{bnode, node_inputs_map, node_outputs_map, handle_map_, self_object_};
    this->execute_node(node, params);

    /* Forward computed outputs to linked input sockets. */
    for (const DOutputSocket *output_socket : node.outputs()) {
      if (output_socket->is_available()) {
        GMutablePointer value = node_outputs_map.extract(output_socket->identifier());
        this->forward_to_inputs(*output_socket, value);
      }
    }
  }

  void execute_node(const DNode &node, GeoNodeExecParams params)
  {
    const bNode &bnode = params.node();
    if (bnode.typeinfo->geometry_node_execute != nullptr) {
      bnode.typeinfo->geometry_node_execute(params);
      return;
    }

    /* Use the multi-function implementation of the node. */
    const MultiFunction &fn = *mf_by_node_.lookup(&node);
    MFContextBuilder fn_context;
    MFParamsBuilder fn_params{fn, 1};
    Vector<GMutablePointer> input_data;
    for (const DInputSocket *dsocket : node.inputs()) {
      if (dsocket->is_available()) {
        GMutablePointer data = params.extract_input(dsocket->identifier());
        fn_params.add_readonly_single_input(GSpan(*data.type(), data.get(), 1));
        input_data.append(data);
      }
    }
    Vector<GMutablePointer> output_data;
    for (const DOutputSocket *dsocket : node.outputs()) {
      if (dsocket->is_available()) {
        const CPPType &type = *blender::nodes::socket_cpp_type_get(*dsocket->typeinfo());
        void *buffer = allocator_.allocate(type.size(), type.alignment());
        fn_params.add_uninitialized_single_output(GMutableSpan(type, buffer, 1));
        output_data.append(GMutablePointer(type, buffer));
      }
    }
    fn.call(IndexRange(1), fn_params, fn_context);
    for (GMutablePointer value : input_data) {
      value.destruct();
    }
    int output_index = 0;
    for (const int i : node.outputs().index_range()) {
      if (node.output(i).is_available()) {
        GMutablePointer value = output_data[output_index];
        params.set_output_by_move(node.output(i).identifier(), value);
        value.destruct();
        output_index++;
      }
    }
  }

  void forward_to_inputs(const DOutputSocket &from_socket, GMutablePointer value_to_forward)
  {
    Span<const DInputSocket *> to_sockets_all = from_socket.linked_sockets();

    const CPPType &from_type = *value_to_forward.type();

    Vector<const DInputSocket *> to_sockets_same_type;
    for (const DInputSocket *to_socket : to_sockets_all) {
      const CPPType &to_type = *blender::nodes::socket_cpp_type_get(*to_socket->typeinfo());
      if (from_type == to_type) {
        to_sockets_same_type.append(to_socket);
      }
      else {
        void *buffer = allocator_.allocate(to_type.size(), to_type.alignment());
        if (conversions_.is_convertible(from_type, to_type)) {
          conversions_.convert(from_type, to_type, value_to_forward.get(), buffer);
        }
        else {
          to_type.copy_to_uninitialized(to_type.default_value(), buffer);
        }
        value_by_input_.add_new(to_socket, GMutablePointer{to_type, buffer});
      }
    }

    if (to_sockets_same_type.size() == 0) {
      /* This value is not further used, so destruct it. */
      value_to_forward.destruct();
    }
    else if (to_sockets_same_type.size() == 1) {
      /* This value is only used on one input socket, no need to copy it. */
      const DInputSocket *to_socket = to_sockets_same_type[0];
      value_by_input_.add_new(to_socket, value_to_forward);
    }
    else {
      /* Multiple inputs use the value, make a copy for every input except for one. */
      const DInputSocket *first_to_socket = to_sockets_same_type[0];
      Span<const DInputSocket *> other_to_sockets = to_sockets_same_type.as_span().drop_front(1);
      const CPPType &type = *value_to_forward.type();

      value_by_input_.add_new(first_to_socket, value_to_forward);
      for (const DInputSocket *to_socket : other_to_sockets) {
        void *buffer = allocator_.allocate(type.size(), type.alignment());
        type.copy_to_uninitialized(value_to_forward.get(), buffer);
        value_by_input_.add_new(to_socket, GMutablePointer{type, buffer});
      }
    }
  }

  GMutablePointer get_unlinked_input_value(const DInputSocket &socket)
  {
    bNodeSocket *bsocket;
    if (socket.linked_group_inputs().size() == 0) {
      bsocket = socket.bsocket();
    }
    else {
      bsocket = socket.linked_group_inputs()[0]->bsocket();
    }
    const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket.typeinfo());
    void *buffer = allocator_.allocate(type.size(), type.alignment());

    if (bsocket->type == SOCK_OBJECT) {
      Object *object = ((bNodeSocketValueObject *)bsocket->default_value)->value;
      blender::bke::PersistentObjectHandle object_handle = handle_map_.lookup(object);
      new (buffer) blender::bke::PersistentObjectHandle(object_handle);
    }
    else if (bsocket->type == SOCK_COLLECTION) {
      Collection *collection = ((bNodeSocketValueCollection *)bsocket->default_value)->value;
      blender::bke::PersistentCollectionHandle collection_handle = handle_map_.lookup(collection);
      new (buffer) blender::bke::PersistentCollectionHandle(collection_handle);
    }
    else {
      blender::nodes::socket_cpp_value_get(*bsocket, buffer);
    }

    return {type, buffer};
  }
};

/**
 * This code is responsible for creating the new property and also creating the group of
 * properties in the prop_ui_container group for the UI info, the mapping for which is
 * scattered about in RNA_access.c.
 *
 * TODO(Hans): Codify this with some sort of table or refactor IDProperty use in RNA_access.c.
 */
struct SocketPropertyType {
  /* Create the actual propery used to store the data for the modifier. */
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
  void (*init_cpp_value)(const IDProperty &property, void *r_value);
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

  /* Make the group in the ui container group to hold the property's UI settings. */
  IDProperty *prop_ui_group;
  {
    IDPropertyTemplate idprop = {0};
    prop_ui_group = IDP_New(IDP_GROUP, &idprop, new_prop_name);
    IDP_AddToGroup(ui_container, prop_ui_group);
  }

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
          [](const IDProperty &property) { return property.type == IDP_FLOAT; },
          [](const IDProperty &property, void *r_value) {
            *(float *)r_value = IDP_Float(&property);
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
          [](const IDProperty &property, void *r_value) { *(int *)r_value = IDP_Int(&property); },
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
          [](const IDProperty &property, void *r_value) {
            copy_v3_v3((float *)r_value, (const float *)IDP_Array(&property));
          },
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
          [](const IDProperty &property, void *r_value) {
            *(bool *)r_value = IDP_Int(&property) != 0;
          },
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
          [](const IDProperty &property, void *r_value) {
            new (r_value) std::string(IDP_String(&property));
          },
      };
      return &string_type;
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
  }
  property_type->init_cpp_value(*property, r_value);
}

static void fill_data_handle_map(const DerivedNodeTree &tree,
                                 blender::bke::PersistentDataHandleMap &handle_map)
{
  Set<ID *> used_ids;
  findUsedIds(*tree.btree(), used_ids);

  int current_handle = 0;
  for (ID *id : used_ids) {
    handle_map.add(current_handle, *id);
    current_handle++;
  }
}

/**
 * Evaluate a node group to compute the output geometry.
 * Currently, this uses a fairly basic and inefficient algorithm that might compute things more
 * often than necessary. It's going to be replaced soon.
 */
static GeometrySet compute_geometry(const DerivedNodeTree &tree,
                                    Span<const DOutputSocket *> group_input_sockets,
                                    const DInputSocket &socket_to_compute,
                                    GeometrySet input_geometry_set,
                                    NodesModifierData *nmd,
                                    const ModifierEvalContext *ctx)
{
  blender::ResourceCollector resources;
  blender::LinearAllocator<> &allocator = resources.linear_allocator();
  blender::nodes::MultiFunctionByNode mf_by_node = get_multi_function_per_node(tree, resources);

  Map<const DOutputSocket *, GMutablePointer> group_inputs;

  if (group_input_sockets.size() > 0) {
    Span<const DOutputSocket *> remaining_input_sockets = group_input_sockets;

    /* If the group expects a geometry as first input, use the geometry that has been passed to
     * modifier. */
    const DOutputSocket *first_input_socket = group_input_sockets[0];
    if (first_input_socket->bsocket()->type == SOCK_GEOMETRY) {
      GeometrySet *geometry_set_in = allocator.construct<GeometrySet>(
          std::move(input_geometry_set));
      group_inputs.add_new(first_input_socket, geometry_set_in);
      remaining_input_sockets = remaining_input_sockets.drop_front(1);
    }

    /* Initialize remaining group inputs. */
    for (const DOutputSocket *socket : remaining_input_sockets) {
      const CPPType &cpp_type = *blender::nodes::socket_cpp_type_get(*socket->typeinfo());
      void *value_in = allocator.allocate(cpp_type.size(), cpp_type.alignment());
      initialize_group_input(*nmd, *socket->bsocket(), cpp_type, value_in);
      group_inputs.add_new(socket, {cpp_type, value_in});
    }
  }

  Vector<const DInputSocket *> group_outputs;
  group_outputs.append(&socket_to_compute);

  blender::bke::PersistentDataHandleMap handle_map;
  fill_data_handle_map(tree, handle_map);

  GeometryNodesEvaluator evaluator{
      group_inputs, group_outputs, mf_by_node, handle_map, ctx->object};
  Vector<GMutablePointer> results = evaluator.execute();
  BLI_assert(results.size() == 1);
  GMutablePointer result = results[0];

  GeometrySet output_geometry = std::move(*(GeometrySet *)result.get());
  return output_geometry;
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
      if (socket->type == SOCK_STRING) {
        BKE_modifier_set_error(ob, md, "String socket can not be exposed in the modifier");
      }
      else if (socket->type == SOCK_OBJECT) {
        BKE_modifier_set_error(ob, md, "Object socket can not be exposed in the modifier");
      }
      else if (socket->type == SOCK_GEOMETRY) {
        BKE_modifier_set_error(ob, md, "Node group can only have one geometry input");
      }
      else if (socket->type == SOCK_COLLECTION) {
        BKE_modifier_set_error(ob, md, "Collection socket can not be exposed in the modifier");
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

  blender::nodes::NodeTreeRefMap tree_refs;
  DerivedNodeTree tree{nmd->node_group, tree_refs};

  if (tree.has_link_cycles()) {
    BKE_modifier_set_error(ctx->object, md, "Node group has cycles");
    return;
  }

  Span<const DNode *> input_nodes = tree.nodes_by_type("NodeGroupInput");
  Span<const DNode *> output_nodes = tree.nodes_by_type("NodeGroupOutput");

  if (input_nodes.size() > 1) {
    return;
  }
  if (output_nodes.size() != 1) {
    return;
  }

  Span<const DOutputSocket *> group_inputs = (input_nodes.size() == 1) ?
                                                 input_nodes[0]->outputs().drop_back(1) :
                                                 Span<const DOutputSocket *>{};
  Span<const DInputSocket *> group_outputs = output_nodes[0]->inputs().drop_back(1);

  if (group_outputs.size() == 0) {
    return;
  }

  const DInputSocket *group_output = group_outputs[0];
  if (group_output->idname() != "NodeSocketGeometry") {
    return;
  }

  geometry_set = compute_geometry(
      tree, group_inputs, *group_outputs[0], std::move(geometry_set), nmd, ctx);
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  GeometrySet geometry_set = GeometrySet::create_with_mesh(mesh, GeometryOwnershipType::Editable);
  geometry_set.get_component_for_write<MeshComponent>().copy_vertex_group_names_from_object(
      *ctx->object);
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
  modifyGeometry(md, ctx, *geometry_set);
}

/* Drawing the properties manually with #uiItemR instead of #uiDefAutoButsRNA allows using
 * the node socket identifier for the property names, since they are unique, but also having
 * the correct label displayed in the UI.  */
static void draw_property_for_socket(uiLayout *layout,
                                     PointerRNA *settings_ptr,
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
  if (property != nullptr && property_type->is_correct_type(*property)) {
    char rna_path[128];
    BLI_snprintf(rna_path, ARRAY_SIZE(rna_path), "[\"%s\"]", socket.identifier);
    uiItemR(layout, settings_ptr, rna_path, 0, socket.name, ICON_NONE);
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);
  /* This should be removed, but animation currently doesn't work with the IDProperties. */
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
    PointerRNA settings_ptr;
    RNA_pointer_create(ptr->owner_id, &RNA_NodesModifierSettings, &nmd->settings, &settings_ptr);

    LISTBASE_FOREACH (bNodeSocket *, socket, &nmd->node_group->inputs) {
      draw_property_for_socket(layout, &settings_ptr, nmd->settings.properties, *socket);
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

ModifierTypeInfo modifierType_Nodes = {
    /* name */ "GeometryNodes",
    /* structName */ "NodesModifierData",
    /* structSize */ sizeof(NodesModifierData),
    /* srna */ &RNA_NodesModifier,
    /* type */ eModifierTypeType_Constructive,
    /* flags */
    static_cast<ModifierTypeFlag>(eModifierTypeFlag_AcceptsMesh |
                                  eModifierTypeFlag_SupportsEditmode |
                                  eModifierTypeFlag_EnableInEditmode),
    /* icon */ ICON_NODETREE,

    /* copyData */ copyData,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ nullptr,
    /* modifyGeometrySet */ modifyGeometrySet,
    /* modifyVolume */ nullptr,

    /* initData */ initData,
    /* requiredDataMask */ nullptr,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ nullptr,
    /* dependsOnNormals */ nullptr,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ nullptr,
    /* freeRuntimeData */ nullptr,
    /* panelRegister */ panelRegister,
    /* blendWrite */ blendWrite,
    /* blendRead */ blendRead,
};
