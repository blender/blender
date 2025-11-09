/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

/* UV Project modifier: Generates UVs projected from an object */

#include "BLI_utildefines.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_camera_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_camera.h"
#include "BKE_customdata.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_uvproject.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph_build.hh"

static void init_data(ModifierData *md)
{
  UVProjectModifierData *umd = (UVProjectModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(umd, modifier));

  MEMCPY_STRUCT_AFTER(umd, DNA_struct_default_get(UVProjectModifierData), modifier);
}

static void required_data_mask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  /* ask for UV coordinates */
  r_cddata_masks->lmask |= CD_MASK_PROP_FLOAT2;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  UVProjectModifierData *umd = (UVProjectModifierData *)md;
  for (int i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; i++) {
    walk(user_data, ob, (ID **)&umd->projectors[i], IDWALK_CB_NOP);
  }
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
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

static blender::bke::SpanAttributeWriter<blender::float2> get_uv_attribute(
    Mesh &mesh, const blender::StringRef md_name)
{
  using namespace blender;
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (md_name.is_empty()) {
    const StringRef name = mesh.active_uv_map_name();
    return attributes.lookup_or_add_for_write_span<float2>(name.is_empty() ? "Float2" : name,
                                                           bke::AttrDomain::Corner);
  }
  if (bke::SpanAttributeWriter<float2> attribute = attributes.lookup_or_add_for_write_span<float2>(
          md_name, bke::AttrDomain::Corner))
  {
    return attribute;
  }
  AttributeOwner owner = AttributeOwner::from_id(&mesh.id);
  const std::string name = BKE_attribute_calc_unique_name(owner, md_name);
  return attributes.lookup_or_add_for_write_span<float2>(name, bke::AttrDomain::Corner);
}

static Mesh *uvprojectModifier_do(UVProjectModifierData *umd,
                                  const ModifierEvalContext * /*ctx*/,
                                  Object *ob,
                                  Mesh *mesh)
{
  using namespace blender;
  Projector projectors[MOD_UVPROJECT_MAXPROJECTORS];
  int projectors_num = 0;
  float aspx = umd->aspectx ? umd->aspectx : 1.0f;
  float aspy = umd->aspecty ? umd->aspecty : 1.0f;
  float scax = umd->scalex ? umd->scalex : 1.0f;
  float scay = umd->scaley ? umd->scaley : 1.0f;
  bool free_uci = false;

  for (int i = 0; i < umd->projectors_num; i++) {
    if (umd->projectors[i] != nullptr) {
      projectors[projectors_num++].ob = umd->projectors[i];
    }
  }

  if (projectors_num == 0) {
    return mesh;
  }

  bke::SpanAttributeWriter uv_attribute = get_uv_attribute(*mesh, umd->uvlayer_name);
  if (!uv_attribute) {
    return mesh;
  }

  /* calculate a projection matrix and normal for each projector */
  for (int i = 0; i < projectors_num; i++) {
    float tmpmat[4][4];
    float offsetmat[4][4];
    /* calculate projection matrix */
    invert_m4_m4(projectors[i].projmat, projectors[i].ob->object_to_world().ptr());

    projectors[i].uci = nullptr;

    if (projectors[i].ob->type == OB_CAMERA) {
      const Camera *cam = (const Camera *)projectors[i].ob->data;
      if (cam->type == CAM_PANO) {
        projectors[i].uci = BKE_uvproject_camera_info(projectors[i].ob, nullptr, aspx, aspy);
        BKE_uvproject_camera_info_scale(
            static_cast<ProjCameraInfo *>(projectors[i].uci), scax, scay);
        free_uci = true;
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
    mul_mat3_m4_v3(projectors[i].ob->object_to_world().ptr(), projectors[i].normal);
  }

  const Span<float3> positions = mesh->vert_positions();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  MutableSpan<float2> mloop_uv = uv_attribute.span;

  /* Convert coords to world-space. */
  Array<float3> coords(positions.size());
  for (int64_t i = 0; i < positions.size(); i++) {
    mul_v3_m4v3(coords[i], ob->object_to_world().ptr(), positions[i]);
  }

  /* if only one projector, project coords to UVs */
  if (projectors_num == 1 && projectors[0].uci == nullptr) {
    for (int64_t i = 0; i < coords.size(); i++) {
      mul_project_m4_v3(projectors[0].projmat, coords[i]);
    }
  }

  /* apply coords as UVs */
  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];
    if (projectors_num == 1) {
      if (projectors[0].uci) {
        for (const int corner : face) {
          const int vert = corner_verts[corner];
          BKE_uvproject_from_camera(
              mloop_uv[corner], coords[vert], static_cast<ProjCameraInfo *>(projectors[0].uci));
        }
      }
      else {
        /* apply transformed coords as UVs */
        for (const int corner : face) {
          const int vert = corner_verts[corner];
          copy_v2_v2(mloop_uv[corner], coords[vert]);
        }
      }
    }
    else {
      /* multiple projectors, select the closest to face normal direction */
      int j;
      Projector *best_projector;
      float best_dot;

      /* get the untransformed face normal */
      const float3 face_no = blender::bke::mesh::face_normal_calc(positions,
                                                                  corner_verts.slice(face));

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
        for (const int corner : face) {
          const int vert = corner_verts[corner];
          BKE_uvproject_from_camera(
              mloop_uv[corner], coords[vert], static_cast<ProjCameraInfo *>(best_projector->uci));
        }
      }
      else {
        for (const int corner : face) {
          const int vert = corner_verts[corner];
          mul_v2_project_m4_v3(mloop_uv[corner], best_projector->projmat, coords[vert]);
        }
      }
    }
  }

  if (free_uci) {
    int j;
    for (j = 0; j < projectors_num; j++) {
      if (projectors[j].uci) {
        MEM_freeN(projectors[j].uci);
      }
    }
  }

  uv_attribute.finish();

  mesh->runtime->is_original_bmesh = false;

  return mesh;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
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

  layout->use_property_split_set(true);

  layout->prop_search(ptr, "uv_layer", &obj_data_ptr, "uv_layers", std::nullopt, ICON_GROUP_UVS);

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

  sub = &layout->column(true);
  sub->active_set(has_camera);
  sub->prop(ptr, "aspect_x", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  sub->prop(ptr, "aspect_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

  sub = &layout->column(true);
  sub->active_set(has_camera);
  sub->prop(ptr, "scale_x", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  sub->prop(ptr, "scale_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

  layout->prop(ptr, "projector_count", UI_ITEM_NONE, IFACE_("Projectors"), ICON_NONE);
  RNA_BEGIN (ptr, projector_ptr, "projectors") {
    layout->prop(&projector_ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  RNA_END;

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_UVProject, panel_draw);
}

ModifierTypeInfo modifierType_UVProject = {
    /*idname*/ "UVProject",
    /*name*/ N_("UVProject"),
    /*struct_name*/ "UVProjectModifierData",
    /*struct_size*/ sizeof(UVProjectModifierData),
    /*srna*/ &RNA_UVProjectModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_UVPROJECT,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
