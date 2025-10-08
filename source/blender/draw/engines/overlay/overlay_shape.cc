/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#include "GPU_batch_utils.hh"
#include "draw_cache.hh"

#include "overlay_private.hh"

namespace blender::draw::overlay {

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

static constexpr std::array<uint, 24> bone_box_wire_lines = {
    0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7,
};

static const std::array<uint3, 12> bone_box_solid_tris{
    uint3{0, 2, 1}, /* bottom */
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
 * Store indices of generated verts from `bone_box_solid_tris` to define adjacency information.
 * See `bone_octahedral_solid_tris` for more information.
 */
static const std::array<uint4, 12> bone_box_wire_lines_adjacency = {
    uint4{4, 2, 0, 11},
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

/* aligned with bone_box_solid_tris */
static const std::array<float3, 12> bone_box_solid_normals = {
    float3{0.0f, -1.0f, 0.0f},
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

static const std::array<float3, 6> bone_octahedral_verts{
    float3{0.0f, 0.0f, 0.0f},
    {0.1f, 0.1f, 0.1f},
    {0.1f, 0.1f, -0.1f},
    {-0.1f, 0.1f, -0.1f},
    {-0.1f, 0.1f, 0.1f},
    {0.0f, 1.0f, 0.0f},
};

/**
 * NOTE: This is not the correct normals.
 * The correct smooth normals for the equator vertices should be
 * {+-0.943608f * M_SQRT1_2, -0.331048f, +-0.943608f * M_SQRT1_2}
 * but it creates problems for outlines when bones are scaled.
 */
static const std::array<float3, 6> bone_octahedral_smooth_normals{
    float3{0.0f, -1.0f, 0.0f},
    {float(M_SQRT1_2), 0.0f, float(M_SQRT1_2)},
    {float(M_SQRT1_2), 0.0f, -float(M_SQRT1_2)},
    {-float(M_SQRT1_2), 0.0f, -float(M_SQRT1_2)},
    {-float(M_SQRT1_2), 0.0f, float(M_SQRT1_2)},
    {0.0f, 1.0f, 0.0f},
};

static const std::array<uint2, 12> bone_octahedral_wire_lines = {
    uint2{0, 1},
    {1, 5},
    {5, 3},
    {3, 0},
    {0, 4},
    {4, 5},
    {5, 2},
    {2, 0},
    {1, 2},
    {2, 3},
    {3, 4},
    {4, 1},
};

static const std::array<uint3, 8> bone_octahedral_solid_tris = {
    uint3{2, 1, 0}, /* bottom */
    {3, 2, 0},
    {4, 3, 0},
    {1, 4, 0},

    {5, 1, 2}, /* top */
    {5, 2, 3},
    {5, 3, 4},
    {5, 4, 1},
};

/**
 * Store indices of generated verts from `bone_octahedral_solid_tris` to define adjacency
 * information.
 * Example: triangle {2, 1, 0} is adjacent to {3, 2, 0}, {1, 4, 0} and {5, 1, 2}.
 * {2, 1, 0} becomes {0, 1, 2}
 * {3, 2, 0} becomes {3, 4, 5}
 * {1, 4, 0} becomes {9, 10, 11}
 * {5, 1, 2} becomes {12, 13, 14}
 * According to opengl specification it becomes (starting from
 * the first vertex of the first face aka. vertex 2):
 * {0, 12, 1, 10, 2, 3}
 */
static const std::array<uint4, 12> bone_octahedral_wire_lines_adjacency = {
    uint4{0, 1, 2, 6},
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

static void append_line_loop(
    Vector<Vertex> &dest, Span<float2> verts, float z, VertexClass flag, bool dashed = false)
{
  const int step = dashed ? 2 : 1;
  for (const int i : IndexRange(verts.size() / step)) {
    for (const int j : IndexRange(2)) {
      const float2 &cv = verts[(i * step + j) % verts.size()];
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
static Vector<float2> ring_vertices(const float radius,
                                    const int segments,
                                    const bool half = false)
{
  Vector<float2> verts;
  const float full = (half ? 1.0f : 2.0f) * math::numbers::pi;
  for (const int angle_i : IndexRange(segments + (half ? 1 : 0))) {
    const float angle = (full * angle_i) / segments;
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

static void light_append_direction_line(const char axis,
                                        Span<float2> diamond,
                                        Vector<Vertex> &verts)
{
  const float zsta = light_distance_z_get(axis, true);
  const float zend = light_distance_z_get(axis, false);
  verts.append({{0.0, 0.0, zsta}, VCLASS_LIGHT_DIST});
  verts.append({{0.0, 0.0, zend}, VCLASS_LIGHT_DIST});
  append_line_loop(verts, diamond, zsta, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);
  append_line_loop(verts, diamond, zend, VCLASS_LIGHT_DIST | VCLASS_SCREENSPACE);
}

static void light_append_direction_line(Vector<Vertex> &verts)
{
  const Vector<float2> diamond = ring_vertices(1.2f, diamond_nsegments);
  light_append_direction_line('z', diamond, verts);
}

static VertShaded sphere_lat_lon_vert(const float2 &lat_pt, const float2 &lon_pt)
{
  const float x = lon_pt.y * lat_pt.x;
  const float y = lon_pt.x;
  const float z = lon_pt.y * lat_pt.y;
  return VertShaded{{x, y, z}, VCLASS_EMPTY_SCALED, {x, y, z}};
}

static void append_sphere(Vector<VertShaded> &dest, const eDRWLevelOfDetail level_of_detail)
{
  /* Sphere shape resolution */
  /* Low */
  constexpr int drw_sphere_shape_latitude_low = 32;
  constexpr int drw_sphere_shape_longitude_low = 24;
  /* Medium */
  constexpr int drw_sphere_shape_latitude_medium = 64;
  constexpr int drw_sphere_shape_longitude_medium = 48;
  /* High */
  constexpr int drw_sphere_shape_latitude_high = 80;
  constexpr int drw_sphere_shape_longitude_high = 60;

  BLI_assert(level_of_detail >= DRW_LOD_LOW && level_of_detail < DRW_LOD_MAX);
  const std::array<Vector<float2>, DRW_LOD_MAX> latitude_rings = {
      ring_vertices(1.0f, drw_sphere_shape_latitude_low),
      ring_vertices(1.0f, drw_sphere_shape_latitude_medium),
      ring_vertices(1.0f, drw_sphere_shape_latitude_high)};
  const std::array<Vector<float2>, DRW_LOD_MAX> longitude_half_rings = {
      ring_vertices(1.0f, drw_sphere_shape_longitude_low, true),
      ring_vertices(1.0f, drw_sphere_shape_longitude_medium, true),
      ring_vertices(1.0f, drw_sphere_shape_longitude_high, true)};

  const Vector<float2> &latitude_ring = latitude_rings[level_of_detail];
  const Vector<float2> &longitude_half_ring = longitude_half_rings[level_of_detail];

  for (const int i : latitude_ring.index_range()) {
    const float2 lat_pt = latitude_ring[i];
    const float2 next_lat_pt = latitude_ring[(i + 1) % latitude_ring.size()];
    for (const int j : IndexRange(longitude_half_ring.size() - 1)) {
      const float2 lon_pt = longitude_half_ring[j];
      const float2 next_lon_pt = longitude_half_ring[j + 1];
      if (j != 0) { /* Pole */
        dest.append(sphere_lat_lon_vert(next_lat_pt, next_lon_pt));
        dest.append(sphere_lat_lon_vert(next_lat_pt, lon_pt));
        dest.append(sphere_lat_lon_vert(lat_pt, lon_pt));
      }
      if (j != longitude_half_ring.index_range().last(1)) { /* Pole */
        dest.append(sphere_lat_lon_vert(lat_pt, next_lon_pt));
        dest.append(sphere_lat_lon_vert(next_lat_pt, next_lon_pt));
        dest.append(sphere_lat_lon_vert(lat_pt, lon_pt));
      }
    }
  }
}

ShapeCache::ShapeCache()
{
  UNUSED_VARS(bone_octahedral_wire_lines);

  /* Armature Octahedron. */
  {
    Vector<VertShaded> verts;
    for (int tri = 0; tri < 8; tri++) {
      for (int v = 0; v < 3; v++) {
        verts.append({bone_octahedral_verts[bone_octahedral_solid_tris[tri][v]],
                      VCLASS_NONE,
                      bone_octahedral_solid_normals[tri]});
      }
    }
    bone_octahedron = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  {
    GPUIndexBufBuilder elb;
    GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, 12, 24);

    for (auto line : bone_octahedral_wire_lines_adjacency) {
      GPU_indexbuf_add_line_adj_verts(&elb, line[0], line[1], line[2], line[3]);
    }
    gpu::IndexBuf *ibo = GPU_indexbuf_build(&elb);

    /* NOTE: Reuses the same VBO as bone_octahedron. Thus has the same vertex format. */
    bone_octahedron_wire = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_LINES_ADJ, bone_octahedron.get()->verts[0], ibo, GPU_BATCH_OWNS_INDEX));
  }

  /* Armature Sphere. */
  {
    constexpr int resolution = 64;
    Vector<float2> ring = ring_vertices(0.05f, resolution);

    Vector<Vertex> verts;
    for (int a : IndexRange(resolution + 1)) {
      float2 cv = ring[a % resolution];
      verts.append({{cv.x, cv.y, 0.0f}, VCLASS_EMPTY_SCALED});
    }

    bone_sphere = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_TRI_FAN, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  {
    bone_sphere_wire = BatchPtr(
        GPU_batch_create(GPU_PRIM_LINE_STRIP, bone_sphere.get()->verts[0], nullptr));
  }

  /* Armature Stick. */
  {
    const StickBoneFlag bone = StickBoneFlag(COL_BONE | POS_BONE);
    /* Gather as a strip and add to main buffer as a list of triangles. */
    Vector<VertexBone> vert_strip;
    vert_strip.append({{0.0f, 1.0f, 0.0f}, StickBoneFlag(bone | POS_HEAD | COL_HEAD | COL_WIRE)});
    vert_strip.append({{0.0f, 1.0f, 0.0f}, StickBoneFlag(bone | POS_TAIL | COL_TAIL | COL_WIRE)});
    vert_strip.append({{0.0f, 0.0f, 0.0f}, StickBoneFlag(bone | POS_HEAD | COL_HEAD)});
    vert_strip.append({{0.0f, 0.0f, 0.0f}, StickBoneFlag(bone | POS_TAIL | COL_TAIL)});
    vert_strip.append({{0.0f, -1.0f, 0.0f}, StickBoneFlag(bone | POS_HEAD | COL_HEAD | COL_WIRE)});
    vert_strip.append({{0.0f, -1.0f, 0.0f}, StickBoneFlag(bone | POS_TAIL | COL_TAIL | COL_WIRE)});

    Vector<VertexBone> verts;
    /* Bone rectangle */
    for (int t : IndexRange(vert_strip.size() - 2)) {
      /* NOTE: Don't care about winding.
       * Theses triangles are facing the camera and should not be backface culled. */
      verts.append(vert_strip[t]);
      verts.append(vert_strip[t + 1]);
      verts.append(vert_strip[t + 2]);
    }

    constexpr int resolution = 12;
    Vector<float2> ring = ring_vertices(2.0f, resolution);
    for (int a : IndexRange(resolution)) {
      float2 cv1 = ring[a % resolution];
      float2 cv2 = ring[(a + 1) % resolution];
      /* Head point. */
      verts.append({{0.0f, 0.0f, 0.0f}, StickBoneFlag(POS_HEAD | COL_HEAD)});
      verts.append({{cv1.x, cv1.y, 0.0f}, StickBoneFlag(POS_HEAD | COL_HEAD | COL_WIRE)});
      verts.append({{cv2.x, cv2.y, 0.0f}, StickBoneFlag(POS_HEAD | COL_HEAD | COL_WIRE)});
      /* Tail point. */
      verts.append({{0.0f, 0.0f, 0.0f}, StickBoneFlag(POS_TAIL | COL_TAIL)});
      verts.append({{cv1.x, cv1.y, 0.0f}, StickBoneFlag(POS_TAIL | COL_TAIL | COL_WIRE)});
      verts.append({{cv2.x, cv2.y, 0.0f}, StickBoneFlag(POS_TAIL | COL_TAIL | COL_WIRE)});
    }

    bone_stick = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }

  /* Armature BBones. */
  {
    Vector<VertShaded> verts;
    for (int tri = 0; tri < 12; tri++) {
      for (int v = 0; v < 3; v++) {
        verts.append({bone_box_verts[bone_box_solid_tris[tri][v]],
                      VCLASS_NONE,
                      bone_box_solid_normals[tri]});
      }
    }
    bone_box = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  {
    GPUIndexBufBuilder elb;
    GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, 12, 36);

    for (auto line : bone_box_wire_lines_adjacency) {
      GPU_indexbuf_add_line_adj_verts(&elb, line[0], line[1], line[2], line[3]);
    }
    gpu::IndexBuf *ibo = GPU_indexbuf_build(&elb);

    /* NOTE: Reuses the same VBO as bone_box. Thus has the same vertex format. */
    bone_box_wire = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_LINES_ADJ, bone_box.get()->verts[0], ibo, GPU_BATCH_OWNS_INDEX));
  }

  /* Armature Envelope. */
  {
    constexpr int lon_res = 24;
    constexpr int lat_res = 24;
    constexpr float lon_inc = 2.0f * M_PI / lon_res;
    constexpr float lat_inc = M_PI / lat_res;

    auto lat_lon_to_co = [](const float lat, const float lon) {
      return float3(sinf(lat) * cosf(lon), sinf(lat) * sinf(lon), cosf(lat));
    };

    Vector<Vertex> verts;
    float lon = 0.0f;
    for (int i = 0; i < lon_res; i++, lon += lon_inc) {
      float lat = 0.0f;
      /* NOTE: the poles are duplicated on purpose, to restart the strip. */
      for (int j = 0; j < lat_res; j++, lat += lat_inc) {
        verts.append({lat_lon_to_co(lat, lon), VCLASS_NONE});
        verts.append({lat_lon_to_co(lat, lon + lon_inc), VCLASS_NONE});
      }
      /* Closing the loop */
      verts.append({lat_lon_to_co(M_PI, lon), VCLASS_NONE});
      verts.append({lat_lon_to_co(M_PI, lon + lon_inc), VCLASS_NONE});
    }

    bone_envelope = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_TRI_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  {
    constexpr int circle_resolution = 64;
    float2 v0, v1, v2;

    auto circle_pt = [](const float angle) { return float2(sinf(angle), cosf(angle)); };

    Vector<VertexTriple> verts;
    /* Output 3 verts for each position. See shader for explanation. */
    v0 = circle_pt((2.0f * M_PI * -2) / float(circle_resolution));
    v1 = circle_pt((2.0f * M_PI * -1) / float(circle_resolution));
    for (int a = 0; a <= circle_resolution; a++, v0 = v1, v1 = v2) {
      v2 = circle_pt((2.0f * M_PI * a) / float(circle_resolution));
      verts.append({v0, v1, v2});
    }

    bone_envelope_wire = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }

  /* Degrees of freedom. */
  {
    constexpr int resolution = 16;

    Vector<Vertex> verts;
    auto set_vert = [&](const float x, const float y, const int quarter) {
      verts.append({{(quarter % 2 == 0) ? -x : x, (quarter < 2) ? -y : y, 0.0f}, VCLASS_NONE});
    };

    for (int quarter : IndexRange(4)) {
      float prev_z = 0.0f;
      for (int i : IndexRange(1, resolution - 1)) {
        float z = sinf(M_PI_2 * i / float(resolution - 1));
        float prev_x = 0.0f;
        for (int j : IndexRange(1, resolution - i)) {
          float x = sinf(M_PI_2 * j / float(resolution - 1));
          if (j == resolution - i) {
            /* Pole triangle. */
            set_vert(prev_x, z, quarter);
            set_vert(prev_x, prev_z, quarter);
            set_vert(x, prev_z, quarter);
          }
          else {
            /* Quad. */
            set_vert(x, z, quarter);
            set_vert(x, prev_z, quarter);
            set_vert(prev_x, z, quarter);

            set_vert(x, prev_z, quarter);
            set_vert(prev_x, prev_z, quarter);
            set_vert(prev_x, z, quarter);
          }
          prev_x = x;
        }
        prev_z = z;
      }
    }

    bone_degrees_of_freedom = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  {
    constexpr int resolution = 16 * 4;
    Vector<float2> ring = ring_vertices(1.0f, resolution);

    Vector<Vertex> verts;
    for (int a : IndexRange(resolution + 1)) {
      float2 cv = ring[a % resolution];
      verts.append({{cv.x, cv.y, 0.0f}, VCLASS_NONE});
    }

    bone_degrees_of_freedom_wire = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }

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
  /* quad_solid */
  {
    const Array<float2> quad = {{-1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, -1.0f}};
    Vector<Vertex> verts;
    for (const float2 &point : quad) {
      verts.append({{point, 0.0f}, VCLASS_EMPTY_SCALED});
    }
    quad_solid = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_TRI_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
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
    for (auto index : bone_box_wire_lines) {
      float x = bone_box_verts[index][0];
      float y = bone_box_verts[index][1] * 2.0 - 1.0f;
      float z = bone_box_verts[index][2];
      verts.append({{x, y, z}, VCLASS_EMPTY_SCALED});
    }

    cube = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* cube_solid */
  {
    cube_solid = BatchPtr(GPU_batch_unit_cube());
  }
  /* circle */
  {
    constexpr int resolution = 64;
    Vector<float2> ring = ring_vertices(1.0f, resolution);

    Vector<Vertex> verts;
    for (int a : IndexRange(resolution + 1)) {
      float2 cv1 = ring[(a + 0) % resolution];
      float2 cv2 = ring[(a + 1) % resolution];
      verts.append({{cv1.x, 0.0f, cv1.y}, VCLASS_EMPTY_SCALED});
      verts.append({{cv2.x, 0.0f, cv2.y}, VCLASS_EMPTY_SCALED});
    }

    circle = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* empty_sphere */
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
      VertexClass flag = VCLASS_EMPTY_AXES | VCLASS_SCREENALIGNED;
      /* Center to axis line. */
      /* NOTE: overlay_armature_shape_wire_vert.glsl expects the axis verts at the origin to be the
       * only ones with this coordinates (it derives the VCLASS from it). */
      float pos_on_axis = float(axis) + 1e-8f;
      verts.append({{0.0f, 0.0f, 0.0f}, VCLASS_NONE});
      verts.append({{0.0f, 0.0f, pos_on_axis}, flag});
      /* Axis end marker. */
      constexpr int marker_fill_layer = 6;
      for (int j = 1; j < marker_fill_layer + 1; j++) {
        for (float2 axis_marker_vert : axis_marker) {
          verts.append({{axis_marker_vert * ((4.0f * j) / marker_fill_layer), pos_on_axis}, flag});
        }
      }
      /* Axis name. */
      const Vector<float2> *axis_names[3] = {&x_axis_name, &y_axis_name, &z_axis_name};
      for (float2 axis_name_vert : *(axis_names[axis])) {
        VertexClass flag = VCLASS_EMPTY_AXES | VCLASS_EMPTY_AXES_NAME | VCLASS_SCREENALIGNED;
        verts.append({{axis_name_vert * 4.0f, pos_on_axis + 0.25f}, flag});
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
    constexpr float bottom_r = 0.5f;
    constexpr float bottom_z = -0.125f;
    constexpr float step_z = 0.25f;
    const Vector<float2> diamond = ring_vertices(bottom_r, 4);
    Vector<float2> ring = ring_vertices(bottom_r, segments);
    Vector<Vertex> verts;

    append_line_loop(verts, ring, bottom_z, VCLASS_NONE);
    for (float2 &point : ring) {
      point *= 0.5f;
    }
    for (const int j : IndexRange(1, 2)) {
      const float z = step_z * j + bottom_z;
      append_line_loop(verts, ring, z, VCLASS_NONE);
    }

    for (const float2 &point : diamond) {
      Vertex vertex{float3(point, bottom_z)};
      verts.append(vertex);
      vertex.pos = float3(point * 0.5f, bottom_z + step_z);
      verts.append(vertex);
      verts.append(vertex);
      vertex.pos.z += step_z;
      verts.append(vertex);
    }
    speaker = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* camera distances */
  {
    const Vector<float2> diamond = ring_vertices(1.5f, 5);
    const Vector<float2> cross = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}};

    Vector<Vertex> verts;
    verts.append({{0.0f, 0.0f, 0.0f}, VCLASS_CAMERA_DIST});
    verts.append({{0.0f, 0.0f, 1.0f}, VCLASS_CAMERA_DIST});

    append_line_loop(verts, diamond, 0.0f, VCLASS_CAMERA_DIST | VCLASS_SCREENSPACE);
    append_line_loop(verts, diamond, 1.0f, VCLASS_CAMERA_DIST | VCLASS_SCREENSPACE);

    /* Focus cross */
    for (const float2 &point : cross) {
      verts.append({{point.x, point.y, 2.0f}, VCLASS_CAMERA_DIST});
    }
    camera_distances = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* camera frame */
  {
    const Vector<float2> rect{{-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}};
    Vector<Vertex> verts;
    /* Frame */
    append_line_loop(verts, rect, 1.0f, VCLASS_CAMERA_FRAME);
    /* Wires to origin. */
    for (const float2 &point : rect) {
      verts.append({{point.x, point.y, 1.0f}, VCLASS_CAMERA_FRAME});
      verts.append({{point.x, point.y, 0.0f}, VCLASS_CAMERA_FRAME});
    }
    camera_frame = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* camera tria */
  {
    const Vector<float2> triangle = {{-1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}};
    Vector<Vertex> verts;
    /* Wire */
    append_line_loop(verts, triangle, 1.0f, VCLASS_CAMERA_FRAME);
    camera_tria_wire = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));

    verts.clear();
    /* Triangle */
    for (const float2 &point : triangle) {
      verts.append({{point.x, point.y, 1.0f}, VCLASS_CAMERA_FRAME});
    }
    camera_tria = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* camera volume */
  {
    Vector<Vertex> verts;
    for (const uint3 &tri : bone_box_solid_tris) {
      for (const int i : IndexRange(uint3::type_length)) {
        const int v = tri[i];
        const float x = bone_box_verts[v][2];
        const float y = bone_box_verts[v][0];
        const float z = bone_box_verts[v][1];
        verts.append({{x, y, z}, VCLASS_CAMERA_FRAME | VCLASS_CAMERA_VOLUME});
      }
    }
    camera_volume = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* camera volume wire */
  {
    Vector<Vertex> verts;
    for (int i : bone_box_wire_lines) {
      const float x = bone_box_verts[i][2];
      const float y = bone_box_verts[i][0];
      const float z = bone_box_verts[i][1];
      verts.append({{x, y, z}, VCLASS_CAMERA_FRAME | VCLASS_CAMERA_VOLUME});
    }
    camera_volume_wire = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* spheres */
  {
    Vector<VertShaded> verts;
    append_sphere(verts, DRW_LOD_LOW);
    sphere_low_detail = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* ground line */
  {
    const Vector<float2> ring = ring_vertices(1.35f, diamond_nsegments);

    Vector<Vertex> verts;
    /* Ground Point */
    append_line_loop(verts, ring, 0.0f, VCLASS_NONE);
    /* Ground Line */
    verts.append({{0.0, 0.0, 1.0}, VCLASS_NONE});
    verts.append({{0.0, 0.0, 0.0}, VCLASS_NONE});

    ground_line = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* image_quad */
  {
    const Array<float2> quad = {{0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}};
    Vector<Vertex> verts;
    for (const float2 &point : quad) {
      verts.append({{point, 0.75f}, VCLASS_NONE});
    }
    image_quad = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_TRI_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* light spot volume */
  {
    Vector<Vertex> verts;

    /* Cone apex */
    verts.append({{0.0f, 0.0f, 0.0f}, VCLASS_NONE});
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
    verts.append({{0.0, 0.0, 0.0}, VCLASS_NONE});
    verts.append({{0.0, 0.0, -20.0}, VCLASS_NONE}); /* Good default. */
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
      verts.append({{0.0f, 0.0f, 0.0f}, VCLASS_NONE});
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
  /* field_force */
  {
    constexpr int circle_resol = 32;
    constexpr VertexClass flag = VCLASS_EMPTY_SIZE | VCLASS_SCREENALIGNED;
    constexpr std::array<float, 2> scales{2.0f, 0.75};
    Vector<float2> ring = ring_vertices(1.0f, circle_resol);

    Vector<Vertex> verts;

    append_line_loop(verts, ring, 0.0f, flag);
    for (const float scale : scales) {
      for (float2 &point : ring) {
        point *= scale;
      }
      append_line_loop(verts, ring, 0.0f, flag);
    }

    field_force = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* field_wind */
  {
    constexpr int circle_resol = 32;
    const Vector<float2> ring = ring_vertices(1.0f, circle_resol);

    Vector<Vertex> verts;

    for (const int i : IndexRange(4)) {
      const float z = 0.05f * float(i);
      append_line_loop(verts, ring, z, VCLASS_EMPTY_SIZE);
    }

    field_wind = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* field_vortex */
  {
    constexpr int spiral_resol = 32;
    const Vector<float2> ring = ring_vertices(1.0f, spiral_resol);

    Vector<Vertex> verts;

    for (const int i : IndexRange(ring.size() * 2 + 1)) {
      /* r: [-1, .., 0, .., 1] */
      const float r = (i - spiral_resol) / float(spiral_resol);
      /* index: [9, spiral_resol - 1, spiral_resol - 2, .., 2, 1, 0, 1, 2, .., spiral_resol - 1, 0]
       */
      const float2 point = ring[abs(spiral_resol - i) % spiral_resol] * r;
      verts.append({float3(point.y, point.x, 0.0f), VCLASS_EMPTY_SIZE});
    }
    field_vortex = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* field_curve */
  {
    constexpr int circle_resol = 32;
    const Vector<float2> ring = ring_vertices(1.0f, circle_resol);

    Vector<Vertex> verts;

    append_line_loop(verts, ring, 0.0f, VCLASS_EMPTY_SIZE | VCLASS_SCREENALIGNED);

    field_curve = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* field_sphere_limit */
  {
    constexpr int circle_resol = 32 * 2;
    const Vector<float2> ring = ring_vertices(1.0f, circle_resol);

    Vector<Vertex> verts;

    append_line_loop(verts, ring, 0.0f, VCLASS_EMPTY_SIZE | VCLASS_SCREENALIGNED, true);

    field_sphere_limit = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* field_tube_limit */
  {
    constexpr int circle_resol = 32;
    constexpr int side_stipple = 32;
    const Vector<float2> ring = ring_vertices(1.0f, circle_resol);
    const Vector<float2> diamond = ring_vertices(1.0f, 4);

    Vector<Vertex> verts;

    /* Caps */
    for (const int i : IndexRange(2)) {
      const float z = i * 2.0f - 1.0f;
      append_line_loop(verts, ring, z, VCLASS_EMPTY_SIZE, true);
    }
    /* Side Edges */
    for (const float2 &point : diamond) {
      for (const int i : IndexRange(side_stipple)) {
        const float z = (i / float(side_stipple)) * 2.0f - 1.0f;
        verts.append({float3(point.y, point.x, z), VCLASS_EMPTY_SIZE});
      }
    }

    field_tube_limit = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* field_cone_limit */
  {
    constexpr int circle_resol = 32;
    constexpr int side_stipple = 32;
    const Vector<float2> ring = ring_vertices(1.0f, circle_resol);
    const Vector<float2> diamond = ring_vertices(1.0f, 4);

    Vector<Vertex> verts;

    /* Caps */
    for (const int i : IndexRange(2)) {
      const float z = i * 2.0f - 1.0f;
      append_line_loop(verts, ring, z, VCLASS_EMPTY_SIZE, true);
    }
    /* Side Edges */
    for (const float2 &point : diamond) {
      for (const int i : IndexRange(side_stipple)) {
        const float z = (i / float(side_stipple)) * 2.0f - 1.0f;
        verts.append({float3(point.y * z, point.x * z, z), VCLASS_EMPTY_SIZE});
      }
    }

    field_cone_limit = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* lightprobe_cube */
  {
    constexpr float r = 14.0f;
    constexpr VertexClass flag = VCLASS_SCREENSPACE;
    /* Icon */
    constexpr float sin_pi_3 = 0.86602540378f;
    constexpr float cos_pi_3 = 0.5f;
    const Array<float2, 6> points = {
        float2(0.0f, 1.0f) * r,
        float2(sin_pi_3, cos_pi_3) * r,
        float2(sin_pi_3, -cos_pi_3) * r,
        float2(0.0f, -1.0f) * r,
        float2(-sin_pi_3, -cos_pi_3) * r,
        float2(-sin_pi_3, cos_pi_3) * r,
    };

    Vector<Vertex> verts;

    append_line_loop(verts, points, 0.0f, flag);
    for (const int i : IndexRange(3)) {
      const float2 &point = points[i * 2 + 1];
      verts.append(Vertex{{point, 0.0f}, flag});
      verts.append(Vertex{{0.0f, 0.0f, 0.0f}, flag});
    }

    /* Direction Lines */
    const Vector<float2> diamond = ring_vertices(1.2f, diamond_nsegments);
    const std::string axes = "zZyYxX";
    for (const char axis : axes) {
      light_append_direction_line(axis, diamond, verts);
    }

    lightprobe_cube = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* lightprobe_planar */
  {
    constexpr float r = 20.0f;
    /* Icon */
    constexpr float sin_pi_3 = 0.86602540378f;
    const Array<float2, 4> points = {
        float2(0.0f, 0.5f) * r,
        float2(sin_pi_3, 0.0f) * r,
        float2(0.0f, -0.5f) * r,
        float2(-sin_pi_3, 0.0f) * r,
    };

    Vector<Vertex> verts;

    append_line_loop(verts, points, 0.0f, VCLASS_SCREENSPACE);
    lightprobe_planar = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* lightprobe_grid */
  {
    constexpr float r = 14.0f;
    constexpr VertexClass flag = VCLASS_SCREENSPACE;
    /* Icon */
    constexpr float sin_pi_3 = 0.86602540378f;
    constexpr float cos_pi_3 = 0.5f;
    const Array<float2, 6> points = {float2(0.0f, 1.0f) * r,
                                     float2(sin_pi_3, cos_pi_3) * r,
                                     float2(sin_pi_3, -cos_pi_3) * r,
                                     float2(0.0f, -1.0f) * r,
                                     float2(-sin_pi_3, -cos_pi_3) * r,
                                     float2(-sin_pi_3, cos_pi_3) * r};
    Vector<Vertex> verts;

    append_line_loop(verts, points, 0.0f, flag);
    /* Internal wires. */
    for (const int i : IndexRange(6)) {
      const float2 tr = points[(i / 2) * 2 + 1] * -0.5f;
      const float2 t1 = points[i] + tr;
      const float2 t2 = points[(i + 1) % 6] + tr;
      verts.append({{t1, 0.0f}, flag});
      verts.append({{t2, 0.0f}, flag});
    }
    for (const int i : IndexRange(3)) {
      const float2 &point = points[i * 2 + 1];
      verts.append(Vertex{{point, 0.0f}, flag});
      verts.append(Vertex{{0.0f, 0.0f, 0.0f}, flag});
    }
    /* Direction Lines */
    const Vector<float2> diamond = ring_vertices(1.2f, diamond_nsegments);
    const std::string axes = "zZyYxX";
    for (const char axis : axes) {
      light_append_direction_line(axis, diamond, verts);
    }

    lightprobe_grid = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* grid */
  {
    constexpr int resolution = 8;
    std::array<float, resolution + 1> steps;
    /* [-1, 1] divided into "resolution" steps. */
    for (const int i : IndexRange(resolution + 1)) {
      steps[i] = -1.0f + float(i * 2) / resolution;
    }

    Vector<Vertex> verts;
    verts.reserve(resolution * resolution * 6);
    for (const int x : IndexRange(resolution)) {
      for (const int y : IndexRange(resolution)) {
        verts.append(Vertex{{steps[x], steps[y], 0.0f}});
        verts.append(Vertex{{steps[x + 1], steps[y], 0.0f}});
        verts.append(Vertex{{steps[x], steps[y + 1], 0.0f}});

        verts.append(Vertex{{steps[x], steps[y + 1], 0.0f}});
        verts.append(Vertex{{steps[x + 1], steps[y], 0.0f}});
        verts.append(Vertex{{steps[x + 1], steps[y + 1], 0.0f}});
      }
    }
    grid = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* cursor circle */
  {
    const int segments = 12;
    const float radius = 0.5f;
    const float color_primary[3] = {1.0f, 0.0f, 0.0f};
    const float color_secondary[3] = {1.0f, 1.0f, 1.0f};

    Vector<VertexWithColor> verts;

    for (int i = 0; i < segments + 1; i++) {
      float angle = float(2 * M_PI) * (float(i) / float(segments));
      verts.append({radius * float3(cosf(angle), sinf(angle), 0.0f),
                    (i % 2 == 0) ? color_secondary : color_primary});
    }

    cursor_circle = BatchPtr(GPU_batch_create_ex(
        GPU_PRIM_LINE_STRIP, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
  /* cursor lines */
  {
    const float outer_limit = 1.0f;
    const float color_limit = 0.85f;
    const float inner_limit = 0.25f;
    const std::array<int, 3> axis_theme = {TH_AXIS_X, TH_AXIS_Y, TH_AXIS_Z};

    float crosshair_color[3];

    Vector<VertexWithColor> verts;

    for (int i = 0; i < 3; i++) {
      float3 axis(0.0f);
      axis[i] = 1.0f;
      /* Draw the positive axes. */
      UI_GetThemeColor3fv(axis_theme[i], crosshair_color);
      verts.append({outer_limit * axis, crosshair_color});
      verts.append({color_limit * axis, crosshair_color});

      /* Inner crosshair. */
      UI_GetThemeColor3fv(TH_VIEW_OVERLAY, crosshair_color);
      verts.append({color_limit * axis, crosshair_color});
      verts.append({inner_limit * axis, crosshair_color});

      /* Draw the negative axis a little darker and desaturated. */
      axis[i] = -1.0f;
      UI_GetThemeColorBlendShade3fv(axis_theme[i], TH_WHITE, .25f, -60, crosshair_color);
      verts.append({outer_limit * axis, crosshair_color});
      verts.append({color_limit * axis, crosshair_color});

      /* Inner crosshair. */
      UI_GetThemeColor3fv(TH_VIEW_OVERLAY, crosshair_color);
      verts.append({color_limit * axis, crosshair_color});
      verts.append({inner_limit * axis, crosshair_color});
    }

    cursor_lines = BatchPtr(
        GPU_batch_create_ex(GPU_PRIM_LINES, vbo_from_vector(verts), nullptr, GPU_BATCH_OWNS_VBO));
  }
}

}  // namespace blender::draw::overlay
