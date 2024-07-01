/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"

#include "BKE_ccg.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "editors/sculpt_paint/sculpt_intern.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>

namespace blender::ed::sculpt_paint {

struct MultiplaneScrapeSampleData {
  std::array<float3, 2> area_cos;
  std::array<float3, 2> area_nos;
  std::array<int, 2> area_count;
};

static void calc_multiplane_scrape_surface_task(Object &object,
                                                const Brush &brush,
                                                const float4x4 &mat,
                                                PBVHNode &node,
                                                MultiplaneScrapeSampleData &sample)
{
  SculptSession &ss = *object.sculpt;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  /* Apply the brush normal radius to the test before sampling. */
  float test_radius = sqrtf(test.radius_squared);
  test_radius *= brush.normal_radius_factor;
  test.radius_squared = test_radius * test_radius;

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      object, ss.cache->automasking.get(), node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, &node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }
    const float3 local_co = math::transform_point(mat, float3(vd.co));
    const float3 normal = vd.no;

    auto_mask::node_update(automask_data, vd);

    /* Use the brush falloff to weight the sampled normals. */
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);

    /* Sample the normal and area of the +X and -X axis individually. */
    const bool plane_index = local_co[0] <= 0.0f;
    sample.area_nos[plane_index] += normal * fade;
    sample.area_cos[plane_index] += vd.co;
    sample.area_count[plane_index]++;
    BKE_pbvh_vertex_iter_end;
  }
}

static void do_multiplane_scrape_brush_task(Object &object,
                                            const Brush &brush,
                                            const float4x4 &mat,
                                            const std::array<float4, 2> &scrape_planes,
                                            const float angle,
                                            PBVHNode *node)
{
  SculptSession &ss = *object.sculpt;

  PBVHVertexIter vd;
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, *node).co;
  const float bstrength = fabsf(ss.cache->bstrength);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      object, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(test, vd.co)) {
      continue;
    }

    float3 local_co = math::transform_point(mat, float3(vd.co));
    const bool plane_index = local_co[0] <= 0.0f;

    bool deform = false;
    deform = !SCULPT_plane_point_side(vd.co, scrape_planes[plane_index]);

    if (angle < 0.0f) {
      deform = true;
    }

    if (!deform) {
      continue;
    }

    float3 intr;

    closest_to_plane_normalized_v3(intr, scrape_planes[plane_index], vd.co);

    float3 translation = intr - float3(vd.co);
    if (!SCULPT_plane_trim(*ss.cache, brush, translation)) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);

    /* Deform the local space along the Y axis to avoid artifacts on curved strokes. */
    /* This produces a not round brush tip. */
    local_co[1] *= 2.0f;
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                len_v3(local_co),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    mul_v3_v3fl(proxy[vd.i], translation, fade);
  }
  BKE_pbvh_vertex_iter_end;
}

void do_multiplane_scrape_brush(const Sculpt &sd, Object &object, const Span<PBVHNode *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  const bool flip = (ss.cache->bstrength < 0.0f);
  const float radius = flip ? -ss.cache->radius : ss.cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = -radius * offset;

  float3 area_no_sp;
  float3 area_co;
  calc_brush_plane(brush, object, nodes, area_no_sp, area_co);

  float3 area_no;
  if (brush.sculpt_plane != SCULPT_DISP_DIR_AREA || (brush.flag & BRUSH_ORIGINAL_NORMAL)) {
    area_no = calc_area_normal(brush, object, nodes).value_or(float3(0));
  }
  else {
    area_no = area_no_sp;
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    ss.cache->multiplane_scrape_angle = 0.0f;
    return;
  }

  if (is_zero_v3(ss.cache->grab_delta_symmetry)) {
    return;
  }

  area_co = area_no_sp * ss.cache->scale * displace;

  /* Init brush local space matrix. */
  float4x4 mat = float4x4::identity();
  mat.x_axis() = math::cross(area_no, ss.cache->grab_delta_symmetry);
  mat.y_axis() = math::cross(area_no, mat.x_axis());
  mat.z_axis() = area_no;
  mat.location() = ss.cache->location;
  /* NOTE: #math::normalize behaves differently for some reason. */
  normalize_m4(mat.ptr());
  mat = math::invert(mat);

  /* Update matrix for the cursor preview. */
  if (ss.cache->mirror_symmetry_pass == 0 && ss.cache->radial_symmetry_pass == 0) {
    ss.cache->stroke_local_mat = mat;
  }

  /* Dynamic mode. */

  if (brush.flag2 & BRUSH_MULTIPLANE_SCRAPE_DYNAMIC) {
    /* Sample the individual normal and area center of the two areas at both sides of the cursor.
     */
    const MultiplaneScrapeSampleData sample = threading::parallel_reduce(
        nodes.index_range(),
        1,
        MultiplaneScrapeSampleData{},
        [&](const IndexRange range, MultiplaneScrapeSampleData sample) {
          for (const int i : range) {
            calc_multiplane_scrape_surface_task(object, brush, mat, *nodes[i], sample);
          }
          return sample;
        },
        [](const MultiplaneScrapeSampleData &a, const MultiplaneScrapeSampleData &b) {
          MultiplaneScrapeSampleData joined = a;

          joined.area_cos[0] = a.area_cos[0] + b.area_cos[0];
          joined.area_cos[1] = a.area_cos[1] + b.area_cos[1];

          joined.area_nos[0] = a.area_nos[0] + b.area_nos[0];
          joined.area_nos[1] = a.area_nos[1] + b.area_nos[1];

          joined.area_count[0] = a.area_count[0] + b.area_count[0];
          joined.area_count[1] = a.area_count[1] + b.area_count[1];
          return joined;
        });

    /* Use the area center of both planes to detect if we are sculpting along a concave or convex
     * edge. */
    const std::array<float3, 2> sampled_plane_co{
        sample.area_cos[0] * 1.0f / float(sample.area_count[0]),
        sample.area_cos[1] * 1.0f / float(sample.area_count[1])};
    const float3 mid_co = math::midpoint(sampled_plane_co[0], sampled_plane_co[1]);

    /* Calculate the scrape planes angle based on the sampled normals. */
    const std::array<float3, 2> sampled_plane_normals{
        math::normalize(sample.area_nos[0] * 1.0f / float(sample.area_count[0])),
        math::normalize(sample.area_nos[1] * 1.0f / float(sample.area_count[1]))};

    float sampled_angle = angle_v3v3(sampled_plane_normals[0], sampled_plane_normals[1]);
    const std::array<float3, 2> sampled_cv{area_no, ss.cache->location - mid_co};

    sampled_angle += DEG2RADF(brush.multiplane_scrape_angle) * ss.cache->pressure;

    /* Invert the angle if we are sculpting along a concave edge. */
    if (math::dot(sampled_cv[0], sampled_cv[1]) < 0.0f) {
      sampled_angle = -sampled_angle;
    }

    /* In dynamic mode, set the angle to 0 when inverting the brush, so you can trim plane
     * surfaces without changing the brush. */
    if (flip) {
      sampled_angle = 0.0f;
    }
    else {
      area_co = ss.cache->location;
    }

    /* Interpolate between the previous and new sampled angles to avoid artifacts when if angle
     * difference between two samples is too big. */
    ss.cache->multiplane_scrape_angle = math::interpolate(
        RAD2DEGF(sampled_angle), ss.cache->multiplane_scrape_angle, 0.2f);
  }
  else {
    /* Standard mode: Scrape with the brush property fixed angle. */
    area_co = ss.cache->location;
    ss.cache->multiplane_scrape_angle = brush.multiplane_scrape_angle;
    if (flip) {
      ss.cache->multiplane_scrape_angle *= -1.0f;
    }
  }

  /* Calculate the final left and right scrape planes. */
  float3 plane_no;
  float3 plane_no_rot;
  const float3 y_axis(0.0f, 1.0f, 0.0f);
  const float4x4 mat_inv = math::invert(mat);

  std::array<float4, 2> multiplane_scrape_planes;

  mul_v3_mat3_m4v3(plane_no, mat.ptr(), area_no);
  rotate_v3_v3v3fl(
      plane_no_rot, plane_no, y_axis, DEG2RADF(-ss.cache->multiplane_scrape_angle * 0.5f));
  mul_v3_mat3_m4v3(plane_no, mat_inv.ptr(), plane_no_rot);
  normalize_v3(plane_no);
  plane_from_point_normal_v3(multiplane_scrape_planes[1], area_co, plane_no);

  mul_v3_mat3_m4v3(plane_no, mat.ptr(), area_no);
  rotate_v3_v3v3fl(
      plane_no_rot, plane_no, y_axis, DEG2RADF(ss.cache->multiplane_scrape_angle * 0.5f));
  mul_v3_mat3_m4v3(plane_no, mat_inv.ptr(), plane_no_rot);
  normalize_v3(plane_no);
  plane_from_point_normal_v3(multiplane_scrape_planes[0], area_co, plane_no);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_multiplane_scrape_brush_task(object,
                                      brush,
                                      mat,
                                      multiplane_scrape_planes,
                                      ss.cache->multiplane_scrape_angle,
                                      nodes[i]);
    }
  });
}

void multiplane_scrape_preview_draw(const uint gpuattr,
                                    const Brush &brush,
                                    const SculptSession &ss,
                                    const float outline_col[3],
                                    const float outline_alpha)
{
  if (!(brush.flag2 & BRUSH_MULTIPLANE_SCRAPE_PLANES_PREVIEW)) {
    return;
  }

  float4x4 local_mat_inv = math::invert(ss.cache->stroke_local_mat);
  GPU_matrix_mul(local_mat_inv.ptr());
  float angle = ss.cache->multiplane_scrape_angle;
  if (ss.cache->pen_flip || ss.cache->invert) {
    angle = -angle;
  }

  float offset = ss.cache->radius * 0.25f;

  const float3 p{0.0f, 0.0f, ss.cache->radius};
  const float3 y_axis{0.0f, 1.0f, 0.0f};
  float3 p_l;
  float3 p_r;
  const float3 area_center(0);
  rotate_v3_v3v3fl(p_r, p, y_axis, DEG2RADF((angle + 180) * 0.5f));
  rotate_v3_v3v3fl(p_l, p, y_axis, DEG2RADF(-(angle + 180) * 0.5f));

  immBegin(GPU_PRIM_LINES, 14);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] + offset, p_r[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] + offset, p_l[2]);

  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] - offset, p_r[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] - offset, p_l[2]);

  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);

  immVertex3f(gpuattr, p_r[0], p_r[1] - offset, p_r[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] + offset, p_r[2]);

  immVertex3f(gpuattr, p_l[0], p_l[1] - offset, p_l[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] + offset, p_l[2]);

  immEnd();

  immUniformColor3fvAlpha(outline_col, outline_alpha * 0.1f);
  immBegin(GPU_PRIM_TRIS, 12);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] + offset, p_r[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] - offset, p_r[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, p_r[0], p_r[1] - offset, p_r[2]);

  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] + offset, p_l[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] - offset, p_l[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] + offset, area_center[2]);
  immVertex3f(gpuattr, area_center[0], area_center[1] - offset, area_center[2]);
  immVertex3f(gpuattr, p_l[0], p_l[1] - offset, p_l[2]);

  immEnd();
}

}  // namespace blender::ed::sculpt_paint
