/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

#include "eigen_capi.h"

struct LaplacianSystem {
  float *eweights;      /* Length weights per Edge */
  float (*fweights)[3]; /* Cotangent weights per face */
  float *ring_areas;    /* Total area per ring. */
  float *vlengths;      /* Total sum of lengths(edges) per vertex. */
  float *vweights;      /* Total sum of weights per vertex. */
  int verts_num;        /* Number of verts. */
  short *ne_fa_num;     /* Number of neighbors faces around vertex. */
  short *ne_ed_num;     /* Number of neighbors Edges around vertex. */
  bool *zerola;         /* Is zero area or length. */

  /* Pointers to data. */
  float (*vertexCos)[3];
  blender::Span<blender::int2> edges;
  blender::OffsetIndices<int> polys;
  blender::Span<int> corner_verts;
  LinearSolver *context;

  /* Data. */
  float min_area;
  float vert_centroid[3];
};

static void delete_laplacian_system(LaplacianSystem *sys)
{
  MEM_SAFE_FREE(sys->eweights);
  MEM_SAFE_FREE(sys->fweights);
  MEM_SAFE_FREE(sys->ne_ed_num);
  MEM_SAFE_FREE(sys->ne_fa_num);
  MEM_SAFE_FREE(sys->ring_areas);
  MEM_SAFE_FREE(sys->vlengths);
  MEM_SAFE_FREE(sys->vweights);
  MEM_SAFE_FREE(sys->zerola);

  if (sys->context) {
    EIG_linear_solver_delete(sys->context);
  }
  sys->vertexCos = nullptr;
  MEM_freeN(sys);
}

static void memset_laplacian_system(LaplacianSystem *sys, int val)
{
  memset(sys->eweights, val, sizeof(float) * sys->edges.size());
  memset(sys->fweights, val, sizeof(float[3]) * sys->corner_verts.size());
  memset(sys->ne_ed_num, val, sizeof(short) * sys->verts_num);
  memset(sys->ne_fa_num, val, sizeof(short) * sys->verts_num);
  memset(sys->ring_areas, val, sizeof(float) * sys->verts_num);
  memset(sys->vlengths, val, sizeof(float) * sys->verts_num);
  memset(sys->vweights, val, sizeof(float) * sys->verts_num);
  memset(sys->zerola, val, sizeof(bool) * sys->verts_num);
}

static LaplacianSystem *init_laplacian_system(int a_numEdges, int a_numLoops, int a_numVerts)
{
  LaplacianSystem *sys;
  sys = static_cast<LaplacianSystem *>(MEM_callocN(sizeof(LaplacianSystem), __func__));
  sys->verts_num = a_numVerts;

  sys->eweights = MEM_cnew_array<float>(a_numEdges, __func__);
  sys->fweights = MEM_cnew_array<float[3]>(a_numLoops, __func__);
  sys->ne_ed_num = MEM_cnew_array<short>(sys->verts_num, __func__);
  sys->ne_fa_num = MEM_cnew_array<short>(sys->verts_num, __func__);
  sys->ring_areas = MEM_cnew_array<float>(sys->verts_num, __func__);
  sys->vlengths = MEM_cnew_array<float>(sys->verts_num, __func__);
  sys->vweights = MEM_cnew_array<float>(sys->verts_num, __func__);
  sys->zerola = MEM_cnew_array<bool>(sys->verts_num, __func__);

  return sys;
}

static float compute_volume(const float center[3],
                            float (*vertexCos)[3],
                            const blender::OffsetIndices<int> polys,
                            const blender::Span<int> corner_verts)
{
  float vol = 0.0f;

  for (const int i : polys.index_range()) {
    const blender::IndexRange poly = polys[i];
    int corner_first = poly.start();
    int corner_prev = corner_first + 1;
    int corner_curr = corner_first + 2;
    int corner_term = corner_first + poly.size();

    for (; corner_curr != corner_term; corner_prev = corner_curr, corner_curr++) {
      vol += volume_tetrahedron_signed_v3(center,
                                          vertexCos[corner_verts[corner_first]],
                                          vertexCos[corner_verts[corner_prev]],
                                          vertexCos[corner_verts[corner_curr]]);
    }
  }

  return fabsf(vol);
}

static void volume_preservation(LaplacianSystem *sys, float vini, float vend, short flag)
{
  float beta;
  int i;

  if (vend != 0.0f) {
    beta = pow(vini / vend, 1.0f / 3.0f);
    for (i = 0; i < sys->verts_num; i++) {
      if (flag & MOD_LAPLACIANSMOOTH_X) {
        sys->vertexCos[i][0] = (sys->vertexCos[i][0] - sys->vert_centroid[0]) * beta +
                               sys->vert_centroid[0];
      }
      if (flag & MOD_LAPLACIANSMOOTH_Y) {
        sys->vertexCos[i][1] = (sys->vertexCos[i][1] - sys->vert_centroid[1]) * beta +
                               sys->vert_centroid[1];
      }
      if (flag & MOD_LAPLACIANSMOOTH_Z) {
        sys->vertexCos[i][2] = (sys->vertexCos[i][2] - sys->vert_centroid[2]) * beta +
                               sys->vert_centroid[2];
      }
    }
  }
}

static void init_laplacian_matrix(LaplacianSystem *sys)
{
  float *v1, *v2;
  float w1, w2, w3;
  float areaf;
  int i;
  uint idv1, idv2;

  for (i = 0; i < sys->edges.size(); i++) {
    idv1 = sys->edges[i][0];
    idv2 = sys->edges[i][1];

    v1 = sys->vertexCos[idv1];
    v2 = sys->vertexCos[idv2];

    sys->ne_ed_num[idv1] = sys->ne_ed_num[idv1] + 1;
    sys->ne_ed_num[idv2] = sys->ne_ed_num[idv2] + 1;
    w1 = len_v3v3(v1, v2);
    if (w1 < sys->min_area) {
      sys->zerola[idv1] = true;
      sys->zerola[idv2] = true;
    }
    else {
      w1 = 1.0f / w1;
    }

    sys->eweights[i] = w1;
  }

  const blender::Span<int> corner_verts = sys->corner_verts;

  for (const int i : sys->polys.index_range()) {
    const blender::IndexRange poly = sys->polys[i];
    int corner_next = poly.start();
    int corner_term = corner_next + poly.size();
    int corner_prev = corner_term - 2;
    int corner_curr = corner_term - 1;

    for (; corner_next != corner_term;
         corner_prev = corner_curr, corner_curr = corner_next, corner_next++)
    {
      const float *v_prev = sys->vertexCos[corner_verts[corner_prev]];
      const float *v_curr = sys->vertexCos[corner_verts[corner_curr]];
      const float *v_next = sys->vertexCos[corner_verts[corner_next]];

      sys->ne_fa_num[corner_verts[corner_curr]] += 1;

      areaf = area_tri_v3(v_prev, v_curr, v_next);

      if (areaf < sys->min_area) {
        sys->zerola[corner_verts[corner_curr]] = true;
      }

      sys->ring_areas[corner_verts[corner_prev]] += areaf;
      sys->ring_areas[corner_verts[corner_curr]] += areaf;
      sys->ring_areas[corner_verts[corner_next]] += areaf;

      w1 = cotangent_tri_weight_v3(v_curr, v_next, v_prev) / 2.0f;
      w2 = cotangent_tri_weight_v3(v_next, v_prev, v_curr) / 2.0f;
      w3 = cotangent_tri_weight_v3(v_prev, v_curr, v_next) / 2.0f;

      sys->fweights[corner_curr][0] += w1;
      sys->fweights[corner_curr][1] += w2;
      sys->fweights[corner_curr][2] += w3;

      sys->vweights[corner_verts[corner_curr]] += w2 + w3;
      sys->vweights[corner_verts[corner_next]] += w1 + w3;
      sys->vweights[corner_verts[corner_prev]] += w1 + w2;
    }
  }
  for (i = 0; i < sys->edges.size(); i++) {
    idv1 = sys->edges[i][0];
    idv2 = sys->edges[i][1];
    /* if is boundary, apply scale-dependent umbrella operator only with neighbors in boundary */
    if (sys->ne_ed_num[idv1] != sys->ne_fa_num[idv1] &&
        sys->ne_ed_num[idv2] != sys->ne_fa_num[idv2]) {
      sys->vlengths[idv1] += sys->eweights[i];
      sys->vlengths[idv2] += sys->eweights[i];
    }
  }
}

static void fill_laplacian_matrix(LaplacianSystem *sys)
{
  int i;
  uint idv1, idv2;

  const blender::Span<int> corner_verts = sys->corner_verts;

  for (const int i : sys->polys.index_range()) {
    const blender::IndexRange poly = sys->polys[i];
    int corner_next = poly.start();
    int corner_term = corner_next + poly.size();
    int corner_prev = corner_term - 2;
    int corner_curr = corner_term - 1;

    for (; corner_next != corner_term;
         corner_prev = corner_curr, corner_curr = corner_next, corner_next++)
    {

      /* Is ring if number of faces == number of edges around vertex. */
      if (sys->ne_ed_num[corner_verts[corner_curr]] == sys->ne_fa_num[corner_verts[corner_curr]] &&
          sys->zerola[corner_verts[corner_curr]] == false)
      {
        EIG_linear_solver_matrix_add(sys->context,
                                     corner_verts[corner_curr],
                                     corner_verts[corner_next],
                                     sys->fweights[corner_curr][2] *
                                         sys->vweights[corner_verts[corner_curr]]);
        EIG_linear_solver_matrix_add(sys->context,
                                     corner_verts[corner_curr],
                                     corner_verts[corner_prev],
                                     sys->fweights[corner_curr][1] *
                                         sys->vweights[corner_verts[corner_curr]]);
      }
      if (sys->ne_ed_num[corner_verts[corner_next]] == sys->ne_fa_num[corner_verts[corner_next]] &&
          sys->zerola[corner_verts[corner_next]] == false)
      {
        EIG_linear_solver_matrix_add(sys->context,
                                     corner_verts[corner_next],
                                     corner_verts[corner_curr],
                                     sys->fweights[corner_curr][2] *
                                         sys->vweights[corner_verts[corner_next]]);
        EIG_linear_solver_matrix_add(sys->context,
                                     corner_verts[corner_next],
                                     corner_verts[corner_prev],
                                     sys->fweights[corner_curr][0] *
                                         sys->vweights[corner_verts[corner_next]]);
      }
      if (sys->ne_ed_num[corner_verts[corner_prev]] == sys->ne_fa_num[corner_verts[corner_prev]] &&
          sys->zerola[corner_verts[corner_prev]] == false)
      {
        EIG_linear_solver_matrix_add(sys->context,
                                     corner_verts[corner_prev],
                                     corner_verts[corner_curr],
                                     sys->fweights[corner_curr][1] *
                                         sys->vweights[corner_verts[corner_prev]]);
        EIG_linear_solver_matrix_add(sys->context,
                                     corner_verts[corner_prev],
                                     corner_verts[corner_next],
                                     sys->fweights[corner_curr][0] *
                                         sys->vweights[corner_verts[corner_prev]]);
      }
    }
  }

  for (i = 0; i < sys->edges.size(); i++) {
    idv1 = sys->edges[i][0];
    idv2 = sys->edges[i][1];
    /* Is boundary */
    if (sys->ne_ed_num[idv1] != sys->ne_fa_num[idv1] &&
        sys->ne_ed_num[idv2] != sys->ne_fa_num[idv2] && sys->zerola[idv1] == false &&
        sys->zerola[idv2] == false)
    {
      EIG_linear_solver_matrix_add(
          sys->context, idv1, idv2, sys->eweights[i] * sys->vlengths[idv1]);
      EIG_linear_solver_matrix_add(
          sys->context, idv2, idv1, sys->eweights[i] * sys->vlengths[idv2]);
    }
  }
}

static void validate_solution(LaplacianSystem *sys, short flag, float lambda, float lambda_border)
{
  int i;
  float lam;
  float vini = 0.0f, vend = 0.0f;

  if (flag & MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME) {
    vini = compute_volume(sys->vert_centroid, sys->vertexCos, sys->polys, sys->corner_verts);
  }
  for (i = 0; i < sys->verts_num; i++) {
    if (sys->zerola[i] == false) {
      lam = sys->ne_ed_num[i] == sys->ne_fa_num[i] ? (lambda >= 0.0f ? 1.0f : -1.0f) :
                                                     (lambda_border >= 0.0f ? 1.0f : -1.0f);
      if (flag & MOD_LAPLACIANSMOOTH_X) {
        sys->vertexCos[i][0] += lam * (float(EIG_linear_solver_variable_get(sys->context, 0, i)) -
                                       sys->vertexCos[i][0]);
      }
      if (flag & MOD_LAPLACIANSMOOTH_Y) {
        sys->vertexCos[i][1] += lam * (float(EIG_linear_solver_variable_get(sys->context, 1, i)) -
                                       sys->vertexCos[i][1]);
      }
      if (flag & MOD_LAPLACIANSMOOTH_Z) {
        sys->vertexCos[i][2] += lam * (float(EIG_linear_solver_variable_get(sys->context, 2, i)) -
                                       sys->vertexCos[i][2]);
      }
    }
  }
  if (flag & MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME) {
    vend = compute_volume(sys->vert_centroid, sys->vertexCos, sys->polys, sys->corner_verts);
    volume_preservation(sys, vini, vend, flag);
  }
}

static void laplaciansmoothModifier_do(
    LaplacianSmoothModifierData *smd, Object *ob, Mesh *mesh, float (*vertexCos)[3], int verts_num)
{
  LaplacianSystem *sys;
  const MDeformVert *dvert = nullptr;
  const MDeformVert *dv = nullptr;
  float w, wpaint;
  int i, iter;
  int defgrp_index;
  const bool invert_vgroup = (smd->flag & MOD_LAPLACIANSMOOTH_INVERT_VGROUP) != 0;

  sys = init_laplacian_system(mesh->totedge, mesh->totloop, verts_num);
  if (!sys) {
    return;
  }

  sys->edges = mesh->edges();
  sys->polys = mesh->polys();
  sys->corner_verts = mesh->corner_verts();
  sys->vertexCos = vertexCos;
  sys->min_area = 0.00001f;
  MOD_get_vgroup(ob, mesh, smd->defgrp_name, &dvert, &defgrp_index);

  sys->vert_centroid[0] = 0.0f;
  sys->vert_centroid[1] = 0.0f;
  sys->vert_centroid[2] = 0.0f;
  memset_laplacian_system(sys, 0);

  sys->context = EIG_linear_least_squares_solver_new(verts_num, verts_num, 3);

  init_laplacian_matrix(sys);

  for (iter = 0; iter < smd->repeat; iter++) {
    for (i = 0; i < verts_num; i++) {
      EIG_linear_solver_variable_set(sys->context, 0, i, vertexCos[i][0]);
      EIG_linear_solver_variable_set(sys->context, 1, i, vertexCos[i][1]);
      EIG_linear_solver_variable_set(sys->context, 2, i, vertexCos[i][2]);
      if (iter == 0) {
        add_v3_v3(sys->vert_centroid, vertexCos[i]);
      }
    }
    if (iter == 0 && verts_num > 0) {
      mul_v3_fl(sys->vert_centroid, 1.0f / float(verts_num));
    }

    dv = dvert;
    for (i = 0; i < verts_num; i++) {
      EIG_linear_solver_right_hand_side_add(sys->context, 0, i, vertexCos[i][0]);
      EIG_linear_solver_right_hand_side_add(sys->context, 1, i, vertexCos[i][1]);
      EIG_linear_solver_right_hand_side_add(sys->context, 2, i, vertexCos[i][2]);
      if (iter == 0) {
        if (dv) {
          wpaint = invert_vgroup ? 1.0f - BKE_defvert_find_weight(dv, defgrp_index) :
                                   BKE_defvert_find_weight(dv, defgrp_index);
          dv++;
        }
        else {
          wpaint = 1.0f;
        }

        if (sys->zerola[i] == false) {
          if (smd->flag & MOD_LAPLACIANSMOOTH_NORMALIZED) {
            w = sys->vweights[i];
            sys->vweights[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda) * wpaint / w;
            w = sys->vlengths[i];
            sys->vlengths[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda_border) * wpaint * 2.0f / w;
            if (sys->ne_ed_num[i] == sys->ne_fa_num[i]) {
              EIG_linear_solver_matrix_add(sys->context, i, i, 1.0f + fabsf(smd->lambda) * wpaint);
            }
            else {
              EIG_linear_solver_matrix_add(
                  sys->context, i, i, 1.0f + fabsf(smd->lambda_border) * wpaint * 2.0f);
            }
          }
          else {
            w = sys->vweights[i] * sys->ring_areas[i];
            sys->vweights[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda) * wpaint / (4.0f * w);
            w = sys->vlengths[i];
            sys->vlengths[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda_border) * wpaint * 2.0f / w;

            if (sys->ne_ed_num[i] == sys->ne_fa_num[i]) {
              EIG_linear_solver_matrix_add(sys->context,
                                           i,
                                           i,
                                           1.0f + fabsf(smd->lambda) * wpaint /
                                                      (4.0f * sys->ring_areas[i]));
            }
            else {
              EIG_linear_solver_matrix_add(
                  sys->context, i, i, 1.0f + fabsf(smd->lambda_border) * wpaint * 2.0f);
            }
          }
        }
        else {
          EIG_linear_solver_matrix_add(sys->context, i, i, 1.0f);
        }
      }
    }

    if (iter == 0) {
      fill_laplacian_matrix(sys);
    }

    if (EIG_linear_solver_solve(sys->context)) {
      validate_solution(sys, smd->flag, smd->lambda, smd->lambda_border);
    }
  }
  EIG_linear_solver_delete(sys->context);
  sys->context = nullptr;

  delete_laplacian_system(sys);
}

static void init_data(ModifierData *md)
{
  LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(LaplacianSmoothModifierData), modifier);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*useRenderParams*/)
{
  LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *)md;
  short flag;

  flag = smd->flag & (MOD_LAPLACIANSMOOTH_X | MOD_LAPLACIANSMOOTH_Y | MOD_LAPLACIANSMOOTH_Z);

  /* disable if modifier is off for X, Y and Z or if factor is 0 */
  if (flag == 0) {
    return 1;
  }

  return 0;
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (smd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int verts_num)
{
  Mesh *mesh_src;

  if (verts_num == 0) {
    return;
  }

  mesh_src = MOD_deform_mesh_eval_get(ctx->object, nullptr, mesh, nullptr);

  laplaciansmoothModifier_do(
      (LaplacianSmoothModifierData *)md, ctx->object, mesh_src, vertexCos, verts_num);

  if (!ELEM(mesh_src, nullptr, mesh)) {
    BKE_id_free(nullptr, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          BMEditMesh *editData,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int verts_num)
{
  Mesh *mesh_src;

  if (verts_num == 0) {
    return;
  }

  mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, nullptr);

  /* TODO(@ideasman42): use edit-mode data only (remove this line). */
  if (mesh_src != nullptr) {
    BKE_mesh_wrapper_ensure_mdata(mesh_src);
  }

  laplaciansmoothModifier_do(
      (LaplacianSmoothModifierData *)md, ctx->object, mesh_src, vertexCos, verts_num);

  if (!ELEM(mesh_src, nullptr, mesh)) {
    BKE_id_free(nullptr, mesh_src);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "iterations", 0, nullptr, ICON_NONE);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Axis"));
  uiItemR(row, ptr, "use_x", toggles_flag, nullptr, ICON_NONE);
  uiItemR(row, ptr, "use_y", toggles_flag, nullptr, ICON_NONE);
  uiItemR(row, ptr, "use_z", toggles_flag, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "lambda_factor", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "lambda_border", 0, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "use_volume_preserve", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_normalized", 0, nullptr, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_LaplacianSmooth, panel_draw);
}

ModifierTypeInfo modifierType_LaplacianSmooth = {
    /*name*/ N_("LaplacianSmooth"),
    /*structName*/ "LaplacianSmoothModifierData",
    /*structSize*/ sizeof(LaplacianSmoothModifierData),
    /*srna*/ &RNA_LaplacianSmoothModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_SMOOTH,

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ deformVertsEM,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ init_data,
    /*requiredDataMask*/ required_data_mask,
    /*freeData*/ nullptr,
    /*isDisabled*/ is_disabled,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ nullptr,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
