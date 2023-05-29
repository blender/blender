/* SPDX-License-Identifier: GPL-2.0-or-later */

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

/* Caller gets ownership of the #GPUVertBuf. */
static GPUVertBuf *vbo_from_vector(Vector<Vertex> &vector)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "vclass", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, vector.size());
  Vertex *vbo_data = (Vertex *)GPU_vertbuf_get_data(vbo);
  /* Copy data to VBO using a wrapper span. Could use memcpy if that's too slow. */
  MutableSpan<Vertex> span(vbo_data, vector.size());
  span.copy_from(vector);
  return vbo;
}

enum VertexClass {
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

/* A single ring of vertices. */
static Vector<float2> ring_vertices(const float radius, const int segments)
{
  Vector<float2> verts;
  for (int i : IndexRange(segments)) {
    float angle = (2 * M_PI * i) / segments;
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
}

}  // namespace blender::draw::overlay
