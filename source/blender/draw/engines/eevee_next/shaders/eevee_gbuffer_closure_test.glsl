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
  data.closure[0].weight = 0.0;
  data.closure[1].weight = 0.0;
  data.closure[2].weight = 0.0;
  data.thickness = 0.2;
  data.object_id = 0xF220u;
  data.surface_N = normalize(vec3(0.1, 0.2, 0.3));
  return data;
}

void main()
{
  GBufferData data_in;
  GBufferReader data_out;
  samplerGBufferHeader header_tx = 0;
  samplerGBufferClosure closure_tx = 0;
  samplerGBufferNormal normal_tx = 0;

  TEST(eevee_gbuffer, ClosureDiffuse)
  {
    data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_DIFFUSE_ID;
    data_in.closure[0].weight = 1.0;
    data_in.closure[0].color = vec3(0.1, 0.2, 0.3);
    data_in.closure[0].N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.data_len, 1);
    EXPECT_EQ(data_out.closure_count, 1);

    ClosureUndetermined out_diffuse = gbuffer_closure_get(data_out, 0);

    EXPECT_EQ(out_diffuse.type, CLOSURE_BSDF_DIFFUSE_ID);
    EXPECT_EQ(data_in.closure[0].type, CLOSURE_BSDF_DIFFUSE_ID);
    EXPECT_NEAR(data_in.closure[0].color, out_diffuse.color, 1e-5);
    EXPECT_NEAR(data_in.closure[0].N, out_diffuse.N, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureSubsurface)
  {
    data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSSRDF_BURLEY_ID;
    data_in.closure[0].weight = 1.0;
    data_in.closure[0].color = vec3(0.1, 0.2, 0.3);
    data_in.closure[0].data.rgb = vec3(0.2, 0.3, 0.4);
    data_in.closure[0].N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.data_len, 2);
    EXPECT_EQ(data_out.closure_count, 1);

    ClosureUndetermined out_sss_burley = gbuffer_closure_get(data_out, 0);

    EXPECT_EQ(out_sss_burley.type, CLOSURE_BSSRDF_BURLEY_ID);
    EXPECT_EQ(data_in.closure[0].type, CLOSURE_BSSRDF_BURLEY_ID);
    EXPECT_NEAR(data_in.closure[0].color, out_sss_burley.color, 1e-5);
    EXPECT_NEAR(data_in.closure[0].N, out_sss_burley.N, 1e-5);
    EXPECT_NEAR(
        data_in.closure[0].data.rgb, to_closure_subsurface(out_sss_burley).sss_radius, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureTranslucent)
  {
    data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_TRANSLUCENT_ID;
    data_in.closure[0].weight = 1.0;
    data_in.closure[0].color = vec3(0.1, 0.2, 0.3);
    data_in.closure[0].N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.data_len, 1);
    EXPECT_EQ(data_out.closure_count, 1);

    ClosureUndetermined out_translucent = gbuffer_closure_get(data_out, 0);

    EXPECT_EQ(out_translucent.type, CLOSURE_BSDF_TRANSLUCENT_ID);
    EXPECT_EQ(data_in.closure[0].type, CLOSURE_BSDF_TRANSLUCENT_ID);
    EXPECT_NEAR(data_in.closure[0].color, out_translucent.color, 1e-5);
    EXPECT_NEAR(data_in.closure[0].N, out_translucent.N, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureReflection)
  {
    data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
    data_in.closure[0].weight = 1.0;
    data_in.closure[0].color = vec3(0.1, 0.2, 0.3);
    data_in.closure[0].data.x = 0.4;
    data_in.closure[0].N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.data_len, 2);
    EXPECT_EQ(data_out.closure_count, 1);

    ClosureUndetermined out_reflection = gbuffer_closure_get(data_out, 0);

    EXPECT_EQ(out_reflection.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_EQ(data_in.closure[0].type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(data_in.closure[0].color, out_reflection.color, 1e-5);
    EXPECT_NEAR(data_in.closure[0].N, out_reflection.N, 1e-5);
    EXPECT_NEAR(data_in.closure[0].data.r, out_reflection.data.r, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureRefraction)
  {
    data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
    data_in.closure[0].weight = 1.0;
    data_in.closure[0].color = vec3(0.1, 0.2, 0.3);
    data_in.closure[0].data.x = 0.4;
    data_in.closure[0].data.y = 0.5;
    data_in.closure[0].N = normalize(vec3(0.2, 0.1, 0.3));

    g_data_packed = gbuffer_pack(data_in);
    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(g_data_packed.data_len, 2);
    EXPECT_EQ(data_out.closure_count, 1);

    ClosureUndetermined out_refraction = gbuffer_closure_get(data_out, 0);

    EXPECT_EQ(out_refraction.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_EQ(data_in.closure[0].type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(data_in.closure[0].color, out_refraction.color, 1e-5);
    EXPECT_NEAR(data_in.closure[0].N, out_refraction.N, 1e-5);
    EXPECT_NEAR(data_in.closure[0].data.r, out_refraction.data.r, 1e-5);
    EXPECT_NEAR(data_in.closure[0].data.g, out_refraction.data.g, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureCombination)
  {
    ClosureUndetermined in_cl0 = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    in_cl0.weight = 1.0;
    in_cl0.color = vec3(0.1, 0.2, 0.3);
    in_cl0.data.x = 0.4;
    in_cl0.data.y = 0.5;
    in_cl0.N = normalize(vec3(0.2, 0.1, 0.3));

    ClosureUndetermined in_cl1 = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    in_cl1.weight = 1.0;
    in_cl1.color = vec3(0.4, 0.5, 0.6);
    in_cl1.data.x = 0.6;
    in_cl1.N = normalize(vec3(0.2, 0.3, 0.4));

    data_in = gbuffer_new();
    data_in.closure[0] = in_cl0;
    data_in.closure[1] = in_cl1;

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.data_len, 4);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 2);

    ClosureUndetermined out_cl0 = gbuffer_closure_get(data_out, 0);
    ClosureUndetermined out_cl1 = gbuffer_closure_get(data_out, 1);

    EXPECT_EQ(out_cl0.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_EQ(in_cl0.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(in_cl0.color, out_cl0.color, 1e-5);
    EXPECT_NEAR(in_cl0.N, out_cl0.N, 1e-5);
    EXPECT_NEAR(in_cl0.data.r, out_cl0.data.r, 1e-5);
    EXPECT_NEAR(in_cl0.data.g, out_cl0.data.g, 1e-5);

    EXPECT_EQ(out_cl1.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_EQ(in_cl1.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(in_cl1.color, out_cl1.color, 1e-5);
    EXPECT_NEAR(in_cl1.N, out_cl1.N, 1e-5);
    EXPECT_NEAR(in_cl1.data.r, out_cl1.data.r, 1e-5);
  }

  TEST(eevee_gbuffer, ClosureColorless)
  {
    data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
    data_in.closure[0].weight = 1.0;
    data_in.closure[0].color = vec3(0.1, 0.1, 0.1);
    data_in.closure[0].data.x = 0.4;
    data_in.closure[0].data.y = 0.5;
    data_in.closure[0].N = normalize(vec3(0.2, 0.1, 0.3));

    data_in.closure[1].type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
    data_in.closure[1].weight = 1.0;
    data_in.closure[1].color = vec3(0.1, 0.1, 0.1);
    data_in.closure[1].data.x = 0.4;
    data_in.closure[1].N = normalize(vec3(0.2, 0.3, 0.4));

    g_data_packed = gbuffer_pack(data_in);

    EXPECT_EQ(g_data_packed.data_len, 2);

    data_out = gbuffer_read(header_tx, closure_tx, normal_tx, ivec2(0));

    EXPECT_EQ(data_out.closure_count, 2);

    ClosureUndetermined out_reflection = gbuffer_closure_get(data_out, 1);
    ClosureUndetermined out_refraction = gbuffer_closure_get(data_out, 0);

    EXPECT_EQ(out_refraction.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_EQ(data_in.closure[0].type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(data_in.closure[0].color, out_refraction.color, 1e-5);
    EXPECT_NEAR(data_in.closure[0].N, out_refraction.N, 1e-5);
    EXPECT_NEAR(data_in.closure[0].data.r, out_refraction.data.r, 1e-5);
    EXPECT_NEAR(data_in.closure[0].data.g, out_refraction.data.g, 1e-5);

    EXPECT_EQ(out_reflection.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_EQ(data_in.closure[1].type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(data_in.closure[1].color, out_reflection.color, 1e-5);
    EXPECT_NEAR(data_in.closure[1].N, out_reflection.N, 1e-5);
    EXPECT_NEAR(data_in.closure[1].data.r, out_reflection.data.r, 1e-5);
  }
}
