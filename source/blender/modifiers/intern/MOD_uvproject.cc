/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup modifiers
 */

/* UV Project modifier: Generates UVs projected from an object */

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_uvproject.h"

#include "BLT_translation.h"

#include "DNA_camera_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void initData(ModifierData *md)
{
  UVProjectModifierData *umd = (UVProjectModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(umd, modifier));

  MEMCPY_STRUCT_AFTER(umd, DNA_struct_default_get(UVProjectModifierData), modifier);
}

static void requiredDataMask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  /* ask for UV coordinates */
  r_cddata_masks->lmask |= CD_MASK_PROP_FLOAT2;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  UVProjectModifierData *umd = (UVProjectModifierData *)md;
  for (int i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; i++) {
    walk(userData, ob, (ID **)&umd->projectors[i], IDWALK_CB_NOP);
  }
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  UVProjectModifierData *umd = (UVProjectModifierData *)md;
  bool do_add_own_transform = false;
  for (int i = 0; i < umd->projectors_num; i++) {
    if (umd->projectors[i] != nullptr) {
      DEG_add_object_relation(
          ctx->node, umd->projectors[i], DEG_OB_COMP_TRANSFORM, "UV Project Modifier");
      do_add_own_transform = true;
    }
  }
  if (do_add_own_transform) {
    DEG_add_depends_on_transform_relation(ctx->node, "UV Project Modifier");
  }
}

struct Projector {
  Object *ob;          /* object this projector is derived from */
  float projmat[4][4]; /* projection matrix */
  float normal[3];     /* projector normal in world space */
  void *uci;           /* optional uv-project info (panorama projection) */
};

static Mesh *uvprojectModifier_do(UVProjectModifierData *umd,
                                  const ModifierEvalContext * /*ctx*/,
                                  Object *ob,
                                  Mesh *mesh)
{
  using namespace blender;
  float(*coords)[3], (*co)[3];
  int i, verts_num;
  Projector projectors[MOD_UVPROJECT_MAXPROJECTORS];
  int projectors_num = 0;
  char uvname[MAX_CUSTOMDATA_LAYER_NAME];
  float aspx = umd->aspectx ? umd->aspectx : 1.0f;
  float aspy = umd->aspecty ? umd->aspecty : 1.0f;
  float scax = umd->scalex ? umd->scalex : 1.0f;
  float scay = umd->scaley ? umd->scaley : 1.0f;
  int free_uci = 0;

  for (i = 0; i < umd->projectors_num; i++) {
    if (umd->projectors[i] != nullptr) {
      projectors[projectors_num++].ob = umd->projectors[i];
    }
  }

  if (projectors_num == 0) {
    return mesh;
  }

  /* Create a new layer if no UV Maps are available
   * (e.g. if a preceding modifier could not preserve it). */
  if (!CustomData_has_layer(&mesh->ldata, CD_PROP_FLOAT2)) {
    CustomData_add_layer_named(
        &mesh->ldata, CD_PROP_FLOAT2, CD_SET_DEFAULT, mesh->totloop, umd->uvlayer_name);
  }

  /* make sure we're using an existing layer */
  CustomData_validate_layer_name(&mesh->ldata, CD_PROP_FLOAT2, umd->uvlayer_name, uvname);

  /* calculate a projection matrix and normal for each projector */
  for (i = 0; i < projectors_num; i++) {
    float tmpmat[4][4];
    float offsetmat[4][4];
    Camera *cam = nullptr;
    /* calculate projection matrix */
    invert_m4_m4(projectors[i].projmat, projectors[i].ob->object_to_world);

    projectors[i].uci = nullptr;

    if (projectors[i].ob->type == OB_CAMERA) {
      cam = (Camera *)projectors[i].ob->data;
      if (cam->type == CAM_PANO) {
        projectors[i].uci = BLI_uvproject_camera_info(projectors[i].ob, nullptr, aspx, aspy);
        BLI_uvproject_camera_info_scale(
            static_cast<ProjCameraInfo *>(projectors[i].uci), scax, scay);
        free_uci = 1;
      }
      else {
        CameraParams params;

        /* setup parameters */
        BKE_camera_params_init(&params);
        BKE_camera_params_from_object(&params, projectors[i].ob);

        /* Compute matrix, view-plane, etc. */
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

    /* Calculate world-space projector normal (for best projector test). */
    projectors[i].normal[0] = 0;
    projectors[i].normal[1] = 0;
    projectors[i].normal[2] = 1;
    mul_mat3_m4_v3(projectors[i].ob->object_to_world, projectors[i].normal);
  }

  const blender::Span<blender::float3> positions = mesh->vert_positions();
  const blender::OffsetIndices polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();

  float(*mloop_uv)[2] = static_cast<float(*)[2]>(CustomData_get_layer_named_for_write(
      &mesh->ldata, CD_PROP_FLOAT2, uvname, corner_verts.size()));

  coords = BKE_mesh_vert_coords_alloc(mesh, &verts_num);

  /* Convert coords to world-space. */
  for (i = 0, co = coords; i < verts_num; i++, co++) {
    mul_m4_v3(ob->object_to_world, *co);
  }

  /* if only one projector, project coords to UVs */
  if (projectors_num == 1 && projectors[0].uci == nullptr) {
    for (i = 0, co = coords; i < verts_num; i++, co++) {
      mul_project_m4_v3(projectors[0].projmat, *co);
    }
  }

  /* apply coords as UVs */
  for (const int i : polys.index_range()) {
    const blender::IndexRange poly = polys[i];
    if (projectors_num == 1) {
      if (projectors[0].uci) {
        uint fidx = poly.size() - 1;
        do {
          uint lidx = poly.start() + fidx;
          const int vidx = corner_verts[lidx];
          BLI_uvproject_from_camera(
              mloop_uv[lidx], coords[vidx], static_cast<ProjCameraInfo *>(projectors[0].uci));
        } while (fidx--);
      }
      else {
        /* apply transformed coords as UVs */
        uint fidx = poly.size() - 1;
        do {
          uint lidx = poly.start() + fidx;
          const int vidx = corner_verts[lidx];
          copy_v2_v2(mloop_uv[lidx], coords[vidx]);
        } while (fidx--);
      }
    }
    else {
      /* multiple projectors, select the closest to face normal direction */
      int j;
      Projector *best_projector;
      float best_dot;

      /* get the untransformed face normal */
      const blender::float3 face_no = blender::bke::mesh::poly_normal_calc(
          positions, corner_verts.slice(poly));

      /* find the projector which the face points at most directly
       * (projector normal with largest dot product is best)
       */
      best_dot = dot_v3v3(projectors[0].normal, face_no);
      best_projector = &projectors[0];

      for (j = 1; j < projectors_num; j++) {
        float tmp_dot = dot_v3v3(projectors[j].normal, face_no);
        if (tmp_dot > best_dot) {
          best_dot = tmp_dot;
          best_projector = &projectors[j];
        }
      }

      if (best_projector->uci) {
        uint fidx = poly.size() - 1;
        do {
          uint lidx = poly.start() + fidx;
          const int vidx = corner_verts[lidx];
          BLI_uvproject_from_camera(
              mloop_uv[lidx], coords[vidx], static_cast<ProjCameraInfo *>(best_projector->uci));
        } while (fidx--);
      }
      else {
        uint fidx = poly.size() - 1;
        do {
          uint lidx = poly.start() + fidx;
          const int vidx = corner_verts[lidx];
          mul_v2_project_m4_v3(mloop_uv[lidx], best_projector->projmat, coords[vidx]);
        } while (fidx--);
      }
    }
  }

  MEM_freeN(coords);

  if (free_uci) {
    int j;
    for (j = 0; j < projectors_num; j++) {
      if (projectors[j].uci) {
        MEM_freeN(projectors[j].uci);
      }
    }
  }

  mesh->runtime->is_original_bmesh = false;

  return mesh;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  UVProjectModifierData *umd = (UVProjectModifierData *)md;

  result = uvprojectModifier_do(umd, ctx, ctx->object, mesh);

  return result;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");

  uiLayoutSetPropSep(layout, true);

  uiItemPointerR(layout, ptr, "uv_layer", &obj_data_ptr, "uv_layers", nullptr, ICON_NONE);

  /* Aspect and Scale are only used for camera projectors. */
  bool has_camera = false;
  RNA_BEGIN (ptr, projector_ptr, "projectors") {
    PointerRNA ob_projector = RNA_pointer_get(&projector_ptr, "object");
    if (!RNA_pointer_is_null(&ob_projector) && RNA_enum_get(&ob_projector, "type") == OB_CAMERA) {
      has_camera = true;
      break;
    }
  }
  RNA_END;

  sub = uiLayoutColumn(layout, true);
  uiLayoutSetActive(sub, has_camera);
  uiItemR(sub, ptr, "aspect_x", 0, nullptr, ICON_NONE);
  uiItemR(sub, ptr, "aspect_y", 0, IFACE_("Y"), ICON_NONE);

  sub = uiLayoutColumn(layout, true);
  uiLayoutSetActive(sub, has_camera);
  uiItemR(sub, ptr, "scale_x", 0, nullptr, ICON_NONE);
  uiItemR(sub, ptr, "scale_y", 0, IFACE_("Y"), ICON_NONE);

  uiItemR(layout, ptr, "projector_count", 0, IFACE_("Projectors"), ICON_NONE);
  RNA_BEGIN (ptr, projector_ptr, "projectors") {
    uiItemR(layout, &projector_ptr, "object", 0, nullptr, ICON_NONE);
  }
  RNA_END;

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_UVProject, panel_draw);
}

ModifierTypeInfo modifierType_UVProject = {
    /*name*/ N_("UVProject"),
    /*structName*/ "UVProjectModifierData",
    /*structSize*/ sizeof(UVProjectModifierData),
    /*srna*/ &RNA_UVProjectModifier,
    /*type*/ eModifierTypeType_NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_UVPROJECT,

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ modifyMesh,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ nullptr,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
