/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_index_buffer.hh"
#include "GPU_ray_tracing.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_vertex_buffer.hh"

#include "gpu_testing.hh"

#include "BLI_math_matrix.hh"
namespace blender::gpu::tests {

#define SUPPORTS_RAY_QUERY() \
  if (!GPU_ray_query_support()) { \
    GTEST_SKIP() << "Platform doesn't support ray tracing"; \
  }

static VertBufPtr build_vertices()
{
  /* Build a cube (vertex + index buffer). */
  static const GPUVertFormat pos_format = GPU_vertformat_from_attribute(
      "pos", gpu::VertAttrType::SFLOAT_32_32_32);
  VertBufPtr vertex_buf(GPU_vertbuf_create_with_format(pos_format));
  GPU_vertbuf_data_alloc(*vertex_buf, 8);
  MutableSpan<float3> vertex_buf_data = vertex_buf->data<float3>();
  vertex_buf_data[0] = float3(-1.0, -1.0, -1.0);
  vertex_buf_data[1] = float3(1.0, -1.0, -1.0);
  vertex_buf_data[2] = float3(-1.0, 1.0, -1.0);
  vertex_buf_data[3] = float3(1.0, 1.0, -1.0);
  vertex_buf_data[4] = float3(-1.0, -1.0, 1.0);
  vertex_buf_data[5] = float3(1.0, -1.0, 1.0);
  vertex_buf_data[6] = float3(-1.0, 1.0, 1.0);
  vertex_buf_data[7] = float3(1.0, 1.0, 1.0);
  GPU_vertbuf_use(vertex_buf.get());
  return vertex_buf;
}

static IndexBufPtr build_indices()
{
  IndexBufPtr index_buf(GPU_indexbuf_calloc());
  Span<uint32_t> indices_src({
      0, 1, 2, 2, 1, 3, 4, 6, 5, 5, 6, 7, 0, 2, 4, 4, 2, 6,
      1, 5, 3, 3, 5, 7, 2, 3, 6, 6, 3, 7, 0, 4, 1, 1, 4, 5,
  });
  uint32_t *indices = MEM_new_array<uint32_t>(indices_src.size(), __func__);
  memcpy(indices, indices_src.data(), indices_src.size() * sizeof(uint32_t));
  index_buf->init(indices_src.size(), indices, 0, 7, GPU_PRIM_TRIS, false);
  GPU_indexbuf_use(index_buf.get());
  return index_buf;
}

struct Ray {
  float3 pos;
  float3 dir;
  bool expected_hit;
};

static void hit_test(TopLevelAS &tlas, Span<Ray> rays, uint intersection_mask = 0xFF)
{
  const int ray_count = rays.size();

  Array<float4> ray_pos(ray_count);
  Array<float4> ray_dir(ray_count);
  Array<uint32_t> expected(ray_count);
  Array<uint32_t> result(ray_count, UINT32_MAX);
  for (int index : rays.index_range()) {
    const Ray &ray = rays[index];
    ray_pos[index] = float4(ray.pos, 0.0f);
    ray_dir[index] = float4(ray.dir, 0.0f);
    expected[index] = ray.expected_hit;
  }

  gpu::StorageBuf *ray_pos_buf = GPU_storagebuf_create(ray_count * sizeof(float4));
  gpu::StorageBuf *ray_dir_buf = GPU_storagebuf_create(ray_count * sizeof(float4));
  gpu::StorageBuf *hit_buf = GPU_storagebuf_create(ray_count * sizeof(uint32_t));
  GPU_storagebuf_update(ray_pos_buf, ray_pos.data());
  GPU_storagebuf_update(ray_dir_buf, ray_dir.data());
  GPU_storagebuf_clear_to_zero(hit_buf);

  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_ray_query_test");
  EXPECT_NE(shader, nullptr);
  int intersection_mask_in = GPU_shader_get_constant(shader, "intersection_mask_in");
  shader::SpecializationConstants constants = GPU_shader_get_default_constant_state(shader);
  constants.set_value(intersection_mask_in, intersection_mask);
  GPU_shader_bind(shader, &constants);
  GPU_storagebuf_bind(ray_pos_buf, 0);
  GPU_storagebuf_bind(ray_dir_buf, 1);
  GPU_storagebuf_bind(hit_buf, 2);
  tlas.bind(0);

  GPU_compute_dispatch(shader, ray_count, 1, 1, &constants);
  GPU_shader_unbind();

  GPU_storagebuf_read(hit_buf, result.data());

  GPU_storagebuf_free(ray_pos_buf);
  GPU_storagebuf_free(ray_dir_buf);
  GPU_storagebuf_free(hit_buf);
  GPU_shader_free(shader);

  EXPECT_EQ_SPAN(expected.as_span(), result.as_span());
}

static void test_ray_tracing_empty_tlas()
{
  SUPPORTS_RAY_QUERY()

  TopLevelASPtr tlas(GPU_ray_tracing_tlas_alloc(__func__));
  tlas->build();

  Vector<Ray> rays;
  /* All rays from inside the cube should hit the cube. */
  for (int x : IndexRange(19)) {
    for (int y : IndexRange(19)) {
      for (int z : IndexRange(19)) {
        float3 pos((x - 9) * 0.1f, (y - 9) * 0.1f, (z - 9) * 0.1f);
        rays.append({pos, float3(-1.0f, 0.0f, 0.0f), false});
        rays.append({pos, float3(1.0f, 0.0f, 0.0f), false});
        rays.append({pos, float3(0.0f, -1.0f, 0.0f), false});
        rays.append({pos, float3(0.0f, 1.0f, 0.0f), false});
        rays.append({pos, float3(0.0f, 0.0f, -1.0f), false});
        rays.append({pos, float3(0.0f, 0.0f, 1.0f), false});
      }
    }
  }

  hit_test(*tlas, rays);
}
GPU_TEST(ray_tracing_empty_tlas)

static void test_ray_tracing_inside_cube()
{
  SUPPORTS_RAY_QUERY()

  VertBufPtr vertex_buf = build_vertices();
  IndexBufPtr index_buf = build_indices();

  BottomLevelASPtr blas(GPU_ray_tracing_blas_alloc(__func__));
  blas->add_geometry(*index_buf, *vertex_buf);
  blas->build();

  TopLevelASPtr tlas(GPU_ray_tracing_tlas_alloc(__func__));
  float4x4 identity = float4x4::identity();
  tlas->add_instance(*blas, identity);
  tlas->build();

  Vector<Ray> rays;
  /* All rays from inside the cube should hit the cube. */
  for (int x : IndexRange(19)) {
    for (int y : IndexRange(19)) {
      for (int z : IndexRange(19)) {
        float3 pos((x - 9) * 0.1f, (y - 9) * 0.1f, (z - 9) * 0.1f);
        rays.append({pos, float3(-1.0f, 0.0f, 0.0f), true});
        rays.append({pos, float3(1.0f, 0.0f, 0.0f), true});
        rays.append({pos, float3(0.0f, -1.0f, 0.0f), true});
        rays.append({pos, float3(0.0f, 1.0f, 0.0f), true});
        rays.append({pos, float3(0.0f, 0.0f, -1.0f), true});
        rays.append({pos, float3(0.0f, 0.0f, 1.0f), true});
      }
    }
  }

  hit_test(*tlas, rays);
}
GPU_TEST(ray_tracing_inside_cube)

static void test_ray_tracing_instance_mask()
{
  SUPPORTS_RAY_QUERY()

  VertBufPtr vertex_buf = build_vertices();
  IndexBufPtr index_buf = build_indices();

  BottomLevelASPtr blas(GPU_ray_tracing_blas_alloc(__func__));
  blas->add_geometry(*index_buf, *vertex_buf);
  blas->build();

  TopLevelASPtr tlas(GPU_ray_tracing_tlas_alloc(__func__));
  float4x4 mat_x_neg = math::from_location<float4x4>(float3(-2.0f, 0.0f, 0.0f));
  float4x4 mat_x_pos = math::from_location<float4x4>(float3(2.0f, 0.0f, 0.0f));
  float4x4 mat_y_neg = math::from_location<float4x4>(float3(0.0f, -2.0f, 0.0f));
  float4x4 mat_y_pos = math::from_location<float4x4>(float3(0.0f, 2.0f, 0.0f));
  float4x4 mat_z_neg = math::from_location<float4x4>(float3(0.0f, 0.0f, -2.0f));
  float4x4 mat_z_pos = math::from_location<float4x4>(float3(0.0f, 0.0f, 2.0f));
  tlas->add_instance(*blas, mat_x_neg, 0x01);
  tlas->add_instance(*blas, mat_x_pos, 0x02);
  tlas->add_instance(*blas, mat_y_neg, 0x04);
  tlas->add_instance(*blas, mat_y_pos, 0x08);
  tlas->add_instance(*blas, mat_z_neg, 0x10);
  tlas->add_instance(*blas, mat_z_pos, 0x20);
  tlas->build();

  Vector<Ray> rays;
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), true});
  hit_test(*tlas, rays, 0xFF);

  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays, 0x01);

  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays, 0x02);

  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays, 0x04);

  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays, 0x08);

  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays, 0x10);

  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), true});
  hit_test(*tlas, rays, 0x20);

  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays, 0x40);
}
GPU_TEST(ray_tracing_instance_mask)

static void test_ray_tracing_instance_update()
{
  SUPPORTS_RAY_QUERY()

  VertBufPtr vertex_buf = build_vertices();
  IndexBufPtr index_buf = build_indices();

  BottomLevelASPtr blas(GPU_ray_tracing_blas_alloc(__func__));
  blas->add_geometry(*index_buf, *vertex_buf);
  blas->build();

  TopLevelASPtr tlas(GPU_ray_tracing_tlas_alloc(__func__));
  float4x4 mat_x_neg = math::from_location<float4x4>(float3(-2.0f, 0.0f, 0.0f));
  float4x4 mat_x_pos = math::from_location<float4x4>(float3(2.0f, 0.0f, 0.0f));
  float4x4 mat_y_neg = math::from_location<float4x4>(float3(0.0f, -2.0f, 0.0f));
  float4x4 mat_y_pos = math::from_location<float4x4>(float3(0.0f, 2.0f, 0.0f));
  float4x4 mat_z_neg = math::from_location<float4x4>(float3(0.0f, 0.0f, -2.0f));
  float4x4 mat_z_pos = math::from_location<float4x4>(float3(0.0f, 0.0f, 2.0f));
  InstanceID instance_id = tlas->add_instance(*blas, mat_x_neg);
  tlas->build();

  Vector<Ray> rays;
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays);

  tlas->update_instance(instance_id, mat_x_pos);
  tlas->build();
  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays);

  tlas->update_instance(instance_id, mat_y_neg);
  tlas->build();
  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays);

  tlas->update_instance(instance_id, mat_y_pos);
  tlas->build();
  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays);

  tlas->update_instance(instance_id, mat_z_neg);
  tlas->build();
  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), true});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), false});
  hit_test(*tlas, rays);

  tlas->update_instance(instance_id, mat_z_pos);
  tlas->build();
  rays.clear();
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(-1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, -1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, -1.0f), false});
  rays.append({float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), true});
  hit_test(*tlas, rays);
}
GPU_TEST(ray_tracing_instance_update)

}  // namespace blender::gpu::tests
