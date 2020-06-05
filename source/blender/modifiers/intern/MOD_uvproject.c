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

/* UV Project modifier: Generates UVs projected from an object */

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_uvproject.h"

#include "BLT_translation.h"

#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void initData(ModifierData *md)
{
  UVProjectModifierData *umd = (UVProjectModifierData *)md;

  umd->num_projectors = 1;
  umd->aspectx = umd->aspecty = 1.0f;
  umd->scalex = umd->scaley = 1.0f;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
{
  /* ask for UV coordinates */
  r_cddata_masks->lmask |= CD_MLOOPUV;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  UVProjectModifierData *umd = (UVProjectModifierData *)md;
  int i;

  for (i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; i++) {
    walk(userData, ob, &umd->projectors[i], IDWALK_CB_NOP);
  }
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
#if 0
  UVProjectModifierData *umd = (UVProjectModifierData *)md;
#endif

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  UVProjectModifierData *umd = (UVProjectModifierData *)md;
  bool do_add_own_transform = false;
  for (int i = 0; i < umd->num_projectors; i++) {
    if (umd->projectors[i] != NULL) {
      DEG_add_object_relation(
          ctx->node, umd->projectors[i], DEG_OB_COMP_TRANSFORM, "UV Project Modifier");
      do_add_own_transform = true;
    }
  }
  if (do_add_own_transform) {
    DEG_add_modifier_to_transform_relation(ctx->node, "UV Project Modifier");
  }
}

typedef struct Projector {
  Object *ob;          /* object this projector is derived from */
  float projmat[4][4]; /* projection matrix */
  float normal[3];     /* projector normal in world space */
  void *uci;           /* optional uv-project info (panorama projection) */
} Projector;

static Mesh *uvprojectModifier_do(UVProjectModifierData *umd,
                                  const ModifierEvalContext *UNUSED(ctx),
                                  Object *ob,
                                  Mesh *mesh)
{
  float(*coords)[3], (*co)[3];
  MLoopUV *mloop_uv;
  int i, numVerts, numPolys, numLoops;
  MPoly *mpoly, *mp;
  MLoop *mloop;
  Projector projectors[MOD_UVPROJECT_MAXPROJECTORS];
  int num_projectors = 0;
  char uvname[MAX_CUSTOMDATA_LAYER_NAME];
  float aspx = umd->aspectx ? umd->aspectx : 1.0f;
  float aspy = umd->aspecty ? umd->aspecty : 1.0f;
  float scax = umd->scalex ? umd->scalex : 1.0f;
  float scay = umd->scaley ? umd->scaley : 1.0f;
  int free_uci = 0;

  for (i = 0; i < umd->num_projectors; i++) {
    if (umd->projectors[i] != NULL) {
      projectors[num_projectors++].ob = umd->projectors[i];
    }
  }

  if (num_projectors == 0) {
    return mesh;
  }

  /* make sure there are UV Maps available */

  if (!CustomData_has_layer(&mesh->ldata, CD_MLOOPUV)) {
    return mesh;
  }

  /* make sure we're using an existing layer */
  CustomData_validate_layer_name(&mesh->ldata, CD_MLOOPUV, umd->uvlayer_name, uvname);

  /* calculate a projection matrix and normal for each projector */
  for (i = 0; i < num_projectors; i++) {
    float tmpmat[4][4];
    float offsetmat[4][4];
    Camera *cam = NULL;
    /* calculate projection matrix */
    invert_m4_m4(projectors[i].projmat, projectors[i].ob->obmat);

    projectors[i].uci = NULL;

    if (projectors[i].ob->type == OB_CAMERA) {
      cam = (Camera *)projectors[i].ob->data;
      if (cam->type == CAM_PANO) {
        projectors[i].uci = BLI_uvproject_camera_info(projectors[i].ob, NULL, aspx, aspy);
        BLI_uvproject_camera_info_scale(projectors[i].uci, scax, scay);
        free_uci = 1;
      }
      else {
        CameraParams params;

        /* setup parameters */
        BKE_camera_params_init(&params);
        BKE_camera_params_from_object(&params, projectors[i].ob);

        /* compute matrix, viewplane, .. */
        BKE_camera_params_compute_viewplane(&params, 1, 1, aspx, aspy);

        /* scale the view-plane */
        params.viewplane.xmin *= scax;
        params.viewplane.xmax *= scax;
        params.viewplane.ymin *= scay;
        params.viewplane.ymax *= scay;

        BKE_camera_params_compute_matrix(&params);
        mul_m4_m4m4(tmpmat, params.winmat, projectors[i].projmat);
      }
    }
    else {
      copy_m4_m4(tmpmat, projectors[i].projmat);
    }

    unit_m4(offsetmat);
    mul_mat3_m4_fl(offsetmat, 0.5);
    offsetmat[3][0] = offsetmat[3][1] = offsetmat[3][2] = 0.5;

    mul_m4_m4m4(projectors[i].projmat, offsetmat, tmpmat);

    /* calculate worldspace projector normal (for best projector test) */
    projectors[i].normal[0] = 0;
    projectors[i].normal[1] = 0;
    projectors[i].normal[2] = 1;
    mul_mat3_m4_v3(projectors[i].ob->obmat, projectors[i].normal);
  }

  numPolys = mesh->totpoly;
  numLoops = mesh->totloop;

  /* make sure we are not modifying the original UV map */
  mloop_uv = CustomData_duplicate_referenced_layer_named(
      &mesh->ldata, CD_MLOOPUV, uvname, numLoops);

  coords = BKE_mesh_vert_coords_alloc(mesh, &numVerts);

  /* convert coords to world space */
  for (i = 0, co = coords; i < numVerts; i++, co++) {
    mul_m4_v3(ob->obmat, *co);
  }

  /* if only one projector, project coords to UVs */
  if (num_projectors == 1 && projectors[0].uci == NULL) {
    for (i = 0, co = coords; i < numVerts; i++, co++) {
      mul_project_m4_v3(projectors[0].projmat, *co);
    }
  }

  mpoly = mesh->mpoly;
  mloop = mesh->mloop;

  /* apply coords as UVs */
  for (i = 0, mp = mpoly; i < numPolys; i++, mp++) {
    if (num_projectors == 1) {
      if (projectors[0].uci) {
        uint fidx = mp->totloop - 1;
        do {
          uint lidx = mp->loopstart + fidx;
          uint vidx = mloop[lidx].v;
          BLI_uvproject_from_camera(mloop_uv[lidx].uv, coords[vidx], projectors[0].uci);
        } while (fidx--);
      }
      else {
        /* apply transformed coords as UVs */
        uint fidx = mp->totloop - 1;
        do {
          uint lidx = mp->loopstart + fidx;
          uint vidx = mloop[lidx].v;
          copy_v2_v2(mloop_uv[lidx].uv, coords[vidx]);
        } while (fidx--);
      }
    }
    else {
      /* multiple projectors, select the closest to face normal direction */
      float face_no[3];
      int j;
      Projector *best_projector;
      float best_dot;

      /* get the untransformed face normal */
      BKE_mesh_calc_poly_normal_coords(
          mp, mloop + mp->loopstart, (const float(*)[3])coords, face_no);

      /* find the projector which the face points at most directly
       * (projector normal with largest dot product is best)
       */
      best_dot = dot_v3v3(projectors[0].normal, face_no);
      best_projector = &projectors[0];

      for (j = 1; j < num_projectors; j++) {
        float tmp_dot = dot_v3v3(projectors[j].normal, face_no);
        if (tmp_dot > best_dot) {
          best_dot = tmp_dot;
          best_projector = &projectors[j];
        }
      }

      if (best_projector->uci) {
        uint fidx = mp->totloop - 1;
        do {
          uint lidx = mp->loopstart + fidx;
          uint vidx = mloop[lidx].v;
          BLI_uvproject_from_camera(mloop_uv[lidx].uv, coords[vidx], best_projector->uci);
        } while (fidx--);
      }
      else {
        uint fidx = mp->totloop - 1;
        do {
          uint lidx = mp->loopstart + fidx;
          uint vidx = mloop[lidx].v;
          mul_v2_project_m4_v3(mloop_uv[lidx].uv, best_projector->projmat, coords[vidx]);
        } while (fidx--);
      }
    }
  }

  MEM_freeN(coords);

  if (free_uci) {
    int j;
    for (j = 0; j < num_projectors; j++) {
      if (projectors[j].uci) {
        MEM_freeN(projectors[j].uci);
      }
    }
  }

  /* Mark tessellated CD layers as dirty. */
  mesh->runtime.cd_dirty_vert |= CD_MASK_TESSLOOPNORMAL;

  return mesh;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  UVProjectModifierData *umd = (UVProjectModifierData *)md;

  result = uvprojectModifier_do(umd, ctx, ctx->object, mesh);

  return result;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");

  uiLayoutSetPropSep(layout, true);

  uiItemPointerR(layout, &ptr, "uv_layer", &obj_data_ptr, "uv_layers", NULL, ICON_NONE);

  sub = uiLayoutColumn(layout, true);
  uiItemR(sub, &ptr, "aspect_x", 0, IFACE_("Aspect X"), ICON_NONE);
  uiItemR(sub, &ptr, "aspect_y", 0, IFACE_("Y"), ICON_NONE);

  sub = uiLayoutColumn(layout, true);
  uiItemR(sub, &ptr, "scale_x", 0, IFACE_("Scale X"), ICON_NONE);
  uiItemR(sub, &ptr, "scale_y", 0, IFACE_("Y"), ICON_NONE);

  uiItemR(layout, &ptr, "projector_count", 0, IFACE_("Projectors"), ICON_NONE);
  RNA_BEGIN (&ptr, projector_ptr, "projectors") {
    uiItemR(layout, &projector_ptr, "object", 0, NULL, ICON_NONE);
  }
  RNA_END;

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_UVProject, panel_draw);
}

ModifierTypeInfo modifierType_UVProject = {
    /* name */ "UVProject",
    /* structName */ "UVProjectModifierData",
    /* structSize */ sizeof(UVProjectModifierData),
    /* type */ eModifierTypeType_NonGeometrical,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

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
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};
