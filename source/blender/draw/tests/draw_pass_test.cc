/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "GPU_context.hh"

#include "draw_cache.hh"
#include "draw_manager.hh"
#include "draw_pass.hh"
#include "draw_shader.hh"
#include "draw_testing.hh"

#include <bitset>

namespace blender::draw {

static void test_draw_pass_all_commands()
{
  Texture tex;
  tex.ensure_2d(blender::gpu::TextureFormat::UNORM_16_16_16_16, int2(1));

  UniformBuffer<uint4> ubo;
  ubo.push_update();

  StorageBuffer<uint4> ssbo;
  ssbo.push_update();

  /* Won't be dereferenced. */
  gpu::VertBuf *vbo = (gpu::VertBuf *)1;
  gpu::IndexBuf *ibo = (gpu::IndexBuf *)1;
  gpu::FrameBuffer *fb = nullptr;

  float4 color(1.0f, 1.0f, 1.0f, 0.0f);
  int3 dispatch_size(1);

  PassSimple pass = {"test.all_commands"};
  pass.init();
  pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_STENCIL);
  pass.clear_color_depth_stencil(float4(0.25f, 0.5f, 100.0f, -2000.0f), 0.5f, 0xF0);
  pass.state_stencil(0x80, 0x0F, 0x8F);
  gpu::Shader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE_COLOR);
  const int color_location = GPU_shader_get_uniform(sh, "color");
  const int mvp_location = GPU_shader_get_uniform(sh, "ModelViewProjectionMatrix");
  pass.shader_set(sh);
  pass.framebuffer_set(&fb);
  pass.subpass_transition(GPU_ATTACHMENT_IGNORE, {GPU_ATTACHMENT_WRITE, GPU_ATTACHMENT_READ});
  pass.bind_texture("image", tex);
  pass.bind_texture("image", &tex);
  pass.bind_image("missing_image", tex);       /* Should not crash. */
  pass.bind_image("missing_image", &tex);      /* Should not crash. */
  pass.bind_ubo("missing_ubo", ubo);           /* Should not crash. */
  pass.bind_ubo("missing_ubo", &ubo);          /* Should not crash. */
  pass.bind_ssbo("missing_ssbo", ssbo);        /* Should not crash. */
  pass.bind_ssbo("missing_ssbo", &ssbo);       /* Should not crash. */
  pass.bind_ssbo("missing_vbo_as_ssbo", vbo);  /* Should not crash. */
  pass.bind_ssbo("missing_vbo_as_ssbo", &vbo); /* Should not crash. */
  pass.bind_ssbo("missing_ibo_as_ssbo", ibo);  /* Should not crash. */
  pass.bind_ssbo("missing_ibo_as_ssbo", &ibo); /* Should not crash. */
  pass.push_constant("color", color);
  pass.push_constant("color", &color);
  pass.push_constant("ModelViewProjectionMatrix", float4x4::identity());
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  /* Should not crash even if shader is not a compute. This is because we only serialize. */
  /* TODO(fclem): Use real compute shader. */
  pass.shader_set(GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE_COLOR));
  pass.dispatch(dispatch_size);
  pass.dispatch(&dispatch_size);
  pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);

  /* Change references. */
  color[3] = 1.0f;
  dispatch_size = int3(2);

  std::string result = pass.serialize();
  std::stringstream expected;
  expected << ".test.all_commands" << std::endl;
  expected << "  .state_set(2147483654)" << std::endl;
  expected << "  .clear(color=(0.25, 0.5, 100, -2000), depth=0.5, stencil=0b11110000))"
           << std::endl;
  expected
      << "  .stencil_set(write_mask=0b10000000, reference=0b00001111, compare_mask=0b10001111)"
      << std::endl;
  expected << "  .shader_bind(gpu_shader_3D_image_color)" << std::endl;
  expected << "  .framebuffer_bind(nullptr)" << std::endl;
  expected << "  .subpass_transition(" << std::endl;
  expected << "depth=ignore," << std::endl;
  expected << "color0=write," << std::endl;
  expected << "color1=read," << std::endl;
  expected << "color2=ignore," << std::endl;
  expected << "color3=ignore," << std::endl;
  expected << "color4=ignore," << std::endl;
  expected << "color5=ignore," << std::endl;
  expected << "color6=ignore," << std::endl;
  expected << "color7=ignore" << std::endl;
  expected << ")" << std::endl;
  expected << "  .bind_texture(0, sampler=internal)" << std::endl;
  expected << "  .bind_texture_ref(0, sampler=internal)" << std::endl;
  expected << "  .bind_image(-1)" << std::endl;
  expected << "  .bind_image_ref(-1)" << std::endl;
  expected << "  .bind_uniform_buf(-1)" << std::endl;
  expected << "  .bind_uniform_buf_ref(-1)" << std::endl;
  expected << "  .bind_storage_buf(-1)" << std::endl;
  expected << "  .bind_storage_buf_ref(-1)" << std::endl;
  expected << "  .bind_vertbuf_as_ssbo(-1)" << std::endl;
  expected << "  .bind_vertbuf_as_ssbo_ref(-1)" << std::endl;
  expected << "  .bind_indexbuf_as_ssbo(-1)" << std::endl;
  expected << "  .bind_indexbuf_as_ssbo_ref(-1)" << std::endl;
  expected << "  .push_constant(" << color_location << ", data=(1, 1, 1, 0))" << std::endl;
  expected << "  .push_constant(" << color_location << ", data=(1, 1, 1, 1))" << std::endl;
  expected << "  .push_constant(" << mvp_location << ", data=(" << std::endl;
  expected << "(1, 0, 0, 0)," << std::endl;
  expected << "(0, 1, 0, 0)," << std::endl;
  expected << "(0, 0, 1, 0)," << std::endl;
  expected << "(0, 0, 0, 1)" << std::endl;
  expected << ")" << std::endl;
  expected << ")" << std::endl;
  expected << "  .draw(inst_len=1, vert_len=3, vert_first=0, res_id=0)" << std::endl;
  expected << "  .shader_bind(gpu_shader_3D_image_color)" << std::endl;
  expected << "  .dispatch(1, 1, 1)" << std::endl;
  expected << "  .dispatch_ref(2, 2, 2)" << std::endl;
  expected << "  .barrier(2)" << std::endl;

  EXPECT_EQ(result, expected.str());
}
DRAW_TEST(draw_pass_all_commands)

static void test_draw_pass_sub_ordering()
{
  PassSimple pass = {"test.sub_ordering"};
  pass.init();
  pass.shader_set(GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE_COLOR));
  pass.push_constant("test_pass", 1);

  PassSimple::Sub &sub1 = pass.sub("Sub1");
  sub1.push_constant("test_sub1", 11);

  PassSimple::Sub &sub2 = pass.sub("Sub2");
  sub2.push_constant("test_sub2", 21);

  /* Will execute after both sub. */
  pass.push_constant("test_pass", 2);

  /* Will execute after sub1. */
  sub2.push_constant("test_sub2", 22);

  /* Will execute before sub2. */
  sub1.push_constant("test_sub1", 12);

  /* Will execute before end of pass. */
  sub2.push_constant("test_sub2", 23);

  std::string result = pass.serialize();
  std::stringstream expected;
  expected << ".test.sub_ordering" << std::endl;
  expected << "  .shader_bind(gpu_shader_3D_image_color)" << std::endl;
  expected << "  .push_constant(-1, data=1)" << std::endl;
  expected << "  .Sub1" << std::endl;
  expected << "    .push_constant(-1, data=11)" << std::endl;
  expected << "    .push_constant(-1, data=12)" << std::endl;
  expected << "  .Sub2" << std::endl;
  expected << "    .push_constant(-1, data=21)" << std::endl;
  expected << "    .push_constant(-1, data=22)" << std::endl;
  expected << "    .push_constant(-1, data=23)" << std::endl;
  expected << "  .push_constant(-1, data=2)" << std::endl;

  EXPECT_EQ(result, expected.str());
}
DRAW_TEST(draw_pass_sub_ordering)

static void test_draw_pass_simple_draw()
{
  PassSimple pass = {"test.simple_draw"};
  pass.init();
  pass.shader_set(GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE_COLOR));
  /* Each draw procedural type uses a different batch. Groups are drawn in correct order. */
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 10, 1, {1});
  pass.draw_procedural(GPU_PRIM_POINTS, 4, 20, 2, {2});
  pass.draw_procedural(GPU_PRIM_TRIS, 2, 30, 3, {3});
  pass.draw_procedural(GPU_PRIM_POINTS, 5, 40, 4, ResourceIndex(4, true));
  pass.draw_procedural(GPU_PRIM_LINES, 1, 50, 5, {5});
  pass.draw_procedural(GPU_PRIM_POINTS, 6, 60, 6, {5});
  pass.draw_procedural(GPU_PRIM_TRIS, 3, 70, 7, {6});

  PassSimple::Sub &sub = pass.sub("sub");
  sub.draw_procedural(GPU_PRIM_TRIS, 3, 80, 8, {8});

  std::string result = pass.serialize();
  std::stringstream expected;
  expected << ".test.simple_draw" << std::endl;
  expected << "  .shader_bind(gpu_shader_3D_image_color)" << std::endl;
  expected << "  .draw(inst_len=1, vert_len=10, vert_first=1, res_id=1)" << std::endl;
  expected << "  .draw(inst_len=4, vert_len=20, vert_first=2, res_id=2)" << std::endl;
  expected << "  .draw(inst_len=2, vert_len=30, vert_first=3, res_id=3)" << std::endl;
  expected << "  .draw(inst_len=5, vert_len=40, vert_first=4, res_id=4)" << std::endl;
  expected << "  .draw(inst_len=1, vert_len=50, vert_first=5, res_id=5)" << std::endl;
  expected << "  .draw(inst_len=6, vert_len=60, vert_first=6, res_id=5)" << std::endl;
  expected << "  .draw(inst_len=3, vert_len=70, vert_first=7, res_id=6)" << std::endl;
  expected << "  .sub" << std::endl;
  expected << "    .draw(inst_len=3, vert_len=80, vert_first=8, res_id=8)" << std::endl;

  EXPECT_EQ(result, expected.str());
}
DRAW_TEST(draw_pass_simple_draw)

static void test_draw_pass_multi_draw()
{
  PassMain pass = {"test.multi_draw"};
  pass.init();
  pass.shader_set(GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE_COLOR));
  /* Each draw procedural type uses a different batch. Groups are drawn in reverse order. */
  pass.draw_procedural(GPU_PRIM_TRIS, 1, -1, -1, {1});
  pass.draw_procedural(GPU_PRIM_POINTS, 4, -1, -1, {2});
  pass.draw_procedural(GPU_PRIM_TRIS, 2, -1, -1, {3});
  pass.draw_procedural(GPU_PRIM_POINTS, 5, -1, -1, ResourceIndex(4, true));
  pass.draw_procedural(GPU_PRIM_LINES, 1, -1, -1, {5});
  pass.draw_procedural(GPU_PRIM_POINTS, 6, -1, -1, {5});
  pass.draw_procedural(GPU_PRIM_TRIS, 3, -1, -1, {6});
  /* Custom calls should use their own group and never be batched. */
  pass.draw_procedural(GPU_PRIM_TRIS, 2, 2, 2, {7});
  pass.draw_procedural(GPU_PRIM_TRIS, 2, 2, 2, {8});

  std::string result = pass.serialize();
  std::stringstream expected;
  expected << ".test.multi_draw" << std::endl;
  expected << "  .shader_bind(gpu_shader_3D_image_color)" << std::endl;
  expected << "  .draw_multi(5)" << std::endl;
  expected << "    .group(id=4, len=2)" << std::endl;
  expected << "      .proto(instance_len=2, resource_id=8, front_face)" << std::endl;
  expected << "    .group(id=3, len=2)" << std::endl;
  expected << "      .proto(instance_len=2, resource_id=7, front_face)" << std::endl;
  expected << "    .group(id=2, len=1)" << std::endl;
  expected << "      .proto(instance_len=1, resource_id=5, front_face)" << std::endl;
  expected << "    .group(id=1, len=15)" << std::endl;
  expected << "      .proto(instance_len=5, resource_id=4, back_face)" << std::endl;
  expected << "      .proto(instance_len=6, resource_id=5, front_face)" << std::endl;
  expected << "      .proto(instance_len=4, resource_id=2, front_face)" << std::endl;
  expected << "    .group(id=0, len=6)" << std::endl;
  expected << "      .proto(instance_len=3, resource_id=6, front_face)" << std::endl;
  expected << "      .proto(instance_len=2, resource_id=3, front_face)" << std::endl;
  expected << "      .proto(instance_len=1, resource_id=1, front_face)" << std::endl;

  EXPECT_EQ(result, expected.str());
}
DRAW_TEST(draw_pass_multi_draw)

static void test_draw_pass_sortable()
{
  PassSortable pass = {"test.sortable"};
  pass.init();

  pass.sub("Sub3", 3.0f);
  pass.sub("Sub2", 2.0f);
  pass.sub("Sub5", 4.0f);
  pass.sub("Sub4", 3.0f);
  pass.sub("Sub1", 1.0f);

  std::string result = pass.serialize();
  std::stringstream expected;
  expected << ".test.sortable" << std::endl;
  expected << "  .Sub1" << std::endl;
  expected << "  .Sub2" << std::endl;
  expected << "  .Sub3" << std::endl;
  expected << "  .Sub4" << std::endl;
  expected << "  .Sub5" << std::endl;

  EXPECT_EQ(result, expected.str());
}
DRAW_TEST(draw_pass_sortable)

static void test_draw_resource_id_gen()
{
  GPU_render_begin();
  Texture color_attachment;
  Framebuffer framebuffer;
  color_attachment.ensure_2d(blender::gpu::TextureFormat::SFLOAT_32_32_32_32, int2(1));
  framebuffer.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(color_attachment));
  framebuffer.bind();

  float4x4 win_mat;
  orthographic_m4(win_mat.ptr(), -1, 1, -1, 1, -1, 1);

  View view("test_view");
  view.sync(float4x4::identity(), win_mat);

  Manager drw;

  float4x4 obmat_1 = math::from_scale<float4x4>(float3(-0.5f));
  float4x4 obmat_2 = math::from_scale<float4x4>(float3(0.5f));

  drw.begin_sync();
  ResourceHandleRange handle1 = drw.resource_handle(obmat_1);
  ResourceHandleRange handle2 = drw.resource_handle(obmat_1);
  ResourceHandleRange handle3 = drw.resource_handle(obmat_2);
  drw.resource_handle(obmat_2, float3(2), float3(1));
  drw.end_sync();

  {
    /* Computed on CPU. */
    PassSimple pass = {"test.resource_id"};
    pass.init();
    pass.shader_set(
        GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA));
    pass.draw_procedural(GPU_PRIM_TRIS, 1, -1, -1, handle2);
    pass.draw_procedural(GPU_PRIM_POINTS, 4, -1, -1, handle1);
    pass.draw_procedural(GPU_PRIM_TRIS, 2, -1, -1, handle3);
    pass.draw_procedural(GPU_PRIM_POINTS, 5, -1, -1, handle1);
    pass.draw_procedural(GPU_PRIM_LINES, 1, -1, -1, handle3);
    pass.draw_procedural(GPU_PRIM_POINTS, 6, -1, -1, handle2);
    pass.draw_procedural(GPU_PRIM_TRIS, 3, -1, -1, handle1);

    Manager::SubmitDebugOutput debug = drw.submit_debug(pass, view);

    std::stringstream result;
    for (auto val : debug.resource_id) {
      result << val << " ";
    }

    StringRefNull expected_simple = "0 2 1 1 1 1 3 3 1 1 1 1 1 3 2 2 2 2 2 2 1 1 1 ";
    EXPECT_EQ(result.str(), expected_simple);
  }

  {
    /* Same thing with PassMain (computed on GPU) */
    PassMain pass = {"test.resource_id"};
    pass.init();
    pass.shader_set(
        GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA));
    pass.draw_procedural(GPU_PRIM_TRIS, 1, -1, -1, handle2);
    pass.draw_procedural(GPU_PRIM_POINTS, 4, -1, -1, handle1);
    pass.draw_procedural(GPU_PRIM_TRIS, 2, -1, -1, handle3);
    pass.draw_procedural(GPU_PRIM_POINTS, 5, -1, -1, handle1);
    pass.draw_procedural(GPU_PRIM_LINES, 1, -1, -1, handle3);
    pass.draw_procedural(GPU_PRIM_POINTS, 6, -1, -1, handle2);
    pass.draw_procedural(GPU_PRIM_TRIS, 3, -1, -1, handle1);

    Manager::SubmitDebugOutput debug = drw.submit_debug(pass, view);

    std::stringstream result;
    for (auto val : debug.resource_id) {
      result << val << " ";
    }

    /* When using PassMain the handles are sorted based on their handles and GPUBatches. Different
     * primitives use different batches.
     */
    StringRefNull expected_main = "2 3 3 1 1 1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 3 ";
    EXPECT_EQ(result.str(), expected_main);
  }

  GPU_render_end();
  DRW_shaders_free();
}
DRAW_TEST(draw_resource_id_gen)

static void test_draw_visibility()
{
  GTEST_SKIP() << "This test needs to be reviewed. It should check visibility checks, but all "
                  "resource handles are visible.";
  GPU_render_begin();
  Texture color_attachment;
  Framebuffer framebuffer;
  color_attachment.ensure_2d(blender::gpu::TextureFormat::SFLOAT_32_32_32_32, int2(1));
  framebuffer.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(color_attachment));
  framebuffer.bind();

  float4x4 win_mat;
  orthographic_m4(win_mat.ptr(), -1, 1, -1, 1, -1, 1);

  View view("test_view");
  view.sync(float4x4::identity(), win_mat);

  Manager drw;

  float4x4 obmat_1 = math::from_scale<float4x4>(float3(-0.5f));
  float4x4 obmat_2 = math::from_scale<float4x4>(float3(0.5f));

  drw.begin_sync();                                   /* Default {0} always visible. */
  drw.resource_handle(obmat_1);                       /* No bounds, always visible. */
  drw.resource_handle(obmat_1, float3(3), float3(1)); /* Out of view. */
  drw.resource_handle(obmat_2, float3(0), float3(1)); /* Inside view. */
  drw.end_sync();

  Texture tex;
  tex.ensure_2d(blender::gpu::TextureFormat::SFLOAT_16_16_16_16, int2(1));

  PassMain pass = {"test.visibility"};
  pass.init();
  pass.shader_set(GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE_COLOR));
  pass.bind_texture("image", tex);
  pass.draw_procedural(GPU_PRIM_TRIS, 1, -1);

  Manager::SubmitDebugOutput debug = drw.submit_debug(pass, view);
  Vector<uint32_t> expected_visibility = {0};

  std::stringstream result;
  for (auto val : debug.visibility) {
    result << std::bitset<32>(val);
  }

  EXPECT_EQ(result.str(), "11111111111111111111111111111011");

  GPU_render_end();
  DRW_shaders_free();
}
DRAW_TEST(draw_visibility)

static void test_draw_manager_sync()
{
  float4x4 obmat_1 = math::from_scale<float4x4>(float3(-0.5f));
  float4x4 obmat_2 = math::from_scale<float4x4>(float3(0.5f));

  /* TODO find a way to create a minimum object to test resource handle creation on it. */
  Manager drw;

  drw.begin_sync();
  drw.resource_handle(obmat_1);
  drw.resource_handle(obmat_2, float3(2), float3(1));
  drw.end_sync();

  Manager::DataDebugOutput debug = drw.data_debug();

  std::stringstream result;
  for (const auto &val : debug.matrices) {
    result << val;
  }
  for (const auto &val : debug.bounds) {
    result << val;
  }
  for (const auto &val : debug.infos) {
    result << val;
  }

  std::stringstream expected;
  expected << "ObjectMatrices(" << std::endl;
  expected << "model=(" << std::endl;
  expected << "(1, 0, 0, 0)," << std::endl;
  expected << "(0, 1, 0, 0)," << std::endl;
  expected << "(0, 0, 1, 0)," << std::endl;
  expected << "(0, 0, 0, 1)" << std::endl;
  expected << ")" << std::endl;
  expected << ", " << std::endl;
  expected << "model_inverse=(" << std::endl;
  expected << "(1, -0, 0, -0)," << std::endl;
  expected << "(-0, 1, -0, 0)," << std::endl;
  expected << "(0, -0, 1, -0)," << std::endl;
  expected << "(-0, 0, -0, 1)" << std::endl;
  expected << ")" << std::endl;
  expected << ")" << std::endl;
  expected << "ObjectMatrices(" << std::endl;
  expected << "model=(" << std::endl;
  expected << "(-0.5, 0, 0, 0)," << std::endl;
  expected << "(0, -0.5, 0, 0)," << std::endl;
  expected << "(0, 0, -0.5, 0)," << std::endl;
  expected << "(0, 0, 0, 1)" << std::endl;
  expected << ")" << std::endl;
  expected << ", " << std::endl;
  expected << "model_inverse=(" << std::endl;
  expected << "(-2, -0, -0, 0)," << std::endl;
  expected << "(-0, -2, 0, -0)," << std::endl;
  expected << "(-0, 0, -2, 0)," << std::endl;
  expected << "(0, -0, 0, 1)" << std::endl;
  expected << ")" << std::endl;
  expected << ")" << std::endl;
  expected << "ObjectMatrices(" << std::endl;
  expected << "model=(" << std::endl;
  expected << "(0.5, 0, 0, 0)," << std::endl;
  expected << "(0, 0.5, 0, 0)," << std::endl;
  expected << "(0, 0, 0.5, 0)," << std::endl;
  expected << "(0, 0, 0, 1)" << std::endl;
  expected << ")" << std::endl;
  expected << ", " << std::endl;
  expected << "model_inverse=(" << std::endl;
  expected << "(2, -0, 0, -0)," << std::endl;
  expected << "(-0, 2, -0, 0)," << std::endl;
  expected << "(0, -0, 2, -0)," << std::endl;
  expected << "(-0, 0, -0, 1)" << std::endl;
  expected << ")" << std::endl;
  expected << ")" << std::endl;
  expected << "ObjectBounds(skipped)" << std::endl;
  expected << "ObjectBounds(skipped)" << std::endl;
  expected << "ObjectBounds(" << std::endl;
  expected << ".bounding_corners[0](1.5, 0.5, 0.5)" << std::endl;
  expected << ".bounding_corners[1](-1, -0, -0)" << std::endl;
  expected << ".bounding_corners[2](0, 1, 0)" << std::endl;
  expected << ".bounding_corners[3](0, 0, 1)" << std::endl;
  expected << ".sphere=(pos=(1, 1, 1), rad=0.866025" << std::endl;
  expected << ")" << std::endl;
  expected << "ObjectInfos(skipped)" << std::endl;
  expected << "ObjectInfos(skipped)" << std::endl;
  expected << "ObjectInfos(skipped)" << std::endl;

  EXPECT_EQ(result.str(), expected.str());

  DRW_shaders_free();
}
DRAW_TEST(draw_manager_sync)

static void test_draw_submit_only()
{
  float4x4 projmat = math::projection::orthographic(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
  float4x4 viewmat = float4x4::identity();

  Texture color_attachment;
  Framebuffer framebuffer;
  color_attachment.ensure_2d(blender::gpu::TextureFormat::SFLOAT_32_32_32_32, int2(1));
  framebuffer.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(color_attachment));
  framebuffer.bind();

  Manager manager;
  View view = {"Test"};
  View view_other = {"Test"};
  PassSimple pass = {"Test"};
  PassMain pass_main = {"Test"};
  PassMain pass_manual = {"Test"};

  manager.begin_sync();
  manager.end_sync();
  view.sync(viewmat, projmat);
  view_other.sync(viewmat, projmat);

  /* Add some draws to prevent empty pass optimization. */
  gpu::Shader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
  pass.init();
  pass.shader_set(sh);
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  pass_main.init();
  pass_main.shader_set(sh);
  pass_main.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  pass_manual.init();
  pass_manual.shader_set(sh);
  pass_manual.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  /* Auto command and visibility computation. */
  manager.submit(pass);
  manager.submit(pass_main, view);

  /* Update manager. */
  manager.begin_sync();
  manager.end_sync();

  /* Auto command and visibility computation. */
  manager.submit(pass);
  manager.submit(pass_main, view);

  /* Update view. */
  view.sync(viewmat, projmat);

  /* Auto command and visibility computation. */
  manager.submit(pass);
  manager.submit(pass_main, view);

  /* Update both. */
  manager.begin_sync();
  manager.end_sync();
  view.sync(viewmat, projmat);

  /* Auto command and visibility computation. */
  manager.submit(pass);
  manager.submit(pass_main, view);

  /* Update both. */
  manager.begin_sync();
  manager.end_sync();
  view.sync(viewmat, projmat);

  {
    /* Manual command and visibility computation. */
    manager.compute_visibility(view);
    manager.generate_commands(pass_manual, view);
    manager.submit_only(pass_manual, view);

    /* Redundant updates. */
    EXPECT_BLI_ASSERT(manager.compute_visibility(view),
                      "Resources did not changed, no need to update");
    EXPECT_BLI_ASSERT(manager.generate_commands(pass_manual, view),
                      "Resources and view did not changed no need to update");
  }
  {
    /* Update view. */
    view.sync(viewmat, projmat);

    /* Submit before visibility. */
    EXPECT_BLI_ASSERT(manager.submit_only(pass_manual, view),
                      "compute_visibility was not called on this view");
    /* Update commands before visibility. */
    EXPECT_BLI_ASSERT(manager.generate_commands(pass_manual, view),
                      "Resources or view changed, but compute_visibility was not called");

    manager.compute_visibility(view);

    /* Submit before command generation. */
    EXPECT_BLI_ASSERT(manager.submit_only(pass_manual, view),
                      "View have changed since last generate_commands");

    manager.generate_commands(pass_manual, view);
    manager.submit_only(pass_manual, view);
  }
  {
    /* Update manager. */
    manager.begin_sync();
    manager.end_sync();

    /* Update commands before visibility. */
    EXPECT_BLI_ASSERT(manager.generate_commands(pass_manual, view),
                      "Resources or view changed, but compute_visibility was not called");
    /* Submit before visibility. */
    EXPECT_BLI_ASSERT(manager.submit_only(pass_manual, view),
                      "Resources changed since last compute_visibility");

    manager.compute_visibility(view);

    /* Submit with stale commands. */
    EXPECT_BLI_ASSERT(manager.submit_only(pass_manual, view),
                      "Resources changed since last generate_command");

    manager.generate_commands(pass_manual, view);
    manager.submit_only(pass_manual, view);
  }
  {
    /* Add some draws to prevent empty pass optimization. */
    pass_manual.init();
    pass_manual.shader_set(sh);
    pass_manual.draw_procedural(GPU_PRIM_TRIS, 1, 3);

    /* Submit before command generation. */
    EXPECT_BLI_ASSERT(manager.submit_only(pass_manual, view),
                      "generate_command was not called on this pass");
    manager.generate_commands(pass_manual, view);
    manager.submit_only(pass_manual, view);
  }
  {
    manager.compute_visibility(view_other);

    /* Submit with a different view before command generation. */
    EXPECT_BLI_ASSERT(manager.submit_only(pass_manual, view_other),
                      "submitting with a different view");
    manager.generate_commands(pass_manual, view_other);
    manager.submit_only(pass_manual, view_other);
  }

  DRW_shaders_free();
}
DRAW_TEST(draw_submit_only)

}  // namespace blender::draw
