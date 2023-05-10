/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_meshcache_util.hh" /* utility functions */
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void initData(ModifierData *md)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mcmd, modifier));

  MEMCPY_STRUCT_AFTER(mcmd, DNA_struct_default_get(MeshCacheModifierData), modifier);
}

static bool dependsOnTime(Scene * /*scene*/, ModifierData *md)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
  return (mcmd->play_mode == MOD_MESHCACHE_PLAY_CFEA);
}

static bool isDisabled(const Scene * /*scene*/, ModifierData *md, bool /*useRenderParams*/)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;

  /* leave it up to the modifier to check the file is valid on calculation */
  return (mcmd->factor <= 0.0f) || (mcmd->filepath[0] == '\0');
}

static void meshcache_do(MeshCacheModifierData *mcmd,
                         Scene *scene,
                         Object *ob,
                         Mesh *mesh,
                         float (*vertexCos_Real)[3],
                         int verts_num)
{
  const bool use_factor = mcmd->factor < 1.0f;
  int influence_group_index;
  const MDeformVert *dvert;
  MOD_get_vgroup(ob, mesh, mcmd->defgrp_name, &dvert, &influence_group_index);

  float(*vertexCos_Store)[3] = (use_factor || influence_group_index != -1 ||
                                (mcmd->deform_mode == MOD_MESHCACHE_DEFORM_INTEGRATE)) ?
                                   static_cast<float(*)[3]>(MEM_malloc_arrayN(
                                       verts_num, sizeof(*vertexCos_Store), __func__)) :
                                   nullptr;
  float(*vertexCos)[3] = vertexCos_Store ? vertexCos_Store : vertexCos_Real;

  const float fps = FPS;

  char filepath[FILE_MAX];
  const char *err_str = nullptr;
  bool ok;

  float time;

  /* -------------------------------------------------------------------- */
  /* Interpret Time (the reading functions also do some of this ) */
  if (mcmd->play_mode == MOD_MESHCACHE_PLAY_CFEA) {
    const float ctime = BKE_scene_ctime_get(scene);

    switch (mcmd->time_mode) {
      case MOD_MESHCACHE_TIME_FRAME: {
        time = ctime;
        break;
      }
      case MOD_MESHCACHE_TIME_SECONDS: {
        time = ctime / fps;
        break;
      }
      case MOD_MESHCACHE_TIME_FACTOR:
      default: {
        time = ctime / fps;
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
  STRNCPY(filepath, mcmd->filepath);
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL((ID *)ob));

  switch (mcmd->type) {
    case MOD_MESHCACHE_TYPE_MDD:
      ok = MOD_meshcache_read_mdd_times(
          filepath, vertexCos, verts_num, mcmd->interp, time, fps, mcmd->time_mode, &err_str);
      break;
    case MOD_MESHCACHE_TYPE_PC2:
      ok = MOD_meshcache_read_pc2_times(
          filepath, vertexCos, verts_num, mcmd->interp, time, fps, mcmd->time_mode, &err_str);
      break;
    default:
      ok = false;
      break;
  }

  /* -------------------------------------------------------------------- */
  /* tricky shape key integration (slow!) */
  if (mcmd->deform_mode == MOD_MESHCACHE_DEFORM_INTEGRATE) {
    Mesh *me = static_cast<Mesh *>(ob->data);

    /* we could support any object type */
    if (UNLIKELY(ob->type != OB_MESH)) {
      BKE_modifier_set_error(ob, &mcmd->modifier, "'Integrate' only valid for Mesh objects");
    }
    else if (UNLIKELY(me->totvert != verts_num)) {
      BKE_modifier_set_error(ob, &mcmd->modifier, "'Integrate' original mesh vertex mismatch");
    }
    else if (UNLIKELY(me->totpoly == 0)) {
      BKE_modifier_set_error(ob, &mcmd->modifier, "'Integrate' requires faces");
    }
    else {
      float(*vertexCos_New)[3] = static_cast<float(*)[3]>(
          MEM_malloc_arrayN(verts_num, sizeof(*vertexCos_New), __func__));

      BKE_mesh_calc_relative_deform(
          BKE_mesh_poly_offsets(me),
          me->totpoly,
          BKE_mesh_corner_verts(me),
          me->totvert,
          BKE_mesh_vert_positions(me),       /* From the original Mesh. */
          (const float(*)[3])vertexCos_Real, /* the input we've been given (shape keys!) */
          (const float(*)[3])vertexCos,      /* The result of this modifier. */
          vertexCos_New                      /* The result of this function. */
      );

      /* write the corrected locations back into the result */
      memcpy(vertexCos, vertexCos_New, sizeof(*vertexCos) * verts_num);

      MEM_freeN(vertexCos_New);
    }
  }

  /* -------------------------------------------------------------------- */
  /* Apply the transformation matrix (if needed) */
  if (UNLIKELY(err_str)) {
    BKE_modifier_set_error(ob, &mcmd->modifier, "%s", err_str);
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
      for (i = 0; i < verts_num; i++) {
        mul_m3_v3(mat, vertexCos[i]);
      }
    }
  }

  if (vertexCos_Store) {
    if (ok) {
      if (influence_group_index != -1) {
        const float global_factor = (mcmd->flag & MOD_MESHCACHE_INVERT_VERTEX_GROUP) ?
                                        -mcmd->factor :
                                        mcmd->factor;
        const float global_offset = (mcmd->flag & MOD_MESHCACHE_INVERT_VERTEX_GROUP) ?
                                        mcmd->factor :
                                        0.0f;
        if (BKE_mesh_deform_verts(mesh) != nullptr) {
          for (int i = 0; i < verts_num; i++) {
            /* For each vertex, compute its blending factor between the mesh cache (for `fac = 0`)
             * and the former position of the vertex (for `fac = 1`). */
            const MDeformVert *currentIndexDVert = dvert + i;
            const float local_vertex_fac = global_offset +
                                           BKE_defvert_find_weight(currentIndexDVert,
                                                                   influence_group_index) *
                                               global_factor;
            interp_v3_v3v3(
                vertexCos_Real[i], vertexCos_Real[i], vertexCos_Store[i], local_vertex_fac);
          }
        }
      }
      else if (use_factor) {
        /* Influence_group_index is -1. */
        interp_vn_vn(*vertexCos_Real, *vertexCos_Store, mcmd->factor, verts_num * 3);
      }
      else {
        memcpy(vertexCos_Real, vertexCos_Store, sizeof(*vertexCos_Store) * verts_num);
      }
    }

    MEM_freeN(vertexCos_Store);
  }
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int verts_num)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  Mesh *mesh_src = nullptr;

  if (ctx->object->type == OB_MESH && mcmd->defgrp_name[0] != '\0') {
    /* `mesh_src` is only needed for vertex groups. */
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, nullptr, mesh, nullptr, verts_num, false);
  }
  meshcache_do(mcmd, scene, ctx->object, mesh_src, vertexCos, verts_num);

  if (!ELEM(mesh_src, nullptr, mesh)) {
    BKE_id_free(nullptr, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          BMEditMesh *editData,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int verts_num)
{
  MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

  Mesh *mesh_src = nullptr;

  if (ctx->object->type == OB_MESH && mcmd->defgrp_name[0] != '\0') {
    /* `mesh_src` is only needed for vertex groups. */
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, nullptr, verts_num, false);
  }
  if (mesh_src != nullptr) {
    BKE_mesh_wrapper_ensure_mdata(mesh_src);
  }

  meshcache_do(mcmd, scene, ctx->object, mesh_src, vertexCos, verts_num);

  if (!ELEM(mesh_src, nullptr, mesh)) {
    BKE_id_free(nullptr, mesh_src);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "cache_format", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "filepath", 0, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "factor", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "deform_mode", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "interpolation", 0, nullptr, ICON_NONE);
  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  modifier_panel_end(layout, ptr);
}

static void time_remapping_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "time_mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "play_mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  if (RNA_enum_get(ptr, "play_mode") == MOD_MESHCACHE_PLAY_CFEA) {
    uiItemR(layout, ptr, "frame_start", 0, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "frame_scale", 0, nullptr, ICON_NONE);
  }
  else { /* play_mode == MOD_MESHCACHE_PLAY_EVAL */
    int time_mode = RNA_enum_get(ptr, "time_mode");
    if (time_mode == MOD_MESHCACHE_TIME_FRAME) {
      uiItemR(layout, ptr, "eval_frame", 0, nullptr, ICON_NONE);
    }
    else if (time_mode == MOD_MESHCACHE_TIME_SECONDS) {
      uiItemR(layout, ptr, "eval_time", 0, nullptr, ICON_NONE);
    }
    else { /* time_mode == MOD_MESHCACHE_TIME_FACTOR */
      uiItemR(layout, ptr, "eval_factor", 0, nullptr, ICON_NONE);
    }
  }
}

static void axis_mapping_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, true);
  uiLayoutSetRedAlert(col, RNA_enum_get(ptr, "forward_axis") == RNA_enum_get(ptr, "up_axis"));
  uiItemR(col, ptr, "forward_axis", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "up_axis", 0, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "flip_axis", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_MeshCache, panel_draw);
  modifier_subpanel_register(region_type,
                             "time_remapping",
                             "Time Remapping",
                             nullptr,
                             time_remapping_panel_draw,
                             panel_type);
  modifier_subpanel_register(
      region_type, "axis_mapping", "Axis Mapping", nullptr, axis_mapping_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_MeshCache = {
    /*name*/ N_("MeshCache"),
    /*structName*/ "MeshCacheModifierData",
    /*structSize*/ sizeof(MeshCacheModifierData),
    /*srna*/ &RNA_MeshCacheModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_MESHDEFORM, /* TODO: Use correct icon. */

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ deformVertsEM,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ nullptr,
    /*freeData*/ nullptr,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ nullptr,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
