/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_object_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  CastModifierData *cmd = (CastModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(cmd, modifier));

  MEMCPY_STRUCT_AFTER(cmd, DNA_struct_default_get(CastModifierData), modifier);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  CastModifierData *cmd = (CastModifierData *)md;
  short flag;

  flag = cmd->flag & (MOD_CAST_X | MOD_CAST_Y | MOD_CAST_Z);

  if ((cmd->fac == 0.0f) || flag == 0) {
    return true;
  }

  return false;
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  CastModifierData *cmd = (CastModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (cmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  CastModifierData *cmd = (CastModifierData *)md;

  walk(user_data, ob, (ID **)&cmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  CastModifierData *cmd = (CastModifierData *)md;
  if (cmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, cmd->object, DEG_OB_COMP_TRANSFORM, "Cast Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "Cast Modifier");
  }
}

static void sphere_do(CastModifierData *cmd,
                      const ModifierEvalContext * /*ctx*/,
                      Object *ob,
                      Mesh *mesh,
                      blender::MutableSpan<blender::float3> positions)
{
  const MDeformVert *dvert = nullptr;
  const bool invert_vgroup = (cmd->flag & MOD_CAST_INVERT_VGROUP) != 0;

  Object *ctrl_ob = nullptr;

  int i, defgrp_index;
  bool has_radius = false;
  short flag, type;
  float len = 0.0f;
  float fac = cmd->fac;
  float facm = 1.0f - fac;
  const float fac_orig = fac;
  float vec[3], center[3] = {0.0f, 0.0f, 0.0f};
  float mat[4][4], imat[4][4];

  flag = cmd->flag;
  type = cmd->type; /* projection type: sphere or cylinder */

  if (type == MOD_CAST_TYPE_CYLINDER) {
    flag &= ~MOD_CAST_Z;
  }

  ctrl_ob = cmd->object;

  /* The spheres center is {0, 0, 0} (the ob's own center in its local space),
   * by default, but if the user defined a control object,
   * we use its location, transformed to ob's local space. */
  if (ctrl_ob) {
    if (flag & MOD_CAST_USE_OB_TRANSFORM) {
      invert_m4_m4(imat, ctrl_ob->object_to_world().ptr());
      mul_m4_m4m4(mat, imat, ob->object_to_world().ptr());
      invert_m4_m4(imat, mat);
    }

    invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
    mul_v3_m4v3(center, ob->world_to_object().ptr(), ctrl_ob->object_to_world().location());
  }

  /* now we check which options the user wants */

  /* 1) (flag was checked in the "if (ctrl_ob)" block above) */
  /* 2) cmd->radius > 0.0f: only the vertices within this radius from
   * the center of the effect should be deformed */
  if (cmd->radius > FLT_EPSILON) {
    has_radius = true;
  }

  /* 3) if we were given a vertex group name,
   * only those vertices should be affected */
  if (cmd->defgrp_name[0] != '\0') {
    MOD_get_vgroup(ob, mesh, cmd->defgrp_name, &dvert, &defgrp_index);
  }

  if (flag & MOD_CAST_SIZE_FROM_RADIUS) {
    len = cmd->radius;
  }
  else {
    len = cmd->size;
  }

  if (len <= 0) {
    for (i = 0; i < positions.size(); i++) {
      len += len_v3v3(center, positions[i]);
    }
    len /= positions.size();

    if (len == 0.0f) {
      len = 10.0f;
    }
  }

  for (i = 0; i < positions.size(); i++) {
    float tmp_co[3];

    copy_v3_v3(tmp_co, positions[i]);
    if (ctrl_ob) {
      if (flag & MOD_CAST_USE_OB_TRANSFORM) {
        mul_m4_v3(mat, tmp_co);
      }
      else {
        sub_v3_v3(tmp_co, center);
      }
    }

    copy_v3_v3(vec, tmp_co);

    if (type == MOD_CAST_TYPE_CYLINDER) {
      vec[2] = 0.0f;
    }

    if (has_radius) {
      if (len_v3(vec) > cmd->radius) {
        continue;
      }
    }

    if (dvert) {
      const float weight = invert_vgroup ?
                               1.0f - BKE_defvert_find_weight(&dvert[i], defgrp_index) :
                               BKE_defvert_find_weight(&dvert[i], defgrp_index);

      if (weight == 0.0f) {
        continue;
      }

      fac = fac_orig * weight;
      facm = 1.0f - fac;
    }

    normalize_v3(vec);

    if (flag & MOD_CAST_X) {
      tmp_co[0] = fac * vec[0] * len + facm * tmp_co[0];
    }
    if (flag & MOD_CAST_Y) {
      tmp_co[1] = fac * vec[1] * len + facm * tmp_co[1];
    }
    if (flag & MOD_CAST_Z) {
      tmp_co[2] = fac * vec[2] * len + facm * tmp_co[2];
    }

    if (ctrl_ob) {
      if (flag & MOD_CAST_USE_OB_TRANSFORM) {
        mul_m4_v3(imat, tmp_co);
      }
      else {
        add_v3_v3(tmp_co, center);
      }
    }

    copy_v3_v3(positions[i], tmp_co);
  }
}

static void cuboid_do(CastModifierData *cmd,
                      const ModifierEvalContext * /*ctx*/,
                      Object *ob,
                      Mesh *mesh,
                      blender::MutableSpan<blender::float3> positions)
{
  const MDeformVert *dvert = nullptr;
  int defgrp_index;
  const bool invert_vgroup = (cmd->flag & MOD_CAST_INVERT_VGROUP) != 0;

  Object *ctrl_ob = nullptr;

  int i;
  bool has_radius = false;
  short flag;
  float fac = cmd->fac;
  float facm = 1.0f - fac;
  const float fac_orig = fac;
  float min[3], max[3], bb[8][3];
  float center[3] = {0.0f, 0.0f, 0.0f};
  float mat[4][4], imat[4][4];

  flag = cmd->flag;

  ctrl_ob = cmd->object;

  /* now we check which options the user wants */

  /* 1) (flag was checked in the "if (ctrl_ob)" block above) */
  /* 2) cmd->radius > 0.0f: only the vertices within this radius from
   * the center of the effect should be deformed */
  if (cmd->radius > FLT_EPSILON) {
    has_radius = true;
  }

  /* 3) if we were given a vertex group name,
   * only those vertices should be affected */
  if (cmd->defgrp_name[0] != '\0') {
    MOD_get_vgroup(ob, mesh, cmd->defgrp_name, &dvert, &defgrp_index);
  }

  if (ctrl_ob) {
    if (flag & MOD_CAST_USE_OB_TRANSFORM) {
      invert_m4_m4(imat, ctrl_ob->object_to_world().ptr());
      mul_m4_m4m4(mat, imat, ob->object_to_world().ptr());
      invert_m4_m4(imat, mat);
    }

    invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
    mul_v3_m4v3(center, ob->world_to_object().ptr(), ctrl_ob->object_to_world().location());
  }

  if ((flag & MOD_CAST_SIZE_FROM_RADIUS) && has_radius) {
    for (i = 0; i < 3; i++) {
      min[i] = -cmd->radius;
      max[i] = cmd->radius;
    }
  }
  else if (!(flag & MOD_CAST_SIZE_FROM_RADIUS) && cmd->size > 0) {
    for (i = 0; i < 3; i++) {
      min[i] = -cmd->size;
      max[i] = cmd->size;
    }
  }
  else {
    /* get bound box */
    /* We can't use the object's bound box because other modifiers
     * may have changed the vertex data. */
    INIT_MINMAX(min, max);

    /* Cast's center is the ob's own center in its local space,
     * by default, but if the user defined a control object, we use
     * its location, transformed to ob's local space. */
    if (ctrl_ob) {
      float vec[3];

      /* let the center of the ctrl_ob be part of the bound box: */
      minmax_v3v3_v3(min, max, center);

      for (i = 0; i < positions.size(); i++) {
        sub_v3_v3v3(vec, positions[i], center);
        minmax_v3v3_v3(min, max, vec);
      }
    }
    else {
      for (i = 0; i < positions.size(); i++) {
        minmax_v3v3_v3(min, max, positions[i]);
      }
    }

    /* we want a symmetric bound box around the origin */
    if (fabsf(min[0]) > fabsf(max[0])) {
      max[0] = fabsf(min[0]);
    }
    if (fabsf(min[1]) > fabsf(max[1])) {
      max[1] = fabsf(min[1]);
    }
    if (fabsf(min[2]) > fabsf(max[2])) {
      max[2] = fabsf(min[2]);
    }
    min[0] = -max[0];
    min[1] = -max[1];
    min[2] = -max[2];
  }

  /* building our custom bounding box */
  bb[0][0] = bb[2][0] = bb[4][0] = bb[6][0] = min[0];
  bb[1][0] = bb[3][0] = bb[5][0] = bb[7][0] = max[0];
  bb[0][1] = bb[1][1] = bb[4][1] = bb[5][1] = min[1];
  bb[2][1] = bb[3][1] = bb[6][1] = bb[7][1] = max[1];
  bb[0][2] = bb[1][2] = bb[2][2] = bb[3][2] = min[2];
  bb[4][2] = bb[5][2] = bb[6][2] = bb[7][2] = max[2];

  /* ready to apply the effect, one vertex at a time */
  for (i = 0; i < positions.size(); i++) {
    int octant, coord;
    float d[3], dmax, apex[3], fbb;
    float tmp_co[3];

    copy_v3_v3(tmp_co, positions[i]);
    if (ctrl_ob) {
      if (flag & MOD_CAST_USE_OB_TRANSFORM) {
        mul_m4_v3(mat, tmp_co);
      }
      else {
        sub_v3_v3(tmp_co, center);
      }
    }

    if (has_radius) {
      if (fabsf(tmp_co[0]) > cmd->radius || fabsf(tmp_co[1]) > cmd->radius ||
          fabsf(tmp_co[2]) > cmd->radius)
      {
        continue;
      }
    }

    if (dvert) {
      const float weight = invert_vgroup ?
                               1.0f - BKE_defvert_find_weight(&dvert[i], defgrp_index) :
                               BKE_defvert_find_weight(&dvert[i], defgrp_index);

      if (weight == 0.0f) {
        continue;
      }

      fac = fac_orig * weight;
      facm = 1.0f - fac;
    }

    /* The algorithm used to project the vertices to their
     * bounding box (bb) is pretty simple:
     * for each vertex v:
     * 1) find in which octant v is in;
     * 2) find which outer "wall" of that octant is closer to v;
     * 3) calculate factor (var fbb) to project v to that wall;
     * 4) project. */

    /* find in which octant this vertex is in */
    octant = 0;
    if (tmp_co[0] > 0.0f) {
      octant += 1;
    }
    if (tmp_co[1] > 0.0f) {
      octant += 2;
    }
    if (tmp_co[2] > 0.0f) {
      octant += 4;
    }

    /* apex is the bb's vertex at the chosen octant */
    copy_v3_v3(apex, bb[octant]);

    /* find which bb plane is closest to this vertex ... */
    d[0] = tmp_co[0] / apex[0];
    d[1] = tmp_co[1] / apex[1];
    d[2] = tmp_co[2] / apex[2];

    /* ... (the closest has the higher (closer to 1) d value) */
    dmax = d[0];
    coord = 0;
    if (d[1] > dmax) {
      dmax = d[1];
      coord = 1;
    }
    if (d[2] > dmax) {
      // dmax = d[2]; /* commented, we don't need it */
      coord = 2;
    }

    /* ok, now we know which coordinate of the vertex to use */

    if (fabsf(tmp_co[coord]) < FLT_EPSILON) { /* avoid division by zero */
      continue;
    }

    /* finally, this is the factor we wanted, to project the vertex
     * to its bounding box (bb) */
    fbb = apex[coord] / tmp_co[coord];

    /* calculate the new vertex position */
    if (flag & MOD_CAST_X) {
      tmp_co[0] = facm * tmp_co[0] + fac * tmp_co[0] * fbb;
    }
    if (flag & MOD_CAST_Y) {
      tmp_co[1] = facm * tmp_co[1] + fac * tmp_co[1] * fbb;
    }
    if (flag & MOD_CAST_Z) {
      tmp_co[2] = facm * tmp_co[2] + fac * tmp_co[2] * fbb;
    }

    if (ctrl_ob) {
      if (flag & MOD_CAST_USE_OB_TRANSFORM) {
        mul_m4_v3(imat, tmp_co);
      }
      else {
        add_v3_v3(tmp_co, center);
      }
    }

    copy_v3_v3(positions[i], tmp_co);
  }
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  CastModifierData *cmd = (CastModifierData *)md;

  if (cmd->type == MOD_CAST_TYPE_CUBOID) {
    cuboid_do(cmd, ctx, ctx->object, mesh, positions);
  }
  else { /* MOD_CAST_TYPE_SPHERE or MOD_CAST_TYPE_CYLINDER */
    sphere_do(cmd, ctx, ctx->object, mesh, positions);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA cast_object_ptr = RNA_pointer_get(ptr, "object");

  layout->use_property_split_set(true);

  layout->prop(ptr, "cast_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true, IFACE_("Axis"));
  row->prop(ptr, "use_x", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_y", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_z", toggles_flag, std::nullopt, ICON_NONE);

  layout->prop(ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "use_radius_as_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  layout->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (!RNA_pointer_is_null(&cast_object_ptr)) {
    layout->prop(ptr, "use_transform", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Cast, panel_draw);
}

ModifierTypeInfo modifierType_Cast = {
    /*idname*/ "Cast",
    /*name*/ N_("Cast"),
    /*struct_name*/ "CastModifierData",
    /*struct_size*/ sizeof(CastModifierData),
    /*srna*/ &RNA_CastModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_CAST,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
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
