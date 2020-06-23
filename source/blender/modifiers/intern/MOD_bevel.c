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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_curveprofile_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_curveprofile.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

#include "BKE_curveprofile.h"
#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph_query.h"

static void initData(ModifierData *md)
{
  BevelModifierData *bmd = (BevelModifierData *)md;

  bmd->value = 0.1f;
  bmd->res = 1;
  bmd->flags = 0;
  bmd->val_flags = MOD_BEVEL_AMT_OFFSET;
  bmd->lim_flags = 0;
  bmd->e_flags = 0;
  bmd->edge_flags = 0;
  bmd->face_str_mode = MOD_BEVEL_FACE_STRENGTH_NONE;
  bmd->miter_inner = MOD_BEVEL_MITER_SHARP;
  bmd->miter_outer = MOD_BEVEL_MITER_SHARP;
  bmd->spread = 0.1f;
  bmd->mat = -1;
  bmd->profile = 0.5f;
  bmd->bevel_angle = DEG2RADF(30.0f);
  bmd->defgrp_name[0] = '\0';
  bmd->custom_profile = BKE_curveprofile_add(PROF_PRESET_LINE);
}

static void copyData(const ModifierData *md_src, ModifierData *md_dst, const int flag)
{
  const BevelModifierData *bmd_src = (const BevelModifierData *)md_src;
  BevelModifierData *bmd_dst = (BevelModifierData *)md_dst;

  BKE_modifier_copydata_generic(md_src, md_dst, flag);
  bmd_dst->custom_profile = BKE_curveprofile_copy(bmd_src->custom_profile);
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  BevelModifierData *bmd = (BevelModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (bmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

/*
 * This calls the new bevel code (added since 2.64)
 */
static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  BMesh *bm;
  BMIter iter;
  BMEdge *e;
  BMVert *v;
  float weight, weight2;
  int vgroup = -1;
  MDeformVert *dvert = NULL;
  BevelModifierData *bmd = (BevelModifierData *)md;
  const float threshold = cosf(bmd->bevel_angle + 0.000000175f);
  const bool vertex_only = (bmd->flags & MOD_BEVEL_VERT) != 0;
  const bool do_clamp = !(bmd->flags & MOD_BEVEL_OVERLAP_OK);
  const int offset_type = bmd->val_flags;
  const int profile_type = bmd->profile_type;
  const float value = bmd->value;
  const int mat = CLAMPIS(bmd->mat, -1, ctx->object->totcol - 1);
  const bool loop_slide = (bmd->flags & MOD_BEVEL_EVEN_WIDTHS) == 0;
  const bool mark_seam = (bmd->edge_flags & MOD_BEVEL_MARK_SEAM);
  const bool mark_sharp = (bmd->edge_flags & MOD_BEVEL_MARK_SHARP);
  bool harden_normals = (bmd->flags & MOD_BEVEL_HARDEN_NORMALS);
  const int face_strength_mode = bmd->face_str_mode;
  const int miter_outer = bmd->miter_outer;
  const int miter_inner = bmd->miter_inner;
  const float spread = bmd->spread;
  const int vmesh_method = bmd->vmesh_method;
  const bool invert_vgroup = (bmd->flags & MOD_BEVEL_INVERT_VGROUP) != 0;

  bm = BKE_mesh_to_bmesh_ex(mesh,
                            &(struct BMeshCreateParams){0},
                            &(struct BMeshFromMeshParams){
                                .calc_face_normal = true,
                                .add_key_index = false,
                                .use_shapekey = false,
                                .active_shapekey = 0,
                                /* XXX We probably can use CD_MASK_BAREMESH_ORIGDINDEX here instead
                                 * (also for other modifiers cases)? */
                                .cd_mask_extra = {.vmask = CD_MASK_ORIGINDEX,
                                                  .emask = CD_MASK_ORIGINDEX,
                                                  .pmask = CD_MASK_ORIGINDEX},
                            });

  if ((bmd->lim_flags & MOD_BEVEL_VGROUP) && bmd->defgrp_name[0]) {
    MOD_get_vgroup(ctx->object, mesh, bmd->defgrp_name, &dvert, &vgroup);
  }

  if (vertex_only) {
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
        weight = BM_elem_float_data_get(&bm->vdata, v, CD_BWEIGHT);
        if (weight == 0.0f) {
          continue;
        }
      }
      else if (vgroup != -1) {
        weight = invert_vgroup ?
                     1.0f -
                         BKE_defvert_array_find_weight_safe(dvert, BM_elem_index_get(v), vgroup) :
                     BKE_defvert_array_find_weight_safe(dvert, BM_elem_index_get(v), vgroup);
        /* Check is against 0.5 rather than != 0.0 because cascaded bevel modifiers will
         * interpolate weights for newly created vertices, and may cause unexpected "selection" */
        if (weight < 0.5f) {
          continue;
        }
      }
      BM_elem_flag_enable(v, BM_ELEM_TAG);
    }
  }
  else if (bmd->lim_flags & MOD_BEVEL_ANGLE) {
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      /* check for 1 edge having 2 face users */
      BMLoop *l_a, *l_b;
      if (BM_edge_loop_pair(e, &l_a, &l_b)) {
        if (dot_v3v3(l_a->f->no, l_b->f->no) < threshold) {
          BM_elem_flag_enable(e, BM_ELEM_TAG);
          BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
          BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
        }
      }
    }
  }
  else {
    /* crummy, is there a way just to operator on all? - campbell */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_edge_is_manifold(e)) {
        if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
          weight = BM_elem_float_data_get(&bm->edata, e, CD_BWEIGHT);
          if (weight == 0.0f) {
            continue;
          }
        }
        else if (vgroup != -1) {
          weight = invert_vgroup ?
                       1.0f - BKE_defvert_array_find_weight_safe(
                                  dvert, BM_elem_index_get(e->v1), vgroup) :
                       BKE_defvert_array_find_weight_safe(dvert, BM_elem_index_get(e->v1), vgroup);
          weight2 = invert_vgroup ? 1.0f - BKE_defvert_array_find_weight_safe(
                                               dvert, BM_elem_index_get(e->v2), vgroup) :
                                    BKE_defvert_array_find_weight_safe(
                                        dvert, BM_elem_index_get(e->v2), vgroup);
          if (weight < 0.5f || weight2 < 0.5f) {
            continue;
          }
        }
        BM_elem_flag_enable(e, BM_ELEM_TAG);
        BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
        BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
      }
    }
  }

  Object *ob = ctx->object;

  if (harden_normals && (ob->type == OB_MESH) && !(((Mesh *)ob->data)->flag & ME_AUTOSMOOTH)) {
    BKE_modifier_set_error(md, "Enable 'Auto Smooth' in Object Data Properties");
    harden_normals = false;
  }

  BM_mesh_bevel(bm,
                value,
                offset_type,
                profile_type,
                bmd->res,
                bmd->profile,
                vertex_only,
                bmd->lim_flags & MOD_BEVEL_WEIGHT,
                do_clamp,
                dvert,
                vgroup,
                mat,
                loop_slide,
                mark_seam,
                mark_sharp,
                harden_normals,
                face_strength_mode,
                miter_outer,
                miter_inner,
                spread,
                mesh->smoothresh,
                bmd->custom_profile,
                vmesh_method);

  result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, mesh);

  /* Make sure we never alloc'd these. */
  BLI_assert(bm->vtoolflagpool == NULL && bm->etoolflagpool == NULL && bm->ftoolflagpool == NULL);

  BM_mesh_free(bm);

  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  return result;
}

static bool dependsOnNormals(ModifierData *UNUSED(md))
{
  return true;
}

static void freeData(ModifierData *md)
{
  BevelModifierData *bmd = (BevelModifierData *)md;
  BKE_curveprofile_free(bmd->custom_profile);
}

static bool isDisabled(const Scene *UNUSED(scene), ModifierData *md, bool UNUSED(userRenderParams))
{
  BevelModifierData *bmd = (BevelModifierData *)md;
  return (bmd->value == 0.0f);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  const char *offset_name = "";
  if (RNA_enum_get(&ptr, "offset_type") == BEVEL_AMT_PERCENT) {
    uiItemR(col, &ptr, "width_pct", 0, NULL, ICON_NONE);
  }
  else {
    switch (RNA_enum_get(&ptr, "offset_type")) {
      case BEVEL_AMT_DEPTH:
        offset_name = "Depth";
        break;
      case BEVEL_AMT_WIDTH:
        offset_name = "Width";
        break;
      case BEVEL_AMT_OFFSET:
        offset_name = "Offset";
        break;
      case BEVEL_AMT_ABSOLUTE:
        offset_name = "Absolute";
        break;
    }
    uiItemR(col, &ptr, "width", 0, IFACE_(offset_name), ICON_NONE);
  }
  uiItemR(col, &ptr, "offset_type", 0, NULL, ICON_NONE);

  uiItemR(layout, &ptr, "segments", 0, NULL, ICON_NONE);

  uiItemS(layout);

  uiItemR(layout, &ptr, "affect", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiItemS(layout);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &ptr, "limit_method", 0, NULL, ICON_NONE);
  int limit_method = RNA_enum_get(&ptr, "limit_method");
  if (limit_method == MOD_BEVEL_ANGLE) {
    uiItemR(col, &ptr, "angle_limit", 0, NULL, ICON_NONE);
  }
  else if (limit_method == MOD_BEVEL_VGROUP) {
    modifier_vgroup_ui(col, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);
  }

  modifier_panel_end(layout, &ptr);
}

static void geometry_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "miter_inner", 0, IFACE_("Miter Inner"), ICON_NONE);
  uiItemR(layout, &ptr, "miter_outer", 0, IFACE_("Outer"), ICON_NONE);
  if (RNA_enum_get(&ptr, "miter_inner") == BEVEL_MITER_ARC) {
    uiItemR(layout, &ptr, "spread", 0, NULL, ICON_NONE);
  }
  uiItemS(layout);

  uiItemR(layout, &ptr, "vmesh_method", 0, IFACE_("Intersections"), ICON_NONE);
  uiItemR(layout, &ptr, "use_clamp_overlap", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "loop_slide", 0, NULL, ICON_NONE);
}

static void shading_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "harden_normals", 0, NULL, ICON_NONE);

  col = uiLayoutColumnWithHeading(layout, true, IFACE_("Mark"));
  uiItemR(col, &ptr, "mark_seam", 0, IFACE_("Seam"), ICON_NONE);
  uiItemR(col, &ptr, "mark_sharp", 0, IFACE_("Sharp"), ICON_NONE);

  uiItemR(layout, &ptr, "material", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "face_strength_mode", 0, NULL, ICON_NONE);
}

static void profile_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  modifier_panel_get_property_pointers(C, panel, NULL, &ptr);

  int profile_type = RNA_enum_get(&ptr, "profile_type");

  uiItemR(layout, &ptr, "profile_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (ELEM(profile_type, MOD_BEVEL_PROFILE_SUPERELLIPSE, MOD_BEVEL_PROFILE_CUSTOM)) {
    uiItemR(layout,
            &ptr,
            "profile",
            UI_ITEM_R_SLIDER,
            (profile_type == MOD_BEVEL_PROFILE_SUPERELLIPSE) ? IFACE_("Shape") :
                                                               IFACE_("Miter Shape"),
            ICON_NONE);

    if (profile_type == MOD_BEVEL_PROFILE_CUSTOM) {
      uiLayout *sub = uiLayoutColumn(layout, false);
      uiLayoutSetPropDecorate(sub, false);
      uiTemplateCurveProfile(sub, &ptr, "custom_profile");
    }
  }
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Bevel, panel_draw);
  modifier_subpanel_register(
      region_type, "profile", "Profile", NULL, profile_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "geometry", "Geometry", NULL, geometry_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "shading", "Shading", NULL, shading_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Bevel = {
    /* name */ "Bevel",
    /* structName */ "BevelModifierData",
    /* structSize */ sizeof(BevelModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_AcceptsCVs,
    /* copyData */ copyData,
    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,
    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* uiPanel */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
