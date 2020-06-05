/*
 * Copyright 2011-2013 Blender Foundation
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

#ifdef WITH_CUDA

#  include "device/device.h"
#  include "device/device_denoising.h"
#  include "device/device_split_kernel.h"

#  include "util/util_map.h"
#  include "util/util_task.h"

#  ifdef WITH_CUDA_DYNLOAD
#    include "cuew.h"
#  else
#    include "util/util_opengl.h"
#    include <cuda.h>
#    include <cudaGL.h>
#  endif

CCL_NAMESPACE_BEGIN

class CUDASplitKernel;

class CUDADevice : public Device {

  friend class CUDASplitKernelFunction;
  friend class CUDASplitKernel;
  friend class CUDAContextScope;

 public:
  DedicatedTaskPool task_pool;
  CUdevice cuDevice;
  CUcontext cuContext;
  CUmodule cuModule, cuFilterModule;
  size_t device_texture_headroom;
  size_t device_working_headroom;
  bool move_texture_to_host;
  size_t map_host_used;
  size_t map_host_limit;
  int can_map_host;
  int pitch_alignment;
  int cuDevId;
  int cuDevArchitecture;
  bool first_error;
  CUDASplitKernel *split_kernel;

  struct CUDAMem {
    CUDAMem() : texobject(0), array(0), use_mapped_host(false)
    {
    }

    CUtexObject texobject;
    CUarray array;

    /* If true, a mapped host memory in shared_pointer is being used. */
    bool use_mapped_host;
  };
  typedef map<device_memory *, CUDAMem> CUDAMemMap;
  CUDAMemMap cuda_mem_map;

  struct PixelMem {
    GLuint cuPBO;
    CUgraphicsResource cuPBOresource;
    GLuint cuTexId;
    int w, h;
  };
  map<device_ptr, PixelMem> pixel_mem_map;

  /* Bindless Textures */
  device_vector<TextureInfo> texture_info;
  bool need_texture_info;

  /* Kernels */
  struct {
    bool loaded;

    CUfunction adaptive_stopping;
    CUfunction adaptive_filter_x;
    CUfunction adaptive_filter_y;
    CUfunction adaptive_scale_samples;
    int adaptive_num_threads_per_block;
  } functions;

  static bool have_precompiled_kernels();

  virtual bool show_samples() const override;

  virtual BVHLayoutMask get_bvh_layout_mask() const override;

  void set_error(const string &error) override;

  CUDADevice(DeviceInfo &info, Stats &stats, Profiler &profiler, bool background_);

  virtual ~CUDADevice();

  bool support_device(const DeviceRequestedFeatures & /*requested_features*/);

  bool check_peer_access(Device *peer_device) override;

  bool use_adaptive_compilation();

  bool use_split_kernel();

  virtual string compile_kernel_get_common_cflags(
      const DeviceRequestedFeatures &requested_features, bool filter = false, bool split = false);

  string compile_kernel(const DeviceRequestedFeatures &requested_features,
                        const char *name,
                        const char *base = "cuda",
                        bool force_ptx = false);

  virtual bool load_kernels(const DeviceRequestedFeatures &requested_features) override;

  void load_functions();

  void reserve_local_memory(const DeviceRequestedFeatures &requested_features);

  void init_host_memory();

  void load_texture_info();

  void move_textures_to_host(size_t size, bool for_texture);

  CUDAMem *generic_alloc(device_memory &mem, size_t pitch_padding = 0);

  void generic_copy_to(device_memory &mem);

  void generic_free(device_memory &mem);

  void mem_alloc(device_memory &mem) override;

  void mem_copy_to(device_memory &mem) override;

  void mem_copy_from(device_memory &mem, int y, int w, int h, int elem) override;

  void mem_zero(device_memory &mem) override;

  void mem_free(device_memory &mem) override;

  device_ptr mem_alloc_sub_ptr(device_memory &mem, int offset, int /*size*/) override;

  virtual void const_copy_to(const char *name, void *host, size_t size) override;

  void global_alloc(device_memory &mem);

  void global_free(device_memory &mem);

  void tex_alloc(device_texture &mem);

  void tex_free(device_texture &mem);

  bool denoising_non_local_means(device_ptr image_ptr,
                                 device_ptr guide_ptr,
                                 device_ptr variance_ptr,
                                 device_ptr out_ptr,
                                 DenoisingTask *task);

  bool denoising_construct_transform(DenoisingTask *task);

  bool denoising_accumulate(device_ptr color_ptr,
                            device_ptr color_variance_ptr,
                            device_ptr scale_ptr,
                            int frame,
                            DenoisingTask *task);

  bool denoising_solve(device_ptr output_ptr, DenoisingTask *task);

  bool denoising_combine_halves(device_ptr a_ptr,
                                device_ptr b_ptr,
                                device_ptr mean_ptr,
                                device_ptr variance_ptr,
                                int r,
                                int4 rect,
                                DenoisingTask *task);

  bool denoising_divide_shadow(device_ptr a_ptr,
                               device_ptr b_ptr,
                               device_ptr sample_variance_ptr,
                               device_ptr sv_variance_ptr,
                               device_ptr buffer_variance_ptr,
                               DenoisingTask *task);

  bool denoising_get_feature(int mean_offset,
                             int variance_offset,
                             device_ptr mean_ptr,
                             device_ptr variance_ptr,
                             float scale,
                             DenoisingTask *task);

  bool denoising_write_feature(int out_offset,
                               device_ptr from_ptr,
                               device_ptr buffer_ptr,
                               DenoisingTask *task);

  bool denoising_detect_outliers(device_ptr image_ptr,
                                 device_ptr variance_ptr,
                                 device_ptr depth_ptr,
                                 device_ptr output_ptr,
                                 DenoisingTask *task);

  void denoise(RenderTile &rtile, DenoisingTask &denoising);

  void adaptive_sampling_filter(uint filter_sample,
                                WorkTile *wtile,
                                CUdeviceptr d_wtile,
                                CUstream stream = 0);
  void adaptive_sampling_post(RenderTile &rtile,
                              WorkTile *wtile,
                              CUdeviceptr d_wtile,
                              CUstream stream = 0);

  void render(DeviceTask &task, RenderTile &rtile, device_vector<WorkTile> &work_tiles);

  void film_convert(DeviceTask &task,
                    device_ptr buffer,
                    device_ptr rgba_byte,
                    device_ptr rgba_half);

  void shader(DeviceTask &task);

  CUdeviceptr map_pixels(device_ptr mem);

  void unmap_pixels(device_ptr mem);

  void pixels_alloc(device_memory &mem);

  void pixels_copy_from(device_memory &mem, int y, int w, int h);

  void pixels_free(device_memory &mem);

  void draw_pixels(device_memory &mem,
                   int y,
                   int w,
                   int h,
                   int width,
                   int height,
                   int dx,
                   int dy,
                   int dw,
                   int dh,
                   bool transparent,
                   const DeviceDrawParams &draw_params) override;

  void thread_run(DeviceTask &task);

  virtual void task_add(DeviceTask &task) override;

  virtual void task_wait() override;

  virtual void task_cancel() override;
};

CCL_NAMESPACE_END

#endif
