/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIPRT

#  include <iomanip>

#  include "device/hip/util.h"
#  include "device/hiprt/device_impl.h"
#  include "kernel/device/hiprt/globals.h"

#  include "util/log.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/progress.h"
#  include "util/string.h"
#  include "util/time.h"
#  include "util/types.h"

#  ifdef _WIN32
#    include "util/windows.h"
#  endif

#  include "bvh/hiprt.h"

#  include "scene/hair.h"
#  include "scene/mesh.h"
#  include "scene/object.h"
#  include "scene/pointcloud.h"

CCL_NAMESPACE_BEGIN

static void get_hiprt_transform(float matrix[][4], Transform &tfm)
{
  int row = 0;
  int col = 0;
  matrix[row][col++] = tfm.x.x;
  matrix[row][col++] = tfm.x.y;
  matrix[row][col++] = tfm.x.z;
  matrix[row][col++] = tfm.x.w;
  row++;
  col = 0;
  matrix[row][col++] = tfm.y.x;
  matrix[row][col++] = tfm.y.y;
  matrix[row][col++] = tfm.y.z;
  matrix[row][col++] = tfm.y.w;
  row++;
  col = 0;
  matrix[row][col++] = tfm.z.x;
  matrix[row][col++] = tfm.z.y;
  matrix[row][col++] = tfm.z.z;
  matrix[row][col++] = tfm.z.w;
}

class HIPRTDevice;

BVHLayoutMask HIPRTDevice::get_bvh_layout_mask(const uint /* kernel_features */) const
{
  return BVH_LAYOUT_HIPRT;
}

HIPRTDevice::HIPRTDevice(const DeviceInfo &info,
                         Stats &stats,
                         Profiler &profiler,
                         const bool headless)
    : HIPDevice(info, stats, profiler, headless),
      hiprt_context(nullptr),
      scene(nullptr),
      functions_table(nullptr),
      scratch_buffer_size(0),
      scratch_buffer(this, "scratch_buffer", MEM_DEVICE_ONLY),
      prim_visibility(this, "prim_visibility", MEM_GLOBAL),
      instance_transform_matrix(this, "instance_transform_matrix", MEM_READ_ONLY),
      transform_headers(this, "transform_headers", MEM_READ_ONLY),
      user_instance_id(this, "user_instance_id", MEM_GLOBAL),
      hiprt_blas_ptr(this, "hiprt_blas_ptr", MEM_READ_WRITE),
      blas_ptr(this, "blas_ptr", MEM_GLOBAL),
      custom_prim_info(this, "custom_prim_info", MEM_GLOBAL),
      custom_prim_info_offset(this, "custom_prim_info_offset", MEM_GLOBAL),
      prims_time(this, "prims_time", MEM_GLOBAL),
      prim_time_offset(this, "prim_time_offset", MEM_GLOBAL)
{
  HIPContextScope scope(this);
  global_stack_buffer = {0};
  hiprtContextCreationInput hiprt_context_input = {nullptr};
  hiprt_context_input.ctxt = hipContext;
  hiprt_context_input.device = hipDevice;
  hiprt_context_input.deviceType = hiprtDeviceAMD;
  hiprtError rt_result = hiprtCreateContext(
      HIPRT_API_VERSION, hiprt_context_input, &hiprt_context);

  if (rt_result != hiprtSuccess) {
    set_error("Failed to create HIPRT context");
    return;
  }

  rt_result = hiprtCreateFuncTable(
      hiprt_context, Max_Primitive_Type, Max_Intersect_Filter_Function, functions_table);

  if (rt_result != hiprtSuccess) {
    set_error("Failed to create HIPRT Function Table");
    return;
  }

  if (LOG_IS_ON(LOG_LEVEL_TRACE)) {
    hiprtSetLogLevel(hiprtLogLevelInfo | hiprtLogLevelWarn | hiprtLogLevelError);
  }
  else {
    hiprtSetLogLevel(hiprtLogLevelNone);
  }
}

HIPRTDevice::~HIPRTDevice()
{
  HIPContextScope scope(this);
  free_bvh_memory_delayed();
  user_instance_id.free();
  prim_visibility.free();
  hiprt_blas_ptr.free();
  blas_ptr.free();
  instance_transform_matrix.free();
  transform_headers.free();
  custom_prim_info_offset.free();
  custom_prim_info.free();
  prim_time_offset.free();
  prims_time.free();

  hiprtDestroyGlobalStackBuffer(hiprt_context, global_stack_buffer);
  hiprtDestroyFuncTable(hiprt_context, functions_table);
  hiprtDestroyScene(hiprt_context, scene);
  hiprtDestroyContext(hiprt_context);
}

unique_ptr<DeviceQueue> HIPRTDevice::gpu_queue_create()
{
  return make_unique<HIPRTDeviceQueue>(this);
}

string HIPRTDevice::compile_kernel_get_common_cflags(const uint kernel_features)
{
  string cflags = HIPDevice::compile_kernel_get_common_cflags(kernel_features);

  cflags += " -D __HIPRT__ ";

  return cflags;
}

string HIPRTDevice::compile_kernel(const uint kernel_features, const char *name, const char *base)
{
  int major, minor;
  hipDeviceGetAttribute(&major, hipDeviceAttributeComputeCapabilityMajor, hipDevId);
  hipDeviceGetAttribute(&minor, hipDeviceAttributeComputeCapabilityMinor, hipDevId);
  const std::string arch = hipDeviceArch(hipDevId);

  if (!use_adaptive_compilation()) {
    const string fatbin = path_get(string_printf("lib/%s_rt_%s.hipfb.zst", name, arch.c_str()));
    LOG_INFO << "Testing for pre-compiled kernel " << fatbin << ".";
    if (path_exists(fatbin)) {
      LOG_INFO << "Using precompiled kernel.";
      return fatbin;
    }
  }

  string source_path = path_get("source");
  const string source_md5 = path_files_md5_hash(source_path);

  string common_cflags = compile_kernel_get_common_cflags(kernel_features);
  const string kernel_md5 = util_md5_string(source_md5 + common_cflags);

  const string include_path = source_path;
  const string fatbin_file = string_printf(
      "cycles_%s_%s_%s.hipfb", name, arch.c_str(), kernel_md5.c_str());
  const string fatbin = path_cache_get(path_join("kernels", fatbin_file));
  const string hiprt_include_path = path_join(source_path, "kernel/device/hiprt");

  LOG_INFO << "Testing for locally compiled kernel " << fatbin << ".";
  if (path_exists(fatbin)) {
    LOG_INFO << "Using locally compiled kernel.";
    return fatbin;
  }

#  ifdef _WIN32
  if (!use_adaptive_compilation() && have_precompiled_kernels()) {
    if (!hipSupportsDevice(hipDevId)) {
      set_error(
          string_printf("HIP backend requires compute capability 10.1 or up, but found %d.%d. "
                        "Your GPU is not supported.",
                        major,
                        minor));
    }
    else {
      set_error(
          string_printf("HIP binary kernel for this graphics card compute "
                        "capability (%d.%d) not found.",
                        major,
                        minor));
    }
    return string();
  }
#  endif

  const char *const hipcc = hipewCompilerPath();
  if (hipcc == nullptr) {
    set_error(
        "HIP hipcc compiler not found. "
        "Install HIP toolkit in default location.");
    return string();
  }

  const int hipcc_hip_version = hipewCompilerVersion();
  LOG_INFO << "Found hipcc " << hipcc << ", HIP version " << hipcc_hip_version << ".";
  if (hipcc_hip_version < 40) {
    LOG_WARNING << "Unsupported HIP version " << hipcc_hip_version / 10 << "."
                << hipcc_hip_version % 10 << ", you need HIP 4.0 or newer.\n";
    return string();
  }

  path_create_directories(fatbin);

  source_path = path_join(path_join(source_path, "kernel"),
                          path_join("device", path_join(base, string_printf("%s.cpp", name))));

  const char *const kernel_ext = "genco";
  string options;
  options.append("-Wno-parentheses-equality -Wno-unused-value -ffast-math -O3 -std=c++17");
  options.append(" --offload-arch=").append(arch.c_str());

  LOG_INFO_IMPORTANT << "Compiling " << source_path << " and caching to " << fatbin;

  double starttime = time_dt();

  string compile_command = string_printf("%s %s -I %s -I %s --%s %s -o \"%s\" %s",
                                         hipcc,
                                         options.c_str(),
                                         include_path.c_str(),
                                         hiprt_include_path.c_str(),
                                         kernel_ext,
                                         source_path.c_str(),
                                         fatbin.c_str(),
                                         common_cflags.c_str());

  LOG_INFO_IMPORTANT << "Compiling " << ((use_adaptive_compilation()) ? "adaptive " : "")
                     << "HIP-RT kernel ... " << compile_command;

#  ifdef _WIN32
  compile_command = "call " + compile_command;
#  endif
  if (system(compile_command.c_str()) != 0) {
    set_error(
        "Failed to execute linking command, "
        "see console for details.");
    return string();
  }

  LOG_INFO_IMPORTANT << "Kernel compilation finished in " << std::fixed << std::setprecision(2)
                     << time_dt() - starttime << "s";

  return fatbin;
}

bool HIPRTDevice::load_kernels(const uint kernel_features)
{
  if (hipModule) {
    if (use_adaptive_compilation()) {
      LOG_INFO << "Skipping HIP kernel reload for adaptive compilation, not currently supported.";
    }
    return true;
  }

  if (hipContext == nullptr) {
    return false;
  }

  if (!support_device(kernel_features)) {
    return false;
  }

  /* Keep track of whether motion blur is enabled, so to enable/disable motion in BVH builds
   * This is necessary since objects may be reported to have motion if the Vector pass is
   * active, but may still need to be rendered without motion blur if that isn't active as well.
   */
  use_motion_blur = use_motion_blur || (kernel_features & KERNEL_FEATURE_OBJECT_MOTION);

  /* get kernel */
  const char *kernel_name = "kernel";
  string fatbin = compile_kernel(kernel_features, kernel_name);
  if (fatbin.empty()) {
    return false;
  }

  /* open module */
  HIPContextScope scope(this);

  string fatbin_data;
  hipError_t result;

  if (path_read_compressed_text(fatbin, fatbin_data)) {
    result = hipModuleLoadData(&hipModule, fatbin_data.c_str());
  }
  else {
    result = hipErrorFileNotFound;
  }

  if (result != hipSuccess) {
    set_error(string_printf(
        "Failed to load HIP kernel from '%s' (%s)", fatbin.c_str(), hipewErrorString(result)));
  }

  if (result == hipSuccess) {
    kernels.load(this);
    {
      const DeviceKernel test_kernel = (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) ?
                                           DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE :
                                       (kernel_features & KERNEL_FEATURE_MNEE) ?
                                           DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE :
                                           DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE;

      HIPRTDeviceQueue queue(this);

      device_ptr d_path_index = 0;
      device_ptr d_render_buffer = 0;
      int d_work_size = 0;
      DeviceKernelArguments args(&d_path_index, &d_render_buffer, &d_work_size);

      queue.init_execution();
      queue.enqueue(test_kernel, 1, args);
      queue.synchronize();
    }
  }

  return (result == hipSuccess);
}

void HIPRTDevice::const_copy_to(const char *name, void *host, const size_t size)
{
  HIPContextScope scope(this);
  hipDeviceptr_t mem;
  size_t bytes;

  if (strcmp(name, "data") == 0) {
    assert(size <= sizeof(KernelData));
    KernelData *const data = (KernelData *)host;
    *(hiprtScene *)&data->device_bvh = scene;
  }

  hip_assert(hipModuleGetGlobal(&mem, &bytes, hipModule, "kernel_params"));
  assert(bytes == sizeof(KernelParamsHIPRT));

#  define KERNEL_DATA_ARRAY(data_type, data_name) \
    if (strcmp(name, #data_name) == 0) { \
      hip_assert(hipMemcpyHtoD(mem + offsetof(KernelParamsHIPRT, data_name), host, size)); \
      return; \
    }
  KERNEL_DATA_ARRAY(KernelData, data)
  KERNEL_DATA_ARRAY(IntegratorStateGPU, integrator_state)
  KERNEL_DATA_ARRAY(int, user_instance_id)
  KERNEL_DATA_ARRAY(uint64_t, blas_ptr)
  KERNEL_DATA_ARRAY(int2, custom_prim_info_offset)
  KERNEL_DATA_ARRAY(int2, custom_prim_info)
  KERNEL_DATA_ARRAY(int, prim_time_offset)
  KERNEL_DATA_ARRAY(float2, prims_time)

#  include "kernel/data_arrays.h"
#  undef KERNEL_DATA_ARRAY
}

hiprtGeometryBuildInput HIPRTDevice::prepare_triangle_blas(BVHHIPRT *bvh, Mesh *mesh)
{
  hiprtGeometryBuildInput geom_input;
  geom_input.geomType = Triangle;

  if (use_motion_blur && mesh->has_motion_blur()) {

    const Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    const float3 *vert_steps = attr_mP->data_float3();
    const size_t num_verts = mesh->get_verts().size();
    const size_t num_steps = mesh->get_motion_steps();
    const size_t num_triangles = mesh->num_triangles();
    const float3 *verts = mesh->get_verts().data();
    int num_bounds = 0;

    if (bvh->params.num_motion_triangle_steps == 0 || bvh->params.use_spatial_split) {
      bvh->custom_primitive_bound.alloc(num_triangles);
      bvh->custom_prim_info.resize(num_triangles);
      for (uint j = 0; j < num_triangles; j++) {
        Mesh::Triangle t = mesh->get_triangle(j);
        BoundBox bounds = BoundBox::empty;
        t.bounds_grow(verts, bounds);
        for (size_t step = 0; step < num_steps - 1; step++) {
          t.bounds_grow(vert_steps + step * num_verts, bounds);
        }

        if (bounds.valid()) {
          bvh->custom_primitive_bound[num_bounds] = bounds;
          bvh->custom_prim_info[num_bounds].x = j;
          bvh->custom_prim_info[num_bounds].y = mesh->primitive_type();
          num_bounds++;
        }
      }
    }
    else {
      const int num_bvh_steps = bvh->params.num_motion_triangle_steps * 2 + 1;
      const float num_bvh_steps_inv_1 = 1.0f / (num_bvh_steps - 1);

      bvh->custom_primitive_bound.alloc(num_triangles * num_bvh_steps);
      bvh->custom_prim_info.resize(num_triangles * num_bvh_steps);
      bvh->prims_time.resize(num_triangles * num_bvh_steps);

      for (uint j = 0; j < num_triangles; j++) {
        Mesh::Triangle t = mesh->get_triangle(j);
        float3 prev_verts[3];
        t.motion_verts(verts, vert_steps, num_verts, num_steps, 0.0f, prev_verts);
        BoundBox prev_bounds = BoundBox::empty;
        prev_bounds.grow(prev_verts[0]);
        prev_bounds.grow(prev_verts[1]);
        prev_bounds.grow(prev_verts[2]);

        for (int bvh_step = 1; bvh_step < num_bvh_steps; ++bvh_step) {
          const float curr_time = (float)(bvh_step)*num_bvh_steps_inv_1;
          float3 curr_verts[3];
          t.motion_verts(verts, vert_steps, num_verts, num_steps, curr_time, curr_verts);
          BoundBox curr_bounds = BoundBox::empty;
          curr_bounds.grow(curr_verts[0]);
          curr_bounds.grow(curr_verts[1]);
          curr_bounds.grow(curr_verts[2]);
          BoundBox bounds = prev_bounds;
          bounds.grow(curr_bounds);
          if (bounds.valid()) {
            const float prev_time = (float)(bvh_step - 1) * num_bvh_steps_inv_1;
            bvh->custom_primitive_bound[num_bounds] = bounds;
            bvh->custom_prim_info[num_bounds].x = j;
            bvh->custom_prim_info[num_bounds].y = mesh->primitive_type();
            bvh->prims_time[num_bounds].x = curr_time;
            bvh->prims_time[num_bounds].y = prev_time;
            num_bounds++;
          }
          prev_bounds = curr_bounds;
        }
      }
    }

    bvh->custom_prim_aabb.aabbCount = num_bounds;
    bvh->custom_prim_aabb.aabbStride = sizeof(BoundBox);
    bvh->custom_primitive_bound.copy_to_device();
    bvh->custom_prim_aabb.aabbs = (void *)bvh->custom_primitive_bound.device_pointer;

    geom_input.type = hiprtPrimitiveTypeAABBList;
    geom_input.primitive.aabbList = bvh->custom_prim_aabb;
    geom_input.geomType = Motion_Triangle;

    if (bvh->custom_primitive_bound.device_pointer == 0) {
      set_error("Failed to allocate triangle custom_primitive_bound for BLAS");
    }
  }
  else {
    size_t triangle_size = mesh->get_triangles().size();
    int *triangle_data = mesh->get_triangles().data();

    size_t vertex_size = mesh->get_verts().size();
    float *vertex_data = reinterpret_cast<float *>(mesh->get_verts().data());

    bvh->triangle_mesh.triangleCount = mesh->num_triangles();
    bvh->triangle_mesh.triangleStride = 3 * sizeof(int);
    bvh->triangle_mesh.vertexCount = vertex_size;
    bvh->triangle_mesh.vertexStride = sizeof(float3);

    /* TODO: reduce memory usage by avoiding copy. */
    int *triangle_index_data = bvh->triangle_index.resize(triangle_size);
    float *vertex_data_data = bvh->vertex_data.resize(vertex_size * 4);

    if (triangle_index_data && vertex_data_data) {
      std::copy_n(triangle_data, triangle_size, triangle_index_data);
      std::copy_n(vertex_data, vertex_size * 4, vertex_data_data);
      static_assert(sizeof(float3) == sizeof(float) * 4);

      bvh->triangle_index.copy_to_device();
      bvh->vertex_data.copy_to_device();
    }

    bvh->triangle_mesh.triangleIndices = (void *)(bvh->triangle_index.device_pointer);
    bvh->triangle_mesh.vertices = (void *)(bvh->vertex_data.device_pointer);

    geom_input.type = hiprtPrimitiveTypeTriangleMesh;
    geom_input.primitive.triangleMesh = bvh->triangle_mesh;

    if (bvh->triangle_index.device_pointer == 0 || bvh->vertex_data.device_pointer == 0) {
      set_error("Failed to allocate triangle data for BLAS");
    }
  }

  return geom_input;
}

hiprtGeometryBuildInput HIPRTDevice::prepare_curve_blas(BVHHIPRT *bvh, Hair *hair)
{
  hiprtGeometryBuildInput geom_input;

  const PrimitiveType primitive_type = hair->primitive_type();
  const size_t num_curves = hair->num_curves();
  const size_t num_segments = hair->num_segments();
  const Attribute *curve_attr_mP = nullptr;

  if (use_motion_blur && hair->has_motion_blur()) {
    curve_attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  if (curve_attr_mP == nullptr || bvh->params.num_motion_curve_steps == 0) {
    bvh->custom_prim_info.resize(num_segments);
    bvh->custom_primitive_bound.alloc(num_segments);
  }
  else {
    size_t num_boxes = bvh->params.num_motion_curve_steps * 2 * num_segments;
    bvh->custom_prim_info.resize(num_boxes);
    bvh->prims_time.resize(num_boxes);
    bvh->custom_primitive_bound.alloc(num_boxes);
  }

  int num_bounds = 0;
  float3 *curve_keys = hair->get_curve_keys().data();

  for (uint j = 0; j < num_curves; j++) {
    const Hair::Curve curve = hair->get_curve(j);
    const float *curve_radius = hair->get_curve_radius().data();
    int first_key = curve.first_key;
    for (int k = 0; k < curve.num_keys - 1; k++) {
      if (curve_attr_mP == nullptr) {
        float3 current_keys[4];
        current_keys[0] = curve_keys[max(first_key + k - 1, first_key)];
        current_keys[1] = curve_keys[first_key + k];
        current_keys[2] = curve_keys[first_key + k + 1];
        current_keys[3] = curve_keys[min(first_key + k + 2, first_key + curve.num_keys - 1)];

        if (current_keys[0].x == current_keys[1].x && current_keys[1].x == current_keys[2].x &&
            current_keys[2].x == current_keys[3].x && current_keys[0].y == current_keys[1].y &&
            current_keys[1].y == current_keys[2].y && current_keys[2].y == current_keys[3].y &&
            current_keys[0].z == current_keys[1].z && current_keys[1].z == current_keys[2].z &&
            current_keys[2].z == current_keys[3].z)
        {
          continue;
        }

        BoundBox bounds = BoundBox::empty;
        curve.bounds_grow(k, hair->get_curve_keys().data(), curve_radius, bounds);
        if (bounds.valid()) {
          int type = PRIMITIVE_PACK_SEGMENT(primitive_type, k);
          bvh->custom_prim_info[num_bounds].x = j;
          bvh->custom_prim_info[num_bounds].y = type;
          bvh->custom_primitive_bound[num_bounds] = bounds;
          num_bounds++;
        }
      }
      else {
        const size_t num_steps = hair->get_motion_steps();
        const float4 *key_steps = curve_attr_mP->data_float4();
        const size_t num_keys = hair->get_curve_keys().size();

        if (bvh->params.num_motion_curve_steps == 0 || bvh->params.use_spatial_split) {
          BoundBox bounds = BoundBox::empty;
          curve.bounds_grow(k, hair->get_curve_keys().data(), curve_radius, bounds);
          for (size_t step = 0; step < num_steps - 1; step++) {
            curve.bounds_grow(k, key_steps + step * num_keys, bounds);
          }
          if (bounds.valid()) {
            int type = PRIMITIVE_PACK_SEGMENT(primitive_type, k);
            bvh->custom_prim_info[num_bounds].x = j;
            bvh->custom_prim_info[num_bounds].y = type;
            bvh->custom_primitive_bound[num_bounds] = bounds;
            num_bounds++;
          }
        }
        else {
          const int num_bvh_steps = bvh->params.num_motion_curve_steps * 2 + 1;
          const float num_bvh_steps_inv_1 = 1.0f / (num_bvh_steps - 1);

          float4 prev_keys[4];
          curve.cardinal_motion_keys(curve_keys,
                                     curve_radius,
                                     key_steps,
                                     num_keys,
                                     num_steps,
                                     0.0f,
                                     k - 1,
                                     k,
                                     k + 1,
                                     k + 2,
                                     prev_keys);
          BoundBox prev_bounds = BoundBox::empty;
          curve.bounds_grow(prev_keys, prev_bounds);

          for (int bvh_step = 1; bvh_step < num_bvh_steps; ++bvh_step) {
            const float curr_time = (float)(bvh_step)*num_bvh_steps_inv_1;
            float4 curr_keys[4];
            curve.cardinal_motion_keys(curve_keys,
                                       curve_radius,
                                       key_steps,
                                       num_keys,
                                       num_steps,
                                       curr_time,
                                       k - 1,
                                       k,
                                       k + 1,
                                       k + 2,
                                       curr_keys);
            BoundBox curr_bounds = BoundBox::empty;
            curve.bounds_grow(curr_keys, curr_bounds);
            BoundBox bounds = prev_bounds;
            bounds.grow(curr_bounds);
            if (bounds.valid()) {
              const float prev_time = (float)(bvh_step - 1) * num_bvh_steps_inv_1;
              int packed_type = PRIMITIVE_PACK_SEGMENT(primitive_type, k);
              bvh->custom_prim_info[num_bounds].x = j;
              bvh->custom_prim_info[num_bounds].y = packed_type;  // k
              bvh->custom_primitive_bound[num_bounds] = bounds;
              bvh->prims_time[num_bounds].x = prev_time;
              bvh->prims_time[num_bounds].y = curr_time;
              num_bounds++;
            }
            prev_bounds = curr_bounds;
          }
        }
      }
    }
  }

  bvh->custom_prim_aabb.aabbCount = num_bounds;
  bvh->custom_prim_aabb.aabbStride = sizeof(BoundBox);
  bvh->custom_primitive_bound.copy_to_device();
  bvh->custom_prim_aabb.aabbs = (void *)bvh->custom_primitive_bound.device_pointer;

  geom_input.type = hiprtPrimitiveTypeAABBList;
  geom_input.primitive.aabbList = bvh->custom_prim_aabb;
  geom_input.geomType = Curve;

  if (bvh->custom_primitive_bound.device_pointer == 0) {
    set_error("Failed to allocate curve custom_primitive_bound for BLAS");
  }

  return geom_input;
}

hiprtGeometryBuildInput HIPRTDevice::prepare_point_blas(BVHHIPRT *bvh, PointCloud *pointcloud)
{
  hiprtGeometryBuildInput geom_input;

  const Attribute *point_attr_mP = nullptr;
  if (use_motion_blur && pointcloud->has_motion_blur()) {
    point_attr_mP = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  const float3 *points_data = pointcloud->get_points().data();
  const float *radius_data = pointcloud->get_radius().data();
  const size_t num_points = pointcloud->num_points();
  const float4 *motion_data = (point_attr_mP) ? point_attr_mP->data_float4() : nullptr;
  const size_t num_steps = pointcloud->get_motion_steps();

  int num_bounds = 0;

  if (point_attr_mP == nullptr) {
    bvh->custom_prim_info.resize(num_points);
    bvh->custom_primitive_bound.alloc(num_points);
    for (uint j = 0; j < num_points; j++) {
      const PointCloud::Point point = pointcloud->get_point(j);
      BoundBox bounds = BoundBox::empty;
      point.bounds_grow(points_data, radius_data, bounds);
      if (bounds.valid()) {
        bvh->custom_primitive_bound[num_bounds] = bounds;
        bvh->custom_prim_info[num_bounds].x = j;
        bvh->custom_prim_info[num_bounds].y = PRIMITIVE_POINT;
        num_bounds++;
      }
    }
  }
  else if (bvh->params.num_motion_point_steps == 0 || bvh->params.use_spatial_split) {
    bvh->custom_prim_info.resize(num_points);
    bvh->custom_primitive_bound.alloc(num_points);

    for (uint j = 0; j < num_points; j++) {
      const PointCloud::Point point = pointcloud->get_point(j);
      BoundBox bounds = BoundBox::empty;
      point.bounds_grow(points_data, radius_data, bounds);
      for (size_t step = 0; step < num_steps - 1; step++) {
        point.bounds_grow(motion_data[step * num_points + j], bounds);
      }
      if (bounds.valid()) {
        bvh->custom_primitive_bound[num_bounds] = bounds;
        bvh->custom_prim_info[num_bounds].x = j;
        bvh->custom_prim_info[num_bounds].y = PRIMITIVE_MOTION_POINT;
        num_bounds++;
      }
    }
  }
  else {
    const int num_bvh_steps = bvh->params.num_motion_point_steps * 2 + 1;
    const float num_bvh_steps_inv_1 = 1.0f / (num_bvh_steps - 1);

    bvh->custom_prim_info.resize(num_points * num_bvh_steps);
    bvh->custom_primitive_bound.alloc(num_points * num_bvh_steps);
    bvh->prims_time.resize(num_points * num_bvh_steps);

    for (uint j = 0; j < num_points; j++) {
      const PointCloud::Point point = pointcloud->get_point(j);
      const size_t num_steps = pointcloud->get_motion_steps();
      const float4 *point_steps = point_attr_mP->data_float4();

      float4 prev_key = point.motion_key(
          points_data, radius_data, point_steps, num_points, num_steps, 0.0f, j);
      BoundBox prev_bounds = BoundBox::empty;
      point.bounds_grow(prev_key, prev_bounds);

      for (int bvh_step = 1; bvh_step < num_bvh_steps; ++bvh_step) {
        const float curr_time = (float)(bvh_step)*num_bvh_steps_inv_1;
        float4 curr_key = point.motion_key(
            points_data, radius_data, point_steps, num_points, num_steps, curr_time, j);
        BoundBox curr_bounds = BoundBox::empty;
        point.bounds_grow(curr_key, curr_bounds);
        BoundBox bounds = prev_bounds;
        bounds.grow(curr_bounds);
        if (bounds.valid()) {
          const float prev_time = (float)(bvh_step - 1) * num_bvh_steps_inv_1;
          bvh->custom_primitive_bound[num_bounds] = bounds;
          bvh->custom_prim_info[num_bounds].x = j;
          bvh->custom_prim_info[num_bounds].y = PRIMITIVE_MOTION_POINT;
          bvh->prims_time[num_bounds].x = prev_time;
          bvh->prims_time[num_bounds].y = curr_time;
          num_bounds++;
        }
        prev_bounds = curr_bounds;
      }
    }
  }

  bvh->custom_prim_aabb.aabbCount = num_bounds;
  bvh->custom_prim_aabb.aabbStride = sizeof(BoundBox);
  bvh->custom_primitive_bound.copy_to_device();
  bvh->custom_prim_aabb.aabbs = (void *)bvh->custom_primitive_bound.device_pointer;

  geom_input.type = hiprtPrimitiveTypeAABBList;
  geom_input.primitive.aabbList = bvh->custom_prim_aabb;
  geom_input.geomType = Point;

  if (bvh->custom_primitive_bound.device_pointer == 0) {
    set_error("Failed to allocate point custom_primitive_bound for BLAS");
  }

  return geom_input;
}

void HIPRTDevice::build_blas(BVHHIPRT *bvh, Geometry *geom, hiprtBuildOptions options)
{
  hiprtGeometryBuildInput geom_input = {};

  switch (geom->geometry_type) {
    case Geometry::MESH:
    case Geometry::VOLUME: {
      Mesh *mesh = static_cast<Mesh *>(geom);

      if (mesh->num_triangles() == 0) {
        return;
      }

      geom_input = prepare_triangle_blas(bvh, mesh);
      break;
    }

    case Geometry::HAIR: {
      Hair *const hair = static_cast<Hair *const>(geom);

      if (hair->num_segments() == 0) {
        return;
      }

      geom_input = prepare_curve_blas(bvh, hair);
      break;
    }

    case Geometry::POINTCLOUD: {
      PointCloud *pointcloud = static_cast<PointCloud *>(geom);
      if (pointcloud->num_points() == 0) {
        return;
      }

      geom_input = prepare_point_blas(bvh, pointcloud);
      break;
    }

    case Geometry::LIGHT:
      return;

    default:
      assert(geom_input.geomType != hiprtInvalidValue);
  }

  if (have_error()) {
    return;
  }

  size_t blas_scratch_buffer_size = 0;
  hiprtError rt_err = hiprtGetGeometryBuildTemporaryBufferSize(
      hiprt_context, geom_input, options, blas_scratch_buffer_size);

  if (rt_err != hiprtSuccess) {
    set_error("Failed to get scratch buffer size for BLAS");
    return;
  }

  rt_err = hiprtCreateGeometry(hiprt_context, geom_input, options, bvh->hiprt_geom);

  if (rt_err != hiprtSuccess) {
    set_error("Failed to create BLAS");
    return;
  }
  {
    thread_scoped_lock lock(hiprt_mutex);
    if (blas_scratch_buffer_size > scratch_buffer_size) {
      scratch_buffer.alloc(blas_scratch_buffer_size);
      scratch_buffer.zero_to_device();
      if (!scratch_buffer.device_pointer) {
        hiprtDestroyGeometry(hiprt_context, bvh->hiprt_geom);
        bvh->hiprt_geom = nullptr;
        set_error("Failed to allocate scratch buffer for BLAS");
        return;
      }
      scratch_buffer_size = blas_scratch_buffer_size;
    }
    bvh->geom_input = geom_input;
    rt_err = hiprtBuildGeometry(hiprt_context,
                                hiprtBuildOperationBuild,
                                bvh->geom_input,
                                options,
                                (void *)(scratch_buffer.device_pointer),
                                nullptr,
                                bvh->hiprt_geom);
  }
  if (rt_err != hiprtSuccess) {
    set_error("Failed to build BLAS");
  }
}

hiprtScene HIPRTDevice::build_tlas(BVHHIPRT *bvh,
                                   const vector<Object *> &objects,
                                   hiprtBuildOptions options,
                                   bool refit)
{

  size_t num_object = objects.size();
  if (num_object == 0) {
    return nullptr;
  }

  hiprtBuildOperation build_operation = refit ? hiprtBuildOperationUpdate :
                                                hiprtBuildOperationBuild;

  array<hiprtFrameMatrix> transform_matrix;

  unordered_map<Geometry *, int2> prim_info_map;
  size_t custom_prim_offset = 0;

  unordered_map<Geometry *, int> prim_time_map;

  size_t num_instances = 0;
  int blender_instance_id = 0;

  user_instance_id.alloc(num_object);
  prim_visibility.alloc(num_object);
  hiprt_blas_ptr.alloc(num_object);
  blas_ptr.alloc(num_object);
  transform_headers.alloc(num_object);
  custom_prim_info_offset.alloc(num_object);
  prim_time_offset.alloc(num_object);

  for (Object *ob : objects) {
    uint32_t mask = 0;
    if (ob->is_traceable()) {
      mask = ob->visibility_for_tracing();
    }

    Transform current_transform = ob->get_tfm();
    Geometry *geom = ob->get_geometry();
    bool transform_applied = geom->transform_applied;

    BVHHIPRT *current_bvh = static_cast<BVHHIPRT *>(geom->bvh.get());
    bool is_valid_geometry = current_bvh->geom_input.geomType != hiprtInvalidValue;
    hiprtGeometry hiprt_geom_current = current_bvh->hiprt_geom;

    hiprtFrameMatrix hiprt_transform_matrix = {{{0}}};
    Transform identity_matrix = transform_identity();
    get_hiprt_transform(hiprt_transform_matrix.matrix, identity_matrix);

    if (is_valid_geometry) {
      bool is_custom_prim = current_bvh->custom_prim_info.size() > 0;

      if (is_custom_prim) {

        bool has_motion_blur = current_bvh->prims_time.size() > 0;

        unordered_map<Geometry *, int2>::iterator it = prim_info_map.find(geom);

        if (prim_info_map.find(geom) != prim_info_map.end()) {

          custom_prim_info_offset[blender_instance_id] = it->second;

          if (has_motion_blur) {

            prim_time_offset[blender_instance_id] = prim_time_map[geom];
          }
        }
        else {
          int offset = bvh->custom_prim_info.size();

          prim_info_map[geom].x = offset;
          prim_info_map[geom].y = custom_prim_offset;

          bvh->custom_prim_info.resize(offset + current_bvh->custom_prim_info.size());
          memcpy(bvh->custom_prim_info.data() + offset,
                 current_bvh->custom_prim_info.data(),
                 current_bvh->custom_prim_info.size() * sizeof(int2));

          custom_prim_info_offset[blender_instance_id].x = offset;
          custom_prim_info_offset[blender_instance_id].y = custom_prim_offset;

          if (geom->is_hair()) {
            custom_prim_offset += ((Hair *)geom)->num_curves();
          }
          else if (geom->is_pointcloud()) {
            custom_prim_offset += ((PointCloud *)geom)->num_points();
          }
          else {
            custom_prim_offset += ((Mesh *)geom)->num_triangles();
          }

          if (has_motion_blur) {
            int time_offset = bvh->prims_time.size();
            prim_time_map[geom] = time_offset;

            bvh->prims_time.resize(time_offset + current_bvh->prims_time.size());
            memcpy(bvh->prims_time.data() + time_offset,
                   current_bvh->prims_time.data(),
                   current_bvh->prims_time.size() * sizeof(float2));

            prim_time_offset[blender_instance_id] = time_offset;
          }
          else {
            prim_time_offset[blender_instance_id] = -1;
          }
        }
      }
      else {
        custom_prim_info_offset[blender_instance_id] = {-1, -1};
      }

      hiprtTransformHeader current_header = {0};
      current_header.frameCount = 1;
      current_header.frameIndex = transform_matrix.size();
      if (use_motion_blur && ob->get_motion().size()) {
        int motion_size = ob->get_motion().size();
        assert(motion_size != 1);

        array<Transform> tfm_array = ob->get_motion();
        float time_iternval = 1 / (float)(motion_size - 1);
        current_header.frameCount = motion_size;

        vector<hiprtFrameMatrix> tfm_hiprt_mb;
        tfm_hiprt_mb.resize(motion_size);
        for (int i = 0; i < motion_size; i++) {
          get_hiprt_transform(tfm_hiprt_mb[i].matrix, tfm_array[i]);
          tfm_hiprt_mb[i].time = (float)i * time_iternval;
          transform_matrix.push_back_slow(tfm_hiprt_mb[i]);
        }
      }
      else {
        if (transform_applied) {
          current_transform = identity_matrix;
        }
        get_hiprt_transform(hiprt_transform_matrix.matrix, current_transform);
        transform_matrix.push_back_slow(hiprt_transform_matrix);
      }

      transform_headers[num_instances] = current_header;

      user_instance_id[num_instances] = blender_instance_id;
      prim_visibility[num_instances] = mask;
      hiprt_blas_ptr[num_instances].geometry = hiprt_geom_current;
      hiprt_blas_ptr[num_instances].type = hiprtInstanceTypeGeometry;
      num_instances++;
    }
    blas_ptr[blender_instance_id] = (uint64_t)hiprt_geom_current;
    blender_instance_id++;
  }

  size_t table_ptr_size = 0;
  hipDeviceptr_t table_device_ptr;

  hip_assert(hipModuleGetGlobal(&table_device_ptr, &table_ptr_size, hipModule, "kernel_params"));
  if (have_error()) {
    return nullptr;
  }

  size_t kernel_param_offset[4];
  int table_index = 0;
  kernel_param_offset[table_index++] = offsetof(KernelParamsHIPRT, table_closest_intersect);
  kernel_param_offset[table_index++] = offsetof(KernelParamsHIPRT, table_shadow_intersect);
  kernel_param_offset[table_index++] = offsetof(KernelParamsHIPRT, table_local_intersect);
  kernel_param_offset[table_index++] = offsetof(KernelParamsHIPRT, table_volume_intersect);

  for (int index = 0; index < table_index; index++) {
    hip_assert(hipMemcpyHtoD(table_device_ptr + kernel_param_offset[index],
                             (void *)&functions_table,
                             sizeof(device_ptr)));
    if (have_error()) {
      return nullptr;
    }
  }

  if (num_instances == 0) {
    return nullptr;
  }

  int frame_count = transform_matrix.size();
  hiprtSceneBuildInput scene_input_ptr = {nullptr};
  scene_input_ptr.instanceCount = num_instances;
  scene_input_ptr.frameCount = frame_count;
  scene_input_ptr.frameType = hiprtFrameTypeMatrix;

  user_instance_id.copy_to_device();
  prim_visibility.copy_to_device();
  hiprt_blas_ptr.copy_to_device();
  blas_ptr.copy_to_device();
  transform_headers.copy_to_device();

  if (user_instance_id.device_pointer == 0 || prim_visibility.device_pointer == 0 ||
      hiprt_blas_ptr.device_pointer == 0 || blas_ptr.device_pointer == 0 ||
      transform_headers.device_pointer == 0)
  {
    set_error("Failed to allocate object buffers for TLAS");
    return nullptr;
  }

  {
    /* TODO: reduce memory usage by avoiding copy. */
    hiprtFrameMatrix *instance_transform_matrix_data = instance_transform_matrix.resize(
        frame_count);
    if (instance_transform_matrix_data == nullptr) {
      set_error("Failed to allocate host instance_transform_matrix for TLAS");
      return nullptr;
    }

    std::copy_n(transform_matrix.data(), frame_count, instance_transform_matrix_data);
    instance_transform_matrix.copy_to_device();

    if (instance_transform_matrix.device_pointer == 0) {
      set_error("Failed to allocate instance_transform_matrix for TLAS");
      return nullptr;
    }
  }

  scene_input_ptr.instanceMasks = (void *)prim_visibility.device_pointer;
  scene_input_ptr.instances = (void *)hiprt_blas_ptr.device_pointer;
  scene_input_ptr.instanceTransformHeaders = (void *)transform_headers.device_pointer;
  scene_input_ptr.instanceFrames = (void *)instance_transform_matrix.device_pointer;

  hiprtScene scene = nullptr;

  hiprtError rt_err = hiprtCreateScene(hiprt_context, scene_input_ptr, options, scene);

  if (rt_err != hiprtSuccess) {
    set_error("Failed to create TLAS");
    return nullptr;
  }

  size_t tlas_scratch_buffer_size;
  rt_err = hiprtGetSceneBuildTemporaryBufferSize(
      hiprt_context, scene_input_ptr, options, tlas_scratch_buffer_size);

  if (rt_err != hiprtSuccess) {
    set_error("Failed to get scratch buffer size for TLAS");
    hiprtDestroyScene(hiprt_context, scene);
    return nullptr;
  }

  if (tlas_scratch_buffer_size > scratch_buffer_size) {
    scratch_buffer.alloc(tlas_scratch_buffer_size);
    scratch_buffer.zero_to_device();
    if (scratch_buffer.device_pointer == 0) {
      set_error("Failed to allocate scratch buffer for TLAS");
      hiprtDestroyScene(hiprt_context, scene);
      return nullptr;
    }
  }

  rt_err = hiprtBuildScene(hiprt_context,
                           build_operation,
                           scene_input_ptr,
                           options,
                           (void *)scratch_buffer.device_pointer,
                           nullptr,
                           scene);

  scratch_buffer.free();
  scratch_buffer_size = 0;

  if (rt_err != hiprtSuccess) {
    set_error("Failed to build TLAS");
    hiprtDestroyScene(hiprt_context, scene);
    return nullptr;
  }

  if (bvh->custom_prim_info.size()) {
    /* TODO: reduce memory usage by avoiding copy. */
    const size_t data_size = bvh->custom_prim_info.size();
    int2 *custom_prim_info_data = custom_prim_info.resize(data_size);
    if (custom_prim_info_data == nullptr) {
      set_error("Failed to allocate host custom_prim_info_data for TLAS");
      hiprtDestroyScene(hiprt_context, scene);
      return nullptr;
    }

    std::copy_n(bvh->custom_prim_info.data(), data_size, custom_prim_info_data);

    custom_prim_info.copy_to_device();
    custom_prim_info_offset.copy_to_device();
    if (custom_prim_info.device_pointer == 0 || custom_prim_info_offset.device_pointer == 0) {
      set_error("Failed to allocate custom_prim_info_offset for TLAS");
      hiprtDestroyScene(hiprt_context, scene);
      return nullptr;
    }
  }

  if (bvh->prims_time.size()) {
    /* TODO: reduce memory usage by avoiding copy. */
    const size_t data_size = bvh->prims_time.size();
    float2 *prims_time_data = prims_time.resize(data_size);
    if (prims_time_data == nullptr) {
      set_error("Failed to allocate host prims_time for TLAS");
      hiprtDestroyScene(hiprt_context, scene);
      return nullptr;
    }

    std::copy_n(bvh->prims_time.data(), data_size, prims_time_data);

    prims_time.copy_to_device();
    prim_time_offset.copy_to_device();

    if (prim_time_offset.device_pointer == 0 || prims_time.device_pointer == 0) {
      set_error("Failed to allocate prims_time for TLAS");
      hiprtDestroyScene(hiprt_context, scene);
      return nullptr;
    }
  }

  return scene;
}

void HIPRTDevice::free_bvh_memory_delayed()
{
  thread_scoped_lock lock(hiprt_mutex);
  if (stale_bvh.size()) {
    for (int bvh_index = 0; bvh_index < stale_bvh.size(); bvh_index++) {
      hiprtGeometry hiprt_geom = stale_bvh[bvh_index];
      hiprtDestroyGeometry(hiprt_context, hiprt_geom);
      hiprt_geom = nullptr;
    }
    stale_bvh.clear();
  }
}

void HIPRTDevice::release_bvh(BVH *bvh)
{
  BVHHIPRT *current_bvh = static_cast<BVHHIPRT *>(bvh);
  thread_scoped_lock lock(hiprt_mutex);
  /* Tracks BLAS pointers whose BVH destructors have been called. */
  stale_bvh.push_back(current_bvh->hiprt_geom);
}

void HIPRTDevice::build_bvh(BVH *bvh, Progress &progress, bool refit)
{
  if (have_error()) {
    return;
  }
  free_bvh_memory_delayed();
  progress.set_substatus("Building HIPRT acceleration structure");

  hiprtBuildOptions options;
  options.buildFlags = hiprtBuildFlagBitPreferHighQualityBuild;

  BVHHIPRT *bvh_rt = static_cast<BVHHIPRT *>(bvh);
  HIPContextScope scope(this);

  if (!bvh_rt->is_tlas()) {
    const vector<Geometry *> &geometry = bvh_rt->geometry;
    assert(geometry.size() == 1);
    build_blas(bvh_rt, geometry[0], options);
  }
  else {

    if (scene) {
      hiprtDestroyScene(hiprt_context, scene);
      scene = nullptr;
    }
    scene = build_tlas(bvh_rt, bvh_rt->objects, options, refit);
  }
}
CCL_NAMESPACE_END

#endif
