/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIPRT

#  include "device/hiprt/device_impl.h"

#  include <hiprt/hiprt.h>
#  include <iomanip>

#  include "device/hip/util.h"
#  include "kernel/device/hiprt/globals.h"

#  include "util/log.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/progress.h"
#  include "util/string.h"
#  include "util/time.h"
#  include "util/types.h"
#  include "util/vector.h"

#  if defined(_WIN32)
#    include "util/windows.h"
#  elif defined(__linux__)
#    include <dlfcn.h>
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

bool HIPRTDevice::is_supported()
{
#  if defined(__linux__)
  static bool is_initialized = false;
  static bool is_supported = false;

  if (is_initialized) {
    return is_supported;
  }
  is_initialized = true;

  /* The current version of HIP-RT requires libamdhip64.so which Fedora puts in a separate package
   * than libamdhip64.so.6 as required by HIP. For now check for the existence of this. In the
   * future update we'll make HIP-RT consistent, and this code can be removed. */
  const vector<const char *> hip_paths = {
      "libamdhip64.so",
      "/opt/rocm/lib/libamdhip64.so",
      "/opt/rocm/hip/lib/libamdhip64.so",
  };

  LOG_INFO << "Checking for libamdhip64.so";

  for (const char *hip_path : hip_paths) {
    void *hip_lib = dlopen(hip_path, RTLD_LAZY);
    if (hip_lib) {
      LOG_DEBUG << "Found libamdhip64.so at: " << hip_path;
      is_supported = true;
      dlclose(hip_lib);
      break;
    }
  }

  if (!is_supported) {
    LOG_INFO << "libamdhip64.so not found, HIP-RT will be disabled";
  }

  return is_supported;
#  else
  return true;
#  endif
}

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
      hiprt_module_(nullptr),
      scene(nullptr),
      functions_table(nullptr),
      scratch_buffer_size(0),
      scratch_buffer(this, "scratch_buffer", MEM_DEVICE_ONLY),
      prim_visibility(this, "prim_visibility", MEM_GLOBAL),
      instance_transform_matrix(this, "instance_transform_matrix", MEM_READ_ONLY),
      transform_headers(this, "transform_headers", MEM_READ_ONLY),
      user_instance_id(this, "user_instance_id", MEM_GLOBAL),
      hiprt_blas_ptr(this, "hiprt_blas_ptr", MEM_READ_WRITE),
      blas_ptr(this, "blas_ptr", MEM_GLOBAL)
{
  HIPContextScope scope(this);
  global_stack_buffer = {0};
  hiprtContextCreationInput hiprt_context_input = {nullptr};
  hiprt_context_input.ctxt = hipContext;
  hiprt_context_input.device = hipDevice;
  hiprt_context_input.deviceType = hiprtDeviceAMD;
  hiprtError rt_result = hiprtCreateContext(HIPRT_API_VERSION, hiprt_context_input, hiprt_context);

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

  hiprtDestroyGlobalStackBuffer(hiprt_context, global_stack_buffer);
  hiprtDestroyFuncTable(hiprt_context, functions_table);
  hiprtDestroyScene(hiprt_context, scene);

  if (hiprt_module_) {
    hip_assert(hipModuleUnload(hiprt_module_));
  }

  hiprtDestroyContext(hiprt_context);
}

unique_ptr<DeviceQueue> HIPRTDevice::gpu_queue_create()
{
  return make_unique<HIPRTDeviceQueue>(this);
}

string HIPRTDevice::compile_kernel_get_common_cflags(const uint kernel_features)
{
  string cflags = HIPDevice::compile_kernel_get_common_cflags(kernel_features);

  cflags += " -D __KERNEL_HIPRT__ ";

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
  if (hiprt_module_) {
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
    result = hipModuleLoadData(&hiprt_module_, fatbin_data.c_str());
  }
  else {
    result = hipErrorFileNotFound;
  }

  if (result != hipSuccess) {
    set_error(string_printf(
        "Failed to load HIP kernel from '%s' (%s)", fatbin.c_str(), hipewErrorString(result)));
  }

  if (result != hipSuccess) {
    return false;
  }

  kernels.load_raytrace(this, hiprt_module_);

  return HIPDevice::load_kernels(kernel_features);
}

void HIPRTDevice::const_copy_to(const char *name, void *host, const size_t size)
{
  /* Set constant memory for HIP module. */
  HIPDevice::const_copy_to(name, host, size);

  HIPContextScope scope(this);
  hipDeviceptr_t mem;
  size_t bytes;

  if (strcmp(name, "data") == 0) {
    assert(size <= sizeof(KernelData));
    KernelData *const data = (KernelData *)host;
    *(hiprtScene *)&data->device_bvh = scene;
  }

  hip_assert(hipModuleGetGlobal(&mem, &bytes, hiprt_module_, "kernel_params"));
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

#  include "kernel/data_arrays.h"
#  undef KERNEL_DATA_ARRAY
}

hiprtGeometryBuildInput HIPRTDevice::prepare_triangle_blas(BVHHIPRT *bvh, Mesh *mesh)
{
  hiprtGeometryBuildInput geom_input;
  geom_input.geomType = Triangle;

  if (use_motion_blur && mesh->has_motion_blur()) {
    const Attribute *attr_P = mesh->attributes.find(ATTR_STD_POSITION);
    const size_t num_triangles = mesh->num_triangles();
    int num_bounds = 0;

    bvh->custom_primitive_bound.alloc(num_triangles);
    for (uint j = 0; j < num_triangles; j++) {
      Mesh::Triangle t = mesh->get_triangle(j);
      BoundBox bounds = BoundBox::empty;
      for (int attr_step = 0; attr_step < attr_P->num_motion_steps(); attr_step++) {
        t.bounds_grow(attr_P->data<packed_float3>(attr_step), bounds);
      }

      bvh->custom_primitive_bound[num_bounds] = bounds;
      num_bounds++;
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

    size_t vertex_size = mesh->num_verts();
    const packed_float3 *verts = mesh->get_position();

    bvh->triangle_mesh.triangleCount = mesh->num_triangles();
    bvh->triangle_mesh.triangleStride = 3 * sizeof(int);
    bvh->triangle_mesh.vertexCount = vertex_size;
    bvh->triangle_mesh.vertexStride = sizeof(packed_float3);

    /* TODO: reduce memory usage by avoiding copy. */
    int *triangle_index_data = bvh->triangle_index.resize(triangle_size);
    float *vertex_data_data = bvh->vertex_data.resize(vertex_size * 3);

    if (triangle_index_data && vertex_data_data) {
      std::copy_n(triangle_data, triangle_size, triangle_index_data);
      std::copy_n(verts, vertex_size, reinterpret_cast<packed_float3 *>(vertex_data_data));

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

  const size_t num_curves = hair->num_curves();
  const size_t num_segments = hair->num_segments();
  const Attribute *attr_P = hair->attributes.find(ATTR_STD_POSITION);
  const Attribute *attr_R = hair->attributes.find(ATTR_STD_RADIUS);
  const bool has_motion = use_motion_blur && hair->has_motion_blur() && attr_P->has_motion();

  bvh->custom_primitive_bound.alloc(num_segments);

  int num_bounds = 0;
  const packed_float3 *curve_keys = hair->get_position();

  for (uint j = 0; j < num_curves; j++) {
    const Hair::Curve curve = hair->get_curve(j);
    const float *curve_radius = hair->get_radius();
    int first_key = curve.first_key;
    for (int k = 0; k < curve.num_keys - 1; k++) {
      if (!has_motion) {
        float3 current_keys[4];
        current_keys[0] = curve_keys[max(first_key + k - 1, first_key)];
        current_keys[1] = curve_keys[first_key + k];
        current_keys[2] = curve_keys[first_key + k + 1];
        current_keys[3] = curve_keys[min(first_key + k + 2, first_key + curve.num_keys - 1)];

        BoundBox bounds = BoundBox::empty;
        curve.bounds_grow(k, hair->get_position(), curve_radius, bounds);

        bvh->custom_primitive_bound[num_bounds] = bounds;
        num_bounds++;
      }
      else {
        BoundBox bounds = BoundBox::empty;
        for (int attr_step = 0; attr_step < attr_P->num_motion_steps(); attr_step++) {
          curve.bounds_grow(
              k, attr_P->data<packed_float3>(attr_step), attr_R->data<float>(attr_step), bounds);
        }
        bvh->custom_primitive_bound[num_bounds] = bounds;
        num_bounds++;
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

  const Attribute *attr_P = nullptr;
  const Attribute *attr_R = nullptr;
  if (use_motion_blur && pointcloud->has_motion_blur()) {
    attr_P = pointcloud->attributes.find(ATTR_STD_POSITION);
    attr_R = pointcloud->attributes.find(ATTR_STD_RADIUS);
    if (!attr_P->has_motion()) {
      attr_P = nullptr;
      attr_R = nullptr;
    }
  }

  const packed_float3 *points_data = pointcloud->get_position();
  const float *radius_data = pointcloud->get_radius();
  const size_t num_points = pointcloud->num_points();

  int num_bounds = 0;

  if (attr_P == nullptr) {
    bvh->custom_primitive_bound.alloc(num_points);
    for (uint j = 0; j < num_points; j++) {
      const PointCloud::Point point = pointcloud->get_point(j);
      BoundBox bounds = BoundBox::empty;
      point.bounds_grow(points_data, radius_data, bounds);

      bvh->custom_primitive_bound[num_bounds] = bounds;
      num_bounds++;
    }
  }
  else {
    bvh->custom_primitive_bound.alloc(num_points);

    for (uint j = 0; j < num_points; j++) {
      const PointCloud::Point point = pointcloud->get_point(j);
      BoundBox bounds = BoundBox::empty;
      for (int attr_step = 0; attr_step < attr_P->num_motion_steps(); attr_step++) {
        point.bounds_grow(
            attr_P->data<packed_float3>(attr_step), attr_R->data<float>(attr_step), bounds);
      }

      bvh->custom_primitive_bound[num_bounds] = bounds;
      num_bounds++;
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

void HIPRTDevice::build_blas(BVHHIPRT *bvh, Geometry *geom)
{
  hiprtGeometryBuildInput geom_input = {};

  hiprtBuildOptions options = {
      .buildFlags = hiprtBuildFlagBitPreferHighQualityBuild,
  };

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
      options.buildFlags = hiprtBuildFlagBitPreferBalancedBuild;
      break;
    }

    case Geometry::POINTCLOUD: {
      PointCloud *pointcloud = static_cast<PointCloud *>(geom);
      if (pointcloud->num_points() == 0) {
        return;
      }
      geom_input = prepare_point_blas(bvh, pointcloud);
      options.buildFlags = hiprtBuildFlagBitPreferBalancedBuild;
      break;
    }

    case Geometry::AREA_LIGHT:
    case Geometry::BACKGROUND_LIGHT:
    case Geometry::POINT_LIGHT:
    case Geometry::SPOT_LIGHT:
    case Geometry::SUN_LIGHT:
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

hiprtScene HIPRTDevice::build_tlas(BVHHIPRT * /*bvh*/, const vector<Object *> &objects, bool refit)
{

  size_t num_object = objects.size();
  if (num_object == 0) {
    return nullptr;
  }

  const hiprtBuildOptions options = {
      .buildFlags = hiprtBuildFlagBitPreferHighQualityBuild,
  };

  hiprtBuildOperation build_operation = refit ? hiprtBuildOperationUpdate :
                                                hiprtBuildOperationBuild;

  array<hiprtFrameMatrix> transform_matrix;

  size_t num_instances = 0;
  int blender_instance_id = 0;

  user_instance_id.alloc(num_object);
  prim_visibility.alloc(num_object);
  hiprt_blas_ptr.alloc(num_object);
  blas_ptr.alloc(num_object);
  transform_headers.alloc(num_object);

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

  hip_assert(
      hipModuleGetGlobal(&table_device_ptr, &table_ptr_size, hiprt_module_, "kernel_params"));
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

  BVHHIPRT *bvh_rt = static_cast<BVHHIPRT *>(bvh);
  HIPContextScope scope(this);

  if (!bvh_rt->is_tlas()) {
    const vector<Geometry *> &geometry = bvh_rt->geometry;
    assert(geometry.size() == 1);
    build_blas(bvh_rt, geometry[0]);
  }
  else {
    if (scene) {
      hiprtDestroyScene(hiprt_context, scene);
      scene = nullptr;
    }
    scene = build_tlas(bvh_rt, bvh_rt->objects, refit);
  }
}
CCL_NAMESPACE_END

#endif
