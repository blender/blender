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

#include "device/multi/device.h"

#include <sstream>
#include <stdlib.h>

#include "bvh/multi.h"

#include "device/device.h"
#include "device/queue.h"

#include "scene/geometry.h"

#include "util/foreach.h"
#include "util/list.h"
#include "util/log.h"
#include "util/map.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

class MultiDevice : public Device {
 public:
  struct SubDevice {
    Stats stats;
    Device *device;
    map<device_ptr, device_ptr> ptr_map;
    int peer_island_index = -1;
  };

  list<SubDevice> devices;
  device_ptr unique_key;
  vector<vector<SubDevice *>> peer_islands;

  MultiDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler)
      : Device(info, stats, profiler), unique_key(1)
  {
    foreach (const DeviceInfo &subinfo, info.multi_devices) {
      /* Always add CPU devices at the back since GPU devices can change
       * host memory pointers, which CPU uses as device pointer. */
      SubDevice *sub;
      if (subinfo.type == DEVICE_CPU) {
        devices.emplace_back();
        sub = &devices.back();
      }
      else {
        devices.emplace_front();
        sub = &devices.front();
      }

      /* The pointer to 'sub->stats' will stay valid even after new devices
       * are added, since 'devices' is a linked list. */
      sub->device = Device::create(subinfo, sub->stats, profiler);
    }

    /* Build a list of peer islands for the available render devices */
    foreach (SubDevice &sub, devices) {
      /* First ensure that every device is in at least once peer island */
      if (sub.peer_island_index < 0) {
        peer_islands.emplace_back();
        sub.peer_island_index = (int)peer_islands.size() - 1;
        peer_islands[sub.peer_island_index].push_back(&sub);
      }

      if (!info.has_peer_memory) {
        continue;
      }

      /* Second check peer access between devices and fill up the islands accordingly */
      foreach (SubDevice &peer_sub, devices) {
        if (peer_sub.peer_island_index < 0 &&
            peer_sub.device->info.type == sub.device->info.type &&
            peer_sub.device->check_peer_access(sub.device)) {
          peer_sub.peer_island_index = sub.peer_island_index;
          peer_islands[sub.peer_island_index].push_back(&peer_sub);
        }
      }
    }
  }

  ~MultiDevice()
  {
    foreach (SubDevice &sub, devices)
      delete sub.device;
  }

  const string &error_message() override
  {
    error_msg.clear();

    foreach (SubDevice &sub, devices)
      error_msg += sub.device->error_message();

    return error_msg;
  }

  virtual bool show_samples() const override
  {
    if (devices.size() > 1) {
      return false;
    }
    return devices.front().device->show_samples();
  }

  virtual BVHLayoutMask get_bvh_layout_mask() const override
  {
    BVHLayoutMask bvh_layout_mask = BVH_LAYOUT_ALL;
    BVHLayoutMask bvh_layout_mask_all = BVH_LAYOUT_NONE;
    foreach (const SubDevice &sub_device, devices) {
      BVHLayoutMask device_bvh_layout_mask = sub_device.device->get_bvh_layout_mask();
      bvh_layout_mask &= device_bvh_layout_mask;
      bvh_layout_mask_all |= device_bvh_layout_mask;
    }

    /* With multiple OptiX devices, every device needs its own acceleration structure */
    if (bvh_layout_mask == BVH_LAYOUT_OPTIX) {
      return BVH_LAYOUT_MULTI_OPTIX;
    }

    /* When devices do not share a common BVH layout, fall back to creating one for each */
    const BVHLayoutMask BVH_LAYOUT_OPTIX_EMBREE = (BVH_LAYOUT_OPTIX | BVH_LAYOUT_EMBREE);
    if ((bvh_layout_mask_all & BVH_LAYOUT_OPTIX_EMBREE) == BVH_LAYOUT_OPTIX_EMBREE) {
      return BVH_LAYOUT_MULTI_OPTIX_EMBREE;
    }

    return bvh_layout_mask;
  }

  bool load_kernels(const uint kernel_features) override
  {
    foreach (SubDevice &sub, devices)
      if (!sub.device->load_kernels(kernel_features))
        return false;

    return true;
  }

  void build_bvh(BVH *bvh, Progress &progress, bool refit) override
  {
    /* Try to build and share a single acceleration structure, if possible */
    if (bvh->params.bvh_layout == BVH_LAYOUT_BVH2 || bvh->params.bvh_layout == BVH_LAYOUT_EMBREE) {
      devices.back().device->build_bvh(bvh, progress, refit);
      return;
    }

    assert(bvh->params.bvh_layout == BVH_LAYOUT_MULTI_OPTIX ||
           bvh->params.bvh_layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE);

    BVHMulti *const bvh_multi = static_cast<BVHMulti *>(bvh);
    bvh_multi->sub_bvhs.resize(devices.size());

    vector<BVHMulti *> geom_bvhs;
    geom_bvhs.reserve(bvh->geometry.size());
    foreach (Geometry *geom, bvh->geometry) {
      geom_bvhs.push_back(static_cast<BVHMulti *>(geom->bvh));
    }

    /* Broadcast acceleration structure build to all render devices */
    size_t i = 0;
    foreach (SubDevice &sub, devices) {
      /* Change geometry BVH pointers to the sub BVH */
      for (size_t k = 0; k < bvh->geometry.size(); ++k) {
        bvh->geometry[k]->bvh = geom_bvhs[k]->sub_bvhs[i];
      }

      if (!bvh_multi->sub_bvhs[i]) {
        BVHParams params = bvh->params;
        if (bvh->params.bvh_layout == BVH_LAYOUT_MULTI_OPTIX)
          params.bvh_layout = BVH_LAYOUT_OPTIX;
        else if (bvh->params.bvh_layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE)
          params.bvh_layout = sub.device->info.type == DEVICE_OPTIX ? BVH_LAYOUT_OPTIX :
                                                                      BVH_LAYOUT_EMBREE;

        /* Skip building a bottom level acceleration structure for non-instanced geometry on Embree
         * (since they are put into the top level directly, see bvh_embree.cpp) */
        if (!params.top_level && params.bvh_layout == BVH_LAYOUT_EMBREE &&
            !bvh->geometry[0]->is_instanced()) {
          i++;
          continue;
        }

        bvh_multi->sub_bvhs[i] = BVH::create(params, bvh->geometry, bvh->objects, sub.device);
      }

      sub.device->build_bvh(bvh_multi->sub_bvhs[i], progress, refit);
      i++;
    }

    /* Change geometry BVH pointers back to the multi BVH. */
    for (size_t k = 0; k < bvh->geometry.size(); ++k) {
      bvh->geometry[k]->bvh = geom_bvhs[k];
    }
  }

  virtual void *get_cpu_osl_memory() override
  {
    if (devices.size() > 1) {
      return NULL;
    }
    return devices.front().device->get_cpu_osl_memory();
  }

  bool is_resident(device_ptr key, Device *sub_device) override
  {
    foreach (SubDevice &sub, devices) {
      if (sub.device == sub_device) {
        return find_matching_mem_device(key, sub)->device == sub_device;
      }
    }
    return false;
  }

  SubDevice *find_matching_mem_device(device_ptr key, SubDevice &sub)
  {
    assert(key != 0 && (sub.peer_island_index >= 0 || sub.ptr_map.find(key) != sub.ptr_map.end()));

    /* Get the memory owner of this key (first try current device, then peer devices) */
    SubDevice *owner_sub = &sub;
    if (owner_sub->ptr_map.find(key) == owner_sub->ptr_map.end()) {
      foreach (SubDevice *island_sub, peer_islands[sub.peer_island_index]) {
        if (island_sub != owner_sub &&
            island_sub->ptr_map.find(key) != island_sub->ptr_map.end()) {
          owner_sub = island_sub;
        }
      }
    }
    return owner_sub;
  }

  SubDevice *find_suitable_mem_device(device_ptr key, const vector<SubDevice *> &island)
  {
    assert(!island.empty());

    /* Get the memory owner of this key or the device with the lowest memory usage when new */
    SubDevice *owner_sub = island.front();
    foreach (SubDevice *island_sub, island) {
      if (key ? (island_sub->ptr_map.find(key) != island_sub->ptr_map.end()) :
                (island_sub->device->stats.mem_used < owner_sub->device->stats.mem_used)) {
        owner_sub = island_sub;
      }
    }
    return owner_sub;
  }

  inline device_ptr find_matching_mem(device_ptr key, SubDevice &sub)
  {
    return find_matching_mem_device(key, sub)->ptr_map[key];
  }

  void mem_alloc(device_memory &mem) override
  {
    device_ptr key = unique_key++;

    assert(mem.type == MEM_READ_ONLY || mem.type == MEM_READ_WRITE || mem.type == MEM_DEVICE_ONLY);
    /* The remaining memory types can be distributed across devices */
    foreach (const vector<SubDevice *> &island, peer_islands) {
      SubDevice *owner_sub = find_suitable_mem_device(key, island);
      mem.device = owner_sub->device;
      mem.device_pointer = 0;
      mem.device_size = 0;

      owner_sub->device->mem_alloc(mem);
      owner_sub->ptr_map[key] = mem.device_pointer;
    }

    mem.device = this;
    mem.device_pointer = key;
    stats.mem_alloc(mem.device_size);
  }

  void mem_copy_to(device_memory &mem) override
  {
    device_ptr existing_key = mem.device_pointer;
    device_ptr key = (existing_key) ? existing_key : unique_key++;
    size_t existing_size = mem.device_size;

    /* The tile buffers are allocated on each device (see below), so copy to all of them */
    foreach (const vector<SubDevice *> &island, peer_islands) {
      SubDevice *owner_sub = find_suitable_mem_device(existing_key, island);
      mem.device = owner_sub->device;
      mem.device_pointer = (existing_key) ? owner_sub->ptr_map[existing_key] : 0;
      mem.device_size = existing_size;

      owner_sub->device->mem_copy_to(mem);
      owner_sub->ptr_map[key] = mem.device_pointer;

      if (mem.type == MEM_GLOBAL || mem.type == MEM_TEXTURE) {
        /* Need to create texture objects and update pointer in kernel globals on all devices */
        foreach (SubDevice *island_sub, island) {
          if (island_sub != owner_sub) {
            island_sub->device->mem_copy_to(mem);
          }
        }
      }
    }

    mem.device = this;
    mem.device_pointer = key;
    stats.mem_alloc(mem.device_size - existing_size);
  }

  void mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem) override
  {
    device_ptr key = mem.device_pointer;
    size_t i = 0, sub_h = h / devices.size();

    foreach (SubDevice &sub, devices) {
      size_t sy = y + i * sub_h;
      size_t sh = (i == (size_t)devices.size() - 1) ? h - sub_h * i : sub_h;

      SubDevice *owner_sub = find_matching_mem_device(key, sub);
      mem.device = owner_sub->device;
      mem.device_pointer = owner_sub->ptr_map[key];

      owner_sub->device->mem_copy_from(mem, sy, w, sh, elem);
      i++;
    }

    mem.device = this;
    mem.device_pointer = key;
  }

  void mem_zero(device_memory &mem) override
  {
    device_ptr existing_key = mem.device_pointer;
    device_ptr key = (existing_key) ? existing_key : unique_key++;
    size_t existing_size = mem.device_size;

    foreach (const vector<SubDevice *> &island, peer_islands) {
      SubDevice *owner_sub = find_suitable_mem_device(existing_key, island);
      mem.device = owner_sub->device;
      mem.device_pointer = (existing_key) ? owner_sub->ptr_map[existing_key] : 0;
      mem.device_size = existing_size;

      owner_sub->device->mem_zero(mem);
      owner_sub->ptr_map[key] = mem.device_pointer;
    }

    mem.device = this;
    mem.device_pointer = key;
    stats.mem_alloc(mem.device_size - existing_size);
  }

  void mem_free(device_memory &mem) override
  {
    device_ptr key = mem.device_pointer;
    size_t existing_size = mem.device_size;

    /* Free memory that was allocated for all devices (see above) on each device */
    foreach (const vector<SubDevice *> &island, peer_islands) {
      SubDevice *owner_sub = find_matching_mem_device(key, *island.front());
      mem.device = owner_sub->device;
      mem.device_pointer = owner_sub->ptr_map[key];
      mem.device_size = existing_size;

      owner_sub->device->mem_free(mem);
      owner_sub->ptr_map.erase(owner_sub->ptr_map.find(key));

      if (mem.type == MEM_TEXTURE) {
        /* Free texture objects on all devices */
        foreach (SubDevice *island_sub, island) {
          if (island_sub != owner_sub) {
            island_sub->device->mem_free(mem);
          }
        }
      }
    }

    mem.device = this;
    mem.device_pointer = 0;
    mem.device_size = 0;
    stats.mem_free(existing_size);
  }

  void const_copy_to(const char *name, void *host, size_t size) override
  {
    foreach (SubDevice &sub, devices)
      sub.device->const_copy_to(name, host, size);
  }

  int device_number(Device *sub_device) override
  {
    int i = 0;

    foreach (SubDevice &sub, devices) {
      if (sub.device == sub_device)
        return i;
      i++;
    }

    return -1;
  }

  virtual void foreach_device(const function<void(Device *)> &callback) override
  {
    foreach (SubDevice &sub, devices) {
      sub.device->foreach_device(callback);
    }
  }
};

Device *device_multi_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
  return new MultiDevice(info, stats, profiler);
}

CCL_NAMESPACE_END
