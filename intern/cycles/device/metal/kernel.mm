/*
 * Copyright 2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef WITH_METAL

#  include "device/metal/kernel.h"
#  include "device/metal/device_impl.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/tbb.h"
#  include "util/time.h"

CCL_NAMESPACE_BEGIN

/* limit to 2 MTLCompiler instances */
int max_mtlcompiler_threads = 2;

const char *kernel_type_as_string(int kernel_type)
{
  switch (kernel_type) {
    case PSO_GENERIC:
      return "PSO_GENERIC";
    case PSO_SPECIALISED:
      return "PSO_SPECIALISED";
    default:
      assert(0);
  }
  return "";
}

MetalDeviceKernel::~MetalDeviceKernel()
{
  for (int i = 0; i < PSO_NUM; i++) {
    pso[i].release();
  }
}

bool MetalDeviceKernel::load(MetalDevice *device,
                             MetalKernelLoadDesc const &desc_in,
                             MD5Hash const &md5)
{
  __block MetalKernelLoadDesc const desc(desc_in);
  if (desc.kernel_index == DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL) {
    /* skip megakernel */
    return true;
  }

  bool use_binary_archive = true;
  if (device->device_vendor == METAL_GPU_APPLE) {
    /* Workaround for T94142: Cycles Metal crash with simultaneous viewport and final render */
    use_binary_archive = false;
  }

  if (auto str = getenv("CYCLES_METAL_DISABLE_BINARY_ARCHIVES")) {
    use_binary_archive = (atoi(str) == 0);
  }

  id<MTLBinaryArchive> archive = nil;
  string metalbin_path;
  if (use_binary_archive) {
    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    string osVersion = [[processInfo operatingSystemVersionString] UTF8String];
    MD5Hash local_md5(md5);
    local_md5.append(osVersion);
    string metalbin_name = string(desc.function_name) + "." + local_md5.get_hex() +
                           to_string(desc.pso_index) + ".bin";
    metalbin_path = path_cache_get(path_join("kernels", metalbin_name));
    path_create_directories(metalbin_path);

    if (path_exists(metalbin_path) && use_binary_archive) {
      if (@available(macOS 11.0, *)) {
        MTLBinaryArchiveDescriptor *archiveDesc = [[MTLBinaryArchiveDescriptor alloc] init];
        archiveDesc.url = [NSURL fileURLWithPath:@(metalbin_path.c_str())];
        archive = [device->mtlDevice newBinaryArchiveWithDescriptor:archiveDesc error:nil];
        [archiveDesc release];
      }
    }
  }

  NSString *entryPoint = [@(desc.function_name) copy];

  NSError *error = NULL;
  if (@available(macOS 11.0, *)) {
    MTLFunctionDescriptor *func_desc = [MTLIntersectionFunctionDescriptor functionDescriptor];
    func_desc.name = entryPoint;
    if (desc.constant_values) {
      func_desc.constantValues = desc.constant_values;
    }
    pso[desc.pso_index].function = [device->mtlLibrary[desc.pso_index]
        newFunctionWithDescriptor:func_desc
                            error:&error];
  }
  [entryPoint release];

  if (pso[desc.pso_index].function == nil) {
    NSString *err = [error localizedDescription];
    string errors = [err UTF8String];

    device->set_error(
        string_printf("Error getting function \"%s\": %s", desc.function_name, errors.c_str()));
    return false;
  }

  pso[desc.pso_index].function.label = [@(desc.function_name) copy];

  __block MTLComputePipelineDescriptor *computePipelineStateDescriptor =
      [[MTLComputePipelineDescriptor alloc] init];

  computePipelineStateDescriptor.buffers[0].mutability = MTLMutabilityImmutable;
  computePipelineStateDescriptor.buffers[1].mutability = MTLMutabilityImmutable;
  computePipelineStateDescriptor.buffers[2].mutability = MTLMutabilityImmutable;

  if (@available(macos 10.14, *)) {
    computePipelineStateDescriptor.maxTotalThreadsPerThreadgroup = desc.threads_per_threadgroup;
  }
  computePipelineStateDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = true;

  computePipelineStateDescriptor.computeFunction = pso[desc.pso_index].function;
  if (@available(macOS 11.0, *)) {
    /* Attach the additional functions to an MTLLinkedFunctions object */
    if (desc.linked_functions) {
      computePipelineStateDescriptor.linkedFunctions = [[MTLLinkedFunctions alloc] init];
      computePipelineStateDescriptor.linkedFunctions.functions = desc.linked_functions;
    }

    computePipelineStateDescriptor.maxCallStackDepth = 1;
  }

  /* Create a new Compute pipeline state object */
  MTLPipelineOption pipelineOptions = MTLPipelineOptionNone;

  bool creating_new_archive = false;
  if (@available(macOS 11.0, *)) {
    if (use_binary_archive) {
      if (!archive) {
        MTLBinaryArchiveDescriptor *archiveDesc = [[MTLBinaryArchiveDescriptor alloc] init];
        archiveDesc.url = nil;
        archive = [device->mtlDevice newBinaryArchiveWithDescriptor:archiveDesc error:nil];
        creating_new_archive = true;

        double starttime = time_dt();

        if (![archive addComputePipelineFunctionsWithDescriptor:computePipelineStateDescriptor
                                                          error:&error]) {
          NSString *errStr = [error localizedDescription];
          metal_printf("Failed to add PSO to archive:\n%s\n",
                       errStr ? [errStr UTF8String] : "nil");
        }
        else {
          double duration = time_dt() - starttime;
          metal_printf("%2d | %-55s | %7.2fs\n",
                       desc.kernel_index,
                       device_kernel_as_string((DeviceKernel)desc.kernel_index),
                       duration);

          if (desc.pso_index == PSO_GENERIC) {
            this->load_duration = duration;
          }
        }
      }
      computePipelineStateDescriptor.binaryArchives = [NSArray arrayWithObjects:archive, nil];
      pipelineOptions = MTLPipelineOptionFailOnBinaryArchiveMiss;
    }
  }

  double starttime = time_dt();

  MTLNewComputePipelineStateWithReflectionCompletionHandler completionHandler = ^(
      id<MTLComputePipelineState> computePipelineState,
      MTLComputePipelineReflection *reflection,
      NSError *error) {
    bool recreate_archive = false;
    if (computePipelineState == nil && archive && !creating_new_archive) {

      assert(0);

      NSString *errStr = [error localizedDescription];
      metal_printf(
          "Failed to create compute pipeline state \"%s\" from archive - attempting recreation... "
          "(error: %s)\n",
          device_kernel_as_string((DeviceKernel)desc.kernel_index),
          errStr ? [errStr UTF8String] : "nil");
      computePipelineState = [device->mtlDevice
          newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                        options:MTLPipelineOptionNone
                                     reflection:nullptr
                                          error:&error];
      recreate_archive = true;
    }

    double duration = time_dt() - starttime;

    if (computePipelineState == nil) {
      NSString *errStr = [error localizedDescription];
      device->set_error(string_printf("Failed to create compute pipeline state \"%s\", error: \n",
                                      device_kernel_as_string((DeviceKernel)desc.kernel_index)) +
                        (errStr ? [errStr UTF8String] : "nil"));
      metal_printf("%2d | %-55s | %7.2fs | FAILED!\n",
                   desc.kernel_index,
                   device_kernel_as_string((DeviceKernel)desc.kernel_index),
                   duration);
      return;
    }

    pso[desc.pso_index].pipeline = computePipelineState;
    num_threads_per_block = round_down(computePipelineState.maxTotalThreadsPerThreadgroup,
                                       computePipelineState.threadExecutionWidth);
    num_threads_per_block = std::max(num_threads_per_block,
                                     (int)computePipelineState.threadExecutionWidth);

    if (!use_binary_archive) {
      metal_printf("%2d | %-55s | %7.2fs\n",
                   desc.kernel_index,
                   device_kernel_as_string((DeviceKernel)desc.kernel_index),
                   duration);

      if (desc.pso_index == PSO_GENERIC) {
        this->load_duration = duration;
      }
    }

    if (@available(macOS 11.0, *)) {
      if (creating_new_archive || recreate_archive) {
        if (![archive serializeToURL:[NSURL fileURLWithPath:@(metalbin_path.c_str())]
                               error:&error]) {
          metal_printf("Failed to save binary archive, error:\n%s\n",
                       [[error localizedDescription] UTF8String]);
        }
      }
    }

    [computePipelineStateDescriptor release];
    computePipelineStateDescriptor = nil;

    if (device->use_metalrt && desc.linked_functions) {
      for (int table = 0; table < METALRT_TABLE_NUM; table++) {
        if (@available(macOS 11.0, *)) {
          MTLIntersectionFunctionTableDescriptor *ift_desc =
              [[MTLIntersectionFunctionTableDescriptor alloc] init];
          ift_desc.functionCount = desc.intersector_functions[table].count;

          pso[desc.pso_index].intersection_func_table[table] = [pso[desc.pso_index].pipeline
              newIntersectionFunctionTableWithDescriptor:ift_desc];

          /* Finally write the function handles into this pipeline's table */
          for (int i = 0; i < 2; i++) {
            id<MTLFunctionHandle> handle = [pso[desc.pso_index].pipeline
                functionHandleWithFunction:desc.intersector_functions[table][i]];
            [pso[desc.pso_index].intersection_func_table[table] setFunction:handle atIndex:i];
          }
        }
      }
    }

    mark_loaded(desc.pso_index);
  };

  if (desc.pso_index == PSO_SPECIALISED) {
    /* Asynchronous load */
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      NSError *error;
      id<MTLComputePipelineState> pipeline = [device->mtlDevice
          newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                        options:pipelineOptions
                                     reflection:nullptr
                                          error:&error];
      completionHandler(pipeline, nullptr, error);
    });
  }
  else {
    /* Block on load to ensure we continue with a valid kernel function */
    id<MTLComputePipelineState> pipeline = [device->mtlDevice
        newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                      options:pipelineOptions
                                   reflection:nullptr
                                        error:&error];
    completionHandler(pipeline, nullptr, error);
  }

  return true;
}

const MetalKernelPipeline &MetalDeviceKernel::get_pso() const
{
  if (pso[PSO_SPECIALISED].loaded) {
    return pso[PSO_SPECIALISED];
  }

  assert(pso[PSO_GENERIC].loaded);
  return pso[PSO_GENERIC];
}

bool MetalDeviceKernels::load(MetalDevice *device, int kernel_type)
{
  bool any_error = false;

  MD5Hash md5;

  /* Build the function constant table */
  MTLFunctionConstantValues *constant_values = nullptr;
  if (kernel_type == PSO_SPECIALISED) {
    constant_values = [MTLFunctionConstantValues new];

#  define KERNEL_FILM(_type, name) \
    [constant_values setConstantValue:&data.film.name \
                                 type:get_MTLDataType_##_type() \
                              atIndex:KernelData_film_##name]; \
    md5.append((uint8_t *)&data.film.name, sizeof(data.film.name));

#  define KERNEL_BACKGROUND(_type, name) \
    [constant_values setConstantValue:&data.background.name \
                                 type:get_MTLDataType_##_type() \
                              atIndex:KernelData_background_##name]; \
    md5.append((uint8_t *)&data.background.name, sizeof(data.background.name));

#  define KERNEL_INTEGRATOR(_type, name) \
    [constant_values setConstantValue:&data.integrator.name \
                                 type:get_MTLDataType_##_type() \
                              atIndex:KernelData_integrator_##name]; \
    md5.append((uint8_t *)&data.integrator.name, sizeof(data.integrator.name));

#  define KERNEL_BVH(_type, name) \
    [constant_values setConstantValue:&data.bvh.name \
                                 type:get_MTLDataType_##_type() \
                              atIndex:KernelData_bvh_##name]; \
    md5.append((uint8_t *)&data.bvh.name, sizeof(data.bvh.name));

    /* METAL_WIP: populate constant_values based on KernelData */
    assert(0);
    /*
        const KernelData &data = device->launch_params.data;
    #    include "kernel/types/background.h"
    #    include "kernel/types/bvh.h"
    #    include "kernel/types/film.h"
    #    include "kernel/types/integrator.h"
    */
  }

  if (device->use_metalrt) {
    if (@available(macOS 11.0, *)) {
      /* create the id<MTLFunction> for each intersection function */
      const char *function_names[] = {
          "__anyhit__cycles_metalrt_visibility_test_tri",
          "__anyhit__cycles_metalrt_visibility_test_box",
          "__anyhit__cycles_metalrt_shadow_all_hit_tri",
          "__anyhit__cycles_metalrt_shadow_all_hit_box",
          "__anyhit__cycles_metalrt_local_hit_tri",
          "__anyhit__cycles_metalrt_local_hit_box",
          "__intersection__curve_ribbon",
          "__intersection__curve_ribbon_shadow",
          "__intersection__curve_all",
          "__intersection__curve_all_shadow",
          "__intersection__point",
          "__intersection__point_shadow",
      };
      assert(sizeof(function_names) / sizeof(function_names[0]) == METALRT_FUNC_NUM);

      MTLFunctionDescriptor *desc = [MTLIntersectionFunctionDescriptor functionDescriptor];
      if (kernel_type == PSO_SPECIALISED) {
        desc.constantValues = constant_values;
      }
      for (int i = 0; i < METALRT_FUNC_NUM; i++) {
        const char *function_name = function_names[i];
        desc.name = [@(function_name) copy];

        NSError *error = NULL;
        rt_intersection_funcs[kernel_type][i] = [device->mtlLibrary[kernel_type]
            newFunctionWithDescriptor:desc
                                error:&error];

        if (rt_intersection_funcs[kernel_type][i] == nil) {
          NSString *err = [error localizedDescription];
          string errors = [err UTF8String];

          device->set_error(string_printf(
              "Error getting intersection function \"%s\": %s", function_name, errors.c_str()));
          any_error = true;
          break;
        }

        rt_intersection_funcs[kernel_type][i].label = [@(function_name) copy];
      }
    }
  }
  md5.append(device->source_used_for_compile[kernel_type]);

  string hash = md5.get_hex();
  if (loaded_md5[kernel_type] == hash) {
    return true;
  }

  if (!any_error) {
    NSArray *table_functions[METALRT_TABLE_NUM] = {nil};
    NSArray *function_list = nil;

    if (device->use_metalrt) {
      id<MTLFunction> curve_intersect_default = nil;
      id<MTLFunction> curve_intersect_shadow = nil;
      id<MTLFunction> point_intersect_default = nil;
      id<MTLFunction> point_intersect_shadow = nil;
      if (device->kernel_features & KERNEL_FEATURE_HAIR) {
        /* Add curve intersection programs. */
        if (device->kernel_features & KERNEL_FEATURE_HAIR_THICK) {
          /* Slower programs for thick hair since that also slows down ribbons.
           * Ideally this should not be needed. */
          curve_intersect_default = rt_intersection_funcs[kernel_type][METALRT_FUNC_CURVE_ALL];
          curve_intersect_shadow =
              rt_intersection_funcs[kernel_type][METALRT_FUNC_CURVE_ALL_SHADOW];
        }
        else {
          curve_intersect_default = rt_intersection_funcs[kernel_type][METALRT_FUNC_CURVE_RIBBON];
          curve_intersect_shadow =
              rt_intersection_funcs[kernel_type][METALRT_FUNC_CURVE_RIBBON_SHADOW];
        }
      }
      if (device->kernel_features & KERNEL_FEATURE_POINTCLOUD) {
        point_intersect_default = rt_intersection_funcs[kernel_type][METALRT_FUNC_POINT];
        point_intersect_shadow = rt_intersection_funcs[kernel_type][METALRT_FUNC_POINT_SHADOW];
      }
      table_functions[METALRT_TABLE_DEFAULT] = [NSArray
          arrayWithObjects:rt_intersection_funcs[kernel_type][METALRT_FUNC_DEFAULT_TRI],
                           curve_intersect_default ?
                               curve_intersect_default :
                               rt_intersection_funcs[kernel_type][METALRT_FUNC_DEFAULT_BOX],
                           point_intersect_default ?
                               point_intersect_default :
                               rt_intersection_funcs[kernel_type][METALRT_FUNC_DEFAULT_BOX],
                           nil];
      table_functions[METALRT_TABLE_SHADOW] = [NSArray
          arrayWithObjects:rt_intersection_funcs[kernel_type][METALRT_FUNC_SHADOW_TRI],
                           curve_intersect_shadow ?
                               curve_intersect_shadow :
                               rt_intersection_funcs[kernel_type][METALRT_FUNC_SHADOW_BOX],
                           point_intersect_shadow ?
                               point_intersect_shadow :
                               rt_intersection_funcs[kernel_type][METALRT_FUNC_SHADOW_BOX],
                           nil];
      table_functions[METALRT_TABLE_LOCAL] = [NSArray
          arrayWithObjects:rt_intersection_funcs[kernel_type][METALRT_FUNC_LOCAL_TRI],
                           rt_intersection_funcs[kernel_type][METALRT_FUNC_LOCAL_BOX],
                           rt_intersection_funcs[kernel_type][METALRT_FUNC_LOCAL_BOX],
                           nil];

      NSMutableSet *unique_functions = [NSMutableSet
          setWithArray:table_functions[METALRT_TABLE_DEFAULT]];
      [unique_functions addObjectsFromArray:table_functions[METALRT_TABLE_SHADOW]];
      [unique_functions addObjectsFromArray:table_functions[METALRT_TABLE_LOCAL]];

      function_list = [[NSArray arrayWithArray:[unique_functions allObjects]]
          sortedArrayUsingComparator:^NSComparisonResult(id<MTLFunction> f1, id<MTLFunction> f2) {
            return [f1.label compare:f2.label];
          }];

      unique_functions = nil;
    }

    metal_printf("Starting %s \"cycles_metal_...\" pipeline builds\n",
                 kernel_type_as_string(kernel_type));

    tbb::task_arena local_arena(max_mtlcompiler_threads);
    local_arena.execute([&]() {
      tbb::parallel_for(int(0), int(DEVICE_KERNEL_NUM), [&](int i) {
        /* skip megakernel */
        if (i == DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL) {
          return;
        }

        /* Only specialize kernels where it can make an impact. */
        if (kernel_type == PSO_SPECIALISED) {
          if (i < DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST ||
              i > DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL) {
            return;
          }
        }

        MetalDeviceKernel &kernel = kernels_[i];

        const std::string function_name = std::string("cycles_metal_") +
                                          device_kernel_as_string((DeviceKernel)i);
        int threads_per_threadgroup = device->max_threads_per_threadgroup;
        if (i > DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL && i < DEVICE_KERNEL_INTEGRATOR_RESET) {
          /* Always use 512 for the sorting kernels */
          threads_per_threadgroup = 512;
        }

        NSArray *kernel_function_list = nil;

        if (i == DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST ||
            i == DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW ||
            i == DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE ||
            i == DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK ||
            i == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE) {
          kernel_function_list = function_list;
        }

        MetalKernelLoadDesc desc;
        desc.pso_index = kernel_type;
        desc.kernel_index = i;
        desc.linked_functions = kernel_function_list;
        desc.intersector_functions.defaults = table_functions[METALRT_TABLE_DEFAULT];
        desc.intersector_functions.shadow = table_functions[METALRT_TABLE_SHADOW];
        desc.intersector_functions.local = table_functions[METALRT_TABLE_LOCAL];
        desc.constant_values = constant_values;
        desc.threads_per_threadgroup = threads_per_threadgroup;
        desc.function_name = function_name.c_str();

        bool success = kernel.load(device, desc, md5);

        any_error |= !success;
      });
    });
  }

  bool loaded = !any_error;
  if (loaded) {
    loaded_md5[kernel_type] = hash;
  }
  return loaded;
}

const MetalDeviceKernel &MetalDeviceKernels::get(DeviceKernel kernel) const
{
  return kernels_[(int)kernel];
}

bool MetalDeviceKernels::available(DeviceKernel kernel) const
{
  return kernels_[(int)kernel].get_pso().function != nil;
}

CCL_NAMESPACE_END

#endif /* WITH_METAL*/
