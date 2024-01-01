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

GBufferDataUndetermined gbuffer_new()
{
  GBufferDataUndetermined data;
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
  GBufferDataUndetermined data_in;
  GBufferReader data_out;
  samplerGBufferHeader header_tx = 0;
  samplerGBufferClosure closure_tx = 0;
  samplerGBufferNormal normal_tx = 0;

  TEST(eevee_gbuffer, ClosureDiffuse)
  {
    data_in = gbuffer_new();
    data_in.diffuse.type = CLOSURE_BSDF_DIFFUSE_ID;
    data_in.diffuse.weight = 1.0;
    data_in.diffuse.color = vec3(0.1, 0.2, 0.3);
    data_in.diffuse.N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.layer_data, 1);
    EXPECT_EQ(data_out.closure_count, 1);
    EXPECT_TRUE(data_out.has_any_surface);
    EXPECT_TRUE(data_out.has_diffuse);

    EXPECT_EQ(data_in.diffuse.type, CLOSURE_BSDF_DIFFUSE_ID);
    EXPECT_NEAR(data_in.diffuse.color, data_out.data.diffuse.color, 1e-5);
    EXPECT_NEAR(data_in.diffuse.N, data_out.data.diffuse.N, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureSubsurface)
  {
    data_in = gbuffer_new();
    data_in.diffuse.type = CLOSURE_BSSRDF_BURLEY_ID;
    data_in.diffuse.weight = 1.0;
    data_in.diffuse.color = vec3(0.1, 0.2, 0.3);
    data_in.diffuse.data.rgb = vec3(0.2, 0.3, 0.4);
    data_in.diffuse.N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.layer_data, 2);
    EXPECT_EQ(data_out.closure_count, 1);
    EXPECT_TRUE(data_out.has_any_surface);
    EXPECT_TRUE(data_out.has_diffuse);
    EXPECT_TRUE(data_out.has_sss);

    EXPECT_EQ(data_in.diffuse.type, CLOSURE_BSSRDF_BURLEY_ID);
    EXPECT_NEAR(data_in.diffuse.color, data_out.data.diffuse.color, 1e-5);
    EXPECT_NEAR(data_in.diffuse.N, data_out.data.diffuse.N, 1e-5);
    EXPECT_NEAR(data_in.diffuse.data.rgb, data_out.data.diffuse.sss_radius, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureTranslucent)
  {
    data_in = gbuffer_new();
    data_in.translucent.type = CLOSURE_BSDF_TRANSLUCENT_ID;
    data_in.translucent.weight = 1.0;
    data_in.translucent.color = vec3(0.1, 0.2, 0.3);
    data_in.translucent.N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.layer_data, 1);
    EXPECT_EQ(data_out.closure_count, 1);
    EXPECT_TRUE(data_out.has_any_surface);
    EXPECT_TRUE(data_out.has_translucent);

    EXPECT_EQ(data_in.translucent.type, CLOSURE_BSDF_TRANSLUCENT_ID);
    EXPECT_NEAR(data_in.translucent.color, data_out.data.translucent.color, 1e-5);
    EXPECT_NEAR(data_in.translucent.N, data_out.data.translucent.N, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureReflection)
  {
    data_in = gbuffer_new();
    data_in.reflection.type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
    data_in.reflection.weight = 1.0;
    data_in.reflection.color = vec3(0.1, 0.2, 0.3);
    data_in.reflection.data.x = 0.4;
    data_in.reflection.N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.layer_data, 2);
    EXPECT_EQ(data_out.closure_count, 1);
    EXPECT_TRUE(data_out.has_any_surface);
    EXPECT_TRUE(data_out.has_reflection);

    EXPECT_EQ(data_in.reflection.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(data_in.reflection.color, data_out.data.reflection.color, 1e-5);
    EXPECT_NEAR(data_in.reflection.N, data_out.data.reflection.N, 1e-5);
    EXPECT_NEAR(data_in.reflection.data.r, data_out.data.reflection.roughness, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureRefraction)
  {
    data_in = gbuffer_new();
    data_in.refraction.type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
    data_in.refraction.weight = 1.0;
    data_in.refraction.color = vec3(0.1, 0.2, 0.3);
    data_in.refraction.data.x = 0.4;
    data_in.refraction.data.y = 0.5;
    data_in.refraction.N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.layer_data, 2);
    EXPECT_EQ(data_out.closure_count, 1);
    EXPECT_TRUE(data_out.has_any_surface);
    EXPECT_TRUE(data_out.has_refraction);

    EXPECT_EQ(data_in.refraction.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(data_in.refraction.color, data_out.data.refraction.color, 1e-5);
    EXPECT_NEAR(data_in.refraction.N, data_out.data.refraction.N, 1e-5);
    EXPECT_NEAR(data_in.refraction.data.r, data_out.data.refraction.roughness, 1e-5);
    EXPECT_NEAR(data_in.refraction.data.g, data_out.data.refraction.ior, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureCombination)
  {
    data_in = gbuffer_new();
    data_in.refraction.type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
    data_in.refraction.weight = 1.0;
    data_in.refraction.color = vec3(0.1, 0.2, 0.3);
    data_in.refraction.data.x = 0.4;
    data_in.refraction.data.y = 0.5;
    data_in.refraction.N = normalize(vec3(0.2, 0.1, 0.3));

    data_in.reflection.type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
    data_in.reflection.weight = 1.0;
    data_in.reflection.color = vec3(0.1, 0.2, 0.3);
    data_in.reflection.data.x = 0.4;
    data_in.reflection.N = normalize(vec3(0.2, 0.3, 0.4));

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.layer_data, 4);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 2);
    EXPECT_TRUE(data_out.has_any_surface);
    EXPECT_TRUE(data_out.has_refraction);
    EXPECT_TRUE(data_out.has_reflection);

    EXPECT_EQ(data_in.refraction.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(data_in.refraction.color, data_out.data.refraction.color, 1e-5);
    EXPECT_NEAR(data_in.refraction.N, data_out.data.refraction.N, 1e-5);
    EXPECT_NEAR(data_in.refraction.data.r, data_out.data.refraction.roughness, 1e-5);
    EXPECT_NEAR(data_in.refraction.data.g, data_out.data.refraction.ior, 1e-5);

    EXPECT_EQ(data_in.reflection.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(data_in.reflection.color, data_out.data.reflection.color, 1e-5);
    EXPECT_NEAR(data_in.reflection.N, data_out.data.reflection.N, 1e-5);
    EXPECT_NEAR(data_in.reflection.data.r, data_out.data.reflection.roughness, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureColorless)
  {
    data_in = gbuffer_new();
    data_in.refraction.type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
    data_in.refraction.weight = 1.0;
    data_in.refraction.color = vec3(0.1, 0.1, 0.1);
    data_in.refraction.data.x = 0.4;
    data_in.refraction.data.y = 0.5;
    data_in.refraction.N = normalize(vec3(0.2, 0.1, 0.3));

    data_in.reflection.type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
    data_in.reflection.weight = 1.0;
    data_in.reflection.color = vec3(0.1, 0.1, 0.1);
    data_in.reflection.data.x = 0.4;
    data_in.reflection.N = normalize(vec3(0.2, 0.3, 0.4));

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.layer_data, 2);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 2);
    EXPECT_TRUE(data_out.has_any_surface);
    EXPECT_TRUE(data_out.has_refraction);
    EXPECT_TRUE(data_out.has_reflection);

    EXPECT_EQ(data_in.refraction.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(data_in.refraction.color, data_out.data.refraction.color, 1e-5);
    EXPECT_NEAR(data_in.refraction.N, data_out.data.refraction.N, 1e-5);
    EXPECT_NEAR(data_in.refraction.data.r, data_out.data.refraction.roughness, 1e-5);
    EXPECT_NEAR(data_in.refraction.data.g, data_out.data.refraction.ior, 1e-5);

    EXPECT_EQ(data_in.reflection.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(data_in.reflection.color, data_out.data.reflection.color, 1e-5);
    EXPECT_NEAR(data_in.reflection.N, data_out.data.reflection.N, 1e-5);
    EXPECT_NEAR(data_in.reflection.data.r, data_out.data.reflection.roughness, 1e-5);
  }
}
