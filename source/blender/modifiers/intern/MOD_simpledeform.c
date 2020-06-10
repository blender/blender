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
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

#include "bmesh.h"

#define BEND_EPS 0.000001f

/* Re-maps the indices for X Y Z by shifting them up and wrapping, such that
 * X = Y, Y = Z, Z = X (for X axis), and X = Z, Y = X, Z = Y (for Y axis). This
 * exists because the deformations (excluding bend) are based on the Z axis.
 * Having this helps avoid long, drawn out switches. */
static const uint axis_map_table[3][3] = {
    {1, 2, 0},
    {2, 0, 1},
    {0, 1, 2},
};

BLI_INLINE void copy_v3_v3_map(float a[3], const float b[3], const uint map[3])
{
  a[0] = b[map[0]];
  a[1] = b[map[1]];
  a[2] = b[map[2]];
}

BLI_INLINE void copy_v3_v3_unmap(float a[3], const float b[3], const uint map[3])
{
  a[map[0]] = b[0];
  a[map[1]] = b[1];
  a[map[2]] = b[2];
}

/* Clamps/Limits the given coordinate to:  limits[0] <= co[axis] <= limits[1]
 * The amount of clamp is saved on dcut */
static void axis_limit(const int axis, const float limits[2], float co[3], float dcut[3])
{
  float val = co[axis];
  if (limits[0] > val) {
    val = limits[0];
  }
  if (limits[1] < val) {
    val = limits[1];
  }

  dcut[axis] = co[axis] - val;
  co[axis] = val;
}

static void simpleDeform_taper(const float factor,
                               const int UNUSED(axis),
                               const float dcut[3],
                               float r_co[3])
{
  float x = r_co[0], y = r_co[1], z = r_co[2];
  float scale = z * factor;

  r_co[0] = x + x * scale;
  r_co[1] = y + y * scale;
  r_co[2] = z;

  add_v3_v3(r_co, dcut);
}

static void simpleDeform_stretch(const float factor,
                                 const int UNUSED(axis),
                                 const float dcut[3],
                                 float r_co[3])
{
  float x = r_co[0], y = r_co[1], z = r_co[2];
  float scale;

  scale = (z * z * factor - factor + 1.0f);

  r_co[0] = x * scale;
  r_co[1] = y * scale;
  r_co[2] = z * (1.0f + factor);

  add_v3_v3(r_co, dcut);
}

static void simpleDeform_twist(const float factor,
                               const int UNUSED(axis),
                               const float *dcut,
                               float r_co[3])
{
  float x = r_co[0], y = r_co[1], z = r_co[2];
  float theta, sint, cost;

  theta = z * factor;
  sint = sinf(theta);
  cost = cosf(theta);

  r_co[0] = x * cost - y * sint;
  r_co[1] = x * sint + y * cost;
  r_co[2] = z;

  add_v3_v3(r_co, dcut);
}

static void simpleDeform_bend(const float factor,
                              const int axis,
                              const float dcut[3],
                              float r_co[3])
{
  float x = r_co[0], y = r_co[1], z = r_co[2];
  float theta, sint, cost;

  BLI_assert(!(fabsf(factor) < BEND_EPS));

  switch (axis) {
    case 0:
      ATTR_FALLTHROUGH;
    case 1:
      theta = z * factor;
      break;
    default:
      theta = x * factor;
  }
  sint = sinf(theta);
  cost = cosf(theta);

  switch (axis) {
    case 0:
      r_co[0] = x;
      r_co[1] = (y - 1.0f / factor) * cost + 1.0f / factor;
      r_co[2] = -(y - 1.0f / factor) * sint;
      {
        r_co[0] += dcut[0];
        r_co[1] += sint * dcut[2];
        r_co[2] += cost * dcut[2];
      }
      break;
    case 1:
      r_co[0] = (x - 1.0f / factor) * cost + 1.0f / factor;
      r_co[1] = y;
      r_co[2] = -(x - 1.0f / factor) * sint;
      {
        r_co[0] += sint * dcut[2];
        r_co[1] += dcut[1];
        r_co[2] += cost * dcut[2];
      }
      break;
    default:
      r_co[0] = -(y - 1.0f / factor) * sint;
      r_co[1] = (y - 1.0f / factor) * cost + 1.0f / factor;
      r_co[2] = z;
      {
        r_co[0] += cost * dcut[0];
        r_co[1] += sint * dcut[0];
        r_co[2] += dcut[2];
      }
  }
}

/* simple deform modifier */
static void SimpleDeformModifier_do(SimpleDeformModifierData *smd,
                                    const ModifierEvalContext *UNUSED(ctx),
                                    struct Object *ob,
                                    struct Mesh *mesh,
                                    float (*vertexCos)[3],
                                    int numVerts)
{
  const float base_limit[2] = {0.0f, 0.0f};
  int i;
  float smd_limit[2], smd_factor;
  SpaceTransform *transf = NULL, tmp_transf;
  void (*simpleDeform_callback)(const float factor,
                                const int axis,
                                const float dcut[3],
                                float co[3]) = NULL; /* Mode callback */
  int vgroup;
  MDeformVert *dvert;

  /* This is historically the lock axis, _not_ the deform axis as the name would imply */
  const int deform_axis = smd->deform_axis;
  int lock_axis = smd->axis;
  if (smd->mode == MOD_SIMPLEDEFORM_MODE_BEND) { /* Bend mode shouldn't have any lock axis */
    lock_axis = 0;
  }
  else {
    /* Don't lock axis if it is the chosen deform axis, as this flattens
     * the geometry */
    if (deform_axis == 0) {
      lock_axis &= ~MOD_SIMPLEDEFORM_LOCK_AXIS_X;
    }
    if (deform_axis == 1) {
      lock_axis &= ~MOD_SIMPLEDEFORM_LOCK_AXIS_Y;
    }
    if (deform_axis == 2) {
      lock_axis &= ~MOD_SIMPLEDEFORM_LOCK_AXIS_Z;
    }
  }

  /* Safe-check */
  if (smd->origin == ob) {
    smd->origin = NULL; /* No self references */
  }

  if (smd->limit[0] < 0.0f) {
    smd->limit[0] = 0.0f;
  }
  if (smd->limit[0] > 1.0f) {
    smd->limit[0] = 1.0f;
  }

  smd->limit[0] = min_ff(smd->limit[0], smd->limit[1]); /* Upper limit >= than lower limit */

  /* Calculate matrixs do convert between coordinate spaces */
  if (smd->origin != NULL) {
    transf = &tmp_transf;
    BLI_SPACE_TRANSFORM_SETUP(transf, ob, smd->origin);
  }

  /* Update limits if needed */
  int limit_axis = deform_axis;
  if (smd->mode == MOD_SIMPLEDEFORM_MODE_BEND) {
    /* Bend is a special case. */
    switch (deform_axis) {
      case 0:
        ATTR_FALLTHROUGH;
      case 1:
        limit_axis = 2;
        break;
      default:
        limit_axis = 0;
    }
  }

  {
    float lower = FLT_MAX;
    float upper = -FLT_MAX;

    for (i = 0; i < numVerts; i++) {
      float tmp[3];
      copy_v3_v3(tmp, vertexCos[i]);

      if (transf) {
        BLI_space_transform_apply(transf, tmp);
      }

      lower = min_ff(lower, tmp[limit_axis]);
      upper = max_ff(upper, tmp[limit_axis]);
    }

    /* SMD values are normalized to the BV, calculate the absolute values */
    smd_limit[1] = lower + (upper - lower) * smd->limit[1];
    smd_limit[0] = lower + (upper - lower) * smd->limit[0];

    smd_factor = smd->factor / max_ff(FLT_EPSILON, smd_limit[1] - smd_limit[0]);
  }

  switch (smd->mode) {
    case MOD_SIMPLEDEFORM_MODE_TWIST:
      simpleDeform_callback = simpleDeform_twist;
      break;
    case MOD_SIMPLEDEFORM_MODE_BEND:
      simpleDeform_callback = simpleDeform_bend;
      break;
    case MOD_SIMPLEDEFORM_MODE_TAPER:
      simpleDeform_callback = simpleDeform_taper;
      break;
    case MOD_SIMPLEDEFORM_MODE_STRETCH:
      simpleDeform_callback = simpleDeform_stretch;
      break;
    default:
      return; /* No simpledeform mode? */
  }

  if (smd->mode == MOD_SIMPLEDEFORM_MODE_BEND) {
    if (fabsf(smd_factor) < BEND_EPS) {
      return;
    }
  }

  MOD_get_vgroup(ob, mesh, smd->vgroup_name, &dvert, &vgroup);
  const bool invert_vgroup = (smd->flag & MOD_SIMPLEDEFORM_FLAG_INVERT_VGROUP) != 0;
  const uint *axis_map =
      axis_map_table[(smd->mode != MOD_SIMPLEDEFORM_MODE_BEND) ? deform_axis : 2];

  for (i = 0; i < numVerts; i++) {
    float weight = BKE_defvert_array_find_weight_safe(dvert, i, vgroup);

    if (invert_vgroup) {
      weight = 1.0f - weight;
    }

    if (weight != 0.0f) {
      float co[3], dcut[3] = {0.0f, 0.0f, 0.0f};

      if (transf) {
        BLI_space_transform_apply(transf, vertexCos[i]);
      }

      copy_v3_v3(co, vertexCos[i]);

      /* Apply axis limits, and axis mappings */
      if (lock_axis & MOD_SIMPLEDEFORM_LOCK_AXIS_X) {
        axis_limit(0, base_limit, co, dcut);
      }
      if (lock_axis & MOD_SIMPLEDEFORM_LOCK_AXIS_Y) {
        axis_limit(1, base_limit, co, dcut);
      }
      if (lock_axis & MOD_SIMPLEDEFORM_LOCK_AXIS_Z) {
        axis_limit(2, base_limit, co, dcut);
      }
      axis_limit(limit_axis, smd_limit, co, dcut);

      /* apply the deform to a mapped copy of the vertex, and then re-map it back. */
      float co_remap[3];
      float dcut_remap[3];
      copy_v3_v3_map(co_remap, co, axis_map);
      copy_v3_v3_map(dcut_remap, dcut, axis_map);
      simpleDeform_callback(smd_factor, deform_axis, dcut_remap, co_remap); /* apply deform */
      copy_v3_v3_unmap(co, co_remap, axis_map);

      /* Use vertex weight has coef of linear interpolation */
      interp_v3_v3v3(vertexCos[i], vertexCos[i], co, weight);

      if (transf) {
        BLI_space_transform_invert(transf, vertexCos[i]);
      }
    }
  }
}

/* SimpleDeform */
static void initData(ModifierData *md)
{
  SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;

  smd->mode = MOD_SIMPLEDEFORM_MODE_TWIST;
  smd->axis = 0;
  smd->deform_axis = 0;

  smd->origin = NULL;
  smd->factor = DEG2RADF(45.0f);
  smd->limit[0] = 0.0f;
  smd->limit[1] = 1.0f;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->vgroup_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;
  walk(userData, ob, &smd->origin, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;
  if (smd->origin != NULL) {
    DEG_add_object_relation(
        ctx->node, smd->origin, DEG_OB_COMP_TRANSFORM, "SimpleDeform Modifier");
    DEG_add_modifier_to_transform_relation(ctx->node, "SimpleDeform Modifier");
  }
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        struct Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  SimpleDeformModifierData *sdmd = (SimpleDeformModifierData *)md;
  Mesh *mesh_src = NULL;

  if (ctx->object->type == OB_MESH && sdmd->vgroup_name[0] != '\0') {
    /* mesh_src is only needed for vgroups. */
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, numVerts, false, false);
  }

  SimpleDeformModifier_do(sdmd, ctx, ctx->object, mesh_src, vertexCos, numVerts);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *editData,
                          struct Mesh *mesh,
                          float (*vertexCos)[3],
                          int numVerts)
{
  SimpleDeformModifierData *sdmd = (SimpleDeformModifierData *)md;
  Mesh *mesh_src = NULL;

  if (ctx->object->type == OB_MESH && sdmd->vgroup_name[0] != '\0') {
    /* mesh_src is only needed for vgroups. */
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, numVerts, false, false);
  }

  /* TODO(Campbell): use edit-mode data only (remove this line). */
  if (mesh_src != NULL) {
    BKE_mesh_wrapper_ensure_mdata(mesh_src);
  }

  SimpleDeformModifier_do(sdmd, ctx, ctx->object, mesh_src, vertexCos, numVerts);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  int deform_method = RNA_enum_get(&ptr, "deform_method");

  row = uiLayoutRow(layout, false);
  uiItemR(row, &ptr, "deform_method", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (ELEM(deform_method, MOD_SIMPLEDEFORM_MODE_TAPER, MOD_SIMPLEDEFORM_MODE_STRETCH)) {
    uiItemR(layout, &ptr, "factor", 0, NULL, ICON_NONE);
  }
  else {
    uiItemR(layout, &ptr, "angle", 0, NULL, ICON_NONE);
  }

  uiItemR(layout, &ptr, "origin", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "deform_axis", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void restrictions_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  int deform_method = RNA_enum_get(&ptr, "deform_method");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "limits", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  if (ELEM(deform_method,
           MOD_SIMPLEDEFORM_MODE_TAPER,
           MOD_SIMPLEDEFORM_MODE_STRETCH,
           MOD_SIMPLEDEFORM_MODE_TWIST)) {
    int deform_axis = RNA_enum_get(&ptr, "deform_axis");

    row = uiLayoutRowWithHeading(layout, true, IFACE_("Lock"));
    if (deform_axis != 0) {
      uiItemR(row, &ptr, "lock_x", toggles_flag, NULL, ICON_NONE);
    }
    if (deform_axis != 1) {
      uiItemR(row, &ptr, "lock_y", toggles_flag, NULL, ICON_NONE);
    }
    if (deform_axis != 2) {
      uiItemR(row, &ptr, "lock_z", toggles_flag, NULL, ICON_NONE);
    }
  }

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_SimpleDeform, panel_draw);
  modifier_subpanel_register(
      region_type, "restrictions", "Restrictions", NULL, restrictions_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_SimpleDeform = {
    /* name */ "SimpleDeform",
    /* structName */ "SimpleDeformModifierData",
    /* structSize */ sizeof(SimpleDeformModifierData),
    /* type */ eModifierTypeType_OnlyDeform,

    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_AcceptsVertexCosOnly | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};
