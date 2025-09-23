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

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "DRW_select_buffer.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_sculpt.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "mesh_intern.hh"

using blender::float3;
using blender::int2;
using blender::MutableSpan;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Join as Shapes
 *
 * Add vertex positions of selected meshes as shape keys to the active mesh.
 * \{ */

static std::string create_mirrored_name(const blender::StringRefNull object_name,
                                        const bool mirror)
{
  if (!mirror) {
    return object_name;
  }
  if (object_name.endswith(".L")) {
    return blender::StringRef(object_name).drop_suffix(2) + ".R";
  }
  if (object_name.endswith(".R")) {
    return blender::StringRef(object_name).drop_suffix(2) + ".L";
  }
  return object_name;
}

wmOperatorStatus ED_mesh_shapes_join_objects_exec(bContext *C,
                                                  const bool ensure_keys_exist,
                                                  const bool mirror,
                                                  ReportList *reports)
{
  using namespace blender;
  Main *bmain = CTX_data_main(C);
  Object &active_object = *CTX_data_active_object(C);
  Depsgraph &depsgraph = *CTX_data_ensure_evaluated_depsgraph(C);
  Mesh &active_mesh = *static_cast<Mesh *>(active_object.data);

  struct ObjectInfo {
    StringRefNull name;
    const Mesh &mesh;
  };

  auto topology_count_matches = [](const Mesh &a, const Mesh &b) {
    return a.verts_num == b.verts_num;
  };

  bool found_object = false;
  bool found_non_equal_count = false;
  Vector<ObjectInfo> compatible_objects;
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == &active_object) {
      continue;
    }
    if (ob_iter->type != OB_MESH) {
      continue;
    }
    const Object *object_eval = DEG_get_evaluated(&depsgraph, ob_iter);
    if (!object_eval) {
      continue;
    }
    found_object = true;
    if (const Mesh *mesh = BKE_object_get_evaluated_mesh(object_eval)) {
      if (topology_count_matches(*mesh, active_mesh)) {
        compatible_objects.append({BKE_id_name(ob_iter->id), *mesh});
        continue;
      }
    }
    /* Fall back to the original mesh. */
    const Mesh &mesh_orig = *static_cast<const Mesh *>(ob_iter->data);
    if (topology_count_matches(mesh_orig, active_mesh)) {
      compatible_objects.append({BKE_id_name(ob_iter->id), mesh_orig});
      continue;
    }
    found_non_equal_count = true;
  }
  CTX_DATA_END;

  if (!found_object) {
    BKE_report(reports, RPT_WARNING, "No source mesh objects selected");
    return OPERATOR_CANCELLED;
  }

  if (found_non_equal_count) {
    BKE_report(reports, RPT_WARNING, "Selected meshes must have equal numbers of vertices");
    return OPERATOR_CANCELLED;
  }

  if (compatible_objects.is_empty()) {
    BKE_report(
        reports, RPT_WARNING, "No additional selected meshes with equal vertex count to join");
    return OPERATOR_CANCELLED;
  }

  if (!active_mesh.key) {
    /* Initialize basis shape key with existing mesh. */
    active_mesh.key = BKE_key_add(bmain, &active_mesh.id);
    active_mesh.key->type = KEY_RELATIVE;
    BKE_keyblock_convert_from_mesh(
        &active_mesh, active_mesh.key, BKE_keyblock_add(active_mesh.key, nullptr));
  }

  if (mirror) {
    for (const ObjectInfo &info : compatible_objects) {
      if (!info.name.endswith(".L") && !info.name.endswith(".R")) {
        BKE_report(reports, RPT_ERROR, "Selected objects' names must use .L or .R suffix");
        return OPERATOR_CANCELLED;
      }
    }
  }

  int mirror_count = 0;
  int mirror_fail_count = 0;
  int keys_changed = 0;
  bool any_keys_added = false;
  for (const ObjectInfo &info : compatible_objects) {
    const std::string name = create_mirrored_name(info.name, mirror);
    if (ensure_keys_exist) {
      KeyBlock *kb = BKE_keyblock_add(active_mesh.key, name.c_str());
      BKE_keyblock_convert_from_mesh(&info.mesh, active_mesh.key, kb);
      any_keys_added = true;
      if (mirror) {
        ed::object::shape_key_mirror(&active_object, kb, false, mirror_count, mirror_fail_count);
      }
    }
    else if (KeyBlock *kb = BKE_keyblock_find_name(active_mesh.key, name.c_str())) {
      keys_changed++;
      BKE_keyblock_update_from_mesh(&info.mesh, kb);
      if (mirror) {
        ed::object::shape_key_mirror(&active_object, kb, false, mirror_count, mirror_fail_count);
      }
    }
  }

  if (!ensure_keys_exist) {
    if (keys_changed == 0) {
      BKE_report(reports, RPT_ERROR, "No name matches between selected objects and shape keys");
      return OPERATOR_CANCELLED;
    }
    BKE_reportf(reports, RPT_INFO, "Updated %d shape key(s)", keys_changed);
  }

  if (mirror) {
    ED_mesh_report_mirror_ex(*reports, mirror_count, mirror_fail_count, SCE_SELECT_VERTEX);
  }

  DEG_id_tag_update(&active_mesh.id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, &active_mesh.id);

  if (any_keys_added && bmain) {
    /* Adding a new shape key should trigger a rebuild of relationships. */
    DEG_relations_tag_update(bmain);
  }

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

  mirrorverts = MEM_calloc_arrayN<int>(totvert, "MirrorVerts");
  mirrorfaces = MEM_calloc_arrayN<int>(2 * totface, "MirrorFaces");

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
    const Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
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

    /* Map the `dm` to `mesh`, setting the `r_index` if possible. */
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

bool ED_mesh_pick_edge(bContext *C, Object *ob, const int mval[2], uint dist_px, uint *r_index)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  BLI_assert(mesh && GS(mesh->id.name) == ID_ME);

  if (!mesh || mesh->edges_num == 0) {
    return false;
  }

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  ED_view3d_select_id_validate(&vc);
  Base *base = BKE_view_layer_base_find(vc.view_layer, vc.obact);
  DRW_select_buffer_context_create(vc.depsgraph, {base}, SCE_SELECT_EDGE);

  uint edge_idx_best = ORIGINDEX_NONE;

  if (dist_px) {
    /* Sample rect to increase chances of selecting, so that when clicking
     * on an edge in the back-buffer, we can still select a face. */
    edge_idx_best = DRW_select_buffer_find_nearest_to_point(
        vc.depsgraph, vc.region, vc.v3d, mval, 1, mesh->edges_num + 1, &dist_px);
  }
  else {
    /* sample only on the exact position */
    edge_idx_best = DRW_select_buffer_sample_point(vc.depsgraph, vc.region, vc.v3d, mval);
  }

  if (edge_idx_best == 0 || edge_idx_best > uint(mesh->edges_num)) {
    return false;
  }

  edge_idx_best--;

  if (edge_idx_best != ORIGINDEX_NONE) {
    *r_index = edge_idx_best;
    return true;
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
    const Object *ob_eval = DEG_get_evaluated(vc.depsgraph, ob);
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
static wmOperatorStatus mesh_reorder_vertices_spatial_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_active_object(C);

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  Scene *scene = CTX_data_scene(C);

  if (ob->mode == OB_MODE_SCULPT && mesh->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    /* Dyntopo not supported. */
    BKE_report(op->reports, RPT_INFO, "Not supported in dynamic topology sculpting");
    return OPERATOR_CANCELLED;
  }

  if (mesh->faces_num == 0 || mesh->verts_num == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ob->mode == OB_MODE_SCULPT) {
    blender::ed::sculpt_paint::undo::geometry_begin(*scene, *ob, op);
  }

  blender::bke::mesh_apply_spatial_organization(*mesh);

  if (ob->mode == OB_MODE_SCULPT) {
    blender::ed::sculpt_paint::undo::geometry_end(*ob);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  BKE_report(op->reports, RPT_INFO, "Mesh faces and vertices reordered spatially");

  return OPERATOR_FINISHED;
}

static bool mesh_reorder_vertices_spatial_poll(bContext *C)
{
  Object *ob = blender::ed::object::context_active_object(C);
  if (!ob || ob->type != OB_MESH) {
    return false;
  }

  return true;
}

void MESH_OT_reorder_vertices_spatial(wmOperatorType *ot)
{
  ot->name = "Reorder Mesh Spatially";
  ot->idname = "MESH_OT_reorder_vertices_spatial";
  ot->description =
      "Reorder mesh faces and vertices based on their spatial position for better BVH building "
      "and sculpting performance.";

  ot->exec = mesh_reorder_vertices_spatial_exec;
  ot->poll = mesh_reorder_vertices_spatial_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
