/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup draw
 *
 * Wrapper classes that make it easier to use GPU objects in C++.
 *
 * All Buffers need to be sent to GPU memory before being used. This is done by using the
 * `push_update()`.
 *
 * A Storage[Array]Buffer can hold much more data than a Uniform[Array]Buffer
 * which can only holds 16KB of data.
 *
 * All types are not copyable and Buffers are not Movable.
 *
 * `draw::UniformArrayBuffer<T, len>`
 *   Uniform buffer object containing an array of T with len elements.
 *   Data can be accessed using the [] operator.
 *
 * `draw::UniformBuffer<T>`
 *   A uniform buffer object class inheriting from T.
 *   Data can be accessed just like a normal T object.
 *
 * `draw::StorageArrayBuffer<T, len>`
 *   Storage buffer object containing an array of T with len elements.
 *   The item count can be changed after creation using `resize()`.
 *   However, this requires the invalidation of the whole buffer and
 *   discarding all data inside it.
 *   Data can be accessed using the [] operator.
 *
 * `draw::StorageVectorBuffer<T, len>`
 *   Same as `StorageArrayBuffer` but has a length counter and act like a `blender::Vector` you can
 *   clear and append to.
 *
 * `draw::StorageBuffer<T>`
 *   A storage buffer object class inheriting from T.
 *   Data can be accessed just like a normal T object.
 *
 * `draw::Texture`
 *   A simple wrapper to #gpu::Texture. A #draw::Texture can be created without
 * allocation. The `ensure_[1d|2d|3d|cube][_array]()` method is here to make sure the underlying
 * texture will meet the requirements and create (or recreate) the #gpu::Texture if
 * needed.
 *
 * `draw::TextureFromPool`
 *   A gpu::Texture from the viewport texture pool. This texture can be shared with other
 * engines and its content is undefined when acquiring it. A #draw::TextureFromPool is acquired for
 * rendering using `acquire()` and released once the rendering is done using `release()`. The same
 * texture can be acquired & released multiple time in one draw loop. The `sync()` method *MUST* be
 * called once during the cache populate (aka: Sync) phase.
 *
 * `draw::Framebuffer`
 *   Simple wrapper to #GPUFramebuffer that can be moved.
 */

#include "MEM_guardedalloc.h"

#include "BKE_global.hh"

#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "GPU_framebuffer.hh"
#include "GPU_storage_buffer.hh"
#include "GPU_texture.hh"
#include "GPU_texture_pool.hh"
#include "GPU_uniform_buffer.hh"

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name Implementation Details
 * \{ */

namespace detail {

template<
    /** Type of the values stored in this uniform buffer. */
    typename T,
    /** The number of values that can be stored in this uniform buffer. */
    int64_t len,
    /** True if the buffer only resides on GPU memory and cannot be accessed. */
    bool device_only>
class DataBuffer {
 protected:
  T *data_ = nullptr;
  int64_t len_ = len;

  BLI_STATIC_ASSERT(((sizeof(T) * len) % 16) == 0,
                    "Buffer size need to be aligned to size of float4.");

 public:
  /**
   * Get the value at the given index. This invokes undefined behavior when the
   * index is out of bounds.
   */
  const T &operator[](int64_t index) const
  {
    BLI_STATIC_ASSERT(!device_only, "");
    BLI_assert(index >= 0);
    BLI_assert(index < len_);
    return data_[index];
  }

  T &operator[](int64_t index)
  {
    BLI_STATIC_ASSERT(!device_only, "");
    BLI_assert(index >= 0);
    BLI_assert(index < len_);
    return data_[index];
  }

  /**
   * Get a pointer to the beginning of the array.
   */
  const T *data() const
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return data_;
  }
  T *data()
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return data_;
  }

  /**
   * Iterator
   */
  const T *begin() const
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return data_;
  }
  const T *end() const
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return data_ + len_;
  }

  T *begin()
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return data_;
  }
  T *end()
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return data_ + len_;
  }

  operator Span<T>() const
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return Span<T>(data_, len_);
  }
};

template<typename T, int64_t len, bool device_only>
class UniformCommon : public DataBuffer<T, len, false>, NonMovable, NonCopyable {
 protected:
  gpu::UniformBuf *ubo_;

#ifndef NDEBUG
  const char *name_ = typeid(T).name();
#else
  const char *name_ = "UniformBuffer";
#endif

 public:
  UniformCommon(const char *name = nullptr)
  {
    if (name) {
      name_ = name;
    }
    ubo_ = GPU_uniformbuf_create_ex(sizeof(T) * len, nullptr, name_);
  }

  ~UniformCommon()
  {
    GPU_uniformbuf_free(ubo_);
  }

  void push_update()
  {
    GPU_uniformbuf_update(ubo_, this->data_);
  }

  /* To be able to use it with DRW_shgroup_*_ref(). */
  operator gpu::UniformBuf *() const
  {
    return ubo_;
  }

  /* To be able to use it with DRW_shgroup_*_ref(). */
  gpu::UniformBuf **operator&()
  {
    return &ubo_;
  }
};

template<typename T, int64_t len, bool device_only>
class StorageCommon : public DataBuffer<T, len, false>, NonMovable, NonCopyable {
 protected:
  gpu::StorageBuf *ssbo_;

#ifndef NDEBUG
  const char *name_ = typeid(T).name();
#else
  const char *name_ = "StorageBuffer";
#endif

 public:
  StorageCommon(const char *name = nullptr)
  {
    if (name) {
      name_ = name;
    }
    this->len_ = len;
    constexpr GPUUsageType usage = device_only ? GPU_USAGE_DEVICE_ONLY : GPU_USAGE_DYNAMIC;
    ssbo_ = GPU_storagebuf_create_ex(sizeof(T) * this->len_, nullptr, usage, this->name_);
  }

  ~StorageCommon()
  {
    GPU_storagebuf_free(ssbo_);
  }

  void push_update()
  {
    BLI_assert(device_only == false);
    GPU_storagebuf_update(ssbo_, this->data_);
  }

  void clear_to_zero()
  {
    GPU_storagebuf_clear_to_zero(ssbo_);
  }

  void async_flush_to_host()
  {
    GPU_storagebuf_sync_to_host(ssbo_);
  }

  void read()
  {
    GPU_storagebuf_read(ssbo_, this->data_);
  }

  operator gpu::StorageBuf *() const
  {
    return ssbo_;
  }
  /* To be able to use it with DRW_shgroup_*_ref(). */
  gpu::StorageBuf **operator&()
  {
    return &ssbo_;
  }
};

}  // namespace detail

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Buffers
 * \{ */

template<
    /** Type of the values stored in this uniform buffer. */
    typename T,
    /** The number of values that can be stored in this uniform buffer. */
    int64_t len
    /** True if the buffer only resides on GPU memory and cannot be accessed. */
    /* TODO(@fclem): Currently unsupported. */
    /* bool device_only = false */>
class UniformArrayBuffer : public detail::UniformCommon<T, len, false> {
 public:
  UniformArrayBuffer(const char *name = nullptr) : detail::UniformCommon<T, len, false>(name)
  {
    /* TODO(@fclem): We should map memory instead. */
    this->data_ = (T *)MEM_mallocN_aligned(len * sizeof(T), 16, this->name_);
  }
  ~UniformArrayBuffer()
  {
    MEM_freeN(static_cast<void *>(this->data_));
  }
};

template<
    /** Type of the values stored in this uniform buffer. */
    typename T
    /** True if the buffer only resides on GPU memory and cannot be accessed. */
    /* TODO(@fclem): Currently unsupported. */
    /* bool device_only = false */>
class UniformBuffer : public T, public detail::UniformCommon<T, 1, false> {
 public:
  UniformBuffer(const char *name = nullptr) : T{}, detail::UniformCommon<T, 1, false>(name)
  {
    /* TODO(@fclem): How could we map this? */
    this->data_ = static_cast<T *>(this);
  }

  UniformBuffer<T> &operator=(const T &other)
  {
    *static_cast<T *>(this) = other;
    return *this;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Storage Buffer
 * \{ */

template<
    /** Type of the values stored in this uniform buffer. */
    typename T,
    /** The number of values that can be stored in this storage buffer at creation. */
    int64_t len = (512u + (sizeof(T) - 1)) / sizeof(T),
    /** True if created on device and no memory host memory is allocated. */
    bool device_only = false>
class StorageArrayBuffer : public detail::StorageCommon<T, len, device_only> {
 public:
  StorageArrayBuffer(const char *name = nullptr) : detail::StorageCommon<T, len, device_only>(name)
  {
    /* TODO(@fclem): We should map memory instead. */
    this->data_ = (T *)MEM_mallocN_aligned(len * sizeof(T), 16, this->name_);
  }
  ~StorageArrayBuffer()
  {
    /* NOTE: T is not always trivial (e.g. can be #blender::eevee::VelocityIndex), so cannot use
     * `MEM_freeN` directly on it, without casting it to `void *`. */
    MEM_freeN(static_cast<void *>(this->data_));
  }

  /* Resize to \a new_size elements. */
  void resize(int64_t new_size)
  {
    BLI_assert(new_size > 0);
    if (new_size != this->len_) {
      /* Manual realloc since MEM_reallocN_aligned does not exists. */
      T *new_data_ = (T *)MEM_mallocN_aligned(new_size * sizeof(T), 16, this->name_);
      memcpy(reinterpret_cast<void *>(new_data_),
             this->data_,
             min_uu(this->len_, new_size) * sizeof(T));
      MEM_freeN(static_cast<void *>(this->data_));
      this->data_ = new_data_;
      GPU_storagebuf_free(this->ssbo_);

      this->len_ = new_size;
      constexpr GPUUsageType usage = device_only ? GPU_USAGE_DEVICE_ONLY : GPU_USAGE_DYNAMIC;
      this->ssbo_ = GPU_storagebuf_create_ex(sizeof(T) * this->len_, nullptr, usage, this->name_);
    }
  }

  /* Resize on access. */
  T &get_or_resize(int64_t index)
  {
    BLI_assert(index >= 0);
    if (index >= this->len_) {
      size_t size = power_of_2_max_u(index + 1);
      this->resize(size);
    }
    return this->data_[index];
  }

  /*
   * Ensure the allocated size is not much larger than the currently required size,
   * using the same heuristic as `get_or_resize`.
   */
  void trim_to_next_power_of_2(int64_t required_size)
  {
    /* Don't go below the size used at creation. */
    required_size = std::max(required_size, len);
    size_t target_size = power_of_2_max_u(required_size);
    if (this->len_ > target_size) {
      this->resize(target_size);
    }
  }

  int64_t size() const
  {
    return this->len_;
  }

  MutableSpan<T> as_span() const
  {
    return {this->data_, this->len_};
  }

  static void swap(StorageArrayBuffer &a, StorageArrayBuffer &b)
  {
    std::swap(a.data_, b.data_);
    std::swap(a.ssbo_, b.ssbo_);
    std::swap(a.len_, b.len_);
    std::swap(a.name_, b.name_);
  }
};

template<
    /** Type of the values stored in this uniform buffer. */
    typename T,
    /** The number of values that can be stored in this storage buffer at creation. */
    int64_t len = (512u + (sizeof(T) - 1)) / sizeof(T)>
class StorageVectorBuffer : public StorageArrayBuffer<T, len, false> {
 private:
  /* Number of items, not the allocated length. */
  int64_t item_len_ = 0;

 public:
  StorageVectorBuffer(const char *name = nullptr) : StorageArrayBuffer<T, len, false>(name) {};
  ~StorageVectorBuffer() = default;

  /**
   * Set item count to zero but does not free memory or resize the buffer.
   */
  void clear()
  {
    item_len_ = 0;
  }

  /**
   * Set item count to zero
   * and trim the buffer if current size is much larger than the current item count.
   */
  void clear_and_trim()
  {
    this->trim_to_next_power_of_2(item_len_);
    clear();
  }

  /**
   * Insert a new element at the end of the vector.
   * This might cause a reallocation with the capacity is exceeded.
   *
   * This is similar to std::vector::push_back.
   */
  void append(const T &value)
  {
    this->append_as(value);
  }
  void append(T &&value)
  {
    this->append_as(std::move(value));
  }
  template<typename... ForwardT> void append_as(ForwardT &&...value)
  {
    if (item_len_ >= this->len_) {
      size_t size = power_of_2_max_u(item_len_ + 1);
      this->resize(size);
    }
    T *ptr = &this->data_[item_len_++];
    new (ptr) T(std::forward<ForwardT>(value)...);
  }

  void extend(const Span<T> values)
  {
    /* TODO(fclem): Optimize to a single memcpy. */
    for (auto v : values) {
      this->append(v);
    }
  }

  int64_t size() const
  {
    return item_len_;
  }

  bool is_empty() const
  {
    return this->size() == 0;
  }

  /* Avoid confusion with the other clear. */
  void clear_to_zero() = delete;

  static void swap(StorageVectorBuffer &a, StorageVectorBuffer &b)
  {
    StorageArrayBuffer<T, len, false>::swap(a, b);
    std::swap(a.item_len_, b.item_len_);
  }
};

template<
    /** Type of the values stored in this uniform buffer. */
    typename T,
    /** True if created on device and no memory host memory is allocated. */
    bool device_only = false>
class StorageBuffer : public T, public detail::StorageCommon<T, 1, device_only> {
 public:
  StorageBuffer(const char *name = nullptr) : T{}, detail::StorageCommon<T, 1, device_only>(name)
  {
    /* TODO(@fclem): How could we map this? */
    this->data_ = static_cast<T *>(this);
  }

  StorageBuffer<T> &operator=(const T &other)
  {
    *static_cast<T *>(this) = other;
    return *this;
  }

  static void swap(StorageBuffer<T> &a, StorageBuffer<T> &b)
  {
    /* Swap content, but not `data_` pointers since they point to `this`. */
    SWAP(T, static_cast<T>(a), static_cast<T>(b));
    std::swap(a.ssbo_, b.ssbo_);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture
 * \{ */

class Texture : NonCopyable {
 protected:
  gpu::Texture *tx_ = nullptr;
  gpu::Texture *stencil_view_ = nullptr;
  Vector<gpu::Texture *, 0> mip_views_;
  Vector<gpu::Texture *, 0> layer_views_;
  gpu::Texture *layer_range_view_ = nullptr;
  const char *name_;

 public:
  Texture(const char *name = "gpu::Texture") : name_(name) {}

  Texture(const char *name,
          blender::gpu::TextureFormat format,
          eGPUTextureUsage usage,
          int extent,
          const float *data = nullptr,
          bool cubemap = false,
          int mip_len = 1)
      : name_(name)
  {
    tx_ = create(extent, 0, 0, mip_len, format, usage, data, false, cubemap);
  }

  Texture(const char *name,
          blender::gpu::TextureFormat format,
          eGPUTextureUsage usage,
          int extent,
          int layers,
          const float *data = nullptr,
          bool cubemap = false,
          int mip_len = 1)
      : name_(name)
  {
    tx_ = create(extent, layers, 0, mip_len, format, usage, data, true, cubemap);
  }

  Texture(const char *name,
          blender::gpu::TextureFormat format,
          eGPUTextureUsage usage,
          int2 extent,
          const float *data = nullptr,
          int mip_len = 1)
      : name_(name)
  {
    tx_ = create(UNPACK2(extent), 0, mip_len, format, usage, data, false, false);
  }

  Texture(const char *name,
          blender::gpu::TextureFormat format,
          eGPUTextureUsage usage,
          int2 extent,
          int layers,
          const float *data = nullptr,
          int mip_len = 1)
      : name_(name)
  {
    tx_ = create(UNPACK2(extent), layers, mip_len, format, usage, data, true, false);
  }

  Texture(const char *name,
          blender::gpu::TextureFormat format,
          eGPUTextureUsage usage,
          int3 extent,
          const float *data = nullptr,
          int mip_len = 1)
      : name_(name)
  {
    tx_ = create(UNPACK3(extent), mip_len, format, usage, data, false, false);
  }

  Texture(Texture &&other) = default;
  ~Texture()
  {
    free();
  }

  gpu::Texture *gpu_texture()
  {
    return tx_;
  }

  /* To be able to use it with DRW_shgroup_uniform_texture(). */
  operator gpu::Texture *() const
  {
    BLI_assert(tx_ != nullptr);
    return tx_;
  }

  /* To be able to use it with DRW_shgroup_uniform_texture_ref(). */
  gpu::Texture **operator&()
  {
    return &tx_;
  }

  /** WORKAROUND: used when needing a ref to the Texture and not the gpu::Texture. */
  Texture *ptr()
  {
    return this;
  }

  Texture &operator=(Texture &&a)
  {
    if (this != std::addressof(a)) {
      this->free();

      this->tx_ = a.tx_;
      this->name_ = a.name_;
      this->stencil_view_ = a.stencil_view_;
      this->layer_range_view_ = a.layer_range_view_;
      this->mip_views_ = std::move(a.mip_views_);
      this->layer_views_ = std::move(a.layer_views_);

      a.tx_ = nullptr;
      a.name_ = nullptr;
      a.stencil_view_ = nullptr;
      a.layer_range_view_ = nullptr;
      a.mip_views_.clear();
      a.layer_views_.clear();
    }
    return *this;
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_1d(blender::gpu::TextureFormat format,
                 int extent,
                 eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                 const float *data = nullptr,
                 int mip_len = 1)
  {
    return ensure_impl(extent, 0, 0, mip_len, format, usage, data, false, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_1d_array(blender::gpu::TextureFormat format,
                       int extent,
                       int layers,
                       eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                       const float *data = nullptr,
                       int mip_len = 1)
  {
    BLI_assert(layers > 0);
    return ensure_impl(extent, layers, 0, mip_len, format, usage, data, true, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_2d(blender::gpu::TextureFormat format,
                 int2 extent,
                 eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                 const float *data = nullptr,
                 int mip_len = 1)
  {
    return ensure_impl(UNPACK2(extent), 0, mip_len, format, usage, data, false, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_2d_array(blender::gpu::TextureFormat format,
                       int2 extent,
                       int layers,
                       eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                       const float *data = nullptr,
                       int mip_len = 1)
  {
    BLI_assert(layers > 0);
    return ensure_impl(UNPACK2(extent), layers, mip_len, format, usage, data, true, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_3d(blender::gpu::TextureFormat format,
                 int3 extent,
                 eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                 const float *data = nullptr,
                 int mip_len = 1)
  {
    return ensure_impl(UNPACK3(extent), mip_len, format, usage, data, false, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_cube(blender::gpu::TextureFormat format,
                   int extent,
                   eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                   float *data = nullptr,
                   int mip_len = 1)
  {
    return ensure_impl(extent, extent, 0, mip_len, format, usage, data, false, true);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_cube_array(blender::gpu::TextureFormat format,
                         int extent,
                         int layers,
                         eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                         const float *data = nullptr,
                         int mip_len = 1)
  {
    return ensure_impl(extent, extent, layers, mip_len, format, usage, data, true, true);
  }

  /**
   * Ensure the availability of mipmap views.
   * MIP view covers all layers of array textures.
   */
  bool ensure_mip_views(bool cube_as_array = false)
  {
    int mip_len = GPU_texture_mip_count(tx_);
    if (mip_views_.size() != mip_len) {
      for (gpu::Texture *&view : mip_views_) {
        GPU_TEXTURE_FREE_SAFE(view);
      }
      blender::gpu::TextureFormat format = GPU_texture_format(tx_);
      for (auto i : IndexRange(mip_len)) {
        mip_views_.append(
            GPU_texture_create_view(name_, tx_, format, i, 1, 0, 9999, cube_as_array, false));
      }
      return true;
    }
    return false;
  }

  gpu::Texture *mip_view(int miplvl)
  {
    BLI_assert_msg(miplvl < mip_views_.size(),
                   "Incorrect mip level requested. "
                   "Might be missing call to ensure_mip_views().");
    return mip_views_[miplvl];
  }

  int mip_count() const
  {
    return GPU_texture_mip_count(tx_);
  }

  /**
   * Ensure the availability of layer views.
   * Layer views covers all layers of array textures.
   * Returns true if the views were (re)created.
   */
  bool ensure_layer_views(bool cube_as_array = false)
  {
    int layer_len = GPU_texture_layer_count(tx_);
    if (layer_views_.size() != layer_len) {
      for (gpu::Texture *&view : layer_views_) {
        GPU_TEXTURE_FREE_SAFE(view);
      }
      blender::gpu::TextureFormat format = GPU_texture_format(tx_);
      for (auto i : IndexRange(layer_len)) {
        layer_views_.append(
            GPU_texture_create_view(name_, tx_, format, 0, 9999, i, 1, cube_as_array, false));
      }
      return true;
    }
    return false;
  }

  gpu::Texture *layer_view(int layer)
  {
    return layer_views_[layer];
  }

  gpu::Texture *stencil_view(bool cube_as_array = false)
  {
    if (stencil_view_ == nullptr) {
      blender::gpu::TextureFormat format = GPU_texture_format(tx_);
      stencil_view_ = GPU_texture_create_view(
          name_, tx_, format, 0, 9999, 0, 9999, cube_as_array, true);
    }
    return stencil_view_;
  }

  /**
   * Layer range view cover only the given range.
   * This can only be called to create one range.
   * View is recreated if:
   * - The source texture is recreated.
   * - The layer_len is different from the last call to this function.
   * IMPORTANT: It is not recreated if the layer_start is different from the last call.
   * IMPORTANT: If this view is recreated any reference to it should be updated.
   */
  gpu::Texture *layer_range_view(int layer_start, int layer_len, bool cube_as_array = false)
  {
    BLI_assert(this->is_valid());
    /* Make sure the range is valid as the GPU_texture_layer_count only returns the effective
     * (clipped) range and not the requested range. */
    BLI_assert_msg((layer_start + layer_len) <= GPU_texture_layer_count(tx_),
                   "Layer range needs to be valid");

    int view_layer_len = (layer_range_view_) ? GPU_texture_layer_count(layer_range_view_) : -1;
    if (layer_len != view_layer_len) {
      GPU_TEXTURE_FREE_SAFE(layer_range_view_);
      blender::gpu::TextureFormat format = GPU_texture_format(tx_);
      layer_range_view_ = GPU_texture_create_view(
          name_, tx_, format, 0, 9999, layer_start, layer_len, cube_as_array, false);
    }
    return layer_range_view_;
  }

  /**
   * Returns true if the texture has been allocated or acquired from the pool.
   */
  bool is_valid() const
  {
    return tx_ != nullptr;
  }

  int width() const
  {
    return GPU_texture_width(tx_);
  }

  int height() const
  {
    return GPU_texture_height(tx_);
  }

  int depth() const
  {
    return GPU_texture_depth(tx_);
  }

  int pixel_count() const
  {
    return GPU_texture_width(tx_) * GPU_texture_height(tx_);
  }

  bool is_depth() const
  {
    return GPU_texture_has_depth_format(tx_);
  }

  bool is_stencil() const
  {
    return GPU_texture_has_stencil_format(tx_);
  }

  bool is_integer() const
  {
    return GPU_texture_has_integer_format(tx_);
  }

  bool is_cube() const
  {
    return GPU_texture_is_cube(tx_);
  }

  bool is_array() const
  {
    return GPU_texture_is_array(tx_);
  }

  int3 size(int miplvl = 0) const
  {
    int3 size(1);
    GPU_texture_get_mipmap_size(tx_, miplvl, size);
    return size;
  }

  /**
   * Clear the entirety of the texture using one pixel worth of data.
   */
  void clear(float4 values)
  {
    GPU_texture_clear(tx_, GPU_DATA_FLOAT, &values[0]);
  }

  /**
   * Clear the entirety of the texture using one pixel worth of data.
   */
  void clear(uint4 values)
  {
    GPU_texture_clear(tx_, GPU_DATA_UINT, &values[0]);
  }

  /**
   * Clear the entirety of the texture using one pixel worth of data.
   */
  void clear(int4 values)
  {
    GPU_texture_clear(tx_, GPU_DATA_INT, &values[0]);
  }

  /**
   * Clear the texture to NaN for floats, or a to debug value for integers.
   * (For debugging uninitialized data issues)
   */
  void debug_clear()
  {
    if (GPU_texture_dimensions(this->tx_) == 1) {
      /* Clearing of 1D texture is currently unsupported. */
      return;
    }

    if (GPU_texture_has_float_format(this->tx_) || GPU_texture_has_normalized_format(this->tx_)) {
      this->clear(float4(NAN_FLT));
    }
    else if (GPU_texture_has_integer_format(this->tx_)) {
      if (GPU_texture_has_signed_format(this->tx_)) {
        this->clear(int4(0xF0F0F0F0));
      }
      else {
        this->clear(uint4(0xF0F0F0F0));
      }
    }
  }

  /**
   * Returns a buffer containing the texture data for the specified miplvl.
   * The memory block needs to be manually freed by MEM_freeN().
   */
  template<typename T> T *read(eGPUDataFormat format, int miplvl = 0)
  {
    return reinterpret_cast<T *>(GPU_texture_read(tx_, format, miplvl));
  }

  void filter_mode(bool do_filter)
  {
    GPU_texture_filter_mode(tx_, do_filter);
  }

  /**
   * Free the internal texture but not the #draw::Texture itself.
   */
  void free()
  {
    GPU_TEXTURE_FREE_SAFE(tx_);
    free_texture_views();
  }

  /**
   * Swap the content of the two textures.
   */
  static void swap(Texture &a, Texture &b)
  {
    std::swap(a.tx_, b.tx_);
    std::swap(a.name_, b.name_);
    std::swap(a.stencil_view_, b.stencil_view_);
    std::swap(a.layer_range_view_, b.layer_range_view_);
    std::swap(a.mip_views_, b.mip_views_);
    std::swap(a.layer_views_, b.layer_views_);
  }

 protected:
  void free_texture_views()
  {
    for (gpu::Texture *&view : mip_views_) {
      GPU_TEXTURE_FREE_SAFE(view);
    }
    for (gpu::Texture *&view : layer_views_) {
      GPU_TEXTURE_FREE_SAFE(view);
    }
    GPU_TEXTURE_FREE_SAFE(stencil_view_);
    GPU_TEXTURE_FREE_SAFE(layer_range_view_);
    mip_views_.clear();
    layer_views_.clear();
  }

 private:
  bool ensure_impl(int w,
                   int h = 0,
                   int d = 0,
                   int mip_len = 1,
                   blender::gpu::TextureFormat format = blender::gpu::TextureFormat::UNORM_8_8_8_8,
                   eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                   const float *data = nullptr,
                   bool layered = false,
                   bool cubemap = false)

  {
    /* TODO(@fclem): In the future, we need to check if mip_count did not change.
     * For now it's ok as we always define all MIP level. */
    if (tx_) {
      int3 size(0);
      GPU_texture_get_mipmap_size(tx_, 0, size);
      if (size != int3(w, h, d) || GPU_texture_format(tx_) != format ||
          GPU_texture_is_cube(tx_) != cubemap || GPU_texture_is_array(tx_) != layered)
      {
        free();
      }
    }
    if (tx_ == nullptr) {
      tx_ = create(w, h, d, mip_len, format, usage, data, layered, cubemap);
      if (is_valid() && data == nullptr && (G.debug & G_DEBUG_GPU)) {
        debug_clear();
      }
      return true;
    }
    return false;
  }

  gpu::Texture *create(int w,
                       int h,
                       int d,
                       int mip_len,
                       blender::gpu::TextureFormat format,
                       eGPUTextureUsage usage,
                       const float *data,
                       bool layered,
                       bool cubemap)
  {
    if (h == 0) {
      return GPU_texture_create_1d(name_, w, mip_len, format, usage, data);
    }
    if (cubemap) {
      if (layered) {
        return GPU_texture_create_cube_array(name_, w, d, mip_len, format, usage, data);
      }
      return GPU_texture_create_cube(name_, w, mip_len, format, usage, data);
    }
    if (d == 0) {
      if (layered) {
        return GPU_texture_create_1d_array(name_, w, h, mip_len, format, usage, data);
      }
      return GPU_texture_create_2d(name_, w, h, mip_len, format, usage, data);
    }
    if (layered) {
      return GPU_texture_create_2d_array(name_, w, h, d, mip_len, format, usage, data);
    }
    return GPU_texture_create_3d(name_, w, h, d, mip_len, format, usage, data);
  }
};

class TextureFromPool : public Texture, NonMovable {
 public:
  TextureFromPool(const char *name = "gpu::Texture") : Texture(name) {};

  /* Always use `release()` after rendering. */
  void acquire(int2 extent,
               blender::gpu::TextureFormat format,
               eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL)
  {
    BLI_assert(this->tx_ == nullptr);

    this->tx_ = gpu::TexturePool::get().acquire_texture(UNPACK2(extent), format, usage);

    if (G.debug & G_DEBUG_GPU) {
      debug_clear();
    }
  }

  void release()
  {
    /* Allows multiple release. */
    if (this->tx_ == nullptr) {
      return;
    }
    gpu::TexturePool::get().release_texture(this->tx_);
    this->tx_ = nullptr;
  }

  /**
   * Swap the content of the two textures.
   * Also change ownership accordingly if needed.
   */
  static void swap(TextureFromPool &a, Texture &b)
  {
    Texture::swap(a, b);
    gpu::TexturePool::get().give_texture_ownership(a);
    gpu::TexturePool::get().take_texture_ownership(b);
  }
  static void swap(Texture &a, TextureFromPool &b)
  {
    swap(b, a);
  }
  static void swap(TextureFromPool &a, TextureFromPool &b)
  {
    Texture::swap(a, b);
  }

  /** WORKAROUND: used when needing a ref to the Texture and not the gpu::Texture. */
  TextureFromPool *ptr()
  {
    return this;
  }

  /** Remove methods that are forbidden with this type of textures. */
  bool ensure_1d(int, int, blender::gpu::TextureFormat, eGPUTextureUsage, const float *) = delete;
  bool ensure_1d_array(
      int, int, int, blender::gpu::TextureFormat, eGPUTextureUsage, const float *) = delete;
  bool ensure_2d(int, int, int, blender::gpu::TextureFormat, eGPUTextureUsage, float *) = delete;
  bool ensure_2d_array(
      int, int, int, int, blender::gpu::TextureFormat, eGPUTextureUsage, const float *) = delete;
  bool ensure_3d(
      int, int, int, int, blender::gpu::TextureFormat, eGPUTextureUsage, const float *) = delete;
  bool ensure_cube(int, int, blender::gpu::TextureFormat, eGPUTextureUsage, const float *) =
      delete;
  bool ensure_cube_array(
      int, int, int, blender::gpu::TextureFormat, eGPUTextureUsage, const float *) = delete;
  void filter_mode(bool) = delete;
  void free() = delete;
  gpu::Texture *mip_view(int) = delete;
  gpu::Texture *layer_view(int) = delete;
  gpu::Texture *stencil_view() = delete;
};

class TextureRef : public Texture {
 public:
  TextureRef() = default;

  ~TextureRef()
  {
    this->tx_ = nullptr;
  }

  void wrap(gpu::Texture *tex)
  {
    if (assign_if_different(this->tx_, tex)) {
      free_texture_views();
    }
  }

  /** Remove methods that are forbidden with this type of textures. */
  bool ensure_1d(int, int, blender::gpu::TextureFormat, const float *) = delete;
  bool ensure_1d_array(int, int, int, blender::gpu::TextureFormat, const float *) = delete;
  bool ensure_2d(int, int, int, blender::gpu::TextureFormat, const float *) = delete;
  bool ensure_2d_array(int, int, int, int, blender::gpu::TextureFormat, const float *) = delete;
  bool ensure_3d(int, int, int, int, blender::gpu::TextureFormat, const float *) = delete;
  bool ensure_cube(int, int, blender::gpu::TextureFormat, const float *) = delete;
  bool ensure_cube_array(int, int, int, blender::gpu::TextureFormat, const float *) = delete;
  void filter_mode(bool) = delete;
  void free() = delete;
  gpu::Texture *mip_view(int) = delete;
  gpu::Texture *layer_view(int) = delete;
  gpu::Texture *stencil_view() = delete;
};

/**
 * Dummy type to bind texture as image.
 * It is just a gpu::Texture in disguise.
 */
class Image {};

static inline Image *as_image(gpu::Texture *tex)
{
  return reinterpret_cast<Image *>(tex);
}

static inline Image **as_image(gpu::Texture **tex)
{
  return reinterpret_cast<Image **>(tex);
}

static inline gpu::Texture *as_texture(Image *img)
{
  return reinterpret_cast<gpu::Texture *>(img);
}

static inline gpu::Texture **as_texture(Image **img)
{
  return reinterpret_cast<gpu::Texture **>(img);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Framebuffer
 * \{ */

class Framebuffer : NonCopyable {
 private:
  gpu::FrameBuffer *fb_ = nullptr;
  const char *name_;

 public:
  Framebuffer() : name_("") {};
  Framebuffer(const char *name) : name_(name) {};

  ~Framebuffer()
  {
    GPU_FRAMEBUFFER_FREE_SAFE(fb_);
  }

  void ensure(GPUAttachment depth = GPU_ATTACHMENT_NONE,
              GPUAttachment color1 = GPU_ATTACHMENT_NONE,
              GPUAttachment color2 = GPU_ATTACHMENT_NONE,
              GPUAttachment color3 = GPU_ATTACHMENT_NONE,
              GPUAttachment color4 = GPU_ATTACHMENT_NONE,
              GPUAttachment color5 = GPU_ATTACHMENT_NONE,
              GPUAttachment color6 = GPU_ATTACHMENT_NONE,
              GPUAttachment color7 = GPU_ATTACHMENT_NONE,
              GPUAttachment color8 = GPU_ATTACHMENT_NONE)
  {
    if (fb_ == nullptr) {
      fb_ = GPU_framebuffer_create(name_);
    }
    GPUAttachment config[] = {
        depth, color1, color2, color3, color4, color5, color6, color7, color8};
    GPU_framebuffer_config_array(fb_, config, sizeof(config) / sizeof(GPUAttachment));
  }

  /**
   * Empty frame-buffer configuration.
   */
  void ensure(int2 target_size)
  {
    if (fb_ == nullptr) {
      fb_ = GPU_framebuffer_create(name_);
    }
    GPU_framebuffer_default_size(fb_, UNPACK2(target_size));
  }

  void bind()
  {
    GPU_framebuffer_bind(fb_);
  }

  void clear_depth(float depth)
  {
    GPU_framebuffer_clear_depth(fb_, depth);
  }

  Framebuffer &operator=(Framebuffer &&a)
  {
    if (*this != a) {
      this->fb_ = a.fb_;
      this->name_ = a.name_;
      a.fb_ = nullptr;
    }
    return *this;
  }

  operator gpu::FrameBuffer *() const
  {
    return fb_;
  }

  gpu::FrameBuffer **operator&()
  {
    return &fb_;
  }

  /**
   * Swap the content of the two framebuffer.
   */
  static void swap(Framebuffer &a, Framebuffer &b)
  {
    std::swap(a.fb_, b.fb_);
    std::swap(a.name_, b.name_);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Double & Triple buffering util
 *
 * This is not strictly related to a GPU type and could be moved elsewhere.
 * \{ */

template<typename T, int64_t len> class SwapChain {
 private:
  BLI_STATIC_ASSERT(len > 1, "A swap-chain needs more than 1 unit in length.");
  std::array<T, len> chain_;

 public:
  void swap()
  {
    for (auto i : IndexRange(len - 1)) {
      auto i_next = (i + 1) % len;
      if constexpr (std::is_trivial_v<T>) {
        std::swap(chain_[i], chain_[i_next]);
      }
      else {
        T::swap(chain_[i], chain_[i_next]);
      }
    }
  }

  constexpr int64_t size()
  {
    return len;
  }

  T &current()
  {
    return chain_[0];
  }

  T &previous()
  {
    /* Avoid modulo operation with negative numbers. */
    return chain_[(0 + len - 1) % len];
  }

  T &next()
  {
    return chain_[(0 + 1) % len];
  }

  const T &current() const
  {
    return chain_[0];
  }

  const T &previous() const
  {
    /* Avoid modulo operation with negative numbers. */
    return chain_[(0 + len - 1) % len];
  }

  const T &next() const
  {
    return chain_[(0 + 1) % len];
  }
};

/** \} */

}  // namespace blender::draw
