/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2023 Blender Foundation */

#ifdef WITH_HIPRT

#  include "device/hiprt/device_impl.h"

#  include "util/debug.h"
#  include "util/foreach.h"
#  include "util/log.h"
#  include "util/map.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/progress.h"
#  include "util/string.h"
#  include "util/system.h"
#  include "util/time.h"
#  include "util/types.h"
#  include "util/windows.h"

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

HIPRTDevice::HIPRTDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler)
    : HIPDevice(info, stats, profiler),
      global_stack_buffer(this, "global_stack_buffer", MEM_DEVICE_ONLY),
      hiprt_context(NULL),
      scene(NULL),
      functions_table(NULL),
      scratch_buffer_size(0),
      scratch_buffer(this, "scratch_buffer", MEM_DEVICE_ONLY),
      visibility(this, "visibility", MEM_READ_ONLY),
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
  hiprtContextCreationInput hiprt_context_input = {0};
  hiprt_context_input.ctxt = hipContext;
  hiprt_context_input.device = hipDevice;
  hiprt_context_input.deviceType = hiprtDeviceAMD;
  hiprtError rt_result = hiprtCreateContext(
      HIPRT_API_VERSION, hiprt_context_input, &hiprt_context);

  if (rt_result != hiprtSuccess) {
    set_error(string_printf("Failed to create HIPRT context"));
    return;
  }

  rt_result = hiprtCreateFuncTable(
      hiprt_context, Max_Primitive_Type, Max_Intersect_Filter_Function, &functions_table);

  if (rt_result != hiprtSuccess) {
    set_error(string_printf("Failed to create HIPRT Function Table"));
    return;
  }
}

HIPRTDevice::~HIPRTDevice()
{
  HIPContextScope scope(this);
  user_instance_id.free();
  visibility.free();
  hiprt_blas_ptr.free();
  blas_ptr.free();
  instance_transform_matrix.free();
  transform_headers.free();
  custom_prim_info_offset.free();
  custom_prim_info.free();
  prim_time_offset.free();
  prims_time.free();
  global_stack_buffer.free();
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
  hipDeviceProp_t props;
  hipGetDeviceProperties(&props, hipDevId);

  char *arch = strtok(props.gcnArchName, ":");
  if (arch == NULL) {
    arch = props.gcnArchName;
  }

  if (!use_adaptive_compilation()) {
    const string fatbin = path_get(string_printf("lib/%s_rt_gfx.hipfb", name));
    VLOG(1) << "Testing for pre-compiled kernel " << fatbin << ".";
    if (path_exists(fatbin)) {
      VLOG(1) << "Using precompiled kernel.";
      return fatbin;
    }
  }

  string source_path = path_get("source");
  const string source_md5 = path_files_md5_hash(source_path);

  string common_cflags = compile_kernel_get_common_cflags(kernel_features);
  const string kernel_md5 = util_md5_string(source_md5 + common_cflags);

  const string include_path = source_path;
  const string bitcode_file = string_printf("cycles_%s_%s_%s.bc", name, arch, kernel_md5.c_str());
  const string bitcode = path_cache_get(path_join("kernels", bitcode_file));
  const string fatbin_file = string_printf(
      "cycles_%s_%s_%s.hipfb", name, arch, kernel_md5.c_str());
  const string fatbin = path_cache_get(path_join("kernels", fatbin_file));

  VLOG(1) << "Testing for locally compiled kernel " << fatbin << ".";
  if (path_exists(fatbin)) {
    VLOG(1) << "Using locally compiled kernel.";
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
  if (hipcc == NULL) {
    set_error(
        "HIP hipcc compiler not found. "
        "Install HIP toolkit in default location.");
    return string();
  }

  const int hipcc_hip_version = hipewCompilerVersion();
  VLOG_INFO << "Found hipcc " << hipcc << ", HIP version " << hipcc_hip_version << ".";
  if (hipcc_hip_version < 40) {
    printf(
        "Unsupported HIP version %d.%d detected, "
        "you need HIP 4.0 or newer.\n",
        hipcc_hip_version / 10,
        hipcc_hip_version % 10);
    return string();
  }

  path_create_directories(fatbin);

  source_path = path_join(path_join(source_path, "kernel"),
                          path_join("device", path_join(base, string_printf("%s.cpp", name))));

  printf("Compiling  %s and caching to %s", source_path.c_str(), fatbin.c_str());

  double starttime = time_dt();

  const string hiprt_path = getenv("HIPRT_ROOT_DIR");
  // First, app kernels are compiled into bitcode, without access to implementation of HIP RT
  // functions
  if (!path_exists(bitcode)) {

    std::string rtc_options;

    rtc_options.append(" --offload-arch=").append(arch);
    rtc_options.append(" -D __HIPRT__");
    rtc_options.append(" -ffast-math -O3 -std=c++17");
    rtc_options.append(" -fgpu-rdc -c --gpu-bundle-output -c -emit-llvm");

    string command = string_printf("%s %s -I %s  -I %s %s -o \"%s\"",
                                   hipcc,
                                   rtc_options.c_str(),
                                   include_path.c_str(),
                                   hiprt_path.c_str(),
                                   source_path.c_str(),
                                   bitcode.c_str());

    printf("Compiling %sHIP kernel ...\n%s\n",
           (use_adaptive_compilation()) ? "adaptive " : "",
           command.c_str());

#  ifdef _WIN32
    command = "call " + command;
#  endif
    if (system(command.c_str()) != 0) {
      set_error(
          "Failed to execute compilation command, "
          "see console for details.");
      return string();
    }
  }

  // After compilation, the bitcode produced is linked with HIP RT bitcode (containing
  // implementations of HIP RT functions, e.g. traversal, to produce the final executable code
  string linker_options;
  linker_options.append(" --offload-arch=").append(arch);
  linker_options.append(" -fgpu-rdc --hip-link --cuda-device-only ");
  string hiprt_ver(HIPRT_VERSION_STR);
  string hiprt_bc;
  hiprt_bc = hiprt_path + "\\hiprt" + hiprt_ver + "_amd_lib_win.bc";

  string linker_command = string_printf("clang++ %s \"%s\" %s -o \"%s\"",
                                        linker_options.c_str(),
                                        bitcode.c_str(),
                                        hiprt_bc.c_str(),
                                        fatbin.c_str());

#  ifdef _WIN32
  linker_command = "call " + linker_command;
#  endif
  if (system(linker_command.c_str()) != 0) {
    set_error(
        "Failed to execute linking command, "
        "see console for details.");
    return string();
  }

  printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

  return fatbin;
}

bool HIPRTDevice::load_kernels(const uint kernel_features)
{
  if (hipModule) {
    if (use_adaptive_compilation()) {
      VLOG(1) << "Skipping HIP kernel reload for adaptive compilation, not currently supported.";
    }
    return true;
  }

  if (hipContext == 0)
    return false;

  if (!support_device(kernel_features)) {
    return false;
  }

  /* get kernel */
  const char *kernel_name = "kernel";
  string fatbin = compile_kernel(kernel_features, kernel_name);
  if (fatbin.empty())
    return false;

  /* open module */
  HIPContextScope scope(this);

  string fatbin_data;
  hipError_t result;

  if (path_read_text(fatbin, fatbin_data)) {

    result = hipModuleLoadData(&hipModule, fatbin_data.c_str());
  }
  else
    result = hipErrorFileNotFound;

  if (result != hipSuccess)
    set_error(string_printf(
        "Failed to load HIP kernel from '%s' (%s)", fatbin.c_str(), hipewErrorString(result)));

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

void HIPRTDevice::const_copy_to(const char *name, void *host, size_t size)
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

  if (mesh->has_motion_blur() &&
      !(bvh->params.num_motion_triangle_steps == 0 || bvh->params.use_spatial_split))
  {

    const Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    const size_t num_triangles = mesh->num_triangles();

    const int num_bvh_steps = bvh->params.num_motion_triangle_steps * 2 + 1;
    const float num_bvh_steps_inv_1 = 1.0f / (num_bvh_steps - 1);

    int num_bounds = 0;
    bvh->custom_primitive_bound.alloc(num_triangles * num_bvh_steps);

    for (uint j = 0; j < num_triangles; j++) {
      Mesh::Triangle t = mesh->get_triangle(j);
      const float3 *verts = mesh->get_verts().data();

      const size_t num_verts = mesh->get_verts().size();
      const size_t num_steps = mesh->get_motion_steps();
      const float3 *vert_steps = attr_mP->data_float3();

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

    bvh->custom_prim_aabb.aabbCount = bvh->custom_primitive_bound.size();
    bvh->custom_prim_aabb.aabbStride = sizeof(BoundBox);
    bvh->custom_primitive_bound.copy_to_device();
    bvh->custom_prim_aabb.aabbs = (void *)bvh->custom_primitive_bound.device_pointer;

    geom_input.type = hiprtPrimitiveTypeAABBList;
    geom_input.aabbList.primitive = &bvh->custom_prim_aabb;
    geom_input.geomType = Motion_Triangle;
  }
  else {

    size_t triangle_size = mesh->get_triangles().size();
    void *triangle_data = mesh->get_triangles().data();

    size_t vertex_size = mesh->get_verts().size();
    void *vertex_data = mesh->get_verts().data();

    bvh->triangle_mesh.triangleCount = mesh->num_triangles();
    bvh->triangle_mesh.triangleStride = 3 * sizeof(int);
    bvh->triangle_mesh.vertexCount = vertex_size;
    bvh->triangle_mesh.vertexStride = sizeof(float3);

    bvh->triangle_index.host_pointer = triangle_data;
    bvh->triangle_index.data_elements = 1;
    bvh->triangle_index.data_type = TYPE_INT;
    bvh->triangle_index.data_size = triangle_size;
    bvh->triangle_index.copy_to_device();
    bvh->triangle_mesh.triangleIndices = (void *)(bvh->triangle_index.device_pointer);
    // either has to set the host pointer to zero, or increment the refcount on triangle_data
    bvh->triangle_index.host_pointer = 0;
    bvh->vertex_data.host_pointer = vertex_data;
    bvh->vertex_data.data_elements = 4;
    bvh->vertex_data.data_type = TYPE_FLOAT;
    bvh->vertex_data.data_size = vertex_size;
    bvh->vertex_data.copy_to_device();
    bvh->triangle_mesh.vertices = (void *)(bvh->vertex_data.device_pointer);
    bvh->vertex_data.host_pointer = 0;

    geom_input.type = hiprtPrimitiveTypeTriangleMesh;
    geom_input.triangleMesh.primitive = &(bvh->triangle_mesh);
  }
  return geom_input;
}

hiprtGeometryBuildInput HIPRTDevice::prepare_curve_blas(BVHHIPRT *bvh, Hair *hair)
{
  hiprtGeometryBuildInput geom_input;

  const PrimitiveType primitive_type = hair->primitive_type();
  const size_t num_curves = hair->num_curves();
  const size_t num_segments = hair->num_segments();
  const Attribute *curve_attr_mP = NULL;

  if (curve_attr_mP == NULL || bvh->params.num_motion_curve_steps == 0) {

    bvh->custom_prim_info.resize(num_segments);
    bvh->custom_primitive_bound.alloc(num_segments);
  }
  else {
    size_t num_boxes = bvh->params.num_motion_curve_steps * 2 * num_segments;
    bvh->custom_prim_info.resize(num_boxes);
    bvh->custom_primitive_bound.alloc(num_boxes);
    curve_attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  int num_bounds = 0;
  float3 *curve_keys = hair->get_curve_keys().data();

  for (uint j = 0; j < num_curves; j++) {
    const Hair::Curve curve = hair->get_curve(j);
    const float *curve_radius = &hair->get_curve_radius()[0];
    int first_key = curve.first_key;
    for (int k = 0; k < curve.num_keys - 1; k++) {
      if (curve_attr_mP == NULL || bvh->params.num_motion_curve_steps == 0) {
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
          continue;

        BoundBox bounds = BoundBox::empty;
        curve.bounds_grow(k, &hair->get_curve_keys()[0], curve_radius, bounds);
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
        const size_t num_steps = hair->get_motion_steps();
        const float3 *curve_keys = &hair->get_curve_keys()[0];
        const float4 *key_steps = curve_attr_mP->data_float4();
        const size_t num_keys = hair->get_curve_keys().size();

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
            bvh->prims_time[num_bounds].x = curr_time;
            bvh->prims_time[num_bounds].y = prev_time;
            num_bounds++;
          }
          prev_bounds = curr_bounds;
        }
      }
    }
  }

  bvh->custom_prim_aabb.aabbCount = num_bounds;
  bvh->custom_prim_aabb.aabbStride = sizeof(BoundBox);
  bvh->custom_primitive_bound.copy_to_device();
  bvh->custom_prim_aabb.aabbs = (void *)bvh->custom_primitive_bound.device_pointer;

  geom_input.type = hiprtPrimitiveTypeAABBList;
  geom_input.aabbList.primitive = &bvh->custom_prim_aabb;
  geom_input.geomType = Curve;

  return geom_input;
}

hiprtGeometryBuildInput HIPRTDevice::prepare_point_blas(BVHHIPRT *bvh, PointCloud *pointcloud)
{
  hiprtGeometryBuildInput geom_input;

  const Attribute *point_attr_mP = NULL;
  if (pointcloud->has_motion_blur()) {
    point_attr_mP = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  const float3 *points_data = pointcloud->get_points().data();
  const float *radius_data = pointcloud->get_radius().data();
  const size_t num_points = pointcloud->num_points();
  const float3 *motion_data = (point_attr_mP) ? point_attr_mP->data_float3() : NULL;
  const size_t num_steps = pointcloud->get_motion_steps();

  int num_bounds = 0;

  if (point_attr_mP == NULL) {
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
  else if (bvh->params.num_motion_point_steps == 0) {

    bvh->custom_primitive_bound.alloc(num_points * num_steps);

    for (uint j = 0; j < num_points; j++) {
      const PointCloud::Point point = pointcloud->get_point(j);
      BoundBox bounds = BoundBox::empty;
      point.bounds_grow(points_data, radius_data, bounds);
      for (size_t step = 0; step < num_steps - 1; step++) {
        point.bounds_grow(motion_data + step * num_points, radius_data, bounds);
      }
      if (bounds.valid()) {
        bvh->custom_primitive_bound[num_bounds] = bounds;
        bvh->custom_prim_info[num_bounds].x = j;
        bvh->custom_prim_info[num_bounds].y = PRIMITIVE_POINT;
        num_bounds++;
      }
    }
  }
  else {

    const int num_bvh_steps = bvh->params.num_motion_point_steps * 2 + 1;
    const float num_bvh_steps_inv_1 = 1.0f / (num_bvh_steps - 1);

    bvh->custom_primitive_bound.alloc(num_points * num_bvh_steps);

    for (uint j = 0; j < num_points; j++) {
      const PointCloud::Point point = pointcloud->get_point(j);
      const size_t num_steps = pointcloud->get_motion_steps();
      const float3 *point_steps = point_attr_mP->data_float3();

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
          bvh->prims_time[num_bounds].x = curr_time;
          bvh->prims_time[num_bounds].y = prev_time;
          num_bounds++;
        }
        prev_bounds = curr_bounds;
      }
    }
  }

  bvh->custom_prim_aabb.aabbCount = bvh->custom_primitive_bound.size();
  bvh->custom_prim_aabb.aabbStride = sizeof(BoundBox);
  bvh->custom_primitive_bound.copy_to_device();
  bvh->custom_prim_aabb.aabbs = (void *)bvh->custom_primitive_bound.device_pointer;

  geom_input.type = hiprtPrimitiveTypeAABBList;
  geom_input.aabbList.primitive = &bvh->custom_prim_aabb;
  geom_input.geomType = Point;

  return geom_input;
}

void HIPRTDevice::build_blas(BVHHIPRT *bvh, Geometry *geom, hiprtBuildOptions options)
{
  hiprtGeometryBuildInput geom_input = {};

  switch (geom->geometry_type) {
    case Geometry::MESH:
    case Geometry::VOLUME: {
      Mesh *mesh = static_cast<Mesh *>(geom);

      if (mesh->num_triangles() == 0)
        return;

      geom_input = prepare_triangle_blas(bvh, mesh);
      break;
    }

    case Geometry::HAIR: {
      Hair *const hair = static_cast<Hair *const>(geom);

      if (hair->num_segments() == 0)
        return;

      geom_input = prepare_curve_blas(bvh, hair);
      break;
    }

    case Geometry::POINTCLOUD: {
      PointCloud *pointcloud = static_cast<PointCloud *>(geom);
      if (pointcloud->num_points() == 0)
        return;

      geom_input = prepare_point_blas(bvh, pointcloud);
      break;
    }

    default:
      assert(geom_input.geomType != hiprtInvalidValue);
  }

  size_t blas_scratch_buffer_size = 0;
  hiprtError rt_err = hiprtGetGeometryBuildTemporaryBufferSize(
      hiprt_context, &geom_input, &options, &blas_scratch_buffer_size);

  if (rt_err != hiprtSuccess) {
    set_error(string_printf("Failed to get scratch buffer size for BLAS!"));
  }

  rt_err = hiprtCreateGeometry(hiprt_context, &geom_input, &options, &bvh->hiprt_geom);

  if (rt_err != hiprtSuccess) {
    set_error(string_printf("Failed to create BLAS!"));
  }
  bvh->geom_input = geom_input;
  {
    thread_scoped_lock lock(hiprt_mutex);
    if (blas_scratch_buffer_size > scratch_buffer_size) {
      scratch_buffer.alloc(blas_scratch_buffer_size);
      scratch_buffer_size = blas_scratch_buffer_size;
      scratch_buffer.zero_to_device();
    }
    rt_err = hiprtBuildGeometry(hiprt_context,
                                hiprtBuildOperationBuild,
                                &bvh->geom_input,
                                &options,
                                (void *)(scratch_buffer.device_pointer),
                                0,
                                bvh->hiprt_geom);
  }
  if (rt_err != hiprtSuccess) {
    set_error(string_printf("Failed to build BLAS"));
  }
}

hiprtScene HIPRTDevice::build_tlas(BVHHIPRT *bvh,
                                   vector<Object *> objects,
                                   hiprtBuildOptions options,
                                   bool refit)
{
  hiprtBuildOperation build_operation = refit ? hiprtBuildOperationUpdate :
                                                hiprtBuildOperationBuild;

  array<hiprtFrameMatrix> transform_matrix;

  unordered_map<Geometry *, int2> prim_info_map;
  size_t custom_prim_offset = 0;

  unordered_map<Geometry *, int> prim_time_map;

  size_t num_instances = 0;
  int blender_instance_id = 0;

  size_t num_object = objects.size();
  user_instance_id.alloc(num_object);
  visibility.alloc(num_object);
  hiprt_blas_ptr.alloc(num_object);
  blas_ptr.alloc(num_object);
  transform_headers.alloc(num_object);
  custom_prim_info_offset.alloc(num_object);
  prim_time_offset.alloc(num_object);

  foreach (Object *ob, objects) {
    uint32_t mask = 0;
    if (ob->is_traceable()) {
      mask = ob->visibility_for_tracing();
    }

    Transform current_transform = ob->get_tfm();
    Geometry *geom = ob->get_geometry();
    bool transform_applied = geom->transform_applied;

    BVHHIPRT *current_bvh = static_cast<BVHHIPRT *>(geom->bvh);
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

          if (geom->geometry_type == Geometry::HAIR) {
            custom_prim_offset += ((Hair *)geom)->num_curves();
          }
          else if (geom->geometry_type == Geometry::POINTCLOUD) {
            custom_prim_offset += ((PointCloud *)geom)->num_points();
          }
          else {
            custom_prim_offset += ((Mesh *)geom)->num_triangles();
          }

          if (has_motion_blur) {
            int time_offset = bvh->prims_time.size();
            prim_time_map[geom] = time_offset;

            memcpy(bvh->prims_time.data() + time_offset,
                   current_bvh->prims_time.data(),
                   current_bvh->prims_time.size() * sizeof(float2));

            prim_time_offset[blender_instance_id] = time_offset;
          }
          else
            prim_time_offset[blender_instance_id] = -1;
        }
      }
      else
        custom_prim_info_offset[blender_instance_id] = {-1, -1};

      hiprtTransformHeader current_header = {0};
      current_header.frameCount = 1;
      current_header.frameIndex = transform_matrix.size();
      if (ob->get_motion().size()) {
        int motion_size = ob->get_motion().size();
        assert(motion_size == 1);

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
        if (transform_applied)
          current_transform = identity_matrix;
        get_hiprt_transform(hiprt_transform_matrix.matrix, current_transform);
        transform_matrix.push_back_slow(hiprt_transform_matrix);
      }

      transform_headers[num_instances] = current_header;

      user_instance_id[num_instances] = blender_instance_id;
      visibility[num_instances] = mask;
      hiprt_blas_ptr[num_instances] = (uint64_t)hiprt_geom_current;
      num_instances++;
    }
    blas_ptr[blender_instance_id] = (uint64_t)hiprt_geom_current;
    blender_instance_id++;
  }

  int frame_count = transform_matrix.size();
  hiprtSceneBuildInput scene_input_ptr = {0};
  scene_input_ptr.instanceCount = num_instances;
  scene_input_ptr.frameCount = frame_count;
  scene_input_ptr.frameType = hiprtFrameTypeMatrix;

  user_instance_id.copy_to_device();
  visibility.copy_to_device();
  hiprt_blas_ptr.copy_to_device();
  blas_ptr.copy_to_device();
  transform_headers.copy_to_device();
  {
    instance_transform_matrix.alloc(frame_count);
    instance_transform_matrix.host_pointer = transform_matrix.data();
    instance_transform_matrix.data_elements = sizeof(hiprtFrameMatrix);
    instance_transform_matrix.data_type = TYPE_UCHAR;
    instance_transform_matrix.data_size = frame_count;
    instance_transform_matrix.copy_to_device();
    instance_transform_matrix.host_pointer = 0;
  }

  scene_input_ptr.instanceMasks = (void *)visibility.device_pointer;
  scene_input_ptr.instanceGeometries = (void *)hiprt_blas_ptr.device_pointer;
  scene_input_ptr.instanceTransformHeaders = (void *)transform_headers.device_pointer;
  scene_input_ptr.instanceFrames = (void *)instance_transform_matrix.device_pointer;

  hiprtScene scene = 0;

  hiprtError rt_err = hiprtCreateScene(hiprt_context, &scene_input_ptr, &options, &scene);

  if (rt_err != hiprtSuccess) {
    set_error(string_printf("Failed to create TLAS"));
  }

  size_t tlas_scratch_buffer_size;
  rt_err = hiprtGetSceneBuildTemporaryBufferSize(
      hiprt_context, &scene_input_ptr, &options, &tlas_scratch_buffer_size);

  if (rt_err != hiprtSuccess) {
    set_error(string_printf("Failed to get scratch buffer size for TLAS"));
  }

  if (tlas_scratch_buffer_size > scratch_buffer_size) {
    scratch_buffer.alloc(tlas_scratch_buffer_size);
    scratch_buffer.zero_to_device();
  }

  rt_err = hiprtBuildScene(hiprt_context,
                           build_operation,
                           &scene_input_ptr,
                           &options,
                           (void *)scratch_buffer.device_pointer,
                           0,
                           scene);

  if (rt_err != hiprtSuccess) {
    set_error(string_printf("Failed to build TLAS"));
  }

  scratch_buffer.free();
  scratch_buffer_size = 0;

  if (bvh->custom_prim_info.size()) {
    size_t data_size = bvh->custom_prim_info.size();
    custom_prim_info.alloc(data_size);
    custom_prim_info.host_pointer = bvh->custom_prim_info.data();
    custom_prim_info.data_elements = 2;
    custom_prim_info.data_type = TYPE_INT;
    custom_prim_info.data_size = data_size;
    custom_prim_info.copy_to_device();
    custom_prim_info.host_pointer = 0;

    custom_prim_info_offset.copy_to_device();
  }

  if (bvh->prims_time.size()) {
    size_t data_size = bvh->prims_time.size();
    prims_time.alloc(data_size);
    prims_time.host_pointer = bvh->prims_time.data();
    prims_time.data_elements = 2;
    prims_time.data_type = TYPE_FLOAT;
    prims_time.data_size = data_size;
    prims_time.copy_to_device();
    prims_time.host_pointer = 0;

    prim_time_offset.copy_to_device();
  }

  size_t table_ptr_size = 0;
  hipDeviceptr_t table_device_ptr;

  hip_assert(hipModuleGetGlobal(&table_device_ptr, &table_ptr_size, hipModule, "kernel_params"));

  size_t kernel_param_offset[4];
  int table_index = 0;
  kernel_param_offset[table_index++] = offsetof(KernelParamsHIPRT, table_closest_intersect);
  kernel_param_offset[table_index++] = offsetof(KernelParamsHIPRT, table_shadow_intersect);
  kernel_param_offset[table_index++] = offsetof(KernelParamsHIPRT, table_local_intersect);
  kernel_param_offset[table_index++] = offsetof(KernelParamsHIPRT, table_volume_intersect);

  for (int index = 0; index < table_index; index++) {

    hip_assert(hipMemcpyHtoD(
        table_device_ptr + kernel_param_offset[index], &functions_table, sizeof(device_ptr)));
  }

  return scene;
}

void HIPRTDevice::build_bvh(BVH *bvh, Progress &progress, bool refit)
{
  progress.set_substatus("Building HIPRT acceleration structure");

  hiprtBuildOptions options;
  options.buildFlags = hiprtBuildFlagBitPreferHighQualityBuild;

  BVHHIPRT *bvh_rt = static_cast<BVHHIPRT *>(bvh);
  HIPContextScope scope(this);

  if (!bvh_rt->is_tlas()) {
    vector<Geometry *> geometry = bvh_rt->geometry;
    assert(geometry.size() == 1);
    Geometry *geom = geometry[0];
    build_blas(bvh_rt, geom, options);
  }
  else {

    const vector<Object *> objects = bvh_rt->objects;
    scene = build_tlas(bvh_rt, objects, options, refit);
  }
}
CCL_NAMESPACE_END

#endif
