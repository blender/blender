/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_geometry.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "mesh_intern.hh"

namespace blender::ed::mesh {

static VectorSet<std::string> join_vertex_groups(const Span<const Object *> objects_to_join,
                                                 const OffsetIndices<int> vert_ranges,
                                                 Mesh &dst_mesh)
{
  VectorSet<std::string> vertex_group_names;
  LISTBASE_FOREACH (const bDeformGroup *, dg, &dst_mesh.vertex_group_names) {
    vertex_group_names.add_new(dg->name);
  }

  bool any_vertex_group_data = false;
  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Mesh &mesh = *static_cast<const Mesh *>(objects_to_join[i]->data);
    any_vertex_group_data |= CustomData_has_layer(&mesh.vert_data, CD_MDEFORMVERT);
    LISTBASE_FOREACH (const bDeformGroup *, dg, &mesh.vertex_group_names) {
      if (vertex_group_names.add_as(dg->name)) {
        BLI_addtail(&dst_mesh.vertex_group_names, BKE_defgroup_duplicate(dg));
      }
    }
  }

  if (!any_vertex_group_data) {
    return vertex_group_names;
  }

  MDeformVert *dvert = (MDeformVert *)CustomData_add_layer(
      &dst_mesh.vert_data, CD_MDEFORMVERT, CD_CONSTRUCT, dst_mesh.verts_num);

  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Mesh &src_mesh = *static_cast<const Mesh *>(objects_to_join[i]->data);
    const Span<MDeformVert> src_dverts = src_mesh.deform_verts().take_front(vert_ranges[i].size());
    if (src_dverts.is_empty()) {
      continue;
    }
    Vector<int, 32> index_map;
    LISTBASE_FOREACH (const bDeformGroup *, dg, &dst_mesh.vertex_group_names) {
      index_map.append(vertex_group_names.index_of_as(dg->name));
    }
    for (const int vert : src_dverts.index_range()) {
      const MDeformVert &src = src_dverts[vert];
      MDeformVert &dst = dvert[vert_ranges[i][vert]];
      dst = src;
      dst.dw = MEM_malloc_arrayN<MDeformWeight>(src.totweight, __func__);
      for (const int weight : IndexRange(src.totweight)) {
        dst.dw[weight].def_nr = index_map[src.dw[weight].def_nr];
        dst.dw[weight].weight = src.dw[weight].weight;
      }
    }
  }

  return vertex_group_names;
}

static void join_positions_and_shape_keys(Main *bmain,
                                          const Span<const Object *> objects_to_join,
                                          const OffsetIndices<int> vert_ranges,
                                          const float4x4 &world_to_dst_mesh,
                                          Mesh &dst_mesh)
{
  Vector<KeyBlock *> key_blocks;
  VectorSet<std::string> key_names;
  if (Key *key = dst_mesh.key) {
    LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
      key_names.add_new(kb->name);
      key_blocks.append(kb);
    }
  }

  const auto ensure_dst_key = [&]() {
    if (!dst_mesh.key) {
      dst_mesh.key = BKE_key_add(bmain, &dst_mesh.id);
      dst_mesh.key->type = KEY_RELATIVE;
    }
  };

  MutableSpan<float3> dst_positions = dst_mesh.vert_positions_for_write();

  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Key *src_key = static_cast<const Mesh *>(objects_to_join[i]->data)->key;
    if (!src_key) {
      continue;
    }
    ensure_dst_key();
    LISTBASE_FOREACH (const KeyBlock *, src_kb, &src_key->block) {
      if (key_names.add_as(src_kb->name)) {
        KeyBlock *dst_kb = BKE_keyblock_add(dst_mesh.key, src_kb->name);
        BKE_keyblock_copy_settings(dst_kb, src_kb);
        dst_kb->data = MEM_malloc_arrayN<float3>(dst_mesh.verts_num, __func__);
        dst_kb->totelem = dst_mesh.verts_num;

        /* Initialize the new shape key data with the base positions for the active object. */
        MutableSpan<float3> key_data(static_cast<float3 *>(dst_kb->data), dst_kb->totelem);
        key_data.take_front(vert_ranges[0].size())
            .copy_from(dst_positions.take_front(vert_ranges[0].size()));

        /* Remap `KeyBlock::relative`. */
        if (const KeyBlock *src_kb_relative = static_cast<KeyBlock *>(
                BLI_findlink(&src_key->block, src_kb->relative)))
        {
          dst_kb->relative = key_names.index_of_as(src_kb_relative->name);
        }
      }
    }
  }

  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Object &src_object = *objects_to_join[i];
    const IndexRange dst_range = vert_ranges[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    const Span<float3> src_positions = src_mesh.vert_positions().take_front(dst_range.size());
    const float4x4 transform = world_to_dst_mesh * src_object.object_to_world();
    math::transform_points(src_positions, transform, dst_positions.slice(dst_range));

    if (Key *dst_key = dst_mesh.key) {
      LISTBASE_FOREACH (KeyBlock *, kb, &dst_key->block) {
        MutableSpan<float3> key_data(static_cast<float3 *>(kb->data), kb->totelem);
        if (const KeyBlock *src_kb = src_mesh.key ?
                                         BKE_keyblock_find_name(src_mesh.key, kb->name) :
                                         nullptr)
        {
          const Span<float3> src_kb_data(static_cast<float3 *>(src_kb->data), dst_range.size());
          math::transform_points(src_kb_data, transform, key_data.slice(dst_range));
        }
        else {
          key_data.slice(dst_range).copy_from(dst_positions.slice(dst_range));
        }
      }
    }
  }
}

static void join_generic_attributes(const Span<const Object *> objects_to_join,
                                    const VectorSet<std::string> &all_vertex_group_names,
                                    const OffsetIndices<int> vert_ranges,
                                    const OffsetIndices<int> edge_ranges,
                                    const OffsetIndices<int> face_ranges,
                                    const OffsetIndices<int> corner_ranges,
                                    Mesh &dst_mesh)
{
  Set<StringRef> skip_names{"position",
                            ".edge_verts",
                            ".corner_vert",
                            ".corner_edge",
                            "material_index",
                            ".sculpt_face_set"};

  Array<std::string> names;
  Array<bke::AttributeDomainAndType> kinds;
  {
    bke::GeometrySet::GatheredAttributes attr_info;
    for (const int i : objects_to_join.index_range()) {
      const Mesh &mesh = *static_cast<const Mesh *>(objects_to_join[i]->data);
      mesh.attributes().foreach_attribute([&](const bke::AttributeIter &attr) {
        if (skip_names.contains(attr.name) || all_vertex_group_names.contains(attr.name)) {
          return;
        }
        attr_info.add(attr.name, {attr.domain, attr.data_type});
      });
    }
    names.reinitialize(attr_info.names.size());
    kinds.reinitialize(attr_info.names.size());
    for (const int i : attr_info.names.index_range()) {
      names[i] = attr_info.names[i];
      kinds[i] = attr_info.kinds[i];
    }
  }

  bke::MutableAttributeAccessor dst_attributes = dst_mesh.attributes_for_write();

  const Set<StringRefNull> attribute_names = dst_attributes.all_ids();
  for (const int attr_i : names.index_range()) {
    const StringRef name = names[attr_i];
    const bke::AttrDomain domain = kinds[attr_i].domain;
    const bke::AttrType data_type = kinds[attr_i].data_type;
    if (const std::optional<bke::AttributeMetaData> meta_data = dst_attributes.lookup_meta_data(
            name))
    {
      if (meta_data->domain != domain || meta_data->data_type != data_type) {
        AttributeOwner owner = AttributeOwner::from_id(&dst_mesh.id);
        geometry::convert_attribute(
            owner, dst_attributes, name, meta_data->domain, meta_data->data_type, nullptr);
      }
    }
    else {
      dst_attributes.add(name, domain, data_type, bke::AttributeInitConstruct());
    }
  }

  for (const int attr_i : names.index_range()) {
    const StringRef name = names[attr_i];
    const bke::AttrDomain domain = kinds[attr_i].domain;
    const bke::AttrType data_type = kinds[attr_i].data_type;

    bke::GSpanAttributeWriter dst = dst_attributes.lookup_for_write_span(name);
    for (const int i : objects_to_join.index_range().drop_front(1)) {
      const Mesh &src_mesh = *static_cast<const Mesh *>(objects_to_join[i]->data);
      const bke::AttributeAccessor src_attributes = src_mesh.attributes();
      const GVArray src = *src_attributes.lookup_or_default(name, domain, data_type);

      const IndexRange dst_range = [&]() {
        switch (domain) {
          case bke::AttrDomain::Point:
            return vert_ranges[i];
          case bke::AttrDomain::Edge:
            return edge_ranges[i];
          case bke::AttrDomain::Face:
            return face_ranges[i];
          case bke::AttrDomain::Corner:
            return corner_ranges[i];
          default:
            BLI_assert_unreachable();
            return IndexRange();
        }
      }();

      src.materialize(IndexRange(dst_range.size()), dst.span.slice(dst_range).data());
    }
    dst.finish();
  }
}

static VectorSet<Material *> join_materials(const Span<const Object *> objects_to_join,
                                            const OffsetIndices<int> face_ranges,
                                            Mesh &dst_mesh)
{
  VectorSet<Material *> materials;
  for (const int i : objects_to_join.index_range()) {
    const Object &src_object = *objects_to_join[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    if (src_mesh.totcol == 0) {
      materials.add(nullptr);
      continue;
    }
    for (const int material_index : IndexRange(src_mesh.totcol)) {
      Material *material = BKE_object_material_get(&const_cast<Object &>(src_object),
                                                   material_index + 1);
      if (materials.size() < MAXMAT) {
        materials.add(material);
      }
    }
  }

  bke::MutableAttributeAccessor dst_attributes = dst_mesh.attributes_for_write();
  if (materials.size() <= 1) {
    dst_attributes.remove("material_index");
    return materials;
  }

  bke::SpanAttributeWriter dst_material_indices = dst_attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Face);
  if (!dst_material_indices) {
    return {};
  }

  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Object &src_object = *objects_to_join[i];
    const IndexRange dst_range = face_ranges[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    const bke::AttributeAccessor src_attributes = src_mesh.attributes();

    const VArray<int> material_indices = *src_attributes.lookup<int>("material_index",
                                                                     bke::AttrDomain::Face);
    if (material_indices.is_empty()) {
      Material *first_material = src_mesh.totcol == 0 ?
                                     nullptr :
                                     BKE_object_material_get(&const_cast<Object &>(src_object), 1);
      dst_material_indices.span.slice(dst_range).fill(materials.index_of(first_material));
      continue;
    }

    if (src_mesh.totcol == 0) {
      /* These material indices are invalid, but copy them anyway to avoid destroying user data. */
      material_indices.materialize(dst_range.index_range(),
                                   dst_material_indices.span.slice(dst_range));
      continue;
    }

    Array<int, 32> index_map(src_mesh.totcol);
    for (const int material_index : IndexRange(src_mesh.totcol)) {
      Material *material = BKE_object_material_get(&const_cast<Object &>(src_object),
                                                   material_index + 1);
      const int dst_index = materials.index_of_try(material);
      index_map[material_index] = dst_index == -1 ? 0 : dst_index;
    }

    const int max = src_mesh.totcol - 1;
    for (const int face : dst_range.index_range()) {
      const int src = std::clamp(material_indices[face], 0, max);
      dst_material_indices.span[dst_range[face]] = index_map[src];
    }
  }

  dst_material_indices.finish();

  return materials;
}

/* Face Sets IDs are a sparse sequence, so this function offsets all the IDs by face_set_offset and
 * updates face_set_offset with the maximum ID value. This way, when used in multiple meshes, all
 * of them will have different IDs for their Face Sets. */
static void join_face_sets(const Span<const Object *> objects_to_join,
                           const OffsetIndices<int> face_ranges,
                           Mesh &dst_mesh)
{
  bke::MutableAttributeAccessor dst_attributes = dst_mesh.attributes_for_write();
  bke::SpanAttributeWriter dst_face_sets = dst_attributes.lookup_for_write_span<int>(
      ".sculpt_face_set");
  if (!dst_face_sets) {
    return;
  }
  if (dst_face_sets.domain != bke::AttrDomain::Face) {
    return;
  }

  int max_face_set = 1;
  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Object &src_object = *objects_to_join[i];
    const IndexRange dst_range = face_ranges[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    const bke::AttributeAccessor src_attributes = src_mesh.attributes();
    const VArraySpan src_face_sets = *src_attributes.lookup<int>(".sculpt_face_set",
                                                                 bke::AttrDomain::Face);
    if (src_face_sets.is_empty()) {
      dst_face_sets.span.slice(dst_range).fill(max_face_set);
    }
    else {
      for (const int face : dst_range.index_range()) {
        dst_face_sets.span[dst_range[face]] = src_face_sets[face] + max_face_set;
      }
      max_face_set = std::max(
          max_face_set,
          *std::max_element(src_face_sets.begin(), src_face_sets.begin() + dst_range.size()));
    }
    max_face_set++;
  }
  dst_face_sets.finish();
}

wmOperatorStatus join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *const active_object = CTX_data_active_object(C);

  if (active_object->mode & OB_MODE_EDIT) {
    BKE_report(op->reports, RPT_WARNING, "Cannot join while in edit mode");
    return OPERATOR_CANCELLED;
  }

  /* active_object is the object we are adding geometry to */
  if (!active_object || active_object->type != OB_MESH) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a mesh");
    return OPERATOR_CANCELLED;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  Vector<Object *> objects_to_join;
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter->type == OB_MESH) {
      objects_to_join.append(ob_iter);
    }
  }
  CTX_DATA_END;

  {
    /* Make sure the active object is first, that way its data will be first in the new mesh. */
    const int active_index = objects_to_join.as_span().first_index_try(active_object);
    if (active_index == -1) {
      BKE_report(op->reports, RPT_WARNING, "Active object is not a selected mesh");
      return OPERATOR_CANCELLED;
    }
    objects_to_join.remove(active_index);
    objects_to_join.prepend(active_object);
  }

  Array<int> vert_offset_data(objects_to_join.size() + 1);
  Array<int> edge_offset_data(objects_to_join.size() + 1);
  Array<int> face_offset_data(objects_to_join.size() + 1);
  Array<int> corner_offset_data(objects_to_join.size() + 1);
  for (const int i : objects_to_join.index_range()) {
    const Mesh &mesh = *static_cast<const Mesh *>(objects_to_join[i]->data);
    vert_offset_data[i] = mesh.verts_num;
    edge_offset_data[i] = mesh.edges_num;
    face_offset_data[i] = mesh.faces_num;
    corner_offset_data[i] = mesh.corners_num;
  }

  const OffsetIndices<int> vert_ranges = offset_indices::accumulate_counts_to_offsets(
      vert_offset_data);
  const OffsetIndices<int> edge_ranges = offset_indices::accumulate_counts_to_offsets(
      edge_offset_data);
  const OffsetIndices<int> face_ranges = offset_indices::accumulate_counts_to_offsets(
      face_offset_data);
  const OffsetIndices<int> corner_ranges = offset_indices::accumulate_counts_to_offsets(
      corner_offset_data);

  /* Apply parent transform if the active object's parent was joined to it.
   * NOTE: This doesn't apply recursive parenting. */
  if (objects_to_join.contains(active_object->parent)) {
    active_object->parent = nullptr;
    BKE_object_apply_mat4_ex(active_object,
                             active_object->object_to_world().ptr(),
                             active_object->parent,
                             active_object->parentinv,
                             false);
  }

  /* Only join meshes if there are verts to join,
   * there aren't too many, and we only had one mesh selected. */
  Mesh *dst_mesh = (Mesh *)active_object->data;
  Key *key = dst_mesh->key;

  if (ELEM(vert_ranges.total_size(), 0, dst_mesh->verts_num)) {
    BKE_report(op->reports, RPT_WARNING, "No mesh data to join");
    return OPERATOR_CANCELLED;
  }

  if (vert_ranges.total_size() > MESH_MAX_VERTS) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Joining results in %d vertices, limit is %ld",
                vert_ranges.total_size(),
                MESH_MAX_VERTS);
    return OPERATOR_CANCELLED;
  }

  CustomData_realloc(&dst_mesh->vert_data, dst_mesh->verts_num, vert_ranges.total_size());
  CustomData_realloc(&dst_mesh->edge_data, dst_mesh->edges_num, edge_ranges.total_size());
  CustomData_realloc(&dst_mesh->face_data, dst_mesh->faces_num, face_ranges.total_size());
  CustomData_realloc(&dst_mesh->corner_data, dst_mesh->corners_num, corner_ranges.total_size());
  if (face_ranges.total_size() != dst_mesh->faces_num) {
    implicit_sharing::resize_trivial_array(&dst_mesh->face_offset_indices,
                                           &dst_mesh->runtime->face_offsets_sharing_info,
                                           dst_mesh->faces_num,
                                           face_ranges.total_size() + 1);
  }
  dst_mesh->verts_num = vert_ranges.total_size();
  dst_mesh->edges_num = edge_ranges.total_size();
  dst_mesh->faces_num = face_ranges.total_size();
  dst_mesh->corners_num = corner_ranges.total_size();
  if (Key *key = dst_mesh->key) {
    LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
      kb->data = MEM_reallocN(kb->data, sizeof(float3) * dst_mesh->verts_num);
      kb->totelem = dst_mesh->verts_num;
    }
  }

  BKE_mesh_runtime_clear_geometry(dst_mesh);

  /* Inverse transform for all selected meshes in this object,
   * See #object_join_exec for detailed comment on why the safe version is used. */
  float4x4 world_to_active_object;
  invert_m4_m4_safe_ortho(world_to_active_object.ptr(), active_object->object_to_world().ptr());

  join_positions_and_shape_keys(
      bmain, objects_to_join, vert_ranges, world_to_active_object, *dst_mesh);

  MutableSpan<int2> dst_edges = dst_mesh->edges_for_write();
  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Object &src_object = *objects_to_join[i];
    const IndexRange dst_range = edge_ranges[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    const Span<int2> src_edges = src_mesh.edges();
    for (const int edge : dst_range.index_range()) {
      dst_edges[dst_range[edge]] = src_edges[edge] + int(vert_ranges[i].start());
    }
  }

  MutableSpan<int> dst_corner_verts = dst_mesh->corner_verts_for_write();
  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Object &src_object = *objects_to_join[i];
    const IndexRange dst_range = corner_ranges[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    const Span<int> src_corner_verts = src_mesh.corner_verts();
    for (const int corner : dst_range.index_range()) {
      dst_corner_verts[dst_range[corner]] = src_corner_verts[corner] + int(vert_ranges[i].start());
    }
  }

  MutableSpan<int> dst_corner_edges = dst_mesh->corner_edges_for_write();
  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Object &src_object = *objects_to_join[i];
    const IndexRange dst_range = corner_ranges[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    const Span<int> src_corner_edges = src_mesh.corner_edges();
    for (const int corner : dst_range.index_range()) {
      dst_corner_edges[dst_range[corner]] = src_corner_edges[corner] + int(edge_ranges[i].start());
    }
  }

  MutableSpan<int> dst_face_offsets = dst_mesh->face_offsets_for_write();
  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Object &src_object = *objects_to_join[i];
    const IndexRange dst_range = face_ranges[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    const Span<int> src_face_offsets = src_mesh.face_offsets();
    for (const int face : dst_range.index_range()) {
      dst_face_offsets[dst_range[face]] = src_face_offsets[face] + corner_ranges[i].start();
    }
  }
  dst_face_offsets.last() = dst_mesh->corners_num;

  for (const int i : objects_to_join.index_range().drop_front(1)) {
    const Object &src_object = *objects_to_join[i];
    const Mesh &src_mesh = *static_cast<const Mesh *>(src_object.data);
    const Key *src_key = src_mesh.key;
    if (!src_key) {
      continue;
    }
  }

  for (const int i : objects_to_join.index_range().drop_front(1)) {
    Object &src_object = *objects_to_join[i];
    multiresModifier_prepare_join(depsgraph, scene, &src_object, active_object);
    if (MultiresModifierData *mmd = get_multires_modifier(scene, &src_object, true)) {
      object::iter_other(
          bmain, &src_object, true, object::multires_update_totlevels, &mmd->totlvl);
    }
  }

  join_face_sets(objects_to_join, face_ranges, *dst_mesh);

  VectorSet<Material *> materials = join_materials(objects_to_join, face_ranges, *dst_mesh);

  VectorSet<std::string> vertex_group_names = join_vertex_groups(
      objects_to_join, vert_ranges, *dst_mesh);

  join_generic_attributes(objects_to_join,
                          vertex_group_names,
                          vert_ranges,
                          edge_ranges,
                          face_ranges,
                          corner_ranges,
                          *dst_mesh);

  for (Object *object : objects_to_join.as_span().drop_front(1)) {
    object::base_free_and_unlink(bmain, scene, object);
  }

  /* old material array */
  for (const int a : IndexRange(active_object->totcol)) {
    if (Material *ma = active_object->mat[a]) {
      id_us_min(&ma->id);
    }
  }
  for (const int a : IndexRange(active_object->totcol)) {
    if (Material *ma = dst_mesh->mat[a]) {
      id_us_min(&ma->id);
    }
  }
  MEM_SAFE_FREE(active_object->mat);
  MEM_SAFE_FREE(active_object->matbits);
  MEM_SAFE_FREE(dst_mesh->mat);

  /* If the object had no slots, don't add an empty one. */
  if (active_object->totcol == 0 && materials.size() == 1 && materials[0] == nullptr) {
    materials.clear();
  }

  const int totcol = materials.size();
  if (totcol) {
    VectorData data = materials.extract_vector().release();
    dst_mesh->mat = data.data;
    for (const int i : IndexRange(totcol)) {
      if (Material *ma = dst_mesh->mat[i]) {
        id_us_plus((ID *)ma);
      }
    }
    active_object->mat = MEM_calloc_arrayN<Material *>(totcol, __func__);
    active_object->matbits = MEM_calloc_arrayN<char>(totcol, __func__);
  }

  active_object->totcol = dst_mesh->totcol = totcol;

  /* other mesh users */
  BKE_objects_materials_sync_length_all(bmain, (ID *)dst_mesh);

  /* ensure newly inserted keys are time sorted */
  if (key && (key->type != KEY_RELATIVE)) {
    BKE_key_sort(key);
  }

  /* Due to dependency cycle some other object might access old derived data. */
  BKE_object_free_derived_caches(active_object);

  DEG_relations_tag_update(bmain); /* removed objects, need to rebuild dag */

  DEG_id_tag_update(&active_object->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

}  // namespace blender::ed::mesh
