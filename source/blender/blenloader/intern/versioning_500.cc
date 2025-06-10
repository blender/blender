/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_mesh_types.h"

#include "BLI_listbase.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "BKE_attribute_legacy_convert.hh"
#include "BKE_main.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"
// static CLG_LogRef LOG = {"blo.readfile.doversion"};

static CustomDataLayer *find_old_seam_layer(CustomData &custom_data, const blender::StringRef name)
{
  for (CustomDataLayer &layer : blender::MutableSpan(custom_data.layers, custom_data.totlayer)) {
    if (layer.name == name) {
      return &layer;
    }
  }
  return nullptr;
}

static void rename_mesh_uv_seam_attribute(Mesh &mesh)
{
  using namespace blender;
  CustomDataLayer *old_seam_layer = find_old_seam_layer(mesh.edge_data, ".uv_seam");
  if (!old_seam_layer) {
    return;
  }
  Set<StringRef> names;
  for (const CustomDataLayer &layer : Span(mesh.vert_data.layers, mesh.vert_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.edge_data.layers, mesh.edge_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.face_data.layers, mesh.face_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.corner_data.layers, mesh.corner_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  LISTBASE_FOREACH (const bDeformGroup *, vertex_group, &mesh.vertex_group_names) {
    names.add(vertex_group->name);
  }

  /* If the new UV name is already taken, still rename the attribute so it becomes visible in the
   * list. Then the user can deal with the name conflict themselves. */
  const std::string new_name = BLI_uniquename_cb(
      [&](const StringRef name) { return names.contains(name); }, '.', "uv_seam");
  STRNCPY(old_seam_layer->name, new_name.c_str());
}

static void initialize_closure_input_structure_types(bNodeTree &ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->type_legacy == GEO_NODE_EVALUATE_CLOSURE) {
      auto *storage = static_cast<NodeGeometryEvaluateClosure *>(node->storage);
      for (const int i : blender::IndexRange(storage->input_items.items_num)) {
        NodeGeometryEvaluateClosureInputItem &item = storage->input_items.items[i];
        if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
          item.structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC;
        }
      }
      for (const int i : blender::IndexRange(storage->output_items.items_num)) {
        NodeGeometryEvaluateClosureOutputItem &item = storage->output_items.items[i];
        if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
          item.structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC;
        }
      }
    }
  }
}

void do_versions_after_linking_500(FileData * /*fd*/, Main * /*bmain*/)
{
  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

void blo_do_versions_500(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  using namespace blender;
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 1)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      bke::mesh_sculpt_mask_to_generic(*mesh);
      bke::mesh_custom_normals_to_generic(*mesh);
      rename_mesh_uv_seam_attribute(*mesh);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 2)) {
    LISTBASE_FOREACH (PointCloud *, pointcloud, &bmain->pointclouds) {
      blender::bke::pointcloud_convert_customdata_to_storage(*pointcloud);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 3)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        initialize_closure_input_structure_types(*ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}
