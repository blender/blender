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
  float3 surface_N = normalize(float3(0.1f, 0.2f, 0.3f));

  ClosureUndetermined cl1 = closure_new(CLOSURE_BSDF_DIFFUSE_ID);
  cl1.weight = 1.0f;
  cl1.color = float3(1);
  cl1.N = normalize(float3(0.2f, 0.1f, 0.3f));

  ClosureUndetermined cl2 = closure_new(CLOSURE_BSDF_DIFFUSE_ID);
  cl2.weight = 1.0f;
  cl2.color = float3(1);
  cl2.N = normalize(float3(0.1f, 0.2f, 0.3f));

  ClosureUndetermined cl3 = closure_new(CLOSURE_BSDF_DIFFUSE_ID);
  cl3.weight = 1.0f;
  cl3.color = float3(1);
  cl3.N = normalize(float3(0.3f, 0.2f, 0.1f));

  ClosureUndetermined cl_none = closure_new(CLOSURE_NONE_ID);
  cl_none.weight = 1.0f;
  cl_none.color = float3(1);
  cl_none.N = normalize(float3(0.0f, 0.0f, 1.0f));

  TEST(eevee_gbuffer, NormalReuseDoubleFirst)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = cl1;
    data_in.closure[1] = cl1;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), 0u);
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 0, 1));
    EXPECT_EQ(header.closure_len(), 2);
    EXPECT_EQ(header.tangent_space_id(0), 0u);
    EXPECT_EQ(header.tangent_space_id(1), 0u);
    EXPECT_NEAR(cl1.N, gbuffer::normal_unpack(data_out.normal[0]), 1e-5f);
    EXPECT_NEAR(cl1.N, gbuffer::normal_unpack(data_out.normal[1]), 1e-5f);
  }

  TEST(eevee_gbuffer, NormalReuseDoubleNone)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = cl1;
    data_in.closure[1] = cl2;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(NORMAL_DATA_1));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 0, 1));
    EXPECT_EQ(header.closure_len(), 2);
    EXPECT_EQ(header.tangent_space_id(0), 0u);
    EXPECT_EQ(header.tangent_space_id(1), 1u);
    EXPECT_NEAR(cl1.N, gbuffer::normal_unpack(data_out.normal[0]), 1e-5f);
    EXPECT_NEAR(cl2.N, gbuffer::normal_unpack(data_out.normal[1]), 1e-5f);
  }

  TEST(eevee_gbuffer, NormalReuseTripleFirst)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = cl1;
    data_in.closure[1] = cl2;
    data_in.closure[2] = cl2;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(NORMAL_DATA_1 | CLOSURE_DATA_2));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 0, 0));
    EXPECT_EQ(header.closure_len(), 3);
    EXPECT_EQ(header.tangent_space_id(0), 0u);
    EXPECT_EQ(header.tangent_space_id(1), 1u);
    EXPECT_EQ(header.tangent_space_id(2), 1u);
    EXPECT_NEAR(cl1.N, gbuffer::normal_unpack(data_out.normal[0]), 1e-5f);
    EXPECT_NEAR(cl2.N, gbuffer::normal_unpack(data_out.normal[1]), 1e-5f);
  }

  TEST(eevee_gbuffer, NormalReuseTripleSecond)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = cl2;
    data_in.closure[1] = cl1;
    data_in.closure[2] = cl2;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(NORMAL_DATA_1 | CLOSURE_DATA_2));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 0, 0));
    EXPECT_EQ(header.closure_len(), 3);
    EXPECT_EQ(header.tangent_space_id(0), 0u);
    EXPECT_EQ(header.tangent_space_id(1), 1u);
    EXPECT_EQ(header.tangent_space_id(2), 0u);
    EXPECT_NEAR(cl2.N, gbuffer::normal_unpack(data_out.normal[0]), 1e-5f);
    EXPECT_NEAR(cl1.N, gbuffer::normal_unpack(data_out.normal[1]), 1e-5f);
  }

  TEST(eevee_gbuffer, NormalReuseTripleThird)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = cl2;
    data_in.closure[1] = cl2;
    data_in.closure[2] = cl1;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(NORMAL_DATA_2 | CLOSURE_DATA_2));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 0, 0));
    EXPECT_EQ(header.closure_len(), 3);
    EXPECT_EQ(header.tangent_space_id(0), 0u);
    EXPECT_EQ(header.tangent_space_id(1), 0u);
    EXPECT_EQ(header.tangent_space_id(2), 2u);
    EXPECT_NEAR(cl2.N, gbuffer::normal_unpack(data_out.normal[0]), 1e-5f);
    EXPECT_NEAR(cl1.N, gbuffer::normal_unpack(data_out.normal[2]), 1e-5f);
  }

  TEST(eevee_gbuffer, NormalReuseTripleNone)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = cl1;
    data_in.closure[1] = cl2;
    data_in.closure[2] = cl3;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(NORMAL_DATA_1 | NORMAL_DATA_2 | CLOSURE_DATA_2));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 0, 0));
    EXPECT_EQ(header.closure_len(), 3);
    EXPECT_EQ(header.tangent_space_id(0), 0u);
    EXPECT_EQ(header.tangent_space_id(1), 1u);
    EXPECT_EQ(header.tangent_space_id(2), 2u);
    EXPECT_NEAR(cl1.N, gbuffer::normal_unpack(data_out.normal[0]), 1e-5f);
    EXPECT_NEAR(cl2.N, gbuffer::normal_unpack(data_out.normal[1]), 1e-5f);
    EXPECT_NEAR(cl3.N, gbuffer::normal_unpack(data_out.normal[2]), 1e-5f);
  }

  TEST(eevee_gbuffer, NormalReuseSingleHole)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = cl1;
    data_in.closure[1] = cl_none;
    data_in.closure[2] = cl3;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(NORMAL_DATA_1));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(0, 1, 0));
    EXPECT_EQ(header.closure_len(), 2);
    EXPECT_EQ(header.tangent_space_id(0), 0u);
    EXPECT_EQ(header.tangent_space_id(1), 1u);
    EXPECT_NEAR(cl1.N, gbuffer::normal_unpack(data_out.normal[0]), 1e-5f);
    EXPECT_NEAR(cl3.N, gbuffer::normal_unpack(data_out.normal[1]), 1e-5f);
  }

  TEST(eevee_gbuffer, NormalReuseDoubleHole)
  {
    gbuffer::InputClosures data_in = gbuffer_new();
    data_in.closure[0] = cl_none;
    data_in.closure[1] = cl_none;
    data_in.closure[2] = cl3;

    const gbuffer::Packed data_out = gbuffer::pack(data_in, Ng, N, thickness, false);
    const gbuffer::Header header = gbuffer::Header::from_data(data_out.header);

    EXPECT_EQ(uint(data_out.used_layers), uint(0));
    EXPECT_EQ(uint3(header.empty_bins()), uint3(1, 1, 0));
    EXPECT_EQ(header.closure_len(), 1);
    EXPECT_EQ(header.tangent_space_id(0), 0u);
    EXPECT_NEAR(cl3.N, gbuffer::normal_unpack(data_out.normal[1]), 1e-5f);
  }
}
