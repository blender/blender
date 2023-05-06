/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_texture.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MEM_guardedalloc.h"

#include "RE_texture.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"
#include "MOD_util.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

static void initData(ModifierData *md)
{
  WaveModifierData *wmd = (WaveModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wmd, modifier));

  MEMCPY_STRUCT_AFTER(wmd, DNA_struct_default_get(WaveModifierData), modifier);
}

static bool dependsOnTime(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  WaveModifierData *wmd = (WaveModifierData *)md;

  walk(userData, ob, (ID **)&wmd->texture, IDWALK_CB_USER);
  walk(userData, ob, (ID **)&wmd->objectcenter, IDWALK_CB_NOP);
  walk(userData, ob, (ID **)&wmd->map_object, IDWALK_CB_NOP);
}

static void foreachTexLink(ModifierData *md, Object *ob, TexWalkFunc walk, void *userData)
{
  walk(userData, ob, md, "texture");
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  WaveModifierData *wmd = (WaveModifierData *)md;
  bool need_transform_relation = false;

  if (wmd->objectcenter != nullptr) {
    DEG_add_object_relation(ctx->node, wmd->objectcenter, DEG_OB_COMP_TRANSFORM, "Wave Modifier");
    need_transform_relation = true;
  }

  if (wmd->texture != nullptr) {
    DEG_add_generic_id_relation(ctx->node, &wmd->texture->id, "Wave Modifier");

    if ((wmd->texmapping == MOD_DISP_MAP_OBJECT) && wmd->map_object != nullptr) {
      MOD_depsgraph_update_object_bone_relation(
          ctx->node, wmd->map_object, wmd->map_bone, "Wave Modifier");
      need_transform_relation = true;
    }
    else if (wmd->texmapping == MOD_DISP_MAP_GLOBAL) {
      need_transform_relation = true;
    }
  }

  if (need_transform_relation) {
    DEG_add_depends_on_transform_relation(ctx->node, "Wave Modifier");
  }
}

static void requiredDataMask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  WaveModifierData *wmd = (WaveModifierData *)md;

  /* ask for UV coordinates if we need them */
  if (wmd->texture && wmd->texmapping == MOD_DISP_MAP_UV) {
    r_cddata_masks->fmask |= CD_MASK_MTFACE;
  }

  /* ask for vertexgroups if we need them */
  if (wmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool dependsOnNormals(ModifierData *md)
{
  WaveModifierData *wmd = (WaveModifierData *)md;

  return (wmd->flag & MOD_WAVE_NORM) != 0;
}

static void waveModifier_do(WaveModifierData *md,
                            const ModifierEvalContext *ctx,
                            Object *ob,
                            Mesh *mesh,
                            float (*vertexCos)[3],
                            int verts_num)
{
  WaveModifierData *wmd = (WaveModifierData *)md;
  const MDeformVert *dvert;
  int defgrp_index;
  float ctime = DEG_get_ctime(ctx->depsgraph);
  float minfac = float(1.0 / exp(wmd->width * wmd->narrow * wmd->width * wmd->narrow));
  float lifefac = wmd->height;
  float(*tex_co)[3] = nullptr;
  const int wmd_axis = wmd->flag & (MOD_WAVE_X | MOD_WAVE_Y);
  const float falloff = wmd->falloff;
  float falloff_fac = 1.0f; /* when falloff == 0.0f this stays at 1.0f */
  const bool invert_group = (wmd->flag & MOD_WAVE_INVERT_VGROUP) != 0;

  blender::Span<blender::float3> vert_normals;
  if ((wmd->flag & MOD_WAVE_NORM) && (mesh != nullptr)) {
    vert_normals = mesh->vert_normals();
  }

  if (wmd->objectcenter != nullptr) {
    float mat[4][4];
    /* get the control object's location in local coordinates */
    invert_m4_m4(ob->world_to_object, ob->object_to_world);
    mul_m4_m4m4(mat, ob->world_to_object, wmd->objectcenter->object_to_world);

    wmd->startx = mat[3][0];
    wmd->starty = mat[3][1];
  }

  /* get the index of the deform group */
  MOD_get_vgroup(ob, mesh, wmd->defgrp_name, &dvert, &defgrp_index);

  if (wmd->damp == 0.0f) {
    wmd->damp = 10.0f;
  }

  if (wmd->lifetime != 0.0f) {
    float x = ctime - wmd->timeoffs;

    if (x > wmd->lifetime) {
      lifefac = x - wmd->lifetime;

      if (lifefac > wmd->damp) {
        lifefac = 0.0;
      }
      else {
        lifefac = float(wmd->height * (1.0f - sqrtf(lifefac / wmd->damp)));
      }
    }
  }

  Tex *tex_target = wmd->texture;
  if (mesh != nullptr && tex_target != nullptr) {
    tex_co = static_cast<float(*)[3]>(MEM_malloc_arrayN(verts_num, sizeof(*tex_co), __func__));
    MOD_get_texture_coords((MappingInfoModifierData *)wmd, ctx, ob, mesh, vertexCos, tex_co);

    MOD_init_texture((MappingInfoModifierData *)wmd, ctx);
  }

  if (lifefac != 0.0f) {
    /* avoid divide by zero checks within the loop */
    float falloff_inv = falloff != 0.0f ? 1.0f / falloff : 1.0f;
    int i;

    for (i = 0; i < verts_num; i++) {
      float *co = vertexCos[i];
      float x = co[0] - wmd->startx;
      float y = co[1] - wmd->starty;
      float amplit = 0.0f;
      float def_weight = 1.0f;

      /* get weights */
      if (dvert) {
        def_weight = invert_group ? 1.0f - BKE_defvert_find_weight(&dvert[i], defgrp_index) :
                                    BKE_defvert_find_weight(&dvert[i], defgrp_index);

        /* if this vert isn't in the vgroup, don't deform it */
        if (def_weight == 0.0f) {
          continue;
        }
      }

      switch (wmd_axis) {
        case MOD_WAVE_X | MOD_WAVE_Y:
          amplit = sqrtf(x * x + y * y);
          break;
        case MOD_WAVE_X:
          amplit = x;
          break;
        case MOD_WAVE_Y:
          amplit = y;
          break;
      }

      /* this way it makes nice circles */
      amplit -= (ctime - wmd->timeoffs) * wmd->speed;

      if (wmd->flag & MOD_WAVE_CYCL) {
        amplit = float(fmodf(amplit - wmd->width, 2.0f * wmd->width)) + wmd->width;
      }

      if (falloff != 0.0f) {
        float dist = 0.0f;

        switch (wmd_axis) {
          case MOD_WAVE_X | MOD_WAVE_Y:
            dist = sqrtf(x * x + y * y);
            break;
          case MOD_WAVE_X:
            dist = fabsf(x);
            break;
          case MOD_WAVE_Y:
            dist = fabsf(y);
            break;
        }

        falloff_fac = (1.0f - (dist * falloff_inv));
        CLAMP(falloff_fac, 0.0f, 1.0f);
      }

      /* GAUSSIAN */
      if ((falloff_fac != 0.0f) && (amplit > -wmd->width) && (amplit < wmd->width)) {
        amplit = amplit * wmd->narrow;
        amplit = float(1.0f / expf(amplit * amplit) - minfac);

        /* Apply texture. */
        if (tex_co) {
          Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
          TexResult texres;
          BKE_texture_get_value(scene, tex_target, tex_co[i], &texres, false);
          amplit *= texres.tin;
        }

        /* Apply weight & falloff. */
        amplit *= def_weight * falloff_fac;

        if (!vert_normals.is_empty()) {
          /* move along normals */
          if (wmd->flag & MOD_WAVE_NORM_X) {
            co[0] += (lifefac * amplit) * vert_normals[i][0];
          }
          if (wmd->flag & MOD_WAVE_NORM_Y) {
            co[1] += (lifefac * amplit) * vert_normals[i][1];
          }
          if (wmd->flag & MOD_WAVE_NORM_Z) {
            co[2] += (lifefac * amplit) * vert_normals[i][2];
          }
        }
        else {
          /* move along local z axis */
          co[2] += lifefac * amplit;
        }
      }
    }
  }

  MEM_SAFE_FREE(tex_co);
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int verts_num)
{
  WaveModifierData *wmd = (WaveModifierData *)md;
  Mesh *mesh_src = nullptr;

  if (wmd->flag & MOD_WAVE_NORM) {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, nullptr, mesh, vertexCos, verts_num, false);
  }
  else if (wmd->texture != nullptr || wmd->defgrp_name[0] != '\0') {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, nullptr, mesh, nullptr, verts_num, false);
  }

  waveModifier_do(wmd, ctx, ctx->object, mesh_src, vertexCos, verts_num);

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
  WaveModifierData *wmd = (WaveModifierData *)md;
  Mesh *mesh_src = nullptr;

  if (wmd->flag & MOD_WAVE_NORM) {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, vertexCos, verts_num, false);
  }
  else if (wmd->texture != nullptr || wmd->defgrp_name[0] != '\0') {
    mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, nullptr, verts_num, false);
  }

  /* TODO(@ideasman42): use edit-mode data only (remove this line). */
  if (mesh_src != nullptr) {
    BKE_mesh_wrapper_ensure_mdata(mesh_src);
  }

  waveModifier_do(wmd, ctx, ctx->object, mesh_src, vertexCos, verts_num);

  if (!ELEM(mesh_src, nullptr, mesh)) {
    /* Important not to free `vertexCos` owned by the caller. */
    EditMeshData *edit_data = mesh_src->runtime->edit_data;
    if (edit_data->vertexCos == vertexCos) {
      edit_data->vertexCos = nullptr;
    }

    BKE_id_free(nullptr, mesh_src);
  }
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Motion"));
  uiItemR(
      row, ptr, "use_x", UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE, nullptr, ICON_NONE);
  uiItemR(
      row, ptr, "use_y", UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "use_cyclic", 0, nullptr, ICON_NONE);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Along Normals"));
  uiItemR(row, ptr, "use_normal", 0, "", ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_normal"));
  uiItemR(sub, ptr, "use_normal_x", UI_ITEM_R_TOGGLE, "X", ICON_NONE);
  uiItemR(sub, ptr, "use_normal_y", UI_ITEM_R_TOGGLE, "Y", ICON_NONE);
  uiItemR(sub, ptr, "use_normal_z", UI_ITEM_R_TOGGLE, "Z", ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "falloff_radius", 0, IFACE_("Falloff"), ICON_NONE);
  uiItemR(col, ptr, "height", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(col, ptr, "width", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(col, ptr, "narrowness", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);

  modifier_panel_end(layout, ptr);
}

static void position_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "start_position_object", 0, IFACE_("Object"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "start_position_x", 0, IFACE_("Start Position X"), ICON_NONE);
  uiItemR(col, ptr, "start_position_y", 0, "Y", ICON_NONE);
}

static void time_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "time_offset", 0, IFACE_("Offset"), ICON_NONE);
  uiItemR(col, ptr, "lifetime", 0, IFACE_("Life"), ICON_NONE);
  uiItemR(col, ptr, "damping_time", 0, IFACE_("Damping"), ICON_NONE);
  uiItemR(col, ptr, "speed", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

static void texture_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int texture_coords = RNA_enum_get(ptr, "texture_coords");

  uiTemplateID(layout, C, ptr, "texture", "texture.new", nullptr, nullptr, 0, ICON_NONE, nullptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "texture_coords", 0, IFACE_("Coordinates"), ICON_NONE);
  if (texture_coords == MOD_DISP_MAP_OBJECT) {
    uiItemR(col, ptr, "texture_coords_object", 0, IFACE_("Object"), ICON_NONE);
    PointerRNA texture_coords_obj_ptr = RNA_pointer_get(ptr, "texture_coords_object");
    if (!RNA_pointer_is_null(&texture_coords_obj_ptr) &&
        (RNA_enum_get(&texture_coords_obj_ptr, "type") == OB_ARMATURE))
    {
      PointerRNA texture_coords_obj_data_ptr = RNA_pointer_get(&texture_coords_obj_ptr, "data");
      uiItemPointerR(col,
                     ptr,
                     "texture_coords_bone",
                     &texture_coords_obj_data_ptr,
                     "bones",
                     IFACE_("Bone"),
                     ICON_NONE);
    }
  }
  else if (texture_coords == MOD_DISP_MAP_UV && RNA_enum_get(&ob_ptr, "type") == OB_MESH) {
    PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");
    uiItemPointerR(col, ptr, "uv_layer", &obj_data_ptr, "uv_layers", nullptr, ICON_NONE);
  }
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Wave, panel_draw);
  modifier_subpanel_register(
      region_type, "position", "Start Position", nullptr, position_panel_draw, panel_type);
  modifier_subpanel_register(region_type, "time", "Time", nullptr, time_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "texture", "Texture", nullptr, texture_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Wave = {
    /*name*/ N_("Wave"),
    /*structName*/ "WaveModifierData",
    /*structSize*/ sizeof(WaveModifierData),
    /*srna*/ &RNA_WaveModifier,
    /*type*/ eModifierTypeType_OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_WAVE,

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ deformVerts,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ deformVertsEM,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ nullptr,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ dependsOnNormals,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ foreachTexLink,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
