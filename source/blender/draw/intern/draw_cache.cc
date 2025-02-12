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

/* Sphere shape resolution */
/* Low */
#define DRW_SPHERE_SHAPE_LATITUDE_LOW 32
#define DRW_SPHERE_SHAPE_LONGITUDE_LOW 24
/* Medium */
#define DRW_SPHERE_SHAPE_LATITUDE_MEDIUM 64
#define DRW_SPHERE_SHAPE_LONGITUDE_MEDIUM 48
/* High */
#define DRW_SPHERE_SHAPE_LATITUDE_HIGH 80
#define DRW_SPHERE_SHAPE_LONGITUDE_HIGH 60

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Types
 * \{ */

struct Vert {
  float pos[3];
  int v_class;

  /** Allows creating a pointer to `Vert` in a single expression. */
  operator const void *() const
  {
    return this;
  }
};

struct VertShaded {
  float pos[3];
  int v_class;
  float nor[3];

  operator const void *() const
  {
    return this;
  }
};

/* Batch's only (freed as an array). */
static struct DRWShapeCache {
  blender::gpu::Batch *drw_procedural_verts;
  blender::gpu::Batch *drw_procedural_lines;
  blender::gpu::Batch *drw_procedural_tris;
  blender::gpu::Batch *drw_procedural_tri_strips;
  blender::gpu::Batch *drw_cursor;
  blender::gpu::Batch *drw_cursor_only_circle;
  blender::gpu::Batch *drw_quad;
  blender::gpu::Batch *drw_cube;
  blender::gpu::Batch *drw_sphere_lod[DRW_LOD_MAX];
} SHC = {nullptr};

void DRW_shape_cache_free()
{
  uint i = sizeof(SHC) / sizeof(blender::gpu::Batch *);
  blender::gpu::Batch **batch = (blender::gpu::Batch **)&SHC;
  while (i--) {
    GPU_BATCH_DISCARD_SAFE(*batch);
    batch++;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Procedural Batches
 * \{ */

blender::gpu::Batch *drw_cache_procedural_points_get()
{
  if (!SHC.drw_procedural_verts) {
    /* TODO(fclem): get rid of this dummy VBO. */
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(*vbo, 1);

    SHC.drw_procedural_verts = GPU_batch_create_ex(
        GPU_PRIM_POINTS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_procedural_verts;
}

blender::gpu::Batch *drw_cache_procedural_lines_get()
{
  if (!SHC.drw_procedural_lines) {
    /* TODO(fclem): get rid of this dummy VBO. */
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(*vbo, 1);

    SHC.drw_procedural_lines = GPU_batch_create_ex(
        GPU_PRIM_LINES, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_procedural_lines;
}

blender::gpu::Batch *drw_cache_procedural_triangles_get()
{
  if (!SHC.drw_procedural_tris) {
    /* TODO(fclem): get rid of this dummy VBO. */
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(*vbo, 1);

    SHC.drw_procedural_tris = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_procedural_tris;
}

blender::gpu::Batch *drw_cache_procedural_triangle_strips_get()
{
  if (!SHC.drw_procedural_tri_strips) {
    /* TODO(fclem): get rid of this dummy VBO. */
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(*vbo, 1);

    SHC.drw_procedural_tri_strips = GPU_batch_create_ex(
        GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_procedural_tri_strips;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helper functions
 * \{ */

static GPUVertFormat extra_vert_format()
{
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  GPU_vertformat_attr_add(&format, "vclass", GPU_COMP_I32, 1, GPU_FETCH_INT);
  return format;
}

static void UNUSED_FUNCTION(add_fancy_edge)(blender::gpu::VertBuf *vbo,
                                            uint pos_id,
                                            uint n1_id,
                                            uint n2_id,
                                            uint *v_idx,
                                            const float co1[3],
                                            const float co2[3],
                                            const float n1[3],
                                            const float n2[3])
{
  GPU_vertbuf_attr_set(vbo, n1_id, *v_idx, n1);
  GPU_vertbuf_attr_set(vbo, n2_id, *v_idx, n2);
  GPU_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, co1);

  GPU_vertbuf_attr_set(vbo, n1_id, *v_idx, n1);
  GPU_vertbuf_attr_set(vbo, n2_id, *v_idx, n2);
  GPU_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, co2);
}

#if 0  /* UNUSED */
static void add_lat_lon_vert(blender::gpu::VertBuf *vbo,
                             uint pos_id,
                             uint nor_id,
                             uint *v_idx,
                             const float rad,
                             const float lat,
                             const float lon)
{
  float pos[3], nor[3];
  nor[0] = sinf(lat) * cosf(lon);
  nor[1] = cosf(lat);
  nor[2] = sinf(lat) * sinf(lon);
  mul_v3_v3fl(pos, nor, rad);

  GPU_vertbuf_attr_set(vbo, nor_id, *v_idx, nor);
  GPU_vertbuf_attr_set(vbo, pos_id, (*v_idx)++, pos);
}

static blender::gpu::VertBuf *fill_arrows_vbo(const float scale)
{
  /* Position Only 3D format */
  static GPUVertFormat format = {0};
  static struct {
    uint pos;
  } attr_id;
  if (format.attr_len == 0) {
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  /* Line */
  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*vbo, 6 * 3);

  float v1[3] = {0.0, 0.0, 0.0};
  float v2[3] = {0.0, 0.0, 0.0};
  float vtmp1[3], vtmp2[3];

  for (int axis = 0; axis < 3; axis++) {
    const int arrow_axis = (axis == 0) ? 1 : 0;

    v2[axis] = 1.0f;
    mul_v3_v3fl(vtmp1, v1, scale);
    mul_v3_v3fl(vtmp2, v2, scale);
    GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 0, vtmp1);
    GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 1, vtmp2);

    v1[axis] = 0.85f;
    v1[arrow_axis] = -0.08f;
    mul_v3_v3fl(vtmp1, v1, scale);
    mul_v3_v3fl(vtmp2, v2, scale);
    GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 2, vtmp1);
    GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 3, vtmp2);

    v1[arrow_axis] = 0.08f;
    mul_v3_v3fl(vtmp1, v1, scale);
    mul_v3_v3fl(vtmp2, v2, scale);
    GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 4, vtmp1);
    GPU_vertbuf_attr_set(vbo, attr_id.pos, axis * 6 + 5, vtmp2);

    /* reset v1 & v2 to zero */
    v1[arrow_axis] = v1[axis] = v2[axis] = 0.0f;
  }

  return vbo;
}
#endif /* UNUSED */

/* Quads */

blender::gpu::Batch *DRW_cache_quad_get()
{
  if (!SHC.drw_quad) {
    GPUVertFormat format = extra_vert_format();

    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(*vbo, 4);

    int v = 0;
    int flag = VCLASS_EMPTY_SCALED;
    const float p[4][2] = {{-1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, -1.0f}};
    for (int a = 0; a < 4; a++) {
      GPU_vertbuf_vert_set(vbo, v++, Vert{{p[a][0], p[a][1], 0.0f}, flag});
    }

    SHC.drw_quad = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_quad;
}

/* Sphere */
static void sphere_lat_lon_vert(blender::gpu::VertBuf *vbo, int *v_ofs, float lat, float lon)
{
  float x = sinf(lat) * cosf(lon);
  float y = cosf(lat);
  float z = sinf(lat) * sinf(lon);
  GPU_vertbuf_vert_set(vbo, *v_ofs, VertShaded{{x, y, z}, VCLASS_EMPTY_SCALED, {x, y, z}});
  (*v_ofs)++;
}

blender::gpu::Batch *DRW_cache_sphere_get(const eDRWLevelOfDetail level_of_detail)
{
  BLI_assert(level_of_detail >= DRW_LOD_LOW && level_of_detail < DRW_LOD_MAX);

  if (!SHC.drw_sphere_lod[level_of_detail]) {
    int lat_res;
    int lon_res;

    switch (level_of_detail) {
      case DRW_LOD_LOW:
        lat_res = DRW_SPHERE_SHAPE_LATITUDE_LOW;
        lon_res = DRW_SPHERE_SHAPE_LONGITUDE_LOW;
        break;
      case DRW_LOD_MEDIUM:
        lat_res = DRW_SPHERE_SHAPE_LATITUDE_MEDIUM;
        lon_res = DRW_SPHERE_SHAPE_LONGITUDE_MEDIUM;
        break;
      case DRW_LOD_HIGH:
        lat_res = DRW_SPHERE_SHAPE_LATITUDE_HIGH;
        lon_res = DRW_SPHERE_SHAPE_LONGITUDE_HIGH;
        break;
      default:
        return nullptr;
    }

    GPUVertFormat format = extra_vert_format();
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    int v_len = (lat_res - 1) * lon_res * 6;
    GPU_vertbuf_data_alloc(*vbo, v_len);

    const float lon_inc = 2 * M_PI / lon_res;
    const float lat_inc = M_PI / lat_res;
    float lon, lat;

    int v = 0;
    lon = 0.0f;
    for (int i = 0; i < lon_res; i++, lon += lon_inc) {
      lat = 0.0f;
      for (int j = 0; j < lat_res; j++, lat += lat_inc) {
        if (j != lat_res - 1) { /* Pole */
          sphere_lat_lon_vert(vbo, &v, lat + lat_inc, lon + lon_inc);
          sphere_lat_lon_vert(vbo, &v, lat + lat_inc, lon);
          sphere_lat_lon_vert(vbo, &v, lat, lon);
        }
        if (j != 0) { /* Pole */
          sphere_lat_lon_vert(vbo, &v, lat, lon + lon_inc);
          sphere_lat_lon_vert(vbo, &v, lat + lat_inc, lon + lon_inc);
          sphere_lat_lon_vert(vbo, &v, lat, lon);
        }
      }
    }

    SHC.drw_sphere_lod[level_of_detail] = GPU_batch_create_ex(
        GPU_PRIM_TRIS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_sphere_lod[level_of_detail];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

/* XXX TODO: move that 1 unit cube to more common/generic place? */
static const float bone_box_verts[8][3] = {
    {1.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, -1.0f},
    {-1.0f, 0.0f, -1.0f},
    {-1.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
    {-1.0f, 1.0f, 1.0f},
};

static const uint bone_box_solid_tris[12][3] = {
    {0, 2, 1}, /* bottom */
    {0, 3, 2},

    {0, 1, 5}, /* sides */
    {0, 5, 4},

    {1, 2, 6},
    {1, 6, 5},

    {2, 3, 7},
    {2, 7, 6},

    {3, 0, 4},
    {3, 4, 7},

    {4, 5, 6}, /* top */
    {4, 6, 7},
};

blender::gpu::Batch *DRW_cache_cube_get()
{
  if (!SHC.drw_cube) {
    GPUVertFormat format = extra_vert_format();

    const int tri_len = ARRAY_SIZE(bone_box_solid_tris);
    const int vert_len = ARRAY_SIZE(bone_box_verts);

    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(*vbo, vert_len);

    GPUIndexBufBuilder elb;
    GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, vert_len);

    int v = 0;
    for (int i = 0; i < vert_len; i++) {
      float x = bone_box_verts[i][0];
      float y = bone_box_verts[i][1] * 2.0f - 1.0f;
      float z = bone_box_verts[i][2];
      GPU_vertbuf_vert_set(vbo, v++, Vert{{x, y, z}, VCLASS_EMPTY_SCALED});
    }

    for (int i = 0; i < tri_len; i++) {
      const uint *tri_indices = bone_box_solid_tris[i];
      GPU_indexbuf_add_tri_verts(&elb, tri_indices[0], tri_indices[1], tri_indices[2]);
    }

    SHC.drw_cube = GPU_batch_create_ex(
        GPU_PRIM_TRIS, vbo, GPU_indexbuf_build(&elb), GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
  }
  return SHC.drw_cube;
}
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

blender::gpu::Batch *DRW_cache_cursor_get(bool crosshair_lines)
{
  blender::gpu::Batch **drw_cursor = crosshair_lines ? &SHC.drw_cursor :
                                                       &SHC.drw_cursor_only_circle;

  if (*drw_cursor == nullptr) {
    const float f5 = 0.25f;
    const float f10 = 0.5f;
    const float f20 = 1.0f;

    const int segments = 16;
    const int vert_len = segments + 8;
    const int index_len = vert_len + 5;

    const float red[3] = {1.0f, 0.0f, 0.0f};
    const float white[3] = {1.0f, 1.0f, 1.0f};

    static GPUVertFormat format = {0};
    static struct {
      uint pos, color;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      attr_id.color = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }

    GPUIndexBufBuilder elb;
    GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, index_len, vert_len);

    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(*vbo, vert_len);

    int v = 0;
    for (int i = 0; i < segments; i++) {
      float angle = float(2 * M_PI) * (float(i) / float(segments));
      float x = f10 * cosf(angle);
      float y = f10 * sinf(angle);

      GPU_vertbuf_attr_set(vbo, attr_id.color, v, (i % 2 == 0) ? red : white);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{x, y});
      GPU_indexbuf_add_generic_vert(&elb, v++);
    }
    GPU_indexbuf_add_generic_vert(&elb, 0);

    if (crosshair_lines) {
      float crosshair_color[3];
      UI_GetThemeColor3fv(TH_VIEW_OVERLAY, crosshair_color);

      /* TODO(fclem): Remove primitive restart. Incompatible with wide lines. */
      GPU_indexbuf_add_primitive_restart(&elb);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{-f20, 0});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{-f5, 0});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);

      GPU_indexbuf_add_primitive_restart(&elb);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{+f5, 0});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{+f20, 0});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);

      GPU_indexbuf_add_primitive_restart(&elb);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{0, -f20});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{0, -f5});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);

      GPU_indexbuf_add_primitive_restart(&elb);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{0, +f5});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, blender::float2{0, +f20});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
    }

    blender::gpu::IndexBuf *ibo = GPU_indexbuf_build(&elb);

    *drw_cursor = GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo, ibo, GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
  }
  return *drw_cursor;
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
