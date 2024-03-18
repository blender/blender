/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#line 9

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_test_lib.glsl)

#define TEST(a, b) if (true)

GBufferData gbuffer_new()
{
  GBufferData data;
  data.diffuse.weight = 0.0;
  data.translucent.weight = 0.0;
  data.reflection.weight = 0.0;
  data.refraction.weight = 0.0;
  data.thickness = 0.2;
  data.object_id = 0xF220u;
  data.surface_N = normalize(vec3(0.1, 0.2, 0.3));
  return data;
}

void main()
{
  TEST(eevee_gbuffer, NormalPack)
  {
    GBufferWriter gbuf;
    gbuf.header = 0u;
    gbuf.layer_gbuf = 0;
    gbuf.layer_data = 0;
    gbuf.layer_normal = 0;

    vec3 N0 = normalize(vec3(0.2, 0.1, 0.3));
    vec3 N1 = normalize(vec3(0.1, 0.2, 0.3));
    vec3 N2 = normalize(vec3(0.3, 0.1, 0.2));

    gbuffer_append_closure(gbuf, GBUF_DIFFUSE);
    gbuffer_append_normal(gbuf, N0);

    EXPECT_EQ(gbuf.layer_gbuf, 1);
    EXPECT_EQ(gbuf.layer_normal, 1);
    EXPECT_EQ(gbuf.N[0], gbuffer_normal_pack(N0));

    gbuffer_append_closure(gbuf, GBUF_DIFFUSE);
    gbuffer_append_normal(gbuf, N1);

    EXPECT_EQ(gbuf.layer_gbuf, 2);
    EXPECT_EQ(gbuf.layer_normal, 2);
    EXPECT_EQ(gbuf.N[1], gbuffer_normal_pack(N1));

    gbuffer_append_closure(gbuf, GBUF_DIFFUSE);
    gbuffer_append_normal(gbuf, N2);

    EXPECT_EQ(gbuf.layer_gbuf, 3);
    EXPECT_EQ(gbuf.layer_normal, 3);
    EXPECT_EQ(gbuf.N[2], gbuffer_normal_pack(N2));
  }

  TEST(eevee_gbuffer, NormalPackOpti)
  {
    GBufferWriter gbuf;
    gbuf.header = 0u;
    gbuf.layer_gbuf = 0;
    gbuf.layer_data = 0;
    gbuf.layer_normal = 0;

    vec3 N0 = normalize(vec3(0.2, 0.1, 0.3));

    gbuffer_append_closure(gbuf, GBUF_DIFFUSE);
    gbuffer_append_normal(gbuf, N0);

    EXPECT_EQ(gbuf.layer_gbuf, 1);
    EXPECT_EQ(gbuf.layer_normal, 1);
    EXPECT_EQ(gbuf.N[0], gbuffer_normal_pack(N0));

    gbuffer_append_closure(gbuf, GBUF_DIFFUSE);
    gbuffer_append_normal(gbuf, N0);

    EXPECT_EQ(gbuf.layer_gbuf, 2);
    EXPECT_EQ(gbuf.layer_normal, 1);

    gbuffer_append_closure(gbuf, GBUF_DIFFUSE);
    gbuffer_append_normal(gbuf, N0);

    EXPECT_EQ(gbuf.layer_gbuf, 3);
    EXPECT_EQ(gbuf.layer_normal, 1);
  }

  GBufferData data_in;
  GBufferReader data_out;
  samplerGBufferHeader header_tx = 0;
  samplerGBufferClosure closure_tx = 0;
  samplerGBufferNormal normal_tx = 0;

  ClosureUndetermined cl1 = closure_new(CLOSURE_BSDF_DIFFUSE_ID);
  cl1.weight = 1.0;
  cl1.color = vec3(1);
  cl1.N = normalize(vec3(0.2, 0.1, 0.3));

  ClosureUndetermined cl2 = closure_new(CLOSURE_BSDF_DIFFUSE_ID);
  cl2.weight = 1.0;
  cl2.color = vec3(1);
  cl2.N = normalize(vec3(0.1, 0.2, 0.3));

  ClosureUndetermined cl3 = closure_new(CLOSURE_BSDF_DIFFUSE_ID);
  cl3.weight = 1.0;
  cl3.color = vec3(1);
  cl3.N = normalize(vec3(0.3, 0.2, 0.1));

  TEST(eevee_gbuffer, NormalReuseDoubleFirst)
  {
    data_in = gbuffer_new();
    data_in.refraction = cl1;
    data_in.reflection = cl1;

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.layer_gbuf, 2);
    EXPECT_EQ(g_data_packed.layer_normal, 1);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 2);
    EXPECT_EQ(data_out.layer_normal, 1);
    EXPECT_NEAR(cl1.N, gbuffer_closure_get(data_out, 0).N, 1e-5);
    EXPECT_NEAR(cl1.N, gbuffer_closure_get(data_out, 1).N, 1e-5);
  }

  TEST(eevee_gbuffer, NormalReuseDoubleNone)
  {
    data_in = gbuffer_new();
    data_in.refraction = cl1;
    data_in.reflection = cl2;

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.layer_gbuf, 2);
    EXPECT_EQ(g_data_packed.layer_normal, 2);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 2);
    EXPECT_EQ(data_out.layer_normal, 2);
    EXPECT_NEAR(cl1.N, gbuffer_closure_get(data_out, 0).N, 1e-5);
    EXPECT_NEAR(cl2.N, gbuffer_closure_get(data_out, 1).N, 1e-5);
  }

  TEST(eevee_gbuffer, NormalReuseTripleFirst)
  {
    data_in = gbuffer_new();
    data_in.diffuse = cl1;
    data_in.refraction = cl2;
    data_in.reflection = cl2;

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.layer_normal, 2);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 3);
    EXPECT_EQ(data_out.layer_normal, 2);
    EXPECT_NEAR(cl1.N, gbuffer_closure_get(data_out, 0).N, 1e-5);
    EXPECT_NEAR(cl2.N, gbuffer_closure_get(data_out, 1).N, 1e-5);
    EXPECT_NEAR(cl2.N, gbuffer_closure_get(data_out, 2).N, 1e-5);
  }

  TEST(eevee_gbuffer, NormalReuseTripleSecond)
  {
    data_in = gbuffer_new();
    data_in.diffuse = cl2;
    data_in.refraction = cl1;
    data_in.reflection = cl2;

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.layer_normal, 2);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 3);
    EXPECT_EQ(data_out.layer_normal, 2);
    EXPECT_NEAR(cl2.N, gbuffer_closure_get(data_out, 0).N, 1e-5);
    EXPECT_NEAR(cl1.N, gbuffer_closure_get(data_out, 1).N, 1e-5);
    EXPECT_NEAR(cl2.N, gbuffer_closure_get(data_out, 2).N, 1e-5);
  }

  TEST(eevee_gbuffer, NormalReuseTripleThird)
  {
    data_in = gbuffer_new();
    data_in.diffuse = cl2;
    data_in.refraction = cl2;
    data_in.reflection = cl1;

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.layer_normal, 2);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 3);
    EXPECT_EQ(data_out.layer_normal, 2);
    EXPECT_NEAR(cl2.N, gbuffer_closure_get(data_out, 0).N, 1e-5);
    EXPECT_NEAR(cl2.N, gbuffer_closure_get(data_out, 1).N, 1e-5);
    EXPECT_NEAR(cl1.N, gbuffer_closure_get(data_out, 2).N, 1e-5);
  }

  TEST(eevee_gbuffer, NormalReuseTripleNone)
  {
    data_in = gbuffer_new();
    data_in.diffuse = cl1;
    data_in.refraction = cl2;
    data_in.reflection = cl3;

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.layer_normal, 3);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 3);
    EXPECT_EQ(data_out.layer_normal, 3);
    EXPECT_NEAR(cl1.N, gbuffer_closure_get(data_out, 0).N, 1e-5);
    EXPECT_NEAR(cl2.N, gbuffer_closure_get(data_out, 1).N, 1e-5);
    EXPECT_NEAR(cl3.N, gbuffer_closure_get(data_out, 2).N, 1e-5);
  }
}
