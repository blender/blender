/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstddef>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_dynamicpaint.h"
#include "BKE_layer.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

static void initData(ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(pmd, modifier));

  MEMCPY_STRUCT_AFTER(pmd, DNA_struct_default_get(DynamicPaintModifierData), modifier);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const DynamicPaintModifierData *pmd = (const DynamicPaintModifierData *)md;
  DynamicPaintModifierData *tpmd = (DynamicPaintModifierData *)target;

  dynamicPaint_Modifier_copy(pmd, tpmd, flag);
}

static void freeRuntimeData(void *runtime_data_v)
{
  if (runtime_data_v == nullptr) {
    return;
  }
  DynamicPaintRuntime *runtime_data = (DynamicPaintRuntime *)runtime_data_v;
  dynamicPaint_Modifier_free_runtime(runtime_data);
}

static void freeData(ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
  dynamicPaint_Modifier_free(pmd);
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  if (pmd->canvas) {
    DynamicPaintSurface *surface = static_cast<DynamicPaintSurface *>(pmd->canvas->surfaces.first);
    for (; surface; surface = surface->next) {
      /* UVs: #CD_PROP_FLOAT2. */
      if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ ||
          surface->init_color_type == MOD_DPAINT_INITIAL_TEXTURE)
      {
        r_cddata_masks->lmask |= CD_MASK_PROP_FLOAT2;
      }
      /* Vertex Colors: #CD_PROP_BYTE_COLOR. */
      if (surface->type == MOD_DPAINT_SURFACE_T_PAINT ||
          surface->init_color_type == MOD_DPAINT_INITIAL_VERTEXCOLOR)
      {
        r_cddata_masks->lmask |= CD_MASK_PROP_BYTE_COLOR;
      }
      /* Vertex Weights: #CD_MDEFORMVERT. */
      if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
        r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
      }
    }
  }
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  /* Don't apply dynamic paint on ORCO mesh stack. */
  if (!(ctx->flag & MOD_APPLY_ORCO)) {
    Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
    return dynamicPaint_Modifier_do(pmd, ctx->depsgraph, scene, ctx->object, mesh);
  }
  return mesh;
}

static bool is_brush_cb(Object * /*ob*/, ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
  return (pmd->brush != nullptr && pmd->type == MOD_DYNAMICPAINT_TYPE_BRUSH);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
  /* Add relation from canvases to all brush objects. */
  if (pmd->canvas != nullptr && pmd->type == MOD_DYNAMICPAINT_TYPE_CANVAS) {
    LISTBASE_FOREACH (DynamicPaintSurface *, surface, &pmd->canvas->surfaces) {
      if (surface->effect & MOD_DPAINT_EFFECT_DO_DRIP) {
        DEG_add_forcefield_relations(
            ctx->node, ctx->object, surface->effector_weights, true, 0, "Dynamic Paint Field");
      }

      /* Actual code uses custom loop over group/scene
       * without layer checks in dynamicPaint_doStep. */
      DEG_add_collision_relations(ctx->node,
                                  ctx->object,
                                  surface->brush_group,
                                  eModifierType_DynamicPaint,
                                  is_brush_cb,
                                  "Dynamic Paint Brush");
    }
  }
}

static bool dependsOnTime(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  if (pmd->canvas) {
    DynamicPaintSurface *surface = static_cast<DynamicPaintSurface *>(pmd->canvas->surfaces.first);

    for (; surface; surface = surface->next) {
      walk(userData, ob, (ID **)&surface->brush_group, IDWALK_CB_NOP);
      walk(userData, ob, (ID **)&surface->init_texture, IDWALK_CB_USER);
      if (surface->effector_weights) {
        walk(userData, ob, (ID **)&surface->effector_weights->group, IDWALK_CB_USER);
      }
    }
  }
}

static void foreachTexLink(ModifierData * /*md*/,
                           Object * /*ob*/,
                           TexWalkFunc /*walk*/,
                           void * /*userData*/)
{
  // walk(userData, ob, md, ""); /* re-enable when possible */
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiItemL(layout, TIP_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_DynamicPaint, panel_draw);
}

ModifierTypeInfo modifierType_DynamicPaint = {
    /*name*/ N_("Dynamic Paint"),
    /*structName*/ "DynamicPaintModifierData",
    /*structSize*/ sizeof(DynamicPaintModifierData),
    /*srna*/ &RNA_DynamicPaintModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_UsesPointCache | eModifierTypeFlag_Single |
        eModifierTypeFlag_UsesPreview,
    /*icon*/ ICON_MOD_DYNAMICPAINT,

    /*copyData*/ copyData,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ modifyMesh,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ freeData,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ foreachTexLink,
    /*freeRuntimeData*/ freeRuntimeData,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
