/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "draw_sculpt.hh"

#include "draw_attributes.hh"
#include "draw_pbvh.h"

#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "DRW_pbvh.hh"

namespace blender::draw {

float3 SculptBatch::debug_color()
{
  static float3 colors[9] = {
      {1.0f, 0.2f, 0.2f},
      {0.2f, 1.0f, 0.2f},
      {0.2f, 0.2f, 1.0f},
      {1.0f, 1.0f, 0.2f},
      {0.2f, 1.0f, 1.0f},
      {1.0f, 0.2f, 1.0f},
      {1.0f, 0.7f, 0.2f},
      {0.2f, 1.0f, 0.7f},
      {0.7f, 0.2f, 1.0f},
  };

  return colors[debug_index % 9];
}

struct SculptCallbackData {
  bool use_wire;
  bool fast_mode;

  PBVHAttrReq *attrs;
  int attrs_len;

  Vector<SculptBatch> batches;
};

static void sculpt_draw_cb(SculptCallbackData *data,
                           PBVHBatches *batches,
                           PBVH_GPU_Args *pbvh_draw_args)
{
  if (!batches) {
    return;
  }

  SculptBatch batch = {};

  int primcount;
  if (data->use_wire) {
    batch.batch = DRW_pbvh_lines_get(
        batches, data->attrs, data->attrs_len, pbvh_draw_args, &primcount, data->fast_mode);
  }
  else {
    batch.batch = DRW_pbvh_tris_get(
        batches, data->attrs, data->attrs_len, pbvh_draw_args, &primcount, data->fast_mode);
  }

  batch.material_slot = drw_pbvh_material_index_get(batches);
  batch.debug_index = data->batches.size();

  data->batches.append(batch);
}

static Vector<SculptBatch> sculpt_batches_get_ex(
    Object *ob, bool use_wire, bool use_materials, PBVHAttrReq *attrs, int attrs_len)
{
  /* PBVH should always exist for non-empty meshes, created by depsgraph eval. */
  PBVH *pbvh = ob->sculpt ? ob->sculpt->pbvh : nullptr;
  if (!pbvh) {
    return {};
  }

  /* TODO(Miguel Pozo): Don't use global context. */
  const DRWContextState *drwctx = DRW_context_state_get();
  RegionView3D *rv3d = drwctx->rv3d;
  const bool navigating = rv3d && (rv3d->rflag & RV3D_NAVIGATING);

  Paint *paint = nullptr;
  if (drwctx->evil_C != nullptr) {
    paint = BKE_paint_get_active_from_context(drwctx->evil_C);
  }

  /* Frustum planes to show only visible PBVH nodes. */
  float4 draw_planes[6];
  PBVHFrustumPlanes draw_frustum = {reinterpret_cast<float(*)[4]>(draw_planes), 6};
  float4 update_planes[6];
  PBVHFrustumPlanes update_frustum = {reinterpret_cast<float(*)[4]>(update_planes), 6};

  /* TODO: take into account partial redraw for clipping planes. */
  DRW_view_frustum_planes_get(DRW_view_default_get(), draw_frustum.planes);
  /* Transform clipping planes to object space. Transforming a plane with a
   * 4x4 matrix is done by multiplying with the transpose inverse.
   * The inverse cancels out here since we transform by inverse(obmat). */
  float4x4 tmat = math::transpose(float4x4(ob->object_to_world));
  for (int i : IndexRange(6)) {
    draw_planes[i] = tmat * draw_planes[i];
    update_planes[i] = draw_planes[i];
  }

  if (paint && (paint->flags & PAINT_SCULPT_DELAY_UPDATES)) {
    if (navigating) {
      BKE_pbvh_get_frustum_planes(pbvh, &update_frustum);
    }
    else {
      BKE_pbvh_set_frustum_planes(pbvh, &update_frustum);
    }
  }

  /* Fast mode to show low poly multires while navigating. */
  bool fast_mode = false;
  if (paint && (paint->flags & PAINT_FAST_NAVIGATE)) {
    fast_mode = navigating;
  }

  /* Update draw buffers only for visible nodes while painting.
   * But do update them otherwise so navigating stays smooth. */
  bool update_only_visible = rv3d && !(rv3d->rflag & RV3D_PAINTING);
  if (paint && (paint->flags & PAINT_SCULPT_DELAY_UPDATES)) {
    update_only_visible = true;
  }

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  BKE_pbvh_update_normals(pbvh, mesh->runtime->subdiv_ccg);

  SculptCallbackData data;
  data.use_wire = use_wire;
  data.fast_mode = fast_mode;
  data.attrs = attrs;
  data.attrs_len = attrs_len;

  BKE_pbvh_draw_cb(pbvh,
                   update_only_visible,
                   &update_frustum,
                   &draw_frustum,
                   (void (*)(void *, PBVHBatches *, PBVH_GPU_Args *))sculpt_draw_cb,
                   &data,
                   use_materials,
                   attrs,
                   attrs_len);

  return data.batches;
}

Vector<SculptBatch> sculpt_batches_get(Object *ob, SculptBatchFeature features)
{
  PBVHAttrReq attrs[16] = {0};
  int attrs_len = 0;

  /* NOTE: these are NOT #eCustomDataType, they are extended values, ASAN may warn about this. */
  attrs[attrs_len++].type = eCustomDataType(CD_PBVH_CO_TYPE);
  attrs[attrs_len++].type = eCustomDataType(CD_PBVH_NO_TYPE);

  if (features & SCULPT_BATCH_MASK) {
    attrs[attrs_len++].type = eCustomDataType(CD_PBVH_MASK_TYPE);
  }

  if (features & SCULPT_BATCH_FACE_SET) {
    attrs[attrs_len++].type = eCustomDataType(CD_PBVH_FSET_TYPE);
  }

  Mesh *mesh = BKE_object_get_original_mesh(ob);

  if (features & SCULPT_BATCH_VERTEX_COLOR) {
    const CustomDataLayer *layer = BKE_id_attributes_color_find(&mesh->id,
                                                                mesh->active_color_attribute);
    if (layer) {
      attrs[attrs_len].type = eCustomDataType(layer->type);
      attrs[attrs_len].domain = BKE_id_attribute_domain(&mesh->id, layer);
      STRNCPY(attrs[attrs_len].name, layer->name);
      attrs_len++;
    }
  }

  if (features & SCULPT_BATCH_UV) {
    int layer_i = CustomData_get_active_layer_index(&mesh->ldata, CD_PROP_FLOAT2);
    if (layer_i != -1) {
      CustomDataLayer *layer = mesh->ldata.layers + layer_i;
      attrs[attrs_len].type = CD_PROP_FLOAT2;
      attrs[attrs_len].domain = ATTR_DOMAIN_CORNER;
      STRNCPY(attrs[attrs_len].name, layer->name);
      attrs_len++;
    }
  }

  return sculpt_batches_get_ex(ob, features & SCULPT_BATCH_WIREFRAME, false, attrs, attrs_len);
}

Vector<SculptBatch> sculpt_batches_per_material_get(Object *ob,
                                                    MutableSpan<GPUMaterial *> materials)
{
  BLI_assert(ob->type == OB_MESH);
  Mesh *mesh = (Mesh *)ob->data;

  DRW_Attributes draw_attrs;
  DRW_MeshCDMask cd_needed;

  DRW_mesh_get_attributes(ob, mesh, materials.data(), materials.size(), &draw_attrs, &cd_needed);

  PBVHAttrReq attrs[16] = {0};
  int attrs_len = 0;

  /* NOTE: these are NOT #eCustomDataType, they are extended values, ASAN may warn about this. */
  attrs[attrs_len++].type = eCustomDataType(CD_PBVH_CO_TYPE);
  attrs[attrs_len++].type = eCustomDataType(CD_PBVH_NO_TYPE);

  for (int i = 0; i < draw_attrs.num_requests; i++) {
    DRW_AttributeRequest *req = draw_attrs.requests + i;
    attrs[attrs_len].type = req->cd_type;
    attrs[attrs_len].domain = req->domain;
    STRNCPY(attrs[attrs_len].name, req->attribute_name);
    attrs_len++;
  }

  /* UV maps are not in attribute requests. */
  for (uint i = 0; i < 32; i++) {
    if (cd_needed.uv & (1 << i)) {
      int layer_i = CustomData_get_layer_index_n(&mesh->ldata, CD_PROP_FLOAT2, i);
      CustomDataLayer *layer = layer_i != -1 ? mesh->ldata.layers + layer_i : nullptr;
      if (layer) {
        attrs[attrs_len].type = CD_PROP_FLOAT2;
        attrs[attrs_len].domain = ATTR_DOMAIN_CORNER;
        STRNCPY(attrs[attrs_len].name, layer->name);
        attrs_len++;
      }
    }
  }

  return sculpt_batches_get_ex(ob, false, true, attrs, attrs_len);
}

}  // namespace blender::draw
