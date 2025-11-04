/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BKE_attribute.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "ED_transform_snap_object_context.hh"

#include "transform_snap_object.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Snap Object Data
 * \{ */

static const Mesh *get_mesh_ref(const Object *ob_eval)
{
  if (const Mesh *me = BKE_object_get_editmesh_eval_final(ob_eval)) {
    return me;
  }

  if (const Mesh *me = BKE_object_get_editmesh_eval_cage(ob_eval)) {
    return me;
  }

  return static_cast<const Mesh *>(ob_eval->data);
}

/**
 * Edit mesh snap cache.
 *
 * \note It's important there is only ever one object
 * per #SnapObjectContext that references this snap cache.
 *
 * Otherwise freed memory access may occur:
 * - While the lookup uses the original object data, change-detection uses the evaluated object.
 * - A change causes the previously cached mesh (#SnapCache_EditMesh::mesh) to be freed.
 * - The cached mesh may be referenced by a snap "hit", so freeing it may crash
 *   when that mesh is later later accessed.
 *
 * Furthermore, constantly re-creating cache is inefficient.
 *
 * Resolve by only using this cache for objects in edit-mode, instead objects with edit-mode data.
 * This works because only one objects-data may be in edit-mode at a time.
 * See: #148788.
 */
struct SnapCache_EditMesh : public SnapObjectContext::SnapCache {
  /* Mesh created from the edited mesh. */
  Mesh *mesh;

  /* Reference to pointers that change when the mesh is changed. It is used to detect updates. */
  const Mesh *mesh_ref;
  bke::MeshRuntime *runtime_ref;
  bke::EditMeshData *edit_data_ref;

  bool has_mesh_updated(const Mesh *mesh)
  {
    if (mesh != this->mesh_ref || mesh->runtime != this->runtime_ref ||
        mesh->runtime->edit_data.get() != this->edit_data_ref)
    {
      return true;
    }

    return false;
  }

  void clear()
  {
    if (this->mesh) {
      BKE_id_free(nullptr, this->mesh);
      this->mesh = nullptr;
    }
  }

  ~SnapCache_EditMesh() override
  {
    this->clear();
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("SnapCache_EditMesh")
};

static Mesh *create_mesh(SnapObjectContext *sctx,
                         const Object *ob_eval,
                         eSnapEditType /*edit_mode_type*/)
{
  Mesh *mesh = BKE_id_new_nomain<Mesh>(nullptr);
  const BMEditMesh *em = BKE_editmesh_from_object(const_cast<Object *>(ob_eval));
  BMesh *bm = em->bm;
  BM_mesh_bm_to_me_compact(*bm, *mesh, nullptr, false);

  bke::MutableAttributeAccessor attrs = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_vert = attrs.lookup_or_add_for_write_only_span<bool>(
      ".hide_vert", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<bool> hide_edge = attrs.lookup_or_add_for_write_only_span<bool>(
      ".hide_edge", bke::AttrDomain::Edge);
  bke::SpanAttributeWriter<bool> hide_poly = attrs.lookup_or_add_for_write_only_span<bool>(
      ".hide_poly", bke::AttrDomain::Face);

  /* Loop over all elements in parallel to choose which elements will participate in the snap.
   * Hidden elements are ignored for snapping. */
  const bool use_threading = (mesh->faces_num + mesh->edges_num) > 1024;
  threading::parallel_invoke(
      use_threading,
      [&]() {
        BMIter iter;
        BMVert *v;
        int i;
        BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
          if (sctx->callbacks.edit_mesh.test_vert_fn) {
            hide_vert.span[i] = !sctx->callbacks.edit_mesh.test_vert_fn(
                v, sctx->callbacks.edit_mesh.user_data);
          }
          else {
            hide_vert.span[i] = BM_elem_flag_test_bool(v, BM_ELEM_HIDDEN);
          }
        }
      },
      [&]() {
        BMIter iter;
        BMEdge *e;
        int i;
        BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
          if (sctx->callbacks.edit_mesh.test_edge_fn) {
            hide_edge.span[i] = !sctx->callbacks.edit_mesh.test_edge_fn(
                e, sctx->callbacks.edit_mesh.user_data);
          }
          else {
            hide_edge.span[i] = BM_elem_flag_test_bool(e, BM_ELEM_HIDDEN);
          }
        }
      },
      [&]() {
        BMIter iter;
        BMFace *f;
        int i;
        BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
          if (sctx->callbacks.edit_mesh.test_face_fn) {
            hide_poly.span[i] = !sctx->callbacks.edit_mesh.test_face_fn(
                f, sctx->callbacks.edit_mesh.user_data);
          }
          else {
            hide_poly.span[i] = BM_elem_flag_test_bool(f, BM_ELEM_HIDDEN);
          }
        }
      });

  hide_vert.finish();
  hide_edge.finish();
  hide_poly.finish();
  return mesh;
}

static SnapCache_EditMesh *snap_object_data_editmesh_get(SnapObjectContext *sctx,
                                                         const Object *ob_eval,
                                                         bool create)
{
  BLI_assert(ob_eval->mode & OB_MODE_EDIT);
  SnapCache_EditMesh *em_cache = nullptr;

  bool init = false;
  const Mesh *mesh_ref = (G.moving) ? /* WORKAROUND:
                                       * Avoid updating while transforming. Do not check if the
                                       * reference mesh has been updated. */
                             nullptr :
                             get_mesh_ref(ob_eval);

  if (std::unique_ptr<SnapObjectContext::SnapCache> *em_cache_p = sctx->editmesh_caches.lookup_ptr(
          ob_eval->runtime->data_orig))
  {
    em_cache = static_cast<SnapCache_EditMesh *>(em_cache_p->get());

    /* Check if the geometry has changed. */
    if (mesh_ref && em_cache->has_mesh_updated(mesh_ref)) {
      em_cache->clear();
      init = true;
    }
  }
  else if (create) {
    std::unique_ptr<SnapCache_EditMesh> em_cache_ptr = std::make_unique<SnapCache_EditMesh>();
    em_cache = em_cache_ptr.get();
    sctx->editmesh_caches.add_new(ob_eval->runtime->data_orig, std::move(em_cache_ptr));
    init = true;
  }

  if (init) {
    em_cache->mesh = create_mesh(sctx, ob_eval, sctx->runtime.params.edit_mode_type);
    if (mesh_ref) {
      em_cache->mesh_ref = mesh_ref;
      em_cache->runtime_ref = mesh_ref->runtime;
      em_cache->edit_data_ref = mesh_ref->runtime->edit_data.get();
    }
  }

  return em_cache;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Object Data
 * \{ */

static eSnapMode editmesh_snap_mode_supported(BMesh *bm)
{
  eSnapMode snap_mode_supported = SCE_SNAP_TO_NONE;
  if (bm->totface) {
    snap_mode_supported |= SCE_SNAP_TO_FACE | SCE_SNAP_INDIVIDUAL_NEAREST | SNAP_TO_EDGE_ELEMENTS |
                           SCE_SNAP_TO_POINT;
  }
  else if (bm->totedge) {
    snap_mode_supported |= SNAP_TO_EDGE_ELEMENTS | SCE_SNAP_TO_POINT;
  }
  else if (bm->totvert) {
    snap_mode_supported |= SCE_SNAP_TO_POINT;
  }
  return snap_mode_supported;
}

static SnapCache_EditMesh *editmesh_snapdata_init(SnapObjectContext *sctx,
                                                  const Object *ob_eval,
                                                  eSnapMode snap_to_flag)
{
  /* See code-comment on #SnapCache_EditMesh for why this is needed.  */
  if ((ob_eval->mode & OB_MODE_EDIT) == 0) {
    return nullptr;
  }

  const BMEditMesh *em = BKE_editmesh_from_object(const_cast<Object *>(ob_eval));
  if (em == nullptr) {
    return nullptr;
  }

  SnapCache_EditMesh *em_cache = snap_object_data_editmesh_get(sctx, ob_eval, false);
  if (em_cache != nullptr) {
    return em_cache;
  }

  eSnapMode snap_mode_used = snap_to_flag & editmesh_snap_mode_supported(em->bm);
  if (snap_mode_used == SCE_SNAP_TO_NONE) {
    return nullptr;
  }

  return snap_object_data_editmesh_get(sctx, ob_eval, true);
}

/** \} */

eSnapMode snap_object_editmesh(SnapObjectContext *sctx,
                               const Object *ob_eval,
                               const ID * /*id*/,
                               const float4x4 &obmat,
                               eSnapMode snap_to_flag,
                               bool /*use_hide*/)
{
  SnapCache_EditMesh *em_cache = editmesh_snapdata_init(sctx, ob_eval, snap_to_flag);
  if (em_cache && em_cache->mesh) {
    return snap_object_mesh(sctx, ob_eval, &em_cache->mesh->id, obmat, snap_to_flag, true, true);
  }
  return SCE_SNAP_TO_NONE;
}

}  // namespace blender::ed::transform
