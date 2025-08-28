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
#include "BLI_math_vector.h"
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

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "mesh_intern.hh"

namespace blender::ed::mesh {

/* join selected meshes into the active mesh, context sensitive
 * return 0 if no join is made (error) and 1 if the join is done */

static void join_mesh_single(Depsgraph *depsgraph,
                             Main *bmain,
                             Scene *scene,
                             Object *ob_dst,
                             Object *ob_src,
                             const float4x4 &world_to_active_object,
                             MutableSpan<float3> dst_positions,
                             MutableSpan<int2> dst_edges,
                             MutableSpan<int> dst_corner_verts,
                             MutableSpan<int> dst_corner_edges,
                             MutableSpan<int> dst_face_offsets,
                             CustomData *vert_data,
                             CustomData *edge_data,
                             CustomData *face_data,
                             CustomData *corner_data,
                             int verts_num,
                             int edges_num,
                             int faces_num,
                             int corners_num,
                             Key *key,
                             Key *nkey,
                             Vector<Material *> &materials,
                             const IndexRange vert_range,
                             const IndexRange edge_range,
                             const IndexRange face_range,
                             const IndexRange corner_range)
{
  int a;

  Mesh *mesh_src = static_cast<Mesh *>(ob_src->data);

  if (mesh_src->verts_num) {
    /* standard data */
    CustomData_merge_layout(
        &mesh_src->vert_data, vert_data, CD_MASK_MESH.vmask, CD_SET_DEFAULT, verts_num);
    CustomData_copy_data_named(
        &mesh_src->vert_data, vert_data, 0, vert_range.start(), mesh_src->verts_num);

    /* vertex groups */
    MDeformVert *dvert = (MDeformVert *)CustomData_get_layer_for_write(
        vert_data, CD_MDEFORMVERT, verts_num);
    const MDeformVert *dvert_src = (const MDeformVert *)CustomData_get_layer(&mesh_src->vert_data,
                                                                             CD_MDEFORMVERT);

    /* Remap to correct new vgroup indices, if needed. */
    if (dvert_src) {
      BLI_assert(dvert != nullptr);

      /* Build src to merged mapping of vgroup indices. */
      int *vgroup_index_map;
      int vgroup_index_map_len;
      vgroup_index_map = BKE_object_defgroup_index_map_create(
          ob_src, ob_dst, &vgroup_index_map_len);
      BKE_object_defgroup_index_map_apply(
          &dvert[vert_range.start()], mesh_src->verts_num, vgroup_index_map, vgroup_index_map_len);
      if (vgroup_index_map != nullptr) {
        MEM_freeN(vgroup_index_map);
      }
    }

    /* if this is the object we're merging into, no need to do anything */
    if (ob_src != ob_dst) {
      float cmat[4][4];

      /* Watch this: switch matrix multiplication order really goes wrong. */
      mul_m4_m4m4(cmat, world_to_active_object.ptr(), ob_src->object_to_world().ptr());

      math::transform_points(float4x4(cmat), dst_positions.slice(vert_range));

      /* For each shape-key in destination mesh:
       * - if there's a matching one, copy it across
       *   (will need to transform vertices into new space...).
       * - otherwise, just copy its own coordinates of mesh
       *   (no need to transform vertex coordinates into new space).
       */
      if (key) {
        /* if this mesh has any shape-keys, check first, otherwise just copy coordinates */
        LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
          MutableSpan<float3> key_data(static_cast<float3 *>(kb->data), kb->totelem);
          if (const KeyBlock *src_kb = mesh_src->key ?
                                           BKE_keyblock_find_name(mesh_src->key, kb->name) :
                                           nullptr)
          {
            const Span<float3> src_kb_data(static_cast<float3 *>(src_kb->data), src_kb->totelem);
            math::transform_points(src_kb_data, float4x4(cmat), key_data);
          }
          else {
            key_data.slice(vert_range).copy_from(dst_positions.slice(vert_range));
          }
        }
      }
    }
    else {
      /* for each shape-key in destination mesh:
       * - if it was an 'original', copy the appropriate data from nkey
       * - otherwise, copy across plain coordinates (no need to transform coordinates)
       */
      if (key) {
        LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
          MutableSpan<float3> key_data(static_cast<float3 *>(kb->data), kb->totelem);
          if (const KeyBlock *src_kb = nkey ? BKE_keyblock_find_name(nkey, kb->name) : nullptr) {
            const Span<float3> src_kb_data(static_cast<float3 *>(src_kb->data), src_kb->totelem);
            key_data.slice(vert_range).copy_from(src_kb_data);
          }
          else {
            key_data.slice(vert_range).copy_from(dst_positions.slice(vert_range));
          }
        }
      }
    }
  }

  if (mesh_src->edges_num) {
    CustomData_merge_layout(
        &mesh_src->edge_data, edge_data, CD_MASK_MESH.emask, CD_SET_DEFAULT, edges_num);
    CustomData_copy_data_named(
        &mesh_src->edge_data, edge_data, 0, edge_range.start(), mesh_src->edges_num);

    for (int2 &edge : dst_edges.slice(edge_range)) {
      edge += vert_range.start();
    }
  }

  if (mesh_src->corners_num) {
    if (ob_src != ob_dst) {
      MultiresModifierData *mmd;

      multiresModifier_prepare_join(depsgraph, scene, ob_src, ob_dst);

      if ((mmd = get_multires_modifier(scene, ob_src, true))) {
        object::iter_other(bmain, ob_src, true, object::multires_update_totlevels, &mmd->totlvl);
      }
    }

    CustomData_merge_layout(
        &mesh_src->corner_data, corner_data, CD_MASK_MESH.lmask, CD_SET_DEFAULT, corners_num);
    CustomData_copy_data_named(
        &mesh_src->corner_data, corner_data, 0, corner_range.start(), mesh_src->corners_num);

    for (int &vert : dst_corner_verts.slice(corner_range)) {
      vert += vert_range.start();
    }
    for (int &edge : dst_corner_edges.slice(corner_range)) {
      edge += edge_range.start();
    }
  }

  /* Make remapping for material indices. Assume at least one slot,
   * that will be null if there are no actual slots. */
  const int totcol = std::max(ob_src->totcol, 1);
  Vector<int> matmap(totcol);
  if (mesh_src->faces_num) {
    for (a = 1; a <= totcol; a++) {
      Material *ma = (a <= ob_src->totcol) ? BKE_object_material_get(ob_src, a) : nullptr;

      /* Try to reuse existing slot. */
      int b = 0;
      for (; b < materials.size(); b++) {
        if (ma == materials[b]) {
          matmap[a - 1] = b;
          break;
        }
      }

      if (b == materials.size()) {
        if (materials.size() == MAXMAT) {
          /* Reached max limit of materials, use first slot. */
          matmap[a - 1] = 0;
        }
        else {
          /* Add new slot. */
          matmap[a - 1] = materials.size();
          materials.append(ma);
          if (ma) {
            id_us_plus(&ma->id);
          }
        }
      }
    }

    CustomData_merge_layout(
        &mesh_src->face_data, face_data, CD_MASK_MESH.pmask, CD_SET_DEFAULT, faces_num);
    CustomData_copy_data_named(
        &mesh_src->face_data, face_data, 0, face_range.start(), mesh_src->faces_num);

    /* Apply matmap. In case we don't have material indices yet, create them if more than one
     * material is the result of joining. */
    int *material_indices = static_cast<int *>(CustomData_get_layer_named_for_write(
        face_data, CD_PROP_INT32, "material_index", faces_num));
    if (!material_indices && materials.size() > 1) {
      material_indices = (int *)CustomData_add_layer_named(
          face_data, CD_PROP_INT32, CD_SET_DEFAULT, faces_num, "material_index");
    }
    if (material_indices) {
      for (a = 0; a < mesh_src->faces_num; a++) {
        /* Clamp invalid slots, matching #BKE_object_material_get_p. */
        const int mat_index = std::clamp(material_indices[a + face_range.start()], 0, totcol - 1);
        material_indices[a + face_range.start()] = matmap[mat_index];
      }
    }

    const Span<int> src_face_offsets = mesh_src->face_offsets();
    for (const int i : face_range.index_range()) {
      dst_face_offsets[face_range[i]] = src_face_offsets[i] + corner_range.start();
    }
  }
}

/* Face Sets IDs are a sparse sequence, so this function offsets all the IDs by face_set_offset and
 * updates face_set_offset with the maximum ID value. This way, when used in multiple meshes, all
 * of them will have different IDs for their Face Sets. */
static void mesh_join_offset_face_sets_ID(Mesh *mesh, int *face_set_offset)
{
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<int> face_sets = attributes.lookup_for_write_span<int>(
      ".sculpt_face_set");
  if (!face_sets) {
    return;
  }

  int max_face_set = 0;
  for (const int i : face_sets.span.index_range()) {
    /* As face sets encode the visibility in the integer sign, the offset needs to be added or
     * subtracted depending on the initial sign of the integer to get the new ID. */
    if (face_sets.span[i] <= *face_set_offset) {
      face_sets.span[i] += *face_set_offset;
    }
    max_face_set = max_ii(max_face_set, face_sets.span[i]);
  }
  *face_set_offset = max_face_set;
  face_sets.finish();
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

  int haskey = 0;

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
    if (mesh.key) {
      haskey++;
    }
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

  /* Active object materials in new main array, is nicer start! */
  Vector<Material *> materials;
  for (const int a : IndexRange(active_object->totcol)) {
    materials.append(BKE_object_material_get(active_object, a + 1));
    id_us_plus((ID *)materials[a]);
    /* increase id->us : will be lowered later */
  }

  /* - If destination mesh had shape-keys, move them somewhere safe, and set up placeholders
   *   with arrays that are large enough to hold shape-key data for all meshes.
   * - If destination mesh didn't have shape-keys, but we encountered some in the meshes we're
   *   joining, set up a new key-block and assign to the mesh.
   */
  Key *nkey = nullptr;
  if (key) {
    /* make a duplicate copy that will only be used here... (must remember to free it!) */
    nkey = (Key *)BKE_id_copy(bmain, &key->id);

    /* for all keys in old block, clear data-arrays */
    LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
      if (kb->data) {
        MEM_freeN(kb->data);
      }
      kb->data = MEM_callocN(sizeof(float[3]) * vert_ranges.total_size(), "join_shapekey");
      kb->totelem = vert_ranges.total_size();
    }
  }
  else if (haskey) {
    /* add a new key-block and add to the mesh */
    key = dst_mesh->key = BKE_key_add(bmain, (ID *)dst_mesh);
    key->type = KEY_RELATIVE;
  }

  /* Update face_set_id_offset with the face set data in the active object first. This way the Face
   * Sets IDs in the active object are not the ones that are modified. */
  int face_set_id_offset = 0;
  mesh_join_offset_face_sets_ID(dst_mesh, &face_set_id_offset);

  /* Copy materials, vertex-groups, face sets & face-maps across objects. */
  for (const Object *ob_iter : objects_to_join) {
    if (ob_iter == active_object) {
      continue;
    }
    Mesh *mesh = static_cast<Mesh *>(ob_iter->data);

    /* Join this object's vertex groups to the base one's */
    LISTBASE_FOREACH (bDeformGroup *, dg, &mesh->vertex_group_names) {
      /* See if this group exists in the object (if it doesn't, add it to the end) */
      if (!BKE_object_defgroup_find_name(active_object, dg->name)) {
        bDeformGroup *odg = MEM_mallocN<bDeformGroup>(__func__);
        memcpy(odg, dg, sizeof(bDeformGroup));
        BLI_addtail(&dst_mesh->vertex_group_names, odg);
      }
    }
    if (!BLI_listbase_is_empty(&dst_mesh->vertex_group_names) &&
        mesh->vertex_group_active_index == 0)
    {
      mesh->vertex_group_active_index = 1;
    }

    mesh_join_offset_face_sets_ID(mesh, &face_set_id_offset);

    if (mesh->verts_num) {
      /* If this mesh has shape-keys,
       * check if destination mesh already has matching entries too. */
      if (mesh->key && key) {
        /* for remapping KeyBlock.relative */
        int *index_map = MEM_malloc_arrayN<int>(mesh->key->totkey, __func__);
        KeyBlock **kb_map = MEM_malloc_arrayN<KeyBlock *>(mesh->key->totkey, __func__);

        int i;
        LISTBASE_FOREACH_INDEX (KeyBlock *, kb, &mesh->key->block, i) {
          BLI_assert(i < mesh->key->totkey);

          KeyBlock *kbn = BKE_keyblock_find_name(key, kb->name);
          /* if key doesn't exist in destination mesh, add it */
          if (kbn) {
            index_map[i] = BLI_findindex(&key->block, kbn);
          }
          else {
            index_map[i] = key->totkey;

            kbn = BKE_keyblock_add(key, kb->name);

            BKE_keyblock_copy_settings(kbn, kb);

            /* adjust settings to fit (allocate a new data-array) */
            kbn->data = MEM_callocN(sizeof(float[3]) * vert_ranges.total_size(),
                                    "joined_shapekey");
            kbn->totelem = vert_ranges.total_size();
          }

          kb_map[i] = kbn;
        }

        /* remap relative index values */
        LISTBASE_FOREACH_INDEX (KeyBlock *, kb, &mesh->key->block, i) {
          /* sanity check, should always be true */
          if (LIKELY(kb->relative < mesh->key->totkey)) {
            kb_map[i]->relative = index_map[kb->relative];
          }
        }

        MEM_freeN(index_map);
        MEM_freeN(kb_map);
      }
    }
  }

  /* setup new data for destination mesh */
  CustomData vert_data;
  CustomData edge_data;
  CustomData face_data;
  CustomData corner_data;
  CustomData_reset(&vert_data);
  CustomData_reset(&edge_data);
  CustomData_reset(&corner_data);
  CustomData_reset(&face_data);

  MutableSpan<float3> vert_positions(
      (float3 *)CustomData_add_layer_named(
          &vert_data, CD_PROP_FLOAT3, CD_SET_DEFAULT, vert_ranges.total_size(), "position"),
      vert_ranges.total_size());
  MutableSpan<int2> edge(
      (int2 *)CustomData_add_layer_named(
          &edge_data, CD_PROP_INT32_2D, CD_CONSTRUCT, edge_ranges.total_size(), ".edge_verts"),
      edge_ranges.total_size());
  MutableSpan<int> corner_verts(
      (int *)CustomData_add_layer_named(
          &corner_data, CD_PROP_INT32, CD_CONSTRUCT, corner_ranges.total_size(), ".corner_vert"),
      corner_ranges.total_size());
  MutableSpan<int> corner_edges(
      (int *)CustomData_add_layer_named(
          &corner_data, CD_PROP_INT32, CD_CONSTRUCT, corner_ranges.total_size(), ".corner_edge"),
      corner_ranges.total_size());
  int *face_offsets = MEM_malloc_arrayN<int>(face_ranges.total_size() + 1, __func__);
  face_offsets[face_ranges.total_size()] = corner_ranges.total_size();

  /* Inverse transform for all selected meshes in this object,
   * See #object_join_exec for detailed comment on why the safe version is used. */
  float4x4 world_to_active_object;
  invert_m4_m4_safe_ortho(world_to_active_object.ptr(), active_object->object_to_world().ptr());

  for (const int i : objects_to_join.index_range()) {
    Object *ob_iter = objects_to_join[i];
    join_mesh_single(depsgraph,
                     bmain,
                     scene,
                     active_object,
                     ob_iter,
                     world_to_active_object,
                     vert_positions,
                     edge,
                     corner_verts,
                     corner_edges,
                     {face_offsets, face_ranges.total_size()},
                     &vert_data,
                     &edge_data,
                     &face_data,
                     &corner_data,
                     vert_ranges.total_size(),
                     edge_ranges.total_size(),
                     face_ranges.total_size(),
                     corner_ranges.total_size(),
                     key,
                     nkey,
                     materials,
                     vert_ranges[i],
                     edge_ranges[i],
                     face_ranges[i],
                     corner_ranges[i]);

    /* free base, now that data is merged */
    if (ob_iter != active_object) {
      object::base_free_and_unlink(bmain, scene, ob_iter);
    }
  }

  BKE_mesh_clear_geometry(dst_mesh);

  if (face_offsets) {
    dst_mesh->face_offset_indices = face_offsets;
    dst_mesh->runtime->face_offsets_sharing_info = implicit_sharing::info_for_mem_free(
        face_offsets);
  }

  dst_mesh->verts_num = vert_ranges.total_size();
  dst_mesh->edges_num = edge_ranges.total_size();
  dst_mesh->faces_num = face_ranges.total_size();
  dst_mesh->corners_num = corner_ranges.total_size();

  dst_mesh->vert_data = vert_data;
  dst_mesh->edge_data = edge_data;
  dst_mesh->corner_data = corner_data;
  dst_mesh->face_data = face_data;

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
    dst_mesh->mat = MEM_calloc_arrayN<Material *>(totcol, __func__);
    std::copy_n(materials.data(), totcol, dst_mesh->mat);
    active_object->mat = MEM_calloc_arrayN<Material *>(totcol, __func__);
    active_object->matbits = MEM_calloc_arrayN<char>(totcol, __func__);
  }

  active_object->totcol = dst_mesh->totcol = totcol;

  /* other mesh users */
  BKE_objects_materials_sync_length_all(bmain, (ID *)dst_mesh);

  /* Free temporary copy of destination shape-keys (if applicable). */
  if (nkey) {
    /* We can assume nobody is using that ID currently. */
    BKE_id_free_ex(bmain, nkey, LIB_ID_FREE_NO_UI_USER, false);
  }

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
