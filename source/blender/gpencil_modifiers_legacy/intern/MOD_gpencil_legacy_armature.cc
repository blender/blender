/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void init_data(GpencilModifierData *md)
{
  ArmatureGpencilModifierData *gpmd = (ArmatureGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(ArmatureGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static void gpencil_deform_verts(ArmatureGpencilModifierData *mmd, Object *target, bGPDstroke *gps)
{
  bGPDspoint *pt = gps->points;
  float(*vert_coords)[3] = static_cast<float(*)[3]>(
      MEM_mallocN(sizeof(float[3]) * gps->totpoints, __func__));
  int i;

  BKE_gpencil_dvert_ensure(gps);

  /* prepare array of points */
  for (i = 0; i < gps->totpoints; i++, pt++) {
    copy_v3_v3(vert_coords[i], &pt->x);
  }

  /* deform verts */
  BKE_armature_deform_coords_with_gpencil_stroke(mmd->object,
                                                 target,
                                                 vert_coords,
                                                 nullptr,
                                                 gps->totpoints,
                                                 mmd->deformflag,
                                                 mmd->vert_coords_prev,
                                                 mmd->vgname,
                                                 gps);

  /* Apply deformed coordinates */
  pt = gps->points;
  for (i = 0; i < gps->totpoints; i++, pt++) {
    copy_v3_v3(&pt->x, vert_coords[i]);
  }

  MEM_freeN(vert_coords);
}

/* deform stroke */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer * /*gpl*/,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;
  if (!mmd->object) {
    return;
  }
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  gpencil_deform_verts(mmd, ob, gps);
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

static void bake_modifier(Main * /*bmain*/,
                          Depsgraph *depsgraph,
                          GpencilModifierData *md,
                          Object *ob)
{
  ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;

  if (mmd->object == nullptr) {
    return;
  }
  generic_bake_deform_stroke(depsgraph, md, ob, true, deform_stroke);
}

static bool is_disabled(GpencilModifierData *md, bool /*use_render_params*/)
{
  ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the armature is missing).
   *
   * In other cases it should be impossible to have a type mismatch. */
  return !mmd->object || mmd->object->type != OB_ARMATURE;
}

static void update_depsgraph(GpencilModifierData *md,
                             const ModifierUpdateDepsgraphContext *ctx,
                             const int /*mode*/)
{
  ArmatureGpencilModifierData *lmd = (ArmatureGpencilModifierData *)md;
  if (lmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_EVAL_POSE, "Armature Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Armature Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Armature Modifier");
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);
  row = uiLayoutRow(layout, true);
  uiItemPointerR(row, ptr, "vertex_group", &ob_ptr, "vertex_groups", nullptr, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, has_vertex_group);
  uiLayoutSetPropDecorate(sub, false);
  uiItemR(sub, ptr, "invert_vertex_group", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

  col = uiLayoutColumnWithHeading(layout, true, IFACE_("Bind To"));
  uiItemR(col, ptr, "use_vertex_groups", UI_ITEM_NONE, IFACE_("Vertex Groups"), ICON_NONE);
  uiItemR(col, ptr, "use_bone_envelopes", UI_ITEM_NONE, IFACE_("Bone Envelopes"), ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  gpencil_modifier_panel_register(region_type, eGpencilModifierType_Armature, panel_draw);
}

GpencilModifierTypeInfo modifierType_Gpencil_Armature = {
    /*name*/ N_("Armature"),
    /*struct_name*/ "ArmatureGpencilModifierData",
    /*struct_size*/ sizeof(ArmatureGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ deform_stroke,
    /*generate_strokes*/ nullptr,
    /*bake_modifier*/ bake_modifier,
    /*remap_time*/ nullptr,
    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
