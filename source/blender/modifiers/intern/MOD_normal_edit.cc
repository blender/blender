/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.hh"
#include "BKE_deform.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void generate_vert_coordinates(Mesh *mesh,
                                      Object *ob,
                                      Object *ob_center,
                                      const float offset[3],
                                      const int verts_num,
                                      float (*r_cos)[3],
                                      blender::float3 *r_size)
{
  using namespace blender;
  float min_co[3], max_co[3];
  float diff[3];
  bool do_diff = false;

  INIT_MINMAX(min_co, max_co);

  const Span<float3> positions = mesh->vert_positions();
  for (int i = 0; i < mesh->verts_num; i++) {
    copy_v3_v3(r_cos[i], positions[i]);
    if (r_size != nullptr && ob_center == nullptr) {
      minmax_v3v3_v3(min_co, max_co, r_cos[i]);
    }
  }

  /* Get size (i.e. deformation of the spheroid generating normals),
   * either from target object, or geometry. */
  if (r_size != nullptr) {
    if (ob_center != nullptr) {
      /* Using 'scale' as 'size' here. The input object is typically an empty
       * who's scale is used to define an ellipsoid instead of a simple sphere. */

      /* Not we are not interested in signs here - they are even troublesome actually,
       * due to security clamping! */
      *r_size = blender::math::abs(blender::float3(ob_center->scale));
    }
    else {
      /* Set size. */
      sub_v3_v3v3(*r_size, max_co, min_co);
    }

    /* Error checks - we do not want one or more of our sizes to be null! */
    if (is_zero_v3(*r_size)) {
      *r_size = float3(1.0f);
    }
    else {
      CLAMP_MIN((*r_size)[0], FLT_EPSILON);
      CLAMP_MIN((*r_size)[1], FLT_EPSILON);
      CLAMP_MIN((*r_size)[2], FLT_EPSILON);
    }
  }

  if (ob_center != nullptr) {
    float inv_obmat[4][4];

    /* Translate our coordinates so that center of ob_center is at (0, 0, 0). */
    /* Get ob_center (world) coordinates in ob local coordinates.
     * No need to take into account ob_center's space here, see #44027. */
    invert_m4_m4(inv_obmat, ob->object_to_world().ptr());
    mul_v3_m4v3(diff, inv_obmat, ob_center->object_to_world().location());
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
                        const blender::Span<int> corner_verts,
                        blender::float3 *nos_old,
                        blender::float3 *nos_new)
{
  /* Mix with org normals... */
  float *facs = nullptr, *wfac;
  blender::float3 *no_new, *no_old;
  int i;

  if (dvert) {
    facs = MEM_malloc_arrayN<float>(size_t(corner_verts.size()), __func__);
    BKE_defvert_extract_vgroup_to_loopweights(
        dvert, defgrp_index, verts_num, corner_verts, use_invert_vgroup, facs);
  }

  for (i = corner_verts.size(), no_new = nos_new, no_old = nos_old, wfac = facs; i--;
       no_new++, no_old++, wfac++)
  {
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

/* Check face normals and new loop normals are compatible, otherwise flip faces
 * (and invert matching face normals). */
static void faces_check_flip(Mesh &mesh,
                             blender::MutableSpan<blender::float3> nos,
                             const blender::Span<blender::float3> face_normals)
{
  using namespace blender;
  const OffsetIndices faces = mesh.faces();
  IndexMaskMemory memory;
  const IndexMask faces_to_flip = IndexMask::from_predicate(
      faces.index_range(), GrainSize(1024), memory, [&](const int i) {
        const blender::IndexRange face = faces[i];
        float norsum[3] = {0.0f};

        for (const int64_t j : face) {
          add_v3_v3(norsum, nos[j]);
        }
        if (!normalize_v3(norsum)) {
          return false;
        }

        /* If average of new loop normals is opposed to face normal, flip face. */
        if (dot_v3v3(face_normals[i], norsum) < 0.0f) {
          nos.slice(faces[i].drop_front(1)).reverse();
          return true;
        }
        return false;
      });

  bke::mesh_flip_faces(mesh, faces_to_flip);
}

static void normalEditModifier_do_radial(NormalEditModifierData *enmd,
                                         const ModifierEvalContext * /*ctx*/,
                                         Object *ob,
                                         Mesh *mesh,
                                         blender::MutableSpan<blender::short2> clnors,
                                         blender::MutableSpan<blender::float3> corner_normals,
                                         const short mix_mode,
                                         const float mix_factor,
                                         const float mix_limit,
                                         const MDeformVert *dvert,
                                         const int defgrp_index,
                                         const bool use_invert_vgroup,
                                         blender::Span<blender::float3> vert_positions,
                                         blender::MutableSpan<bool> sharp_edges,
                                         blender::MutableSpan<int> corner_verts,
                                         blender::MutableSpan<int> corner_edges,
                                         const blender::OffsetIndices<int> faces)
{
  using namespace blender;
  Object *ob_target = enmd->target;

  const bool do_facenors_fix = (enmd->flag & MOD_NORMALEDIT_NO_POLYNORS_FIX) == 0;

  float (*cos)[3] = MEM_malloc_arrayN<float[3]>(size_t(vert_positions.size()), __func__);
  blender::Array<blender::float3> nos(corner_verts.size());
  float3 size;

  BLI_bitmap *done_verts = BLI_BITMAP_NEW(size_t(vert_positions.size()), __func__);

  generate_vert_coordinates(mesh, ob, ob_target, enmd->offset, vert_positions.size(), cos, &size);

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

    /* We reuse cos to now store the ellipsoid-normal of the verts! */
    for (const int64_t i : corner_verts.index_range()) {
      const int vidx = corner_verts[i];
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
      nos[i] = co;
    }
  }

  if (!corner_normals.is_empty()) {
    mix_normals(mix_factor,
                dvert,
                defgrp_index,
                use_invert_vgroup,
                mix_limit,
                mix_mode,
                vert_positions.size(),
                corner_verts,
                corner_normals.data(),
                nos.data());
  }

  if (do_facenors_fix) {
    faces_check_flip(*mesh, nos, mesh->face_normals_true());
  }
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
  bke::mesh::normals_corner_custom_set(vert_positions,
                                       faces,
                                       corner_verts,
                                       corner_edges,
                                       mesh->vert_to_face_map(),
                                       mesh->vert_normals_true(),
                                       mesh->face_normals_true(),
                                       sharp_faces,
                                       sharp_edges,
                                       nos,
                                       clnors);

  MEM_freeN(cos);
  MEM_freeN(done_verts);
}

static void normalEditModifier_do_directional(NormalEditModifierData *enmd,
                                              const ModifierEvalContext * /*ctx*/,
                                              Object *ob,
                                              Mesh *mesh,
                                              blender::MutableSpan<blender::short2> clnors,
                                              blender::MutableSpan<blender::float3> corner_normals,
                                              const short mix_mode,
                                              const float mix_factor,
                                              const float mix_limit,
                                              const MDeformVert *dvert,
                                              const int defgrp_index,
                                              const bool use_invert_vgroup,
                                              const blender::Span<blender::float3> positions,
                                              blender::MutableSpan<bool> sharp_edges,
                                              blender::MutableSpan<int> corner_verts,
                                              blender::MutableSpan<int> corner_edges,
                                              const blender::OffsetIndices<int> faces)
{
  using namespace blender;
  Object *ob_target = enmd->target;

  const bool do_facenors_fix = (enmd->flag & MOD_NORMALEDIT_NO_POLYNORS_FIX) == 0;
  const bool use_parallel_normals = (enmd->flag & MOD_NORMALEDIT_USE_DIRECTION_PARALLEL) != 0;

  blender::Array<blender::float3> nos(corner_verts.size());

  float target_co[3];
  int i;

  /* Get target's center coordinates in ob local coordinates. */
  float mat[4][4];

  invert_m4_m4(mat, ob->object_to_world().ptr());
  mul_m4_m4m4(mat, mat, ob_target->object_to_world().ptr());
  copy_v3_v3(target_co, mat[3]);

  if (use_parallel_normals) {
    float no[3];

    sub_v3_v3v3(no, target_co, enmd->offset);
    normalize_v3(no);

    for (i = corner_verts.size(); i--;) {
      copy_v3_v3(nos[i], no);
    }
  }
  else {
    float (*cos)[3] = MEM_malloc_arrayN<float[3]>(size_t(positions.size()), __func__);
    generate_vert_coordinates(mesh, ob, ob_target, nullptr, positions.size(), cos, nullptr);

    BLI_bitmap *done_verts = BLI_BITMAP_NEW(size_t(positions.size()), __func__);

    /* We reuse cos to now store the 'to target' normal of the verts! */
    for (const int64_t i : corner_verts.index_range()) {
      const int vidx = corner_verts[i];
      float *co = cos[vidx];

      if (!BLI_BITMAP_TEST(done_verts, vidx)) {
        sub_v3_v3v3(co, target_co, co);
        normalize_v3(co);

        BLI_BITMAP_ENABLE(done_verts, vidx);
      }
      nos[i] = co;
    }

    MEM_freeN(done_verts);
    MEM_freeN(cos);
  }

  if (!corner_normals.is_empty()) {
    mix_normals(mix_factor,
                dvert,
                defgrp_index,
                use_invert_vgroup,
                mix_limit,
                mix_mode,
                positions.size(),
                corner_verts,
                corner_normals.data(),
                nos.data());
  }

  if (do_facenors_fix) {
    faces_check_flip(*mesh, nos, mesh->face_normals_true());
  }
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
  bke::mesh::normals_corner_custom_set(positions,
                                       faces,
                                       corner_verts,
                                       corner_edges,
                                       mesh->vert_to_face_map(),
                                       mesh->vert_normals_true(),
                                       mesh->face_normals_true(),
                                       sharp_faces,
                                       sharp_edges,
                                       nos,
                                       clnors);
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
  using namespace blender;
  const bool use_invert_vgroup = ((enmd->flag & MOD_NORMALEDIT_INVERT_VGROUP) != 0);
  const bool use_current_clnors = !((enmd->mix_mode == MOD_NORMALEDIT_MIX_COPY) &&
                                    (enmd->mix_factor == 1.0f) && (enmd->defgrp_name[0] == '\0') &&
                                    (enmd->mix_limit == float(M_PI)));

  /* Do not run that modifier at all if auto-smooth is disabled! */
  if (!is_valid_target_with_error(ctx->object, enmd) || mesh->corners_num == 0) {
    return mesh;
  }

  Mesh *result;
  if (mesh->edges().data() == ((Mesh *)ob->data)->edges().data()) {
    /* We need to duplicate data here, otherwise setting custom normals
     * (which may also affect sharp edges) could
     * modify original mesh, see #43671. */
    result = (Mesh *)BKE_id_copy_ex(nullptr, &mesh->id, nullptr, LIB_ID_COPY_LOCALIZE);
  }
  else {
    result = mesh;
  }

  const blender::Span<blender::float3> positions = result->vert_positions();
  const OffsetIndices faces = result->faces();
  blender::MutableSpan<int> corner_verts = result->corner_verts_for_write();
  blender::MutableSpan<int> corner_edges = result->corner_edges_for_write();

  int defgrp_index;
  const MDeformVert *dvert;

  blender::Array<blender::float3> corner_normals;

  bke::MutableAttributeAccessor attributes = result->attributes_for_write();
  bke::SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_edge", bke::AttrDomain::Edge);

  bke::SpanAttributeWriter custom_nors_dst = attributes.lookup_or_add_for_write_span<short2>(
      "custom_normal", bke::AttrDomain::Corner);
  if (!custom_nors_dst) {
    return result;
  }
  if (use_current_clnors) {
    corner_normals.reinitialize(corner_verts.size());
    const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
    blender::bke::mesh::normals_calc_corners(positions,
                                             faces,
                                             corner_verts,
                                             corner_edges,
                                             result->vert_to_face_map(),
                                             result->face_normals_true(),
                                             sharp_edges.span,
                                             sharp_faces,
                                             custom_nors_dst.span,
                                             nullptr,
                                             corner_normals);
  }

  MOD_get_vgroup(ob, result, enmd->defgrp_name, &dvert, &defgrp_index);

  if (enmd->mode == MOD_NORMALEDIT_MODE_RADIAL) {
    normalEditModifier_do_radial(enmd,
                                 ctx,
                                 ob,
                                 result,
                                 custom_nors_dst.span,
                                 corner_normals,
                                 enmd->mix_mode,
                                 enmd->mix_factor,
                                 enmd->mix_limit,
                                 dvert,
                                 defgrp_index,
                                 use_invert_vgroup,
                                 positions,
                                 sharp_edges.span,
                                 corner_verts,
                                 corner_edges,
                                 faces);
  }
  else if (enmd->mode == MOD_NORMALEDIT_MODE_DIRECTIONAL) {
    normalEditModifier_do_directional(enmd,
                                      ctx,
                                      ob,
                                      result,
                                      custom_nors_dst.span,
                                      corner_normals,
                                      enmd->mix_mode,
                                      enmd->mix_factor,
                                      enmd->mix_limit,
                                      dvert,
                                      defgrp_index,
                                      use_invert_vgroup,
                                      positions,
                                      sharp_edges.span,
                                      corner_verts,
                                      corner_edges,
                                      faces);
  }

  result->runtime->is_original_bmesh = false;

  custom_nors_dst.finish();
  sharp_edges.finish();

  return result;
}

static void init_data(ModifierData *md)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(enmd, modifier));

  MEMCPY_STRUCT_AFTER(enmd, DNA_struct_default_get(NormalEditModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (enmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  walk(user_data, ob, (ID **)&enmd->target, IDWALK_CB_NOP);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;

  return !is_valid_target(enmd);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NormalEditModifierData *enmd = (NormalEditModifierData *)md;
  if (enmd->target) {
    DEG_add_object_relation(ctx->node, enmd->target, DEG_OB_COMP_TRANSFORM, "NormalEdit Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "NormalEdit Modifier");
  }
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
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

  layout->prop(ptr, "mode", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  layout->prop(ptr, "target", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->active_set(mode == MOD_NORMALEDIT_MODE_DIRECTIONAL);
  col->prop(ptr, "use_direction_parallel", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

/* This panel could be open by default, but it isn't currently. */
static void mix_mode_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "mix_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "mix_factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  row = &layout->row(true);
  row->prop(ptr, "mix_limit", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row->prop(ptr,
            "no_polynors_fix",
            UI_ITEM_NONE,
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

  layout->use_property_split_set(true);

  layout->active_set(needs_object_offset);
  layout->prop(ptr, "offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_NormalEdit, panel_draw);
  modifier_subpanel_register(region_type, "mix", "Mix", nullptr, mix_mode_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "offset", "Offset", nullptr, offset_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_NormalEdit = {
    /*idname*/ "NormalEdit",
    /*name*/ N_("NormalEdit"),
    /*struct_name*/ "NormalEditModifierData",
    /*struct_size*/ sizeof(NormalEditModifierData),
    /*srna*/ &RNA_NormalEditModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_NORMALEDIT,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
