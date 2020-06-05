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

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_meshcache_util.h" /* utility functions */
#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

static void initData(ModifierData *md)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;

  mcmd->flag = 0;
  mcmd->type = MOD_MESHCACHE_TYPE_MDD;
  mcmd->interp = MOD_MESHCACHE_INTERP_LINEAR;
  mcmd->frame_scale = 1.0f;

  mcmd->factor = 1.0f;

  /* (Y, Z). Blender default */
  mcmd->forward_axis = 1;
  mcmd->up_axis = 2;
}

static bool dependsOnTime(ModifierData *md)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
  return (mcmd->play_mode == MOD_MESHCACHE_PLAY_CFEA);
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;

  /* leave it up to the modifier to check the file is valid on calculation */
  return (mcmd->factor <= 0.0f) || (mcmd->filepath[0] == '\0');
}

static void meshcache_do(MeshCacheModifierData *mcmd,
                         Scene *scene,
                         Object *ob,
                         float (*vertexCos_Real)[3],
                         int numVerts)
{
  const bool use_factor = mcmd->factor < 1.0f;
  float(*vertexCos_Store)[3] = (use_factor ||
                                (mcmd->deform_mode == MOD_MESHCACHE_DEFORM_INTEGRATE)) ?
                                   MEM_malloc_arrayN(
                                       numVerts, sizeof(*vertexCos_Store), __func__) :
                                   NULL;
  float(*vertexCos)[3] = vertexCos_Store ? vertexCos_Store : vertexCos_Real;

  const float fps = FPS;

  char filepath[FILE_MAX];
  const char *err_str = NULL;
  bool ok;

  float time;

  /* -------------------------------------------------------------------- */
  /* Interpret Time (the reading functions also do some of this ) */
  if (mcmd->play_mode == MOD_MESHCACHE_PLAY_CFEA) {
    const float cfra = BKE_scene_frame_get(scene);

    switch (mcmd->time_mode) {
      case MOD_MESHCACHE_TIME_FRAME: {
        time = cfra;
        break;
      }
      case MOD_MESHCACHE_TIME_SECONDS: {
        time = cfra / fps;
        break;
      }
      case MOD_MESHCACHE_TIME_FACTOR:
      default: {
        time = cfra / fps;
        break;
      }
    }

    /* apply offset and scale */
    time = (mcmd->frame_scale * time) - mcmd->frame_start;
  }
  else { /*  if (mcmd->play_mode == MOD_MESHCACHE_PLAY_EVAL) { */
    switch (mcmd->time_mode) {
      case MOD_MESHCACHE_TIME_FRAME: {
        time = mcmd->eval_frame;
        break;
      }
      case MOD_MESHCACHE_TIME_SECONDS: {
        time = mcmd->eval_time;
        break;
      }
      case MOD_MESHCACHE_TIME_FACTOR:
      default: {
        time = mcmd->eval_factor;
        break;
      }
    }
  }

  /* -------------------------------------------------------------------- */
  /* Read the File (or error out when the file is bad) */

  /* would be nice if we could avoid doing this _every_ frame */
  BLI_strncpy(filepath, mcmd->filepath, sizeof(filepath));
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL((ID *)ob));

  switch (mcmd->type) {
    case MOD_MESHCACHE_TYPE_MDD:
      ok = MOD_meshcache_read_mdd_times(
          filepath, vertexCos, numVerts, mcmd->interp, time, fps, mcmd->time_mode, &err_str);
      break;
    case MOD_MESHCACHE_TYPE_PC2:
      ok = MOD_meshcache_read_pc2_times(
          filepath, vertexCos, numVerts, mcmd->interp, time, fps, mcmd->time_mode, &err_str);
      break;
    default:
      ok = false;
      break;
  }

  /* -------------------------------------------------------------------- */
  /* tricky shape key integration (slow!) */
  if (mcmd->deform_mode == MOD_MESHCACHE_DEFORM_INTEGRATE) {
    Mesh *me = ob->data;

    /* we could support any object type */
    if (UNLIKELY(ob->type != OB_MESH)) {
      BKE_modifier_set_error(&mcmd->modifier, "'Integrate' only valid for Mesh objects");
    }
    else if (UNLIKELY(me->totvert != numVerts)) {
      BKE_modifier_set_error(&mcmd->modifier, "'Integrate' original mesh vertex mismatch");
    }
    else if (UNLIKELY(me->totpoly == 0)) {
      BKE_modifier_set_error(&mcmd->modifier, "'Integrate' requires faces");
    }
    else {
      /* the moons align! */
      int i;

      float(*vertexCos_Source)[3] = MEM_malloc_arrayN(
          numVerts, sizeof(*vertexCos_Source), __func__);
      float(*vertexCos_New)[3] = MEM_malloc_arrayN(numVerts, sizeof(*vertexCos_New), __func__);
      MVert *mv = me->mvert;

      for (i = 0; i < numVerts; i++, mv++) {
        copy_v3_v3(vertexCos_Source[i], mv->co);
      }

      BKE_mesh_calc_relative_deform(
          me->mpoly,
          me->totpoly,
          me->mloop,
          me->totvert,

          (const float(*)[3])vertexCos_Source, /* from the original Mesh*/
          (const float(*)[3])vertexCos_Real,   /* the input we've been given (shape keys!) */

          (const float(*)[3])vertexCos, /* the result of this modifier */
          vertexCos_New                 /* the result of this function */
      );

      /* write the corrected locations back into the result */
      memcpy(vertexCos, vertexCos_New, sizeof(*vertexCos) * numVerts);

      MEM_freeN(vertexCos_Source);
      MEM_freeN(vertexCos_New);
    }
  }

  /* -------------------------------------------------------------------- */
  /* Apply the transformation matrix (if needed) */
  if (UNLIKELY(err_str)) {
    BKE_modifier_set_error(&mcmd->modifier, "%s", err_str);
  }
  else if (ok) {
    bool use_matrix = false;
    float mat[3][3];
    unit_m3(mat);

    if (mat3_from_axis_conversion(mcmd->forward_axis, mcmd->up_axis, 1, 2, mat)) {
      use_matrix = true;
    }

    if (mcmd->flip_axis) {
      float tmat[3][3];
      unit_m3(tmat);
      if (mcmd->flip_axis & (1 << 0)) {
        tmat[0][0] = -1.0f;
      }
      if (mcmd->flip_axis & (1 << 1)) {
        tmat[1][1] = -1.0f;
      }
      if (mcmd->flip_axis & (1 << 2)) {
        tmat[2][2] = -1.0f;
      }
      mul_m3_m3m3(mat, tmat, mat);

      use_matrix = true;
    }

    if (use_matrix) {
      int i;
      for (i = 0; i < numVerts; i++) {
        mul_m3_v3(mat, vertexCos[i]);
      }
    }
  }

  if (vertexCos_Store) {
    if (ok) {
      if (use_factor) {
        interp_vn_vn(*vertexCos_Real, *vertexCos_Store, mcmd->factor, numVerts * 3);
      }
      else {
        memcpy(vertexCos_Real, vertexCos_Store, sizeof(*vertexCos_Store) * numVerts);
      }
    }

    MEM_freeN(vertexCos_Store);
  }
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *UNUSED(mesh),
                        float (*vertexCos)[3],
                        int numVerts)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  meshcache_do(mcmd, scene, ctx->object, vertexCos, numVerts);
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *UNUSED(editData),
                          Mesh *UNUSED(mesh),
                          float (*vertexCos)[3],
                          int numVerts)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  meshcache_do(mcmd, scene, ctx->object, vertexCos, numVerts);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "cache_format", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "filepath", 0, NULL, ICON_NONE);

  uiItemR(layout, &ptr, "factor", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "deform_mode", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "interpolation", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void time_remapping_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiItemR(layout, &ptr, "time_mode", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "play_mode", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  if (RNA_enum_get(&ptr, "play_mode") == MOD_MESHCACHE_PLAY_CFEA) {
    uiItemR(layout, &ptr, "frame_start", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "frame_scale", 0, NULL, ICON_NONE);
  }
  else { /* play_mode == MOD_MESHCACHE_PLAY_EVAL */
    int time_mode = RNA_enum_get(&ptr, "time_mode");
    if (time_mode == MOD_MESHCACHE_TIME_FRAME) {
      uiItemR(layout, &ptr, "eval_frame", 0, NULL, ICON_NONE);
    }
    else if (time_mode == MOD_MESHCACHE_TIME_SECONDS) {
      uiItemR(layout, &ptr, "eval_time", 0, NULL, ICON_NONE);
    }
    else { /* time_mode == MOD_MESHCACHE_TIME_FACTOR */
      uiItemR(layout, &ptr, "eval_factor", 0, NULL, ICON_NONE);
    }
  }
}

static void axis_mapping_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, true);
  uiLayoutSetRedAlert(col, RNA_enum_get(&ptr, "forward_axis") == RNA_enum_get(&ptr, "up_axis"));
  uiItemR(col, &ptr, "forward_axis", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "up_axis", 0, NULL, ICON_NONE);

  uiItemR(layout, &ptr, "flip_axis", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_MeshCache, panel_draw);
  modifier_subpanel_register(region_type,
                             "time_remapping",
                             "Time Remapping",
                             NULL,
                             time_remapping_panel_draw,
                             panel_type);
  modifier_subpanel_register(
      region_type, "axis_mapping", "Axis Mapping", NULL, axis_mapping_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_MeshCache = {
    /* name */ "MeshCache",
    /* structName */ "MeshCacheModifierData",
    /* structSize */ sizeof(MeshCacheModifierData),
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,

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
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};
