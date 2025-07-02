/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DNA_curve_types.h"
#include "DNA_curves_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BKE_subdiv_modifier.hh"

#include "DRW_render.hh"
#include "GPU_batch.hh"
#include "GPU_batch_utils.hh"
#include "GPU_capabilities.hh"

#include "draw_cache.hh"
#include "draw_cache_impl.hh"
#include "draw_context_private.hh"

/* -------------------------------------------------------------------- */
/** \name Internal Defines
 * \{ */

#define VCLASS_LIGHT_AREA_SHAPE (1 << 0)
#define VCLASS_LIGHT_SPOT_SHAPE (1 << 1)
#define VCLASS_LIGHT_SPOT_BLEND (1 << 2)
#define VCLASS_LIGHT_SPOT_CONE (1 << 3)
#define VCLASS_LIGHT_DIST (1 << 4)

#define VCLASS_CAMERA_FRAME (1 << 5)
#define VCLASS_CAMERA_DIST (1 << 6)
#define VCLASS_CAMERA_VOLUME (1 << 7)

#define VCLASS_SCREENSPACE (1 << 8)
#define VCLASS_SCREENALIGNED (1 << 9)

#define VCLASS_EMPTY_SCALED (1 << 10)
#define VCLASS_EMPTY_AXES (1 << 11)
#define VCLASS_EMPTY_AXES_NAME (1 << 12)
#define VCLASS_EMPTY_AXES_SHADOW (1 << 13)
#define VCLASS_EMPTY_SIZE (1 << 14)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

namespace blender::draw {

void DRW_vertbuf_create_wiredata(gpu::VertBuf *vbo, const int vert_len)
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute("wd",
                                                                    gpu::VertAttrType::SFLOAT_32);
  GPU_vertbuf_init_with_format(*vbo, format);
  GPU_vertbuf_data_alloc(*vbo, vert_len);
  vbo->data<float>().fill(1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Object API
 *
 * \note Curve and text objects evaluate to the evaluated geometry set's mesh component if
 * they have a surface, so curve objects themselves do not have a surface (the mesh component
 * is presented to render engines as a separate object).
 * \{ */

gpu::Batch *DRW_cache_object_all_edges_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_all_edges_get(ob);
    /* TODO: should match #DRW_cache_object_surface_get. */
    default:
      return nullptr;
  }
}

gpu::Batch *DRW_cache_object_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_edge_detection_get(ob, r_is_manifold);
    default:
      return nullptr;
  }
}

gpu::Batch *DRW_cache_object_face_wireframe_get(const Scene *scene, Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_face_wireframe_get(ob);
    case OB_POINTCLOUD:
      return DRW_pointcloud_batch_cache_get_dots(ob);
    case OB_VOLUME:
      return DRW_cache_volume_face_wireframe_get(ob);
    case OB_GREASE_PENCIL:
      return DRW_cache_grease_pencil_face_wireframe_get(scene, ob);
    default:
      return nullptr;
  }
}

gpu::Batch *DRW_cache_object_loose_edges_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_loose_edges_get(ob);
    default:
      return nullptr;
  }
}

gpu::Batch *DRW_cache_object_surface_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_surface_get(ob);
    default:
      return nullptr;
  }
}

Span<gpu::Batch *> DRW_cache_object_surface_material_get(Object *ob,
                                                         const Span<const GPUMaterial *> materials)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_surface_shaded_get(ob, materials);
    default:
      return {};
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Meshes
 * \{ */

gpu::Batch *DRW_cache_mesh_all_verts_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_all_verts(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_paint_overlay_verts_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_paint_overlay_verts(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_all_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_all_edges(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_loose_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_loose_edges(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_edge_detection(DRW_object_get_data_for_drawing<Mesh>(*ob),
                                                 r_is_manifold);
}

gpu::Batch *DRW_cache_mesh_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_paint_overlay_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_paint_overlay_surface(
      DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_paint_overlay_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_paint_overlay_edges(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

Span<gpu::Batch *> DRW_cache_mesh_surface_shaded_get(Object *ob,
                                                     const Span<const GPUMaterial *> materials)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_shaded(
      *ob, DRW_object_get_data_for_drawing<Mesh>(*ob), materials);
}

Span<gpu::Batch *> DRW_cache_mesh_surface_texpaint_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_texpaint(*ob,
                                                   DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_surface_texpaint_single_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_texpaint_single(
      *ob, DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_surface_vertpaint_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_vertpaint(*ob,
                                                    DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_surface_sculptcolors_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_sculpt(*ob, DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_surface_weights_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_weights(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_wireframes_face(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_surface_mesh_analysis_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_edit_mesh_analysis(DRW_object_get_data_for_drawing<Mesh>(*ob));
}

gpu::Batch *DRW_cache_mesh_surface_viewer_attribute_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_viewer_attribute(
      DRW_object_get_data_for_drawing<Mesh>(*ob));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve
 * \{ */

gpu::Batch *DRW_cache_curve_edge_wire_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVES_LEGACY);
  Curve &cu = DRW_object_get_data_for_drawing<Curve>(*ob);
  return DRW_curve_batch_cache_get_wire_edge(&cu);
}

gpu::Batch *DRW_cache_curve_edge_wire_viewer_attribute_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVES_LEGACY);
  Curve &cu = DRW_object_get_data_for_drawing<Curve>(*ob);
  return DRW_curve_batch_cache_get_wire_edge_viewer_attribute(&cu);
}

gpu::Batch *DRW_cache_curve_edge_normal_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVES_LEGACY);
  Curve &cu = DRW_object_get_data_for_drawing<Curve>(*ob);
  return DRW_curve_batch_cache_get_normal_edge(&cu);
}

gpu::Batch *DRW_cache_curve_edge_overlay_get(Object *ob)
{
  BLI_assert(ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF));

  Curve &cu = DRW_object_get_data_for_drawing<Curve>(*ob);
  return DRW_curve_batch_cache_get_edit_edges(&cu);
}

gpu::Batch *DRW_cache_curve_vert_overlay_get(Object *ob)
{
  BLI_assert(ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF));

  Curve &cu = DRW_object_get_data_for_drawing<Curve>(*ob);
  return DRW_curve_batch_cache_get_edit_verts(&cu);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Font
 * \{ */

gpu::Batch *DRW_cache_text_edge_wire_get(Object *ob)
{
  BLI_assert(ob->type == OB_FONT);
  Curve &cu = DRW_object_get_data_for_drawing<Curve>(*ob);
  return DRW_curve_batch_cache_get_wire_edge(&cu);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

gpu::Batch *DRW_cache_surf_edge_wire_get(Object *ob)
{
  BLI_assert(ob->type == OB_SURF);
  Curve &cu = DRW_object_get_data_for_drawing<Curve>(*ob);
  return DRW_curve_batch_cache_get_wire_edge(&cu);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice
 * \{ */

gpu::Batch *DRW_cache_lattice_verts_get(Object *ob)
{
  BLI_assert(ob->type == OB_LATTICE);

  Lattice &lt = DRW_object_get_data_for_drawing<Lattice>(*ob);
  return DRW_lattice_batch_cache_get_all_verts(&lt);
}

gpu::Batch *DRW_cache_lattice_wire_get(Object *ob, bool use_weight)
{
  BLI_assert(ob->type == OB_LATTICE);

  Lattice &lt = DRW_object_get_data_for_drawing<Lattice>(*ob);
  int actdef = -1;

  if (use_weight && !BLI_listbase_is_empty(&lt.vertex_group_names) && lt.editlatt &&
      lt.editlatt->latt->dvert)
  {
    actdef = lt.vertex_group_active_index - 1;
  }

  return DRW_lattice_batch_cache_get_all_edges(&lt, use_weight, actdef);
}

gpu::Batch *DRW_cache_lattice_vert_overlay_get(Object *ob)
{
  BLI_assert(ob->type == OB_LATTICE);

  Lattice &lt = DRW_object_get_data_for_drawing<Lattice>(*ob);
  return DRW_lattice_batch_cache_get_edit_verts(&lt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name PointCloud
 * \{ */

gpu::Batch *DRW_cache_pointcloud_vert_overlay_get(Object *ob)
{
  BLI_assert(ob->type == OB_POINTCLOUD);

  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*ob);
  return DRW_pointcloud_batch_cache_get_edit_dots(&pointcloud);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

gpu::Batch *DRW_cache_volume_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_VOLUME);
  return DRW_volume_batch_cache_get_wireframes_face(&DRW_object_get_data_for_drawing<Volume>(*ob));
}

gpu::Batch *DRW_cache_volume_selection_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_VOLUME);
  return DRW_volume_batch_cache_get_selection_surface(
      &DRW_object_get_data_for_drawing<Volume>(*ob));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particles
 * \{ */

gpu::Batch *DRW_cache_particles_get_hair(Object *object, ParticleSystem *psys, ModifierData *md)
{
  return DRW_particles_batch_cache_get_hair(object, psys, md);
}

gpu::Batch *DRW_cache_particles_get_dots(Object *object, ParticleSystem *psys)
{
  return DRW_particles_batch_cache_get_dots(object, psys);
}

gpu::Batch *DRW_cache_particles_get_edit_strands(Object *object,
                                                 ParticleSystem *psys,
                                                 PTCacheEdit *edit,
                                                 bool use_weight)
{
  return DRW_particles_batch_cache_get_edit_strands(object, psys, edit, use_weight);
}

gpu::Batch *DRW_cache_particles_get_edit_inner_points(Object *object,
                                                      ParticleSystem *psys,
                                                      PTCacheEdit *edit)
{
  return DRW_particles_batch_cache_get_edit_inner_points(object, psys, edit);
}

gpu::Batch *DRW_cache_particles_get_edit_tip_points(Object *object,
                                                    ParticleSystem *psys,
                                                    PTCacheEdit *edit)
{
  return DRW_particles_batch_cache_get_edit_tip_points(object, psys, edit);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Batch Cache Implementation (common)
 * \{ */

void drw_batch_cache_validate(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      DRW_mesh_batch_cache_validate(DRW_object_get_data_for_drawing<Mesh>(*ob));
      break;
    case OB_CURVES_LEGACY:
    case OB_FONT:
    case OB_SURF:
      DRW_curve_batch_cache_validate(&DRW_object_get_data_for_drawing<Curve>(*ob));
      break;
    case OB_LATTICE:
      DRW_lattice_batch_cache_validate(&DRW_object_get_data_for_drawing<Lattice>(*ob));
      break;
    case OB_CURVES:
      DRW_curves_batch_cache_validate(&DRW_object_get_data_for_drawing<Curves>(*ob));
      break;
    case OB_POINTCLOUD:
      DRW_pointcloud_batch_cache_validate(&DRW_object_get_data_for_drawing<PointCloud>(*ob));
      break;
    case OB_VOLUME:
      DRW_volume_batch_cache_validate(&DRW_object_get_data_for_drawing<Volume>(*ob));
      break;
    case OB_GREASE_PENCIL:
      DRW_grease_pencil_batch_cache_validate(&DRW_object_get_data_for_drawing<GreasePencil>(*ob));
    default:
      break;
  }
}

void drw_batch_cache_generate_requested(Object *ob, TaskGraph &task_graph)
{
  const DRWContext *draw_ctx = DRW_context_get();
  const Scene *scene = draw_ctx->scene;
  const enum eContextObjectMode mode = CTX_data_mode_enum_ex(
      draw_ctx->object_edit, draw_ctx->obact, draw_ctx->object_mode);
  const bool is_paint_mode = ELEM(
      mode, CTX_MODE_SCULPT, CTX_MODE_PAINT_TEXTURE, CTX_MODE_PAINT_VERTEX, CTX_MODE_PAINT_WEIGHT);

  const bool use_hide = ((ob->type == OB_MESH) &&
                         ((is_paint_mode && (ob == draw_ctx->obact) &&
                           DRW_object_use_hide_faces(ob)) ||
                          ((mode == CTX_MODE_EDIT_MESH) && (ob->mode == OB_MODE_EDIT))));

  switch (ob->type) {
    case OB_MESH:
      DRW_mesh_batch_cache_create_requested(task_graph,
                                            *ob,
                                            DRW_object_get_data_for_drawing<Mesh>(*ob),
                                            *scene,
                                            is_paint_mode,
                                            use_hide);
      break;
    case OB_CURVES_LEGACY:
    case OB_FONT:
    case OB_SURF:
      DRW_curve_batch_cache_create_requested(ob, scene);
      break;
    case OB_CURVES:
      DRW_curves_batch_cache_create_requested(ob);
      break;
    case OB_POINTCLOUD:
      DRW_pointcloud_batch_cache_create_requested(ob);
      break;
    /* TODO: all cases. */
    default:
      break;
  }
}

void drw_batch_cache_generate_requested_evaluated_mesh_or_curve(Object *ob, TaskGraph &task_graph)
{
  /* NOTE: Logic here is duplicated from #drw_batch_cache_generate_requested. */

  const DRWContext *draw_ctx = DRW_context_get();
  const Scene *scene = draw_ctx->scene;
  const enum eContextObjectMode mode = CTX_data_mode_enum_ex(
      draw_ctx->object_edit, draw_ctx->obact, draw_ctx->object_mode);
  const bool is_paint_mode = ELEM(
      mode, CTX_MODE_SCULPT, CTX_MODE_PAINT_TEXTURE, CTX_MODE_PAINT_VERTEX, CTX_MODE_PAINT_WEIGHT);

  const bool use_hide = ((ob->type == OB_MESH) &&
                         ((is_paint_mode && (ob == draw_ctx->obact) &&
                           DRW_object_use_hide_faces(ob)) ||
                          ((mode == CTX_MODE_EDIT_MESH) && (ob->mode == OB_MODE_EDIT))));

  Mesh *mesh = BKE_object_get_evaluated_mesh_no_subsurf_unchecked(ob);
  /* Try getting the mesh first and if that fails, try getting the curve data.
   * If the curves are surfaces or have certain modifiers applied to them, the will have mesh data
   * of the final result.
   */
  if (mesh != nullptr) {
    DRW_mesh_batch_cache_create_requested(task_graph, *ob, *mesh, *scene, is_paint_mode, use_hide);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    DRW_curve_batch_cache_create_requested(ob, scene);
  }
}

void drw_batch_cache_generate_requested_delayed(Object *ob)
{
  DRWContext &draw_ctx = drw_get();
  if (draw_ctx.delayed_extraction == nullptr) {
    draw_ctx.delayed_extraction = BLI_gset_ptr_new(__func__);
  }
  BLI_gset_add(draw_ctx.delayed_extraction, ob);
}

void DRW_batch_cache_free_old(Object *ob, int ctime)
{
  switch (ob->type) {
    case OB_MESH:
      DRW_mesh_batch_cache_free_old(&DRW_object_get_data_for_drawing<Mesh>(*ob), ctime);
      break;
    case OB_CURVES:
      DRW_curves_batch_cache_free_old(&DRW_object_get_data_for_drawing<Curves>(*ob), ctime);
      break;
    case OB_POINTCLOUD:
      DRW_pointcloud_batch_cache_free_old(&DRW_object_get_data_for_drawing<PointCloud>(*ob),
                                          ctime);
      break;
    default:
      break;
  }
}

/** \} */

void DRW_cdlayer_attr_aliases_add(GPUVertFormat *format,
                                  const char *base_name,
                                  const bke::AttrType data_type,
                                  const StringRef layer_name,
                                  bool is_active_render,
                                  bool is_active_layer)
{
  char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
  GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

  /* Attribute layer name. */
  SNPRINTF(attr_name, "%s%s", base_name, attr_safe_name);
  GPU_vertformat_alias_add(format, attr_name);

  /* Auto layer name. */
  SNPRINTF(attr_name, "a%s", attr_safe_name);
  GPU_vertformat_alias_add(format, attr_name);

  /* Active render layer name. */
  if (is_active_render) {
    GPU_vertformat_alias_add(format, data_type == bke::AttrType::Float2 ? "a" : base_name);
  }

  /* Active display layer name. */
  if (is_active_layer) {
    SNPRINTF(attr_name, "a%s", base_name);
    GPU_vertformat_alias_add(format, attr_name);
  }
}

}  // namespace blender::draw
