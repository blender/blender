/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
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
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

static void generate_vert_coordinates(Mesh *mesh,
                                      Object *ob,
                                      Object *ob_center,
                                      const float offset[3],
                                      const int verts_num,
                                      float (*r_cos)[3],
                                      float r_size[3])
{
  float min_co[3], max_co[3];
  float diff[3];
  bool do_diff = false;

  INIT_MINMAX(min_co, max_co);

  const MVert *mv = BKE_mesh_verts(mesh);
  for (int i = 0; i < mesh->totvert; i++, mv++) {
    copy_v3_v3(r_cos[i], mv->co);
    if (r_size != nullptr && ob_center == nullptr) {
      minmax_v3v3_v3(min_co, max_co, r_cos[i]);
    }
  }

  /* Get size (i.e. deformation of the spheroid generating normals),
   * either from target object, or own geometry. */
  if (r_size != nullptr) {
    if (ob_center != nullptr) {
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

  if (ob_center != nullptr) {
    float inv_obmat[4][4];

    /* Translate our coordinates so that center of ob_center is at (0, 0, 0). */
    /* Get ob_center (world) coordinates in ob local coordinates.
     * No need to take into account ob_center's space here, see T44027. */
    invert_m4_m4(inv_obmat, ob->object_to_world);
    mul_v3_m4v3(diff, inv_obmat, ob_center->object_to_world[3]);
    negate_v3(diff);

    do_diff = true;
  }
  else if (offset != nullptr && !is_zero_v3(offset)) {
    negate_v3_v3(diff, offset);

    do_diff = true;
  }
  /* Else, no need to change coordinates! */

  if (do_diff) {
    int i = verts_num;
    while (i--) {
      add_v3_v3(r_cos[i], diff);
    }
  }
}

/* Note this modifies nos_new in-place. */
static void mix_normals(const float mix_factor,
                        const MDeformVert *dvert,
                        const int defgrp_index,
                        const bool use_invert_vgroup,
                        const float mix_limit,
                        const short mix_mode,
                        const int verts_num,
                        const MLoop *mloop,
                        float (*nos_old)[3],
                        float (*nos_new)[3],
                        const int loops_num)
{
  /* Mix with org normals... */
  float *facs = nullptr, *wfac;
  float(*no_new)[3], (*no_old)[3];
  int i;

  if (dvert) {
    facs = static_cast<float *>(MEM_malloc_arrayN(size_t(loops_num), sizeof(*facs), __func__));
    BKE_defvert_extract_vgroup_to_loopweights(
        dvert, defgrp_index, verts_num, mloop, loops_num, use_invert_vgroup, facs);
  }

  for (i = loops_num, no_new = nos_new, no_old = nos_old, wfac = facs; i--;
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
        (mix_limit < float(M_PI)) ? min_ff(fac, mix_limit / angle_v3v3(*no_new, *no_old)) : fac);
  }

  MEM_SAFE_FREE(facs);
}

/* Check poly normals and new loop normals are compatible, otherwise flip polygons
 * (and invert matching poly normals). */
static bool polygons_check_flip(MLoop *mloop,
                                float (*nos)[3],
                                CustomData *ldata,
                                const MPoly *mpoly,
                                float (*poly_normals)[3],
                                const int polys_num)
{
  const MPoly *mp;
  MDisps *mdisp = static_cast<MDisps *>(CustomData_get_layer(ldata, CD_MDISPS));
  int i;
  bool flipped = false;

  for (i = 0, mp = mpoly; i < polys_num; i++, mp++) {
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
    if (dot_v3v3(poly_normals[i], norsum) < 0.0f) {
      BKE_mesh_polygon_flip_ex(mp, mloop, ldata, nos, mdisp, true);
      negate_v3(poly_normals[i]);
      flipped = true;
    }
  }

  return flipped;
}

static void normalEditModifier_do_radial(NormalEditModifierData *enmd,
                                         const ModifierEvalContext * /*ctx*/,
                                         Object *ob,
                                         Mesh *mesh,
                                         short (*clnors)[2],
                                         float (*loop_normals)[3],
                                         const float (*poly_normals)[3],
                                         const short mix_mode,
                                         const float mix_factor,
                                         const float mix_limit,
                                         const MDeformVert *dvert,
                                         const int defgrp_index,
                                         const bool use_invert_vgroup,
                                         const MVert *mvert,
                                         const int verts_num,
                                         MEdge *medge,
                                         const int edges_num,
                                         MLoop *mloop,
                                         const int loops_num,
                                         const MPoly *mpoly,
                                         const int polys_num)
{
  Object *ob_target = enmd->target;

  const bool do_polynors_fix = (enmd->flag & MOD_NORMALEDIT_NO_POLYNORS_FIX) == 0;
  int i;

  float(*cos)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(size_t(verts_num), sizeof(*cos), __func__));
  float(*nos)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(size_t(loops_num), sizeof(*nos), __func__));
  float size[3];

  BLI_bitmap *done_verts = BLI_BITMAP_NEW(size_t(verts_num), __func__);

  generate_vert_coordinates(mesh, ob, ob_target, enmd->offset, verts_num, cos, size);

  /**
   * size gives us our spheroid coefficients `(A, B, C)`.
   * Then, we want to find out for each vert its (a, b, c) triple (proportional to (A, B, C) one).
   *
   * Ellipsoid basic equation: `(x^2/a^2) + (y^2/b^2) + (z^2/c^2) = 1`.
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

    const MLoop *ml;
    float(*no)[3];

    /* We reuse cos to now store the ellipsoid-normal of the verts! */
    for (i = loops_num, ml = mloop, no = nos; i--; ml++, no++) {
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

  if (loop_normals) {
    mix_normals(mix_factor,
                dvert,
                defgrp_index,
                use_invert_vgroup,
                mix_limit,
                mix_mode,
                verts_num,
                mloop,
                loop_normals,
                nos,
                loops_num);
  }

  if (do_polynors_fix &&
      polygons_check_flip(
          mloop, nos, &mesh->ldata, mpoly, BKE_mesh_poly_normals_for_write(mesh), polys_num)) {
    /* We need to recompute vertex normals! */
    BKE_mesh_normals_tag_dirty(mesh);
  }

  BKE_mesh_normals_loop_custom_set(mvert,
                                   BKE_mesh_vertex_normals_ensure(mesh),
                                   verts_num,
                                   medge,
                                   edges_num,
                                   mloop,
                                   nos,
                                   loops_num,
                                   mpoly,
                                   poly_normals,
                                   polys_num,
                                   clnors);

  MEM_freeN(cos);
  MEM_freeN(nos);
  MEM_freeN(done_verts);
}

static void normalEditModifier_do_directional(NormalEditModifierData *enmd,
                                              const ModifierEvalContext * /*ctx*/,
                                              Object *ob,
                                              Mesh *mesh,
                                              short (*clnors)[2],
                                              float (*loop_normals)[3],
                                              const float (*poly_normals)[3],
                                              const short mix_mode,
                                              const float mix_factor,
                                              const float mix_limit,
                                              const MDeformVert *dvert,
                                              const int defgrp_index,
                                              const bool use_invert_vgroup,
                                              const MVert *mvert,
                                              const int verts_num,
                                              MEdge *medge,
                                              const int edges_num,
                                              MLoop *mloop,
                                              const int loops_num,
                                              const MPoly *mpoly,
                                              const int polys_num)
{
  Object *ob_target = enmd->target;

  const bool do_polynors_fix = (enmd->flag & MOD_NORMALEDIT_NO_POLYNORS_FIX) == 0;
  const bool use_parallel_normals = (enmd->flag & MOD_NORMALEDIT_USE_DIRECTION_PARALLEL) != 0;

  float(*nos)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(size_t(loops_num), sizeof(*nos), __func__));

  float target_co[3];
  int i;

  /* Get target's center coordinates in ob local coordinates. */
  float mat[4][4];

  invert_m4_m4(mat, ob->object_to_world);
  mul_m4_m4m4(mat, mat, ob_target->object_to_world);
  copy_v3_v3(target_co, mat[3]);

  if (use_parallel_normals) {
    float no[3];

    sub_v3_v3v3(no, target_co, enmd->offset);
    normalize_v3(no);

    for (i = loops_num; i--;) {
      copy_v3_v3(nos[i], no);
    }
  }
  else {
    float(*cos)[3] = static_cast<float(*)[3]>(
        MEM_malloc_arrayN(size_t(verts_num), sizeof(*cos), __func__));
    generate_vert_coordinates(mesh, ob, ob_target, nullptr, verts_num, cos, nullptr);

    BLI_bitmap *done_verts = BLI_BITMAP_NEW(size_t(verts_num), __func__);
    const MLoop *ml;
    float(*no)[3];

    /* We reuse cos to now store the 'to target' normal of the verts! */
    for (i = loops_num, no = nos, ml = mloop; i--; no++, ml++) {
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

  if (loop_normals) {
    mix_normals(mix_factor,
                dvert,
                defgrp_index,
                use_invert_vgroup,
                mix_limit,
                mix_mode,
                verts_num,
                mloop,
                loop_normals,
                nos,
                loops_num);
  }

  if (do_polynors_fix &&
      polygons_check_flip(
          mloop, nos, &mesh->ldata, mpoly, BKE_mesh_poly_normals_for_write(mesh), polys_num)) {
    BKE_mesh_normals_tag_dirty(mesh);
  }

  BKE_mesh_normals_loop_custom_set(mvert,
                                   BKE_mesh_vertex_normals_ensure(mesh),
                                   verts_num,
                                   medge,
                                   edges_num,
                                   mloop,
                                   nos,
                                   loops_num,
                                   mpoly,
                                   poly_normals,
                                   polys_num,
                                   clnors);

  MEM_freeN(nos);
}

static bool is_valid_target(NormalEditModifierData *enmd)
{
  if (enmd->mode == MOD_NORMALEDIT_MODE_RADIAL) {
    return true;
  }
  if ((enmd->mode == MOD_NORMALEDIT_MODE_DIRECTIONAL) && enmd->target) {
    return true;
  }
  return false;
}

static bool is_valid_target_with_error(const Object *ob, NormalEditModifierData *enmd)
{
  if (is_valid_target(enmd)) {
    return true;
  }
  BKE_modifier_set_error(ob, (ModifierData *)enmd, "Invalid target settings");
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
                                    (enmd->mix_limit == float(M_PI)));

  /* Do not run that modifier at all if autosmooth is disabled! */
  if (!is_valid_target_with_error(ctx->object, enmd) || mesh->totloop == 0) {
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
    BKE_modifier_set_error(
        ob, (ModifierData *)enmd, "Enable 'Auto Smooth' in Object Data Properties");
    return mesh;
  }

  Mesh *result;
  if (BKE_mesh_edges(mesh) == BKE_mesh_edges((Mesh *)ob->data)) {
    /* We need to duplicate data here, otherwise setting custom normals
     * (which may also affect sharp edges) could
     * modify original mesh, see T43671. */
    result = (Mesh *)BKE_id_copy_ex(nullptr, &mesh->id, nullptr, LIB_ID_COPY_LOCALIZE);
  }
  else {
    result = mesh;
  }

  const int verts_num = result->totvert;
  const int edges_num = result->totedge;
  const int loops_num = result->totloop;
  const int polys_num = result->totpoly;
  const MVert *verts = BKE_mesh_verts(result);
  MEdge *edges = BKE_mesh_edges_for_write(result);
  const MPoly *polys = BKE_mesh_polys(result);
  MLoop *loops = BKE_mesh_loops_for_write(result);

  int defgrp_index;
  const MDeformVert *dvert;

  float(*loop_normals)[3] = nullptr;

  CustomData *ldata = &result->ldata;

  const float(*vert_normals)[3] = BKE_mesh_vertex_normals_ensure(result);
  const float(*poly_normals)[3] = BKE_mesh_poly_normals_ensure(result);

  short(*clnors)[2] = static_cast<short(*)[2]>(CustomData_get_layer(ldata, CD_CUSTOMLOOPNORMAL));
  if (use_current_clnors) {
    clnors = static_cast<short(*)[2]>(
        CustomData_duplicate_referenced_layer(ldata, CD_CUSTOMLOOPNORMAL, loops_num));
    loop_normals = static_cast<float(*)[3]>(
        MEM_malloc_arrayN(size_t(loops_num), sizeof(*loop_normals), __func__));

    BKE_mesh_normals_loop_split(verts,
                                vert_normals,
                                verts_num,
                                edges,
                                edges_num,
                                loops,
                                loop_normals,
                                loops_num,
                                polys,
                                poly_normals,
                                polys_num,
                                true,
                                result->smoothresh,
                                nullptr,
                                nullptr,
                                clnors);
  }

  if (clnors == nullptr) {
    clnors = static_cast<short(*)[2]>(
        CustomData_add_layer(ldata, CD_CUSTOMLOOPNORMAL, CD_SET_DEFAULT, nullptr, loops_num));
  }

  MOD_get_vgroup(ob, result, enmd->defgrp_name, &dvert, &defgrp_index);

  if (enmd->mode == MOD_NORMALEDIT_MODE_RADIAL) {
    normalEditModifier_do_radial(enmd,
                                 ctx,
                                 ob,
                                 result,
                                 clnors,
                                 loop_normals,
                                 poly_normals,
                                 enmd->mix_mode,
                                 enmd->mix_factor,
                                 enmd->mix_limit,
                                 dvert,
                                 defgrp_index,
                                 use_invert_vgroup,
                                 verts,
                                 verts_num,
                                 edges,
                                 edges_num,
                                 loops,
                                 loops_num,
                                 polys,
                                 polys_num);
  }
  else if (enmd->mode == MOD_NORMALEDIT_MODE_DIRECTIONAL) {
    normalEditModifier_do_directional(enmd,
                                      ctx,
                                      ob,
                                      result,
                                      clnors,
                                      loop_normals,
                                      poly_normals,
                                      enmd->mix_mode,
                                      enmd->mix_factor,
                                      enmd->mix_limit,
                                      dvert,
                                      defgrp_index,
                                      use_invert_vgroup,
                                      verts,
                                      verts_num,
                                      edges,
                                      edges_num,
                                      loops,
                                      loops_num,
                                      polys,
                                      polys_num);
  }

  MEM_SAFE_FREE(loop_normals);

  result->runtime->is_original_bmesh = false;

  return result;
}

static void initData(ModifierData *md)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(enmd, modifier));

  MEMCPY_STRUCT_AFTER(enmd, DNA_struct_default_get(NormalEditModifierData), modifier);
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  r_cddata_masks->lmask |= CD_MASK_CUSTOMLOOPNORMAL;

  /* Ask for vertexgroups if we need them. */
  if (enmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool dependsOnNormals(ModifierData * /*md*/)
{
  return true;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  walk(userData, ob, (ID **)&enmd->target, IDWALK_CB_NOP);
}

static bool isDisabled(const struct Scene * /*scene*/, ModifierData *md, bool /*useRenderParams*/)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  return !is_valid_target(enmd);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;
  if (enmd->target) {
    DEG_add_object_relation(ctx->node, enmd->target, DEG_OB_COMP_TRANSFORM, "NormalEdit Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "NormalEdit Modifier");
  }
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  return normalEditModifier_do((NormalEditModifierData *)md, ctx, ctx->object, mesh);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "target", 0, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, mode == MOD_NORMALEDIT_MODE_DIRECTIONAL);
  uiItemR(col, ptr, "use_direction_parallel", 0, nullptr, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

/* This panel could be open by default, but it isn't currently. */
static void mix_mode_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mix_mode", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "mix_factor", 0, nullptr, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "mix_limit", 0, nullptr, ICON_NONE);
  uiItemR(row,
          ptr,
          "no_polynors_fix",
          0,
          "",
          (RNA_boolean_get(ptr, "no_polynors_fix") ? ICON_LOCKED : ICON_UNLOCKED));
}

static void offset_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");
  PointerRNA target_ptr = RNA_pointer_get(ptr, "target");
  bool needs_object_offset = (mode == MOD_NORMALEDIT_MODE_RADIAL &&
                              RNA_pointer_is_null(&target_ptr)) ||
                             (mode == MOD_NORMALEDIT_MODE_DIRECTIONAL &&
                              RNA_boolean_get(ptr, "use_direction_parallel"));

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, needs_object_offset);
  uiItemR(layout, ptr, "offset", 0, nullptr, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_NormalEdit, panel_draw);
  modifier_subpanel_register(region_type, "mix", "Mix", nullptr, mix_mode_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "offset", "Offset", nullptr, offset_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_NormalEdit = {
    /* name */ N_("NormalEdit"),
    /* structName */ "NormalEditModifierData",
    /* structSize */ sizeof(NormalEditModifierData),
    /* srna */ &RNA_NormalEditModifier,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /* icon */ ICON_MOD_NORMALEDIT,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ modifyMesh,
    /* modifyGeometrySet */ nullptr,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ nullptr,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ nullptr,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ nullptr,
    /* freeRuntimeData */ nullptr,
    /* panelRegister */ panelRegister,
    /* blendWrite */ nullptr,
    /* blendRead */ nullptr,
};
