/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_shader.h"
#include "GPU_storage_buffer.h"

#include "BLI_math_vector.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "gpu_testing.hh"

namespace blender::gpu::tests {
struct CallData {
  GPUStorageBuf *ssbo = nullptr;
  Vector<float> data;

  float float_in;
  float2 vec2_in;
  float3 vec3_in;
  float4 vec4_in;

  void init_ssbo(size_t num_floats)
  {
    if (ssbo == nullptr) {
      ssbo = GPU_storagebuf_create_ex(
          num_floats * sizeof(float), nullptr, GPU_USAGE_DEVICE_ONLY, __func__);
      data.resize(num_floats);
    }
  }

  ~CallData()
  {
    if (ssbo != nullptr) {
      GPU_storagebuf_free(ssbo);
      ssbo = nullptr;
    }
  }

  void generate_test_data(const float vector_mul, const float scalar_mul)
  {
    float_in = vector_mul;
    vec2_in = float2(vector_mul * 2.0, vector_mul * 2.0 + scalar_mul);
    vec3_in = float3(
        vector_mul * 3.0, vector_mul * 3.0 + scalar_mul, vector_mul * 3.0 + scalar_mul * 2.0);
    vec4_in = float4(vector_mul * 4.0,
                     vector_mul * 4.0 + scalar_mul,
                     vector_mul * 4.0 + scalar_mul * 2.0,
                     vector_mul * 4.0 + scalar_mul * 3.0);
  }

  void read_back()
  {
    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
    GPU_storagebuf_read(ssbo, data.data());
  }

  void validate()
  {
    /* Check the results. */
    EXPECT_EQ(data[0], float_in);
    EXPECT_EQ(data[1], vec2_in.x);
    EXPECT_EQ(data[2], vec2_in.y);
    EXPECT_EQ(data[3], vec3_in.x);
    EXPECT_EQ(data[4], vec3_in.y);
    EXPECT_EQ(data[5], vec3_in.z);
    EXPECT_EQ(data[6], vec4_in.x);
    EXPECT_EQ(data[7], vec4_in.y);
    EXPECT_EQ(data[8], vec4_in.z);
    EXPECT_EQ(data[9], vec4_in.w);
  }
};

struct Shader {
  GPUShader *shader = nullptr;
  Vector<CallData> call_datas;

  Shader()
  {
    call_datas.reserve(10);
  }

  ~Shader()
  {
    if (shader != nullptr) {
      GPU_shader_unbind();
      GPU_shader_free(shader);
    }
  }

  void init_shader(const char *info_name)
  {
    if (shader == nullptr) {
      shader = GPU_shader_create_from_info_name(info_name);
      EXPECT_NE(shader, nullptr);
      GPU_shader_bind(shader);
    }
  }

  CallData &new_call()
  {
    CallData call_data;
    call_datas.append(call_data);
    return call_datas.last();
  }

  void bind(CallData &call_data)
  {
    GPU_storagebuf_bind(call_data.ssbo, GPU_shader_get_ssbo_binding(shader, "data_out"));
  }

  void update_push_constants(const CallData &call_data)
  {
    GPU_shader_uniform_1f(shader, "float_in", call_data.float_in);
    GPU_shader_uniform_2fv(shader, "vec2_in", call_data.vec2_in);
    GPU_shader_uniform_3fv(shader, "vec3_in", call_data.vec3_in);
    GPU_shader_uniform_4fv(shader, "vec4_in", call_data.vec4_in);
  }

  void dispatch()
  {
    /* Dispatching 1000000 times to add some stress to the GPU. Without it tests may succeed when
     * using too simple shaders. */
    GPU_compute_dispatch(shader, 1000, 1000, 1);
  }
};

/** Test the given info when doing a single call. */
static void do_push_constants_test(const char *info_name, const int num_calls_simultaneously = 1)
{
  if (!GPU_compute_shader_support() && !GPU_shader_storage_buffer_objects_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 16;

  Shader shader;
  shader.init_shader(info_name);

  for (const int call_index : IndexRange(num_calls_simultaneously)) {
    CallData &call_data = shader.new_call();
    call_data.generate_test_data(call_index * 10.0, call_index * 1.0);
    call_data.init_ssbo(SIZE);
    shader.bind(call_data);
    shader.update_push_constants(call_data);
    shader.dispatch();
  }
  /* All calls will be "simultaneously" in flight. First read-back will wait until the dispatches
   * have finished execution. */
  for (const int call_index : IndexRange(num_calls_simultaneously)) {
    CallData &call_data = shader.call_datas[call_index];
    call_data.read_back();
    call_data.validate();
  }
}

/* Test case with single call as sanity check, before we make it more interesting. */
static void test_push_constants()
{
  do_push_constants_test("gpu_push_constants_test");
}
GPU_TEST(push_constants)

static void test_push_constants_128bytes()
{
  do_push_constants_test("gpu_push_constants_128bytes_test");
}
GPU_TEST(push_constants_128bytes)

static void test_push_constants_256bytes()
{
  do_push_constants_test("gpu_push_constants_256bytes_test");
}
GPU_TEST(push_constants_256bytes)

static void test_push_constants_512bytes()
{
  do_push_constants_test("gpu_push_constants_512bytes_test");
}
GPU_TEST(push_constants_512bytes)

/* Schedule multiple simultaneously. */
static void test_push_constants_multiple()
{
  do_push_constants_test("gpu_push_constants_test", 10);
}
GPU_TEST(push_constants_multiple)

static void test_push_constants_multiple_128bytes()
{
  do_push_constants_test("gpu_push_constants_128bytes_test", 10);
}
GPU_TEST(push_constants_multiple_128bytes)

static void test_push_constants_multiple_256bytes()
{
  do_push_constants_test("gpu_push_constants_256bytes_test", 10);
}
GPU_TEST(push_constants_multiple_256bytes)

static void test_push_constants_multiple_512bytes()
{
  do_push_constants_test("gpu_push_constants_512bytes_test", 10);
}
GPU_TEST(push_constants_multiple_512bytes)

}  // namespace blender::gpu::tests
