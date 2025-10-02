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

#include "BLI_linklist.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_editmesh.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_object_types.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph_query.hh"

BLI_INLINE void tan_calc_quat_v3(float r_quat[4],
                                 const float co_1[3],
                                 const float co_2[3],
                                 const float co_3[3])
{
  float vec_u[3], vec_v[3];
  float nor[3];

  sub_v3_v3v3(vec_u, co_1, co_2);
  sub_v3_v3v3(vec_v, co_1, co_3);

  cross_v3_v3v3(nor, vec_u, vec_v);

  if (normalize_v3(nor) > FLT_EPSILON) {
    const float zero_vec[3] = {0.0f};
    tri_to_quat_ex(r_quat, zero_vec, vec_u, vec_v, nor);
  }
  else {
    unit_qt(r_quat);
  }
}

static void set_crazy_vertex_quat(float r_quat[4],
                                  const float co_1[3],
                                  const float co_2[3],
                                  const float co_3[3],
                                  const float vd_1[3],
                                  const float vd_2[3],
                                  const float vd_3[3])
{
  float q1[4], q2[4];

  tan_calc_quat_v3(q1, co_1, co_2, co_3);
  tan_calc_quat_v3(q2, vd_1, vd_2, vd_3);

  sub_qt_qtqt(r_quat, q2, q1);
}

static bool modifiers_disable_subsurf_temporary(Object *ob, const int cageIndex)
{
  bool changed = false;

  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.first);
  for (int i = 0; md && i <= cageIndex; i++, md = md->next) {
    if (md->type == eModifierType_Subsurf) {
      md->mode ^= eModifierMode_DisableTemporary;
      changed = true;
    }
  }

  return changed;
}

blender::Array<blender::float3> BKE_crazyspace_get_mapped_editverts(Depsgraph *depsgraph,
                                                                    Object *obedit)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *obedit_eval = DEG_get_evaluated(depsgraph, obedit);
  const int cageIndex = BKE_modifiers_get_cage_index(scene_eval, obedit_eval, nullptr, true);

  /* Disable subsurf temporal, get mapped cos, and enable it. */
  if (modifiers_disable_subsurf_temporary(obedit_eval, cageIndex)) {
    /* Need to make new cage.
     * TODO: Avoid losing original evaluated geometry. */
    blender::bke::mesh_data_update(*depsgraph, *scene_eval, *obedit_eval, CD_MASK_BAREMESH);
  }

  /* Now get the cage. */
  BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);
  Mesh *mesh_eval_cage = blender::bke::editbmesh_get_eval_cage(
      depsgraph, scene_eval, obedit_eval, em_eval, &CD_MASK_BAREMESH);

  const int nverts = em_eval->bm->totvert;
  blender::Array<blender::float3> vertexcos(nverts);
  blender::bke::mesh_get_mapped_verts_coords(mesh_eval_cage, vertexcos);

  /* Set back the flag, and ensure new cage needs to be built. */
  if (modifiers_disable_subsurf_temporary(obedit_eval, cageIndex)) {
    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  }

  return vertexcos;
}

void BKE_crazyspace_set_quats_editmesh(BMEditMesh *em,
                                       const blender::Span<blender::float3> origcos,
                                       const blender::Span<blender::float3> mappedcos,
                                       float (*quats)[4],
                                       const bool use_select)
{
  using namespace blender;
  BMFace *f;
  BMIter iter;
  int index;
  const bool has_origcos = !origcos.is_empty();

  {
    BMVert *v;
    BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, index) {
      BM_elem_flag_disable(v, BM_ELEM_TAG);
      BM_elem_index_set(v, index); /* set_inline */
    }
    em->bm->elem_index_dirty &= ~BM_VERT;
  }

  BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (BM_elem_flag_test(l_iter->v, BM_ELEM_HIDDEN) ||
          BM_elem_flag_test(l_iter->v, BM_ELEM_TAG) ||
          (use_select && !BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)))
      {
        continue;
      }

      if (!BM_elem_flag_test(l_iter->v, BM_ELEM_TAG)) {
        const float *co_prev, *co_curr, *co_next; /* orig */

        const int vert_prev = BM_elem_index_get(l_iter->prev->v);
        const int vert = BM_elem_index_get(l_iter->v);
        const int vert_next = BM_elem_index_get(l_iter->next->v);

        /* Retrieve mapped coordinates. */
        const float3 &vd_prev = mappedcos[vert_prev];
        const float3 &vd_curr = mappedcos[vert];
        const float3 &vd_next = mappedcos[vert_next];

        if (has_origcos) {
          co_prev = origcos[vert_prev];
          co_curr = origcos[vert];
          co_next = origcos[vert_next];
        }
        else {
          co_prev = l_iter->prev->v->co;
          co_curr = l_iter->v->co;
          co_next = l_iter->next->v->co;
        }

        set_crazy_vertex_quat(quats[vert], co_curr, co_next, co_prev, vd_curr, vd_next, vd_prev);

        BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
      }
    } while ((l_iter = l_iter->next) != l_first);
  }
}

void BKE_crazyspace_set_quats_mesh(Mesh *mesh,
                                   const blender::Span<blender::float3> origcos,
                                   const blender::Span<blender::float3> mappedcos,
                                   float (*quats)[4])
{
  using namespace blender;
  using namespace blender::bke;
  BitVector<> vert_tag(mesh->verts_num);

  /* first store two sets of tangent vectors in vertices, we derive it just from the face-edges */
  const Span<float3> positions = origcos.is_empty() ? mesh->vert_positions() : origcos;
  const OffsetIndices<int> faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];

    for (const int corner : face) {
      const int vert = corner_verts[corner];
      if (vert_tag[vert]) {
        continue;
      }
      const int vert_prev = corner_verts[mesh::face_corner_prev(face, corner)];
      const int vert_next = corner_verts[mesh::face_corner_next(face, corner)];

      const float3 &vd_prev = mappedcos[vert_prev];
      const float3 &vd_curr = mappedcos[vert];
      const float3 &vd_next = mappedcos[vert_next];

      const float3 &co_prev = positions[vert_prev];
      const float3 &co_curr = positions[vert];
      const float3 &co_next = positions[vert_next];

      set_crazy_vertex_quat(quats[vert], co_curr, co_next, co_prev, vd_curr, vd_next, vd_prev);

      vert_tag[vert].set();
    }
  }
}

int BKE_crazyspace_get_first_deform_matrices_editbmesh(
    Depsgraph *depsgraph,
    Scene *scene,
    Object *ob,
    BMEditMesh *em,
    blender::Array<blender::float3x3, 0> &deformmats,
    blender::Array<blender::float3, 0> &deformcos)
{
  ModifierData *md;
  Mesh *me_input = static_cast<Mesh *>(ob->data);
  Mesh *mesh = nullptr;
  int i, modifiers_left_num = 0;
  const int verts_num = em->bm->totvert;
  int cageIndex = BKE_modifiers_get_cage_index(scene, ob, nullptr, true);
  VirtualModifierData virtual_modifier_data;
  ModifierEvalContext mectx = {depsgraph, ob, ModifierApplyFlag(0)};

  BKE_modifiers_clear_errors(ob);

  md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data);

  /* compute the deformation matrices and coordinates for the first
   * modifiers with on cage editing that are enabled and support computing
   * deform matrices */
  for (i = 0; md && i <= cageIndex; i++, md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(static_cast<ModifierType>(md->type));

    if (!blender::bke::editbmesh_modifier_is_enabled(scene, ob, md, mesh != nullptr)) {
      continue;
    }

    if (mti->type == ModifierTypeType::OnlyDeform && mti->deform_matrices_EM) {
      if (deformmats.is_empty()) {
        const int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;
        CustomData_MeshMasks cd_mask_extra = CD_MASK_BAREMESH;
        CDMaskLink *datamasks = BKE_modifier_calc_data_masks(
            scene, md, &cd_mask_extra, required_mode);
        cd_mask_extra = datamasks->mask;
        BLI_linklist_free((LinkNode *)datamasks, nullptr);

        mesh = BKE_mesh_wrapper_from_editmesh(
            std::make_shared<BMEditMesh>(*em), &cd_mask_extra, me_input);
        deformcos.reinitialize(verts_num);
        BKE_mesh_wrapper_vert_coords_copy(mesh, deformcos);
        deformmats.reinitialize(verts_num);
        deformmats.fill(blender::float3x3::identity());
      }
      mti->deform_matrices_EM(md, &mectx, em, mesh, deformcos, deformmats);
    }
    else {
      break;
    }
  }

  for (; md && i <= cageIndex; md = md->next, i++) {
    if (blender::bke::editbmesh_modifier_is_enabled(scene, ob, md, mesh != nullptr) &&
        BKE_modifier_is_correctable_deformed(md))
    {
      modifiers_left_num++;
    }
  }

  if (mesh) {
    BKE_id_free(nullptr, mesh);
  }

  return modifiers_left_num;
}

/**
 * Crazy-space evaluation needs to have an object which has all the fields
 * evaluated, but the mesh data being at undeformed state. This way it can
 * re-apply modifiers and also have proper pointers to key data blocks.
 *
 * Similar to #BKE_object_eval_reset(), but does not modify the actual evaluated object.
 */
static void crazyspace_init_object_for_eval(Depsgraph *depsgraph,
                                            Object *object,
                                            Object *object_crazy)
{
  Object *object_eval = DEG_get_evaluated(depsgraph, object);
  *object_crazy = blender::dna::shallow_copy(*object_eval);
  object_crazy->runtime = MEM_new<blender::bke::ObjectRuntime>(__func__, *object_eval->runtime);
  if (object_crazy->runtime->data_orig != nullptr) {
    object_crazy->data = object_crazy->runtime->data_orig;
  }
}

static bool crazyspace_modifier_supports_deform_matrices(ModifierData *md)
{
  if (ELEM(md->type, eModifierType_Subsurf, eModifierType_Multires)) {
    return true;
  }
  const ModifierTypeInfo *mti = BKE_modifier_get_info(static_cast<ModifierType>(md->type));
  return (mti->type == ModifierTypeType::OnlyDeform);
}

static bool crazyspace_modifier_supports_deform(ModifierData *md)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info(static_cast<ModifierType>(md->type));
  return (mti->type == ModifierTypeType::OnlyDeform);
}

int BKE_sculpt_get_first_deform_matrices(Depsgraph *depsgraph,
                                         Scene *scene,
                                         Object *object,
                                         blender::Array<blender::float3x3, 0> &deformmats,
                                         blender::Array<blender::float3, 0> &deformcos)
{
  ModifierData *md;
  Mesh *mesh_eval = nullptr;
  int modifiers_left_num = 0;
  VirtualModifierData virtual_modifier_data;
  Object object_eval;
  crazyspace_init_object_for_eval(depsgraph, object, &object_eval);
  BLI_SCOPED_DEFER([&]() { MEM_delete(object_eval.runtime); });
  MultiresModifierData *mmd = get_multires_modifier(scene, &object_eval, false);
  const bool is_sculpt_mode = (object->mode & OB_MODE_SCULPT) != 0;
  const bool has_multires = mmd != nullptr && mmd->sculptlvl > 0;
  const ModifierEvalContext mectx = {depsgraph, &object_eval, ModifierApplyFlag(0)};

  if (is_sculpt_mode && has_multires) {
    deformcos = {};
    deformmats = {};
    return modifiers_left_num;
  }

  md = BKE_modifiers_get_virtual_modifierlist(&object_eval, &virtual_modifier_data);

  for (; md; md = md->next) {
    if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
      continue;
    }

    if (crazyspace_modifier_supports_deform_matrices(md)) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(static_cast<ModifierType>(md->type));
      if (deformmats.is_empty()) {
        /* NOTE: Evaluated object is re-set to its original un-deformed state. */
        Mesh *mesh = static_cast<Mesh *>(object_eval.data);
        mesh_eval = BKE_mesh_copy_for_eval(*mesh);
        deformcos = mesh->vert_positions();
        deformmats.reinitialize(mesh->verts_num);
        deformmats.fill(blender::float3x3::identity());
      }

      if (mti->deform_matrices) {
        mti->deform_matrices(md, &mectx, mesh_eval, deformcos, deformmats);
      }
      else {
        /* More complex handling will continue in BKE_crazyspace_build_sculpt.
         * Exiting the loop on a non-deform modifier causes issues - #71213. */
        BLI_assert(crazyspace_modifier_supports_deform(md));
        break;
      }
    }
  }

  for (; md; md = md->next) {
    if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
      continue;
    }

    if (crazyspace_modifier_supports_deform(md)) {
      modifiers_left_num++;
    }
  }

  if (mesh_eval != nullptr) {
    BKE_id_free(nullptr, mesh_eval);
  }

  return modifiers_left_num;
}

void BKE_crazyspace_build_sculpt(Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *object,
                                 blender::Array<blender::float3x3, 0> &deformmats,
                                 blender::Array<blender::float3, 0> &deformcos)
{
  int totleft = BKE_sculpt_get_first_deform_matrices(
      depsgraph, scene, object, deformmats, deformcos);

  if (totleft) {
    /* There are deformation modifier which doesn't support deformation matrices calculation.
     * Need additional crazy-space correction. */

    Mesh *mesh = (Mesh *)object->data;
    Mesh *mesh_eval = nullptr;

    if (deformcos.is_empty()) {
      deformcos = mesh->vert_positions();
      deformmats.reinitialize(mesh->verts_num);
      deformmats.fill(blender::float3x3::identity());
    }

    blender::Array<blender::float3, 0> origVerts = deformcos;
    float (*quats)[4];
    int i, deformed = 0;
    VirtualModifierData virtual_modifier_data;
    Object object_eval;
    crazyspace_init_object_for_eval(depsgraph, object, &object_eval);
    BLI_SCOPED_DEFER([&]() { MEM_delete(object_eval.runtime); });
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(&object_eval,
                                                              &virtual_modifier_data);
    const ModifierEvalContext mectx = {depsgraph, &object_eval, ModifierApplyFlag(0)};

    for (; md; md = md->next) {
      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (crazyspace_modifier_supports_deform(md)) {
        const ModifierTypeInfo *mti = BKE_modifier_get_info(static_cast<ModifierType>(md->type));

        /* skip leading modifiers which have been already
         * handled in sculpt_get_first_deform_matrices */
        if (mti->deform_matrices && !deformed) {
          continue;
        }

        if (mesh_eval == nullptr) {
          mesh_eval = BKE_mesh_copy_for_eval(*mesh);
        }

        mti->deform_verts(md, &mectx, mesh_eval, deformcos);
        deformed = 1;
      }
    }

    quats = MEM_malloc_arrayN<float[4]>(size_t(mesh->verts_num), "crazy quats");

    BKE_crazyspace_set_quats_mesh(mesh, origVerts, deformcos, quats);

    for (i = 0; i < mesh->verts_num; i++) {
      float qmat[3][3], tmat[3][3];

      quat_to_mat3(qmat, quats[i]);
      mul_m3_m3m3(tmat, qmat, deformmats[i].ptr());
      copy_m3_m3(deformmats[i].ptr(), tmat);
    }

    MEM_freeN(quats);

    if (mesh_eval != nullptr) {
      BKE_id_free(nullptr, mesh_eval);
    }
  }

  if (deformmats.is_empty()) {
    Mesh *mesh = (Mesh *)object->data;

    deformcos = mesh->vert_positions();
    deformmats.reinitialize(mesh->verts_num);
    deformmats.fill(blender::float3x3::identity());
  }
}

/* -------------------------------------------------------------------- */
/** \name Crazyspace API
 * \{ */

void BKE_crazyspace_api_eval(Depsgraph *depsgraph,
                             Scene *scene,
                             Object *object,
                             ReportList *reports)
{
  if (!object->runtime->crazyspace_deform_imats.is_empty() ||
      !object->runtime->crazyspace_deform_cos.is_empty())
  {
    return;
  }

  if (object->type != OB_MESH) {
    BKE_report(reports,
               RPT_ERROR,
               "Crazyspace transformation is only available for Mesh type of objects");
    return;
  }

  BKE_crazyspace_build_sculpt(depsgraph,
                              scene,
                              object,
                              object->runtime->crazyspace_deform_imats,
                              object->runtime->crazyspace_deform_cos);
}

void BKE_crazyspace_api_displacement_to_deformed(Object *object,
                                                 ReportList *reports,
                                                 int vert,
                                                 const float displacement[3],
                                                 float r_displacement_deformed[3])
{
  if (vert < 0 || vert >= object->runtime->crazyspace_deform_imats.size()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Invalid vertex index %d (expected to be within 0 to %d range)",
                vert,
                int(object->runtime->crazyspace_deform_imats.size()));
    return;
  }

  mul_v3_m3v3(
      r_displacement_deformed, object->runtime->crazyspace_deform_imats[vert].ptr(), displacement);
}

void BKE_crazyspace_api_displacement_to_original(Object *object,
                                                 ReportList *reports,
                                                 int vert,
                                                 const float displacement_deformed[3],
                                                 float r_displacement[3])
{
  if (vert < 0 || vert >= object->runtime->crazyspace_deform_imats.size()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Invalid vertex index %d (expected to be within 0 to %d range)",
                vert,
                int(object->runtime->crazyspace_deform_imats.size()));
    return;
  }

  float mat[3][3];
  if (!invert_m3_m3(mat, object->runtime->crazyspace_deform_imats[vert].ptr())) {
    copy_v3_v3(r_displacement, displacement_deformed);
    return;
  }

  mul_v3_m3v3(r_displacement, mat, displacement_deformed);
}

void BKE_crazyspace_api_eval_clear(Object *object)
{
  object->runtime->crazyspace_deform_imats = {};
  object->runtime->crazyspace_deform_cos = {};
}

/** \} */

namespace blender::bke::crazyspace {

GeometryDeformation get_evaluated_curves_deformation(const Object *ob_eval, const Object &ob_orig)
{
  BLI_assert(ob_orig.type == OB_CURVES);
  const Curves &curves_id_orig = *static_cast<const Curves *>(ob_orig.data);
  const CurvesGeometry &curves_orig = curves_id_orig.geometry.wrap();
  const int points_num = curves_orig.points_num();

  GeometryDeformation deformation;
  /* Use the undeformed positions by default. */
  deformation.positions = curves_orig.positions();

  if (ob_eval == nullptr) {
    return deformation;
  }
  const GeometrySet *geometry_eval = ob_eval->runtime->geometry_set_eval;
  if (geometry_eval == nullptr) {
    return deformation;
  }

  /* If available, use deformation information generated during evaluation. */
  const GeometryComponentEditData *edit_component_eval =
      geometry_eval->get_component<GeometryComponentEditData>();
  bool uses_extra_positions = false;
  if (edit_component_eval != nullptr) {
    const CurvesEditHints *edit_hints = edit_component_eval->curves_edit_hints_.get();
    if (edit_hints != nullptr && &edit_hints->curves_id_orig == &curves_id_orig) {
      if (const std::optional<Span<float3>> positions = edit_hints->positions()) {
        BLI_assert(positions->size() == points_num);
        deformation.positions = *positions;
        uses_extra_positions = true;
      }
      if (edit_hints->deform_mats.has_value()) {
        BLI_assert(edit_hints->deform_mats->size() == points_num);
        deformation.deform_mats = *edit_hints->deform_mats;
      }
    }
  }

  /* Use the positions of the evaluated curves directly, if the number of points matches. */
  if (!uses_extra_positions) {
    const CurveComponent *curves_component_eval = geometry_eval->get_component<CurveComponent>();
    if (curves_component_eval != nullptr) {
      const Curves *curves_id_eval = curves_component_eval->get();
      if (curves_id_eval != nullptr) {
        const CurvesGeometry &curves_eval = curves_id_eval->geometry.wrap();
        if (curves_eval.points_num() == points_num) {
          deformation.positions = curves_eval.positions();
        }
      }
    }
  }
  return deformation;
}

GeometryDeformation get_evaluated_curves_deformation(const Depsgraph &depsgraph,
                                                     const Object &ob_orig)
{
  const Object *ob_eval = DEG_get_evaluated(&depsgraph, &ob_orig);
  return get_evaluated_curves_deformation(ob_eval, ob_orig);
}

static const GreasePencilDrawingEditHints *get_drawing_edit_hint_for_original_drawing(
    const GreasePencilEditHints *edit_hints, const bke::greasepencil::Drawing &drawing_orig)
{
  for (const GreasePencilDrawingEditHints &drawing_hint : *edit_hints->drawing_hints) {
    if (drawing_hint.drawing_orig == &drawing_orig) {
      return &drawing_hint;
    }
  }
  return {};
}

GeometryDeformation get_evaluated_grease_pencil_drawing_deformation(
    const Object *ob_eval, const Object &ob_orig, const bke::greasepencil::Drawing &drawing_orig)
{
  BLI_assert(ob_orig.type == OB_GREASE_PENCIL);
  const GreasePencil &grease_pencil_orig = *static_cast<const GreasePencil *>(ob_orig.data);

  GeometryDeformation deformation;
  /* Use the undeformed positions by default. */
  deformation.positions = drawing_orig.strokes().positions();

  if (ob_eval == nullptr) {
    return deformation;
  }
  const GeometrySet *geometry_eval = ob_eval->runtime->geometry_set_eval;
  if (geometry_eval == nullptr) {
    return deformation;
  }

  /* If there are edit hints, use the positions of those. */
  if (geometry_eval->has<GeometryComponentEditData>()) {
    const GeometryComponentEditData &edit_component_eval =
        *geometry_eval->get_component<GeometryComponentEditData>();
    const GreasePencilEditHints *edit_hints = edit_component_eval.grease_pencil_edit_hints_.get();
    if (edit_hints != nullptr && &edit_hints->grease_pencil_id_orig == &grease_pencil_orig &&
        edit_hints->drawing_hints.has_value())
    {
      if (const GreasePencilDrawingEditHints *drawing_hints =
              get_drawing_edit_hint_for_original_drawing(edit_hints, drawing_orig))
      {
        if (drawing_hints->positions()) {
          deformation.positions = *drawing_hints->positions();
        }
        if (drawing_hints->deform_mats.has_value()) {
          deformation.deform_mats = *drawing_hints->deform_mats;
        }
      }
    }
  }

  return deformation;
}

GeometryDeformation get_evaluated_grease_pencil_drawing_deformation(
    const Depsgraph &depsgraph,
    const Object &ob_orig,
    const bke::greasepencil::Drawing &drawing_orig)
{
  const Object *ob_eval = DEG_get_evaluated(&depsgraph, &ob_orig);
  return get_evaluated_grease_pencil_drawing_deformation(ob_eval, ob_orig, drawing_orig);
}

}  // namespace blender::bke::crazyspace
