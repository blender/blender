/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

/* Matches Vertex Format. */
struct Vertex {
  float3 pos;
  int vclass;
};

/* Caller gets ownership of the #gpu::VertBuf. */
static gpu::VertBuf *vbo_from_vector(const Vector<Vertex> &vector)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "vclass", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }

  gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*vbo, vector.size());
  vbo->data<Vertex>().copy_from(vector);
  return vbo;
}

enum VertexClass {
  VCLASS_NONE = 0,

  VCLASS_LIGHT_AREA_SHAPE = 1 << 0,
  VCLASS_LIGHT_SPOT_SHAPE = 1 << 1,
  VCLASS_LIGHT_SPOT_BLEND = 1 << 2,
  VCLASS_LIGHT_SPOT_CONE = 1 << 3,
  VCLASS_LIGHT_DIST = 1 << 4,

  VCLASS_CAMERA_FRAME = 1 << 5,
  VCLASS_CAMERA_DIST = 1 << 6,
  VCLASS_CAMERA_VOLUME = 1 << 7,

  VCLASS_SCREENSPACE = 1 << 8,
  VCLASS_SCREENALIGNED = 1 << 9,

  VCLASS_EMPTY_SCALED = 1 << 10,
  VCLASS_EMPTY_AXES = 1 << 11,
  VCLASS_EMPTY_AXES_NAME = 1 << 12,
  VCLASS_EMPTY_AXES_SHADOW = 1 << 13,
  VCLASS_EMPTY_SIZE = 1 << 14,
};

static constexpr int diamond_nsegments = 4;
static constexpr int inner_nsegments = 8;
static constexpr int outer_nsegments = 10;
static constexpr int circle_nsegments = 32;

static constexpr float bone_box_verts[8][3] = {
    {1.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, -1.0f},
    {-1.0f, 0.0f, -1.0f},
    {-1.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
    {-1.0f, 1.0f, 1.0f},
};

static constexpr std::array<uint, 24> bone_box_wire = {
    0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7,
};

static void append_line_loop(
    Vector<Vertex> &dest, Span<float2> verts, float z, int flag, bool dashed = false)
{
  const int step = dashed ? 2 : 1;
  for (const int i : IndexRange(verts.size() / step)) {
    for (const int j : IndexRange(2)) {
      const float2 &cv = verts[(i * step + j) % (verts.size())];
      dest.append({{cv[0], cv[1], z}, flag});
    }
  }
}

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

/* A single ring of vertices. */
static Vector<float2> ring_vertices(const float radius, const int segments)
{
  Vector<float2> verts;
  for (int i : IndexRange(segments)) {
    float angle = (2 * math::numbers::pi * i) / segments;
    verts.append(radius * float2(math::cos(angle), math::sin(angle)));
  }
  return verts;
}

/* Returns lines segment geometry forming 3 circles, one on each axis. */
static Vector<Vertex> sphere_axes_circles(const float radius,
                                          const VertexClass vclass,
                                          const int segments)
{
  Vector<float2> ring = ring_vertices(radius, segments);

  Vector<Vertex> verts;
  for (int axis : IndexRange(3)) {
    for (int i : IndexRange(segments)) {
      for (int j : IndexRange(2)) {
        float2 cv = ring[(i + j) % segments];
        if (axis == 0) {
          verts.append({{cv[0], cv[1], 0.0f}, vclass});
        }
        else if (axis == 1) {
          verts.append({{cv[0], 0.0f, cv[1]}, vclass});
        }
        else {
          verts.append({{0.0f, cv[0], cv[1]}, vclass});
        }
      }
    }
  }
  return verts;
}

static void light_append_direction_line(Vector<Vertex> &verts)
{
  const Vector<float2> diamond = ring_vertices(1.2f, diamond_nsegments);
  const float zsta = light_distance_z_get('z', true);
  const float zend = light_distance_z_get('z', false);
  verts.append({{0.0, 0.0, zsta}, VCLASS_LIGHT_DIST});
  verts.append({{0.0, 0.0, zend}, VCLASS_LIGHT_DIST});
  append_line_loop(verts, diamond, zsta, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);
  append_line_loop(verts, diamond, zend, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);
}

ShapeCache::ShapeCache()
{
  /* quad_wire */
  {
    Vector<Vertex> verts;
    verts.append({{-1.0f, -1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{-1.0f, +1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{-1.0f, +1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{+1.0f, +1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{+1.0f, +1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{+1.0f, -1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{+1.0f, -1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{-1.0f, -1.0f, 0.0f}, VCLASS_EMPTY_SCALED});

    quad_wire = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* plain_axes */
  {
    Vector<Vertex> verts;
    verts.append({{0.0f, -1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{0.0f, +1.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{-1.0f, 0.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{+1.0f, 0.0f, 0.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{0.0f, 0.0f, -1.0f}, VCLASS_EMPTY_SCALED});
    verts.append({{0.0f, 0.0f, +1.0f}, VCLASS_EMPTY_SCALED});

    plain_axes = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* single_arrow */
  {
    Vector<Vertex> verts;
    float p[3][3] = {{0}};
    p[0][2] = 1.0f;
    p[1][0] = 0.035f;
    p[1][1] = 0.035f;
    p[2][0] = -0.035f;
    p[2][1] = 0.035f;
    p[1][2] = p[2][2] = 0.75f;
    for (int sides : IndexRange(4)) {
      if (sides % 2 == 1) {
        p[1][0] = -p[1][0];
        p[2][1] = -p[2][1];
      }
      else {
        p[1][1] = -p[1][1];
        p[2][0] = -p[2][0];
      }
      for (int i = 0, a = 1; i < 2; i++, a++) {
        verts.append({{p[i][0], p[i][1], p[i][2]}, VCLASS_EMPTY_SCALED});
        verts.append({{p[a][0], p[a][1], p[a][2]}, VCLASS_EMPTY_SCALED});
      }
    }
    verts.append({{0.0f, 0.0f, 0.0}, VCLASS_EMPTY_SCALED});
    verts.append({{0.0f, 0.0f, 0.75f}, VCLASS_EMPTY_SCALED});

    single_arrow = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* cube */
  {
    Vector<Vertex> verts;
    for (auto index : bone_box_wire) {
      float x = bone_box_verts[index][0];
      float y = bone_box_verts[index][1] * 2.0 - 1.0f;
      float z = bone_box_verts[index][2];
      verts.append({{x, y, z}, VCLASS_EMPTY_SCALED});
    }

    cube = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* circle */
  {
    constexpr int resolution = 64;
    Vector<float2> ring = ring_vertices(1.0f, resolution);

    Vector<Vertex> verts;
    for (int a : IndexRange(resolution + 1)) {
      float2 cv = ring[a % resolution];
      verts.append({{cv.x, 0.0f, cv.y}, VCLASS_EMPTY_SCALED});
    }

    circle = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* empty_spehere */
  {
    Vector<Vertex> verts = sphere_axes_circles(1.0f, VCLASS_EMPTY_SCALED, 32);

    empty_sphere = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* empty_cone */
  {
    constexpr int resolution = 8;
    Vector<float2> ring = ring_vertices(1.0f, resolution);

    Vector<Vertex> verts;
    for (int i : IndexRange(resolution)) {
      float2 cv = ring[i % resolution];
      /* Cone sides. */
      verts.append({{cv[0], 0.0f, cv[1]}, VCLASS_EMPTY_SCALED});
      verts.append({{0.0f, 2.0f, 0.0f}, VCLASS_EMPTY_SCALED});
      /* Base ring. */
      for (int j : IndexRange(2)) {
        float2 cv = ring[(i + j) % resolution];
        verts.append({{cv[0], 0.0f, cv[1]}, VCLASS_EMPTY_SCALED});
      }
    }

    empty_cone = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* cylinder */
  {
    constexpr int n_segments = 12;
    const Vector<float2> ring = ring_vertices(1.0f, n_segments);
    Vector<Vertex> verts;
    /* top ring */
    append_line_loop(verts, ring, 1.0f, VCLASS_EMPTY_SCALED);
    /* bottom ring */
    append_line_loop(verts, ring, -1.0f, VCLASS_EMPTY_SCALED);
    /* cylinder sides */
    for (const float2 &point : ring) {
      verts.append({{point.x, point.y, 1.0f}, VCLASS_EMPTY_SCALED});
      verts.append({{point.x, point.y, -1.0f}, VCLASS_EMPTY_SCALED});
    }
    cylinder = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* capsule body */
  {
    const Vector<float2> diamond = ring_vertices(1.0f, 4);
    Vector<Vertex> verts;
    for (const float2 &point : diamond) {
      verts.append({{point.x, point.y, 1.0f}, VCLASS_NONE});
      verts.append({{point.x, point.y, 0.0f}, VCLASS_NONE});
    }
    capsule_body = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* capsule cap */
  {
    constexpr int n_segments = 24;
    const Vector<float2> ring = ring_vertices(1.0f, n_segments);
    Vector<Vertex> verts;
    /* Base circle */
    append_line_loop(verts, ring, 0.0f, VCLASS_NONE);
    for (const int i : IndexRange(n_segments / 2)) {
      const float2 &point = ring[i];
      const float2 &next_point = ring[i + 1];
      /* Y half circle */
      verts.append({{point.x, 0.0f, point.y}, VCLASS_NONE});
      verts.append({{next_point.x, 0.0f, next_point.y}, VCLASS_NONE});
      /* X half circle */
      verts.append({{0.0f, point.x, point.y}, VCLASS_NONE});
      verts.append({{0.0f, next_point.x, next_point.y}, VCLASS_NONE});
    }
    capsule_cap = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* arrows */
  {
    float2 x_axis_name_scale = {0.0215f, 0.025f};
    Vector<float2> x_axis_name = {
        float2(0.9f, 1.0f) * x_axis_name_scale,
        float2(-1.0f, -1.0f) * x_axis_name_scale,
        float2(-0.9f, 1.0f) * x_axis_name_scale,
        float2(1.0f, -1.0f) * x_axis_name_scale,
    };

    float2 y_axis_name_scale = {0.0175f, 0.025f};
    Vector<float2> y_axis_name = {
        float2(-1.0f, 1.0f) * y_axis_name_scale,
        float2(0.0f, -0.1f) * y_axis_name_scale,
        float2(1.0f, 1.0f) * y_axis_name_scale,
        float2(0.0f, -0.1f) * y_axis_name_scale,
        float2(0.0f, -0.1f) * y_axis_name_scale,
        float2(0.0f, -1.0f) * y_axis_name_scale,
    };

    float2 z_axis_name_scale = {0.02f, 0.025f};
    Vector<float2> z_axis_name = {
        float2(-0.95f, 1.00f) * z_axis_name_scale,
        float2(0.95f, 1.00f) * z_axis_name_scale,
        float2(0.95f, 1.00f) * z_axis_name_scale,
        float2(0.95f, 0.90f) * z_axis_name_scale,
        float2(0.95f, 0.90f) * z_axis_name_scale,
        float2(-1.00f, -0.90f) * z_axis_name_scale,
        float2(-1.00f, -0.90f) * z_axis_name_scale,
        float2(-1.00f, -1.00f) * z_axis_name_scale,
        float2(-1.00f, -1.00f) * z_axis_name_scale,
        float2(1.00f, -1.00f) * z_axis_name_scale,
    };

    float2 axis_marker_scale = {0.007f, 0.007f};
    Vector<float2> axis_marker = {
#if 0 /* square */
      float2(-1.0f, 1.0f) * axis_marker_scale,
      float2(1.0f, 1.0f) * axis_marker_scale,
      float2(1.0f, 1.0f) * axis_marker_scale,
      float2(1.0f, -1.0f) * axis_marker_scale,
      float2(1.0f, -1.0f) * axis_marker_scale,
      float2(-1.0f, -1.0f) * axis_marker_scale,
      float2(-1.0f, -1.0f) * axis_marker_scale,
      float2(-1.0f, 1.0f) * axis_marker_scale,
#else /* diamond */
      float2(-1.0f, 0.0f) * axis_marker_scale,
      float2(0.0f, 1.0f) * axis_marker_scale,
      float2(0.0f, 1.0f) * axis_marker_scale,
      float2(1.0f, 0.0f) * axis_marker_scale,
      float2(1.0f, 0.0f) * axis_marker_scale,
      float2(0.0f, -1.0f) * axis_marker_scale,
      float2(0.0f, -1.0f) * axis_marker_scale,
      float2(-1.0f, 0.0f) * axis_marker_scale,
#endif
    };

    Vector<Vertex> verts;
    for (int axis : IndexRange(3)) {
      /* Vertex layout is XY screen position and axis in Z.
       * Fractional part of Z is a positive offset at axis unit position. */
      int flag = VCLASS_EMPTY_AXES | VCLASS_SCREENALIGNED;
      /* Center to axis line. */
      verts.append({{0.0f, 0.0f, 0.0f}, 0});
      verts.append({{0.0f, 0.0f, float(axis)}, flag});
      /* Axis end marker. */
      constexpr int marker_fill_layer = 6;
      for (int j = 1; j < marker_fill_layer + 1; j++) {
        for (float2 axis_marker_vert : axis_marker) {
          verts.append({{axis_marker_vert * ((4.0f * j) / marker_fill_layer), float(axis)}, flag});
        }
      }
      /* Axis name. */
      Vector<float2> *axis_names[3] = {&x_axis_name, &y_axis_name, &z_axis_name};
      for (float2 axis_name_vert : *(axis_names[axis])) {
        int flag = VCLASS_EMPTY_AXES | VCLASS_EMPTY_AXES_NAME | VCLASS_SCREENALIGNED;
        verts.append({{axis_name_vert * 4.0f, axis + 0.25f}, flag});
      }
    }
    arrows = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* metaball_wire_circle */
  {
    constexpr int resolution = 64;
    constexpr float radius = 1.0f;
    Vector<float2> ring = ring_vertices(radius, resolution);

    Vector<Vertex> verts;
    for (int i : IndexRange(resolution + 1)) {
      float2 cv = ring[i % resolution];
      verts.append({{cv[0], cv[1], 0.0f}, VCLASS_SCREENALIGNED});
    }
    metaball_wire_circle = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  };
  /* speaker */
  {
    constexpr int segments = 16;
    Vector<Vertex> verts;

    for (int j = 0; j < 3; j++) {
      float z = 0.25f * j - 0.125f;
      float r = (j == 0 ? 0.5f : 0.25f);

      verts.append({{r, 0.0f, z}});

      for (int i = 1; i < segments; i++) {
        float x = cosf(2.0f * math::numbers::pi * i / segments) * r;
        float y = sinf(2.0f * math::numbers::pi * i / segments) * r;
        Vertex v{{x, y, z}};
        verts.append(v);
        verts.append(v);
      }
      Vertex v{{r, 0.0f, z}};
      verts.append(v);
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
        Vertex v{{x, y, z}};
        verts.append(v);
        if (i == 1) {
          verts.append(v);
        }
      }
    }
    speaker = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* ground line */
  {
    const Vector<float2> ring = ring_vertices(1.35f, diamond_nsegments);

    Vector<Vertex> verts;
    /* Ground Point */
    append_line_loop(verts, ring, 0.0f, 0);
    /* Ground Line */
    verts.append({{0.0, 0.0, 1.0}, 0});
    verts.append({{0.0, 0.0, 0.0}, 0});

    ground_line = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light spot volume */
  {
    Vector<Vertex> verts;

    /* Cone apex */
    verts.append({{0.0f, 0.0f, 0.0f}, 0});
    /* Cone silhouette */
    for (const int angle_i : IndexRange(circle_nsegments + 1)) {
      const float angle = (2.0f * math::numbers::pi * angle_i) / circle_nsegments;
      const float s = sinf(-angle);
      const float c = cosf(-angle);
      verts.append({{s, c, -1.0f}, VCLASS_LIGHT_SPOT_SHAPE});
    }
    light_spot_volume = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_TRI_FAN, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light icon outer lines */
  {
    constexpr float r = 9.0f;
    const Vector<float2> ring = ring_vertices(r * 1.33f, outer_nsegments * 2);

    Vector<Vertex> verts;
    append_line_loop(verts, ring, 0.0f, VCLASS_SCREENSPACE, true);
    light_icon_outer_lines = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light icon inner lines */
  {
    constexpr float r = 9.0f;
    const Vector<float2> diamond = ring_vertices(r * 0.3f, diamond_nsegments);
    const Vector<float2> ring = ring_vertices(r, inner_nsegments * 2);

    Vector<Vertex> verts;
    append_line_loop(verts, diamond, 0.0f, VCLASS_SCREENSPACE);
    append_line_loop(verts, ring, 0.0f, VCLASS_SCREENSPACE, true);

    light_icon_inner_lines = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light icon sun rays */
  {
    constexpr int num_rays = 8;
    constexpr float r = 9.0f;
    const Vector<float2> ring = ring_vertices(r, num_rays);
    const std::array<float, 4> scales{1.6f, 1.9f, 2.2f, 2.5f};

    Vector<Vertex> verts;
    for (const float2 &point : ring) {
      for (float scale : scales) {
        float2 scaled = point * scale;
        verts.append({{scaled.x, scaled.y, 0.0f}, VCLASS_SCREENSPACE});
      }
    }
    light_icon_sun_rays = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light point lines */
  {
    const Vector<float2> ring = ring_vertices(1.0f, circle_nsegments);

    Vector<Vertex> verts;
    append_line_loop(verts, ring, 0.0f, VCLASS_SCREENALIGNED | VCLASS_LIGHT_AREA_SHAPE);
    light_point_lines = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light sun lines */
  {
    Vector<Vertex> verts;
    /* Direction Line */
    verts.append({{0.0, 0.0, 0.0}, 0});
    verts.append({{0.0, 0.0, -20.0}, 0}); /* Good default. */
    light_sun_lines = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light spot lines */
  {
    const Vector<float2> ring = ring_vertices(1.0f, circle_nsegments);

    Vector<Vertex> verts;
    /* Light area */
    append_line_loop(verts, ring, 0.0f, VCLASS_SCREENALIGNED | VCLASS_LIGHT_AREA_SHAPE);
    /* Cone cap */
    append_line_loop(verts, ring, 0.0f, VCLASS_LIGHT_SPOT_SHAPE);
    append_line_loop(verts, ring, 0.0f, VCLASS_LIGHT_SPOT_SHAPE | VCLASS_LIGHT_SPOT_BLEND);
    /* Cone silhouette */
    for (const float2 &point : ring) {
      verts.append({{0.0f, 0.0f, 0.0f}, 0});
      verts.append({{point.x, point.y, -1.0f}, VCLASS_LIGHT_SPOT_SHAPE | VCLASS_LIGHT_SPOT_CONE});
    }

    light_append_direction_line(verts);

    light_spot_lines = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light area disk lines */
  {
    const Vector<float2> ring = ring_vertices(0.5f, circle_nsegments);

    Vector<Vertex> verts;
    /* Light area */
    append_line_loop(verts, ring, 0.0f, VCLASS_LIGHT_AREA_SHAPE);

    light_append_direction_line(verts);

    light_area_disk_lines = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light area square lines */
  {
    const Array<float2> rect{{-0.5f, -0.5f}, {-0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, -0.5f}};

    Vector<Vertex> verts;
    /* Light area */
    append_line_loop(verts, rect, 0.0f, VCLASS_LIGHT_AREA_SHAPE);

    light_append_direction_line(verts);

    light_area_square_lines = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
}

}  // namespace blender::draw::overlay
