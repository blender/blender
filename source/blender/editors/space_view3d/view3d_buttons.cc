/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BLI_array_utils.h"
#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "ANIM_bone_collections.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h" /* own include */

/* ******************* view3d space & buttons ************** */
enum {
  B_REDR = 2,
  B_TRANSFORM_PANEL_MEDIAN = 1008,
  B_TRANSFORM_PANEL_DIMS = 1009,
};

/* All must start w/ location */

struct TransformMedian_Generic {
  float location[3];
};

struct TransformMedian_Mesh {
  float location[3], bv_weight, v_crease, be_weight, skin[2], e_crease;
};

struct TransformMedian_Curve {
  float location[3], weight, b_weight, radius, tilt;
};

struct TransformMedian_Lattice {
  float location[3], weight;
};

union TransformMedian {
  TransformMedian_Generic generic;
  TransformMedian_Mesh mesh;
  TransformMedian_Curve curve;
  TransformMedian_Lattice lattice;
};

/* temporary struct for storing transform properties */

struct TransformProperties {
  float ob_obmat_orig[4][4];
  float ob_dims_orig[3];
  float ob_scale_orig[3];
  float ob_dims[3];
  /* Floats only (treated as an array). */
  TransformMedian ve_median, median;
  bool tag_for_update;
};

#define TRANSFORM_MEDIAN_ARRAY_LEN (sizeof(TransformMedian) / sizeof(float))

static TransformProperties *v3d_transform_props_ensure(View3D *v3d);

/* -------------------------------------------------------------------- */
/** \name Edit Mesh Partial Updates
 * \{ */

static void *editmesh_partial_update_begin_fn(bContext * /*C*/,
                                              const uiBlockInteraction_Params *params,
                                              void *arg1)
{
  const int retval_test = B_TRANSFORM_PANEL_MEDIAN;
  if (BLI_array_findindex(
          params->unique_retval_ids, params->unique_retval_ids_len, &retval_test) == -1)
  {
    return nullptr;
  }

  BMEditMesh *em = static_cast<BMEditMesh *>(arg1);

  int verts_mask_count = 0;
  BMIter iter;
  BMVert *eve;
  int i;

  BLI_bitmap *verts_mask = BLI_BITMAP_NEW(em->bm->totvert, __func__);
  BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
    if (!BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
      continue;
    }
    BLI_BITMAP_ENABLE(verts_mask, i);
    verts_mask_count += 1;
  }

  BMPartialUpdate_Params update_params{};
  update_params.do_tessellate = true;
  update_params.do_normals = true;
  BMPartialUpdate *bmpinfo = BM_mesh_partial_create_from_verts_group_single(
      em->bm, &update_params, verts_mask, verts_mask_count);

  MEM_freeN(verts_mask);

  return bmpinfo;
}

static void editmesh_partial_update_end_fn(bContext * /*C*/,
                                           const uiBlockInteraction_Params * /*params*/,
                                           void * /*arg1*/,
                                           void *user_data)
{
  BMPartialUpdate *bmpinfo = static_cast<BMPartialUpdate *>(user_data);
  if (bmpinfo == nullptr) {
    return;
  }
  BM_mesh_partial_destroy(bmpinfo);
}

static void editmesh_partial_update_update_fn(bContext *C,
                                              const uiBlockInteraction_Params * /*params*/,
                                              void *arg1,
                                              void *user_data)
{
  BMPartialUpdate *bmpinfo = static_cast<BMPartialUpdate *>(user_data);
  if (bmpinfo == nullptr) {
    return;
  }

  View3D *v3d = CTX_wm_view3d(C);
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  if (tfp->tag_for_update == false) {
    return;
  }
  tfp->tag_for_update = false;

  BMEditMesh *em = static_cast<BMEditMesh *>(arg1);

  BKE_editmesh_looptri_and_normals_calc_with_partial(em, bmpinfo);
}

/** \} */

/* Helper function to compute a median changed value,
 * when the value should be clamped in [0.0, 1.0].
 * Returns either 0.0, 1.0 (both can be applied directly), a positive scale factor
 * for scale down, or a negative one for scale up.
 */
static float compute_scale_factor(const float ve_median, const float median)
{
  if (ve_median <= 0.0f) {
    return 0.0f;
  }
  if (ve_median >= 1.0f) {
    return 1.0f;
  }

  /* Scale value to target median. */
  float median_new = ve_median;
  float median_orig = ve_median - median; /* Previous median value. */

  /* In case of floating point error. */
  CLAMP(median_orig, 0.0f, 1.0f);
  CLAMP(median_new, 0.0f, 1.0f);

  if (median_new <= median_orig) {
    /* Scale down. */
    return median_new / median_orig;
  }

  /* Scale up, negative to indicate it... */
  return -(1.0f - median_new) / (1.0f - median_orig);
}

/**
 * Apply helpers.
 * \note In case we only have one element,
 * copy directly the value instead of applying the diff or scale factor.
 * Avoids some glitches when going e.g. from 3 to 0.0001 (see #37327).
 */
static void apply_raw_diff(float *val, const int tot, const float ve_median, const float median)
{
  *val = (tot == 1) ? ve_median : (*val + median);
}

static void apply_raw_diff_v3(float val[3],
                              const int tot,
                              const float ve_median[3],
                              const float median[3])
{
  if (tot == 1) {
    copy_v3_v3(val, ve_median);
  }
  else {
    add_v3_v3(val, median);
  }
}

static void apply_scale_factor(
    float *val, const int tot, const float ve_median, const float median, const float sca)
{
  if (tot == 1 || ve_median == median) {
    *val = ve_median;
  }
  else {
    *val *= sca;
  }
}

static void apply_scale_factor_clamp(float *val,
                                     const int tot,
                                     const float ve_median,
                                     const float sca)
{
  if (tot == 1) {
    *val = ve_median;
    CLAMP(*val, 0.0f, 1.0f);
  }
  else if (ELEM(sca, 0.0f, 1.0f)) {
    *val = sca;
  }
  else {
    *val = (sca > 0.0f) ? (*val * sca) : (1.0f + ((1.0f - *val) * sca));
    CLAMP(*val, 0.0f, 1.0f);
  }
}

static TransformProperties *v3d_transform_props_ensure(View3D *v3d)
{
  if (v3d->runtime.properties_storage == nullptr) {
    v3d->runtime.properties_storage = MEM_callocN(sizeof(TransformProperties),
                                                  "TransformProperties");
  }
  return static_cast<TransformProperties *>(v3d->runtime.properties_storage);
}

/* is used for both read and write... */
static void v3d_editvertex_buts(uiLayout *layout, View3D *v3d, Object *ob, float lim)
{
  uiBlock *block = (layout) ? uiLayoutAbsoluteBlock(layout) : nullptr;
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);
  TransformMedian median_basis, ve_median_basis;
  int tot, totedgedata, totcurvedata, totlattdata, totcurvebweight;
  bool has_meshdata = false;
  bool has_skinradius = false;
  PointerRNA data_ptr;

  copy_vn_fl((float *)&median_basis, TRANSFORM_MEDIAN_ARRAY_LEN, 0.0f);
  tot = totedgedata = totcurvedata = totlattdata = totcurvebweight = 0;

  if (ob->type == OB_MESH) {
    TransformMedian_Mesh *median = &median_basis.mesh;
    Mesh *me = static_cast<Mesh *>(ob->data);
    BMEditMesh *em = me->edit_mesh;
    BMesh *bm = em->bm;
    BMVert *eve;
    BMEdge *eed;
    BMIter iter;

    const int cd_vert_bweight_offset = CustomData_get_offset_named(
        &bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
    const int cd_vert_crease_offset = CustomData_get_offset_named(
        &bm->vdata, CD_PROP_FLOAT, "crease_vert");
    const int cd_vert_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);
    const int cd_edge_bweight_offset = CustomData_get_offset_named(
        &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
    const int cd_edge_crease_offset = CustomData_get_offset_named(
        &bm->edata, CD_PROP_FLOAT, "crease_edge");

    has_skinradius = (cd_vert_skin_offset != -1);

    if (bm->totvertsel) {
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          tot++;
          add_v3_v3(median->location, eve->co);

          if (cd_vert_bweight_offset != -1) {
            median->bv_weight += BM_ELEM_CD_GET_FLOAT(eve, cd_vert_bweight_offset);
          }

          if (cd_vert_crease_offset != -1) {
            median->v_crease += BM_ELEM_CD_GET_FLOAT(eve, cd_vert_crease_offset);
          }

          if (has_skinradius) {
            MVertSkin *vs = static_cast<MVertSkin *>(
                BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset));
            add_v2_v2(median->skin, vs->radius); /* Third val not used currently. */
          }
        }
      }
    }

    if ((cd_edge_bweight_offset != -1) || (cd_edge_crease_offset != -1)) {
      if (bm->totedgesel) {
        BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
            if (cd_edge_bweight_offset != -1) {
              median->be_weight += BM_ELEM_CD_GET_FLOAT(eed, cd_edge_bweight_offset);
            }

            if (cd_edge_crease_offset != -1) {
              median->e_crease += BM_ELEM_CD_GET_FLOAT(eed, cd_edge_crease_offset);
            }

            totedgedata++;
          }
        }
      }
    }
    else {
      totedgedata = bm->totedgesel;
    }

    has_meshdata = (tot || totedgedata);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
    TransformMedian_Curve *median = &median_basis.curve;
    Curve *cu = static_cast<Curve *>(ob->data);
    BPoint *bp;
    BezTriple *bezt;
    int a;
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    StructRNA *seltype = nullptr;
    void *selp = nullptr;

    LISTBASE_FOREACH (Nurb *, nu, nurbs) {
      if (nu->type == CU_BEZIER) {
        bezt = nu->bezt;
        a = nu->pntsu;
        while (a--) {
          if (bezt->f2 & SELECT) {
            add_v3_v3(median->location, bezt->vec[1]);
            tot++;
            median->weight += bezt->weight;
            median->radius += bezt->radius;
            median->tilt += bezt->tilt;
            if (!totcurvedata) { /* I.e. first time... */
              selp = bezt;
              seltype = &RNA_BezierSplinePoint;
            }
            totcurvedata++;
          }
          else {
            if (bezt->f1 & SELECT) {
              add_v3_v3(median->location, bezt->vec[0]);
              tot++;
            }
            if (bezt->f3 & SELECT) {
              add_v3_v3(median->location, bezt->vec[2]);
              tot++;
            }
          }
          bezt++;
        }
      }
      else {
        bp = nu->bp;
        a = nu->pntsu * nu->pntsv;
        while (a--) {
          if (bp->f1 & SELECT) {
            add_v3_v3(median->location, bp->vec);
            median->b_weight += bp->vec[3];
            totcurvebweight++;
            tot++;
            median->weight += bp->weight;
            median->radius += bp->radius;
            median->tilt += bp->tilt;
            if (!totcurvedata) { /* I.e. first time... */
              selp = bp;
              seltype = &RNA_SplinePoint;
            }
            totcurvedata++;
          }
          bp++;
        }
      }
    }

    if (totcurvedata == 1) {
      RNA_pointer_create(&cu->id, seltype, selp, &data_ptr);
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = static_cast<Lattice *>(ob->data);
    TransformMedian_Lattice *median = &median_basis.lattice;
    BPoint *bp;
    int a;
    StructRNA *seltype = nullptr;
    void *selp = nullptr;

    a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
    bp = lt->editlatt->latt->def;
    while (a--) {
      if (bp->f1 & SELECT) {
        add_v3_v3(median->location, bp->vec);
        tot++;
        median->weight += bp->weight;
        if (!totlattdata) { /* I.e. first time... */
          selp = bp;
          seltype = &RNA_LatticePoint;
        }
        totlattdata++;
      }
      bp++;
    }

    if (totlattdata == 1) {
      RNA_pointer_create(&lt->id, seltype, selp, &data_ptr);
    }
  }

  if (tot == 0) {
    uiDefBut(block,
             UI_BTYPE_LABEL,
             0,
             IFACE_("Nothing selected"),
             0,
             130,
             200,
             20,
             nullptr,
             0,
             0,
             0,
             0,
             "");
    return;
  }

  /* Location, X/Y/Z */
  mul_v3_fl(median_basis.generic.location, 1.0f / float(tot));
  if (v3d->flag & V3D_GLOBAL_STATS) {
    mul_m4_v3(ob->object_to_world, median_basis.generic.location);
  }

  if (has_meshdata) {
    TransformMedian_Mesh *median = &median_basis.mesh;
    if (totedgedata) {
      median->e_crease /= float(totedgedata);
      median->be_weight /= float(totedgedata);
    }
    if (tot) {
      median->bv_weight /= float(tot);
      median->v_crease /= float(tot);
      if (has_skinradius) {
        median->skin[0] /= float(tot);
        median->skin[1] /= float(tot);
      }
    }
  }
  else if (totcurvedata) {
    TransformMedian_Curve *median = &median_basis.curve;
    if (totcurvebweight) {
      median->b_weight /= float(totcurvebweight);
    }
    median->weight /= float(totcurvedata);
    median->radius /= float(totcurvedata);
    median->tilt /= float(totcurvedata);
  }
  else if (totlattdata) {
    TransformMedian_Lattice *median = &median_basis.lattice;
    median->weight /= float(totlattdata);
  }

  if (block) { /* buttons */
    uiBut *but;
    int yi = 200;
    const float tilt_limit = DEG2RADF(21600.0f);
    const int butw = 200;
    const int buth = 20 * UI_SCALE_FAC;
    const int but_margin = 2;
    const char *c;

    memcpy(&tfp->ve_median, &median_basis, sizeof(tfp->ve_median));

    UI_block_align_begin(block);
    if (tot == 1) {
      if (totcurvedata) {
        /* Curve */
        c = IFACE_("Control Point:");
      }
      else {
        /* Mesh or lattice */
        c = IFACE_("Vertex:");
      }
    }
    else {
      c = IFACE_("Median:");
    }
    uiDefBut(block, UI_BTYPE_LABEL, 0, c, 0, yi -= buth, butw, buth, nullptr, 0, 0, 0, 0, "");

    UI_block_align_begin(block);

    /* Should be no need to translate these. */
    but = uiDefButF(block,
                    UI_BTYPE_NUM,
                    B_TRANSFORM_PANEL_MEDIAN,
                    IFACE_("X:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[0],
                    -lim,
                    lim,
                    0,
                    0,
                    "");
    UI_but_number_step_size_set(but, 10);
    UI_but_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    UI_but_unit_type_set(but, PROP_UNIT_LENGTH);
    but = uiDefButF(block,
                    UI_BTYPE_NUM,
                    B_TRANSFORM_PANEL_MEDIAN,
                    IFACE_("Y:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[1],
                    -lim,
                    lim,
                    0,
                    0,
                    "");
    UI_but_number_step_size_set(but, 10);
    UI_but_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    UI_but_unit_type_set(but, PROP_UNIT_LENGTH);
    but = uiDefButF(block,
                    UI_BTYPE_NUM,
                    B_TRANSFORM_PANEL_MEDIAN,
                    IFACE_("Z:"),
                    0,
                    yi -= buth,
                    butw,
                    buth,
                    &tfp->ve_median.generic.location[2],
                    -lim,
                    lim,
                    0,
                    0,
                    "");
    UI_but_number_step_size_set(but, 10);
    UI_but_number_precision_set(but, RNA_TRANSLATION_PREC_DEFAULT);
    UI_but_unit_type_set(but, PROP_UNIT_LENGTH);

    if (totcurvebweight == tot) {
      but = uiDefButF(block,
                      UI_BTYPE_NUM,
                      B_TRANSFORM_PANEL_MEDIAN,
                      IFACE_("W:"),
                      0,
                      yi -= buth,
                      butw,
                      buth,
                      &(tfp->ve_median.curve.b_weight),
                      0.01,
                      100.0,
                      0,
                      0,
                      "");
      UI_but_number_step_size_set(but, 1);
      UI_but_number_precision_set(but, 3);
    }

    UI_block_align_begin(block);
    uiDefButBitS(block,
                 UI_BTYPE_TOGGLE,
                 V3D_GLOBAL_STATS,
                 B_REDR,
                 IFACE_("Global"),
                 0,
                 yi -= buth + but_margin,
                 100,
                 buth,
                 &v3d->flag,
                 0,
                 0,
                 0,
                 0,
                 TIP_("Displays global values"));
    uiDefButBitS(block,
                 UI_BTYPE_TOGGLE_N,
                 V3D_GLOBAL_STATS,
                 B_REDR,
                 IFACE_("Local"),
                 100,
                 yi,
                 100,
                 buth,
                 &v3d->flag,
                 0,
                 0,
                 0,
                 0,
                 TIP_("Displays local values"));
    UI_block_align_end(block);

    /* Meshes... */
    if (has_meshdata) {
      TransformMedian_Mesh *ve_median = &tfp->ve_median.mesh;
      if (tot) {
        uiDefBut(block,
                 UI_BTYPE_LABEL,
                 0,
                 tot == 1 ? IFACE_("Vertex Data:") : IFACE_("Vertices Data:"),
                 0,
                 yi -= buth + but_margin,
                 butw,
                 buth,
                 nullptr,
                 0.0,
                 0.0,
                 0,
                 0,
                 "");
        /* customdata layer added on demand */
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        tot == 1 ? IFACE_("Bevel Weight:") : IFACE_("Mean Bevel Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->bv_weight,
                        0.0,
                        1.0,
                        0,
                        0,
                        TIP_("Vertex weight used by Bevel modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 2);
        /* customdata layer added on demand */
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        tot == 1 ? IFACE_("Vertex Crease:") : IFACE_("Mean Vertex Crease:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->v_crease,
                        0.0,
                        1.0,
                        0,
                        0,
                        TIP_("Weight used by the Subdivision Surface modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 2);
      }
      if (has_skinradius) {
        UI_block_align_begin(block);
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        tot == 1 ? IFACE_("Radius X:") : IFACE_("Mean Radius X:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->skin[0],
                        0.0,
                        100.0,
                        0,
                        0,
                        TIP_("X radius used by Skin modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        tot == 1 ? IFACE_("Radius Y:") : IFACE_("Mean Radius Y:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->skin[1],
                        0.0,
                        100.0,
                        0,
                        0,
                        TIP_("Y radius used by Skin modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        UI_block_align_end(block);
      }
      if (totedgedata) {
        uiDefBut(block,
                 UI_BTYPE_LABEL,
                 0,
                 totedgedata == 1 ? IFACE_("Edge Data:") : IFACE_("Edges Data:"),
                 0,
                 yi -= buth + but_margin,
                 butw,
                 buth,
                 nullptr,
                 0.0,
                 0.0,
                 0,
                 0,
                 "");
        /* customdata layer added on demand */
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        totedgedata == 1 ? IFACE_("Bevel Weight:") : IFACE_("Mean Bevel Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->be_weight,
                        0.0,
                        1.0,
                        0,
                        0,
                        TIP_("Edge weight used by Bevel modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 2);
        /* customdata layer added on demand */
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        totedgedata == 1 ? IFACE_("Crease:") : IFACE_("Mean Crease:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->e_crease,
                        0.0,
                        1.0,
                        0,
                        0,
                        TIP_("Weight used by the Subdivision Surface modifier"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 2);
      }
    }
    /* Curve... */
    else if (totcurvedata) {
      TransformMedian_Curve *ve_median = &tfp->ve_median.curve;
      if (totcurvedata == 1) {
        but = uiDefButR(block,
                        UI_BTYPE_NUM,
                        0,
                        IFACE_("Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &data_ptr,
                        "weight_softbody",
                        0,
                        0.0,
                        1.0,
                        0,
                        0,
                        nullptr);
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButR(block,
                        UI_BTYPE_NUM,
                        0,
                        IFACE_("Radius:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &data_ptr,
                        "radius",
                        0,
                        0.0,
                        100.0,
                        0,
                        0,
                        nullptr);
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButR(block,
                        UI_BTYPE_NUM,
                        0,
                        IFACE_("Tilt:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &data_ptr,
                        "tilt",
                        0,
                        -tilt_limit,
                        tilt_limit,
                        0,
                        0,
                        nullptr);
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
      }
      else if (totcurvedata > 1) {
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        IFACE_("Mean Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->weight,
                        0.0,
                        1.0,
                        0,
                        0,
                        TIP_("Weight used for Soft Body Goal"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        IFACE_("Mean Radius:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->radius,
                        0.0,
                        100.0,
                        0,
                        0,
                        TIP_("Radius of curve control points"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        IFACE_("Mean Tilt:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->tilt,
                        -tilt_limit,
                        tilt_limit,
                        0,
                        0,
                        TIP_("Tilt of curve control points"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
        UI_but_unit_type_set(but, PROP_UNIT_ROTATION);
      }
    }
    /* Lattice... */
    else if (totlattdata) {
      TransformMedian_Lattice *ve_median = &tfp->ve_median.lattice;
      if (totlattdata == 1) {
        uiDefButR(block,
                  UI_BTYPE_NUM,
                  0,
                  IFACE_("Weight:"),
                  0,
                  yi -= buth + but_margin,
                  butw,
                  buth,
                  &data_ptr,
                  "weight_softbody",
                  0,
                  0.0,
                  1.0,
                  0,
                  0,
                  nullptr);
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
      }
      else if (totlattdata > 1) {
        but = uiDefButF(block,
                        UI_BTYPE_NUM,
                        B_TRANSFORM_PANEL_MEDIAN,
                        IFACE_("Mean Weight:"),
                        0,
                        yi -= buth + but_margin,
                        butw,
                        buth,
                        &ve_median->weight,
                        0.0,
                        1.0,
                        0,
                        0,
                        TIP_("Weight used for Soft Body Goal"));
        UI_but_number_step_size_set(but, 1);
        UI_but_number_precision_set(but, 3);
      }
    }

    UI_block_align_end(block);

    if (ob->type == OB_MESH) {
      Mesh *me = static_cast<Mesh *>(ob->data);
      BMEditMesh *em = me->edit_mesh;
      if (em != nullptr) {
        uiBlockInteraction_CallbackData callback_data{};
        callback_data.begin_fn = editmesh_partial_update_begin_fn;
        callback_data.end_fn = editmesh_partial_update_end_fn;
        callback_data.update_fn = editmesh_partial_update_update_fn;
        callback_data.arg1 = em;
        UI_block_interaction_set(block, &callback_data);
      }
    }
  }
  else { /* apply */
    memcpy(&ve_median_basis, &tfp->ve_median, sizeof(tfp->ve_median));

    if (v3d->flag & V3D_GLOBAL_STATS) {
      invert_m4_m4(ob->world_to_object, ob->object_to_world);
      mul_m4_v3(ob->world_to_object, median_basis.generic.location);
      mul_m4_v3(ob->world_to_object, ve_median_basis.generic.location);
    }
    sub_vn_vnvn((float *)&median_basis,
                (float *)&ve_median_basis,
                (float *)&median_basis,
                TRANSFORM_MEDIAN_ARRAY_LEN);

    /* Note with a single element selected, we always do. */
    const bool apply_vcos = (tot == 1) || (len_squared_v3(median_basis.generic.location) != 0.0f);

    if ((ob->type == OB_MESH) &&
        (apply_vcos || median_basis.mesh.bv_weight || median_basis.mesh.v_crease ||
         median_basis.mesh.skin[0] || median_basis.mesh.skin[1] || median_basis.mesh.be_weight ||
         median_basis.mesh.e_crease))
    {
      const TransformMedian_Mesh *median = &median_basis.mesh, *ve_median = &ve_median_basis.mesh;
      Mesh *me = static_cast<Mesh *>(ob->data);
      BMEditMesh *em = me->edit_mesh;
      BMesh *bm = em->bm;
      BMIter iter;
      BMVert *eve;
      BMEdge *eed;

      int cd_vert_bweight_offset = -1;
      int cd_vert_crease_offset = -1;
      int cd_vert_skin_offset = -1;
      int cd_edge_bweight_offset = -1;
      int cd_edge_crease_offset = -1;

      float scale_bv_weight = 1.0f;
      float scale_v_crease = 1.0f;
      float scale_skin[2] = {1.0f, 1.0f};
      float scale_be_weight = 1.0f;
      float scale_e_crease = 1.0f;

      /* Vertices */

      if (apply_vcos || median->bv_weight || median->v_crease || median->skin[0] ||
          median->skin[1]) {
        if (median->bv_weight) {
          if (!CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert")) {
            BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
          }
          cd_vert_bweight_offset = CustomData_get_offset_named(
              &bm->vdata, CD_PROP_FLOAT, "bevel_weight_vert");
          BLI_assert(cd_vert_bweight_offset != -1);

          scale_bv_weight = compute_scale_factor(ve_median->bv_weight, median->bv_weight);
        }

        if (median->v_crease) {
          if (!CustomData_has_layer_named(&bm->vdata, CD_PROP_FLOAT, "crease_vert")) {
            BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLOAT, "crease_vert");
          }
          cd_vert_crease_offset = CustomData_get_offset_named(
              &bm->vdata, CD_PROP_FLOAT, "crease_vert");
          BLI_assert(cd_vert_crease_offset != -1);

          scale_v_crease = compute_scale_factor(ve_median->v_crease, median->v_crease);
        }

        for (int i = 0; i < 2; i++) {
          if (median->skin[i]) {
            cd_vert_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);
            BLI_assert(cd_vert_skin_offset != -1);

            if (ve_median->skin[i] != median->skin[i]) {
              scale_skin[i] = ve_median->skin[i] / (ve_median->skin[i] - median->skin[i]);
            }
          }
        }

        BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
          if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
            if (apply_vcos) {
              apply_raw_diff_v3(eve->co, tot, ve_median->location, median->location);
            }

            if (cd_vert_bweight_offset != -1) {
              float *b_weight = static_cast<float *>(
                  BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset));
              apply_scale_factor_clamp(b_weight, tot, ve_median->bv_weight, scale_bv_weight);
            }

            if (cd_vert_crease_offset != -1) {
              float *crease = static_cast<float *>(
                  BM_ELEM_CD_GET_VOID_P(eve, cd_vert_crease_offset));
              apply_scale_factor_clamp(crease, tot, ve_median->v_crease, scale_v_crease);
            }

            if (cd_vert_skin_offset != -1) {
              MVertSkin *vs = static_cast<MVertSkin *>(
                  BM_ELEM_CD_GET_VOID_P(eve, cd_vert_skin_offset));

              /* That one is not clamped to [0.0, 1.0]. */
              for (int i = 0; i < 2; i++) {
                if (median->skin[i] != 0.0f) {
                  apply_scale_factor(
                      &vs->radius[i], tot, ve_median->skin[i], median->skin[i], scale_skin[i]);
                }
              }
            }
          }
        }
      }

      if (apply_vcos) {
        /* Tell the update callback to run. */
        tfp->tag_for_update = true;
      }

      /* Edges */

      if (median->be_weight || median->e_crease) {
        if (median->be_weight) {
          if (!CustomData_has_layer_named(&bm->edata, CD_PROP_FLOAT, "bevel_weight_edge")) {
            BM_data_layer_add_named(bm, &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
          }
          cd_edge_bweight_offset = CustomData_get_offset_named(
              &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
          BLI_assert(cd_edge_bweight_offset != -1);

          scale_be_weight = compute_scale_factor(ve_median->be_weight, median->be_weight);
        }

        if (median->e_crease) {
          if (!CustomData_has_layer_named(&bm->edata, CD_PROP_FLOAT, "crease_edge")) {
            BM_data_layer_add_named(bm, &bm->edata, CD_PROP_FLOAT, "crease_edge");
          }
          cd_edge_crease_offset = CustomData_get_offset_named(
              &bm->edata, CD_PROP_FLOAT, "crease_edge");
          BLI_assert(cd_edge_crease_offset != -1);

          scale_e_crease = compute_scale_factor(ve_median->e_crease, median->e_crease);
        }

        BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
            if (median->be_weight != 0.0f) {
              float *b_weight = static_cast<float *>(
                  BM_ELEM_CD_GET_VOID_P(eed, cd_edge_bweight_offset));
              apply_scale_factor_clamp(b_weight, tot, ve_median->be_weight, scale_be_weight);
            }

            if (median->e_crease != 0.0f) {
              float *crease = static_cast<float *>(
                  BM_ELEM_CD_GET_VOID_P(eed, cd_edge_crease_offset));
              apply_scale_factor_clamp(crease, tot, ve_median->e_crease, scale_e_crease);
            }
          }
        }
      }
    }
    else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF) &&
             (apply_vcos || median_basis.curve.b_weight || median_basis.curve.weight ||
              median_basis.curve.radius || median_basis.curve.tilt))
    {
      const TransformMedian_Curve *median = &median_basis.curve,
                                  *ve_median = &ve_median_basis.curve;
      Curve *cu = static_cast<Curve *>(ob->data);
      BPoint *bp;
      BezTriple *bezt;
      int a;
      ListBase *nurbs = BKE_curve_editNurbs_get(cu);
      const float scale_w = compute_scale_factor(ve_median->weight, median->weight);

      LISTBASE_FOREACH (Nurb *, nu, nurbs) {
        if (nu->type == CU_BEZIER) {
          for (a = nu->pntsu, bezt = nu->bezt; a--; bezt++) {
            if (bezt->f2 & SELECT) {
              if (apply_vcos) {
                /* Here we always have to use the diff... :/
                 * Cannot avoid some glitches when going e.g. from 3 to 0.0001 (see #37327),
                 * unless we use doubles.
                 */
                add_v3_v3(bezt->vec[0], median->location);
                add_v3_v3(bezt->vec[1], median->location);
                add_v3_v3(bezt->vec[2], median->location);
              }
              if (median->weight) {
                apply_scale_factor_clamp(&bezt->weight, tot, ve_median->weight, scale_w);
              }
              if (median->radius) {
                apply_raw_diff(&bezt->radius, tot, ve_median->radius, median->radius);
              }
              if (median->tilt) {
                apply_raw_diff(&bezt->tilt, tot, ve_median->tilt, median->tilt);
              }
            }
            else if (apply_vcos) {
              /* Handles can only have their coordinates changed here. */
              if (bezt->f1 & SELECT) {
                apply_raw_diff_v3(bezt->vec[0], tot, ve_median->location, median->location);
              }
              if (bezt->f3 & SELECT) {
                apply_raw_diff_v3(bezt->vec[2], tot, ve_median->location, median->location);
              }
            }
          }
        }
        else {
          for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a--; bp++) {
            if (bp->f1 & SELECT) {
              if (apply_vcos) {
                apply_raw_diff_v3(bp->vec, tot, ve_median->location, median->location);
              }
              if (median->b_weight) {
                apply_raw_diff(&bp->vec[3], tot, ve_median->b_weight, median->b_weight);
              }
              if (median->weight) {
                apply_scale_factor_clamp(&bp->weight, tot, ve_median->weight, scale_w);
              }
              if (median->radius) {
                apply_raw_diff(&bp->radius, tot, ve_median->radius, median->radius);
              }
              if (median->tilt) {
                apply_raw_diff(&bp->tilt, tot, ve_median->tilt, median->tilt);
              }
            }
          }
        }
        if (CU_IS_2D(cu)) {
          BKE_nurb_project_2d(nu);
        }
        BKE_nurb_handles_test(nu, true, false); /* test for bezier too */
      }
    }
    else if ((ob->type == OB_LATTICE) && (apply_vcos || median_basis.lattice.weight)) {
      const TransformMedian_Lattice *median = &median_basis.lattice,
                                    *ve_median = &ve_median_basis.lattice;
      Lattice *lt = static_cast<Lattice *>(ob->data);
      BPoint *bp;
      int a;
      const float scale_w = compute_scale_factor(ve_median->weight, median->weight);

      a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
      bp = lt->editlatt->latt->def;
      while (a--) {
        if (bp->f1 & SELECT) {
          if (apply_vcos) {
            apply_raw_diff_v3(bp->vec, tot, ve_median->location, median->location);
          }
          if (median->weight) {
            apply_scale_factor_clamp(&bp->weight, tot, ve_median->weight, scale_w);
          }
        }
        bp++;
      }
    }

    /*      ED_undo_push(C, "Transform properties"); */
  }
}

#undef TRANSFORM_MEDIAN_ARRAY_LEN

static void v3d_object_dimension_buts(bContext *C, uiLayout *layout, View3D *v3d, Object *ob)
{
  uiBlock *block = (layout) ? uiLayoutAbsoluteBlock(layout) : nullptr;
  TransformProperties *tfp = v3d_transform_props_ensure(v3d);

  if (block) {
    BLI_assert(C == nullptr);
    int yi = 200;
    const int butw = 200;
    const int buth = 20 * UI_SCALE_FAC;

    BKE_object_dimensions_get(ob, tfp->ob_dims);
    copy_v3_v3(tfp->ob_dims_orig, tfp->ob_dims);
    copy_v3_v3(tfp->ob_scale_orig, ob->scale);
    copy_m4_m4(tfp->ob_obmat_orig, ob->object_to_world);

    uiDefBut(block,
             UI_BTYPE_LABEL,
             0,
             IFACE_("Dimensions:"),
             0,
             yi -= buth,
             butw,
             buth,
             nullptr,
             0,
             0,
             0,
             0,
             "");
    UI_block_align_begin(block);
    const float lim = FLT_MAX;
    for (int i = 0; i < 3; i++) {
      uiBut *but;
      const char text[3] = {char('X' + i), ':', '\0'};
      but = uiDefButF(block,
                      UI_BTYPE_NUM,
                      B_TRANSFORM_PANEL_DIMS,
                      text,
                      0,
                      yi -= buth,
                      butw,
                      buth,
                      &(tfp->ob_dims[i]),
                      0.0f,
                      lim,
                      0,
                      0,
                      "");
      UI_but_number_step_size_set(but, 10);
      UI_but_number_precision_set(but, 3);
      UI_but_unit_type_set(but, PROP_UNIT_LENGTH);
    }
    UI_block_align_end(block);
  }
  else { /* apply */
    int axis_mask = 0;
    for (int i = 0; i < 3; i++) {
      if (tfp->ob_dims[i] == tfp->ob_dims_orig[i]) {
        axis_mask |= (1 << i);
      }
    }
    BKE_object_dimensions_set_ex(
        ob, tfp->ob_dims, axis_mask, tfp->ob_scale_orig, tfp->ob_obmat_orig);

    PointerRNA obptr;
    RNA_id_pointer_create(&ob->id, &obptr);
    PropertyRNA *prop = RNA_struct_find_property(&obptr, "scale");
    RNA_property_update(C, &obptr, prop);
  }
}

#define B_VGRP_PNL_EDIT_SINGLE 8 /* or greater */

static void do_view3d_vgroup_buttons(bContext *C, void * /*arg*/, int event)
{
  if (event < B_VGRP_PNL_EDIT_SINGLE) {
    /* not for me */
    return;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  ED_vgroup_vert_active_mirror(ob, event - B_VGRP_PNL_EDIT_SINGLE);
  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
}

static bool view3d_panel_vgroup_poll(const bContext *C, PanelType * /*pt*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob && (BKE_object_is_in_editmode_vgroup(ob) || BKE_object_is_in_wpaint_select_vert(ob))) {
    MDeformVert *dvert_act = ED_mesh_active_dvert_get_only(ob);
    if (dvert_act) {
      return (dvert_act->totweight != 0);
    }
  }

  return false;
}

static void view3d_panel_vgroup(const bContext *C, Panel *panel)
{
  uiBlock *block = uiLayoutAbsoluteBlock(panel->layout);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  MDeformVert *dv;

  dv = ED_mesh_active_dvert_get_only(ob);

  if (dv && dv->totweight) {
    ToolSettings *ts = scene->toolsettings;

    wmOperatorType *ot;
    PointerRNA op_ptr, tools_ptr;
    PointerRNA *but_ptr;

    uiLayout *col, *bcol;
    uiLayout *row;
    uiBut *but;
    bDeformGroup *dg;
    uint i;
    int subset_count, vgroup_tot;
    const bool *vgroup_validmap;
    eVGroupSelect subset_type = eVGroupSelect(ts->vgroupsubset);
    int yco = 0;
    int lock_count = 0;

    UI_block_func_handle_set(block, do_view3d_vgroup_buttons, nullptr);

    bcol = uiLayoutColumn(panel->layout, true);
    row = uiLayoutRow(bcol, true); /* The filter button row */

    RNA_pointer_create(nullptr, &RNA_ToolSettings, ts, &tools_ptr);
    uiItemR(row, &tools_ptr, "vertex_group_subset", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

    col = uiLayoutColumn(bcol, true);

    vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
        ob, subset_type, &vgroup_tot, &subset_count);
    const ListBase *defbase = BKE_object_defgroup_list(ob);

    for (i = 0, dg = static_cast<bDeformGroup *>(defbase->first); dg; i++, dg = dg->next) {
      bool locked = (dg->flag & DG_LOCK_WEIGHT) != 0;
      if (vgroup_validmap[i]) {
        MDeformWeight *dw = BKE_defvert_find_index(dv, i);
        if (dw) {
          int x, xco = 0;
          int icon;
          uiLayout *split = uiLayoutSplit(col, 0.45, true);
          row = uiLayoutRow(split, true);

          /* The Weight Group Name */

          ot = WM_operatortype_find("OBJECT_OT_vertex_weight_set_active", true);
          but = uiDefButO_ptr(block,
                              UI_BTYPE_BUT,
                              ot,
                              WM_OP_EXEC_DEFAULT,
                              dg->name,
                              xco,
                              yco,
                              (x = UI_UNIT_X * 5),
                              UI_UNIT_Y,
                              "");
          but_ptr = UI_but_operator_ptr_get(but);
          RNA_int_set(but_ptr, "weight_group", i);
          UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
          if (BKE_object_defgroup_active_index_get(ob) != i + 1) {
            UI_but_flag_enable(but, UI_BUT_INACTIVE);
          }
          xco += x;

          row = uiLayoutRow(split, true);
          uiLayoutSetEnabled(row, !locked);

          /* The weight group value */
          /* To be reworked still */
          but = uiDefButF(block,
                          UI_BTYPE_NUM,
                          B_VGRP_PNL_EDIT_SINGLE + i,
                          "",
                          xco,
                          yco,
                          (x = UI_UNIT_X * 4),
                          UI_UNIT_Y,
                          &dw->weight,
                          0.0,
                          1.0,
                          0,
                          0,
                          "");
          UI_but_number_step_size_set(but, 1);
          UI_but_number_precision_set(but, 3);
          UI_but_drawflag_enable(but, UI_BUT_TEXT_LEFT);
          if (locked) {
            lock_count++;
          }
          xco += x;

          /* The weight group paste function */
          icon = (locked) ? ICON_BLANK1 : ICON_PASTEDOWN;
          uiItemFullO(row,
                      "OBJECT_OT_vertex_weight_paste",
                      "",
                      icon,
                      nullptr,
                      WM_OP_INVOKE_DEFAULT,
                      UI_ITEM_NONE,
                      &op_ptr);
          RNA_int_set(&op_ptr, "weight_group", i);

          /* The weight entry delete function */
          icon = (locked) ? ICON_LOCKED : ICON_X;
          uiItemFullO(row,
                      "OBJECT_OT_vertex_weight_delete",
                      "",
                      icon,
                      nullptr,
                      WM_OP_INVOKE_DEFAULT,
                      UI_ITEM_NONE,
                      &op_ptr);
          RNA_int_set(&op_ptr, "weight_group", i);

          yco -= UI_UNIT_Y;
        }
      }
    }
    MEM_freeN((void *)vgroup_validmap);

    yco -= 2;

    col = uiLayoutColumn(panel->layout, true);
    row = uiLayoutRow(col, true);

    ot = WM_operatortype_find("OBJECT_OT_vertex_weight_normalize_active_vertex", true);
    but = uiDefButO_ptr(
        block,
        UI_BTYPE_BUT,
        ot,
        WM_OP_EXEC_DEFAULT,
        "Normalize",
        0,
        yco,
        UI_UNIT_X * 5,
        UI_UNIT_Y,
        TIP_("Normalize weights of active vertex (if affected groups are unlocked)"));
    if (lock_count) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }

    ot = WM_operatortype_find("OBJECT_OT_vertex_weight_copy", true);
    but = uiDefButO_ptr(
        block,
        UI_BTYPE_BUT,
        ot,
        WM_OP_EXEC_DEFAULT,
        "Copy",
        UI_UNIT_X * 5,
        yco,
        UI_UNIT_X * 5,
        UI_UNIT_Y,
        TIP_("Copy active vertex to other selected vertices (if affected groups are unlocked)"));
    if (lock_count) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
  }
}

static void v3d_transform_butsR(uiLayout *layout, PointerRNA *ptr)
{
  uiLayout *split, *colsub;

  split = uiLayoutSplit(layout, 0.8f, false);

  if (ptr->type == &RNA_PoseBone) {
    PointerRNA boneptr;
    Bone *bone;

    boneptr = RNA_pointer_get(ptr, "bone");
    bone = static_cast<Bone *>(boneptr.data);
    uiLayoutSetActive(split, !(bone->parent && bone->flag & BONE_CONNECTED));
  }
  colsub = uiLayoutColumn(split, true);
  uiItemR(colsub, ptr, "location", UI_ITEM_NONE, nullptr, ICON_NONE);
  colsub = uiLayoutColumn(split, true);
  uiLayoutSetEmboss(colsub, UI_EMBOSS_NONE_OR_STATUS);
  uiItemL(colsub, "", ICON_NONE);
  uiItemR(colsub,
          ptr,
          "lock_location",
          UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
          "",
          ICON_DECORATE_UNLOCKED);

  split = uiLayoutSplit(layout, 0.8f, false);

  switch (RNA_enum_get(ptr, "rotation_mode")) {
    case ROT_MODE_QUAT: /* quaternion */
      colsub = uiLayoutColumn(split, true);
      uiItemR(colsub, ptr, "rotation_quaternion", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = uiLayoutColumn(split, true);
      uiLayoutSetEmboss(colsub, UI_EMBOSS_NONE_OR_STATUS);
      uiItemR(colsub, ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, IFACE_("4L"), ICON_NONE);
      if (RNA_boolean_get(ptr, "lock_rotations_4d")) {
        uiItemR(colsub,
                ptr,
                "lock_rotation_w",
                UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
                "",
                ICON_DECORATE_UNLOCKED);
      }
      else {
        uiItemL(colsub, "", ICON_NONE);
      }
      uiItemR(colsub,
              ptr,
              "lock_rotation",
              UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
              "",
              ICON_DECORATE_UNLOCKED);
      break;
    case ROT_MODE_AXISANGLE: /* axis angle */
      colsub = uiLayoutColumn(split, true);
      uiItemR(colsub, ptr, "rotation_axis_angle", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = uiLayoutColumn(split, true);
      uiLayoutSetEmboss(colsub, UI_EMBOSS_NONE_OR_STATUS);
      uiItemR(colsub, ptr, "lock_rotations_4d", UI_ITEM_R_TOGGLE, IFACE_("4L"), ICON_NONE);
      if (RNA_boolean_get(ptr, "lock_rotations_4d")) {
        uiItemR(colsub,
                ptr,
                "lock_rotation_w",
                UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
                "",
                ICON_DECORATE_UNLOCKED);
      }
      else {
        uiItemL(colsub, "", ICON_NONE);
      }
      uiItemR(colsub,
              ptr,
              "lock_rotation",
              UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
              "",
              ICON_DECORATE_UNLOCKED);
      break;
    default: /* euler rotations */
      colsub = uiLayoutColumn(split, true);
      uiItemR(colsub, ptr, "rotation_euler", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
      colsub = uiLayoutColumn(split, true);
      uiLayoutSetEmboss(colsub, UI_EMBOSS_NONE_OR_STATUS);
      uiItemL(colsub, "", ICON_NONE);
      uiItemR(colsub,
              ptr,
              "lock_rotation",
              UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
              "",
              ICON_DECORATE_UNLOCKED);
      break;
  }
  uiItemR(layout, ptr, "rotation_mode", UI_ITEM_NONE, "", ICON_NONE);

  split = uiLayoutSplit(layout, 0.8f, false);
  colsub = uiLayoutColumn(split, true);
  uiItemR(colsub, ptr, "scale", UI_ITEM_NONE, nullptr, ICON_NONE);
  colsub = uiLayoutColumn(split, true);
  uiLayoutSetEmboss(colsub, UI_EMBOSS_NONE_OR_STATUS);
  uiItemL(colsub, "", ICON_NONE);
  uiItemR(colsub,
          ptr,
          "lock_scale",
          UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
          "",
          ICON_DECORATE_UNLOCKED);
}

static void v3d_posearmature_buts(uiLayout *layout, Object *ob)
{
  bPoseChannel *pchan;
  PointerRNA pchanptr;
  uiLayout *col;

  pchan = BKE_pose_channel_active_if_layer_visible(ob);

  if (!pchan) {
    uiItemL(layout, IFACE_("No Bone Active"), ICON_NONE);
    return;
  }

  RNA_pointer_create(&ob->id, &RNA_PoseBone, pchan, &pchanptr);

  col = uiLayoutColumn(layout, false);

  /* XXX: RNA buts show data in native types (i.e. quaternion, 4-component axis/angle, etc.)
   * but old-school UI shows in eulers always. Do we want to be able to still display in Eulers?
   * Maybe needs RNA/UI options to display rotations as different types. */
  v3d_transform_butsR(col, &pchanptr);
}

static void v3d_editarmature_buts(uiLayout *layout, Object *ob)
{
  bArmature *arm = static_cast<bArmature *>(ob->data);
  EditBone *ebone;
  uiLayout *col;
  PointerRNA eboneptr;

  ebone = arm->act_edbone;

  if (!ebone || !ANIM_bonecoll_is_visible_editbone(arm, ebone)) {
    uiItemL(layout, IFACE_("Nothing selected"), ICON_NONE);
    return;
  }

  RNA_pointer_create(&arm->id, &RNA_EditBone, ebone, &eboneptr);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &eboneptr, "head", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (ebone->parent && ebone->flag & BONE_CONNECTED) {
    PointerRNA parptr = RNA_pointer_get(&eboneptr, "parent");
    uiItemR(col, &parptr, "tail_radius", UI_ITEM_NONE, IFACE_("Radius (Parent)"), ICON_NONE);
  }
  else {
    uiItemR(col, &eboneptr, "head_radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);
  }

  uiItemR(col, &eboneptr, "tail", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, &eboneptr, "tail_radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);

  uiItemR(col, &eboneptr, "roll", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, &eboneptr, "length", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, &eboneptr, "envelope_distance", UI_ITEM_NONE, IFACE_("Envelope"), ICON_NONE);
}

static void v3d_editmetaball_buts(uiLayout *layout, Object *ob)
{
  PointerRNA mbptr, ptr;
  MetaBall *mball = static_cast<MetaBall *>(ob->data);
  uiLayout *col;

  if (!mball || !(mball->lastelem)) {
    uiItemL(layout, IFACE_("Nothing selected"), ICON_NONE);
    return;
  }

  RNA_pointer_create(&mball->id, &RNA_MetaBall, mball, &mbptr);

  RNA_pointer_create(&mball->id, &RNA_MetaElement, mball->lastelem, &ptr);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &ptr, "co", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(col, &ptr, "radius", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, &ptr, "stiffness", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(col, &ptr, "type", UI_ITEM_NONE, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  switch (RNA_enum_get(&ptr, "type")) {
    case MB_BALL:
      break;
    case MB_CUBE:
      uiItemL(col, IFACE_("Size:"), ICON_NONE);
      uiItemR(col, &ptr, "size_x", UI_ITEM_NONE, "X", ICON_NONE);
      uiItemR(col, &ptr, "size_y", UI_ITEM_NONE, "Y", ICON_NONE);
      uiItemR(col, &ptr, "size_z", UI_ITEM_NONE, "Z", ICON_NONE);
      break;
    case MB_TUBE:
      uiItemL(col, IFACE_("Size:"), ICON_NONE);
      uiItemR(col, &ptr, "size_x", UI_ITEM_NONE, "X", ICON_NONE);
      break;
    case MB_PLANE:
      uiItemL(col, IFACE_("Size:"), ICON_NONE);
      uiItemR(col, &ptr, "size_x", UI_ITEM_NONE, "X", ICON_NONE);
      uiItemR(col, &ptr, "size_y", UI_ITEM_NONE, "Y", ICON_NONE);
      break;
    case MB_ELIPSOID:
      uiItemL(col, IFACE_("Size:"), ICON_NONE);
      uiItemR(col, &ptr, "size_x", UI_ITEM_NONE, "X", ICON_NONE);
      uiItemR(col, &ptr, "size_y", UI_ITEM_NONE, "Y", ICON_NONE);
      uiItemR(col, &ptr, "size_z", UI_ITEM_NONE, "Z", ICON_NONE);
      break;
  }
}

static void do_view3d_region_buttons(bContext *C, void * /*index*/, int event)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  switch (event) {

    case B_REDR:
      ED_area_tag_redraw(CTX_wm_area(C));
      return; /* no notifier! */

    case B_TRANSFORM_PANEL_MEDIAN:
      if (ob) {
        v3d_editvertex_buts(nullptr, v3d, ob, 1.0);
        DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_GEOMETRY);
      }
      break;
    case B_TRANSFORM_PANEL_DIMS:
      if (ob) {
        v3d_object_dimension_buts(C, nullptr, v3d, ob);
      }
      break;
  }

  /* default for now */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
}

static bool view3d_panel_transform_poll(const bContext *C, PanelType * /*pt*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  return (BKE_view_layer_active_base_get(view_layer) != nullptr);
}

static void view3d_panel_transform(const bContext *C, Panel *panel)
{
  uiBlock *block;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);
  uiLayout *col;

  block = uiLayoutGetBlock(panel->layout);
  UI_block_func_handle_set(block, do_view3d_region_buttons, nullptr);

  col = uiLayoutColumn(panel->layout, false);

  if (ob == obedit) {
    if (ob->type == OB_ARMATURE) {
      v3d_editarmature_buts(col, ob);
    }
    else if (ob->type == OB_MBALL) {
      v3d_editmetaball_buts(col, ob);
    }
    else {
      View3D *v3d = CTX_wm_view3d(C);
      v3d_editvertex_buts(col, v3d, ob, FLT_MAX);
    }
  }
  else if (ob->mode & OB_MODE_POSE) {
    v3d_posearmature_buts(col, ob);
  }
  else {
    PointerRNA obptr;

    RNA_id_pointer_create(&ob->id, &obptr);
    v3d_transform_butsR(col, &obptr);

    /* Dimensions and editmode are mostly the same check. */
    if (OB_TYPE_SUPPORT_EDITMODE(ob->type) || ELEM(ob->type, OB_VOLUME, OB_CURVES, OB_POINTCLOUD))
    {
      View3D *v3d = CTX_wm_view3d(C);
      v3d_object_dimension_buts(nullptr, col, v3d, ob);
    }
  }
}

static void hide_collections_menu_draw(const bContext *C, Menu *menu)
{
  ED_collection_hide_menu_draw(C, menu->layout);
}

void view3d_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = static_cast<PanelType *>(MEM_callocN(sizeof(PanelType), "spacetype view3d panel object"));
  STRNCPY(pt->idname, "VIEW3D_PT_transform");
  STRNCPY(pt->label, N_("Transform")); /* XXX C panels unavailable through RNA bpy.types! */
  STRNCPY(pt->category, "Item");
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = view3d_panel_transform;
  pt->poll = view3d_panel_transform_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = static_cast<PanelType *>(MEM_callocN(sizeof(PanelType), "spacetype view3d panel vgroup"));
  STRNCPY(pt->idname, "VIEW3D_PT_vgroup");
  STRNCPY(pt->label, N_("Vertex Weights")); /* XXX C panels unavailable through RNA bpy.types! */
  STRNCPY(pt->category, "Item");
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = view3d_panel_vgroup;
  pt->poll = view3d_panel_vgroup_poll;
  BLI_addtail(&art->paneltypes, pt);

  MenuType *mt;

  mt = static_cast<MenuType *>(MEM_callocN(sizeof(MenuType), "spacetype view3d menu collections"));
  STRNCPY(mt->idname, "VIEW3D_MT_collection");
  STRNCPY(mt->label, N_("Collection"));
  STRNCPY(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = hide_collections_menu_draw;
  WM_menutype_add(mt);
}

static int view3d_object_mode_menu(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "No active object found");
    return OPERATOR_CANCELLED;
  }
  if (((ob->mode & OB_MODE_EDIT) == 0) && ELEM(ob->type, OB_ARMATURE)) {
    ED_object_mode_set(C, (ob->mode == OB_MODE_OBJECT) ? OB_MODE_POSE : OB_MODE_OBJECT);
    return OPERATOR_CANCELLED;
  }

  UI_pie_menu_invoke(C, "VIEW3D_MT_object_mode_pie", CTX_wm_window(C)->eventstate);
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_object_mode_pie_or_toggle(wmOperatorType *ot)
{
  ot->name = "Object Mode Menu";
  ot->idname = "VIEW3D_OT_object_mode_pie_or_toggle";

  ot->exec = view3d_object_mode_menu;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}
