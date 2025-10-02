/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <algorithm>

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"
#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

#define BEND_EPS 0.000001f

BLI_ALIGN_STRUCT struct DeformUserData {
  bool invert_vgroup;
  char mode;
  char deform_axis;
  int lock_axis;
  int vgroup;
  int limit_axis;
  float smd_factor;
  float smd_limit[2];
  float (*vertexCos)[3];
  const SpaceTransform *transf;
  const MDeformVert *dvert;
};

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

/**
 * Clamps/Limits the given coordinate to: limits[0] <= co[axis] <= limits[1]
 * The amount of clamp is saved on `dcut`.
 */
static void axis_limit(const int axis, const float limits[2], float co[3], float dcut[3])
{
  float val = co[axis];
  val = std::max(limits[0], val);
  val = std::min(limits[1], val);

  dcut[axis] = co[axis] - val;
  co[axis] = val;
}

static void simpleDeform_taper(const float factor,
                               const int /*axis*/,
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
                                 const int /*axis*/,
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
                               const int /*axis*/,
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

  /* NOTE: the operations below a susceptible to float precision errors
   * regarding the order of operations, take care when changing, see: #85470 */
  switch (axis) {
    case 0:
      r_co[0] = x;
      r_co[1] = y * cost + (1.0f - cost) / factor;
      r_co[2] = -(y - 1.0f / factor) * sint;
      {
        r_co[0] += dcut[0];
        r_co[1] += sint * dcut[2];
        r_co[2] += cost * dcut[2];
      }
      break;
    case 1:
      r_co[0] = x * cost + (1.0f - cost) / factor;
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
      r_co[1] = y * cost + (1.0f - cost) / factor;
      r_co[2] = z;
      {
        r_co[0] += cost * dcut[0];
        r_co[1] += sint * dcut[0];
        r_co[2] += dcut[2];
      }
  }
}

static void simple_helper(void *__restrict userdata,
                          const int iter,
                          const TaskParallelTLS *__restrict /*tls*/)
{
  const DeformUserData *curr_deform_data = static_cast<const DeformUserData *>(userdata);
  float weight = BKE_defvert_array_find_weight_safe(
      curr_deform_data->dvert, iter, curr_deform_data->vgroup, curr_deform_data->invert_vgroup);
  const uint *axis_map = axis_map_table[(curr_deform_data->mode != MOD_SIMPLEDEFORM_MODE_BEND) ?
                                            curr_deform_data->deform_axis :
                                            2];
  const float base_limit[2] = {0.0f, 0.0f};

  if (weight != 0.0f) {
    float co[3], dcut[3] = {0.0f, 0.0f, 0.0f};

    if (curr_deform_data->transf) {
      BLI_space_transform_apply(curr_deform_data->transf, curr_deform_data->vertexCos[iter]);
    }

    copy_v3_v3(co, curr_deform_data->vertexCos[iter]);

    /* Apply axis limits, and axis mappings */
    if (curr_deform_data->lock_axis & MOD_SIMPLEDEFORM_LOCK_AXIS_X) {
      axis_limit(0, base_limit, co, dcut);
    }
    if (curr_deform_data->lock_axis & MOD_SIMPLEDEFORM_LOCK_AXIS_Y) {
      axis_limit(1, base_limit, co, dcut);
    }
    if (curr_deform_data->lock_axis & MOD_SIMPLEDEFORM_LOCK_AXIS_Z) {
      axis_limit(2, base_limit, co, dcut);
    }
    axis_limit(curr_deform_data->limit_axis, curr_deform_data->smd_limit, co, dcut);

    /* apply the deform to a mapped copy of the vertex, and then re-map it back. */
    float co_remap[3];
    float dcut_remap[3];
    copy_v3_v3_map(co_remap, co, axis_map);
    copy_v3_v3_map(dcut_remap, dcut, axis_map);
    switch (curr_deform_data->mode) {
      case MOD_SIMPLEDEFORM_MODE_TWIST:
        /* Apply deform. */
        simpleDeform_twist(
            curr_deform_data->smd_factor, curr_deform_data->deform_axis, dcut_remap, co_remap);
        break;
      case MOD_SIMPLEDEFORM_MODE_BEND:
        /* Apply deform. */
        simpleDeform_bend(
            curr_deform_data->smd_factor, curr_deform_data->deform_axis, dcut_remap, co_remap);
        break;
      case MOD_SIMPLEDEFORM_MODE_TAPER:
        /* Apply deform. */
        simpleDeform_taper(
            curr_deform_data->smd_factor, curr_deform_data->deform_axis, dcut_remap, co_remap);
        break;
      case MOD_SIMPLEDEFORM_MODE_STRETCH:
        /* Apply deform. */
        simpleDeform_stretch(
            curr_deform_data->smd_factor, curr_deform_data->deform_axis, dcut_remap, co_remap);
        break;
      default:
        return; /* No simple-deform mode? */
    }
    copy_v3_v3_unmap(co, co_remap, axis_map);

    /* Use vertex weight coefficient of the linear interpolation. */
    interp_v3_v3v3(
        curr_deform_data->vertexCos[iter], curr_deform_data->vertexCos[iter], co, weight);

    if (curr_deform_data->transf) {
      BLI_space_transform_invert(curr_deform_data->transf, curr_deform_data->vertexCos[iter]);
    }
  }
}

/* simple deform modifier */
static void SimpleDeformModifier_do(SimpleDeformModifierData *smd,
                                    const ModifierEvalContext * /*ctx*/,
                                    Object *ob,
                                    Mesh *mesh,
                                    float (*vertexCos)[3],
                                    int verts_num)
{
  int i;
  float smd_limit[2], smd_factor;
  SpaceTransform *transf = nullptr, tmp_transf;
  int vgroup;
  const MDeformVert *dvert;

  /* This is historically the lock axis, _not_ the deform axis as the name would imply */
  const int deform_axis = std::clamp(int(smd->deform_axis), 0, 2);
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
    smd->origin = nullptr; /* No self references */
  }

  smd->limit[0] = std::max(smd->limit[0], 0.0f);
  smd->limit[0] = std::min(smd->limit[0], 1.0f);

  smd->limit[0] = min_ff(smd->limit[0], smd->limit[1]); /* Upper limit >= than lower limit */

  /* Calculate matrix to convert between coordinate spaces. */
  if (smd->origin != nullptr) {
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

    for (i = 0; i < verts_num; i++) {
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

  if (smd->mode == MOD_SIMPLEDEFORM_MODE_BEND) {
    if (fabsf(smd_factor) < BEND_EPS) {
      return;
    }
  }

  MOD_get_vgroup(ob, mesh, smd->vgroup_name, &dvert, &vgroup);
  const bool invert_vgroup = (smd->flag & MOD_SIMPLEDEFORM_FLAG_INVERT_VGROUP) != 0;

  /* Build our data. */
  DeformUserData deform_pool_data{};
  deform_pool_data.mode = smd->mode;
  deform_pool_data.smd_factor = smd_factor;
  deform_pool_data.deform_axis = deform_axis;
  deform_pool_data.transf = transf;
  deform_pool_data.vertexCos = vertexCos;
  deform_pool_data.invert_vgroup = invert_vgroup;
  deform_pool_data.lock_axis = lock_axis;
  deform_pool_data.vgroup = vgroup;
  deform_pool_data.smd_limit[0] = smd_limit[0];
  deform_pool_data.smd_limit[1] = smd_limit[1];
  deform_pool_data.dvert = dvert;
  deform_pool_data.limit_axis = limit_axis;

  /* Do deformation. */
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, verts_num, (void *)&deform_pool_data, simple_helper, &settings);
}

/* SimpleDeform */
static void init_data(ModifierData *md)
{
  SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SimpleDeformModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (smd->vgroup_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;
  walk(user_data, ob, (ID **)&smd->origin, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;
  if (smd->origin != nullptr) {
    DEG_add_object_relation(
        ctx->node, smd->origin, DEG_OB_COMP_TRANSFORM, "SimpleDeform Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "SimpleDeform Modifier");
  }
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  SimpleDeformModifierData *sdmd = (SimpleDeformModifierData *)md;
  SimpleDeformModifier_do(sdmd,
                          ctx,
                          ctx->object,
                          mesh,
                          reinterpret_cast<float (*)[3]>(positions.data()),
                          positions.size());
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int deform_method = RNA_enum_get(ptr, "deform_method");

  row = &layout->row(false);
  row->prop(ptr, "deform_method", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  if (ELEM(deform_method, MOD_SIMPLEDEFORM_MODE_TAPER, MOD_SIMPLEDEFORM_MODE_STRETCH)) {
    layout->prop(ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else {
    layout->prop(ptr, "angle", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  layout->prop(ptr, "origin", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "deform_axis", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void restrictions_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int deform_method = RNA_enum_get(ptr, "deform_method");

  layout->use_property_split_set(true);

  layout->prop(ptr, "limits", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

  if (ELEM(deform_method,
           MOD_SIMPLEDEFORM_MODE_TAPER,
           MOD_SIMPLEDEFORM_MODE_STRETCH,
           MOD_SIMPLEDEFORM_MODE_TWIST))
  {
    int deform_axis = RNA_enum_get(ptr, "deform_axis");

    row = &layout->row(true, IFACE_("Lock"));
    if (deform_axis != 0) {
      row->prop(ptr, "lock_x", toggles_flag, std::nullopt, ICON_NONE);
    }
    if (deform_axis != 1) {
      row->prop(ptr, "lock_y", toggles_flag, std::nullopt, ICON_NONE);
    }
    if (deform_axis != 2) {
      row->prop(ptr, "lock_z", toggles_flag, std::nullopt, ICON_NONE);
    }
  }

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_SimpleDeform, panel_draw);
  modifier_subpanel_register(
      region_type, "restrictions", "Restrictions", nullptr, restrictions_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_SimpleDeform = {
    /*idname*/ "SimpleDeform",
    /*name*/ N_("SimpleDeform"),
    /*struct_name*/ "SimpleDeformModifierData",
    /*struct_size*/ sizeof(SimpleDeformModifierData),
    /*srna*/ &RNA_SimpleDeformModifier,
    /*type*/ ModifierTypeType::OnlyDeform,

    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_AcceptsVertexCosOnly | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SIMPLEDEFORM,

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
    /*is_disabled*/ nullptr,
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
