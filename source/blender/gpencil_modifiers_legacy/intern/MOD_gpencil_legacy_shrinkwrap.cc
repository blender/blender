/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstring> /* For #MEMCPY_STRUCT_AFTER. */

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_shrinkwrap.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void initData(GpencilModifierData *md)
{
  ShrinkwrapGpencilModifierData *gpmd = (ShrinkwrapGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(ShrinkwrapGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void deformStroke(GpencilModifierData *md,
                         Depsgraph * /*depsgraph*/,
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe * /*gpf*/,
                         bGPDstroke *gps)
{
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  ShrinkwrapGpencilModifierData *mmd = (ShrinkwrapGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_SHRINKWRAP_INVERT_LAYER,
                                      mmd->flag & GP_SHRINKWRAP_INVERT_PASS,
                                      mmd->flag & GP_SHRINKWRAP_INVERT_LAYERPASS,
                                      mmd->flag & GP_SHRINKWRAP_INVERT_MATERIAL))
  {
    return;
  }

  if ((mmd->cache_data == nullptr) || (mmd->target == ob) || (mmd->aux_target == ob)) {
    return;
  }

  bGPDspoint *pt = gps->points;
  float(*vert_coords)[3] = static_cast<float(*)[3]>(
      MEM_mallocN(sizeof(float[3]) * gps->totpoints, __func__));
  int i;
  /* Prepare array of points. */
  for (i = 0; i < gps->totpoints; i++, pt++) {
    copy_v3_v3(vert_coords[i], &pt->x);
  }

  shrinkwrapGpencilModifier_deform(mmd, ob, gps->dvert, def_nr, vert_coords, gps->totpoints);

  /* Apply deformed coordinates. */
  pt = gps->points;
  for (i = 0; i < gps->totpoints; i++, pt++) {
    copy_v3_v3(&pt->x, vert_coords[i]);
  }

  MEM_freeN(vert_coords);

  /* Smooth stroke. */
  BKE_gpencil_stroke_smooth(
      gps, mmd->smooth_factor, mmd->smooth_step, true, false, false, false, true, nullptr);

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

static void bakeModifier(Main * /*bmain*/,
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  ShrinkwrapGpencilModifierData *mmd = (ShrinkwrapGpencilModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  int oldframe = int(DEG_get_ctime(depsgraph));

  if ((mmd->target == ob) || (mmd->aux_target == ob)) {
    return;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* Apply shrinkwrap effects on this frame. */
      scene->r.cfra = gpf->framenum;
      BKE_scene_graph_update_for_newframe(depsgraph);

      /* Recalculate shrinkwrap data. */
      if (mmd->cache_data) {
        BKE_shrinkwrap_free_tree(mmd->cache_data);
        MEM_SAFE_FREE(mmd->cache_data);
      }
      Object *ob_target = DEG_get_evaluated_object(depsgraph, mmd->target);
      Mesh *target = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target);
      mmd->cache_data = static_cast<ShrinkwrapTreeData *>(
          MEM_callocN(sizeof(ShrinkwrapTreeData), __func__));
      if (BKE_shrinkwrap_init_tree(
              mmd->cache_data, target, mmd->shrink_type, mmd->shrink_mode, false)) {

        /* Compute shrinkwrap effects on this frame. */
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          deformStroke(md, depsgraph, ob, gpl, gpf, gps);
        }
      }
      /* Free data. */
      if (mmd->cache_data) {
        BKE_shrinkwrap_free_tree(mmd->cache_data);
        MEM_SAFE_FREE(mmd->cache_data);
      }
    }
  }

  /* Return frame state and DB to original state. */
  scene->r.cfra = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph);
}

static void freeData(GpencilModifierData *md)
{
  ShrinkwrapGpencilModifierData *mmd = (ShrinkwrapGpencilModifierData *)md;
  if (mmd->cache_data) {
    BKE_shrinkwrap_free_tree(mmd->cache_data);
    MEM_SAFE_FREE(mmd->cache_data);
  }
}

static bool isDisabled(GpencilModifierData *md, int /*userRenderParams*/)
{
  ShrinkwrapGpencilModifierData *mmd = (ShrinkwrapGpencilModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  if (!mmd->target || mmd->target->type != OB_MESH) {
    return true;
  }
  if (mmd->aux_target && mmd->aux_target->type != OB_MESH) {
    return true;
  }
  return false;
}

static void updateDepsgraph(GpencilModifierData *md,
                            const ModifierUpdateDepsgraphContext *ctx,
                            const int /*mode*/)
{
  ShrinkwrapGpencilModifierData *mmd = (ShrinkwrapGpencilModifierData *)md;
  CustomData_MeshMasks mask = {0};

  if (BKE_shrinkwrap_needs_normals(mmd->shrink_type, mmd->shrink_mode)) {
    mask.lmask |= CD_MASK_NORMAL | CD_MASK_CUSTOMLOOPNORMAL;
  }

  if (mmd->target != nullptr) {
    DEG_add_object_relation(ctx->node, mmd->target, DEG_OB_COMP_TRANSFORM, "Shrinkwrap Modifier");
    DEG_add_object_relation(ctx->node, mmd->target, DEG_OB_COMP_GEOMETRY, "Shrinkwrap Modifier");
    DEG_add_customdata_mask(ctx->node, mmd->target, &mask);
    if (mmd->shrink_type == MOD_SHRINKWRAP_TARGET_PROJECT) {
      DEG_add_special_eval_flag(ctx->node, &mmd->target->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
    }
  }
  if (mmd->aux_target != nullptr) {
    DEG_add_object_relation(
        ctx->node, mmd->aux_target, DEG_OB_COMP_TRANSFORM, "Shrinkwrap Modifier");
    DEG_add_object_relation(
        ctx->node, mmd->aux_target, DEG_OB_COMP_GEOMETRY, "Shrinkwrap Modifier");
    DEG_add_customdata_mask(ctx->node, mmd->aux_target, &mask);
    if (mmd->shrink_type == MOD_SHRINKWRAP_TARGET_PROJECT) {
      DEG_add_special_eval_flag(
          ctx->node, &mmd->aux_target->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
    }
  }
  DEG_add_depends_on_transform_relation(ctx->node, "Shrinkwrap Modifier");
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  ShrinkwrapGpencilModifierData *mmd = (ShrinkwrapGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->target, IDWALK_CB_NOP);
  walk(userData, ob, (ID **)&mmd->aux_target, IDWALK_CB_NOP);
  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  int wrap_method = RNA_enum_get(ptr, "wrap_method");

  uiItemR(layout, ptr, "wrap_method", 0, nullptr, ICON_NONE);

  if (ELEM(wrap_method,
           MOD_SHRINKWRAP_PROJECT,
           MOD_SHRINKWRAP_NEAREST_SURFACE,
           MOD_SHRINKWRAP_TARGET_PROJECT))
  {
    uiItemR(layout, ptr, "wrap_mode", 0, nullptr, ICON_NONE);
  }

  if (wrap_method == MOD_SHRINKWRAP_PROJECT) {
    uiItemR(layout, ptr, "project_limit", 0, IFACE_("Limit"), ICON_NONE);
    uiItemR(layout, ptr, "subsurf_levels", 0, nullptr, ICON_NONE);

    col = uiLayoutColumn(layout, false);
    row = uiLayoutRowWithHeading(col, true, IFACE_("Axis"));
    uiItemR(row, ptr, "use_project_x", toggles_flag, nullptr, ICON_NONE);
    uiItemR(row, ptr, "use_project_y", toggles_flag, nullptr, ICON_NONE);
    uiItemR(row, ptr, "use_project_z", toggles_flag, nullptr, ICON_NONE);

    uiItemR(col, ptr, "use_negative_direction", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "use_positive_direction", 0, nullptr, ICON_NONE);

    uiItemR(layout, ptr, "cull_face", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
    col = uiLayoutColumn(layout, false);
    uiLayoutSetActive(col,
                      RNA_boolean_get(ptr, "use_negative_direction") &&
                          RNA_enum_get(ptr, "cull_face") != 0);
    uiItemR(col, ptr, "use_invert_cull", 0, nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "target", 0, nullptr, ICON_NONE);
  if (wrap_method == MOD_SHRINKWRAP_PROJECT) {
    uiItemR(layout, ptr, "auxiliary_target", 0, nullptr, ICON_NONE);
  }
  uiItemR(layout, ptr, "offset", 0, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "smooth_factor", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "smooth_step", 0, IFACE_("Repeat"), ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Shrinkwrap, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Shrinkwrap = {
    /*name*/ N_("Shrinkwrap"),
    /*struct_name*/ "ShrinkwrapGpencilModifierData",
    /*struct_size*/ sizeof(ShrinkwrapGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ nullptr,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ nullptr,

    /*initData*/ initData,
    /*freeData*/ freeData,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*panelRegister*/ panelRegister,
};
