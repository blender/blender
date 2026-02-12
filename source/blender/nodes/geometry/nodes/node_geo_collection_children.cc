/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_collection_types.h"

#include "BKE_collection.hh"
#include "BKE_lib_id.hh"

#include "DEG_depsgraph_query.hh"

#include "node_geometry_util.hh"

#include <algorithm>

namespace blender::nodes::node_geo_collection_children_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Collection>("Collection").optional_label();
  b.add_input<decl::Bool>("Recursive").description("Recursively retrieve collections and objects");
  b.add_output<decl::Collection>("Collections").structure_type(StructureType::List);
  b.add_output<decl::Object>("Objects").structure_type(StructureType::List);
}

static void collection_children_recursive(Collection *collection,
                                          Vector<Collection *> &collections,
                                          Set<Collection *> &visited)
{
  for (CollectionChild &child : collection->children) {
    Collection *cc = child.collection;
    if (visited.add(cc)) {
      collections.append(cc);
      collection_children_recursive(cc, collections, visited);
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Collection *collection = params.extract_input<Collection *>("Collection");
  const bool recursive = params.extract_input<bool>("Recursive");

  if (collection == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }

  Vector<Collection *> child_collections;
  if (recursive) {
    Set<Collection *> visited;
    collection_children_recursive(collection, child_collections, visited);
  }
  else {
    for (CollectionChild &child : collection->children) {
      child_collections.append(child.collection);
    }
  }

  std::ranges::sort(child_collections, [](const Collection *a, const Collection *b) {
    return BLI_strcasecmp_natural(BKE_id_name(a->id), BKE_id_name(b->id)) < 0;
  });

  Vector<Object *> child_objects;
  Vector<Collection *> obj_collections;
  obj_collections.append(collection);
  if (recursive) {
    obj_collections.extend(child_collections);
  }

  params.set_output("Collections", List::from_container(std::move(child_collections)));

  if (!params.output_is_required("Objects")) {
    params.set_default_remaining_outputs();
    return;
  }

  Set<const Object *> obj_visited;
  for (Collection *col : obj_collections) {
    for (CollectionObject &cob : col->gobject) {
      Object *obj_original = DEG_get_original(cob.ob);
      if (obj_visited.add(obj_original)) {
        child_objects.append(obj_original);
      }
    }
  }

  std::ranges::sort(child_objects, [](const Object *a, const Object *b) {
    return BLI_strcasecmp_natural(BKE_id_name(a->id), BKE_id_name(b->id)) < 0;
  });

  params.set_output("Objects", List::from_container(std::move(child_objects)));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeCollectionChildren");
  ntype.ui_name = "Collection Children";
  ntype.ui_description =
      "Retrieve a collection's object and collection children, in a name-based order";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_collection_children_cc
