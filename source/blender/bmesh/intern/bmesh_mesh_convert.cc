/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BM mesh conversion functions.
 *
 * \section bm_mesh_conv_shapekey Converting Shape Keys
 *
 * When converting to/from a Mesh/BMesh you can optionally pass a shape key to edit.
 * This has the effect of editing the shape key-block rather than the original mesh vertex coords
 * (although additional geometry is still allowed and uses fallback locations on converting).
 *
 * While this works for any mesh/bmesh this is made use of by entering and exiting edit-mode.
 *
 * There are comments in code but this should help explain the general
 * intention as to how this works converting from/to bmesh.
 * \subsection user_pov User Perspective
 *
 * - Editmode operations when a shape key-block is active edits only that key-block.
 * - The first Basis key-block always matches the Mesh verts.
 * - Changing vertex locations of _any_ Basis
 *   will apply offsets to those shape keys using this as their Basis.
 *
 * \subsection enter_editmode Entering EditMode - #BM_mesh_bm_from_me
 *
 * - The active key-block is used for BMesh vertex locations on entering edit-mode.
 *   So obviously the meshes vertex locations remain unchanged and the shape key
 *   itself is not being edited directly.
 *   Simply the #BMVert.co is a initialized from active shape key (when its set).
 * - All key-blocks are added as CustomData layers (read code for details).
 *
 * \subsection exit_editmode Exiting EditMode - #BM_mesh_bm_to_me
 *
 * This is where the most confusing code is! Won't attempt to document the details here,
 * for that read the code.
 * But basics are as follows.
 *
 * - Vertex locations (possibly modified from initial active key-block)
 *   are copied directly into the mesh position attribute.
 *   (special confusing note that these may be restored later, when editing the 'Basis', read on).
 * - if the 'Key' is relative, and the active key-block is the basis for ANY other key-blocks -
 *   get an array of offsets between the new vertex locations and the original shape key
 *   (before entering edit-mode), these offsets get applied later on to inactive key-blocks
 *   using the active one (which we are editing) as their Basis.
 *
 * Copying the locations back to the shape keys is quite confusing...
 * One main area of confusion is that when editing a 'Basis' key-block 'mesh->key->refkey'
 * The coords are written into the mesh, from the users perspective the Basis coords are written
 * into the mesh when exiting edit-mode.
 *
 * When _not_ editing the 'Basis', the original vertex locations
 * (stored in the mesh and unchanged during edit-mode), are copied back into the mesh.
 *
 * This has the effect from the users POV of leaving the mesh un-touched,
 * and only editing the active shape key-block.
 *
 * \subsection other_notes Other Notes
 *
 * Other details noted here which might not be so obvious:
 *
 * - The #CD_SHAPEKEY layer is only used in edit-mode,
 *   and the #Mesh.key is only used in object-mode.
 *   Although the #CD_SHAPEKEY custom-data layer is converted into #Key data-blocks for each
 *   undo-step while in edit-mode.
 * - The #CD_SHAPE_KEYINDEX layer is used to check if vertices existed when entering edit-mode.
 *   Values of the indices are only used for shape-keys when the #CD_SHAPEKEY layer can't be found,
 *   allowing coordinates from the #Key to be used to prevent data-loss.
 *   These indices are also used to maintain correct indices for hook modifiers and vertex parents.
 */

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_multires.hh"

#include "BKE_key.hh"
#include "BKE_main.hh"

#include "DEG_depsgraph_query.hh"

#include "bmesh.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"geom.bmesh.convert"};

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;
using blender::StringRef;
using blender::Vector;
using blender::bke::AttrDomain;

bool BM_attribute_stored_in_bmesh_builtin(const StringRef name)
{
  if (name.is_empty()) {
    return false;
  }
  if (name[0] == '.') {
    return ELEM(name,
                ".edge_verts",
                ".corner_vert",
                ".corner_edge",
                ".hide_vert",
                ".hide_edge",
                ".hide_poly",
                ".select_vert",
                ".select_edge",
                ".select_poly",
                ".uv_select_vert",
                ".uv_select_edge",
                ".uv_select_face");
  }
  return ELEM(name, "position", "uv_seam", "material_index", "sharp_face", "sharp_edge");
}

static BMFace *bm_face_create_from_mpoly(BMesh &bm,
                                         Span<int> face_verts,
                                         Span<int> face_edges,
                                         Span<BMVert *> vtable,
                                         Span<BMEdge *> etable)
{
  const int size = face_verts.size();
  Array<BMVert *, BM_DEFAULT_NGON_STACK_SIZE> verts(size);
  Array<BMEdge *, BM_DEFAULT_NGON_STACK_SIZE> edges(size);

  for (const int i : IndexRange(size)) {
    verts[i] = vtable[face_verts[i]];
    edges[i] = etable[face_edges[i]];
  }

  return BM_face_create(&bm, verts.data(), edges.data(), size, nullptr, BM_CREATE_SKIP_CD);
}

struct MeshToBMeshLayerInfo {
  eCustomDataType type;
  /** The layer's position in the BMesh element's data block. */
  int bmesh_offset;
  /** The mesh's #CustomDataLayer::data. When null, the BMesh block is set to its default value. */
  const void *mesh_data;
  /** The size of every custom data element. */
  size_t elem_size;
};

/**
 * Calculate the necessary information to copy every data layer from the Mesh to the BMesh.
 */
static Vector<MeshToBMeshLayerInfo> mesh_to_bm_copy_info_calc(const CustomData &mesh_data,
                                                              CustomData &bm_data)
{
  Vector<MeshToBMeshLayerInfo> infos;
  std::array<int, CD_NUMTYPES> per_type_index;
  per_type_index.fill(0);
  for (const int i : IndexRange(bm_data.totlayer)) {
    const CustomDataLayer &bm_layer = bm_data.layers[i];
    const eCustomDataType type = eCustomDataType(bm_layer.type);
    const int mesh_layer_index =
        bm_layer.name[0] == '\0' ?
            CustomData_get_layer_index_n(&mesh_data, type, per_type_index[type]) :
            CustomData_get_named_layer_index(&mesh_data, type, bm_layer.name);

    MeshToBMeshLayerInfo info{};
    info.type = type;
    info.bmesh_offset = bm_layer.offset;
    info.mesh_data = (mesh_layer_index == -1) ? nullptr : mesh_data.layers[mesh_layer_index].data;
    info.elem_size = CustomData_get_elem_size(&bm_layer);
    infos.append(info);

    per_type_index[type]++;
  }
  return infos;
}

static void mesh_attributes_copy_to_bmesh_block(CustomData &data,
                                                const Span<MeshToBMeshLayerInfo> copy_info,
                                                const int mesh_index,
                                                BMHeader &header)
{
  CustomData_bmesh_alloc_block(&data, &header.data);
  for (const MeshToBMeshLayerInfo &info : copy_info) {
    if (info.mesh_data) {
      CustomData_data_copy_value(info.type,
                                 POINTER_OFFSET(info.mesh_data, info.elem_size * mesh_index),
                                 POINTER_OFFSET(header.data, info.bmesh_offset));
    }
    else {
      CustomData_data_set_default_value(info.type, POINTER_OFFSET(header.data, info.bmesh_offset));
    }
  }
}

void BM_mesh_bm_from_me(BMesh *bm, const Mesh *mesh, const BMeshFromMeshParams *params)
{
  using namespace blender;
  if (!mesh) {
    /* Sanity check. */
    return;
  }
  const bool is_new = !(bm->totvert || (bm->vdata.totlayer || bm->edata.totlayer ||
                                        bm->pdata.totlayer || bm->ldata.totlayer));
  KeyBlock *actkey;
  float (*keyco)[3] = nullptr;
  CustomData_MeshMasks mask = CD_MASK_BMESH;
  CustomData_MeshMasks_update(&mask, &params->cd_mask_extra);

  CustomData mesh_vdata = CustomData_shallow_copy_remove_non_bmesh_attributes(&mesh->vert_data,
                                                                              mask.vmask);
  CustomData mesh_edata = CustomData_shallow_copy_remove_non_bmesh_attributes(&mesh->edge_data,
                                                                              mask.emask);
  CustomData mesh_pdata = CustomData_shallow_copy_remove_non_bmesh_attributes(&mesh->face_data,
                                                                              mask.pmask);
  CustomData mesh_ldata = CustomData_shallow_copy_remove_non_bmesh_attributes(&mesh->corner_data,
                                                                              mask.lmask);

  blender::Vector<std::string> temporary_layers_to_delete;

  for (const int layer_index :
       IndexRange(CustomData_number_of_layers(&mesh_ldata, CD_PROP_FLOAT2)))
  {
    char buffer[MAX_CUSTOMDATA_LAYER_NAME];
    {
      const StringRef name = BKE_uv_map_pin_name_get(
          CustomData_get_layer_name(&mesh_ldata, CD_PROP_FLOAT2, layer_index), buffer);
      if (CustomData_get_named_layer_index(&mesh_ldata, CD_PROP_BOOL, name) < 0) {
        CustomData_add_layer_named(
            &mesh_ldata, CD_PROP_BOOL, CD_SET_DEFAULT, mesh->corners_num, name);
        temporary_layers_to_delete.append(std::string(name));
      }
    }
  }

  BLI_SCOPED_DEFER([&]() {
    for (const std::string &name : temporary_layers_to_delete) {
      CustomData_free_layer_named(&mesh_ldata, name);
    }

    MEM_SAFE_FREE(mesh_vdata.layers);
    MEM_SAFE_FREE(mesh_edata.layers);
    MEM_SAFE_FREE(mesh_pdata.layers);
    MEM_SAFE_FREE(mesh_ldata.layers);
  });

  if (mesh->verts_num == 0) {
    if (is_new) {
      /* No verts? still copy custom-data layout. */
      CustomData_init_layout_from(&mesh_vdata, &bm->vdata, mask.vmask, CD_CONSTRUCT, 0);
      CustomData_init_layout_from(&mesh_edata, &bm->edata, mask.emask, CD_CONSTRUCT, 0);
      CustomData_init_layout_from(&mesh_pdata, &bm->pdata, mask.pmask, CD_CONSTRUCT, 0);
      CustomData_init_layout_from(&mesh_ldata, &bm->ldata, mask.lmask, CD_CONSTRUCT, 0);

      CustomData_bmesh_init_pool(&bm->vdata, mesh->verts_num, BM_VERT);
      CustomData_bmesh_init_pool(&bm->edata, mesh->edges_num, BM_EDGE);
      CustomData_bmesh_init_pool(&bm->ldata, mesh->corners_num, BM_LOOP);
      CustomData_bmesh_init_pool(&bm->pdata, mesh->faces_num, BM_FACE);
    }
    return;
  }

  blender::Span<blender::float3> vert_normals;
  if (params->calc_vert_normal) {
    vert_normals = mesh->vert_normals();
  }

  if (is_new) {
    CustomData_init_layout_from(&mesh_vdata, &bm->vdata, mask.vmask, CD_SET_DEFAULT, 0);
    CustomData_init_layout_from(&mesh_edata, &bm->edata, mask.emask, CD_SET_DEFAULT, 0);
    CustomData_init_layout_from(&mesh_pdata, &bm->pdata, mask.pmask, CD_SET_DEFAULT, 0);
    CustomData_init_layout_from(&mesh_ldata, &bm->ldata, mask.lmask, CD_SET_DEFAULT, 0);
  }
  else {
    CustomData_bmesh_merge_layout(
        &mesh_vdata, &bm->vdata, mask.vmask, CD_SET_DEFAULT, bm, BM_VERT);
    CustomData_bmesh_merge_layout(
        &mesh_edata, &bm->edata, mask.emask, CD_SET_DEFAULT, bm, BM_EDGE);
    CustomData_bmesh_merge_layout(
        &mesh_pdata, &bm->pdata, mask.pmask, CD_SET_DEFAULT, bm, BM_FACE);
    CustomData_bmesh_merge_layout(
        &mesh_ldata, &bm->ldata, mask.lmask, CD_SET_DEFAULT, bm, BM_LOOP);
  }

  /* -------------------------------------------------------------------- */
  /* Shape Key */
  int tot_shape_keys = 0;
  if (mesh->key != nullptr && DEG_is_original(mesh)) {
    /* Evaluated meshes can be topologically inconsistent with their shape keys.
     * Shape keys are also already integrated into the state of the evaluated
     * mesh, so considering them here would kind of apply them twice. */
    tot_shape_keys = BLI_listbase_count(&mesh->key->block);

    /* Original meshes must never contain a shape-key custom-data layers.
     *
     * This may happen if and object's mesh data is accidentally
     * set to the output from the modifier stack, causing it to be an "original" ID,
     * even though the data isn't fully compatible (hence this assert).
     *
     * This results in:
     * - The newly created #BMesh having twice the number of custom-data layers.
     * - When converting the #BMesh back to a regular mesh,
     *   At least one of the extra shape-key blocks will be created in #Mesh.key
     *   depending on the value of #CustomDataLayer.uid.
     *
     * We could support mixing both kinds of data if there is a compelling use-case for it.
     * At the moment it's simplest to assume all original meshes use the key-block and meshes
     * that are evaluated (through the modifier stack for example) use custom-data layers.
     */
    BLI_assert(!CustomData_has_layer(&mesh->vert_data, CD_SHAPEKEY));
  }
  if (is_new == false) {
    tot_shape_keys = min_ii(tot_shape_keys, CustomData_number_of_layers(&bm->vdata, CD_SHAPEKEY));
  }
  const float (**shape_key_table)[3] = tot_shape_keys ? (const float (**)[3])BLI_array_alloca(
                                                            shape_key_table, tot_shape_keys) :
                                                        nullptr;

  if ((params->active_shapekey != 0) && tot_shape_keys > 0) {
    actkey = static_cast<KeyBlock *>(BLI_findlink(&mesh->key->block, params->active_shapekey - 1));
  }
  else {
    actkey = nullptr;
  }

  if (is_new) {
    if (tot_shape_keys || params->add_key_index) {
      CustomData_add_layer(&bm->vdata, CD_SHAPE_KEYINDEX, CD_SET_DEFAULT, 0);
    }
  }

  if (tot_shape_keys) {
    if (is_new) {
      /* Check if we need to generate unique ids for the shape-keys.
       * This also exists in the file reading code, but is here for a sanity check. */
      if (!mesh->key->uidgen) {
        fprintf(stderr,
                "%s had to generate shape key uid's in a situation we shouldn't need to! "
                "(bmesh internal error)\n",
                __func__);

        mesh->key->uidgen = 1;
        LISTBASE_FOREACH (KeyBlock *, block, &mesh->key->block) {
          block->uid = mesh->key->uidgen++;
        }
      }
    }

    if (actkey && actkey->totelem == mesh->verts_num) {
      keyco = params->use_shapekey ? static_cast<float (*)[3]>(actkey->data) : nullptr;
      if (is_new) {
        bm->shapenr = params->active_shapekey;
      }
    }

    int i;
    KeyBlock *block;
    for (i = 0, block = static_cast<KeyBlock *>(mesh->key->block.first); i < tot_shape_keys;
         block = block->next, i++)
    {
      if (is_new) {
        CustomData_add_layer_named(&bm->vdata, CD_SHAPEKEY, CD_SET_DEFAULT, 0, block->name);
        int j = CustomData_get_layer_index_n(&bm->vdata, CD_SHAPEKEY, i);
        bm->vdata.layers[j].uid = block->uid;
      }
      shape_key_table[i] = static_cast<const float (*)[3]>(block->data);
    }
  }

  const Vector<MeshToBMeshLayerInfo> vert_info = mesh_to_bm_copy_info_calc(mesh_vdata, bm->vdata);
  const Vector<MeshToBMeshLayerInfo> edge_info = mesh_to_bm_copy_info_calc(mesh_edata, bm->edata);
  const Vector<MeshToBMeshLayerInfo> poly_info = mesh_to_bm_copy_info_calc(mesh_pdata, bm->pdata);
  const Vector<MeshToBMeshLayerInfo> loop_info = mesh_to_bm_copy_info_calc(mesh_ldata, bm->ldata);
  if (is_new) {
    CustomData_bmesh_init_pool(&bm->vdata, mesh->verts_num, BM_VERT);
    CustomData_bmesh_init_pool(&bm->edata, mesh->edges_num, BM_EDGE);
    CustomData_bmesh_init_pool(&bm->ldata, mesh->corners_num, BM_LOOP);
    CustomData_bmesh_init_pool(&bm->pdata, mesh->faces_num, BM_FACE);
  }

  /* Only copy these values over if the source mesh is flagged to be using them.
   * Even if `bm` has these layers, they may have been added from another mesh, when `!is_new`. */
  const int cd_shape_key_offset = tot_shape_keys ? CustomData_get_offset(&bm->vdata, CD_SHAPEKEY) :
                                                   -1;
  const int cd_shape_keyindex_offset = is_new && (tot_shape_keys || params->add_key_index) ?
                                           CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX) :
                                           -1;

  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan select_vert = *attributes.lookup<bool>(".select_vert", AttrDomain::Point);
  const VArraySpan select_edge = *attributes.lookup<bool>(".select_edge", AttrDomain::Edge);
  const VArraySpan select_poly = *attributes.lookup<bool>(".select_poly", AttrDomain::Face);
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", AttrDomain::Point);
  const VArraySpan hide_edge = *attributes.lookup<bool>(".hide_edge", AttrDomain::Edge);
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", AttrDomain::Face);
  const VArraySpan material_indices = *attributes.lookup<int>("material_index", AttrDomain::Face);
  const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
  const VArraySpan sharp_edges = *attributes.lookup<bool>("sharp_edge", AttrDomain::Edge);
  const VArraySpan uv_seams = *attributes.lookup<bool>("uv_seam", AttrDomain::Edge);

  const VArraySpan uv_select_vert = *attributes.lookup<bool>(".uv_select_vert",
                                                             AttrDomain::Corner);
  const VArraySpan uv_select_edge = *attributes.lookup<bool>(".uv_select_edge",
                                                             AttrDomain::Corner);
  const VArraySpan uv_select_face = *attributes.lookup<bool>(".uv_select_face", AttrDomain::Face);

  const bool need_uv_select = is_new && (!uv_select_vert.is_empty() &&
                                         !uv_select_edge.is_empty() && !uv_select_face.is_empty());

  const Span<float3> positions = mesh->vert_positions();
  Array<BMVert *> vtable(mesh->verts_num);
  for (const int i : positions.index_range()) {
    BMVert *v = vtable[i] = BM_vert_create(
        bm, keyco ? keyco[i] : positions[i], nullptr, BM_CREATE_SKIP_CD);
    BM_elem_index_set(v, i); /* set_ok */

    if (!hide_vert.is_empty() && hide_vert[i]) {
      BM_elem_flag_enable(v, BM_ELEM_HIDDEN);
    }
    if (!select_vert.is_empty() && select_vert[i]) {
      BM_vert_select_set(bm, v, true);
    }

    if (!vert_normals.is_empty()) {
      copy_v3_v3(v->no, vert_normals[i]);
    }

    mesh_attributes_copy_to_bmesh_block(bm->vdata, vert_info, i, v->head);

    /* Set shape key original index. */
    if (cd_shape_keyindex_offset != -1) {
      BM_ELEM_CD_SET_INT(v, cd_shape_keyindex_offset, i);
    }

    /* Set shape-key data. */
    if (tot_shape_keys) {
      float (*co_dst)[3] = (float (*)[3])BM_ELEM_CD_GET_VOID_P(v, cd_shape_key_offset);
      for (int j = 0; j < tot_shape_keys; j++, co_dst++) {
        copy_v3_v3(*co_dst, shape_key_table[j][i]);
      }
    }
  }
  if (is_new) {
    bm->elem_index_dirty &= ~BM_VERT; /* Added in order, clear dirty flag. */
  }

  const Span<blender::int2> edges = mesh->edges();
  Array<BMEdge *> etable(mesh->edges_num);
  for (const int i : edges.index_range()) {
    BMEdge *e = etable[i] = BM_edge_create(
        bm, vtable[edges[i][0]], vtable[edges[i][1]], nullptr, BM_CREATE_SKIP_CD);
    BM_elem_index_set(e, i); /* set_ok */

    e->head.hflag = 0;
    if (!uv_seams.is_empty() && uv_seams[i]) {
      BM_elem_flag_enable(e, BM_ELEM_SEAM);
    }
    if (!hide_edge.is_empty() && hide_edge[i]) {
      BM_elem_flag_enable(e, BM_ELEM_HIDDEN);
    }
    if (!select_edge.is_empty() && select_edge[i]) {
      BM_edge_select_set(bm, e, true);
    }
    if (!(!sharp_edges.is_empty() && sharp_edges[i])) {
      BM_elem_flag_enable(e, BM_ELEM_SMOOTH);
    }

    mesh_attributes_copy_to_bmesh_block(bm->edata, edge_info, i, e->head);
  }
  if (is_new) {
    bm->elem_index_dirty &= ~BM_EDGE; /* Added in order, clear dirty flag. */
  }

  const blender::OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int> corner_edges = mesh->corner_edges();

  /* Only needed for selection. */

  Array<BMFace *> ftable;
  if (mesh->mselect && mesh->totselect != 0) {
    ftable.reinitialize(mesh->faces_num);
  }

  int totloops = 0;
  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];
    BMFace *f = bm_face_create_from_mpoly(
        *bm, corner_verts.slice(face), corner_edges.slice(face), vtable, etable);
    if (!ftable.is_empty()) {
      ftable[i] = f;
    }

    if (UNLIKELY(f == nullptr)) {
      printf(
          "%s: Warning! Bad face in mesh"
          " \"%s\" at index %d!, skipping\n",
          __func__,
          mesh->id.name + 2,
          i);
      continue;
    }

    /* Don't use 'i' since we may have skipped the face. */
    BM_elem_index_set(f, bm->totface - 1); /* set_ok */

    /* Transfer flag. */
    if (!(!sharp_faces.is_empty() && sharp_faces[i])) {
      BM_elem_flag_enable(f, BM_ELEM_SMOOTH);
    }
    if (!hide_poly.is_empty() && hide_poly[i]) {
      BM_elem_flag_enable(f, BM_ELEM_HIDDEN);
    }
    if (!select_poly.is_empty() && select_poly[i]) {
      BM_face_select_set(bm, f, true);
    }

    f->mat_nr = material_indices.is_empty() ? 0 : material_indices[i];
    if (i == mesh->act_face) {
      bm->act_face = f;
    }

    int j = face.start();
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      /* Don't use 'j' since we may have skipped some faces, hence some loops. */
      BM_elem_index_set(l_iter, totloops++); /* set_ok */

      mesh_attributes_copy_to_bmesh_block(bm->ldata, loop_info, j, l_iter->head);

      if (need_uv_select) {
        if (uv_select_vert[j]) {
          BM_elem_flag_enable(l_iter, BM_ELEM_SELECT_UV);
        }
        if (uv_select_edge[j]) {
          BM_elem_flag_enable(l_iter, BM_ELEM_SELECT_UV_EDGE);
        }
      }

      j++;
    } while ((l_iter = l_iter->next) != l_first);

    mesh_attributes_copy_to_bmesh_block(bm->pdata, poly_info, i, f->head);

    if (need_uv_select) {
      if (uv_select_face[i]) {
        BM_elem_flag_enable(f, BM_ELEM_SELECT_UV);
      }
    }

    if (params->calc_face_normal) {
      BM_face_normal_update(f);
    }
  }
  if (is_new) {
    bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP); /* Added in order, clear dirty flag. */
  }

  bm->uv_select_sync_valid = (need_uv_select && (mesh->flag & ME_FLAG_UV_SELECT_SYNC_VALID)) != 0;

  /* -------------------------------------------------------------------- */
  /* MSelect clears the array elements (to avoid adding multiple times).
   *
   * Take care to keep this last and not use (v/e/ftable) after this.
   */

  if (mesh->mselect && mesh->totselect != 0) {
    for (const int i : IndexRange(mesh->totselect)) {
      const MSelect &msel = mesh->mselect[i];

      BMElem **ele_p;
      switch (msel.type) {
        case ME_VSEL:
          ele_p = (BMElem **)&vtable[msel.index];
          break;
        case ME_ESEL:
          ele_p = (BMElem **)&etable[msel.index];
          break;
        case ME_FSEL:
          ele_p = (BMElem **)&ftable[msel.index];
          break;
        default:
          continue;
      }

      if (*ele_p != nullptr) {
        BM_select_history_store_notest(bm, *ele_p);
        *ele_p = nullptr;
      }
    }
  }
  else {
    BM_select_history_clear(bm);
  }
}

/**
 * \brief BMesh -> Mesh
 */
static BMVert **bm_to_mesh_vertex_map(BMesh *bm, const int old_verts_num)
{
  const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);
  BMVert **vertMap = nullptr;
  BMVert *eve;
  int i = 0;
  BMIter iter;

  /* Caller needs to ensure this. */
  BLI_assert(old_verts_num > 0);

  vertMap = MEM_calloc_arrayN<BMVert *>(old_verts_num, "vertMap");
  if (cd_shape_keyindex_offset != -1) {
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
      const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
      if ((keyi != ORIGINDEX_NONE) && (keyi < old_verts_num) &&
          /* Not fool-proof, but chances are if we have many verts with the same index,
           * we will want to use the first one,
           * since the second is more likely to be a duplicate. */
          (vertMap[keyi] == nullptr))
      {
        vertMap[keyi] = eve;
      }
    }
  }
  else {
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (i < old_verts_num) {
        vertMap[i] = eve;
      }
      else {
        break;
      }
    }
  }

  return vertMap;
}

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh to Shape Key Conversion
 *
 * There are some details relating to using data from shape keys that need to be
 * considered carefully for shape key synchronization logic.
 *
 * Key Block Usage
 * ***************
 *
 * Key blocks (data in #Mesh.key must be used carefully).
 *
 * They can be used to query which key blocks are relative to the basis
 * since it's not possible to add/remove/reorder key blocks while in edit-mode.
 *
 * Key Block Coordinates
 * =====================
 *
 * Key blocks locations must *not* be used. This was done from v2.67 to 3.0,
 * causing bugs #35170 & #44415.
 *
 * Shape key synchronizing could work under the assumption that the key-block is
 * fixed-in-place when entering edit-mode allowing them to be used as a reference when exiting.
 * It often does work but isn't reliable since for example rendering may flush changes
 * from the edit-mesh to the key-block (there are a handful of other situations where
 * changes may be flushed, see #ED_editors_flush_edits and related functions).
 * When using undo, it's not known if the data in key-block is from the past or future,
 * so just don't use this data as it causes pain and suffering for users and developers alike.
 *
 * Instead, use the shape-key values stored in #CD_SHAPEKEY since they are reliably
 * based on the original locations, unless explicitly manipulated.
 * It's important to write the final shape-key values back to the #CD_SHAPEKEY so applying
 * the difference between the original-basis and the new coordinates isn't done multiple times.
 * Therefore #ED_editors_flush_edits and other flushing calls will update both the #Mesh.key
 * and the edit-mode #CD_SHAPEKEY custom-data layers.
 *
 * WARNING: There is an exception to the rule of ignoring coordinates in the destination:
 * that is when shape-key data in `bm` can't be found (which is itself an error/exception).
 * In this case our own rule is violated as the alternative is losing the shape-data entirely.
 *
 * Flushing Coordinates Back to the #BMesh
 * ---------------------------------------
 *
 * The edit-mesh may be flushed back to the #Mesh and #Key used to generate it.
 * When this is done, the new values are written back to the #BMesh's #CD_SHAPEKEY as well.
 * This is necessary when editing basis-shapes so the difference in shape keys
 * is not applied multiple times. If it were important to avoid it could be skipped while
 * exiting edit-mode (as the entire #BMesh is freed in that case), however it's just copying
 * back a `float[3]` so the work to check if it's necessary isn't worth the overhead.
 *
 * In general updating the #BMesh's #CD_SHAPEKEY makes shake-key logic easier to reason about
 * since it means flushing data back to the mesh has the same behavior as exiting and entering
 * edit-mode (a more common operation). Meaning there is one less corner-case to have to consider.
 *
 * Exceptional Cases
 * *****************
 *
 * There are some situations that should not happen in typical usage but are
 * still handled in this code, since failure to handle them could loose user-data.
 * These could be investigated further since if they never happen in practice,
 * we might consider removing them. However, the possibility of an mesh directly
 * being modified by Python or some other low level logic that changes key-blocks
 * means there is a potential this to happen so keeping code to these cases remain supported.
 *
 * - Custom Data & Mesh Key Block Synchronization.
 *   Key blocks in `mesh->key->block` should always have an associated
 *   #CD_SHAPEKEY layer in `bm->vdata`.
 *   If they don't there are two fall-backs for setting the location,
 *   - Use the value from the original shape key
 *     WARNING: this is technically incorrect! (see note on "Key Block Usage").
 *   - Use the current vertex location,
 *     Also not correct but it's better then having it zeroed for example.
 *
 * - Missing key-index layer.
 *   In this case the basis key won't apply its deltas to other keys and if a shape-key layer is
 *   missing, its coordinates will be initialized from the edit-mesh vertex locations instead of
 *   attempting to remap the shape-keys coordinates.
 *
 * \note These cases are considered abnormal and shouldn't occur in typical usage.
 * A warning is logged in this case to help troubleshooting bugs with shape-keys.
 * \{ */

/**
 * Returns custom-data shape-key index from a key-block or -1
 * \note could split this out into a more generic function.
 */
static int bm_to_mesh_shape_layer_index_from_kb(BMesh *bm, KeyBlock *currkey)
{
  int i;
  int j = 0;

  for (i = 0; i < bm->vdata.totlayer; i++) {
    if (bm->vdata.layers[i].type == CD_SHAPEKEY) {
      if (currkey->uid == bm->vdata.layers[i].uid) {
        return j;
      }
      j++;
    }
  }
  return -1;
}

/**
 * Update `key` with shape key data stored in `bm`.
 *
 * \param bm: The source BMesh.
 * \param key: The destination key.
 * \param positions: The destination vertex array (in some situations its coordinates are updated).
 * \param active_shapekey_to_mvert: When editing a non-basis shape key, the coordinates for the
 * basis are typically copied into the `positions` array since it makes sense for the meshes
 * vertex coordinates to match the "Basis" key.
 * When enabled, skip this step and copy #BMVert.co directly to the mesh position.
 * See #BMeshToMeshParams.active_shapekey_to_mvert doc-string.
 */
static void bm_to_mesh_shape(BMesh *bm,
                             Key *key,
                             MutableSpan<float3> positions,
                             const bool active_shapekey_to_mvert)
{
  KeyBlock *actkey = static_cast<KeyBlock *>(BLI_findlink(&key->block, bm->shapenr - 1));

  /* It's unlikely this ever remains false, check for correctness. */
  bool actkey_has_layer = false;

  /* Go through and find any shape-key custom-data layers
   * that might not have corresponding KeyBlocks, and add them if necessary. */
  for (int i = 0; i < bm->vdata.totlayer; i++) {
    if (bm->vdata.layers[i].type != CD_SHAPEKEY) {
      continue;
    }

    KeyBlock *currkey;
    for (currkey = (KeyBlock *)key->block.first; currkey; currkey = currkey->next) {
      if (currkey->uid == bm->vdata.layers[i].uid) {
        break;
      }
    }

    if (currkey) {
      if (currkey == actkey) {
        actkey_has_layer = true;
      }
    }
    else {
      currkey = BKE_keyblock_add(key, bm->vdata.layers[i].name);
      currkey->uid = bm->vdata.layers[i].uid;
    }
  }

  const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);
  BMIter iter;
  BMVert *eve;
  float (*ofs)[3] = nullptr;
  std::optional<Array<bool>> dependent;

  /* Editing the basis key updates others. */
  if ((key->type == KEY_RELATIVE) &&
      /* The shape-key coordinates used from entering edit-mode are used. */
      (actkey_has_layer == true) &&
      /* Original key-indices are only used to check the vertex existed when entering edit-mode. */
      (cd_shape_keyindex_offset != -1) &&
      /* Offsets are only needed if the current shape is a basis for others. */
      (dependent = BKE_keyblock_get_dependent_keys(key, bm->shapenr - 1)).has_value())
  {

    BLI_assert(actkey != nullptr); /* Assured by `actkey_has_layer` check. */
    const int actkey_uuid = bm_to_mesh_shape_layer_index_from_kb(bm, actkey);

    /* Since `actkey_has_layer == true`, this must never fail. */
    BLI_assert(actkey_uuid != -1);

    const int cd_shape_offset = CustomData_get_n_offset(&bm->vdata, CD_SHAPEKEY, actkey_uuid);

    ofs = static_cast<float (*)[3]>(MEM_mallocN(sizeof(float[3]) * bm->totvert, __func__));
    int i;
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
      const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
      /* Check the vertex existed when entering edit-mode (otherwise don't apply an offset). */
      if (keyi != ORIGINDEX_NONE) {
        float *co_orig = (float *)BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset);
        /* Could use 'eve->co' or the destination position, they're the same at this point. */
        sub_v3_v3v3(ofs[i], eve->co, co_orig);
      }
      else {
        /* If there are new vertices in the mesh, we can't propagate the offset
         * because it will only work for the existing vertices and not the new
         * ones, creating a mess when doing e.g. subdivide + translate. */
        MEM_freeN(ofs);
        ofs = nullptr;
        dependent.reset();
        break;
      }
    }
  }

  /* Without this, the real mesh coordinates (uneditable) as soon as you create the Basis shape.
   * while users might not notice since the shape-key is applied in the viewport,
   * exporters for example may still use the underlying coordinates, see: #30771 & #96135.
   *
   * Needed when editing any shape that isn't the (`key->refkey`), the vertices in mesh positions
   * currently have vertex coordinates set from the current-shape (initialized from #BMVert.co).
   * In this case it's important to overwrite these coordinates with the basis-keys coordinates. */
  bool update_vertex_coords_from_refkey = false;
  int cd_shape_offset_refkey = -1;
  if (active_shapekey_to_mvert == false) {
    if ((actkey != key->refkey) && (cd_shape_keyindex_offset != -1)) {
      const int refkey_uuid = bm_to_mesh_shape_layer_index_from_kb(bm, key->refkey);
      if (refkey_uuid != -1) {
        cd_shape_offset_refkey = CustomData_get_n_offset(&bm->vdata, CD_SHAPEKEY, refkey_uuid);
        if (cd_shape_offset_refkey != -1) {
          update_vertex_coords_from_refkey = true;
        }
      }
    }
  }

  int currkey_i;
  LISTBASE_FOREACH_INDEX (KeyBlock *, currkey, &key->block, currkey_i) {
    int keyi;
    float (*currkey_data)[3];

    const int currkey_uuid = bm_to_mesh_shape_layer_index_from_kb(bm, currkey);
    const int cd_shape_offset = (currkey_uuid == -1) ?
                                    -1 :
                                    CustomData_get_n_offset(&bm->vdata, CD_SHAPEKEY, currkey_uuid);

    /* Common case, the layer data is available, use it where possible. */
    if (cd_shape_offset != -1) {
      const bool apply_offset = (ofs != nullptr) && (currkey != actkey) && (*dependent)[currkey_i];

      if (currkey->data && (currkey->totelem == bm->totvert)) {
        /* Use memory in-place. */
      }
      else {
        currkey->data = MEM_reallocN(currkey->data, key->elemsize * bm->totvert);
        currkey->totelem = bm->totvert;
      }
      currkey_data = (float (*)[3])currkey->data;

      int i;
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
        float *co_orig = (float *)BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset);

        if (currkey == actkey) {
          copy_v3_v3(currkey_data[i], eve->co);

          if (update_vertex_coords_from_refkey) {
            BLI_assert(actkey != key->refkey);
            keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
            if (keyi != ORIGINDEX_NONE) {
              float *co_refkey = (float *)BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset_refkey);
              copy_v3_v3(positions[i], co_refkey);
            }
          }
        }
        else {
          copy_v3_v3(currkey_data[i], co_orig);
        }

        /* Propagate edited basis offsets to other shapes. */
        if (apply_offset) {
          add_v3_v3(currkey_data[i], ofs[i]);
        }

        /* Apply back new coordinates shape-keys that have offset into #BMesh.
         * Otherwise, in case we call again #BM_mesh_bm_to_me on same #BMesh,
         * we'll apply diff from previous call to #BM_mesh_bm_to_me,
         * to shape-key values from original creation of the #BMesh. See #50524. */
        copy_v3_v3(co_orig, currkey_data[i]);
      }
    }
    else {
      /* No original layer data, use fallback information. */
      if (currkey->data && (cd_shape_keyindex_offset != -1)) {
        CLOG_WARN(&LOG,
                  "Found shape-key but no CD_SHAPEKEY layers to read from, "
                  "using existing shake-key data where possible");
      }
      else {
        CLOG_WARN(&LOG,
                  "Found shape-key but no CD_SHAPEKEY layers to read from, "
                  "using basis shape-key data");
      }

      currkey_data = static_cast<float (*)[3]>(
          MEM_mallocN(key->elemsize * bm->totvert, "currkey->data"));

      int i;
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {

        if ((currkey->data != nullptr) && (cd_shape_keyindex_offset != -1) &&
            ((keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset)) != ORIGINDEX_NONE) &&
            (keyi < currkey->totelem))
        {
          /* Reconstruct keys via vertices original key indices.
           * WARNING(@ideasman42): `currkey->data` is known to be unreliable as the edit-mesh
           * coordinates may be flushed back to the shape-key when exporting or rendering.
           * This is a last resort! If this branch is running as part of regular usage
           * it can be considered a bug. */
          const float (*oldkey)[3] = static_cast<const float (*)[3]>(currkey->data);
          copy_v3_v3(currkey_data[i], oldkey[keyi]);
        }
        else {
          /* Fail! fill in with dummy value. */
          copy_v3_v3(currkey_data[i], eve->co);
        }
      }

      currkey->totelem = bm->totvert;
      if (currkey->data) {
        MEM_freeN(currkey->data);
      }
      currkey->data = currkey_data;
    }
  }

  MEM_SAFE_FREE(ofs);
}

/** \} */

static void assert_bmesh_has_no_mesh_only_attributes(const BMesh &bm)
{
  (void)bm; /* Unused in the release builds. */
  BLI_assert(!CustomData_has_layer_named(&bm.vdata, CD_PROP_FLOAT3, "position"));
  BLI_assert(!CustomData_has_layer_named(&bm.ldata, CD_PROP_FLOAT3, ".corner_vert"));
  BLI_assert(!CustomData_has_layer_named(&bm.ldata, CD_PROP_FLOAT3, ".corner_edge"));

  /* The "hide" attributes are stored as flags on #BMesh. */
  BLI_assert(!CustomData_has_layer_named(&bm.vdata, CD_PROP_BOOL, ".hide_vert"));
  BLI_assert(!CustomData_has_layer_named(&bm.edata, CD_PROP_BOOL, ".hide_edge"));
  BLI_assert(!CustomData_has_layer_named(&bm.pdata, CD_PROP_BOOL, ".hide_poly"));
  /* The "selection" attributes are stored as flags on #BMesh. */
  BLI_assert(!CustomData_has_layer_named(&bm.vdata, CD_PROP_BOOL, ".select_vert"));
  BLI_assert(!CustomData_has_layer_named(&bm.edata, CD_PROP_BOOL, ".select_edge"));
  BLI_assert(!CustomData_has_layer_named(&bm.pdata, CD_PROP_BOOL, ".select_poly"));
}

static void bmesh_to_mesh_calc_object_remap(Main &bmain,
                                            Mesh &mesh,
                                            BMesh &bm,
                                            const int old_totvert)
{
  BMVert **vertMap = nullptr;
  BMVert *eve;

  LISTBASE_FOREACH (Object *, ob, &bmain.objects) {
    if ((ob->parent) && (ob->parent->data == &mesh) && ELEM(ob->partype, PARVERT1, PARVERT3)) {

      if (vertMap == nullptr) {
        vertMap = bm_to_mesh_vertex_map(&bm, old_totvert);
      }

      if (ob->par1 < old_totvert) {
        eve = vertMap[ob->par1];
        if (eve) {
          ob->par1 = BM_elem_index_get(eve);
        }
      }
      if (ob->par2 < old_totvert) {
        eve = vertMap[ob->par2];
        if (eve) {
          ob->par2 = BM_elem_index_get(eve);
        }
      }
      if (ob->par3 < old_totvert) {
        eve = vertMap[ob->par3];
        if (eve) {
          ob->par3 = BM_elem_index_get(eve);
        }
      }
    }
    if (ob->data == &mesh) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Hook) {
          HookModifierData *hmd = (HookModifierData *)md;

          if (vertMap == nullptr) {
            vertMap = bm_to_mesh_vertex_map(&bm, old_totvert);
          }
          int i, j;
          for (i = j = 0; i < hmd->indexar_num; i++) {
            if (hmd->indexar[i] < old_totvert) {
              eve = vertMap[hmd->indexar[i]];

              if (eve) {
                hmd->indexar[j++] = BM_elem_index_get(eve);
              }
            }
            else {
              j++;
            }
          }

          hmd->indexar_num = j;
        }
      }
    }
  }

  if (vertMap) {
    MEM_freeN(vertMap);
  }
}

struct BMeshToMeshLayerInfo {
  eCustomDataType type;
  /** The layer's position in the BMesh element's data block. */
  int bmesh_offset;
  /** The mesh's #CustomDataLayer::data. When null, the BMesh block is set to its default value. */
  void *mesh_data;
  /** The size of every custom data element. */
  size_t elem_size;
};

/**
 * Calculate the necessary information to copy every data layer from the BMesh to the Mesh.
 */
static Vector<BMeshToMeshLayerInfo> bm_to_mesh_copy_info_calc(const CustomData &bm_data,
                                                              CustomData &mesh_data)
{
  Vector<BMeshToMeshLayerInfo> infos;
  std::array<int, CD_NUMTYPES> per_type_index;
  per_type_index.fill(0);
  for (const int i : IndexRange(mesh_data.totlayer)) {
    const CustomDataLayer &mesh_layer = mesh_data.layers[i];
    const eCustomDataType type = eCustomDataType(mesh_layer.type);
    const int bm_layer_index =
        mesh_layer.name[0] == '\0' ?
            CustomData_get_layer_index_n(&bm_data, type, per_type_index[type]) :
            CustomData_get_named_layer_index(&bm_data, type, mesh_layer.name);

    /* Skip layers that don't exist in `bm_data` or are explicitly set to not be
     * copied. The layers are either set separately or shouldn't exist on the mesh. */
    if (bm_layer_index == -1) {
      continue;
    }
    const CustomDataLayer &bm_layer = bm_data.layers[bm_layer_index];
    if (bm_layer.flag & CD_FLAG_NOCOPY) {
      continue;
    }

    BMeshToMeshLayerInfo info{};
    info.type = type;
    info.bmesh_offset = bm_layer.offset;
    info.mesh_data = mesh_layer.data;
    info.elem_size = CustomData_get_elem_size(&mesh_layer);
    infos.append(info);

    per_type_index[type]++;
  }
  return infos;
}

namespace blender {

static void bm_vert_table_build(BMesh &bm,
                                MutableSpan<const BMVert *> table,
                                bool &need_select_vert,
                                bool &need_hide_vert)
{
  char hflag = 0;
  BMIter iter;
  int i;
  BMVert *vert;
  BM_ITER_MESH_INDEX (vert, &iter, &bm, BM_VERTS_OF_MESH, i) {
    BM_elem_index_set(vert, i); /* set_inline */
    table[i] = vert;
    hflag |= vert->head.hflag;
  }
  need_select_vert = (hflag & BM_ELEM_SELECT) != 0;
  need_hide_vert = (hflag & BM_ELEM_HIDDEN) != 0;
}

static void bm_edge_table_build(BMesh &bm,
                                MutableSpan<const BMEdge *> table,
                                bool &need_select_edge,
                                bool &need_hide_edge,
                                bool &need_sharp_edge,
                                bool &need_uv_seams)
{
  char hflag = 0;
  BMIter iter;
  int i;
  BMEdge *edge;
  BM_ITER_MESH_INDEX (edge, &iter, &bm, BM_EDGES_OF_MESH, i) {
    BM_elem_index_set(edge, i); /* set_inline */
    table[i] = edge;
    hflag |= edge->head.hflag;
    need_sharp_edge |= (edge->head.hflag & BM_ELEM_SMOOTH) == 0;
  }
  need_select_edge = (hflag & BM_ELEM_SELECT) != 0;
  need_hide_edge = (hflag & BM_ELEM_HIDDEN) != 0;
  need_uv_seams = (hflag & BM_ELEM_SEAM) != 0;
}

/**
 * UV map vertex and edge selection, and UV pinning are all stored in separate boolean layers. On
 * #Mesh they are only meant to exist if they have a true value, but on #BMesh they currently
 * always exist. To avoid creating unnecessary mesh attributes, mark the UV helper layers with no
 * true values with the "no copy" flag.
 */
static void bm_face_loop_table_build(BMesh &bm,
                                     MutableSpan<const BMFace *> face_table,
                                     MutableSpan<const BMLoop *> loop_table,
                                     bool &need_select_poly,
                                     bool &need_hide_poly,
                                     bool &need_sharp_face,
                                     bool &need_material_index,
                                     Vector<int> &loop_layers_not_to_copy)
{
  const CustomData &ldata = bm.ldata;
  Vector<int> pin_layers;
  for (const int i : IndexRange(CustomData_number_of_layers(&ldata, CD_PROP_FLOAT2))) {
    char const *layer_name = CustomData_get_layer_name(&ldata, CD_PROP_FLOAT2, i);
    char sub_layer_name[MAX_CUSTOMDATA_LAYER_NAME];
    auto add_bool_layer = [&](Vector<int> &layers, const StringRef name) {
      const int layer_index = CustomData_get_named_layer_index(&ldata, CD_PROP_BOOL, name);
      if (layer_index != -1) {
        layers.append(layer_index);
      }
    };
    add_bool_layer(pin_layers, BKE_uv_map_pin_name_get(layer_name, sub_layer_name));
  }
  Array<int> pin_offsets(pin_layers.size());
  for (const int i : pin_layers.index_range()) {
    pin_offsets[i] = ldata.layers[pin_layers[i]].offset;
  }

  Array<bool> need_pin(pin_layers.size(), false);
  char hflag = 0;
  BMIter iter;
  int face_i = 0;
  int loop_i = 0;
  BMFace *face;
  BM_ITER_MESH_INDEX (face, &iter, &bm, BM_FACES_OF_MESH, face_i) {
    BM_elem_index_set(face, face_i); /* set_inline */
    face_table[face_i] = face;
    hflag |= face->head.hflag;
    need_sharp_face |= (face->head.hflag & BM_ELEM_SMOOTH) == 0;
    need_material_index |= face->mat_nr != 0;

    BMLoop *loop = BM_FACE_FIRST_LOOP(face);
    for ([[maybe_unused]] const int i : IndexRange(face->len)) {
      BM_elem_index_set(loop, loop_i); /* set_inline */
      loop_table[loop_i] = loop;
      for (const int i : pin_offsets.index_range()) {
        if (BM_ELEM_CD_GET_BOOL(loop, pin_offsets[i])) {
          need_pin[i] = true;
        }
      }
      loop = loop->next;
      loop_i++;
    }
  }
  need_select_poly = (hflag & BM_ELEM_SELECT) != 0;
  need_hide_poly = (hflag & BM_ELEM_HIDDEN) != 0;

  for (const int i : pin_layers.index_range()) {
    if (!need_pin[i]) {
      loop_layers_not_to_copy.append(pin_layers[i]);
    }
  }
}

static void bmesh_block_copy_to_mesh_attributes(const Span<BMeshToMeshLayerInfo> copy_info,
                                                const int mesh_index,
                                                const void *block)
{
  for (const BMeshToMeshLayerInfo &info : copy_info) {
    CustomData_data_copy_value(info.type,
                               POINTER_OFFSET(block, info.bmesh_offset),
                               POINTER_OFFSET(info.mesh_data, info.elem_size * mesh_index));
  }
}

static void bm_to_mesh_verts(const BMesh &bm,
                             const Span<const BMVert *> bm_verts,
                             Mesh &mesh,
                             MutableSpan<bool> select_vert,
                             MutableSpan<bool> hide_vert)
{
  CustomData_free_layer_named(&mesh.vert_data, "position");
  CustomData_add_layer_named(
      &mesh.vert_data, CD_PROP_FLOAT3, CD_CONSTRUCT, mesh.verts_num, "position");
  const Vector<BMeshToMeshLayerInfo> info = bm_to_mesh_copy_info_calc(bm.vdata, mesh.vert_data);
  MutableSpan<float3> dst_vert_positions = mesh.vert_positions_for_write();

  std::atomic<bool> any_loose_vert = false;
  threading::parallel_for(dst_vert_positions.index_range(), 1024, [&](const IndexRange range) {
    bool any_loose_vert_local = false;
    for (const int vert_i : range) {
      const BMVert &src_vert = *bm_verts[vert_i];
      copy_v3_v3(dst_vert_positions[vert_i], src_vert.co);
      bmesh_block_copy_to_mesh_attributes(info, vert_i, src_vert.head.data);
      any_loose_vert_local = any_loose_vert_local || src_vert.e == nullptr;
    }
    if (any_loose_vert_local) {
      any_loose_vert.store(true, std::memory_order_relaxed);
    }
    if (!select_vert.is_empty()) {
      for (const int vert_i : range) {
        select_vert[vert_i] = BM_elem_flag_test(bm_verts[vert_i], BM_ELEM_SELECT);
      }
    }
    if (!hide_vert.is_empty()) {
      for (const int vert_i : range) {
        hide_vert[vert_i] = BM_elem_flag_test(bm_verts[vert_i], BM_ELEM_HIDDEN);
      }
    }
  });

  if (!any_loose_vert) {
    mesh.tag_loose_verts_none();
  }
}

static void bm_to_mesh_edges(const BMesh &bm,
                             const Span<const BMEdge *> bm_edges,
                             Mesh &mesh,
                             MutableSpan<bool> select_edge,
                             MutableSpan<bool> hide_edge,
                             MutableSpan<bool> sharp_edge,
                             MutableSpan<bool> uv_seams)
{
  CustomData_free_layer_named(&mesh.edge_data, ".edge_verts");
  CustomData_add_layer_named(
      &mesh.edge_data, CD_PROP_INT32_2D, CD_CONSTRUCT, mesh.edges_num, ".edge_verts");
  const Vector<BMeshToMeshLayerInfo> info = bm_to_mesh_copy_info_calc(bm.edata, mesh.edge_data);
  MutableSpan<int2> dst_edges = mesh.edges_for_write();

  std::atomic<bool> any_loose_edge = false;
  threading::parallel_for(dst_edges.index_range(), 512, [&](const IndexRange range) {
    bool any_loose_edge_local = false;
    for (const int edge_i : range) {
      const BMEdge &src_edge = *bm_edges[edge_i];
      dst_edges[edge_i] = int2(BM_elem_index_get(src_edge.v1), BM_elem_index_get(src_edge.v2));
      bmesh_block_copy_to_mesh_attributes(info, edge_i, src_edge.head.data);
      any_loose_edge_local |= BM_edge_is_wire(&src_edge);
    }
    if (any_loose_edge_local) {
      any_loose_edge.store(true, std::memory_order_relaxed);
    }
    if (!select_edge.is_empty()) {
      for (const int edge_i : range) {
        select_edge[edge_i] = BM_elem_flag_test(bm_edges[edge_i], BM_ELEM_SELECT);
      }
    }
    if (!hide_edge.is_empty()) {
      for (const int edge_i : range) {
        hide_edge[edge_i] = BM_elem_flag_test(bm_edges[edge_i], BM_ELEM_HIDDEN);
      }
    }
    if (!sharp_edge.is_empty()) {
      for (const int edge_i : range) {
        sharp_edge[edge_i] = !BM_elem_flag_test(bm_edges[edge_i], BM_ELEM_SMOOTH);
      }
    }
    if (!uv_seams.is_empty()) {
      for (const int edge_i : range) {
        uv_seams[edge_i] = BM_elem_flag_test(bm_edges[edge_i], BM_ELEM_SEAM);
      }
    }
  });

  if (!any_loose_edge) {
    mesh.tag_loose_edges_none();
  }
}

static void bm_to_mesh_faces(const BMesh &bm,
                             const Span<const BMFace *> bm_faces,
                             Mesh &mesh,
                             MutableSpan<bool> select_poly,
                             MutableSpan<bool> hide_poly,
                             MutableSpan<bool> sharp_faces,
                             MutableSpan<bool> uv_select_face,
                             MutableSpan<int> material_indices)
{
  BKE_mesh_face_offsets_ensure_alloc(&mesh);
  const Vector<BMeshToMeshLayerInfo> info = bm_to_mesh_copy_info_calc(bm.pdata, mesh.face_data);
  MutableSpan<int> dst_face_offsets = mesh.face_offsets_for_write();
  threading::parallel_for(bm_faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face_i : range) {
      const BMFace &src_face = *bm_faces[face_i];
      dst_face_offsets[face_i] = BM_elem_index_get(BM_FACE_FIRST_LOOP(&src_face));
      bmesh_block_copy_to_mesh_attributes(info, face_i, src_face.head.data);
    }
    if (!select_poly.is_empty()) {
      for (const int face_i : range) {
        select_poly[face_i] = BM_elem_flag_test(bm_faces[face_i], BM_ELEM_SELECT);
      }
    }
    if (!hide_poly.is_empty()) {
      for (const int face_i : range) {
        hide_poly[face_i] = BM_elem_flag_test(bm_faces[face_i], BM_ELEM_HIDDEN);
      }
    }
    if (!material_indices.is_empty()) {
      for (const int face_i : range) {
        material_indices[face_i] = bm_faces[face_i]->mat_nr;
      }
    }
    if (!sharp_faces.is_empty()) {
      for (const int face_i : range) {
        sharp_faces[face_i] = !BM_elem_flag_test(bm_faces[face_i], BM_ELEM_SMOOTH);
      }
    }
    if (!uv_select_face.is_empty()) {
      for (const int face_i : range) {
        uv_select_face[face_i] = BM_elem_flag_test(bm_faces[face_i], BM_ELEM_SELECT_UV);
      }
    }
  });
}

static void bm_to_mesh_loops(const BMesh &bm,
                             const Span<const BMLoop *> bm_loops,
                             Mesh &mesh,
                             MutableSpan<bool> uv_select_vert,
                             MutableSpan<bool> uv_select_edge)
{
  CustomData_free_layer_named(&mesh.corner_data, ".corner_vert");
  CustomData_free_layer_named(&mesh.corner_data, ".corner_edge");
  CustomData_add_layer_named(
      &mesh.corner_data, CD_PROP_INT32, CD_CONSTRUCT, mesh.corners_num, ".corner_vert");
  CustomData_add_layer_named(
      &mesh.corner_data, CD_PROP_INT32, CD_CONSTRUCT, mesh.corners_num, ".corner_edge");
  const Vector<BMeshToMeshLayerInfo> info = bm_to_mesh_copy_info_calc(bm.ldata, mesh.corner_data);

  MutableSpan<int> dst_corner_verts = mesh.corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = mesh.corner_edges_for_write();

  const bool need_uv_select = !uv_select_vert.is_empty() && !uv_select_edge.is_empty();
  threading::parallel_for(dst_corner_verts.index_range(), 1024, [&](const IndexRange range) {
    for (const int loop_i : range) {
      const BMLoop &src_loop = *bm_loops[loop_i];
      dst_corner_verts[loop_i] = BM_elem_index_get(src_loop.v);
      dst_corner_edges[loop_i] = BM_elem_index_get(src_loop.e);
      bmesh_block_copy_to_mesh_attributes(info, loop_i, src_loop.head.data);
    }

    if (need_uv_select) {
      for (const int loop_i : range) {
        const BMLoop &src_loop = *bm_loops[loop_i];
        uv_select_vert[loop_i] = BM_elem_flag_test(&src_loop, BM_ELEM_SELECT_UV);
        uv_select_edge[loop_i] = BM_elem_flag_test(&src_loop, BM_ELEM_SELECT_UV_EDGE);
      }
    }
  });
}

}  // namespace blender

void BM_mesh_bm_to_me(Main *bmain, BMesh *bm, Mesh *mesh, const BMeshToMeshParams *params)
{
  using namespace blender;
  const int old_verts_num = mesh->verts_num;

  BKE_mesh_clear_geometry(mesh);

  mesh->verts_num = bm->totvert;
  mesh->edges_num = bm->totedge;
  mesh->totface_legacy = 0;
  mesh->corners_num = bm->totloop;
  mesh->faces_num = bm->totface;
  mesh->act_face = -1;

  /* Will have been cleared when clearing geometry. */
  const bool need_uv_select = CustomData_has_layer(&bm->ldata, CD_PROP_FLOAT2);
  if (need_uv_select & bm->uv_select_sync_valid) {
    mesh->flag |= ME_FLAG_UV_SELECT_SYNC_VALID;
  }

  bool need_select_vert = false;
  bool need_select_edge = false;
  bool need_select_poly = false;
  bool need_hide_vert = false;
  bool need_hide_edge = false;
  bool need_hide_poly = false;
  bool need_material_index = false;
  bool need_sharp_edge = false;
  bool need_sharp_face = false;
  bool need_uv_seams = false;
  Array<const BMVert *> vert_table;
  Array<const BMEdge *> edge_table;
  Array<const BMFace *> face_table;
  Array<const BMLoop *> loop_table;
  Vector<int> loop_layers_not_to_copy;
  threading::parallel_invoke(
      (mesh->faces_num + mesh->edges_num) > 1024,
      [&]() {
        vert_table.reinitialize(bm->totvert);
        bm_vert_table_build(*bm, vert_table, need_select_vert, need_hide_vert);
      },
      [&]() {
        edge_table.reinitialize(bm->totedge);
        bm_edge_table_build(
            *bm, edge_table, need_select_edge, need_hide_edge, need_sharp_edge, need_uv_seams);
      },
      [&]() {
        face_table.reinitialize(bm->totface);
        loop_table.reinitialize(bm->totloop);
        bm_face_loop_table_build(*bm,
                                 face_table,
                                 loop_table,
                                 need_select_poly,
                                 need_hide_poly,
                                 need_sharp_face,
                                 need_material_index,
                                 loop_layers_not_to_copy);
        for (const int i : loop_layers_not_to_copy) {
          bm->ldata.layers[i].flag |= CD_FLAG_NOCOPY;
        }
      });
  bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE | BM_FACE | BM_LOOP);

  {
    CustomData_MeshMasks mask = CD_MASK_MESH;
    CustomData_MeshMasks_update(&mask, &params->cd_mask_extra);
    CustomData_init_layout_from(
        &bm->vdata, &mesh->vert_data, mask.vmask, CD_CONSTRUCT, mesh->verts_num);
    CustomData_init_layout_from(
        &bm->edata, &mesh->edge_data, mask.emask, CD_CONSTRUCT, mesh->edges_num);
    CustomData_init_layout_from(
        &bm->ldata, &mesh->corner_data, mask.lmask, CD_CONSTRUCT, mesh->corners_num);
    CustomData_init_layout_from(
        &bm->pdata, &mesh->face_data, mask.pmask, CD_CONSTRUCT, mesh->faces_num);
  }

  /* Add optional mesh attributes before parallel iteration. */
  assert_bmesh_has_no_mesh_only_attributes(*bm);
  bke::MutableAttributeAccessor attrs = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_vert;
  bke::SpanAttributeWriter<bool> hide_vert;
  bke::SpanAttributeWriter<bool> select_edge;
  bke::SpanAttributeWriter<bool> hide_edge;
  bke::SpanAttributeWriter<bool> sharp_edge;
  bke::SpanAttributeWriter<bool> uv_seams;
  bke::SpanAttributeWriter<bool> select_poly;
  bke::SpanAttributeWriter<bool> hide_poly;
  bke::SpanAttributeWriter<bool> sharp_face;
  bke::SpanAttributeWriter<bool> uv_select_vert;
  bke::SpanAttributeWriter<bool> uv_select_edge;
  bke::SpanAttributeWriter<bool> uv_select_face;
  bke::SpanAttributeWriter<int> material_index;
  if (need_select_vert) {
    select_vert = attrs.lookup_or_add_for_write_only_span<bool>(".select_vert", AttrDomain::Point);
  }
  if (need_hide_vert) {
    hide_vert = attrs.lookup_or_add_for_write_only_span<bool>(".hide_vert", AttrDomain::Point);
  }
  if (need_select_edge) {
    select_edge = attrs.lookup_or_add_for_write_only_span<bool>(".select_edge", AttrDomain::Edge);
  }
  if (need_sharp_edge) {
    sharp_edge = attrs.lookup_or_add_for_write_only_span<bool>("sharp_edge", AttrDomain::Edge);
  }
  if (need_uv_seams) {
    uv_seams = attrs.lookup_or_add_for_write_only_span<bool>("uv_seam", AttrDomain::Edge);
  }
  if (need_hide_edge) {
    hide_edge = attrs.lookup_or_add_for_write_only_span<bool>(".hide_edge", AttrDomain::Edge);
  }
  if (need_select_poly) {
    select_poly = attrs.lookup_or_add_for_write_only_span<bool>(".select_poly", AttrDomain::Face);
  }
  if (need_hide_poly) {
    hide_poly = attrs.lookup_or_add_for_write_only_span<bool>(".hide_poly", AttrDomain::Face);
  }
  if (need_sharp_face) {
    sharp_face = attrs.lookup_or_add_for_write_only_span<bool>("sharp_face", AttrDomain::Face);
  }
  if (need_uv_select) {
    uv_select_vert = attrs.lookup_or_add_for_write_only_span<bool>(".uv_select_vert",
                                                                   AttrDomain::Corner);
    uv_select_edge = attrs.lookup_or_add_for_write_only_span<bool>(".uv_select_edge",
                                                                   AttrDomain::Corner);
    uv_select_face = attrs.lookup_or_add_for_write_only_span<bool>(".uv_select_face",
                                                                   AttrDomain::Face);
  }
  if (need_material_index) {
    material_index = attrs.lookup_or_add_for_write_only_span<int>("material_index",
                                                                  AttrDomain::Face);
  }

  /* Loop over all elements in parallel, copying attributes and building the Mesh topology. */
  threading::parallel_invoke(
      (mesh->faces_num + mesh->edges_num) > 1024,
      [&]() {
        bm_to_mesh_verts(*bm, vert_table, *mesh, select_vert.span, hide_vert.span);
        if (mesh->key) {
          bm_to_mesh_shape(
              bm, mesh->key, mesh->vert_positions_for_write(), params->active_shapekey_to_mvert);
        }
      },
      [&]() {
        bm_to_mesh_edges(*bm,
                         edge_table,
                         *mesh,
                         select_edge.span,
                         hide_edge.span,
                         sharp_edge.span,
                         uv_seams.span);
      },
      [&]() {
        bm_to_mesh_faces(*bm,
                         face_table,
                         *mesh,
                         select_poly.span,
                         hide_poly.span,
                         sharp_face.span,
                         uv_select_face.span,
                         material_index.span);
        if (bm->act_face) {
          mesh->act_face = BM_elem_index_get(bm->act_face);
        }
      },
      [&]() {
        bm_to_mesh_loops(*bm, loop_table, *mesh, uv_select_vert.span, uv_select_edge.span);
        /* Topology could be changed, ensure #CD_MDISPS are ok. */
        multires_topology_changed(mesh);
        for (const int i : loop_layers_not_to_copy) {
          bm->ldata.layers[i].flag &= ~CD_FLAG_NOCOPY;
        }
      },
      [&]() {
        /* Patch hook indices and vertex parents. */
        if (params->calc_object_remap && (old_verts_num > 0)) {
          bmesh_to_mesh_calc_object_remap(*bmain, *mesh, *bm, old_verts_num);
        }
      },
      [&]() {
        mesh->totselect = BLI_listbase_count(&(bm->selected));

        MEM_SAFE_FREE(mesh->mselect);
        if (mesh->totselect != 0) {
          mesh->mselect = MEM_malloc_arrayN<MSelect>(mesh->totselect, "Mesh selection history");
        }
        int i;
        LISTBASE_FOREACH_INDEX (BMEditSelection *, selected, &bm->selected, i) {
          if (selected->htype == BM_VERT) {
            mesh->mselect[i].type = ME_VSEL;
          }
          else if (selected->htype == BM_EDGE) {
            mesh->mselect[i].type = ME_ESEL;
          }
          else if (selected->htype == BM_FACE) {
            mesh->mselect[i].type = ME_FSEL;
          }

          mesh->mselect[i].index = BM_elem_index_get(selected->ele);
        }
      },
      [&]() {
        /* Run this even when shape keys aren't used since it may be used for hooks or vertex
         * parents. */
        if (params->update_shapekey_indices) {
          /* We have written a new shape key, if this mesh is _not_ going to be freed,
           * update the shape key indices to match the newly updated. */
          const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata,
                                                                     CD_SHAPE_KEYINDEX);
          if (cd_shape_keyindex_offset != -1) {
            BMIter iter;
            BMVert *vert;
            int i;
            BM_ITER_MESH_INDEX (vert, &iter, bm, BM_VERTS_OF_MESH, i) {
              BM_ELEM_CD_SET_INT(vert, cd_shape_keyindex_offset, i);
            }
          }
        }
      });

  select_vert.finish();
  hide_vert.finish();
  select_edge.finish();
  hide_edge.finish();
  sharp_edge.finish();
  uv_seams.finish();
  select_poly.finish();
  hide_poly.finish();
  sharp_face.finish();
  uv_select_vert.finish();
  uv_select_edge.finish();
  uv_select_face.finish();
  material_index.finish();
}

void BM_mesh_bm_to_me_compact(BMesh &bm,
                              Mesh &mesh,
                              const CustomData_MeshMasks *mask,
                              const bool add_mesh_attributes)
{
  /* NOTE: The function is called from multiple threads with the same input BMesh and different
   * mesh objects. */

  using namespace blender;
  /* Must be an empty mesh. */
  BLI_assert(mesh.verts_num == 0);
  /* Just in case, clear the derived geometry caches from the input mesh. */
  BKE_mesh_runtime_clear_geometry(&mesh);

  mesh.verts_num = bm.totvert;
  mesh.edges_num = bm.totedge;
  mesh.totface_legacy = 0;
  mesh.corners_num = bm.totloop;
  mesh.faces_num = bm.totface;

  /* Will have been cleared when clearing geometry. */
  const bool need_uv_select = CustomData_has_layer(&bm.ldata, CD_PROP_FLOAT2);
  if (need_uv_select && bm.uv_select_sync_valid) {
    mesh.flag |= ME_FLAG_UV_SELECT_SYNC_VALID;
  }

  mesh.runtime->deformed_only = true;

  const bool use_threading = (mesh.faces_num + mesh.edges_num) > 1024;

  /* In a first pass, update indices of BMesh elements and build tables for easy iteration later.
   * Also check if some optional mesh attributes should be added in the next step. Since each
   * domain has no effect on others, process the independent domains on separate threads. */
  bool need_select_vert = false;
  bool need_select_edge = false;
  bool need_select_poly = false;
  bool need_hide_vert = false;
  bool need_hide_edge = false;
  bool need_hide_poly = false;
  bool need_material_index = false;
  bool need_sharp_edge = false;
  bool need_sharp_face = false;
  bool need_uv_seams = false;

  Array<const BMVert *> vert_table;
  Array<const BMEdge *> edge_table;
  Array<const BMFace *> face_table;
  Array<const BMLoop *> loop_table;
  Vector<int> loop_layers_not_to_copy;
  threading::parallel_invoke(
      use_threading,
      [&]() {
        vert_table.reinitialize(bm.totvert);
        bm_vert_table_build(bm, vert_table, need_select_vert, need_hide_vert);
      },
      [&]() {
        edge_table.reinitialize(bm.totedge);
        bm_edge_table_build(
            bm, edge_table, need_select_edge, need_hide_edge, need_sharp_edge, need_uv_seams);
      },
      [&]() {
        face_table.reinitialize(bm.totface);
        loop_table.reinitialize(bm.totloop);
        bm_face_loop_table_build(bm,
                                 face_table,
                                 loop_table,
                                 need_select_poly,
                                 need_hide_poly,
                                 need_sharp_face,
                                 need_material_index,
                                 loop_layers_not_to_copy);
        for (const int i : loop_layers_not_to_copy) {
          bm.ldata.layers[i].flag |= CD_FLAG_NOCOPY;
        }
      });
  bm.elem_index_dirty &= ~(BM_VERT | BM_EDGE | BM_FACE | BM_LOOP);

  if (mask) {
    CustomData_merge_layout(&bm.vdata, &mesh.vert_data, mask->vmask, CD_CONSTRUCT, mesh.verts_num);
    CustomData_merge_layout(&bm.edata, &mesh.edge_data, mask->emask, CD_CONSTRUCT, mesh.edges_num);
    CustomData_merge_layout(
        &bm.ldata, &mesh.corner_data, mask->lmask, CD_CONSTRUCT, mesh.corners_num);
    CustomData_merge_layout(&bm.pdata, &mesh.face_data, mask->pmask, CD_CONSTRUCT, mesh.faces_num);
  }

  /* Add optional mesh attributes before parallel iteration. */
  assert_bmesh_has_no_mesh_only_attributes(bm);
  bke::SpanAttributeWriter<bool> select_vert;
  bke::SpanAttributeWriter<bool> hide_vert;
  bke::SpanAttributeWriter<bool> select_edge;
  bke::SpanAttributeWriter<bool> hide_edge;
  bke::SpanAttributeWriter<bool> sharp_edge;
  bke::SpanAttributeWriter<bool> uv_seams;
  bke::SpanAttributeWriter<bool> select_poly;
  bke::SpanAttributeWriter<bool> hide_poly;
  bke::SpanAttributeWriter<bool> sharp_face;
  bke::SpanAttributeWriter<bool> uv_select_vert;
  bke::SpanAttributeWriter<bool> uv_select_edge;
  bke::SpanAttributeWriter<bool> uv_select_face;
  bke::SpanAttributeWriter<int> material_index;

  if (add_mesh_attributes) {
    bke::MutableAttributeAccessor attrs = mesh.attributes_for_write();
    if (need_select_vert) {
      select_vert = attrs.lookup_or_add_for_write_only_span<bool>(".select_vert",
                                                                  AttrDomain::Point);
    }
    if (need_hide_vert) {
      hide_vert = attrs.lookup_or_add_for_write_only_span<bool>(".hide_vert", AttrDomain::Point);
    }
    if (need_select_edge) {
      select_edge = attrs.lookup_or_add_for_write_only_span<bool>(".select_edge",
                                                                  AttrDomain::Edge);
    }
    if (need_sharp_edge) {
      sharp_edge = attrs.lookup_or_add_for_write_only_span<bool>("sharp_edge", AttrDomain::Edge);
    }
    if (need_uv_seams) {
      uv_seams = attrs.lookup_or_add_for_write_only_span<bool>("uv_seam", AttrDomain::Edge);
    }
    if (need_hide_edge) {
      hide_edge = attrs.lookup_or_add_for_write_only_span<bool>(".hide_edge", AttrDomain::Edge);
    }
    if (need_select_poly) {
      select_poly = attrs.lookup_or_add_for_write_only_span<bool>(".select_poly",
                                                                  AttrDomain::Face);
    }
    if (need_hide_poly) {
      hide_poly = attrs.lookup_or_add_for_write_only_span<bool>(".hide_poly", AttrDomain::Face);
    }
    if (need_sharp_face) {
      sharp_face = attrs.lookup_or_add_for_write_only_span<bool>("sharp_face", AttrDomain::Face);
    }
    if (need_uv_select) {
      uv_select_vert = attrs.lookup_or_add_for_write_only_span<bool>(".uv_select_vert",
                                                                     AttrDomain::Corner);
      uv_select_edge = attrs.lookup_or_add_for_write_only_span<bool>(".uv_select_edge",
                                                                     AttrDomain::Corner);
      uv_select_face = attrs.lookup_or_add_for_write_only_span<bool>(".uv_select_face",
                                                                     AttrDomain::Face);
    }
    if (need_material_index) {
      material_index = attrs.lookup_or_add_for_write_only_span<int>("material_index",
                                                                    AttrDomain::Face);
    }
  }

  /* Loop over all elements in parallel, copying attributes and building the Mesh topology. */
  threading::parallel_invoke(
      use_threading,
      [&]() { bm_to_mesh_verts(bm, vert_table, mesh, select_vert.span, hide_vert.span); },
      [&]() {
        bm_to_mesh_edges(bm,
                         edge_table,
                         mesh,
                         select_edge.span,
                         hide_edge.span,
                         sharp_edge.span,
                         uv_seams.span);
      },
      [&]() {
        bm_to_mesh_faces(bm,
                         face_table,
                         mesh,
                         select_poly.span,
                         hide_poly.span,
                         sharp_face.span,
                         uv_select_face.span,
                         material_index.span);
        if (bm.act_face) {
          mesh.act_face = BM_elem_index_get(bm.act_face);
        }
      },
      [&]() {
        bm_to_mesh_loops(bm, loop_table, mesh, uv_select_vert.span, uv_select_edge.span);
        for (const int i : loop_layers_not_to_copy) {
          bm.ldata.layers[i].flag &= ~CD_FLAG_NOCOPY;
        }
      });

  if (add_mesh_attributes) {
    select_vert.finish();
    hide_vert.finish();
    select_edge.finish();
    hide_edge.finish();
    sharp_edge.finish();
    uv_seams.finish();
    select_poly.finish();
    hide_poly.finish();
    sharp_face.finish();
    uv_select_vert.finish();
    uv_select_edge.finish();
    uv_select_face.finish();
    material_index.finish();
  }
}

void BM_mesh_bm_to_me_for_eval(BMesh &bm, Mesh &mesh, const CustomData_MeshMasks *cd_mask_extra)
{
  /* Don't process shape-keys. We only feed them through the modifier stack as needed,
   * e.g. for applying modifiers or the like. */
  CustomData_MeshMasks mask = CD_MASK_DERIVEDMESH;
  if (cd_mask_extra != nullptr) {
    CustomData_MeshMasks_update(&mask, cd_mask_extra);
  }
  mask.vmask &= ~CD_MASK_SHAPEKEY;

  BM_mesh_bm_to_me_compact(bm, mesh, &mask, true);
}
