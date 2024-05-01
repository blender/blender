/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * `meshtools.cc`: no editmode (violated already :), mirror & join),
 * tools operating on meshes
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_virtual_array.hh"

#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "DRW_select_buffer.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

using blender::float3;
using blender::int2;
using blender::MutableSpan;
using blender::Span;

/* * ********************** no editmode!!! *********** */

/*********************** JOIN ***************************/

/* join selected meshes into the active mesh, context sensitive
 * return 0 if no join is made (error) and 1 if the join is done */

static void join_mesh_single(Depsgraph *depsgraph,
                             Main *bmain,
                             Scene *scene,
                             Object *ob_dst,
                             Object *ob_src,
                             const float imat[4][4],
                             float3 **vert_positions_pp,
                             blender::int2 **medge_pp,
                             int **corner_verts_pp,
                             int **corner_edges_pp,
                             int *all_face_offsets,
                             CustomData *vert_data,
                             CustomData *edge_data,
                             CustomData *ldata,
                             CustomData *face_data,
                             int totvert,
                             int totedge,
                             int totloop,
                             int faces_num,
                             Key *key,
                             Key *nkey,
                             Material **matar,
                             int *matmap,
                             int totcol,
                             int *vertofs,
                             int *edgeofs,
                             int *loopofs,
                             int *polyofs)
{
  int a, b;

  Mesh *mesh = static_cast<Mesh *>(ob_src->data);
  float3 *vert_positions = *vert_positions_pp;
  blender::int2 *edge = *medge_pp;
  int *corner_verts = *corner_verts_pp;
  int *corner_edges = *corner_edges_pp;

  if (mesh->verts_num) {
    /* standard data */
    CustomData_merge_layout(
        &mesh->vert_data, vert_data, CD_MASK_MESH.vmask, CD_SET_DEFAULT, totvert);
    CustomData_copy_data_named(&mesh->vert_data, vert_data, 0, *vertofs, mesh->verts_num);

    /* vertex groups */
    MDeformVert *dvert = (MDeformVert *)CustomData_get_for_write(
        vert_data, *vertofs, CD_MDEFORMVERT, totvert);
    const MDeformVert *dvert_src = (const MDeformVert *)CustomData_get_layer(&mesh->vert_data,
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
          dvert, mesh->verts_num, vgroup_index_map, vgroup_index_map_len);
      if (vgroup_index_map != nullptr) {
        MEM_freeN(vgroup_index_map);
      }
    }

    /* if this is the object we're merging into, no need to do anything */
    if (ob_src != ob_dst) {
      float cmat[4][4];

      /* Watch this: switch matrix multiplication order really goes wrong. */
      mul_m4_m4m4(cmat, imat, ob_src->object_to_world().ptr());

      /* transform vertex coordinates into new space */
      for (a = 0; a < mesh->verts_num; a++) {
        mul_m4_v3(cmat, vert_positions[a]);
      }

      /* For each shape-key in destination mesh:
       * - if there's a matching one, copy it across
       *   (will need to transform vertices into new space...).
       * - otherwise, just copy its own coordinates of mesh
       *   (no need to transform vertex coordinates into new space).
       */
      if (key) {
        /* if this mesh has any shape-keys, check first, otherwise just copy coordinates */
        LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
          /* get pointer to where to write data for this mesh in shape-key's data array */
          float(*cos)[3] = ((float(*)[3])kb->data) + *vertofs;

          /* Check if this mesh has such a shape-key. */
          KeyBlock *okb = mesh->key ? BKE_keyblock_find_name(mesh->key, kb->name) : nullptr;
          if (okb) {
            /* copy this mesh's shape-key to the destination shape-key
             * (need to transform first) */
            float(*ocos)[3] = static_cast<float(*)[3]>(okb->data);
            for (a = 0; a < mesh->verts_num; a++, cos++, ocos++) {
              copy_v3_v3(*cos, *ocos);
              mul_m4_v3(cmat, *cos);
            }
          }
          else {
            /* Copy this mesh's vertex coordinates to the destination shape-key. */
            for (a = 0; a < mesh->verts_num; a++, cos++) {
              copy_v3_v3(*cos, vert_positions[a]);
            }
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
          /* get pointer to where to write data for this mesh in shape-key's data array */
          float(*cos)[3] = ((float(*)[3])kb->data) + *vertofs;

          /* Check if this was one of the original shape-keys. */
          KeyBlock *okb = nkey ? BKE_keyblock_find_name(nkey, kb->name) : nullptr;
          if (okb) {
            /* copy this mesh's shape-key to the destination shape-key */
            float(*ocos)[3] = static_cast<float(*)[3]>(okb->data);
            for (a = 0; a < mesh->verts_num; a++, cos++, ocos++) {
              copy_v3_v3(*cos, *ocos);
            }
          }
          else {
            /* Copy base-coordinates to the destination shape-key. */
            for (a = 0; a < mesh->verts_num; a++, cos++) {
              copy_v3_v3(*cos, vert_positions[a]);
            }
          }
        }
      }
    }
  }

  if (mesh->edges_num) {
    CustomData_merge_layout(
        &mesh->edge_data, edge_data, CD_MASK_MESH.emask, CD_SET_DEFAULT, totedge);
    CustomData_copy_data_named(&mesh->edge_data, edge_data, 0, *edgeofs, mesh->edges_num);

    for (a = 0; a < mesh->edges_num; a++, edge++) {
      (*edge) += *vertofs;
    }
  }

  if (mesh->corners_num) {
    if (ob_src != ob_dst) {
      MultiresModifierData *mmd;

      multiresModifier_prepare_join(depsgraph, scene, ob_src, ob_dst);

      if ((mmd = get_multires_modifier(scene, ob_src, true))) {
        blender::ed::object::iter_other(
            bmain, ob_src, true, blender::ed::object::multires_update_totlevels, &mmd->totlvl);
      }
    }

    CustomData_merge_layout(
        &mesh->corner_data, ldata, CD_MASK_MESH.lmask, CD_SET_DEFAULT, totloop);
    CustomData_copy_data_named(&mesh->corner_data, ldata, 0, *loopofs, mesh->corners_num);

    for (a = 0; a < mesh->corners_num; a++) {
      corner_verts[a] += *vertofs;
      corner_edges[a] += *edgeofs;
    }
  }

  if (mesh->faces_num) {
    if (matmap) {
      /* make mapping for materials */
      for (a = 1; a <= ob_src->totcol; a++) {
        Material *ma = BKE_object_material_get(ob_src, a);

        for (b = 0; b < totcol; b++) {
          if (ma == matar[b]) {
            matmap[a - 1] = b;
            break;
          }
        }
      }
    }

    CustomData_merge_layout(
        &mesh->face_data, face_data, CD_MASK_MESH.pmask, CD_SET_DEFAULT, faces_num);
    CustomData_copy_data_named(&mesh->face_data, face_data, 0, *polyofs, mesh->faces_num);

    /* Apply matmap. In case we don't have material indices yet, create them if more than one
     * material is the result of joining. */
    int *material_indices = static_cast<int *>(CustomData_get_layer_named_for_write(
        face_data, CD_PROP_INT32, "material_index", faces_num));
    if (!material_indices && totcol > 1) {
      material_indices = (int *)CustomData_add_layer_named(
          face_data, CD_PROP_INT32, CD_SET_DEFAULT, faces_num, "material_index");
    }
    if (material_indices) {
      for (a = 0; a < mesh->faces_num; a++) {
        material_indices[a + *polyofs] = matmap ? matmap[material_indices[a + *polyofs]] : 0;
      }
    }

    const Span<int> src_face_offsets = mesh->face_offsets();
    int *face_offsets = all_face_offsets + *polyofs;
    for (const int i : blender::IndexRange(mesh->faces_num)) {
      face_offsets[i] = src_face_offsets[i] + *loopofs;
    }
  }

  /* these are used for relinking (cannot be set earlier, or else reattaching goes wrong) */
  *vertofs += mesh->verts_num;
  *vert_positions_pp += mesh->verts_num;
  *edgeofs += mesh->edges_num;
  *medge_pp += mesh->edges_num;
  *loopofs += mesh->corners_num;
  *corner_verts_pp += mesh->corners_num;
  *corner_edges_pp += mesh->corners_num;
  *polyofs += mesh->faces_num;
}

/* Face Sets IDs are a sparse sequence, so this function offsets all the IDs by face_set_offset and
 * updates face_set_offset with the maximum ID value. This way, when used in multiple meshes, all
 * of them will have different IDs for their Face Sets. */
static void mesh_join_offset_face_sets_ID(Mesh *mesh, int *face_set_offset)
{
  using namespace blender;
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
}

int ED_mesh_join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Material **matar = nullptr, *ma;
  Mesh *mesh;
  blender::int2 *edge = nullptr;
  Key *key, *nkey = nullptr;
  float imat[4][4];
  int a, b, totcol, totmat = 0, totedge = 0, totvert = 0;
  int totloop = 0, faces_num = 0, vertofs, *matmap = nullptr;
  int i, haskey = 0, edgeofs, loopofs, polyofs;
  bool ok = false, join_parent = false;
  CustomData vert_data, edge_data, ldata, face_data;

  if (ob->mode & OB_MODE_EDIT) {
    BKE_report(op->reports, RPT_WARNING, "Cannot join while in edit mode");
    return OPERATOR_CANCELLED;
  }

  /* ob is the object we are adding geometry to */
  if (!ob || ob->type != OB_MESH) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a mesh");
    return OPERATOR_CANCELLED;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* count & check */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter->type == OB_MESH) {
      mesh = static_cast<Mesh *>(ob_iter->data);

      totvert += mesh->verts_num;
      totedge += mesh->edges_num;
      totloop += mesh->corners_num;
      faces_num += mesh->faces_num;
      totmat += ob_iter->totcol;

      if (ob_iter == ob) {
        ok = true;
      }

      if ((ob->parent != nullptr) && (ob_iter == ob->parent)) {
        join_parent = true;
      }

      /* Check for shape-keys. */
      if (mesh->key) {
        haskey++;
      }
    }
  }
  CTX_DATA_END;

  /* Apply parent transform if the active object's parent was joined to it.
   * NOTE: This doesn't apply recursive parenting. */
  if (join_parent) {
    ob->parent = nullptr;
    BKE_object_apply_mat4_ex(ob, ob->object_to_world().ptr(), ob->parent, ob->parentinv, false);
  }

  /* that way the active object is always selected */
  if (ok == false) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a selected mesh");
    return OPERATOR_CANCELLED;
  }

  /* Only join meshes if there are verts to join,
   * there aren't too many, and we only had one mesh selected. */
  mesh = (Mesh *)ob->data;
  key = mesh->key;

  if (ELEM(totvert, 0, mesh->verts_num)) {
    BKE_report(op->reports, RPT_WARNING, "No mesh data to join");
    return OPERATOR_CANCELLED;
  }

  if (totvert > MESH_MAX_VERTS) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Joining results in %d vertices, limit is %ld",
                totvert,
                MESH_MAX_VERTS);
    return OPERATOR_CANCELLED;
  }

  /* new material indices and material array */
  if (totmat) {
    matar = static_cast<Material **>(MEM_callocN(sizeof(*matar) * totmat, __func__));
    matmap = static_cast<int *>(MEM_callocN(sizeof(*matmap) * totmat, __func__));
  }
  totcol = ob->totcol;

  /* Active object materials in new main array, is nicer start! */
  for (a = 0; a < ob->totcol; a++) {
    matar[a] = BKE_object_material_get(ob, a + 1);
    id_us_plus((ID *)matar[a]);
    /* increase id->us : will be lowered later */
  }

  /* - If destination mesh had shape-keys, move them somewhere safe, and set up placeholders
   *   with arrays that are large enough to hold shape-key data for all meshes.
   * - If destination mesh didn't have shape-keys, but we encountered some in the meshes we're
   *   joining, set up a new key-block and assign to the mesh.
   */
  if (key) {
    /* make a duplicate copy that will only be used here... (must remember to free it!) */
    nkey = (Key *)BKE_id_copy(bmain, &key->id);

    /* for all keys in old block, clear data-arrays */
    LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
      if (kb->data) {
        MEM_freeN(kb->data);
      }
      kb->data = MEM_callocN(sizeof(float[3]) * totvert, "join_shapekey");
      kb->totelem = totvert;
    }
  }
  else if (haskey) {
    /* add a new key-block and add to the mesh */
    key = mesh->key = BKE_key_add(bmain, (ID *)mesh);
    key->type = KEY_RELATIVE;
  }

  /* Update face_set_id_offset with the face set data in the active object first. This way the Face
   * Sets IDs in the active object are not the ones that are modified. */
  Mesh *mesh_active = BKE_mesh_from_object(ob);
  int face_set_id_offset = 0;
  mesh_join_offset_face_sets_ID(mesh_active, &face_set_id_offset);

  /* Copy materials, vertex-groups, face sets & face-maps across objects. */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    /* only act if a mesh, and not the one we're joining to */
    if ((ob != ob_iter) && (ob_iter->type == OB_MESH)) {
      mesh = static_cast<Mesh *>(ob_iter->data);

      /* Join this object's vertex groups to the base one's */
      LISTBASE_FOREACH (bDeformGroup *, dg, &mesh->vertex_group_names) {
        /* See if this group exists in the object (if it doesn't, add it to the end) */
        if (!BKE_object_defgroup_find_name(ob, dg->name)) {
          bDeformGroup *odg = static_cast<bDeformGroup *>(
              MEM_mallocN(sizeof(bDeformGroup), __func__));
          memcpy(odg, dg, sizeof(bDeformGroup));
          BLI_addtail(&mesh_active->vertex_group_names, odg);
        }
      }
      if (!BLI_listbase_is_empty(&mesh_active->vertex_group_names) &&
          mesh->vertex_group_active_index == 0)
      {
        mesh->vertex_group_active_index = 1;
      }

      mesh_join_offset_face_sets_ID(mesh, &face_set_id_offset);

      if (mesh->verts_num) {
        /* Add this object's materials to the base one's if they don't exist already
         * (but only if limits not exceeded yet) */
        if (totcol < MAXMAT) {
          for (a = 1; a <= ob_iter->totcol; a++) {
            ma = BKE_object_material_get(ob_iter, a);

            for (b = 0; b < totcol; b++) {
              if (ma == matar[b]) {
                break;
              }
            }
            if (b == totcol) {
              matar[b] = ma;
              if (ma) {
                id_us_plus(&ma->id);
              }
              totcol++;
            }
            if (totcol >= MAXMAT) {
              break;
            }
          }
        }

        /* If this mesh has shape-keys,
         * check if destination mesh already has matching entries too. */
        if (mesh->key && key) {
          /* for remapping KeyBlock.relative */
          int *index_map = static_cast<int *>(
              MEM_mallocN(sizeof(int) * mesh->key->totkey, __func__));
          KeyBlock **kb_map = static_cast<KeyBlock **>(
              MEM_mallocN(sizeof(KeyBlock *) * mesh->key->totkey, __func__));

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
              kbn->data = MEM_callocN(sizeof(float[3]) * totvert, "joined_shapekey");
              kbn->totelem = totvert;
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
  }
  CTX_DATA_END;

  /* setup new data for destination mesh */
  CustomData_reset(&vert_data);
  CustomData_reset(&edge_data);
  CustomData_reset(&ldata);
  CustomData_reset(&face_data);

  float3 *vert_positions = (float3 *)CustomData_add_layer_named(
      &vert_data, CD_PROP_FLOAT3, CD_SET_DEFAULT, totvert, "position");
  edge = (int2 *)CustomData_add_layer_named(
      &edge_data, CD_PROP_INT32_2D, CD_CONSTRUCT, totedge, ".edge_verts");
  int *corner_verts = (int *)CustomData_add_layer_named(
      &ldata, CD_PROP_INT32, CD_CONSTRUCT, totloop, ".corner_vert");
  int *corner_edges = (int *)CustomData_add_layer_named(
      &ldata, CD_PROP_INT32, CD_CONSTRUCT, totloop, ".corner_edge");
  int *face_offsets = static_cast<int *>(MEM_malloc_arrayN(faces_num + 1, sizeof(int), __func__));
  face_offsets[faces_num] = totloop;

  vertofs = 0;
  edgeofs = 0;
  loopofs = 0;
  polyofs = 0;

  /* Inverse transform for all selected meshes in this object,
   * See #object_join_exec for detailed comment on why the safe version is used. */
  invert_m4_m4_safe_ortho(imat, ob->object_to_world().ptr());

  /* Add back active mesh first.
   * This allows to keep things similar as they were, as much as possible
   * (i.e. data from active mesh will remain first ones in new result of the merge,
   * in same order for CD layers, etc). See also #50084.
   */
  join_mesh_single(depsgraph,
                   bmain,
                   scene,
                   ob,
                   ob,
                   imat,
                   &vert_positions,
                   &edge,
                   &corner_verts,
                   &corner_edges,
                   face_offsets,
                   &vert_data,
                   &edge_data,
                   &ldata,
                   &face_data,
                   totvert,
                   totedge,
                   totloop,
                   faces_num,
                   key,
                   nkey,
                   matar,
                   matmap,
                   totcol,
                   &vertofs,
                   &edgeofs,
                   &loopofs,
                   &polyofs);

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob) {
      continue;
    }
    /* only join if this is a mesh */
    if (ob_iter->type == OB_MESH) {
      join_mesh_single(depsgraph,
                       bmain,
                       scene,
                       ob,
                       ob_iter,
                       imat,
                       &vert_positions,
                       &edge,
                       &corner_verts,
                       &corner_edges,
                       face_offsets,
                       &vert_data,
                       &edge_data,
                       &ldata,
                       &face_data,
                       totvert,
                       totedge,
                       totloop,
                       faces_num,
                       key,
                       nkey,
                       matar,
                       matmap,
                       totcol,
                       &vertofs,
                       &edgeofs,
                       &loopofs,
                       &polyofs);

      /* free base, now that data is merged */
      if (ob_iter != ob) {
        blender::ed::object::base_free_and_unlink(bmain, scene, ob_iter);
      }
    }
  }
  CTX_DATA_END;

  /* return to mesh we're merging to */
  mesh = static_cast<Mesh *>(ob->data);

  BKE_mesh_clear_geometry(mesh);

  if (faces_num) {
    mesh->face_offset_indices = face_offsets;
    mesh->runtime->face_offsets_sharing_info = blender::implicit_sharing::info_for_mem_free(
        face_offsets);
  }

  mesh->verts_num = totvert;
  mesh->edges_num = totedge;
  mesh->corners_num = totloop;
  mesh->faces_num = faces_num;

  mesh->vert_data = vert_data;
  mesh->edge_data = edge_data;
  mesh->corner_data = ldata;
  mesh->face_data = face_data;

  /* old material array */
  for (a = 1; a <= ob->totcol; a++) {
    ma = ob->mat[a - 1];
    if (ma) {
      id_us_min(&ma->id);
    }
  }
  for (a = 1; a <= mesh->totcol; a++) {
    ma = mesh->mat[a - 1];
    if (ma) {
      id_us_min(&ma->id);
    }
  }
  MEM_SAFE_FREE(ob->mat);
  MEM_SAFE_FREE(ob->matbits);
  MEM_SAFE_FREE(mesh->mat);

  if (totcol) {
    mesh->mat = matar;
    ob->mat = static_cast<Material **>(MEM_callocN(sizeof(*ob->mat) * totcol, __func__));
    ob->matbits = static_cast<char *>(MEM_callocN(sizeof(*ob->matbits) * totcol, __func__));
    MEM_freeN(matmap);
  }

  ob->totcol = mesh->totcol = totcol;

  /* other mesh users */
  BKE_objects_materials_test_all(bmain, (ID *)mesh);

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
  BKE_object_free_derived_caches(ob);

  DEG_relations_tag_update(bmain); /* removed objects, need to rebuild dag */

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

/* -------------------------------------------------------------------- */
/** \name Join as Shapes
 *
 * Append selected meshes vertex locations as shapes of the active mesh.
 * \{ */

int ED_mesh_shapes_join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob_active = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mesh *mesh = (Mesh *)ob_active->data;
  Mesh *selme = nullptr;
  Mesh *me_deformed = nullptr;
  Key *key = mesh->key;
  KeyBlock *kb;
  bool ok = false, nonequal_verts = false;

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob_active) {
      continue;
    }

    if (ob_iter->type == OB_MESH) {
      selme = (Mesh *)ob_iter->data;

      if (selme->verts_num == mesh->verts_num) {
        ok = true;
      }
      else {
        nonequal_verts = true;
      }
    }
  }
  CTX_DATA_END;

  if (!ok) {
    if (nonequal_verts) {
      BKE_report(op->reports, RPT_WARNING, "Selected meshes must have equal numbers of vertices");
    }
    else {
      BKE_report(op->reports,
                 RPT_WARNING,
                 "No additional selected meshes with equal vertex count to join");
    }
    return OPERATOR_CANCELLED;
  }

  if (key == nullptr) {
    key = mesh->key = BKE_key_add(bmain, (ID *)mesh);
    key->type = KEY_RELATIVE;

    /* first key added, so it was the basis. initialize it with the existing mesh */
    kb = BKE_keyblock_add(key, nullptr);
    BKE_keyblock_convert_from_mesh(mesh, key, kb);
  }

  /* now ready to add new keys from selected meshes */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob_active) {
      continue;
    }

    if (ob_iter->type == OB_MESH) {
      selme = (Mesh *)ob_iter->data;

      if (selme->verts_num == mesh->verts_num) {
        Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
        Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob_iter);

        me_deformed = mesh_get_eval_deform(depsgraph, scene_eval, ob_eval, &CD_MASK_BAREMESH);

        if (!me_deformed) {
          continue;
        }

        kb = BKE_keyblock_add(key, ob_iter->id.name + 2);

        BKE_mesh_runtime_eval_to_meshkey(me_deformed, mesh, kb);
      }
    }
  }
  CTX_DATA_END;

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Topology Mirror API
 * \{ */

static MirrTopoStore_t mesh_topo_store = {nullptr, -1, -1, false};

BLI_INLINE void mesh_mirror_topo_table_get_meshes(Object *ob,
                                                  Mesh *mesh_eval,
                                                  Mesh **r_mesh_mirror,
                                                  BMEditMesh **r_em_mirror)
{
  Mesh *mesh_mirror = nullptr;
  BMEditMesh *em_mirror = nullptr;

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  if (mesh_eval != nullptr) {
    mesh_mirror = mesh_eval;
  }
  else if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    em_mirror = em;
  }
  else {
    mesh_mirror = mesh;
  }

  *r_mesh_mirror = mesh_mirror;
  *r_em_mirror = em_mirror;
}

void ED_mesh_mirror_topo_table_begin(Object *ob, Mesh *mesh_eval)
{
  Mesh *mesh_mirror;
  BMEditMesh *em_mirror;
  mesh_mirror_topo_table_get_meshes(ob, mesh_eval, &mesh_mirror, &em_mirror);

  ED_mesh_mirrtopo_init(em_mirror, mesh_mirror, &mesh_topo_store, false);
}

void ED_mesh_mirror_topo_table_end(Object * /*ob*/)
{
  /* TODO: store this in object/object-data (keep unused argument for now). */
  ED_mesh_mirrtopo_free(&mesh_topo_store);
}

/* Returns true on success. */
static bool ed_mesh_mirror_topo_table_update(Object *ob, Mesh *mesh_eval)
{
  Mesh *mesh_mirror;
  BMEditMesh *em_mirror;
  mesh_mirror_topo_table_get_meshes(ob, mesh_eval, &mesh_mirror, &em_mirror);

  if (ED_mesh_mirrtopo_recalc_check(em_mirror, mesh_mirror, &mesh_topo_store)) {
    ED_mesh_mirror_topo_table_begin(ob, mesh_eval);
  }
  return true;
}

/** \} */

static int mesh_get_x_mirror_vert_spatial(Object *ob, Mesh *mesh_eval, int index)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const Span<float3> positions = mesh_eval ? mesh_eval->vert_positions() : mesh->vert_positions();

  float vec[3];

  vec[0] = -positions[index][0];
  vec[1] = positions[index][1];
  vec[2] = positions[index][2];

  return ED_mesh_mirror_spatial_table_lookup(ob, nullptr, mesh_eval, vec);
}

static int mesh_get_x_mirror_vert_topo(Object *ob, Mesh *mesh, int index)
{
  if (!ed_mesh_mirror_topo_table_update(ob, mesh)) {
    return -1;
  }

  return mesh_topo_store.index_lookup[index];
}

int mesh_get_x_mirror_vert(Object *ob, Mesh *mesh_eval, int index, const bool use_topology)
{
  if (use_topology) {
    return mesh_get_x_mirror_vert_topo(ob, mesh_eval, index);
  }
  return mesh_get_x_mirror_vert_spatial(ob, mesh_eval, index);
}

static BMVert *editbmesh_get_x_mirror_vert_spatial(Object *ob, BMEditMesh *em, const float co[3])
{
  float vec[3];
  int i;

  /* ignore nan verts */
  if ((isfinite(co[0]) == false) || (isfinite(co[1]) == false) || (isfinite(co[2]) == false)) {
    return nullptr;
  }

  vec[0] = -co[0];
  vec[1] = co[1];
  vec[2] = co[2];

  i = ED_mesh_mirror_spatial_table_lookup(ob, em, nullptr, vec);
  if (i != -1) {
    return BM_vert_at_index(em->bm, i);
  }
  return nullptr;
}

static BMVert *editbmesh_get_x_mirror_vert_topo(Object *ob, BMEditMesh *em, BMVert *eve, int index)
{
  intptr_t poinval;
  if (!ed_mesh_mirror_topo_table_update(ob, nullptr)) {
    return nullptr;
  }

  if (index == -1) {
    BMIter iter;
    BMVert *v;

    index = 0;
    BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
      if (v == eve) {
        break;
      }
      index++;
    }

    if (index == em->bm->totvert) {
      return nullptr;
    }
  }

  poinval = mesh_topo_store.index_lookup[index];

  if (poinval != -1) {
    return (BMVert *)(poinval);
  }
  return nullptr;
}

BMVert *editbmesh_get_x_mirror_vert(
    Object *ob, BMEditMesh *em, BMVert *eve, const float co[3], int index, const bool use_topology)
{
  if (use_topology) {
    return editbmesh_get_x_mirror_vert_topo(ob, em, eve, index);
  }
  return editbmesh_get_x_mirror_vert_spatial(ob, em, co);
}

int ED_mesh_mirror_get_vert(Object *ob, int index)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  bool use_topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;
  int index_mirr;

  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    BMVert *eve, *eve_mirr;
    eve = BM_vert_at_index(em->bm, index);
    eve_mirr = editbmesh_get_x_mirror_vert(ob, em, eve, eve->co, index, use_topology);
    index_mirr = eve_mirr ? BM_elem_index_get(eve_mirr) : -1;
  }
  else {
    index_mirr = mesh_get_x_mirror_vert(ob, nullptr, index, use_topology);
  }

  return index_mirr;
}

#if 0

static float *editmesh_get_mirror_uv(
    BMEditMesh *em, int axis, float *uv, float *mirrCent, float *face_cent)
{
  float vec[2];
  float cent_vec[2];
  float cent[2];

  /* ignore nan verts */
  if (isnan(uv[0]) || !isfinite(uv[0]) || isnan(uv[1]) || !isfinite(uv[1])) {
    return nullptr;
  }

  if (axis) {
    vec[0] = uv[0];
    vec[1] = -((uv[1]) - mirrCent[1]) + mirrCent[1];

    cent_vec[0] = face_cent[0];
    cent_vec[1] = -((face_cent[1]) - mirrCent[1]) + mirrCent[1];
  }
  else {
    vec[0] = -((uv[0]) - mirrCent[0]) + mirrCent[0];
    vec[1] = uv[1];

    cent_vec[0] = -((face_cent[0]) - mirrCent[0]) + mirrCent[0];
    cent_vec[1] = face_cent[1];
  }

  /* TODO: Optimize. */
  {
    BMIter iter;
    BMFace *efa;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_face_uv_calc_center_median(efa, cd_loop_uv_offset, cent);

      if ((fabsf(cent[0] - cent_vec[0]) < 0.001f) && (fabsf(cent[1] - cent_vec[1]) < 0.001f)) {
        BMIter liter;
        BMLoop *l;

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          float *luv2 = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
          if ((fabsf(luv[0] - vec[0]) < 0.001f) && (fabsf(luv[1] - vec[1]) < 0.001f)) {
            return luv;
          }
        }
      }
    }
  }

  return nullptr;
}

#endif

static uint mirror_facehash(const void *ptr)
{
  const MFace *mf = static_cast<const MFace *>(ptr);
  uint v0, v1;

  if (mf->v4) {
    v0 = std::min({mf->v1, mf->v2, mf->v3, mf->v4});
    v1 = std::max({mf->v1, mf->v2, mf->v3, mf->v4});
  }
  else {
    v0 = std::min({mf->v1, mf->v2, mf->v3});
    v1 = std::min({mf->v1, mf->v2, mf->v3});
  }

  return ((v0 * 39) ^ (v1 * 31));
}

static int mirror_facerotation(const MFace *a, const MFace *b)
{
  if (b->v4) {
    if (a->v1 == b->v1 && a->v2 == b->v2 && a->v3 == b->v3 && a->v4 == b->v4) {
      return 0;
    }
    if (a->v4 == b->v1 && a->v1 == b->v2 && a->v2 == b->v3 && a->v3 == b->v4) {
      return 1;
    }
    if (a->v3 == b->v1 && a->v4 == b->v2 && a->v1 == b->v3 && a->v2 == b->v4) {
      return 2;
    }
    if (a->v2 == b->v1 && a->v3 == b->v2 && a->v4 == b->v3 && a->v1 == b->v4) {
      return 3;
    }
  }
  else {
    if (a->v1 == b->v1 && a->v2 == b->v2 && a->v3 == b->v3) {
      return 0;
    }
    if (a->v3 == b->v1 && a->v1 == b->v2 && a->v2 == b->v3) {
      return 1;
    }
    if (a->v2 == b->v1 && a->v3 == b->v2 && a->v1 == b->v3) {
      return 2;
    }
  }

  return -1;
}

static bool mirror_facecmp(const void *a, const void *b)
{
  return (mirror_facerotation((MFace *)a, (MFace *)b) == -1);
}

int *mesh_get_x_mirror_faces(Object *ob, BMEditMesh *em, Mesh *mesh_eval)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  MFace mirrormf;
  const MFace *mf, *hashmf;
  GHash *fhash;
  int *mirrorverts, *mirrorfaces;

  BLI_assert(em == nullptr); /* Does not work otherwise, currently... */

  const bool use_topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;
  const int totvert = mesh_eval ? mesh_eval->verts_num : mesh->verts_num;
  const int totface = mesh_eval ? mesh_eval->totface_legacy : mesh->totface_legacy;
  int a;

  mirrorverts = static_cast<int *>(MEM_callocN(sizeof(int) * totvert, "MirrorVerts"));
  mirrorfaces = static_cast<int *>(MEM_callocN(sizeof(int[2]) * totface, "MirrorFaces"));

  const Span<float3> vert_positions = mesh_eval ? mesh_eval->vert_positions() :
                                                  mesh->vert_positions();
  const MFace *mface = (const MFace *)CustomData_get_layer(
      &(mesh_eval ? mesh_eval : mesh)->fdata_legacy, CD_MFACE);

  ED_mesh_mirror_spatial_table_begin(ob, em, mesh_eval);

  for (const int i : vert_positions.index_range()) {
    mirrorverts[i] = mesh_get_x_mirror_vert(ob, mesh_eval, i, use_topology);
  }

  ED_mesh_mirror_spatial_table_end(ob);

  fhash = BLI_ghash_new_ex(
      mirror_facehash, mirror_facecmp, "mirror_facehash gh", mesh->totface_legacy);
  for (a = 0, mf = mface; a < totface; a++, mf++) {
    BLI_ghash_insert(fhash, (void *)mf, (void *)mf);
  }

  for (a = 0, mf = mface; a < totface; a++, mf++) {
    mirrormf.v1 = mirrorverts[mf->v3];
    mirrormf.v2 = mirrorverts[mf->v2];
    mirrormf.v3 = mirrorverts[mf->v1];
    mirrormf.v4 = (mf->v4) ? mirrorverts[mf->v4] : 0;

    /* make sure v4 is not 0 if a quad */
    if (mf->v4 && mirrormf.v4 == 0) {
      std::swap(mirrormf.v1, mirrormf.v3);
      std::swap(mirrormf.v2, mirrormf.v4);
    }

    hashmf = static_cast<const MFace *>(BLI_ghash_lookup(fhash, &mirrormf));
    if (hashmf) {
      mirrorfaces[a * 2] = hashmf - mface;
      mirrorfaces[a * 2 + 1] = mirror_facerotation(&mirrormf, hashmf);
    }
    else {
      mirrorfaces[a * 2] = -1;
    }
  }

  BLI_ghash_free(fhash, nullptr, nullptr);
  MEM_freeN(mirrorverts);

  return mirrorfaces;
}

/* Selection (vertex and face). */

bool ED_mesh_pick_face(bContext *C, Object *ob, const int mval[2], uint dist_px, uint *r_index)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  BLI_assert(mesh && GS(mesh->id.name) == ID_ME);

  if (!mesh || mesh->faces_num == 0) {
    return false;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  ED_view3d_select_id_validate(&vc);

  if (dist_px) {
    /* Sample rect to increase chances of selecting, so that when clicking
     * on an edge in the back-buffer, we can still select a face. */
    *r_index = DRW_select_buffer_find_nearest_to_point(
        vc.depsgraph, vc.region, vc.v3d, mval, 1, mesh->faces_num + 1, &dist_px);
  }
  else {
    /* sample only on the exact position */
    *r_index = DRW_select_buffer_sample_point(vc.depsgraph, vc.region, vc.v3d, mval);
  }

  if ((*r_index) == 0 || (*r_index) > uint(mesh->faces_num)) {
    return false;
  }

  (*r_index)--;

  return true;
}

static void ed_mesh_pick_face_vert__mpoly_find(
    /* context */
    ARegion *region,
    const float mval[2],
    /* mesh data (evaluated) */
    const blender::IndexRange face,
    const Span<float3> vert_positions,
    const int *corner_verts,
    /* return values */
    float *r_len_best,
    int *r_v_idx_best)
{
  for (int j = face.size(); j--;) {
    float sco[2];
    const int v_idx = corner_verts[face[j]];
    if (ED_view3d_project_float_object(region, vert_positions[v_idx], sco, V3D_PROJ_TEST_NOP) ==
        V3D_PROJ_RET_OK)
    {
      const float len_test = len_manhattan_v2v2(mval, sco);
      if (len_test < *r_len_best) {
        *r_len_best = len_test;
        *r_v_idx_best = v_idx;
      }
    }
  }
}
bool ED_mesh_pick_face_vert(
    bContext *C, Object *ob, const int mval[2], uint dist_px, uint *r_index)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  uint face_index;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  BLI_assert(mesh && GS(mesh->id.name) == ID_ME);

  if (ED_mesh_pick_face(C, ob, mval, dist_px, &face_index)) {
    const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
    if (!mesh_eval) {
      return false;
    }
    ARegion *region = CTX_wm_region(C);

    int v_idx_best = ORIGINDEX_NONE;

    /* find the vert closest to 'mval' */
    const float mval_f[2] = {float(mval[0]), float(mval[1])};
    float len_best = FLT_MAX;

    const Span<float3> vert_positions = mesh_eval->vert_positions();
    const blender::OffsetIndices faces = mesh_eval->faces();
    const Span<int> corner_verts = mesh_eval->corner_verts();

    const int *index_mp_to_orig = (const int *)CustomData_get_layer(&mesh_eval->face_data,
                                                                    CD_ORIGINDEX);

    /* tag all verts using this face */
    if (index_mp_to_orig) {
      for (const int i : faces.index_range()) {
        if (index_mp_to_orig[i] == face_index) {
          ed_mesh_pick_face_vert__mpoly_find(region,
                                             mval_f,
                                             faces[i],
                                             vert_positions,
                                             corner_verts.data(),
                                             &len_best,
                                             &v_idx_best);
        }
      }
    }
    else {
      if (face_index < faces.size()) {
        ed_mesh_pick_face_vert__mpoly_find(region,
                                           mval_f,
                                           faces[face_index],
                                           vert_positions,
                                           corner_verts.data(),
                                           &len_best,
                                           &v_idx_best);
      }
    }

    /* map 'dm -> mesh' r_index if possible */
    if (v_idx_best != ORIGINDEX_NONE) {
      const int *index_mv_to_orig = (const int *)CustomData_get_layer(&mesh_eval->vert_data,
                                                                      CD_ORIGINDEX);
      if (index_mv_to_orig) {
        v_idx_best = index_mv_to_orig[v_idx_best];
      }
    }

    if ((v_idx_best != ORIGINDEX_NONE) && (v_idx_best < mesh->verts_num)) {
      *r_index = v_idx_best;
      return true;
    }
  }

  return false;
}

/**
 * Vertex selection in object mode,
 * currently only weight paint uses this.
 *
 * \return boolean true == Found
 */
struct VertPickData {
  blender::VArraySpan<bool> hide_vert;
  const float *mval_f; /* [2] */
  ARegion *region;

  /* runtime */
  float len_best;
  int v_idx_best;
};

static void ed_mesh_pick_vert__mapFunc(void *user_data,
                                       int index,
                                       const float co[3],
                                       const float /*no*/[3])
{
  VertPickData *data = static_cast<VertPickData *>(user_data);
  if (!data->hide_vert.is_empty() && data->hide_vert[index]) {
    return;
  }
  float sco[2];
  if (ED_view3d_project_float_object(data->region, co, sco, V3D_PROJ_TEST_CLIP_DEFAULT) ==
      V3D_PROJ_RET_OK)
  {
    const float len = len_manhattan_v2v2(data->mval_f, sco);
    if (len < data->len_best) {
      data->len_best = len;
      data->v_idx_best = index;
    }
  }
}
bool ED_mesh_pick_vert(
    bContext *C, Object *ob, const int mval[2], uint dist_px, bool use_zbuf, uint *r_index)
{
  using namespace blender;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  BLI_assert(mesh && GS(mesh->id.name) == ID_ME);

  if (!mesh || mesh->verts_num == 0) {
    return false;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  ED_view3d_select_id_validate(&vc);

  if (use_zbuf) {
    if (dist_px > 0) {
      /* Sample rectangle to increase chances of selecting, so that when clicking
       * on an face in the back-buffer, we can still select a vert. */
      *r_index = DRW_select_buffer_find_nearest_to_point(
          vc.depsgraph, vc.region, vc.v3d, mval, 1, mesh->verts_num + 1, &dist_px);
    }
    else {
      /* sample only on the exact position */
      *r_index = DRW_select_buffer_sample_point(vc.depsgraph, vc.region, vc.v3d, mval);
    }

    if ((*r_index) == 0 || (*r_index) > uint(mesh->verts_num)) {
      return false;
    }

    (*r_index)--;
  }
  else {
    const Object *ob_eval = DEG_get_evaluated_object(vc.depsgraph, ob);
    const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
    ARegion *region = vc.region;
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

    /* find the vert closest to 'mval' */
    const float mval_f[2] = {float(mval[0]), float(mval[1])};

    VertPickData data{};

    ED_view3d_init_mats_rv3d(ob, rv3d);

    if (mesh_eval == nullptr) {
      return false;
    }

    const bke::AttributeAccessor attributes = mesh->attributes();

    /* setup data */
    data.region = region;
    data.mval_f = mval_f;
    data.len_best = FLT_MAX;
    data.v_idx_best = -1;
    data.hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

    BKE_mesh_foreach_mapped_vert(mesh_eval, ed_mesh_pick_vert__mapFunc, &data, MESH_FOREACH_NOP);

    if (data.v_idx_best == -1) {
      return false;
    }

    *r_index = data.v_idx_best;
  }

  return true;
}

MDeformVert *ED_mesh_active_dvert_get_em(Object *ob, BMVert **r_eve)
{
  if (ob->mode & OB_MODE_EDIT && ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    if (!BLI_listbase_is_empty(&mesh->vertex_group_names)) {
      BMesh *bm = mesh->runtime->edit_mesh->bm;
      const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);

      if (cd_dvert_offset != -1) {
        BMVert *eve = BM_mesh_active_vert_get(bm);

        if (eve) {
          if (r_eve) {
            *r_eve = eve;
          }
          return static_cast<MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
        }
      }
    }
  }

  if (r_eve) {
    *r_eve = nullptr;
  }
  return nullptr;
}

MDeformVert *ED_mesh_active_dvert_get_ob(Object *ob, int *r_index)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  int index = BKE_mesh_mselect_active_get(mesh, ME_VSEL);
  if (r_index) {
    *r_index = index;
  }
  if (index == -1 || mesh->deform_verts().is_empty()) {
    return nullptr;
  }
  MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
  return &dverts[index];
}

MDeformVert *ED_mesh_active_dvert_get_only(Object *ob)
{
  if (ob->type == OB_MESH) {
    if (ob->mode & OB_MODE_EDIT) {
      return ED_mesh_active_dvert_get_em(ob, nullptr);
    }
    return ED_mesh_active_dvert_get_ob(ob, nullptr);
  }
  return nullptr;
}

void EDBM_mesh_stats_multi(const Span<Object *> objects, int totelem[3], int totelem_sel[3])
{
  if (totelem) {
    totelem[0] = 0;
    totelem[1] = 0;
    totelem[2] = 0;
  }
  if (totelem_sel) {
    totelem_sel[0] = 0;
    totelem_sel[1] = 0;
    totelem_sel[2] = 0;
  }

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    if (totelem) {
      totelem[0] += bm->totvert;
      totelem[1] += bm->totedge;
      totelem[2] += bm->totface;
    }
    if (totelem_sel) {
      totelem_sel[0] += bm->totvertsel;
      totelem_sel[1] += bm->totedgesel;
      totelem_sel[2] += bm->totfacesel;
    }
  }
}

void EDBM_mesh_elem_index_ensure_multi(const Span<Object *> objects, const char htype)
{
  int elem_offset[4] = {0, 0, 0, 0};
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    BM_mesh_elem_index_ensure_ex(bm, htype, elem_offset);
  }
}
