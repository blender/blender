/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

static void generate_vert_coordinates(Mesh *mesh,
                                      Object *ob,
                                      Object *ob_center,
                                      const float offset[3],
                                      const int num_verts,
                                      float (*r_cos)[3],
                                      float r_size[3])
{
  float min_co[3], max_co[3];
  float diff[3];
  bool do_diff = false;

  INIT_MINMAX(min_co, max_co);

  MVert *mv = mesh->mvert;
  for (int i = 0; i < mesh->totvert; i++, mv++) {
    copy_v3_v3(r_cos[i], mv->co);
    if (r_size != NULL && ob_center == NULL) {
      minmax_v3v3_v3(min_co, max_co, r_cos[i]);
    }
  }

  /* Get size (i.e. deformation of the spheroid generating normals),
   * either from target object, or own geometry. */
  if (r_size != NULL) {
    if (ob_center != NULL) {
      /* Using 'scale' as 'size' here. The input object is typically an empty
       * who's scale is used to define an ellipsoid instead of a simple sphere. */

      /* Not we are not interested in signs here - they are even troublesome actually,
       * due to security clamping! */
      abs_v3_v3(r_size, ob_center->scale);
    }
    else {
      /* Set size. */
      sub_v3_v3v3(r_size, max_co, min_co);
    }

    /* Error checks - we do not want one or more of our sizes to be null! */
    if (is_zero_v3(r_size)) {
      r_size[0] = r_size[1] = r_size[2] = 1.0f;
    }
    else {
      CLAMP_MIN(r_size[0], FLT_EPSILON);
      CLAMP_MIN(r_size[1], FLT_EPSILON);
      CLAMP_MIN(r_size[2], FLT_EPSILON);
    }
  }

  if (ob_center != NULL) {
    float inv_obmat[4][4];

    /* Translate our coordinates so that center of ob_center is at (0, 0, 0). */
    /* Get ob_center (world) coordinates in ob local coordinates.
     * No need to take into account ob_center's space here, see T44027. */
    invert_m4_m4(inv_obmat, ob->obmat);
    mul_v3_m4v3(diff, inv_obmat, ob_center->obmat[3]);
    negate_v3(diff);

    do_diff = true;
  }
  else if (offset != NULL && !is_zero_v3(offset)) {
    negate_v3_v3(diff, offset);

    do_diff = true;
  }
  /* Else, no need to change coordinates! */

  if (do_diff) {
    int i = num_verts;
    while (i--) {
      add_v3_v3(r_cos[i], diff);
    }
  }
}

/* Note this modifies nos_new in-place. */
static void mix_normals(const float mix_factor,
                        MDeformVert *dvert,
                        const int defgrp_index,
                        const bool use_invert_vgroup,
                        const float mix_limit,
                        const short mix_mode,
                        const int num_verts,
                        MLoop *mloop,
                        float (*nos_old)[3],
                        float (*nos_new)[3],
                        const int num_loops)
{
  /* Mix with org normals... */
  float *facs = NULL, *wfac;
  float(*no_new)[3], (*no_old)[3];
  int i;

  if (dvert) {
    facs = MEM_malloc_arrayN((size_t)num_loops, sizeof(*facs), __func__);
    BKE_defvert_extract_vgroup_to_loopweights(
        dvert, defgrp_index, num_verts, mloop, num_loops, facs, use_invert_vgroup);
  }

  for (i = num_loops, no_new = nos_new, no_old = nos_old, wfac = facs; i--;
       no_new++, no_old++, wfac++) {
    const float fac = facs ? *wfac * mix_factor : mix_factor;

    switch (mix_mode) {
      case MOD_NORMALEDIT_MIX_ADD:
        add_v3_v3(*no_new, *no_old);
        normalize_v3(*no_new);
        break;
      case MOD_NORMALEDIT_MIX_SUB:
        sub_v3_v3(*no_new, *no_old);
        normalize_v3(*no_new);
        break;
      case MOD_NORMALEDIT_MIX_MUL:
        mul_v3_v3(*no_new, *no_old);
        normalize_v3(*no_new);
        break;
      case MOD_NORMALEDIT_MIX_COPY:
        break;
    }

    interp_v3_v3v3_slerp_safe(
        *no_new,
        *no_old,
        *no_new,
        (mix_limit < (float)M_PI) ? min_ff(fac, mix_limit / angle_v3v3(*no_new, *no_old)) : fac);
  }

  MEM_SAFE_FREE(facs);
}

/* Check poly normals and new loop normals are compatible, otherwise flip polygons
 * (and invert matching poly normals). */
static bool polygons_check_flip(MLoop *mloop,
                                float (*nos)[3],
                                CustomData *ldata,
                                MPoly *mpoly,
                                float (*polynors)[3],
                                const int num_polys)
{
  MPoly *mp;
  MDisps *mdisp = CustomData_get_layer(ldata, CD_MDISPS);
  int i;
  bool flipped = false;

  for (i = 0, mp = mpoly; i < num_polys; i++, mp++) {
    float norsum[3] = {0.0f};
    float(*no)[3];
    int j;

    for (j = 0, no = &nos[mp->loopstart]; j < mp->totloop; j++, no++) {
      add_v3_v3(norsum, *no);
    }

    if (!normalize_v3(norsum)) {
      continue;
    }

    /* If average of new loop normals is opposed to polygon normal, flip polygon. */
    if (dot_v3v3(polynors[i], norsum) < 0.0f) {
      BKE_mesh_polygon_flip_ex(mp, mloop, ldata, nos, mdisp, true);
      negate_v3(polynors[i]);
      flipped = true;
    }
  }

  return flipped;
}

static void normalEditModifier_do_radial(NormalEditModifierData *enmd,
                                         const ModifierEvalContext *UNUSED(ctx),
                                         Object *ob,
                                         Mesh *mesh,
                                         short (*clnors)[2],
                                         float (*loopnors)[3],
                                         float (*polynors)[3],
                                         const short mix_mode,
                                         const float mix_factor,
                                         const float mix_limit,
                                         MDeformVert *dvert,
                                         const int defgrp_index,
                                         const bool use_invert_vgroup,
                                         MVert *mvert,
                                         const int num_verts,
                                         MEdge *medge,
                                         const int num_edges,
                                         MLoop *mloop,
                                         const int num_loops,
                                         MPoly *mpoly,
                                         const int num_polys)
{
  Object *ob_target = enmd->target;

  const bool do_polynors_fix = (enmd->flag & MOD_NORMALEDIT_NO_POLYNORS_FIX) == 0;
  int i;

  float(*cos)[3] = MEM_malloc_arrayN((size_t)num_verts, sizeof(*cos), __func__);
  float(*nos)[3] = MEM_malloc_arrayN((size_t)num_loops, sizeof(*nos), __func__);
  float size[3];

  BLI_bitmap *done_verts = BLI_BITMAP_NEW((size_t)num_verts, __func__);

  generate_vert_coordinates(mesh, ob, ob_target, enmd->offset, num_verts, cos, size);

  /**
   * size gives us our spheroid coefficients ``(A, B, C)``.
   * Then, we want to find out for each vert its (a, b, c) triple (proportional to (A, B, C) one).
   *
   * Ellipsoid basic equation: ``(x^2/a^2) + (y^2/b^2) + (z^2/c^2) = 1.``
   * Since we want to find (a, b, c) matching this equation and proportional to (A, B, C),
   * we can do:
   * <pre>
   *     m = B / A
   *     n = C / A
   * </pre>
   *
   * hence:
   * <pre>
   *     (x^2/a^2) + (y^2/b^2) + (z^2/c^2) = 1
   *  -> b^2*c^2*x^2 + a^2*c^2*y^2 + a^2*b^2*z^2 = a^2*b^2*c^2
   *     b = ma
   *     c = na
   *  -> m^2*a^2*n^2*a^2*x^2 + a^2*n^2*a^2*y^2 + a^2*m^2*a^2*z^2 = a^2*m^2*a^2*n^2*a^2
   *  -> m^2*n^2*a^4*x^2 + n^2*a^4*y^2 + m^2*a^4*z^2 = m^2*n^2*a^6
   *  -> a^2 = (m^2*n^2*x^2 + n^2y^2 + m^2z^2) / (m^2*n^2) = x^2 + (y^2 / m^2) + (z^2 / n^2)
   *  -> b^2 = (m^2*n^2*x^2 + n^2y^2 + m^2z^2) / (n^2)     = (m^2 * x^2) + y^2 + (m^2 * z^2 / n^2)
   *  -> c^2 = (m^2*n^2*x^2 + n^2y^2 + m^2z^2) / (m^2)     = (n^2 * x^2) + (n^2 * y^2 / m^2) + z^2
   * </pre>
   *
   * All we have to do now is compute normal of the spheroid at that point:
   * <pre>
   *     n = (x / a^2, y / b^2, z / c^2)
   * </pre>
   * And we are done!
   */
  {
    const float a = size[0], b = size[1], c = size[2];
    const float m2 = (b * b) / (a * a);
    const float n2 = (c * c) / (a * a);

    MLoop *ml;
    float(*no)[3];

    /* We reuse cos to now store the ellipsoid-normal of the verts! */
    for (i = num_loops, ml = mloop, no = nos; i--; ml++, no++) {
      const int vidx = ml->v;
      float *co = cos[vidx];

      if (!BLI_BITMAP_TEST(done_verts, vidx)) {
        const float x2 = co[0] * co[0];
        const float y2 = co[1] * co[1];
        const float z2 = co[2] * co[2];
        const float a2 = x2 + (y2 / m2) + (z2 / n2);
        const float b2 = (m2 * x2) + y2 + (m2 * z2 / n2);
        const float c2 = (n2 * x2) + (n2 * y2 / m2) + z2;

        co[0] /= a2;
        co[1] /= b2;
        co[2] /= c2;
        normalize_v3(co);

        BLI_BITMAP_ENABLE(done_verts, vidx);
      }
      copy_v3_v3(*no, co);
    }
  }

  if (loopnors) {
    mix_normals(mix_factor,
                dvert,
                defgrp_index,
                use_invert_vgroup,
                mix_limit,
                mix_mode,
                num_verts,
                mloop,
                loopnors,
                nos,
                num_loops);
  }

  if (do_polynors_fix &&
      polygons_check_flip(mloop, nos, &mesh->ldata, mpoly, polynors, num_polys)) {
    /* XXX TODO is this still needed? */
    // mesh->dirty |= DM_DIRTY_TESS_CDLAYERS;
    /* We need to recompute vertex normals! */
    BKE_mesh_calc_normals(mesh);
  }

  BKE_mesh_normals_loop_custom_set(mvert,
                                   num_verts,
                                   medge,
                                   num_edges,
                                   mloop,
                                   nos,
                                   num_loops,
                                   mpoly,
                                   (const float(*)[3])polynors,
                                   num_polys,
                                   clnors);

  MEM_freeN(cos);
  MEM_freeN(nos);
  MEM_freeN(done_verts);
}

static void normalEditModifier_do_directional(NormalEditModifierData *enmd,
                                              const ModifierEvalContext *UNUSED(ctx),
                                              Object *ob,
                                              Mesh *mesh,
                                              short (*clnors)[2],
                                              float (*loopnors)[3],
                                              float (*polynors)[3],
                                              const short mix_mode,
                                              const float mix_factor,
                                              const float mix_limit,
                                              MDeformVert *dvert,
                                              const int defgrp_index,
                                              const bool use_invert_vgroup,
                                              MVert *mvert,
                                              const int num_verts,
                                              MEdge *medge,
                                              const int num_edges,
                                              MLoop *mloop,
                                              const int num_loops,
                                              MPoly *mpoly,
                                              const int num_polys)
{
  Object *ob_target = enmd->target;

  const bool do_polynors_fix = (enmd->flag & MOD_NORMALEDIT_NO_POLYNORS_FIX) == 0;
  const bool use_parallel_normals = (enmd->flag & MOD_NORMALEDIT_USE_DIRECTION_PARALLEL) != 0;

  float(*nos)[3] = MEM_malloc_arrayN((size_t)num_loops, sizeof(*nos), __func__);

  float target_co[3];
  int i;

  /* Get target's center coordinates in ob local coordinates. */
  float mat[4][4];

  invert_m4_m4(mat, ob->obmat);
  mul_m4_m4m4(mat, mat, ob_target->obmat);
  copy_v3_v3(target_co, mat[3]);

  if (use_parallel_normals) {
    float no[3];

    sub_v3_v3v3(no, target_co, enmd->offset);
    normalize_v3(no);

    for (i = num_loops; i--;) {
      copy_v3_v3(nos[i], no);
    }
  }
  else {
    float(*cos)[3] = MEM_malloc_arrayN((size_t)num_verts, sizeof(*cos), __func__);
    generate_vert_coordinates(mesh, ob, ob_target, NULL, num_verts, cos, NULL);

    BLI_bitmap *done_verts = BLI_BITMAP_NEW((size_t)num_verts, __func__);
    MLoop *ml;
    float(*no)[3];

    /* We reuse cos to now store the 'to target' normal of the verts! */
    for (i = num_loops, no = nos, ml = mloop; i--; no++, ml++) {
      const int vidx = ml->v;
      float *co = cos[vidx];

      if (!BLI_BITMAP_TEST(done_verts, vidx)) {
        sub_v3_v3v3(co, target_co, co);
        normalize_v3(co);

        BLI_BITMAP_ENABLE(done_verts, vidx);
      }

      copy_v3_v3(*no, co);
    }

    MEM_freeN(done_verts);
    MEM_freeN(cos);
  }

  if (loopnors) {
    mix_normals(mix_factor,
                dvert,
                defgrp_index,
                use_invert_vgroup,
                mix_limit,
                mix_mode,
                num_verts,
                mloop,
                loopnors,
                nos,
                num_loops);
  }

  if (do_polynors_fix &&
      polygons_check_flip(mloop, nos, &mesh->ldata, mpoly, polynors, num_polys)) {
    mesh->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  }

  BKE_mesh_normals_loop_custom_set(mvert,
                                   num_verts,
                                   medge,
                                   num_edges,
                                   mloop,
                                   nos,
                                   num_loops,
                                   mpoly,
                                   (const float(*)[3])polynors,
                                   num_polys,
                                   clnors);

  MEM_freeN(nos);
}

static bool is_valid_target(NormalEditModifierData *enmd)
{
  if (enmd->mode == MOD_NORMALEDIT_MODE_RADIAL) {
    return true;
  }
  else if ((enmd->mode == MOD_NORMALEDIT_MODE_DIRECTIONAL) && enmd->target) {
    return true;
  }
  BKE_modifier_set_error((ModifierData *)enmd, "Invalid target settings");
  return false;
}

static Mesh *normalEditModifier_do(NormalEditModifierData *enmd,
                                   const ModifierEvalContext *ctx,
                                   Object *ob,
                                   Mesh *mesh)
{
  const bool use_invert_vgroup = ((enmd->flag & MOD_NORMALEDIT_INVERT_VGROUP) != 0);
  const bool use_current_clnors = !((enmd->mix_mode == MOD_NORMALEDIT_MIX_COPY) &&
                                    (enmd->mix_factor == 1.0f) && (enmd->defgrp_name[0] == '\0') &&
                                    (enmd->mix_limit == (float)M_PI));

  /* Do not run that modifier at all if autosmooth is disabled! */
  if (!is_valid_target(enmd) || mesh->totloop == 0) {
    return mesh;
  }

  /* XXX TODO(Rohan Rathi):
   * Once we fully switch to Mesh evaluation of modifiers,
   * we can expect to get that flag from the COW copy.
   * But for now, it is lost in the DM intermediate step,
   * so we need to directly check orig object's data. */
#if 0
  if (!(mesh->flag & ME_AUTOSMOOTH))
#else
  if (!(((Mesh *)ob->data)->flag & ME_AUTOSMOOTH))
#endif
  {
    BKE_modifier_set_error((ModifierData *)enmd, "Enable 'Auto Smooth' in Object Data Properties");
    return mesh;
  }

  Mesh *result;
  if (mesh->medge == ((Mesh *)ob->data)->medge) {
    /* We need to duplicate data here, otherwise setting custom normals
     * (which may also affect sharp edges) could
     * modify original mesh, see T43671. */
    BKE_id_copy_ex(NULL, &mesh->id, (ID **)&result, LIB_ID_COPY_LOCALIZE);
  }
  else {
    result = mesh;
  }

  const int num_verts = result->totvert;
  const int num_edges = result->totedge;
  const int num_loops = result->totloop;
  const int num_polys = result->totpoly;
  MVert *mvert = result->mvert;
  MEdge *medge = result->medge;
  MLoop *mloop = result->mloop;
  MPoly *mpoly = result->mpoly;

  int defgrp_index;
  MDeformVert *dvert;

  float(*loopnors)[3] = NULL;
  short(*clnors)[2] = NULL;

  float(*polynors)[3];

  CustomData *ldata = &result->ldata;

  /* Compute poly (always needed) and vert normals. */
  CustomData *pdata = &result->pdata;
  polynors = CustomData_get_layer(pdata, CD_NORMAL);
  if (!polynors) {
    polynors = CustomData_add_layer(pdata, CD_NORMAL, CD_CALLOC, NULL, num_polys);
    CustomData_set_layer_flag(pdata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }
  BKE_mesh_calc_normals_poly(mvert,
                             NULL,
                             num_verts,
                             mloop,
                             mpoly,
                             num_loops,
                             num_polys,
                             polynors,
                             (result->runtime.cd_dirty_vert & CD_MASK_NORMAL) ? false : true);

  result->runtime.cd_dirty_vert &= ~CD_MASK_NORMAL;

  clnors = CustomData_get_layer(ldata, CD_CUSTOMLOOPNORMAL);
  if (use_current_clnors) {
    clnors = CustomData_duplicate_referenced_layer(ldata, CD_CUSTOMLOOPNORMAL, num_loops);
    loopnors = MEM_malloc_arrayN((size_t)num_loops, sizeof(*loopnors), __func__);

    BKE_mesh_normals_loop_split(mvert,
                                num_verts,
                                medge,
                                num_edges,
                                mloop,
                                loopnors,
                                num_loops,
                                mpoly,
                                (const float(*)[3])polynors,
                                num_polys,
                                true,
                                result->smoothresh,
                                NULL,
                                clnors,
                                NULL);
  }

  if (clnors == NULL) {
    clnors = CustomData_add_layer(ldata, CD_CUSTOMLOOPNORMAL, CD_CALLOC, NULL, num_loops);
  }

  MOD_get_vgroup(ob, result, enmd->defgrp_name, &dvert, &defgrp_index);

  if (enmd->mode == MOD_NORMALEDIT_MODE_RADIAL) {
    normalEditModifier_do_radial(enmd,
                                 ctx,
                                 ob,
                                 result,
                                 clnors,
                                 loopnors,
                                 polynors,
                                 enmd->mix_mode,
                                 enmd->mix_factor,
                                 enmd->mix_limit,
                                 dvert,
                                 defgrp_index,
                                 use_invert_vgroup,
                                 mvert,
                                 num_verts,
                                 medge,
                                 num_edges,
                                 mloop,
                                 num_loops,
                                 mpoly,
                                 num_polys);
  }
  else if (enmd->mode == MOD_NORMALEDIT_MODE_DIRECTIONAL) {
    normalEditModifier_do_directional(enmd,
                                      ctx,
                                      ob,
                                      result,
                                      clnors,
                                      loopnors,
                                      polynors,
                                      enmd->mix_mode,
                                      enmd->mix_factor,
                                      enmd->mix_limit,
                                      dvert,
                                      defgrp_index,
                                      use_invert_vgroup,
                                      mvert,
                                      num_verts,
                                      medge,
                                      num_edges,
                                      mloop,
                                      num_loops,
                                      mpoly,
                                      num_polys);
  }

  /* Currently Modifier stack assumes there is no poly normal data passed around... */
  CustomData_free_layers(pdata, CD_NORMAL, num_polys);
  MEM_SAFE_FREE(loopnors);

  return result;
}

static void initData(ModifierData *md)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  enmd->mode = MOD_NORMALEDIT_MODE_RADIAL;

  enmd->mix_mode = MOD_NORMALEDIT_MIX_COPY;
  enmd->mix_factor = 1.0f;
  enmd->mix_limit = M_PI;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  r_cddata_masks->lmask |= CD_MASK_CUSTOMLOOPNORMAL;

  /* Ask for vertexgroups if we need them. */
  if (enmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool dependsOnNormals(ModifierData *UNUSED(md))
{
  return true;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  walk(userData, ob, &enmd->target, IDWALK_CB_NOP);
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  return !is_valid_target(enmd);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;
  if (enmd->target) {
    DEG_add_object_relation(ctx->node, enmd->target, DEG_OB_COMP_TRANSFORM, "NormalEdit Modifier");
    DEG_add_modifier_to_transform_relation(ctx->node, "NormalEdit Modifier");
  }
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  return normalEditModifier_do((NormalEditModifierData *)md, ctx, ctx->object, mesh);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  int mode = RNA_enum_get(&ptr, "mode");

  uiItemR(layout, &ptr, "mode", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "target", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, mode == MOD_NORMALEDIT_MODE_DIRECTIONAL);
  uiItemR(col, &ptr, "use_direction_parallel", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

/* This panel could be open by default, but it isn't currently. */
static void mix_mode_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "mix_mode", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "mix_factor", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  row = uiLayoutRow(layout, true);
  uiItemR(row, &ptr, "mix_limit", 0, NULL, ICON_NONE);
  uiItemR(row,
          &ptr,
          "no_polynors_fix",
          0,
          "",
          (RNA_boolean_get(&ptr, "no_polynors_fix") ? ICON_LOCKED : ICON_UNLOCKED));
}

static void offset_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  int mode = RNA_enum_get(&ptr, "mode");
  PointerRNA target_ptr = RNA_pointer_get(&ptr, "target");
  bool needs_object_offset = (mode == MOD_NORMALEDIT_MODE_RADIAL &&
                              RNA_pointer_is_null(&target_ptr)) ||
                             (mode == MOD_NORMALEDIT_MODE_DIRECTIONAL &&
                              RNA_boolean_get(&ptr, "use_direction_parallel"));

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, needs_object_offset);
  uiItemR(layout, &ptr, "offset", 0, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_NormalEdit, panel_draw);
  modifier_subpanel_register(region_type, "mix", "Mix", NULL, mix_mode_panel_draw, panel_type);
  modifier_subpanel_register(region_type, "offset", "Offset", NULL, offset_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_NormalEdit = {
    /* name */ "NormalEdit",
    /* structName */ "NormalEditModifierData",
    /* structSize */ sizeof(NormalEditModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
