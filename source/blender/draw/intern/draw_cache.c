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
 */

/** \file
 * \ingroup draw
 */

#include "DNA_curve_types.h"
#include "DNA_hair_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "UI_resources.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_object.h"
#include "BKE_paint.h"

#include "GPU_batch.h"
#include "GPU_batch_utils.h"

#include "MEM_guardedalloc.h"

#include "draw_cache.h"
#include "draw_cache_impl.h"
#include "draw_manager.h"

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

typedef struct Vert {
  float pos[3];
  int class;
} Vert;

typedef struct VertShaded {
  float pos[3];
  int class;
  float nor[3];
} VertShaded;

/* Batch's only (free'd as an array) */
static struct DRWShapeCache {
  GPUBatch *drw_procedural_verts;
  GPUBatch *drw_procedural_lines;
  GPUBatch *drw_procedural_tris;
  GPUBatch *drw_cursor;
  GPUBatch *drw_cursor_only_circle;
  GPUBatch *drw_fullscreen_quad;
  GPUBatch *drw_quad;
  GPUBatch *drw_quad_wires;
  GPUBatch *drw_grid;
  GPUBatch *drw_sphere;
  GPUBatch *drw_plain_axes;
  GPUBatch *drw_single_arrow;
  GPUBatch *drw_cube;
  GPUBatch *drw_circle;
  GPUBatch *drw_normal_arrow;
  GPUBatch *drw_empty_cube;
  GPUBatch *drw_empty_sphere;
  GPUBatch *drw_empty_cylinder;
  GPUBatch *drw_empty_capsule_body;
  GPUBatch *drw_empty_capsule_cap;
  GPUBatch *drw_empty_cone;
  GPUBatch *drw_field_wind;
  GPUBatch *drw_field_force;
  GPUBatch *drw_field_vortex;
  GPUBatch *drw_field_curve;
  GPUBatch *drw_field_tube_limit;
  GPUBatch *drw_field_cone_limit;
  GPUBatch *drw_field_sphere_limit;
  GPUBatch *drw_ground_line;
  GPUBatch *drw_light_point_lines;
  GPUBatch *drw_light_sun_lines;
  GPUBatch *drw_light_spot_lines;
  GPUBatch *drw_light_spot_volume;
  GPUBatch *drw_light_area_disk_lines;
  GPUBatch *drw_light_area_square_lines;
  GPUBatch *drw_speaker;
  GPUBatch *drw_lightprobe_cube;
  GPUBatch *drw_lightprobe_planar;
  GPUBatch *drw_lightprobe_grid;
  GPUBatch *drw_bone_octahedral;
  GPUBatch *drw_bone_octahedral_wire;
  GPUBatch *drw_bone_box;
  GPUBatch *drw_bone_box_wire;
  GPUBatch *drw_bone_envelope;
  GPUBatch *drw_bone_envelope_outline;
  GPUBatch *drw_bone_point;
  GPUBatch *drw_bone_point_wire;
  GPUBatch *drw_bone_stick;
  GPUBatch *drw_bone_arrows;
  GPUBatch *drw_bone_dof_sphere;
  GPUBatch *drw_bone_dof_lines;
  GPUBatch *drw_camera_frame;
  GPUBatch *drw_camera_tria;
  GPUBatch *drw_camera_tria_wire;
  GPUBatch *drw_camera_distances;
  GPUBatch *drw_camera_volume;
  GPUBatch *drw_camera_volume_wire;
  GPUBatch *drw_particle_cross;
  GPUBatch *drw_particle_circle;
  GPUBatch *drw_particle_axis;
  GPUBatch *drw_gpencil_dummy_quad;
} SHC = {NULL};

void DRW_shape_cache_free(void)
{
  uint i = sizeof(SHC) / sizeof(GPUBatch *);
  GPUBatch **batch = (GPUBatch **)&SHC;
  while (i--) {
    GPU_BATCH_DISCARD_SAFE(*batch);
    batch++;
  }
}

void DRW_shape_cache_reset(void)
{
  uint i = sizeof(SHC) / sizeof(GPUBatch *);
  GPUBatch **batch = (GPUBatch **)&SHC;
  while (i--) {
    if (*batch) {
      GPU_batch_vao_cache_clear(*batch);
    }
    batch++;
  }
}

/* -------------------------------------------------------------------- */
/** \name Procedural Batches
 * \{ */

GPUBatch *drw_cache_procedural_points_get(void)
{
  if (!SHC.drw_procedural_verts) {
    /* TODO(fclem) get rid of this dummy VBO. */
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 1);

    SHC.drw_procedural_verts = GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_procedural_verts;
}

GPUBatch *drw_cache_procedural_lines_get(void)
{
  if (!SHC.drw_procedural_lines) {
    /* TODO(fclem) get rid of this dummy VBO. */
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 1);

    SHC.drw_procedural_lines = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_procedural_lines;
}

GPUBatch *drw_cache_procedural_triangles_get(void)
{
  if (!SHC.drw_procedural_tris) {
    /* TODO(fclem) get rid of this dummy VBO. */
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 1);

    SHC.drw_procedural_tris = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_procedural_tris;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helper functions
 * \{ */

static GPUVertFormat extra_vert_format(void)
{
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  GPU_vertformat_attr_add(&format, "vclass", GPU_COMP_I32, 1, GPU_FETCH_INT);
  return format;
}

static void UNUSED_FUNCTION(add_fancy_edge)(GPUVertBuf *vbo,
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
static void add_lat_lon_vert(GPUVertBuf *vbo,
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

static GPUVertBuf *fill_arrows_vbo(const float scale)
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
  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, 6 * 3);

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

static GPUVertBuf *sphere_wire_vbo(const float rad, int flag)
{
#define NSEGMENTS 32
  /* Position Only 3D format */
  GPUVertFormat format = extra_vert_format();

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 2 * 3);

  int v = 0;
  /* a single ring of vertices */
  float p[NSEGMENTS][2];
  for (int i = 0; i < NSEGMENTS; i++) {
    float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
    p[i][0] = rad * cosf(angle);
    p[i][1] = rad * sinf(angle);
  }

  for (int axis = 0; axis < 3; axis++) {
    for (int i = 0; i < NSEGMENTS; i++) {
      for (int j = 0; j < 2; j++) {
        float cv[2];

        cv[0] = p[(i + j) % NSEGMENTS][0];
        cv[1] = p[(i + j) % NSEGMENTS][1];

        if (axis == 0) {
          GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], cv[1], 0.0f}, flag});
        }
        else if (axis == 1) {
          GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], 0.0f, cv[1]}, flag});
        }
        else {
          GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, cv[0], cv[1]}, flag});
        }
      }
    }
  }

  return vbo;
#undef NSEGMENTS
}

/* Quads */
/* Use this one for rendering fullscreen passes. For 3D objects use DRW_cache_quad_get(). */
GPUBatch *DRW_cache_fullscreen_quad_get(void)
{
  if (!SHC.drw_fullscreen_quad) {
    /* Use a triangle instead of a real quad */
    /* https://www.slideshare.net/DevCentralAMD/vertex-shader-tricks-bill-bilodeau - slide 14 */
    float pos[3][2] = {{-1.0f, -1.0f}, {3.0f, -1.0f}, {-1.0f, 3.0f}};
    float uvs[3][2] = {{0.0f, 0.0f}, {2.0f, 0.0f}, {0.0f, 2.0f}};

    /* Position Only 2D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos, uvs;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      attr_id.uvs = GPU_vertformat_attr_add(&format, "uvs", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      GPU_vertformat_alias_add(&format, "texCoord");
      GPU_vertformat_alias_add(&format, "orco"); /* Fix driver bug (see T70004) */
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 3);

    for (int i = 0; i < 3; i++) {
      GPU_vertbuf_attr_set(vbo, attr_id.pos, i, pos[i]);
      GPU_vertbuf_attr_set(vbo, attr_id.uvs, i, uvs[i]);
    }

    SHC.drw_fullscreen_quad = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_fullscreen_quad;
}

/* Just a regular quad with 4 vertices. */
GPUBatch *DRW_cache_quad_get(void)
{
  if (!SHC.drw_quad) {
    GPUVertFormat format = extra_vert_format();

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 4);

    int v = 0;
    int flag = VCLASS_EMPTY_SCALED;
    float p[4][2] = {{-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}};
    for (int a = 0; a < 4; a++) {
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[a][0], p[a][1], 0.0f}, flag});
    }

    SHC.drw_quad = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_quad;
}

/* Just a regular quad with 4 vertices - wires. */
GPUBatch *DRW_cache_quad_wires_get(void)
{
  if (!SHC.drw_quad_wires) {
    GPUVertFormat format = extra_vert_format();

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 5);

    int v = 0;
    int flag = VCLASS_EMPTY_SCALED;
    float p[4][2] = {{-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}};
    for (int a = 0; a < 5; a++) {
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[a % 4][0], p[a % 4][1], 0.0f}, flag});
    }

    SHC.drw_quad_wires = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_quad_wires;
}

/* Grid */
GPUBatch *DRW_cache_grid_get(void)
{
  if (!SHC.drw_grid) {
    /* Position Only 2D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 8 * 8 * 2 * 3);

    uint v_idx = 0;
    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        float pos0[2] = {(float)i / 8.0f, (float)j / 8.0f};
        float pos1[2] = {(float)(i + 1) / 8.0f, (float)j / 8.0f};
        float pos2[2] = {(float)i / 8.0f, (float)(j + 1) / 8.0f};
        float pos3[2] = {(float)(i + 1) / 8.0f, (float)(j + 1) / 8.0f};

        madd_v2_v2v2fl(pos0, (float[2]){-1.0f, -1.0f}, pos0, 2.0f);
        madd_v2_v2v2fl(pos1, (float[2]){-1.0f, -1.0f}, pos1, 2.0f);
        madd_v2_v2v2fl(pos2, (float[2]){-1.0f, -1.0f}, pos2, 2.0f);
        madd_v2_v2v2fl(pos3, (float[2]){-1.0f, -1.0f}, pos3, 2.0f);

        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, pos0);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, pos1);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, pos2);

        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, pos2);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, pos1);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, pos3);
      }
    }

    SHC.drw_grid = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_grid;
}

/* Sphere */
static void sphere_lat_lon_vert(GPUVertBuf *vbo, int *v_ofs, float lat, float lon)
{
  float x = sinf(lat) * cosf(lon);
  float y = cosf(lat);
  float z = sinf(lat) * sinf(lon);
  GPU_vertbuf_vert_set(vbo, *v_ofs, &(VertShaded){{x, y, z}, VCLASS_EMPTY_SCALED, {x, y, z}});
  (*v_ofs)++;
}

GPUBatch *DRW_cache_sphere_get(void)
{
  if (!SHC.drw_sphere) {
    const int lat_res = 32;
    const int lon_res = 24;

    GPUVertFormat format = extra_vert_format();
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    int v_len = (lat_res - 1) * lon_res * 6;
    GPU_vertbuf_data_alloc(vbo, v_len);

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

    SHC.drw_sphere = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_sphere;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

static void circle_verts(
    GPUVertBuf *vbo, int *vert_idx, int segments, float radius, float z, int flag)
{
  for (int a = 0; a < segments; a++) {
    for (int b = 0; b < 2; b++) {
      float angle = (2.0f * M_PI * (a + b)) / segments;
      float s = sinf(angle) * radius;
      float c = cosf(angle) * radius;
      int v = *vert_idx;
      *vert_idx = v + 1;
      GPU_vertbuf_vert_set(vbo, v, &(Vert){{s, c, z}, flag});
    }
  }
}

static void circle_dashed_verts(
    GPUVertBuf *vbo, int *vert_idx, int segments, float radius, float z, int flag)
{
  for (int a = 0; a < segments * 2; a += 2) {
    for (int b = 0; b < 2; b++) {
      float angle = (2.0f * M_PI * (a + b)) / (segments * 2);
      float s = sinf(angle) * radius;
      float c = cosf(angle) * radius;
      int v = *vert_idx;
      *vert_idx = v + 1;
      GPU_vertbuf_vert_set(vbo, v, &(Vert){{s, c, z}, flag});
    }
  }
}

/* XXX TODO move that 1 unit cube to more common/generic place? */
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

static const float bone_box_smooth_normals[8][3] = {
    {M_SQRT3, -M_SQRT3, M_SQRT3},
    {M_SQRT3, -M_SQRT3, -M_SQRT3},
    {-M_SQRT3, -M_SQRT3, -M_SQRT3},
    {-M_SQRT3, -M_SQRT3, M_SQRT3},
    {M_SQRT3, M_SQRT3, M_SQRT3},
    {M_SQRT3, M_SQRT3, -M_SQRT3},
    {-M_SQRT3, M_SQRT3, -M_SQRT3},
    {-M_SQRT3, M_SQRT3, M_SQRT3},
};

static const uint bone_box_wire[24] = {
    0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7,
};

#if 0 /* UNUSED */
/* aligned with bone_octahedral_wire
 * Contains adjacent normal index */
static const uint bone_box_wire_adjacent_face[24] = {
    0, 2, 0, 4, 1, 6, 1, 8, 3, 10, 5, 10, 7, 11, 9, 11, 3, 8, 2, 5, 4, 7, 6, 9,
};
#endif

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

/**
 * Store indices of generated verts from bone_box_solid_tris to define adjacency infos.
 * See bone_octahedral_solid_tris for more infos.
 */
static const uint bone_box_wire_lines_adjacency[12][4] = {
    {4, 2, 0, 11},
    {0, 1, 2, 8},
    {2, 4, 1, 14},
    {1, 0, 4, 20}, /* bottom */
    {0, 8, 11, 14},
    {2, 14, 8, 20},
    {1, 20, 14, 11},
    {4, 11, 20, 8}, /* top */
    {20, 0, 11, 2},
    {11, 2, 8, 1},
    {8, 1, 14, 4},
    {14, 4, 20, 0}, /* sides */
};

#if 0 /* UNUSED */
static const uint bone_box_solid_tris_adjacency[12][6] = {
    {0, 5, 1, 14, 2, 8},
    {3, 26, 4, 20, 5, 1},

    {6, 2, 7, 16, 8, 11},
    {9, 7, 10, 32, 11, 24},

    {12, 0, 13, 22, 14, 17},
    {15, 13, 16, 30, 17, 6},

    {18, 3, 19, 28, 20, 23},
    {21, 19, 22, 33, 23, 12},

    {24, 4, 25, 10, 26, 29},
    {27, 25, 28, 34, 29, 18},

    {30, 9, 31, 15, 32, 35},
    {33, 31, 34, 21, 35, 27},
};
#endif

/* aligned with bone_box_solid_tris */
static const float bone_box_solid_normals[12][3] = {
    {0.0f, -1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},

    {1.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f},

    {0.0f, 0.0f, -1.0f},
    {0.0f, 0.0f, -1.0f},

    {-1.0f, 0.0f, 0.0f},
    {-1.0f, 0.0f, 0.0f},

    {0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 1.0f},

    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
};

GPUBatch *DRW_cache_cube_get(void)
{
  if (!SHC.drw_cube) {
    GPUVertFormat format = extra_vert_format();

    const int tri_len = ARRAY_SIZE(bone_box_solid_tris);
    const int vert_len = ARRAY_SIZE(bone_box_verts);

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, vert_len);

    GPUIndexBufBuilder elb;
    GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, vert_len);

    int v = 0;
    for (int i = 0; i < vert_len; i++) {
      float x = bone_box_verts[i][0];
      float y = bone_box_verts[i][1] * 2.0f - 1.0f;
      float z = bone_box_verts[i][2];
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, z}, VCLASS_EMPTY_SCALED});
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

GPUBatch *DRW_cache_circle_get(void)
{
#define CIRCLE_RESOL 64
  if (!SHC.drw_circle) {
    GPUVertFormat format = extra_vert_format();

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL + 1);

    int v = 0;
    for (int a = 0; a < CIRCLE_RESOL + 1; a++) {
      float x = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
      float z = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
      float y = 0.0f;
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, z}, VCLASS_EMPTY_SCALED});
    }

    SHC.drw_circle = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_circle;
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_normal_arrow_get(void)
{
  if (!SHC.drw_normal_arrow) {
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 2);

    /* TODO real arrow. For now, it's a line positioned in the vertex shader. */

    SHC.drw_normal_arrow = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_normal_arrow;
}

/* -------------------------------------------------------------------- */
/** \name Dummy vbos
 *
 * We need a dummy vbo containing the vertex count to draw instances ranges.
 *
 * \{ */

GPUBatch *DRW_gpencil_dummy_buffer_get(void)
{
  if (SHC.drw_gpencil_dummy_quad == NULL) {
    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U8, 1, GPU_FETCH_INT);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 4);

    SHC.drw_gpencil_dummy_quad = GPU_batch_create_ex(
        GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_gpencil_dummy_quad;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Object API
 * \{ */

GPUBatch *DRW_cache_object_all_edges_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_all_edges_get(ob);

    /* TODO, should match 'DRW_cache_object_surface_get' */
    default:
      return NULL;
  }
}

GPUBatch *DRW_cache_object_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_edge_detection_get(ob, r_is_manifold);
    case OB_CURVE:
      return DRW_cache_curve_edge_detection_get(ob, r_is_manifold);
    case OB_SURF:
      return DRW_cache_surf_edge_detection_get(ob, r_is_manifold);
    case OB_FONT:
      return DRW_cache_text_edge_detection_get(ob, r_is_manifold);
    case OB_MBALL:
      return DRW_cache_mball_edge_detection_get(ob, r_is_manifold);
    case OB_HAIR:
      return NULL;
    case OB_POINTCLOUD:
      return NULL;
    case OB_VOLUME:
      return NULL;
    default:
      return NULL;
  }
}

GPUBatch *DRW_cache_object_face_wireframe_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_face_wireframe_get(ob);
    case OB_CURVE:
      return DRW_cache_curve_face_wireframe_get(ob);
    case OB_SURF:
      return DRW_cache_surf_face_wireframe_get(ob);
    case OB_FONT:
      return DRW_cache_text_face_wireframe_get(ob);
    case OB_MBALL:
      return DRW_cache_mball_face_wireframe_get(ob);
    case OB_HAIR:
      return NULL;
    case OB_POINTCLOUD:
      return NULL;
    case OB_VOLUME:
      return DRW_cache_volume_face_wireframe_get(ob);
    case OB_GPENCIL: {
      return DRW_cache_gpencil_face_wireframe_get(ob);
    }
    default:
      return NULL;
  }
}

GPUBatch *DRW_cache_object_loose_edges_get(struct Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_loose_edges_get(ob);
    case OB_CURVE:
      return DRW_cache_curve_loose_edges_get(ob);
    case OB_SURF:
      return DRW_cache_surf_loose_edges_get(ob);
    case OB_FONT:
      return DRW_cache_text_loose_edges_get(ob);
    case OB_MBALL:
      return NULL;
    case OB_HAIR:
      return NULL;
    case OB_POINTCLOUD:
      return NULL;
    case OB_VOLUME:
      return NULL;
    default:
      return NULL;
  }
}

GPUBatch *DRW_cache_object_surface_get(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_surface_get(ob);
    case OB_CURVE:
      return DRW_cache_curve_surface_get(ob);
    case OB_SURF:
      return DRW_cache_surf_surface_get(ob);
    case OB_FONT:
      return DRW_cache_text_surface_get(ob);
    case OB_MBALL:
      return DRW_cache_mball_surface_get(ob);
    case OB_HAIR:
      return NULL;
    case OB_POINTCLOUD:
      return NULL;
    case OB_VOLUME:
      return NULL;
    default:
      return NULL;
  }
}

/* Returns the vertbuf used by shaded surface batch. */
GPUVertBuf *DRW_cache_object_pos_vertbuf_get(Object *ob)
{
  Mesh *me = BKE_object_get_evaluated_mesh(ob);
  short type = (me != NULL) ? OB_MESH : ob->type;

  switch (type) {
    case OB_MESH:
      return DRW_mesh_batch_cache_pos_vertbuf_get((me != NULL) ? me : ob->data);
    case OB_CURVE:
    case OB_SURF:
    case OB_FONT:
      return DRW_curve_batch_cache_pos_vertbuf_get(ob->data);
    case OB_MBALL:
      return DRW_mball_batch_cache_pos_vertbuf_get(ob);
    case OB_HAIR:
      return NULL;
    case OB_POINTCLOUD:
      return NULL;
    case OB_VOLUME:
      return NULL;
    default:
      return NULL;
  }
}

int DRW_cache_object_material_count_get(struct Object *ob)
{
  Mesh *me = BKE_object_get_evaluated_mesh(ob);
  short type = (me != NULL) ? OB_MESH : ob->type;

  switch (type) {
    case OB_MESH:
      return DRW_mesh_material_count_get((me != NULL) ? me : ob->data);
    case OB_CURVE:
    case OB_SURF:
    case OB_FONT:
      return DRW_curve_material_count_get(ob->data);
    case OB_MBALL:
      return DRW_metaball_material_count_get(ob->data);
    case OB_HAIR:
      return DRW_hair_material_count_get(ob->data);
    case OB_POINTCLOUD:
      return DRW_pointcloud_material_count_get(ob->data);
    case OB_VOLUME:
      return DRW_volume_material_count_get(ob->data);
    default:
      BLI_assert(0);
      return 0;
  }
}

GPUBatch **DRW_cache_object_surface_material_get(struct Object *ob,
                                                 struct GPUMaterial **gpumat_array,
                                                 uint gpumat_array_len)
{
  switch (ob->type) {
    case OB_MESH:
      return DRW_cache_mesh_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
    case OB_CURVE:
      return DRW_cache_curve_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
    case OB_SURF:
      return DRW_cache_surf_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
    case OB_FONT:
      return DRW_cache_text_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
    case OB_MBALL:
      return DRW_cache_mball_surface_shaded_get(ob, gpumat_array, gpumat_array_len);
    case OB_HAIR:
      return NULL;
    case OB_POINTCLOUD:
      return NULL;
    case OB_VOLUME:
      return NULL;
    default:
      return NULL;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Empties
 * \{ */

GPUBatch *DRW_cache_plain_axes_get(void)
{
  if (!SHC.drw_plain_axes) {
    GPUVertFormat format = extra_vert_format();

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 6);

    int v = 0;
    int flag = VCLASS_EMPTY_SCALED;
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, -1.0f, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 1.0f, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{-1.0f, 0.0f, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{1.0f, 0.0f, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, -1.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 1.0f}, flag});

    SHC.drw_plain_axes = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_plain_axes;
}

GPUBatch *DRW_cache_empty_cube_get(void)
{
  if (!SHC.drw_empty_cube) {
    GPUVertFormat format = extra_vert_format();
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, ARRAY_SIZE(bone_box_wire));

    int v = 0;
    for (int i = 0; i < ARRAY_SIZE(bone_box_wire); i++) {
      float x = bone_box_verts[bone_box_wire[i]][0];
      float y = bone_box_verts[bone_box_wire[i]][1] * 2.0 - 1.0f;
      float z = bone_box_verts[bone_box_wire[i]][2];
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, z}, VCLASS_EMPTY_SCALED});
    }

    SHC.drw_empty_cube = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_empty_cube;
}

GPUBatch *DRW_cache_single_arrow_get(void)
{
  if (!SHC.drw_single_arrow) {
    GPUVertFormat format = extra_vert_format();
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 4 * 2 * 2 + 2);

    int v = 0;
    int flag = VCLASS_EMPTY_SCALED;
    float p[3][3] = {{0}};
    p[0][2] = 1.0f;
    p[1][0] = 0.035f;
    p[1][1] = 0.035f;
    p[2][0] = -0.035f;
    p[2][1] = 0.035f;
    p[1][2] = p[2][2] = 0.75f;
    for (int sides = 0; sides < 4; sides++) {
      if (sides % 2 == 1) {
        p[1][0] = -p[1][0];
        p[2][1] = -p[2][1];
      }
      else {
        p[1][1] = -p[1][1];
        p[2][0] = -p[2][0];
      }
      for (int i = 0, a = 1; i < 2; i++, a++) {
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[i][0], p[i][1], p[i][2]}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[a][0], p[a][1], p[a][2]}, flag});
      }
    }
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 0.0}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 0.75f}, flag});

    SHC.drw_single_arrow = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_single_arrow;
}

GPUBatch *DRW_cache_empty_sphere_get(void)
{
  if (!SHC.drw_empty_sphere) {
    GPUVertBuf *vbo = sphere_wire_vbo(1.0f, VCLASS_EMPTY_SCALED);
    SHC.drw_empty_sphere = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_empty_sphere;
}

GPUBatch *DRW_cache_empty_cone_get(void)
{
#define NSEGMENTS 8
  if (!SHC.drw_empty_cone) {
    GPUVertFormat format = extra_vert_format();
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 4);

    int v = 0;
    int flag = VCLASS_EMPTY_SCALED;
    /* a single ring of vertices */
    float p[NSEGMENTS][2];
    for (int i = 0; i < NSEGMENTS; i++) {
      float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
      p[i][0] = cosf(angle);
      p[i][1] = sinf(angle);
    }
    for (int i = 0; i < NSEGMENTS; i++) {
      float cv[2];
      cv[0] = p[(i) % NSEGMENTS][0];
      cv[1] = p[(i) % NSEGMENTS][1];

      /* cone sides */
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], 0.0f, cv[1]}, flag});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 2.0f, 0.0f}, flag});

      /* end ring */
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], 0.0f, cv[1]}, flag});
      cv[0] = p[(i + 1) % NSEGMENTS][0];
      cv[1] = p[(i + 1) % NSEGMENTS][1];
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], 0.0f, cv[1]}, flag});
    }

    SHC.drw_empty_cone = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_empty_cone;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_empty_cylinder_get(void)
{
#define NSEGMENTS 12
  if (!SHC.drw_empty_cylinder) {
    GPUVertFormat format = extra_vert_format();
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, NSEGMENTS * 6);

    /* a single ring of vertices */
    int v = 0;
    int flag = VCLASS_EMPTY_SCALED;
    float p[NSEGMENTS][2];
    for (int i = 0; i < NSEGMENTS; i++) {
      float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
      p[i][0] = cosf(angle);
      p[i][1] = sinf(angle);
    }
    for (int i = 0; i < NSEGMENTS; i++) {
      float cv[2], pv[2];
      cv[0] = p[(i) % NSEGMENTS][0];
      cv[1] = p[(i) % NSEGMENTS][1];
      pv[0] = p[(i + 1) % NSEGMENTS][0];
      pv[1] = p[(i + 1) % NSEGMENTS][1];

      /* cylinder sides */
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], cv[1], -1.0f}, flag});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], cv[1], 1.0f}, flag});
      /* top ring */
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], cv[1], 1.0f}, flag});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{pv[0], pv[1], 1.0f}, flag});
      /* bottom ring */
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{cv[0], cv[1], -1.0f}, flag});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{pv[0], pv[1], -1.0f}, flag});
    }

    SHC.drw_empty_cylinder = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_empty_cylinder;
#undef NSEGMENTS
}

GPUBatch *DRW_cache_empty_capsule_body_get(void)
{
  if (!SHC.drw_empty_capsule_body) {
    const float pos[8][3] = {
        {1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 0.0f},
        {-1.0f, 0.0f, 1.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, -1.0f, 1.0f},
        {0.0f, -1.0f, 0.0f},
    };

    /* Position Only 3D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 8);
    GPU_vertbuf_attr_fill(vbo, attr_id.pos, pos);

    SHC.drw_empty_capsule_body = GPU_batch_create_ex(
        GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_empty_capsule_body;
}

GPUBatch *DRW_cache_empty_capsule_cap_get(void)
{
#define NSEGMENTS 24 /* Must be multiple of 2. */
  if (!SHC.drw_empty_capsule_cap) {
    /* a single ring of vertices */
    float p[NSEGMENTS][2];
    for (int i = 0; i < NSEGMENTS; i++) {
      float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
      p[i][0] = cosf(angle);
      p[i][1] = sinf(angle);
    }

    /* Position Only 3D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, (NSEGMENTS * 2) * 2);

    /* Base circle */
    int vidx = 0;
    for (int i = 0; i < NSEGMENTS; i++) {
      float v[3] = {0.0f, 0.0f, 0.0f};
      copy_v2_v2(v, p[(i) % NSEGMENTS]);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
      copy_v2_v2(v, p[(i + 1) % NSEGMENTS]);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
    }

    for (int i = 0; i < NSEGMENTS / 2; i++) {
      float v[3] = {0.0f, 0.0f, 0.0f};
      int ci = i % NSEGMENTS;
      int pi = (i + 1) % NSEGMENTS;
      /* Y half circle */
      copy_v3_fl3(v, p[ci][0], 0.0f, p[ci][1]);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
      copy_v3_fl3(v, p[pi][0], 0.0f, p[pi][1]);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
      /* X half circle */
      copy_v3_fl3(v, 0.0f, p[ci][0], p[ci][1]);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
      copy_v3_fl3(v, 0.0f, p[pi][0], p[pi][1]);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
    }

    SHC.drw_empty_capsule_cap = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_empty_capsule_cap;
#undef NSEGMENTS
}

/* Force Field */
GPUBatch *DRW_cache_field_wind_get(void)
{
#define CIRCLE_RESOL 32
  if (!SHC.drw_field_wind) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (CIRCLE_RESOL * 4);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_EMPTY_SIZE;
    for (int i = 0; i < 4; i++) {
      float z = 0.05f * (float)i;
      circle_verts(vbo, &v, CIRCLE_RESOL, 1.0f, z, flag);
    }

    SHC.drw_field_wind = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_field_wind;
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_field_force_get(void)
{
#define CIRCLE_RESOL 32
  if (!SHC.drw_field_force) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (CIRCLE_RESOL * 3);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_EMPTY_SIZE | VCLASS_SCREENALIGNED;
    for (int i = 0; i < 3; i++) {
      float radius = 1.0f + 0.5f * i;
      circle_verts(vbo, &v, CIRCLE_RESOL, radius, 0.0f, flag);
    }

    SHC.drw_field_force = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_field_force;
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_field_vortex_get(void)
{
#define SPIRAL_RESOL 32
  if (!SHC.drw_field_vortex) {
    GPUVertFormat format = extra_vert_format();

    int v_len = SPIRAL_RESOL * 2 + 1;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_EMPTY_SIZE;
    for (int a = SPIRAL_RESOL; a > -1; a--) {
      float r = a / (float)SPIRAL_RESOL;
      float angle = (2.0f * M_PI * a) / SPIRAL_RESOL;
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{sinf(angle) * r, cosf(angle) * r, 0.0f}, flag});
    }
    for (int a = 1; a <= SPIRAL_RESOL; a++) {
      float r = a / (float)SPIRAL_RESOL;
      float angle = (2.0f * M_PI * a) / SPIRAL_RESOL;
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{sinf(angle) * -r, cosf(angle) * -r, 0.0f}, flag});
    }

    SHC.drw_field_vortex = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_field_vortex;
#undef SPIRAL_RESOL
}

/* Screenaligned circle. */
GPUBatch *DRW_cache_field_curve_get(void)
{
#define CIRCLE_RESOL 32
  if (!SHC.drw_field_curve) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (CIRCLE_RESOL);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_EMPTY_SIZE | VCLASS_SCREENALIGNED;
    circle_verts(vbo, &v, CIRCLE_RESOL, 1.0f, 0.0f, flag);

    SHC.drw_field_curve = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_field_curve;
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_field_tube_limit_get(void)
{
#define CIRCLE_RESOL 32
#define SIDE_STIPPLE 32
  if (!SHC.drw_field_tube_limit) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (CIRCLE_RESOL * 2 + 4 * SIDE_STIPPLE / 2);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_EMPTY_SIZE;
    /* Caps */
    for (int i = 0; i < 2; i++) {
      float z = i * 2.0f - 1.0f;
      circle_dashed_verts(vbo, &v, CIRCLE_RESOL, 1.0f, z, flag);
    }
    /* Side Edges */
    for (int a = 0; a < 4; a++) {
      float angle = (2.0f * M_PI * a) / 4.0f;
      for (int i = 0; i < SIDE_STIPPLE; i++) {
        float z = (i / (float)SIDE_STIPPLE) * 2.0f - 1.0f;
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{sinf(angle), cosf(angle), z}, flag});
      }
    }

    SHC.drw_field_tube_limit = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_field_tube_limit;
#undef SIDE_STIPPLE
#undef CIRCLE_RESOL
}

GPUBatch *DRW_cache_field_cone_limit_get(void)
{
#define CIRCLE_RESOL 32
#define SIDE_STIPPLE 32
  if (!SHC.drw_field_cone_limit) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (CIRCLE_RESOL * 2 + 4 * SIDE_STIPPLE / 2);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_EMPTY_SIZE;
    /* Caps */
    for (int i = 0; i < 2; i++) {
      float z = i * 2.0f - 1.0f;
      circle_dashed_verts(vbo, &v, CIRCLE_RESOL, 1.0f, z, flag);
    }
    /* Side Edges */
    for (int a = 0; a < 4; a++) {
      float angle = (2.0f * M_PI * a) / 4.0f;
      for (int i = 0; i < SIDE_STIPPLE; i++) {
        float z = (i / (float)SIDE_STIPPLE) * 2.0f - 1.0f;
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{sinf(angle) * z, cosf(angle) * z, z}, flag});
      }
    }

    SHC.drw_field_cone_limit = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_field_cone_limit;
#undef SIDE_STIPPLE
#undef CIRCLE_RESOL
}

/* Screenaligned dashed circle */
GPUBatch *DRW_cache_field_sphere_limit_get(void)
{
#define CIRCLE_RESOL 32
  if (!SHC.drw_field_sphere_limit) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * CIRCLE_RESOL;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_EMPTY_SIZE | VCLASS_SCREENALIGNED;
    circle_dashed_verts(vbo, &v, CIRCLE_RESOL, 1.0f, 0.0f, flag);

    SHC.drw_field_sphere_limit = GPU_batch_create_ex(
        GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_field_sphere_limit;
#undef CIRCLE_RESOL
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lights
 * \{ */

#define DIAMOND_NSEGMENTS 4
#define INNER_NSEGMENTS 8
#define OUTER_NSEGMENTS 10
#define CIRCLE_NSEGMENTS 32

static float light_distance_z_get(char axis, const bool start)
{
  switch (axis) {
    case 'x': /* - X */
      return start ? 0.4f : 0.3f;
    case 'X': /* + X */
      return start ? 0.6f : 0.7f;
    case 'y': /* - Y */
      return start ? 1.4f : 1.3f;
    case 'Y': /* + Y */
      return start ? 1.6f : 1.7f;
    case 'z': /* - Z */
      return start ? 2.4f : 2.3f;
    case 'Z': /* + Z */
      return start ? 2.6f : 2.7f;
  }
  return 0.0;
}

GPUBatch *DRW_cache_groundline_get(void)
{
  if (!SHC.drw_ground_line) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (1 + DIAMOND_NSEGMENTS);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    /* Ground Point */
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.35f, 0.0f, 0);
    /* Ground Line */
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, 1.0}, 0});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, 0.0}, 0});

    SHC.drw_ground_line = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_ground_line;
}

GPUBatch *DRW_cache_light_point_lines_get(void)
{
  if (!SHC.drw_light_point_lines) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (DIAMOND_NSEGMENTS + INNER_NSEGMENTS + OUTER_NSEGMENTS + CIRCLE_NSEGMENTS);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    const float r = 9.0f;
    int v = 0;
    /* Light Icon */
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, r * 0.3f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, INNER_NSEGMENTS, r * 1.0f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, OUTER_NSEGMENTS, r * 1.33f, 0.0f, VCLASS_SCREENSPACE);
    /* Light area */
    int flag = VCLASS_SCREENALIGNED | VCLASS_LIGHT_AREA_SHAPE;
    circle_verts(vbo, &v, CIRCLE_NSEGMENTS, 1.0f, 0.0f, flag);

    SHC.drw_light_point_lines = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_light_point_lines;
}

GPUBatch *DRW_cache_light_sun_lines_get(void)
{
  if (!SHC.drw_light_sun_lines) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (DIAMOND_NSEGMENTS + INNER_NSEGMENTS + OUTER_NSEGMENTS + 8 * 2 + 1);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    const float r = 9.0f;
    int v = 0;
    /* Light Icon */
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, r * 0.3f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, INNER_NSEGMENTS, r * 1.0f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, OUTER_NSEGMENTS, r * 1.33f, 0.0f, VCLASS_SCREENSPACE);
    /* Sun Rays */
    for (int a = 0; a < 8; a++) {
      float angle = (2.0f * M_PI * a) / 8.0f;
      float s = sinf(angle) * r;
      float c = cosf(angle) * r;
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{s * 1.6f, c * 1.6f, 0.0f}, VCLASS_SCREENSPACE});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{s * 1.9f, c * 1.9f, 0.0f}, VCLASS_SCREENSPACE});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{s * 2.2f, c * 2.2f, 0.0f}, VCLASS_SCREENSPACE});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{s * 2.5f, c * 2.5f, 0.0f}, VCLASS_SCREENSPACE});
    }
    /* Direction Line */
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, 0.0}, 0});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, -20.0}, 0}); /* Good default. */

    SHC.drw_light_sun_lines = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_light_sun_lines;
}

GPUBatch *DRW_cache_light_spot_lines_get(void)
{
  if (!SHC.drw_light_spot_lines) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * (DIAMOND_NSEGMENTS * 3 + INNER_NSEGMENTS + OUTER_NSEGMENTS +
                     CIRCLE_NSEGMENTS * 4 + 1);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    const float r = 9.0f;
    int v = 0;
    /* Light Icon */
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, r * 0.3f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, INNER_NSEGMENTS, r * 1.0f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, OUTER_NSEGMENTS, r * 1.33f, 0.0f, VCLASS_SCREENSPACE);
    /* Light area */
    int flag = VCLASS_SCREENALIGNED | VCLASS_LIGHT_AREA_SHAPE;
    circle_verts(vbo, &v, CIRCLE_NSEGMENTS, 1.0f, 0.0f, flag);
    /* Cone cap */
    flag = VCLASS_LIGHT_SPOT_SHAPE;
    circle_verts(vbo, &v, CIRCLE_NSEGMENTS, 1.0f, 0.0f, flag);
    flag = VCLASS_LIGHT_SPOT_SHAPE | VCLASS_LIGHT_SPOT_BLEND;
    circle_verts(vbo, &v, CIRCLE_NSEGMENTS, 1.0f, 0.0f, flag);
    /* Cone silhouette */
    flag = VCLASS_LIGHT_SPOT_SHAPE | VCLASS_LIGHT_SPOT_CONE;
    for (int a = 0; a < CIRCLE_NSEGMENTS; a++) {
      float angle = (2.0f * M_PI * a) / CIRCLE_NSEGMENTS;
      float s = sinf(angle);
      float c = cosf(angle);
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 0.0f}, 0});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{s, c, -1.0f}, flag});
    }
    /* Direction Line */
    float zsta = light_distance_z_get('z', true);
    float zend = light_distance_z_get('z', false);
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, zsta}, VCLASS_LIGHT_DIST});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, zend}, VCLASS_LIGHT_DIST});
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zsta, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zend, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);

    SHC.drw_light_spot_lines = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_light_spot_lines;
}

GPUBatch *DRW_cache_light_spot_volume_get(void)
{
  if (!SHC.drw_light_spot_volume) {
    GPUVertFormat format = extra_vert_format();

    int v_len = CIRCLE_NSEGMENTS + 1 + 1;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    /* Cone apex */
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 0.0f}, 0});
    /* Cone silhouette */
    int flag = VCLASS_LIGHT_SPOT_SHAPE;
    for (int a = 0; a < CIRCLE_NSEGMENTS + 1; a++) {
      float angle = (2.0f * M_PI * a) / CIRCLE_NSEGMENTS;
      float s = sinf(-angle);
      float c = cosf(-angle);
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{s, c, -1.0f}, flag});
    }

    SHC.drw_light_spot_volume = GPU_batch_create_ex(
        GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_light_spot_volume;
}

GPUBatch *DRW_cache_light_area_disk_lines_get(void)
{
  if (!SHC.drw_light_area_disk_lines) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 *
                (DIAMOND_NSEGMENTS * 3 + INNER_NSEGMENTS + OUTER_NSEGMENTS + CIRCLE_NSEGMENTS + 1);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    const float r = 9.0f;
    int v = 0;
    /* Light Icon */
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, r * 0.3f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, INNER_NSEGMENTS, r * 1.0f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, OUTER_NSEGMENTS, r * 1.33f, 0.0f, VCLASS_SCREENSPACE);
    /* Light area */
    circle_verts(vbo, &v, CIRCLE_NSEGMENTS, 0.5f, 0.0f, VCLASS_LIGHT_AREA_SHAPE);
    /* Direction Line */
    float zsta = light_distance_z_get('z', true);
    float zend = light_distance_z_get('z', false);
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, zsta}, VCLASS_LIGHT_DIST});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, zend}, VCLASS_LIGHT_DIST});
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zsta, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zend, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);

    SHC.drw_light_area_disk_lines = GPU_batch_create_ex(
        GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_light_area_disk_lines;
}

GPUBatch *DRW_cache_light_area_square_lines_get(void)
{
  if (!SHC.drw_light_area_square_lines) {
    GPUVertFormat format = extra_vert_format();

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    int v_len = 2 * (DIAMOND_NSEGMENTS * 3 + INNER_NSEGMENTS + OUTER_NSEGMENTS + 4 + 1);
    GPU_vertbuf_data_alloc(vbo, v_len);

    const float r = 9.0f;
    int v = 0;
    /* Light Icon */
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, r * 0.3f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, INNER_NSEGMENTS, r * 1.0f, 0.0f, VCLASS_SCREENSPACE);
    circle_dashed_verts(vbo, &v, OUTER_NSEGMENTS, r * 1.33f, 0.0f, VCLASS_SCREENSPACE);
    /* Light area */
    int flag = VCLASS_LIGHT_AREA_SHAPE;
    for (int a = 0; a < 4; a++) {
      for (int b = 0; b < 2; b++) {
        float p[4][2] = {{-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}};
        float x = p[(a + b) % 4][0];
        float y = p[(a + b) % 4][1];
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x * 0.5f, y * 0.5f, 0.0f}, flag});
      }
    }
    /* Direction Line */
    float zsta = light_distance_z_get('z', true);
    float zend = light_distance_z_get('z', false);
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, zsta}, VCLASS_LIGHT_DIST});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, zend}, VCLASS_LIGHT_DIST});
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zsta, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zend, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);

    SHC.drw_light_area_square_lines = GPU_batch_create_ex(
        GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_light_area_square_lines;
}

#undef CIRCLE_NSEGMENTS
#undef OUTER_NSEGMENTS
#undef INNER_NSEGMENTS

/** \} */

/* -------------------------------------------------------------------- */
/** \name Speaker
 * \{ */

GPUBatch *DRW_cache_speaker_get(void)
{
  if (!SHC.drw_speaker) {
    float v[3];
    const int segments = 16;
    int vidx = 0;

    /* Position Only 3D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 3 * segments * 2 + 4 * 4);

    for (int j = 0; j < 3; j++) {
      float z = 0.25f * j - 0.125f;
      float r = (j == 0 ? 0.5f : 0.25f);

      copy_v3_fl3(v, r, 0.0f, z);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
      for (int i = 1; i < segments; i++) {
        float x = cosf(2.f * (float)M_PI * i / segments) * r;
        float y = sinf(2.f * (float)M_PI * i / segments) * r;
        copy_v3_fl3(v, x, y, z);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
      }
      copy_v3_fl3(v, r, 0.0f, z);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
    }

    for (int j = 0; j < 4; j++) {
      float x = (((j + 1) % 2) * (j - 1)) * 0.5f;
      float y = ((j % 2) * (j - 2)) * 0.5f;
      for (int i = 0; i < 3; i++) {
        if (i == 1) {
          x *= 0.5f;
          y *= 0.5f;
        }

        float z = 0.25f * i - 0.125f;
        copy_v3_fl3(v, x, y, z);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
        if (i == 1) {
          GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx++, v);
        }
      }
    }

    SHC.drw_speaker = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_speaker;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Probe
 * \{ */

GPUBatch *DRW_cache_lightprobe_cube_get(void)
{
  if (!SHC.drw_lightprobe_cube) {
    GPUVertFormat format = extra_vert_format();

    int v_len = (6 + 3 + (1 + 2 * DIAMOND_NSEGMENTS) * 6) * 2;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    const float r = 14.0f;
    int v = 0;
    int flag = VCLASS_SCREENSPACE;
    /* Icon */
    const float sin_pi_3 = 0.86602540378f;
    const float cos_pi_3 = 0.5f;
    const float p[7][2] = {
        {0.0f, 1.0f},
        {sin_pi_3, cos_pi_3},
        {sin_pi_3, -cos_pi_3},
        {0.0f, -1.0f},
        {-sin_pi_3, -cos_pi_3},
        {-sin_pi_3, cos_pi_3},
        {0.0f, 0.0f},
    };
    for (int i = 0; i < 6; i++) {
      float t1[2], t2[2];
      copy_v2_v2(t1, p[i]);
      copy_v2_v2(t2, p[(i + 1) % 6]);
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{t1[0] * r, t1[1] * r, 0.0f}, flag});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{t2[0] * r, t2[1] * r, 0.0f}, flag});
    }
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[1][0] * r, p[1][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[6][0] * r, p[6][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[5][0] * r, p[5][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[6][0] * r, p[6][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[3][0] * r, p[3][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[6][0] * r, p[6][1] * r, 0.0f}, flag});
    /* Direction Lines */
    flag = VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE;
    for (int i = 0; i < 6; i++) {
      char axes[] = "zZyYxX";
      float zsta = light_distance_z_get(axes[i], true);
      float zend = light_distance_z_get(axes[i], false);
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, zsta}, flag});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, zend}, flag});
      circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zsta, flag);
      circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zend, flag);
    }

    SHC.drw_lightprobe_cube = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_lightprobe_cube;
}

GPUBatch *DRW_cache_lightprobe_grid_get(void)
{
  if (!SHC.drw_lightprobe_grid) {
    GPUVertFormat format = extra_vert_format();

    int v_len = (6 * 2 + 3 + (1 + 2 * DIAMOND_NSEGMENTS) * 6) * 2;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    const float r = 14.0f;
    int v = 0;
    int flag = VCLASS_SCREENSPACE;
    /* Icon */
    const float sin_pi_3 = 0.86602540378f;
    const float cos_pi_3 = 0.5f;
    const float p[7][2] = {
        {0.0f, 1.0f},
        {sin_pi_3, cos_pi_3},
        {sin_pi_3, -cos_pi_3},
        {0.0f, -1.0f},
        {-sin_pi_3, -cos_pi_3},
        {-sin_pi_3, cos_pi_3},
        {0.0f, 0.0f},
    };
    for (int i = 0; i < 6; i++) {
      float t1[2], t2[2], tr[2];
      copy_v2_v2(t1, p[i]);
      copy_v2_v2(t2, p[(i + 1) % 6]);
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{t1[0] * r, t1[1] * r, 0.0f}, flag});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{t2[0] * r, t2[1] * r, 0.0f}, flag});
      /* Internal wires. */
      for (int j = 1; j < 2; j++) {
        mul_v2_v2fl(tr, p[(i / 2) * 2 + 1], -0.5f * j);
        add_v2_v2v2(t1, p[i], tr);
        add_v2_v2v2(t2, p[(i + 1) % 6], tr);
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{t1[0] * r, t1[1] * r, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{t2[0] * r, t2[1] * r, 0.0f}, flag});
      }
    }
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[1][0] * r, p[1][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[6][0] * r, p[6][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[5][0] * r, p[5][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[6][0] * r, p[6][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[3][0] * r, p[3][1] * r, 0.0f}, flag});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[6][0] * r, p[6][1] * r, 0.0f}, flag});
    /* Direction Lines */
    flag = VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE;
    for (int i = 0; i < 6; i++) {
      char axes[] = "zZyYxX";
      float zsta = light_distance_z_get(axes[i], true);
      float zend = light_distance_z_get(axes[i], false);
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, zsta}, flag});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, zend}, flag});
      circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zsta, flag);
      circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.2f, zend, flag);
    }

    SHC.drw_lightprobe_grid = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_lightprobe_grid;
}

GPUBatch *DRW_cache_lightprobe_planar_get(void)
{
  if (!SHC.drw_lightprobe_planar) {
    GPUVertFormat format = extra_vert_format();

    int v_len = 2 * 4;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    const float r = 20.0f;
    int v = 0;
    /* Icon */
    const float sin_pi_3 = 0.86602540378f;
    const float p[4][2] = {
        {0.0f, 0.5f},
        {sin_pi_3, 0.0f},
        {0.0f, -0.5f},
        {-sin_pi_3, 0.0f},
    };
    for (int i = 0; i < 4; i++) {
      for (int a = 0; a < 2; a++) {
        float x = p[(i + a) % 4][0] * r;
        float y = p[(i + a) % 4][1] * r;
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, 0.0}, VCLASS_SCREENSPACE});
      }
    }

    SHC.drw_lightprobe_planar = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_lightprobe_planar;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Bones
 * \{ */

static const float bone_octahedral_verts[6][3] = {
    {0.0f, 0.0f, 0.0f},
    {0.1f, 0.1f, 0.1f},
    {0.1f, 0.1f, -0.1f},
    {-0.1f, 0.1f, -0.1f},
    {-0.1f, 0.1f, 0.1f},
    {0.0f, 1.0f, 0.0f},
};

static const float bone_octahedral_smooth_normals[6][3] = {
    {0.0f, -1.0f, 0.0f},
#if 0 /* creates problems for outlines when scaled */
    {0.943608f * M_SQRT1_2, -0.331048f, 0.943608f * M_SQRT1_2},
    {0.943608f * M_SQRT1_2, -0.331048f, -0.943608f * M_SQRT1_2},
    {-0.943608f * M_SQRT1_2, -0.331048f, -0.943608f * M_SQRT1_2},
    {-0.943608f * M_SQRT1_2, -0.331048f, 0.943608f * M_SQRT1_2},
#else
    {M_SQRT1_2, 0.0f, M_SQRT1_2},
    {M_SQRT1_2, 0.0f, -M_SQRT1_2},
    {-M_SQRT1_2, 0.0f, -M_SQRT1_2},
    {-M_SQRT1_2, 0.0f, M_SQRT1_2},
#endif
    {0.0f, 1.0f, 0.0f},
};

#if 0 /* UNUSED */

static const uint bone_octahedral_wire[24] = {
    0, 1, 1, 5, 5, 3, 3, 0, 0, 4, 4, 5, 5, 2, 2, 0, 1, 2, 2, 3, 3, 4, 4, 1,
};

/* aligned with bone_octahedral_wire
 * Contains adjacent normal index */
static const uint bone_octahedral_wire_adjacent_face[24] = {
    0, 3, 4, 7, 5, 6, 1, 2, 2, 3, 6, 7, 4, 5, 0, 1, 0, 4, 1, 5, 2, 6, 3, 7,
};
#endif

static const uint bone_octahedral_solid_tris[8][3] = {
    {2, 1, 0}, /* bottom */
    {3, 2, 0},
    {4, 3, 0},
    {1, 4, 0},

    {5, 1, 2}, /* top */
    {5, 2, 3},
    {5, 3, 4},
    {5, 4, 1},
};

/**
 * Store indices of generated verts from bone_octahedral_solid_tris to define adjacency infos.
 * Example: triangle {2, 1, 0} is adjacent to {3, 2, 0}, {1, 4, 0} and {5, 1, 2}.
 * {2, 1, 0} becomes {0, 1, 2}
 * {3, 2, 0} becomes {3, 4, 5}
 * {1, 4, 0} becomes {9, 10, 11}
 * {5, 1, 2} becomes {12, 13, 14}
 * According to opengl specification it becomes (starting from
 * the first vertex of the first face aka. vertex 2):
 * {0, 12, 1, 10, 2, 3}
 */
static const uint bone_octahedral_wire_lines_adjacency[12][4] = {
    {0, 1, 2, 6},
    {0, 12, 1, 6},
    {0, 3, 12, 6},
    {0, 2, 3, 6},
    {1, 6, 2, 3},
    {1, 12, 6, 3},
    {1, 0, 12, 3},
    {1, 2, 0, 3},
    {2, 0, 1, 12},
    {2, 3, 0, 12},
    {2, 6, 3, 12},
    {2, 1, 6, 12},
};

#if 0 /* UNUSED */
static const uint bone_octahedral_solid_tris_adjacency[8][6] = {
    {0, 12, 1, 10, 2, 3},
    {3, 15, 4, 1, 5, 6},
    {6, 18, 7, 4, 8, 9},
    {9, 21, 10, 7, 11, 0},

    {12, 22, 13, 2, 14, 17},
    {15, 13, 16, 5, 17, 20},
    {18, 16, 19, 8, 20, 23},
    {21, 19, 22, 11, 23, 14},
};
#endif

/* aligned with bone_octahedral_solid_tris */
static const float bone_octahedral_solid_normals[8][3] = {
    {M_SQRT1_2, -M_SQRT1_2, 0.00000000f},
    {-0.00000000f, -M_SQRT1_2, -M_SQRT1_2},
    {-M_SQRT1_2, -M_SQRT1_2, 0.00000000f},
    {0.00000000f, -M_SQRT1_2, M_SQRT1_2},
    {0.99388373f, 0.11043154f, -0.00000000f},
    {0.00000000f, 0.11043154f, -0.99388373f},
    {-0.99388373f, 0.11043154f, 0.00000000f},
    {0.00000000f, 0.11043154f, 0.99388373f},
};

GPUBatch *DRW_cache_bone_octahedral_get(void)
{
  if (!SHC.drw_bone_octahedral) {
    uint v_idx = 0;

    static GPUVertFormat format = {0};
    static struct {
      uint pos, nor, snor;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
      attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
      attr_id.snor = GPU_vertformat_attr_add(&format, "snor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }

    /* Vertices */
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 24);

    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 3; j++) {
        GPU_vertbuf_attr_set(vbo, attr_id.nor, v_idx, bone_octahedral_solid_normals[i]);
        GPU_vertbuf_attr_set(vbo,
                             attr_id.snor,
                             v_idx,
                             bone_octahedral_smooth_normals[bone_octahedral_solid_tris[i][j]]);
        GPU_vertbuf_attr_set(
            vbo, attr_id.pos, v_idx++, bone_octahedral_verts[bone_octahedral_solid_tris[i][j]]);
      }
    }

    SHC.drw_bone_octahedral = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_bone_octahedral;
}

GPUBatch *DRW_cache_bone_octahedral_wire_get(void)
{
  if (!SHC.drw_bone_octahedral_wire) {
    GPUIndexBufBuilder elb;
    GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, 12, 24);

    for (int i = 0; i < 12; i++) {
      GPU_indexbuf_add_line_adj_verts(&elb,
                                      bone_octahedral_wire_lines_adjacency[i][0],
                                      bone_octahedral_wire_lines_adjacency[i][1],
                                      bone_octahedral_wire_lines_adjacency[i][2],
                                      bone_octahedral_wire_lines_adjacency[i][3]);
    }

    /* HACK Reuse vertex buffer. */
    GPUBatch *pos_nor_batch = DRW_cache_bone_octahedral_get();

    SHC.drw_bone_octahedral_wire = GPU_batch_create_ex(GPU_PRIM_LINES_ADJ,
                                                       pos_nor_batch->verts[0],
                                                       GPU_indexbuf_build(&elb),
                                                       GPU_BATCH_OWNS_INDEX);
  }
  return SHC.drw_bone_octahedral_wire;
}

GPUBatch *DRW_cache_bone_box_get(void)
{
  if (!SHC.drw_bone_box) {
    uint v_idx = 0;

    static GPUVertFormat format = {0};
    static struct {
      uint pos, nor, snor;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
      attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
      attr_id.snor = GPU_vertformat_attr_add(&format, "snor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }

    /* Vertices */
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 36);

    for (int i = 0; i < 12; i++) {
      for (int j = 0; j < 3; j++) {
        GPU_vertbuf_attr_set(vbo, attr_id.nor, v_idx, bone_box_solid_normals[i]);
        GPU_vertbuf_attr_set(
            vbo, attr_id.snor, v_idx, bone_box_smooth_normals[bone_box_solid_tris[i][j]]);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, bone_box_verts[bone_box_solid_tris[i][j]]);
      }
    }

    SHC.drw_bone_box = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_bone_box;
}

GPUBatch *DRW_cache_bone_box_wire_get(void)
{
  if (!SHC.drw_bone_box_wire) {
    GPUIndexBufBuilder elb;
    GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, 12, 36);

    for (int i = 0; i < 12; i++) {
      GPU_indexbuf_add_line_adj_verts(&elb,
                                      bone_box_wire_lines_adjacency[i][0],
                                      bone_box_wire_lines_adjacency[i][1],
                                      bone_box_wire_lines_adjacency[i][2],
                                      bone_box_wire_lines_adjacency[i][3]);
    }

    /* HACK Reuse vertex buffer. */
    GPUBatch *pos_nor_batch = DRW_cache_bone_box_get();

    SHC.drw_bone_box_wire = GPU_batch_create_ex(GPU_PRIM_LINES_ADJ,
                                                pos_nor_batch->verts[0],
                                                GPU_indexbuf_build(&elb),
                                                GPU_BATCH_OWNS_INDEX);
  }
  return SHC.drw_bone_box_wire;
}

/* Helpers for envelope bone's solid sphere-with-hidden-equatorial-cylinder.
 * Note that here we only encode head/tail in forth component of the vector. */
static void benv_lat_lon_to_co(const float lat, const float lon, float r_nor[3])
{
  r_nor[0] = sinf(lat) * cosf(lon);
  r_nor[1] = sinf(lat) * sinf(lon);
  r_nor[2] = cosf(lat);
}

GPUBatch *DRW_cache_bone_envelope_solid_get(void)
{
  if (!SHC.drw_bone_envelope) {
    const int lon_res = 24;
    const int lat_res = 24;
    const float lon_inc = 2.0f * M_PI / lon_res;
    const float lat_inc = M_PI / lat_res;
    uint v_idx = 0;

    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }

    /* Vertices */
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, ((lat_res + 1) * 2) * lon_res * 1);

    float lon = 0.0f;
    for (int i = 0; i < lon_res; i++, lon += lon_inc) {
      float lat = 0.0f;
      float co1[3], co2[3];

      /* Note: the poles are duplicated on purpose, to restart the strip. */

      /* 1st sphere */
      for (int j = 0; j < lat_res; j++, lat += lat_inc) {
        benv_lat_lon_to_co(lat, lon, co1);
        benv_lat_lon_to_co(lat, lon + lon_inc, co2);

        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, co1);
        GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, co2);
      }

      /* Closing the loop */
      benv_lat_lon_to_co(M_PI, lon, co1);
      benv_lat_lon_to_co(M_PI, lon + lon_inc, co2);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, co1);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v_idx++, co2);
    }

    SHC.drw_bone_envelope = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_bone_envelope;
}

GPUBatch *DRW_cache_bone_envelope_outline_get(void)
{
  if (!SHC.drw_bone_envelope_outline) {
#define CIRCLE_RESOL 64
    float v0[2], v1[2], v2[2];
    const float radius = 1.0f;

    /* Position Only 2D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos0, pos1, pos2;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos0 = GPU_vertformat_attr_add(&format, "pos0", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      attr_id.pos1 = GPU_vertformat_attr_add(&format, "pos1", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      attr_id.pos2 = GPU_vertformat_attr_add(&format, "pos2", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL + 1);

    v0[0] = radius * sinf((2.0f * M_PI * -2) / ((float)CIRCLE_RESOL));
    v0[1] = radius * cosf((2.0f * M_PI * -2) / ((float)CIRCLE_RESOL));
    v1[0] = radius * sinf((2.0f * M_PI * -1) / ((float)CIRCLE_RESOL));
    v1[1] = radius * cosf((2.0f * M_PI * -1) / ((float)CIRCLE_RESOL));

    /* Output 4 verts for each position. See shader for explanation. */
    uint v = 0;
    for (int a = 0; a <= CIRCLE_RESOL; a++) {
      v2[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
      v2[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
      GPU_vertbuf_attr_set(vbo, attr_id.pos0, v, v0);
      GPU_vertbuf_attr_set(vbo, attr_id.pos1, v, v1);
      GPU_vertbuf_attr_set(vbo, attr_id.pos2, v++, v2);
      copy_v2_v2(v0, v1);
      copy_v2_v2(v1, v2);
    }

    SHC.drw_bone_envelope_outline = GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
#undef CIRCLE_RESOL
  }
  return SHC.drw_bone_envelope_outline;
}

GPUBatch *DRW_cache_bone_point_get(void)
{
  if (!SHC.drw_bone_point) {
#if 0 /* old style geometry sphere */
    const int lon_res = 16;
    const int lat_res = 8;
    const float rad = 0.05f;
    const float lon_inc = 2 * M_PI / lon_res;
    const float lat_inc = M_PI / lat_res;
    uint v_idx = 0;

    static GPUVertFormat format = {0};
    static struct {
      uint pos, nor;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
      attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    }

    /* Vertices */
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, (lat_res - 1) * lon_res * 6);

    float lon = 0.0f;
    for (int i = 0; i < lon_res; i++, lon += lon_inc) {
      float lat = 0.0f;
      for (int j = 0; j < lat_res; j++, lat += lat_inc) {
        if (j != lat_res - 1) { /* Pole */
          add_lat_lon_vert(
              vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat + lat_inc, lon + lon_inc);
          add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat + lat_inc, lon);
          add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat, lon);
        }

        if (j != 0) { /* Pole */
          add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat, lon + lon_inc);
          add_lat_lon_vert(
              vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat + lat_inc, lon + lon_inc);
          add_lat_lon_vert(vbo, attr_id.pos, attr_id.nor, &v_idx, rad, lat, lon);
        }
      }
    }

    SHC.drw_bone_point = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
#else
#  define CIRCLE_RESOL 64
    float v[2];
    const float radius = 0.05f;

    /* Position Only 2D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL);

    for (int a = 0; a < CIRCLE_RESOL; a++) {
      v[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
      v[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
      GPU_vertbuf_attr_set(vbo, attr_id.pos, a, v);
    }

    SHC.drw_bone_point = GPU_batch_create_ex(GPU_PRIM_TRI_FAN, vbo, NULL, GPU_BATCH_OWNS_VBO);
#  undef CIRCLE_RESOL
#endif
  }
  return SHC.drw_bone_point;
}

GPUBatch *DRW_cache_bone_point_wire_outline_get(void)
{
  if (!SHC.drw_bone_point_wire) {
#if 0 /* old style geometry sphere */
    GPUVertBuf *vbo = sphere_wire_vbo(0.05f);
    SHC.drw_bone_point_wire = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
#else
#  define CIRCLE_RESOL 64
    const float radius = 0.05f;

    /* Position Only 2D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL + 1);

    uint v = 0;
    for (int a = 0; a <= CIRCLE_RESOL; a++) {
      float pos[2];
      pos[0] = radius * sinf((2.0f * M_PI * a) / CIRCLE_RESOL);
      pos[1] = radius * cosf((2.0f * M_PI * a) / CIRCLE_RESOL);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v++, pos);
    }

    SHC.drw_bone_point_wire = GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
#  undef CIRCLE_RESOL
#endif
  }
  return SHC.drw_bone_point_wire;
}

/* keep in sync with armature_stick_vert.glsl */
#define COL_WIRE (1 << 0)
#define COL_HEAD (1 << 1)
#define COL_TAIL (1 << 2)
#define COL_BONE (1 << 3)

#define POS_HEAD (1 << 4)
#define POS_TAIL (1 << 5)
#define POS_BONE (1 << 6)

GPUBatch *DRW_cache_bone_stick_get(void)
{
  if (!SHC.drw_bone_stick) {
#define CIRCLE_RESOL 12
    uint v = 0;
    uint flag;
    const float radius = 2.0f; /* head/tail radius */
    float pos[2];

    /* Position Only 2D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos, flag;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      attr_id.flag = GPU_vertformat_attr_add(&format, "flag", GPU_COMP_U32, 1, GPU_FETCH_INT);
    }

    const uint vcount = (CIRCLE_RESOL + 1) * 2 + 6;

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, vcount);

    GPUIndexBufBuilder elb;
    GPU_indexbuf_init_ex(&elb, GPU_PRIM_TRI_FAN, (CIRCLE_RESOL + 2) * 2 + 6 + 2, vcount);

    /* head/tail points */
    for (int i = 0; i < 2; i++) {
      /* center vertex */
      copy_v2_fl(pos, 0.0f);
      flag = (i == 0) ? POS_HEAD : POS_TAIL;
      flag |= (i == 0) ? COL_HEAD : COL_TAIL;
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, pos);
      GPU_vertbuf_attr_set(vbo, attr_id.flag, v, &flag);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      /* circle vertices */
      flag |= COL_WIRE;
      for (int a = 0; a < CIRCLE_RESOL; a++) {
        pos[0] = radius * sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
        pos[1] = radius * cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
        GPU_vertbuf_attr_set(vbo, attr_id.pos, v, pos);
        GPU_vertbuf_attr_set(vbo, attr_id.flag, v, &flag);
        GPU_indexbuf_add_generic_vert(&elb, v++);
      }
      /* Close the circle */
      GPU_indexbuf_add_generic_vert(&elb, v - CIRCLE_RESOL);

      GPU_indexbuf_add_primitive_restart(&elb);
    }

    /* Bone rectangle */
    pos[0] = 0.0f;
    for (int i = 0; i < 6; i++) {
      pos[1] = (i == 0 || i == 3) ? 0.0f : ((i < 3) ? 1.0f : -1.0f);
      flag = ((i < 2 || i > 4) ? POS_HEAD : POS_TAIL) | ((i == 0 || i == 3) ? 0 : COL_WIRE) |
             COL_BONE | POS_BONE;
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, pos);
      GPU_vertbuf_attr_set(vbo, attr_id.flag, v, &flag);
      GPU_indexbuf_add_generic_vert(&elb, v++);
    }

    SHC.drw_bone_stick = GPU_batch_create_ex(GPU_PRIM_TRI_FAN,
                                             vbo,
                                             GPU_indexbuf_build(&elb),
                                             GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
#undef CIRCLE_RESOL
  }
  return SHC.drw_bone_stick;
}

#define S_X 0.0215f
#define S_Y 0.025f
static float x_axis_name[4][2] = {
    {0.9f * S_X, 1.0f * S_Y},
    {-1.0f * S_X, -1.0f * S_Y},
    {-0.9f * S_X, 1.0f * S_Y},
    {1.0f * S_X, -1.0f * S_Y},
};
#define X_LEN (sizeof(x_axis_name) / (sizeof(float) * 2))
#undef S_X
#undef S_Y

#define S_X 0.0175f
#define S_Y 0.025f
static float y_axis_name[6][2] = {
    {-1.0f * S_X, 1.0f * S_Y},
    {0.0f * S_X, -0.1f * S_Y},
    {1.0f * S_X, 1.0f * S_Y},
    {0.0f * S_X, -0.1f * S_Y},
    {0.0f * S_X, -0.1f * S_Y},
    {0.0f * S_X, -1.0f * S_Y},
};
#define Y_LEN (sizeof(y_axis_name) / (sizeof(float) * 2))
#undef S_X
#undef S_Y

#define S_X 0.02f
#define S_Y 0.025f
static float z_axis_name[10][2] = {
    {-0.95f * S_X, 1.00f * S_Y},
    {0.95f * S_X, 1.00f * S_Y},
    {0.95f * S_X, 1.00f * S_Y},
    {0.95f * S_X, 0.90f * S_Y},
    {0.95f * S_X, 0.90f * S_Y},
    {-1.00f * S_X, -0.90f * S_Y},
    {-1.00f * S_X, -0.90f * S_Y},
    {-1.00f * S_X, -1.00f * S_Y},
    {-1.00f * S_X, -1.00f * S_Y},
    {1.00f * S_X, -1.00f * S_Y},
};
#define Z_LEN (sizeof(z_axis_name) / (sizeof(float) * 2))
#undef S_X
#undef S_Y

#define S_X 0.007f
#define S_Y 0.007f
static float axis_marker[8][2] = {
#if 0 /* square */
    {-1.0f * S_X, 1.0f * S_Y},
    {1.0f * S_X, 1.0f * S_Y},
    {1.0f * S_X, 1.0f * S_Y},
    {1.0f * S_X, -1.0f * S_Y},
    {1.0f * S_X, -1.0f * S_Y},
    {-1.0f * S_X, -1.0f * S_Y},
    {-1.0f * S_X, -1.0f * S_Y},
    {-1.0f * S_X, 1.0f * S_Y}
#else /* diamond */
    {-S_X, 0.f},
    {0.f, S_Y},
    {0.f, S_Y},
    {S_X, 0.f},
    {S_X, 0.f},
    {0.f, -S_Y},
    {0.f, -S_Y},
    {-S_X, 0.f}
#endif
};
#define MARKER_LEN (sizeof(axis_marker) / (sizeof(float) * 2))
#define MARKER_FILL_LAYER 6
#undef S_X
#undef S_Y

GPUBatch *DRW_cache_bone_arrows_get(void)
{
  if (!SHC.drw_bone_arrows) {
    GPUVertFormat format = extra_vert_format();
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    int v_len = (2 + MARKER_LEN * MARKER_FILL_LAYER) * 3 + (X_LEN + Y_LEN + Z_LEN);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    for (int axis = 0; axis < 3; axis++) {
      int flag = VCLASS_EMPTY_AXES | VCLASS_SCREENALIGNED;
      /* Vertex layout is XY screen position and axis in Z.
       * Fractional part of Z is a positive offset at axis unit position.*/
      float p[3] = {0.0f, 0.0f, axis};
      /* center to axis line */
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 0.0f}, 0});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[0], p[1], p[2]}, flag});
      /* Axis end marker */
      for (int j = 1; j < MARKER_FILL_LAYER + 1; j++) {
        for (int i = 0; i < MARKER_LEN; i++) {
          mul_v2_v2fl(p, axis_marker[i], 4.0f * j / (float)MARKER_FILL_LAYER);
          GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[0], p[1], p[2]}, flag});
        }
      }
      /* Axis name */
      flag = VCLASS_EMPTY_AXES | VCLASS_EMPTY_AXES_NAME | VCLASS_SCREENALIGNED;
      int axis_v_len[] = {X_LEN, Y_LEN, Z_LEN};
      float(*axis_v)[2] = (axis == 0) ? x_axis_name : ((axis == 1) ? y_axis_name : z_axis_name);
      p[2] = axis + 0.25f;
      for (int i = 0; i < axis_v_len[axis]; i++) {
        mul_v2_v2fl(p, axis_v[i], 4.0f);
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{p[0], p[1], p[2]}, flag});
      }
    }

    SHC.drw_bone_arrows = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_bone_arrows;
}

static const float staticSine[16] = {
    0.0f,
    0.104528463268f,
    0.207911690818f,
    0.309016994375f,
    0.406736643076f,
    0.5f,
    0.587785252292f,
    0.669130606359f,
    0.743144825477f,
    0.809016994375f,
    0.866025403784f,
    0.913545457643f,
    0.951056516295f,
    0.978147600734f,
    0.994521895368f,
    1.0f,
};

#define set_vert(a, b, quarter) \
  { \
    copy_v2_fl2(pos, (quarter % 2 == 0) ? -(a) : (a), (quarter < 2) ? -(b) : (b)); \
    GPU_vertbuf_attr_set(vbo, attr_id.pos, v++, pos); \
  } \
  ((void)0)

GPUBatch *DRW_cache_bone_dof_sphere_get(void)
{
  if (!SHC.drw_bone_dof_sphere) {
    int i, j, q, n = ARRAY_SIZE(staticSine);
    float x, z, px, pz, pos[2];

    /* Position Only 3D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, n * n * 6 * 4);

    uint v = 0;
    for (q = 0; q < 4; q++) {
      pz = 0.0f;
      for (i = 1; i < n; i++) {
        z = staticSine[i];
        px = 0.0f;
        for (j = 1; j <= (n - i); j++) {
          x = staticSine[j];
          if (j == n - i) {
            set_vert(px, z, q);
            set_vert(px, pz, q);
            set_vert(x, pz, q);
          }
          else {
            set_vert(x, z, q);
            set_vert(x, pz, q);
            set_vert(px, z, q);

            set_vert(x, pz, q);
            set_vert(px, pz, q);
            set_vert(px, z, q);
          }
          px = x;
        }
        pz = z;
      }
    }
    /* TODO allocate right count from the beginning. */
    GPU_vertbuf_data_resize(vbo, v);

    SHC.drw_bone_dof_sphere = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_bone_dof_sphere;
}

GPUBatch *DRW_cache_bone_dof_lines_get(void)
{
  if (!SHC.drw_bone_dof_lines) {
    int i, n = ARRAY_SIZE(staticSine);
    float pos[2];

    /* Position Only 3D format */
    static GPUVertFormat format = {0};
    static struct {
      uint pos;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    }

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, n * 4);

    uint v = 0;
    for (i = 0; i < n * 4; i++) {
      float a = (1.0f - (i / (float)(n * 4))) * 2.0f * M_PI;
      float x = cosf(a);
      float y = sinf(a);
      set_vert(x, y, 0);
    }

    SHC.drw_bone_dof_lines = GPU_batch_create_ex(
        GPU_PRIM_LINE_LOOP, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_bone_dof_lines;
}

#undef set_vert

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera
 * \{ */

GPUBatch *DRW_cache_camera_frame_get(void)
{
  if (!SHC.drw_camera_frame) {
    GPUVertFormat format = extra_vert_format();

    const int v_len = 2 * (4 + 4);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    float p[4][2] = {{-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}};
    /* Frame */
    for (int a = 0; a < 4; a++) {
      for (int b = 0; b < 2; b++) {
        float x = p[(a + b) % 4][0];
        float y = p[(a + b) % 4][1];
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, 1.0f}, VCLASS_CAMERA_FRAME});
      }
    }
    /* Wires to origin. */
    for (int a = 0; a < 4; a++) {
      float x = p[a][0];
      float y = p[a][1];
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, 1.0f}, VCLASS_CAMERA_FRAME});
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, 0.0f}, VCLASS_CAMERA_FRAME});
    }

    SHC.drw_camera_frame = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_camera_frame;
}

GPUBatch *DRW_cache_camera_volume_get(void)
{
  if (!SHC.drw_camera_volume) {
    GPUVertFormat format = extra_vert_format();

    const int v_len = ARRAY_SIZE(bone_box_solid_tris) * 3;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_CAMERA_FRAME | VCLASS_CAMERA_VOLUME;
    for (int i = 0; i < ARRAY_SIZE(bone_box_solid_tris); i++) {
      for (int a = 0; a < 3; a++) {
        float x = bone_box_verts[bone_box_solid_tris[i][a]][2];
        float y = bone_box_verts[bone_box_solid_tris[i][a]][0];
        float z = bone_box_verts[bone_box_solid_tris[i][a]][1];
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, z}, flag});
      }
    }

    SHC.drw_camera_volume = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_camera_volume;
}

GPUBatch *DRW_cache_camera_volume_wire_get(void)
{
  if (!SHC.drw_camera_volume_wire) {
    GPUVertFormat format = extra_vert_format();

    const int v_len = ARRAY_SIZE(bone_box_wire);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    int flag = VCLASS_CAMERA_FRAME | VCLASS_CAMERA_VOLUME;
    for (int i = 0; i < ARRAY_SIZE(bone_box_wire); i++) {
      float x = bone_box_verts[bone_box_wire[i]][2];
      float y = bone_box_verts[bone_box_wire[i]][0];
      float z = bone_box_verts[bone_box_wire[i]][1];
      GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, z}, flag});
    }

    SHC.drw_camera_volume_wire = GPU_batch_create_ex(
        GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_camera_volume_wire;
}

GPUBatch *DRW_cache_camera_tria_wire_get(void)
{
  if (!SHC.drw_camera_tria_wire) {
    GPUVertFormat format = extra_vert_format();

    const int v_len = 2 * 3;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    float p[3][2] = {{-1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}};
    for (int a = 0; a < 3; a++) {
      for (int b = 0; b < 2; b++) {
        float x = p[(a + b) % 3][0];
        float y = p[(a + b) % 3][1];
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, 1.0f}, VCLASS_CAMERA_FRAME});
      }
    }

    SHC.drw_camera_tria_wire = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_camera_tria_wire;
}

GPUBatch *DRW_cache_camera_tria_get(void)
{
  if (!SHC.drw_camera_tria) {
    GPUVertFormat format = extra_vert_format();

    const int v_len = 3;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    /* Use camera frame position */
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{-1.0f, 1.0f, 1.0f}, VCLASS_CAMERA_FRAME});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{1.0f, 1.0f, 1.0f}, VCLASS_CAMERA_FRAME});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 1.0f}, VCLASS_CAMERA_FRAME});

    SHC.drw_camera_tria = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_camera_tria;
}

GPUBatch *DRW_cache_camera_distances_get(void)
{
  if (!SHC.drw_camera_distances) {
    GPUVertFormat format = extra_vert_format();

    const int v_len = 2 * (1 + DIAMOND_NSEGMENTS * 2 + 2);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, v_len);

    int v = 0;
    /* Direction Line */
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, 0.0}, VCLASS_CAMERA_DIST});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 0.0, 1.0}, VCLASS_CAMERA_DIST});
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.5f, 0.0f, VCLASS_CAMERA_DIST | VCLASS_SCREENSPACE);
    circle_verts(vbo, &v, DIAMOND_NSEGMENTS, 1.5f, 1.0f, VCLASS_CAMERA_DIST | VCLASS_SCREENSPACE);
    /* Focus cross */
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{1.0, 0.0, 2.0}, VCLASS_CAMERA_DIST});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{-1.0, 0.0, 2.0}, VCLASS_CAMERA_DIST});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, 1.0, 2.0}, VCLASS_CAMERA_DIST});
    GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0, -1.0, 2.0}, VCLASS_CAMERA_DIST});

    SHC.drw_camera_distances = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }
  return SHC.drw_camera_distances;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Meshes
 * \{ */

GPUBatch *DRW_cache_mesh_all_verts_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_all_verts(ob->data);
}

GPUBatch *DRW_cache_mesh_all_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_all_edges(ob->data);
}

GPUBatch *DRW_cache_mesh_loose_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_loose_edges(ob->data);
}

GPUBatch *DRW_cache_mesh_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_edge_detection(ob->data, r_is_manifold);
}

GPUBatch *DRW_cache_mesh_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface(ob->data);
}

GPUBatch *DRW_cache_mesh_surface_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_edges(ob->data);
}

/* Return list of batches with length equal to max(1, totcol). */
GPUBatch **DRW_cache_mesh_surface_shaded_get(Object *ob,
                                             struct GPUMaterial **gpumat_array,
                                             uint gpumat_array_len)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_shaded(ob->data, gpumat_array, gpumat_array_len);
}

/* Return list of batches with length equal to max(1, totcol). */
GPUBatch **DRW_cache_mesh_surface_texpaint_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_texpaint(ob->data);
}

GPUBatch *DRW_cache_mesh_surface_texpaint_single_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_texpaint_single(ob->data);
}

GPUBatch *DRW_cache_mesh_surface_vertpaint_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_vertpaint(ob->data);
}

GPUBatch *DRW_cache_mesh_surface_weights_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_surface_weights(ob->data);
}

GPUBatch *DRW_cache_mesh_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_wireframes_face(ob->data);
}

GPUBatch *DRW_cache_mesh_surface_mesh_analysis_get(Object *ob)
{
  BLI_assert(ob->type == OB_MESH);
  return DRW_mesh_batch_cache_get_edit_mesh_analysis(ob->data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve
 * \{ */

GPUBatch *DRW_cache_curve_edge_wire_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVE);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_loose_edges(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_wire_edge(cu);
  }
}

GPUBatch *DRW_cache_curve_edge_normal_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVE);

  struct Curve *cu = ob->data;
  return DRW_curve_batch_cache_get_normal_edge(cu);
}

GPUBatch *DRW_cache_curve_edge_overlay_get(Object *ob)
{
  BLI_assert(ELEM(ob->type, OB_CURVE, OB_SURF));

  struct Curve *cu = ob->data;
  return DRW_curve_batch_cache_get_edit_edges(cu);
}

GPUBatch *DRW_cache_curve_vert_overlay_get(Object *ob)
{
  BLI_assert(ELEM(ob->type, OB_CURVE, OB_SURF));

  struct Curve *cu = ob->data;
  return DRW_curve_batch_cache_get_edit_verts(cu);
}

GPUBatch *DRW_cache_curve_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVE);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_surface(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_triangles_with_normals(cu);
  }
}

GPUBatch *DRW_cache_curve_loose_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVE);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_loose_edges(mesh_eval);
  }
  else {
    /* TODO */
    UNUSED_VARS(cu);
    return NULL;
  }
}

GPUBatch *DRW_cache_curve_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVE);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_wireframes_face(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_wireframes_face(cu);
  }
}

GPUBatch *DRW_cache_curve_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  BLI_assert(ob->type == OB_CURVE);
  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_edge_detection(mesh_eval, r_is_manifold);
  }
  else {
    return DRW_curve_batch_cache_get_edge_detection(cu, r_is_manifold);
  }
}

/* Return list of batches */
GPUBatch **DRW_cache_curve_surface_shaded_get(Object *ob,
                                              struct GPUMaterial **gpumat_array,
                                              uint gpumat_array_len)
{
  BLI_assert(ob->type == OB_CURVE);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_surface_shaded(mesh_eval, gpumat_array, gpumat_array_len);
  }
  else {
    return DRW_curve_batch_cache_get_surface_shaded(cu, gpumat_array, gpumat_array_len);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MetaBall
 * \{ */

GPUBatch *DRW_cache_mball_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_MBALL);
  return DRW_metaball_batch_cache_get_triangles_with_normals(ob);
}

GPUBatch *DRW_cache_mball_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  BLI_assert(ob->type == OB_MBALL);
  return DRW_metaball_batch_cache_get_edge_detection(ob, r_is_manifold);
}

GPUBatch *DRW_cache_mball_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_MBALL);
  return DRW_metaball_batch_cache_get_wireframes_face(ob);
}

GPUBatch **DRW_cache_mball_surface_shaded_get(Object *ob,
                                              struct GPUMaterial **gpumat_array,
                                              uint gpumat_array_len)
{
  BLI_assert(ob->type == OB_MBALL);
  MetaBall *mb = ob->data;
  return DRW_metaball_batch_cache_get_surface_shaded(ob, mb, gpumat_array, gpumat_array_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Font
 * \{ */

GPUBatch *DRW_cache_text_edge_wire_get(Object *ob)
{
  BLI_assert(ob->type == OB_FONT);
  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  const bool has_surface = (cu->flag & (CU_FRONT | CU_BACK)) || cu->ext1 != 0.0f ||
                           cu->ext2 != 0.0f;
  if (!has_surface) {
    return NULL;
  }
  else if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_loose_edges(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_wire_edge(cu);
  }
}

GPUBatch *DRW_cache_text_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_FONT);
  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (cu->editfont && (cu->flag & CU_FAST)) {
    return NULL;
  }
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_surface(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_triangles_with_normals(cu);
  }
}

GPUBatch *DRW_cache_text_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  BLI_assert(ob->type == OB_FONT);
  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (cu->editfont && (cu->flag & CU_FAST)) {
    return NULL;
  }
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_edge_detection(mesh_eval, r_is_manifold);
  }
  else {
    return DRW_curve_batch_cache_get_edge_detection(cu, r_is_manifold);
  }
}

GPUBatch *DRW_cache_text_loose_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_FONT);
  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (cu->editfont && (cu->flag & CU_FAST)) {
    return NULL;
  }
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_loose_edges(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_wire_edge(cu);
  }
}

GPUBatch *DRW_cache_text_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_FONT);
  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (cu->editfont && (cu->flag & CU_FAST)) {
    return NULL;
  }
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_wireframes_face(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_wireframes_face(cu);
  }
}

GPUBatch **DRW_cache_text_surface_shaded_get(Object *ob,
                                             struct GPUMaterial **gpumat_array,
                                             uint gpumat_array_len)
{
  BLI_assert(ob->type == OB_FONT);
  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (cu->editfont && (cu->flag & CU_FAST)) {
    return NULL;
  }
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_surface_shaded(mesh_eval, gpumat_array, gpumat_array_len);
  }
  else {
    return DRW_curve_batch_cache_get_surface_shaded(cu, gpumat_array, gpumat_array_len);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

GPUBatch *DRW_cache_surf_surface_get(Object *ob)
{
  BLI_assert(ob->type == OB_SURF);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_surface(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_triangles_with_normals(cu);
  }
}

GPUBatch *DRW_cache_surf_edge_wire_get(Object *ob)
{
  BLI_assert(ob->type == OB_SURF);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_loose_edges(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_wire_edge(cu);
  }
}

GPUBatch *DRW_cache_surf_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_SURF);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_wireframes_face(mesh_eval);
  }
  else {
    return DRW_curve_batch_cache_get_wireframes_face(cu);
  }
}

GPUBatch *DRW_cache_surf_edge_detection_get(Object *ob, bool *r_is_manifold)
{
  BLI_assert(ob->type == OB_SURF);
  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_edge_detection(mesh_eval, r_is_manifold);
  }
  else {
    return DRW_curve_batch_cache_get_edge_detection(cu, r_is_manifold);
  }
}

GPUBatch *DRW_cache_surf_loose_edges_get(Object *ob)
{
  BLI_assert(ob->type == OB_SURF);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_loose_edges(mesh_eval);
  }
  else {
    /* TODO */
    UNUSED_VARS(cu);
    return NULL;
  }
}

/* Return list of batches */
GPUBatch **DRW_cache_surf_surface_shaded_get(Object *ob,
                                             struct GPUMaterial **gpumat_array,
                                             uint gpumat_array_len)
{
  BLI_assert(ob->type == OB_SURF);

  struct Curve *cu = ob->data;
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh_eval != NULL) {
    return DRW_mesh_batch_cache_get_surface_shaded(mesh_eval, gpumat_array, gpumat_array_len);
  }
  else {
    return DRW_curve_batch_cache_get_surface_shaded(cu, gpumat_array, gpumat_array_len);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice
 * \{ */

GPUBatch *DRW_cache_lattice_verts_get(Object *ob)
{
  BLI_assert(ob->type == OB_LATTICE);

  struct Lattice *lt = ob->data;
  return DRW_lattice_batch_cache_get_all_verts(lt);
}

GPUBatch *DRW_cache_lattice_wire_get(Object *ob, bool use_weight)
{
  BLI_assert(ob->type == OB_LATTICE);

  Lattice *lt = ob->data;
  int actdef = -1;

  if (use_weight && ob->defbase.first && lt->editlatt->latt->dvert) {
    actdef = ob->actdef - 1;
  }

  return DRW_lattice_batch_cache_get_all_edges(lt, use_weight, actdef);
}

GPUBatch *DRW_cache_lattice_vert_overlay_get(Object *ob)
{
  BLI_assert(ob->type == OB_LATTICE);

  struct Lattice *lt = ob->data;
  return DRW_lattice_batch_cache_get_edit_verts(lt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name PointCloud
 * \{ */

GPUBatch *DRW_cache_pointcloud_get_dots(Object *object)
{
  return DRW_pointcloud_batch_cache_get_dots(object);
}

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

GPUBatch *DRW_cache_volume_face_wireframe_get(Object *ob)
{
  BLI_assert(ob->type == OB_VOLUME);
  return DRW_volume_batch_cache_get_wireframes_face(ob->data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particles
 * \{ */

GPUBatch *DRW_cache_particles_get_hair(Object *object, ParticleSystem *psys, ModifierData *md)
{
  return DRW_particles_batch_cache_get_hair(object, psys, md);
}

GPUBatch *DRW_cache_particles_get_dots(Object *object, ParticleSystem *psys)
{
  return DRW_particles_batch_cache_get_dots(object, psys);
}

GPUBatch *DRW_cache_particles_get_edit_strands(Object *object,
                                               ParticleSystem *psys,
                                               struct PTCacheEdit *edit,
                                               bool use_weight)
{
  return DRW_particles_batch_cache_get_edit_strands(object, psys, edit, use_weight);
}

GPUBatch *DRW_cache_particles_get_edit_inner_points(Object *object,
                                                    ParticleSystem *psys,
                                                    struct PTCacheEdit *edit)
{
  return DRW_particles_batch_cache_get_edit_inner_points(object, psys, edit);
}

GPUBatch *DRW_cache_particles_get_edit_tip_points(Object *object,
                                                  ParticleSystem *psys,
                                                  struct PTCacheEdit *edit)
{
  return DRW_particles_batch_cache_get_edit_tip_points(object, psys, edit);
}

GPUBatch *DRW_cache_particles_get_prim(int type)
{
  switch (type) {
    case PART_DRAW_CROSS:
      if (!SHC.drw_particle_cross) {
        GPUVertFormat format = extra_vert_format();
        GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
        GPU_vertbuf_data_alloc(vbo, 6);

        int v = 0;
        int flag = 0;
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, -1.0f, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 1.0f, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{-1.0f, 0.0f, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{1.0f, 0.0f, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, -1.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 1.0f}, flag});

        SHC.drw_particle_cross = GPU_batch_create_ex(
            GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
      }

      return SHC.drw_particle_cross;
    case PART_DRAW_AXIS:
      if (!SHC.drw_particle_axis) {
        GPUVertFormat format = extra_vert_format();
        GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
        GPU_vertbuf_data_alloc(vbo, 6);

        int v = 0;
        int flag = VCLASS_EMPTY_AXES;
        /* Set minimum to 0.001f so we can easilly normalize to get the color. */
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0001f, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 2.0f, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0001f, 0.0f, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{2.0f, 0.0f, 0.0f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 0.0001f}, flag});
        GPU_vertbuf_vert_set(vbo, v++, &(Vert){{0.0f, 0.0f, 2.0f}, flag});

        SHC.drw_particle_axis = GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
      }

      return SHC.drw_particle_axis;
    case PART_DRAW_CIRC:
#define CIRCLE_RESOL 32
      if (!SHC.drw_particle_circle) {
        GPUVertFormat format = extra_vert_format();
        GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
        GPU_vertbuf_data_alloc(vbo, CIRCLE_RESOL + 1);

        int v = 0;
        int flag = VCLASS_SCREENALIGNED;
        for (int a = 0; a <= CIRCLE_RESOL; a++) {
          float angle = (2.0f * M_PI * a) / CIRCLE_RESOL;
          float x = sinf(angle);
          float y = cosf(angle);
          GPU_vertbuf_vert_set(vbo, v++, &(Vert){{x, y, 0.0f}, flag});
        }

        SHC.drw_particle_circle = GPU_batch_create_ex(
            GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
      }

      return SHC.drw_particle_circle;
#undef CIRCLE_RESOL
    default:
      BLI_assert(false);
      break;
  }

  return NULL;
}

/* 3D cursor */
GPUBatch *DRW_cache_cursor_get(bool crosshair_lines)
{
  GPUBatch **drw_cursor = crosshair_lines ? &SHC.drw_cursor : &SHC.drw_cursor_only_circle;

  if (*drw_cursor == NULL) {
    const float f5 = 0.25f;
    const float f10 = 0.5f;
    const float f20 = 1.0f;

    const int segments = 16;
    const int vert_len = segments + 8;
    const int index_len = vert_len + 5;

    uchar red[3] = {255, 0, 0};
    uchar white[3] = {255, 255, 255};

    static GPUVertFormat format = {0};
    static struct {
      uint pos, color;
    } attr_id;
    if (format.attr_len == 0) {
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      attr_id.color = GPU_vertformat_attr_add(
          &format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }

    GPUIndexBufBuilder elb;
    GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, index_len, vert_len);

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, vert_len);

    int v = 0;
    for (int i = 0; i < segments; i++) {
      float angle = (float)(2 * M_PI) * ((float)i / (float)segments);
      float x = f10 * cosf(angle);
      float y = f10 * sinf(angle);

      GPU_vertbuf_attr_set(vbo, attr_id.color, v, (i % 2 == 0) ? red : white);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){x, y});
      GPU_indexbuf_add_generic_vert(&elb, v++);
    }
    GPU_indexbuf_add_generic_vert(&elb, 0);

    if (crosshair_lines) {
      uchar crosshair_color[3];
      UI_GetThemeColor3ubv(TH_VIEW_OVERLAY, crosshair_color);

      GPU_indexbuf_add_primitive_restart(&elb);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){-f20, 0});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){-f5, 0});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);

      GPU_indexbuf_add_primitive_restart(&elb);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){+f5, 0});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){+f20, 0});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);

      GPU_indexbuf_add_primitive_restart(&elb);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){0, -f20});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){0, -f5});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);

      GPU_indexbuf_add_primitive_restart(&elb);

      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){0, +f5});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
      GPU_vertbuf_attr_set(vbo, attr_id.pos, v, (const float[2]){0, +f20});
      GPU_vertbuf_attr_set(vbo, attr_id.color, v, crosshair_color);
      GPU_indexbuf_add_generic_vert(&elb, v++);
    }

    GPUIndexBuf *ibo = GPU_indexbuf_build(&elb);

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
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  switch (ob->type) {
    case OB_MESH:
      DRW_mesh_batch_cache_validate((Mesh *)ob->data);
      break;
    case OB_CURVE:
    case OB_FONT:
    case OB_SURF:
      if (mesh_eval != NULL) {
        DRW_mesh_batch_cache_validate(mesh_eval);
      }
      DRW_curve_batch_cache_validate((Curve *)ob->data);
      break;
    case OB_MBALL:
      DRW_mball_batch_cache_validate((MetaBall *)ob->data);
      break;
    case OB_LATTICE:
      DRW_lattice_batch_cache_validate((Lattice *)ob->data);
      break;
    case OB_HAIR:
      DRW_hair_batch_cache_validate((Hair *)ob->data);
      break;
    case OB_POINTCLOUD:
      DRW_pointcloud_batch_cache_validate((PointCloud *)ob->data);
      break;
    case OB_VOLUME:
      DRW_volume_batch_cache_validate((Volume *)ob->data);
      break;
    default:
      break;
  }
}

void drw_batch_cache_generate_requested(Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  const enum eContextObjectMode mode = CTX_data_mode_enum_ex(
      draw_ctx->object_edit, draw_ctx->obact, draw_ctx->object_mode);
  const bool is_paint_mode = ELEM(
      mode, CTX_MODE_SCULPT, CTX_MODE_PAINT_TEXTURE, CTX_MODE_PAINT_VERTEX, CTX_MODE_PAINT_WEIGHT);

  const bool use_hide = ((ob->type == OB_MESH) &&
                         ((is_paint_mode && (ob == draw_ctx->obact) &&
                           DRW_object_use_hide_faces(ob)) ||
                          ((mode == CTX_MODE_EDIT_MESH) && DRW_object_is_in_edit_mode(ob))));

  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  switch (ob->type) {
    case OB_MESH:
      DRW_mesh_batch_cache_create_requested(
          DST.task_graph, ob, (Mesh *)ob->data, scene, is_paint_mode, use_hide);
      break;
    case OB_CURVE:
    case OB_FONT:
    case OB_SURF:
      if (mesh_eval) {
        DRW_mesh_batch_cache_create_requested(
            DST.task_graph, ob, mesh_eval, scene, is_paint_mode, use_hide);
      }
      DRW_curve_batch_cache_create_requested(ob);
      break;
    /* TODO all cases */
    default:
      break;
  }
}

void drw_batch_cache_generate_requested_delayed(Object *ob)
{
  BLI_gset_add(DST.delayed_extraction, ob);
}

void DRW_batch_cache_free_old(Object *ob, int ctime)
{
  struct Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);

  switch (ob->type) {
    case OB_MESH:
      DRW_mesh_batch_cache_free_old((Mesh *)ob->data, ctime);
      break;
    case OB_CURVE:
    case OB_FONT:
    case OB_SURF:
      if (mesh_eval) {
        DRW_mesh_batch_cache_free_old(mesh_eval, ctime);
      }
      break;
    /* TODO all cases */
    default:
      break;
  }
}

/** \} */
