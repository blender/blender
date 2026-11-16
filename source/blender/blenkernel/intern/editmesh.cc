/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_bitmap.hh"
#include "BLI_listbase.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_vector_c.hh"

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

namespace blender {

EditMesh *BKE_editmesh_create(BMesh *bm)
{
  EditMesh *em = MEM_new<EditMesh>(__func__);
  em->bm = bm;
  return em;
}

EditMesh *BKE_editmesh_copy(EditMesh *em)
{
  EditMesh *em_copy = MEM_new<EditMesh>(__func__);
  *em_copy = *em;

  em_copy->bm = BM_mesh_copy(em->bm);

  /* The tessellation is NOT calculated on the copy here,
   * because currently all the callers of this function use
   * it to make a backup copy of the #EditMesh to restore
   * it in the case of errors in an operation. For performance reasons,
   * in that case it makes more sense to do the
   * tessellation only when/if that copy ends up getting used. */
  em_copy->looptris = {};

  /* Copy various settings. */
  em_copy->selectmode = em->selectmode;
  em_copy->mat_nr = em->mat_nr;

  return em_copy;
}

EditMesh *BKE_editmesh_from_object(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return (id_cast<Mesh *>(ob->data))->runtime->edit_mesh.get();
}

const BMesh *BKE_editmesh_bmesh_get(const Object *ob)
{
  /* After evaluation, an evaluated object's data is the final mesh, not a copy of the original. */
  BLI_assert(DEG_is_original(ob));
  const Mesh *mesh = id_cast<const Mesh *>(ob->data);
  return BKE_editmesh_bmesh_get(mesh);
}

namespace bke {

static bool is_vert_parent_of(const Object &ob, const Mesh &mesh)
{
  return ob.parent && ob.parent->data == &mesh.id && ELEM(ob.partype, PARVERT1, PARVERT3);
}

static bool has_vert_index_hook(const Object &ob, const Mesh &mesh)
{
  if (ob.data != &mesh.id) {
    return false;
  }
  for (const ModifierData &md : ob.modifiers) {
    if (md.type == eModifierType_Hook) {
      return true;
    }
  }
  return false;
}

Vector<Object *> mesh_find_objects_with_vert_references(Main &bmain, const Mesh &mesh)
{
  Vector<Object *> objects;
  for (Object &ob : bmain.objects) {
    if (is_vert_parent_of(ob, mesh) || has_vert_index_hook(ob, mesh)) {
      objects.append(&ob);
    }
  }
  return objects;
}

void mesh_remap_object_vert_indices(const Span<Object *> objects,
                                    const Mesh &mesh,
                                    const Span<int> old_to_new_verts)
{
  for (Object *object : objects) {
    Object &ob = *object;
    if (is_vert_parent_of(ob, mesh)) {
      for (int *par : {&ob.par1, &ob.par2, &ob.par3}) {
        /* Parents to removed vertices keep their index. */
        if (old_to_new_verts.index_range().contains(*par) && old_to_new_verts[*par] != -1) {
          *par = old_to_new_verts[*par];
        }
      }
    }
    if (!has_vert_index_hook(ob, mesh)) {
      continue;
    }
    for (ModifierData &md : ob.modifiers) {
      if (md.type != eModifierType_Hook) {
        continue;
      }
      HookModifierData &hmd = reinterpret_cast<HookModifierData &>(md);
      /* Hooks drop removed vertices. */
      int new_num = 0;
      for (const int i : IndexRange(hmd.indexar_num)) {
        const int old_vert = hmd.indexar[i];
        if (old_to_new_verts.index_range().contains(old_vert) && old_to_new_verts[old_vert] != -1)
        {
          hmd.indexar[new_num++] = old_to_new_verts[old_vert];
        }
      }
      hmd.indexar_num = new_num;
    }
  }
}

}  // namespace bke

const BMesh *BKE_editmesh_bmesh_get(const Mesh *mesh)
{
  const EditMesh *em = mesh->runtime->edit_mesh.get();
  if (!em) {
    /* The mesh is not in edit mode. */
    return nullptr;
  }
  if ((mesh->id.tag & ID_TAG_NO_MAIN) && !(mesh->id.tag & ID_TAG_COPIED_ON_EVAL)) {
    /* The mesh was created during evaluation and unless it wraps a #BMesh, it holds its own data
     * and only references the original #EditMesh to map elements back to the original edit mode
     * state. */
    if (mesh->runtime->wrapper_type != ME_WRAPPER_TYPE_BMESH) {
      return nullptr;
    }
  }
  return em->bm;
}

BMesh *BKE_editmesh_bmesh_get_for_write(Mesh *mesh)
{
  EditMesh *em = mesh->runtime->edit_mesh.get();
  if (!em) {
    return nullptr;
  }
  if ((mesh->id.tag & ID_TAG_NO_MAIN) && !(mesh->id.tag & ID_TAG_COPIED_ON_EVAL)) {
    /* See #BKE_editmesh_bmesh_get. */
    if (mesh->runtime->wrapper_type != ME_WRAPPER_TYPE_BMESH) {
      return nullptr;
    }
  }
  /* Only original meshes can edit the #BMesh. Evaluated meshes share the original's #EditMesh. */
  BLI_assert((mesh->id.tag & ID_TAG_NO_MAIN) == 0);
  return em->bm;
}

BMesh *BKE_editmesh_bmesh_get_for_write(Object *ob)
{
  Mesh *mesh = id_cast<Mesh *>(ob->data);
  return BKE_editmesh_bmesh_get_for_write(mesh);
}

BMesh *BKE_editmesh_bmesh_ensure(Mesh *mesh)
{
  return BKE_editmesh_bmesh_get_for_write(mesh);
}

BMesh *BKE_editmesh_bmesh_ensure(Object *object)
{
  return BKE_editmesh_bmesh_ensure(id_cast<Mesh *>(object->data));
}

bool BKE_editmesh_eval_orig_map_available(const Mesh &mesh_eval, const Mesh *mesh_orig)
{
  if (!mesh_orig) {
    return false;
  }
  if (&mesh_eval == mesh_orig) {
    return true;
  }
  if (mesh_eval.runtime->edit_mesh) {
    return mesh_eval.runtime->edit_mesh == mesh_orig->runtime->edit_mesh;
  }
  return false;
}

void BKE_editmesh_looptris_calc_ex(EditMesh *em,
                                   BMesh *bm,
                                   const BMeshCalcTessellation_Params *params)
{
  em->looptris.reinitialize(poly_to_tri_count(bm->totface, bm->totloop));
  BM_mesh_calc_tessellation_ex(bm, em->looptris, params);
}

void BKE_editmesh_looptris_calc(EditMesh *em, BMesh *bm)
{
  BMeshCalcTessellation_Params params{};
  params.face_normals = false;
  BKE_editmesh_looptris_calc_ex(em, bm, &params);
}

void BKE_editmesh_looptris_and_normals_calc(EditMesh *em, BMesh *bm)
{
  BMeshCalcTessellation_Params looptris_params{};
  looptris_params.face_normals = true;
  BKE_editmesh_looptris_calc_ex(em, bm, &looptris_params);
  BMeshNormalsUpdate_Params normals_params{};
  normals_params.face_normals = false;
  BM_mesh_normals_update_ex(bm, &normals_params);
}

void BKE_editmesh_looptris_calc_with_partial_ex(EditMesh *em,
                                                BMesh *bm,
                                                BMPartialUpdate *bmpinfo,
                                                const BMeshCalcTessellation_Params *params)
{
  BLI_assert(em->looptris.size() == poly_to_tri_count(bm->totface, bm->totloop));
  BLI_assert(!(bm->totface && em->looptris.is_empty()));

  BM_mesh_calc_tessellation_with_partial_ex(bm, em->looptris, bmpinfo, params);
}

void BKE_editmesh_looptris_calc_with_partial(EditMesh *em, BMesh *bm, BMPartialUpdate *bmpinfo)
{
  BMeshCalcTessellation_Params looptris_params{};
  looptris_params.face_normals = false;
  BKE_editmesh_looptris_calc_with_partial_ex(em, bm, bmpinfo, &looptris_params);
}

void BKE_editmesh_looptris_and_normals_calc_with_partial(EditMesh *em,
                                                         BMesh *bm,
                                                         BMPartialUpdate *bmpinfo)
{
  BMeshCalcTessellation_Params looptris_params{};
  looptris_params.face_normals = true;
  BKE_editmesh_looptris_calc_with_partial_ex(em, bm, bmpinfo, &looptris_params);
  BMeshNormalsUpdate_Params normals_params{};
  normals_params.face_normals = false;
  BM_mesh_normals_update_with_partial_ex(bm, bmpinfo, &normals_params);
}

void BKE_editmesh_free_data(EditMesh *em)
{
  em->looptris = {};

  if (BMesh *bm = em->bm) {
    BM_mesh_free(bm);
  }
}

struct CageUserData {
  int totvert;
  MutableSpan<float3> positions_cage;
  BLI_bitmap *visit_bitmap;
};

static void cage_mapped_verts_callback(void *user_data,
                                       int index,
                                       const float co[3],
                                       const float /*no*/[3])
{
  CageUserData *data = static_cast<CageUserData *>(user_data);

  if ((index >= 0 && index < data->totvert) && !BLI_BITMAP_TEST(data->visit_bitmap, index)) {
    BLI_BITMAP_ENABLE(data->visit_bitmap, index);
    copy_v3_v3(data->positions_cage[index], co);
  }
}

Array<float3> BKE_editmesh_vert_coords_alloc(Depsgraph *depsgraph,
                                             const BMesh *bm,
                                             Scene *scene,
                                             Object *ob)
{
  const Mesh *cage = bke::editbmesh_get_eval_cage(depsgraph, scene, ob, &CD_MASK_BAREMESH);
  Array<float3> positions_cage(bm->totvert);

  /* When initializing cage verts, we only want the first cage coordinate for each vertex,
   * so that e.g. mirror or array use original vertex coordinates and not mirrored or duplicate. */
  BLI_bitmap *visit_bitmap = BLI_BITMAP_NEW(bm->totvert, __func__);

  CageUserData data;
  data.totvert = bm->totvert;
  data.positions_cage = positions_cage;
  data.visit_bitmap = visit_bitmap;

  BKE_mesh_foreach_mapped_vert(cage, cage_mapped_verts_callback, &data, MESH_FOREACH_NOP);

  MEM_delete(visit_bitmap);

  return positions_cage;
}

Span<float3> BKE_editmesh_vert_coords_when_deformed(Depsgraph *depsgraph,
                                                    Scene *scene,
                                                    Object *ob,
                                                    Array<float3> &r_alloc)
{
  Object *object_orig = DEG_get_original(ob);
  const Object *object_eval = DEG_get_evaluated(depsgraph, ob);
  const Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(object_eval);
  const Mesh *mesh_cage = BKE_object_get_editmesh_eval_cage(ob);
  const BMesh *bm = BKE_editmesh_bmesh_get(object_orig);

  Span<float3> vert_positions;
  if (mesh_cage && mesh_cage->runtime->deformed_only) {
    BLI_assert(BKE_mesh_wrapper_vert_len(mesh_cage) == bm->totvert);
    /* Deformed, and we have deformed coords already. */
    vert_positions = BKE_mesh_wrapper_vert_coords(mesh_cage);
  }
  else if ((editmesh_eval_final != nullptr) &&
           (editmesh_eval_final->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH))
  {
    /* If this is an edit-mesh type, leave nullptr as we can use the vertex coords. */

    /* If this is not empty, it's value should be assigned to `vert_positions`
     * however the `mesh_cage` check above should handle this case. */
    BLI_assert(BKE_mesh_wrapper_vert_coords(mesh_cage).is_empty());
  }
  else {
    /* Constructive modifiers have been used, we need to allocate coordinates. */
    r_alloc = BKE_editmesh_vert_coords_alloc(depsgraph, bm, scene, ob);
    return r_alloc.as_span();
  }
  return vert_positions;
}

Array<float3> BKE_editmesh_vert_coords_alloc_orco(const BMesh *bm)
{
  return BM_mesh_vert_coords_alloc(bm);
}

void BKE_editmesh_lnorspace_update(BMesh *bm)
{
  BM_lnorspace_update(bm);
}

}  // namespace blender
