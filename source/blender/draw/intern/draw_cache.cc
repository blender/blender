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

#include "UI_resources.hh"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_object.hh"

#include "GPU_batch.hh"
#include "GPU_batch_utils.hh"
#include "GPU_capabilities.hh"

#include "draw_cache.hh"
#include "draw_cache_impl.hh"
#include "draw_manager_c.hh"

using blender::Span;

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

void DRW_vertbuf_create_wiredata(blender::gpu::VertBuf *vbo, const int vert_len)
{
  static GPUVertFormat format = {0};
  static struct {
    uint wd;
  } attr_id;
  if (format.attr_len == 0) {
    /* initialize vertex format */
    if (!GPU_crappy_amd_driver()) {
      /* Some AMD drivers strangely crash with a vbo with this format. */
      attr_id.wd = GPU_vertformat_attr_add(
          &format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }
    else {
      attr_id.wd = GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
  }

  GPU_vertbuf_init_with_format(*vbo, format);
  GPU_vertbuf_data_alloc(*vbo, vert_len);

  if (GPU_vertbuf_get_format(vbo)->stride == 1) {
    memset(vbo->data<uint8_t>().data(), 0xFF, size_t(vert_len));
  }
  else {
    GPUVertBufRaw wd_step;
    GPU_vertbuf_attr_get_raw_data(vbo, attr_id.wd, &wd_step);
    for (int i = 0; i < vert_len; i++) {
      *((float *)GPU_vertbuf_raw_step(&wd_step)) = 1.0f;
    }
  }
}

}  // namespace blender::draw

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Object API
 *
 * \note Curve and text objects evaluate to the evaluated geometry set's mesh component if
 * they have a surface, so curve objects themselves do not have a surface (the mesh component
 * is presented to render engines as a separate object).
 * \{ */

blender::gpu::Batch *DRW_cache_object_all_edges_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_all_edges_get(ob);
    /* TODO: should match #DRW_cache_object_surface_get. */
    default:
      return nullptr;
  }
}

blender::gpu::Batch *DRW_cache_object_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_edge_detection_get(ob, r_is_manifold);
    default:
      return nullptr;
  }
}

blender::gpu::Batch *DRW_cache_object_face_wireframe_get(const Scene *scene, Object *ob)
{
  using namespace blender::draw;
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

blender::gpu::Batch *DRW_cache_object_loose_edges_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_loose_edges_get(ob);
    default:
      return nullptr;
  }
}

blender::gpu::Batch *DRW_cache_object_surface_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_surface_get(ob);
    default:
      return nullptr;
  }
}

blender::gpu::VertBuf *DRW_cache_object_pos_vertbuf_get(Object *ob)
{
  using namespace blender::draw;
  Mesh *mesh = BKE_object_get_evaluated_mesh_no_subsurf_unchecked(ob);
  short type = (mesh != nullptr) ? short(OB_MESH) : ob->type;

  switch (type) {
    case OB_MESH:
      return DRW_mesh_batch_cache_pos_vertbuf_get(
          *static_cast<Mesh *>((mesh != nullptr) ? mesh : ob->data));
    default:
      return nullptr;
  }
}

Span<blender::gpu::Batch *> DRW_cache_object_surface_material_get(
    Object *ob, const Span<const GPUMaterial *> materials)
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

blender::gpu::Batch *DRW_cache_mesh_all_verts_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_all_verts(*static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_all_edges_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_all_edges(*static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_loose_edges_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_loose_edges(*static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_edge_detection(*static_cast<Mesh *>(ob->data), r_is_manifold);
}

blender::gpu::Batch *DRW_cache_mesh_surface_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface(*static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_surface_edges_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_edges(*ob, *static_cast<Mesh *>(ob->data));
}

Span<blender::gpu::Batch *> DRW_cache_mesh_surface_shaded_get(
    Object *ob, const blender::Span<const GPUMaterial *> materials)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_shaded(*ob, *static_cast<Mesh *>(ob->data), materials);
}

Span<blender::gpu::Batch *> DRW_cache_mesh_surface_texpaint_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_texpaint(*ob, *static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_surface_texpaint_single_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_texpaint_single(*ob, *static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_surface_vertpaint_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_vertpaint(*ob, *static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_surface_sculptcolors_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_sculpt(*ob, *static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_surface_weights_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_weights(*static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_face_wireframe_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_wireframes_face(*static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_surface_mesh_analysis_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_edit_mesh_analysis(*static_cast<Mesh *>(ob->data));
}

blender::gpu::Batch *DRW_cache_mesh_surface_viewer_attribute_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_viewer_attribute(*static_cast<Mesh *>(ob->data));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve
 * \{ */

blender::gpu::Batch *DRW_cache_curve_edge_wire_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_CURVES_LEGACY);
  Curve *cu = static_cast<Curve *>(ob->data);
  return DRW_curve_batch_cache_get_wire_edge(cu);
}

blender::gpu::Batch *DRW_cache_curve_edge_wire_viewer_attribute_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_CURVES_LEGACY);
  Curve *cu = static_cast<Curve *>(ob->data);
  return DRW_curve_batch_cache_get_wire_edge_viewer_attribute(cu);
}

blender::gpu::Batch *DRW_cache_curve_edge_normal_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_CURVES_LEGACY);
  Curve *cu = static_cast<Curve *>(ob->data);
  return DRW_curve_batch_cache_get_normal_edge(cu);
}

blender::gpu::Batch *DRW_cache_curve_edge_overlay_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF));

  Curve *cu = static_cast<Curve *>(ob->data);
  return DRW_curve_batch_cache_get_edit_edges(cu);
}

blender::gpu::Batch *DRW_cache_curve_vert_overlay_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF));

  Curve *cu = static_cast<Curve *>(ob->data);
  return DRW_curve_batch_cache_get_edit_verts(cu);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Font
 * \{ */

blender::gpu::Batch *DRW_cache_text_edge_wire_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_FONT);
  Curve *cu = static_cast<Curve *>(ob->data);
  return DRW_curve_batch_cache_get_wire_edge(cu);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

blender::gpu::Batch *DRW_cache_surf_edge_wire_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_SURF);
  Curve *cu = static_cast<Curve *>(ob->data);
  return DRW_curve_batch_cache_get_wire_edge(cu);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice
 * \{ */

blender::gpu::Batch *DRW_cache_lattice_verts_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_LATTICE);

  Lattice *lt = static_cast<Lattice *>(ob->data);
  return DRW_lattice_batch_cache_get_all_verts(lt);
}

blender::gpu::Batch *DRW_cache_lattice_wire_get(Object *ob, bool use_weight)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_LATTICE);

  Lattice *lt = static_cast<Lattice *>(ob->data);
  int actdef = -1;

  if (use_weight && !BLI_listbase_is_empty(&lt->vertex_group_names) && lt->editlatt->latt->dvert) {
    actdef = lt->vertex_group_active_index - 1;
  }

  return DRW_lattice_batch_cache_get_all_edges(lt, use_weight, actdef);
}

blender::gpu::Batch *DRW_cache_lattice_vert_overlay_get(Object *ob)
{
  using namespace blender::draw;
  BLI_assert(ob->type == OB_LATTICE);

  Lattice *lt = static_cast<Lattice *>(ob->data);
  return DRW_lattice_batch_cache_get_edit_verts(lt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name PointCloud
 * \{ */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

namespace blender::draw {

blender::gpu::Batch *DRW_cache_volume_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_VOLUME);
  return DRW_volume_batch_cache_get_wireframes_face(static_cast<Volume *>(ob->data));
}

blender::gpu::Batch *DRW_cache_volume_selection_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_VOLUME);
  return DRW_volume_batch_cache_get_selection_surface(static_cast<Volume *>(ob->data));
}

}  // namespace blender::draw

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particles
 * \{ */

blender::gpu::Batch *DRW_cache_particles_get_hair(Object *object,
                                                  ParticleSystem *psys,
                                                  ModifierData *md)
{
  using namespace blender::draw;
  return DRW_particles_batch_cache_get_hair(object, psys, md);
}

blender::gpu::Batch *DRW_cache_particles_get_dots(Object *object, ParticleSystem *psys)
{
  using namespace blender::draw;
  return DRW_particles_batch_cache_get_dots(object, psys);
}

blender::gpu::Batch *DRW_cache_particles_get_edit_strands(Object *object,
                                                          ParticleSystem *psys,
                                                          PTCacheEdit *edit,
                                                          bool use_weight)
{
  using namespace blender::draw;
  return DRW_particles_batch_cache_get_edit_strands(object, psys, edit, use_weight);
}

blender::gpu::Batch *DRW_cache_particles_get_edit_inner_points(Object *object,
                                                               ParticleSystem *psys,
                                                               PTCacheEdit *edit)
{
  using namespace blender::draw;
  return DRW_particles_batch_cache_get_edit_inner_points(object, psys, edit);
}

blender::gpu::Batch *DRW_cache_particles_get_edit_tip_points(Object *object,
                                                             ParticleSystem *psys,
                                                             PTCacheEdit *edit)
{
  using namespace blender::draw;
  return DRW_particles_batch_cache_get_edit_tip_points(object, psys, edit);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Batch Cache Implementation (common)
 * \{ */

void drw_batch_cache_validate(Object *ob)
{
  using namespace blender::draw;
  switch (ob->type) {
    case OB_MESH:
      DRW_mesh_batch_cache_validate(*(Mesh *)ob->data);
      break;
    case OB_CURVES_LEGACY:
    case OB_FONT:
    case OB_SURF:
      DRW_curve_batch_cache_validate((Curve *)ob->data);
      break;
    case OB_LATTICE:
      DRW_lattice_batch_cache_validate((Lattice *)ob->data);
      break;
    case OB_CURVES:
      DRW_curves_batch_cache_validate((Curves *)ob->data);
      break;
    case OB_POINTCLOUD:
      DRW_pointcloud_batch_cache_validate((PointCloud *)ob->data);
      break;
    case OB_VOLUME:
      DRW_volume_batch_cache_validate((Volume *)ob->data);
      break;
    case OB_GREASE_PENCIL:
      DRW_grease_pencil_batch_cache_validate((GreasePencil *)ob->data);
    default:
      break;
  }
}

void drw_batch_cache_generate_requested(Object *ob)
{
  using namespace blender::draw;
  const DRWContextState *draw_ctx = DRW_context_state_get();
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
      DRW_mesh_batch_cache_create_requested(
          *DST.task_graph, *ob, *(Mesh *)ob->data, *scene, is_paint_mode, use_hide);
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

void drw_batch_cache_generate_requested_evaluated_mesh_or_curve(Object *ob)
{
  using namespace blender::draw;
  /* NOTE: Logic here is duplicated from #drw_batch_cache_generate_requested. */

  const DRWContextState *draw_ctx = DRW_context_state_get();
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
    DRW_mesh_batch_cache_create_requested(
        *DST.task_graph, *ob, *mesh, *scene, is_paint_mode, use_hide);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    DRW_curve_batch_cache_create_requested(ob, scene);
  }
}

void drw_batch_cache_generate_requested_delayed(Object *ob)
{
  BLI_gset_add(DST.delayed_extraction, ob);
}

namespace blender::draw {
void DRW_batch_cache_free_old(Object *ob, int ctime)
{
  switch (ob->type) {
    case OB_MESH:
      DRW_mesh_batch_cache_free_old((Mesh *)ob->data, ctime);
      break;
    case OB_CURVES:
      DRW_curves_batch_cache_free_old((Curves *)ob->data, ctime);
      break;
    case OB_POINTCLOUD:
      DRW_pointcloud_batch_cache_free_old((PointCloud *)ob->data, ctime);
      break;
    default:
      break;
  }
}
}  // namespace blender::draw

/** \} */

void DRW_cdlayer_attr_aliases_add(GPUVertFormat *format,
                                  const char *base_name,
                                  const int data_type,
                                  const char *layer_name,
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
    GPU_vertformat_alias_add(format, data_type == CD_PROP_FLOAT2 ? "a" : base_name);
  }

  /* Active display layer name. */
  if (is_active_layer) {
    SNPRINTF(attr_name, "a%s", base_name);
    GPU_vertformat_alias_add(format, attr_name);
  }
}
