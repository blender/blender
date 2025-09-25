/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#if 1 /* WORKAROUND: GLSL shader compilation mutate line directives searching `#line` pattern. */
#  line 10
#endif

#include "eevee_gbuffer_write_lib.glsl"
#include "gpu_shader_test_lib.glsl"

#define TEST(a, b) if (true)

gbuffer::InputClosures gbuffer_new()
{
  gbuffer::InputClosures data;
  data.closure[0].type = CLOSURE_NONE_ID;
  data.closure[0].weight = 0.0f;
  data.closure[1].type = CLOSURE_NONE_ID;
  data.closure[1].weight = 0.0f;
  data.closure[2].type = CLOSURE_NONE_ID;
  data.closure[2].weight = 0.0f;
  return data;
}

void main()
{
  float3 Ng = float3(1.0f, 0.0f, 0.0f);
  float3 N = Ng;
  float thickness = 0.2f;

  TEST(eevee_gbuffer, ClosureDiffuse)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[1].type = CLOSURE_BSDF_DIFFUSE_ID;
    data_in.closure[1].weight = 1.0f;
    data_in.closure[1].color = float3(0.1f, 0.2f, 0.3f);
    data_in.closure[1].N = normalize(float3(0.2f, 0.1f, 0.3f));

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), 0u);
    EXPECT_EQ(uint3(header.empty_bins()), uint3(1, 0, 1));
    EXPECT_EQ(header.closure_len(), 1);

    ClosureUndetermined out_diffuse;
    out_diffuse.type = gbuffer::mode_to_closure_type(header.bin_type(1));
    out_diffuse.color = gbuffer::closure_color_unpack(data_out.closure[0]);
    out_diffuse.N = gbuffer::normal_unpack(data_out.normal[0]);

    EXPECT_EQ(out_diffuse.type, CLOSURE_BSDF_DIFFUSE_ID);
    EXPECT_NEAR(out_diffuse.color, data_in.closure[1].color, 1e-5f);
    EXPECT_NEAR(out_diffuse.N, data_in.closure[1].N, 1e-5f);
  }

  TEST(eevee_gbuffer, ClosureSubsurface)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSSRDF_BURLEY_ID;
    data_in.closure[0].weight = 1.0f;
    data_in.closure[0].color = float3(0.1f, 0.2f, 0.3f);
    data_in.closure[0].data.rgb = float3(0.2f, 0.3f, 0.4f);
    data_in.closure[0].N = normalize(float3(0.2f, 0.1f, 0.3f));

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(ADDITIONAL_DATA));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 1, 1));
    EXPECT_EQ(header.closure_len(), 1);

    ClosureUndetermined out_sss_burley;
    out_sss_burley.type = gbuffer::mode_to_closure_type(header.bin_type(0));
    out_sss_burley.color = gbuffer::closure_color_unpack(data_out.closure[0]);
    out_sss_burley.N = gbuffer::normal_unpack(data_out.normal[0]);
    gbuffer::Subsurface::unpack_additional(out_sss_burley, data_out.closure[1]);

    EXPECT_EQ(out_sss_burley.type, CLOSURE_BSSRDF_BURLEY_ID);
    EXPECT_NEAR(out_sss_burley.color, data_in.closure[0].color, 1e-5f);
    EXPECT_NEAR(out_sss_burley.N, data_in.closure[0].N, 1e-5f);
    EXPECT_NEAR(out_sss_burley.data.rgb, data_in.closure[0].data.rgb, 1e-5f);
  }

  TEST(eevee_gbuffer, ClosureTranslucent)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_TRANSLUCENT_ID;
    data_in.closure[0].weight = 1.0f;
    data_in.closure[0].color = float3(0.1f, 0.2f, 0.3f);
    data_in.closure[0].N = normalize(float3(0.2f, 0.1f, 0.3f));

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(ADDITIONAL_DATA));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 1, 1));
    EXPECT_EQ(header.closure_len(), 1);

    ClosureUndetermined out_translucent;
    out_translucent.type = gbuffer::mode_to_closure_type(header.bin_type(0));
    out_translucent.color = gbuffer::closure_color_unpack(data_out.closure[0]);
    out_translucent.N = gbuffer::normal_unpack(data_out.normal[0]);

    EXPECT_EQ(out_translucent.type, CLOSURE_BSDF_TRANSLUCENT_ID);
    EXPECT_NEAR(out_translucent.color, data_in.closure[0].color, 1e-5f);
    EXPECT_NEAR(out_translucent.N, data_in.closure[0].N, 1e-5f);
  }

  TEST(eevee_gbuffer, ClosureReflection)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
    data_in.closure[0].weight = 1.0f;
    data_in.closure[0].color = float3(0.1f, 0.2f, 0.3f);
    data_in.closure[0].data.x = 0.4f;
    data_in.closure[0].N = normalize(float3(0.2f, 0.1f, 0.3f));

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), 0u);
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 1, 1));
    EXPECT_EQ(header.closure_len(), 1);

    ClosureUndetermined out_reflection;
    out_reflection.type = gbuffer::mode_to_closure_type(header.bin_type(0));
    out_reflection.color = gbuffer::closure_color_unpack(data_out.closure[0]);
    out_reflection.N = gbuffer::normal_unpack(data_out.normal[0]);
    gbuffer::Reflection::unpack_additional(out_reflection, data_out.closure[1]);

    EXPECT_EQ(out_reflection.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(out_reflection.color, data_in.closure[0].color, 1e-5f);
    EXPECT_NEAR(out_reflection.N, data_in.closure[0].N, 1e-5f);
    EXPECT_NEAR(out_reflection.data.r, data_in.closure[0].data.r, 1e-5f);
  }

  TEST(eevee_gbuffer, ClosureRefraction)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
    data_in.closure[0].weight = 1.0f;
    data_in.closure[0].color = float3(0.1f, 0.2f, 0.3f);
    data_in.closure[0].data.x = 0.4f;
    data_in.closure[0].data.y = 0.5f;
    data_in.closure[0].N = normalize(float3(0.2f, 0.1f, 0.3f));

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(ADDITIONAL_DATA));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 1, 1));
    EXPECT_EQ(header.closure_len(), 1);

    ClosureUndetermined out_refraction;
    out_refraction.type = gbuffer::mode_to_closure_type(header.bin_type(0));
    out_refraction.color = gbuffer::closure_color_unpack(data_out.closure[0]);
    out_refraction.N = gbuffer::normal_unpack(data_out.normal[0]);
    gbuffer::Refraction::unpack_additional(out_refraction, data_out.closure[1]);

    EXPECT_EQ(out_refraction.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(out_refraction.color, data_in.closure[0].color, 1e-5f);
    EXPECT_NEAR(out_refraction.N, data_in.closure[0].N, 1e-5f);
    EXPECT_NEAR(out_refraction.data.r, data_in.closure[0].data.r, 1e-5f);
    EXPECT_NEAR(out_refraction.data.g, data_in.closure[0].data.g, 1e-5f);
  }

  TEST(eevee_gbuffer, ClosureCombination)
  {
    ClosureUndetermined in_cl0 = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    in_cl0.weight = 1.0f;
    in_cl0.color = float3(0.1f, 0.2f, 0.3f);
    in_cl0.data.x = 0.4f;
    in_cl0.data.y = 0.5f;
    in_cl0.N = normalize(float3(0.2f, 0.1f, 0.3f));

    ClosureUndetermined in_cl1 = closure_new(CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    in_cl1.weight = 1.0f;
    in_cl1.color = float3(0.4f, 0.5f, 0.6f);
    in_cl1.data.x = 0.6f;
    in_cl1.N = normalize(float3(0.2f, 0.3f, 0.4f));

    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = in_cl0;
    data_in.closure[2] = in_cl1;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers),
              uint(ADDITIONAL_DATA | NORMAL_DATA_1 | CLOSURE_DATA_2 | CLOSURE_DATA_3));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 1, 0));
    EXPECT_EQ(header.closure_len(), 2);

    ClosureUndetermined out_cl0;
    out_cl0.type = gbuffer::mode_to_closure_type(header.bin_type(0));
    out_cl0.color = gbuffer::closure_color_unpack(data_out.closure[0]);
    out_cl0.N = gbuffer::normal_unpack(data_out.normal[0]);
    gbuffer::Refraction::unpack_additional(out_cl0, data_out.closure[2]);

    ClosureUndetermined out_cl1;
    out_cl1.type = gbuffer::mode_to_closure_type(header.bin_type(2));
    out_cl1.color = gbuffer::closure_color_unpack(data_out.closure[1]);
    out_cl1.N = gbuffer::normal_unpack(data_out.normal[1]);
    gbuffer::Reflection::unpack_additional(out_cl1, data_out.closure[3]);

    EXPECT_EQ(out_cl0.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(out_cl0.color, in_cl0.color, 1e-5f);
    EXPECT_NEAR(out_cl0.N, in_cl0.N, 1e-5f);
    EXPECT_NEAR(out_cl0.data.r, in_cl0.data.r, 1e-5f);
    EXPECT_NEAR(out_cl0.data.g, in_cl0.data.g, 1e-5f);

    EXPECT_EQ(out_cl1.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(out_cl1.color, in_cl1.color, 1e-5f);
    EXPECT_NEAR(out_cl1.N, in_cl1.N, 1e-5f);
    EXPECT_NEAR(out_cl1.data.r, in_cl1.data.r, 1e-5f);
  }

  TEST(eevee_gbuffer, ClosureColorless)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0].type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
    data_in.closure[0].weight = 1.0f;
    data_in.closure[0].color = float3(0.1f, 0.1f, 0.1f);
    data_in.closure[0].data.x = 0.4f;
    data_in.closure[0].data.y = 0.5f;
    data_in.closure[0].N = normalize(float3(0.2f, 0.1f, 0.3f));

    data_in.closure[1].type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
    data_in.closure[1].weight = 1.0f;
    data_in.closure[1].color = float3(0.1f, 0.1f, 0.1f);
    data_in.closure[1].data.x = 0.4f;
    data_in.closure[1].N = normalize(float3(0.2f, 0.3f, 0.4f));

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(ADDITIONAL_DATA | NORMAL_DATA_1));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 0, 1));
    EXPECT_EQ(header.closure_len(), 2);

    ClosureUndetermined out_refraction;
    out_refraction.type = gbuffer::mode_to_closure_type(header.bin_type(0));
    out_refraction.color = gbuffer::closure_color_unpack(data_out.closure[0]);
    out_refraction.N = gbuffer::normal_unpack(data_out.normal[0]);
    gbuffer::RefractionColorless::unpack_additional(out_refraction, data_out.closure[0]);

    ClosureUndetermined out_reflection;
    out_reflection.type = gbuffer::mode_to_closure_type(header.bin_type(1));
    out_reflection.color = gbuffer::closure_color_unpack(data_out.closure[1]);
    out_reflection.N = gbuffer::normal_unpack(data_out.normal[1]);
    gbuffer::ReflectionColorless::unpack_additional(out_reflection, data_out.closure[1]);

    EXPECT_EQ(out_refraction.type, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
    EXPECT_NEAR(out_refraction.color, data_in.closure[0].color, 1e-5f);
    EXPECT_NEAR(out_refraction.N, data_in.closure[0].N, 1e-5f);
    EXPECT_NEAR(out_refraction.data.r, data_in.closure[0].data.r, 1e-5f);
    EXPECT_NEAR(out_refraction.data.g, data_in.closure[0].data.g, 1e-5f);

    EXPECT_EQ(out_reflection.type, CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID);
    EXPECT_NEAR(out_reflection.color, data_in.closure[1].color, 1e-5f);
    EXPECT_NEAR(out_reflection.N, data_in.closure[1].N, 1e-5f);
    EXPECT_NEAR(out_reflection.data.r, data_in.closure[1].data.r, 1e-5f);
  }
}
