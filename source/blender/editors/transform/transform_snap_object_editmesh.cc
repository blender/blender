/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_index_mask.hh"

#include "BKE_attribute.hh"
#include "BKE_bvhutils.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "GEO_mesh_selection.hh"

#include "DEG_depsgraph_query.hh"

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

  return id_cast<const Mesh *>(ob_eval->data);
}

struct SnapTargetElems {
  IndexMaskMemory memory;
  IndexMask faces;
  IndexMask loose_edges;
  IndexMask loose_verts;
};

static void calc_target_elems(const Mesh &mesh,
                              const SnapEditMeshTarget target,
                              SnapTargetElems &r_elems)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const Span<int2> edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();
  IndexMaskMemory &memory = r_elems.memory;

  IndexMask target_verts = IndexMask::from_bools_inverse(
      *attributes.lookup_or_default(".hide_vert", bke::AttrDomain::Point, false), memory);
  IndexMask target_edges = IndexMask::from_bools_inverse(
      *attributes.lookup_or_default(".hide_edge", bke::AttrDomain::Edge, false), memory);
  IndexMask target_faces = IndexMask::from_bools_inverse(
      *attributes.lookup_or_default(".hide_poly", bke::AttrDomain::Face, false), memory);

  if (target == SnapEditMeshTarget::VisibleUnselected) {
    const VArray<bool> select_vert = *attributes.lookup_or_default(
        ".select_vert", bke::AttrDomain::Point, false);

    target_verts = IndexMask::from_bools_inverse(target_verts, select_vert, memory);
    target_edges = IndexMask::from_bools_inverse(
        target_edges,
        *attributes.lookup_or_default(".select_edge", bke::AttrDomain::Edge, false),
        memory);
    target_faces = IndexMask::from_bools_inverse(
        target_faces,
        *attributes.lookup_or_default(".select_poly", bke::AttrDomain::Face, false),
        memory);

    /* Also remove edges and faces that use a selected vertex. */
    target_edges = IndexMask::from_predicate(target_edges, memory, [&](const int edge) {
      return !select_vert[edges[edge][0]] && !select_vert[edges[edge][1]];
    });
    const Span<int> corner_verts = mesh.corner_verts();
    target_faces = IndexMask::from_predicate(target_faces, memory, [&](const int face) {
      return std::ranges::none_of(corner_verts.slice(faces[face]),
                                  [&](const int vert) { return select_vert[vert]; });
    });
  }

  r_elems.faces = target_faces;

  /* Vertices and edges are only snapped to on their own when nothing of a higher dimension that
   * is snapped to covers them already. */
  r_elems.loose_verts = IndexMask::from_difference(
      target_verts,
      geometry::vert_selection_from_edge(edges, target_edges, mesh.verts_num, memory),
      memory);
  r_elems.loose_edges = IndexMask::from_difference(
      target_edges,
      geometry::edge_selection_from_face(
          faces, target_faces, mesh.corner_edges(), mesh.edges_num, memory),
      memory);
}

/**
 * The BVH trees of the elements of a mesh converted from edit-mode that can be snapped to.
 * This data is specialized enough that it isn't cached on the mesh itself.
 */
class SnapTargetTreesEditMesh : public SnapTargetTrees {
  const Mesh *mesh_ = nullptr;
  SnapEditMeshTarget target_ = SnapEditMeshTarget::Visible;

  std::optional<SnapTargetElems> elems_;
  std::optional<bke::BVHTreeFromMesh> corner_tris_;
  std::optional<bke::BVHTreeFromMesh> loose_edges_;
  std::optional<bke::BVHTreeFromMesh> loose_verts_;

 public:
  /** Discard what was built before when the mesh or the set of snappable elements changed. */
  void set_source(const Mesh &mesh, const SnapEditMeshTarget target)
  {
    if (mesh_ == &mesh && target_ == target) {
      return;
    }
    this->clear();
    mesh_ = &mesh;
    target_ = target;
  }

  /**
   * \note Must be called before the mesh is freed. A new mesh may be allocated at the same
   * address, in which case #set_source wouldn't notice the change.
   */
  void clear()
  {
    mesh_ = nullptr;
    elems_.reset();
    corner_tris_.reset();
    loose_edges_.reset();
    loose_verts_.reset();
  }

  bke::BVHTreeFromMesh &corner_tris() override
  {
    if (!corner_tris_) {
      corner_tris_ = bke::bvhtree_from_mesh_corner_tris_ex(mesh_->vert_positions(),
                                                           mesh_->faces(),
                                                           mesh_->corner_verts(),
                                                           mesh_->corner_tris(),
                                                           this->selection().faces);
    }
    return *corner_tris_;
  }

  bke::BVHTreeFromMesh &loose_edges() override
  {
    if (!loose_edges_) {
      loose_edges_ = bke::bvhtree_from_mesh_edges_ex(
          mesh_->vert_positions(), mesh_->edges(), this->selection().loose_edges);
    }
    return *loose_edges_;
  }

  bke::BVHTreeFromMesh &loose_verts() override
  {
    if (!loose_verts_) {
      loose_verts_ = bke::bvhtree_from_mesh_verts_ex(mesh_->vert_positions(),
                                                     this->selection().loose_verts);
    }
    return *loose_verts_;
  }

 private:
  const SnapTargetElems &selection()
  {
    if (!elems_) {
      calc_target_elems(*mesh_, target_, elems_.emplace());
    }
    return *elems_;
  }
};

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
 *   when that mesh is later accessed.
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

  /* Trees of the elements of #mesh that can be snapped to. */
  SnapTargetTreesEditMesh trees;

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
    this->trees.clear();
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

static Mesh *create_mesh(const Object *ob_eval)
{
  Mesh *mesh = BKE_id_new_nomain<Mesh>(nullptr);
  Object *ob_orig = const_cast<Object *>(DEG_get_original(ob_eval));
  BMesh *bm = BKE_editmesh_bmesh_get_for_write(ob_orig);
  /* The hide and selection status is all that's needed to find the elements that can be snapped
   * to, see #SnapTargetTreesEditMesh. */
  BM_mesh_bm_to_me_only_select_and_hide(*bm, *mesh);
  return mesh;
}

static SnapCache_EditMesh *snap_object_data_editmesh_get(SnapObjectContext *sctx,
                                                         const Object *ob_eval,
                                                         bool create)
{
  BLI_assert((ob_eval->mode & OB_MODE_EDIT) || sctx->runtime.params.ignore_editmode_filtering);
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
    em_cache->mesh = create_mesh(ob_eval);
    if (mesh_ref) {
      em_cache->mesh_ref = mesh_ref;
      em_cache->runtime_ref = mesh_ref->runtime;
      em_cache->edit_data_ref = mesh_ref->runtime->edit_data.get();
    }
  }

  if (em_cache && em_cache->mesh) {
    em_cache->trees.set_source(*em_cache->mesh, sctx->editmesh_target);
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
    snap_mode_supported |= SCE_SNAP_TO_FACE | SCE_SNAP_TO_FACE_MIDPOINT |
                           SCE_SNAP_INDIVIDUAL_NEAREST | SNAP_TO_EDGE_ELEMENTS | SCE_SNAP_TO_POINT;
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
  if (!sctx->runtime.params.ignore_editmode_filtering) {
    if ((ob_eval->mode & OB_MODE_EDIT) == 0) {
      return nullptr;
    }
  }

  Object *ob_orig = const_cast<Object *>(DEG_get_original(ob_eval));
  BMesh *bm = BKE_editmesh_bmesh_get_for_write(ob_orig);
  if (bm == nullptr) {
    return nullptr;
  }

  SnapCache_EditMesh *em_cache = snap_object_data_editmesh_get(sctx, ob_eval, false);
  if (em_cache != nullptr) {
    return em_cache;
  }

  eSnapMode snap_mode_used = snap_to_flag & editmesh_snap_mode_supported(bm);
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
    return snap_object_mesh(
        sctx, ob_eval, &em_cache->mesh->id, obmat, snap_to_flag, em_cache->trees, true);
  }
  return SCE_SNAP_TO_NONE;
}

}  // namespace blender::ed::transform
