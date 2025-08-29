/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstddef>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_dynamicpaint.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_physics.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_ui_common.hh"

static void init_data(ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(pmd, modifier));

  MEMCPY_STRUCT_AFTER(pmd, DNA_struct_default_get(DynamicPaintModifierData), modifier);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const DynamicPaintModifierData *pmd = (const DynamicPaintModifierData *)md;
  DynamicPaintModifierData *tpmd = (DynamicPaintModifierData *)target;

  dynamicPaint_Modifier_copy(pmd, tpmd, flag);
}

static void free_runtime_data(void *runtime_data_v)
{
  if (runtime_data_v == nullptr) {
    return;
  }
  DynamicPaintRuntime *runtime_data = (DynamicPaintRuntime *)runtime_data_v;
  dynamicPaint_Modifier_free_runtime(runtime_data);
}

static void free_data(ModifierData *md)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
  dynamicPaint_Modifier_free(pmd);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
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

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
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

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
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

static bool depends_on_time(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

  if (pmd->canvas) {
    DynamicPaintSurface *surface = static_cast<DynamicPaintSurface *>(pmd->canvas->surfaces.first);

    for (; surface; surface = surface->next) {
      walk(user_data, ob, (ID **)&surface->brush_group, IDWALK_CB_NOP);
      walk(user_data, ob, (ID **)&surface->init_texture, IDWALK_CB_USER);
      if (surface->effector_weights) {
        walk(user_data, ob, (ID **)&surface->effector_weights->group, IDWALK_CB_USER);
      }
    }
  }
}

static void foreach_tex_link(ModifierData * /*md*/,
                             Object * /*ob*/,
                             TexWalkFunc /*walk*/,
                             void * /*user_data*/)
{
  // walk(user_data, ob, md, ""); /* re-enable when possible */
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->label(RPT_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_DynamicPaint, panel_draw);
}

ModifierTypeInfo modifierType_DynamicPaint = {
    /*idname*/ "Dynamic Paint",
    /*name*/ N_("Dynamic Paint"),
    /*struct_name*/ "DynamicPaintModifierData",
    /*struct_size*/ sizeof(DynamicPaintModifierData),
    /*srna*/ &RNA_DynamicPaintModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_UsesPointCache | eModifierTypeFlag_Single,
    /*icon*/ ICON_MOD_DYNAMICPAINT,

    /*copy_data*/ copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ foreach_tex_link,
    /*free_runtime_data*/ free_runtime_data,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
