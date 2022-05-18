/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

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
 * `draw::StorageFlexibleBuffer<T>`
 *   Same as StorageArrayBuffer but will auto resize on access when using the [] operator.
 *
 * `draw::StorageBuffer<T>`
 *   A storage buffer object class inheriting from T.
 *   Data can be accessed just like a normal T object.
 *
 * `draw::Texture`
 *   A simple wrapper to #GPUTexture. A #draw::Texture can be created without allocation.
 *   The `ensure_[1d|2d|3d|cube][_array]()` method is here to make sure the underlying texture
 *   will meet the requirements and create (or recreate) the #GPUTexture if needed.
 *
 * `draw::TextureFromPool`
 *   A GPUTexture from the viewport texture pool. This texture can be shared with other engines
 *   and its content is undefined when acquiring it.
 *   A #draw::TextureFromPool is acquired for rendering using `acquire()` and released once the
 *   rendering is done using `release()`. The same texture can be acquired & released multiple
 *   time in one draw loop.
 *   The `sync()` method *MUST* be called once during the cache populate (aka: Sync) phase.
 *
 * `draw::Framebuffer`
 *   Simple wrapper to #GPUFramebuffer that can be moved.
 *
 */

#include "DRW_render.h"

#include "MEM_guardedalloc.h"

#include "draw_texture_pool.h"

#include "BLI_math_vec_types.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "GPU_framebuffer.h"
#include "GPU_storage_buffer.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

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
    BLI_assert(index < len);
    return data_[index];
  }

  T &operator[](int64_t index)
  {
    BLI_STATIC_ASSERT(!device_only, "");
    BLI_assert(index >= 0);
    BLI_assert(index < len);
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
    return data_ + len;
  }

  T *begin()
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return data_;
  }
  T *end()
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return data_ + len;
  }

  operator Span<T>() const
  {
    BLI_STATIC_ASSERT(!device_only, "");
    return Span<T>(data_, len);
  }
};

template<typename T, int64_t len, bool device_only>
class UniformCommon : public DataBuffer<T, len, false>, NonMovable, NonCopyable {
 protected:
  GPUUniformBuf *ubo_;

#ifdef DEBUG
  const char *name_ = typeid(T).name();
#else
  const char *name_ = "UniformBuffer";
#endif

 public:
  UniformCommon()
  {
    ubo_ = GPU_uniformbuf_create_ex(sizeof(T) * len, nullptr, name_);
  }

  ~UniformCommon()
  {
    GPU_uniformbuf_free(ubo_);
  }

  void push_update(void)
  {
    GPU_uniformbuf_update(ubo_, this->data_);
  }

  /* To be able to use it with DRW_shgroup_*_ref(). */
  operator GPUUniformBuf *() const
  {
    return ubo_;
  }

  /* To be able to use it with DRW_shgroup_*_ref(). */
  GPUUniformBuf **operator&()
  {
    return &ubo_;
  }
};

template<typename T, int64_t len, bool device_only>
class StorageCommon : public DataBuffer<T, len, false>, NonMovable, NonCopyable {
 protected:
  GPUStorageBuf *ssbo_;

#ifdef DEBUG
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
    init(len);
  }

  ~StorageCommon()
  {
    GPU_storagebuf_free(ssbo_);
  }

  void resize(int64_t new_size)
  {
    BLI_assert(new_size > 0);
    if (new_size != this->len_) {
      GPU_storagebuf_free(ssbo_);
      this->init(new_size);
    }
  }

  void push_update(void)
  {
    BLI_assert(device_only == false);
    GPU_storagebuf_update(ssbo_, this->data_);
  }

  operator GPUStorageBuf *() const
  {
    return ssbo_;
  }
  /* To be able to use it with DRW_shgroup_*_ref(). */
  GPUStorageBuf **operator&()
  {
    return &ssbo_;
  }

 private:
  void init(int64_t new_size)
  {
    this->len_ = new_size;
    GPUUsageType usage = device_only ? GPU_USAGE_DEVICE_ONLY : GPU_USAGE_DYNAMIC;
    ssbo_ = GPU_storagebuf_create_ex(sizeof(T) * this->len_, nullptr, usage, this->name_);
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
  UniformArrayBuffer()
  {
    /* TODO(@fclem): We should map memory instead. */
    this->data_ = (T *)MEM_mallocN_aligned(len * sizeof(T), 16, this->name_);
  }
  ~UniformArrayBuffer()
  {
    MEM_freeN(this->data_);
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
  UniformBuffer()
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
    /** The number of values that can be stored in this uniform buffer. */
    int64_t len,
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
    MEM_freeN(this->data_);
  }
};

template<
    /** Type of the values stored in this uniform buffer. */
    typename T,
    /** True if created on device and no memory host memory is allocated. */
    bool device_only = false>
class StorageFlexibleBuffer : public detail::StorageCommon<T, 1, device_only> {
 public:
  StorageFlexibleBuffer(const char *name = nullptr)
      : detail::StorageCommon<T, 1, device_only>(name)
  {
    /* TODO(@fclem): We should map memory instead. */
    this->data_ = (T *)MEM_mallocN_aligned(sizeof(T), 16, this->name_);
  }
  ~StorageFlexibleBuffer()
  {
    MEM_freeN(this->data_);
  }

  /* Resize on access. */
  T &operator[](int64_t index)
  {
    BLI_STATIC_ASSERT(!device_only, "");
    BLI_assert(index >= 0);
    if (index >= this->len_) {
      this->resize(this->len_ * 2);
    }
    return this->data_[index];
  }

  /* TODO(fclem): Implement shrinking. */
};

template<
    /** Type of the values stored in this uniform buffer. */
    typename T,
    /** True if created on device and no memory host memory is allocated. */
    bool device_only = false>
class StorageBuffer : public T, public detail::StorageCommon<T, 1, device_only> {
 public:
  StorageBuffer(const char *name = nullptr) : detail::StorageCommon<T, 1, device_only>(name)
  {
    /* TODO(@fclem): How could we map this? */
    this->data_ = static_cast<T *>(this);
  }

  StorageBuffer<T> &operator=(const T &other)
  {
    *static_cast<T *>(this) = other;
    return *this;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture
 * \{ */

class Texture : NonCopyable {
 protected:
  GPUTexture *tx_ = nullptr;
  GPUTexture *stencil_view_ = nullptr;
  Vector<GPUTexture *, 0> mip_views_;
  Vector<GPUTexture *, 0> layer_views_;
  const char *name_;

 public:
  Texture(const char *name = "gpu::Texture") : name_(name)
  {
  }

  Texture(const char *name,
          eGPUTextureFormat format,
          int extent,
          float *data = nullptr,
          bool cubemap = false,
          int mips = 1)
      : name_(name)
  {
    tx_ = create(extent, 0, 0, mips, format, data, false, cubemap);
  }

  Texture(const char *name,
          eGPUTextureFormat format,
          int extent,
          int layers,
          float *data = nullptr,
          bool cubemap = false,
          int mips = 1)
      : name_(name)
  {
    tx_ = create(extent, layers, 0, mips, format, data, true, cubemap);
  }

  Texture(
      const char *name, eGPUTextureFormat format, int2 extent, float *data = nullptr, int mips = 1)
      : name_(name)
  {
    tx_ = create(UNPACK2(extent), 0, mips, format, data, false, false);
  }

  Texture(const char *name,
          eGPUTextureFormat format,
          int2 extent,
          int layers,
          float *data = nullptr,
          int mips = 1)
      : name_(name)
  {
    tx_ = create(UNPACK2(extent), layers, mips, format, data, true, false);
  }

  Texture(
      const char *name, eGPUTextureFormat format, int3 extent, float *data = nullptr, int mips = 1)
      : name_(name)
  {
    tx_ = create(UNPACK3(extent), mips, format, data, false, false);
  }

  ~Texture()
  {
    free();
  }

  /* To be able to use it with DRW_shgroup_uniform_texture(). */
  operator GPUTexture *() const
  {
    BLI_assert(tx_ != nullptr);
    return tx_;
  }

  /* To be able to use it with DRW_shgroup_uniform_texture_ref(). */
  GPUTexture **operator&()
  {
    return &tx_;
  }

  Texture &operator=(Texture &&a)
  {
    if (*this != a) {
      this->tx_ = a.tx_;
      this->name_ = a.name_;
      a.tx_ = nullptr;
    }
    return *this;
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_1d(eGPUTextureFormat format, int extent, float *data = nullptr, int mips = 1)
  {
    return ensure_impl(extent, 0, 0, mips, format, data, false, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_1d_array(
      eGPUTextureFormat format, int extent, int layers, float *data = nullptr, int mips = 1)
  {
    return ensure_impl(extent, layers, 0, mips, format, data, true, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_2d(eGPUTextureFormat format, int2 extent, float *data = nullptr, int mips = 1)
  {
    return ensure_impl(UNPACK2(extent), 0, mips, format, data, false, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_2d_array(
      eGPUTextureFormat format, int2 extent, int layers, float *data = nullptr, int mips = 1)
  {
    return ensure_impl(UNPACK2(extent), layers, mips, format, data, true, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_3d(eGPUTextureFormat format, int3 extent, float *data = nullptr, int mips = 1)
  {
    return ensure_impl(UNPACK3(extent), mips, format, data, false, false);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_cube(eGPUTextureFormat format, int extent, float *data = nullptr, int mips = 1)
  {
    return ensure_impl(extent, extent, 0, mips, format, data, false, true);
  }

  /**
   * Ensure the texture has the correct properties. Recreating it if needed.
   * Return true if a texture has been created.
   */
  bool ensure_cube_array(
      eGPUTextureFormat format, int extent, int layers, float *data = nullptr, int mips = 1)
  {
    return ensure_impl(extent, extent, layers, mips, format, data, false, true);
  }

  /**
   * Ensure the availability of mipmap views.
   * MIP view covers all layers of array textures.
   */
  bool ensure_mip_views(bool cube_as_array = false)
  {
    int mip_len = GPU_texture_mip_count(tx_);
    if (mip_views_.size() != mip_len) {
      for (GPUTexture *&view : mip_views_) {
        GPU_TEXTURE_FREE_SAFE(view);
      }
      eGPUTextureFormat format = GPU_texture_format(tx_);
      for (auto i : IndexRange(mip_len)) {
        mip_views_.append(
            GPU_texture_create_view(name_, tx_, format, i, 1, 0, 9999, cube_as_array));
      }
      return true;
    }
    return false;
  }

  GPUTexture *mip_view(int miplvl)
  {
    return mip_views_[miplvl];
  }

  /**
   * Ensure the availability of mipmap views.
   * Layer views covers all layers of array textures.
   */
  bool ensure_layer_views(bool cube_as_array = false)
  {
    int layer_len = GPU_texture_layer_count(tx_);
    if (layer_views_.size() != layer_len) {
      for (GPUTexture *&view : layer_views_) {
        GPU_TEXTURE_FREE_SAFE(view);
      }
      eGPUTextureFormat format = GPU_texture_format(tx_);
      for (auto i : IndexRange(layer_len)) {
        layer_views_.append(
            GPU_texture_create_view(name_, tx_, format, 0, 9999, i, 1, cube_as_array));
      }
      return true;
    }
    return false;
  }

  GPUTexture *layer_view(int layer)
  {
    return layer_views_[layer];
  }

  GPUTexture *stencil_view(bool cube_as_array = false)
  {
    if (stencil_view_ == nullptr) {
      eGPUTextureFormat format = GPU_texture_format(tx_);
      stencil_view_ = GPU_texture_create_view(name_, tx_, format, 0, 9999, 0, 9999, cube_as_array);
      GPU_texture_stencil_texture_mode_set(stencil_view_, true);
    }
    return stencil_view_;
  }

  /**
   * Returns true if the texture has been allocated or acquired from the pool.
   */
  bool is_valid(void) const
  {
    return tx_ != nullptr;
  }

  int width(void) const
  {
    return GPU_texture_width(tx_);
  }

  int height(void) const
  {
    return GPU_texture_height(tx_);
  }

  bool depth(void) const
  {
    return GPU_texture_depth(tx_);
  }

  bool is_stencil(void) const
  {
    return GPU_texture_stencil(tx_);
  }

  bool is_integer(void) const
  {
    return GPU_texture_integer(tx_);
  }

  bool is_cube(void) const
  {
    return GPU_texture_cube(tx_);
  }

  bool is_array(void) const
  {
    return GPU_texture_array(tx_);
  }

  int3 size(int miplvl = 0) const
  {
    int3 size(0);
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
    for (GPUTexture *&view : mip_views_) {
      GPU_TEXTURE_FREE_SAFE(view);
    }
    for (GPUTexture *&view : layer_views_) {
      GPU_TEXTURE_FREE_SAFE(view);
    }
    GPU_TEXTURE_FREE_SAFE(stencil_view_);
    mip_views_.clear();
  }

  /**
   * Swap the content of the two textures.
   */
  static void swap(Texture &a, Texture &b)
  {
    SWAP(GPUTexture *, a.tx_, b.tx_);
    SWAP(const char *, a.name_, b.name_);
  }

 private:
  bool ensure_impl(int w,
                   int h = 0,
                   int d = 0,
                   int mips = 1,
                   eGPUTextureFormat format = GPU_RGBA8,
                   float *data = nullptr,
                   bool layered = false,
                   bool cubemap = false)

  {
    /* TODO(@fclem): In the future, we need to check if mip_count did not change.
     * For now it's ok as we always define all MIP level. */
    if (tx_) {
      int3 size = this->size();
      if (size != int3(w, h, d) || GPU_texture_format(tx_) != format ||
          GPU_texture_cube(tx_) != cubemap || GPU_texture_array(tx_) != layered) {
        GPU_TEXTURE_FREE_SAFE(tx_);
      }
    }
    if (tx_ == nullptr) {
      tx_ = create(w, h, d, mips, format, data, layered, cubemap);
      return true;
    }
    return false;
  }

  GPUTexture *create(int w,
                     int h,
                     int d,
                     int mips,
                     eGPUTextureFormat format,
                     float *data,
                     bool layered,
                     bool cubemap)
  {
    if (h == 0) {
      return GPU_texture_create_1d(name_, w, mips, format, data);
    }
    else if (cubemap) {
      if (layered) {
        return GPU_texture_create_cube_array(name_, w, d, mips, format, data);
      }
      else {
        return GPU_texture_create_cube(name_, w, mips, format, data);
      }
    }
    else if (d == 0) {
      if (layered) {
        return GPU_texture_create_1d_array(name_, w, h, mips, format, data);
      }
      else {
        return GPU_texture_create_2d(name_, w, h, mips, format, data);
      }
    }
    else {
      if (layered) {
        return GPU_texture_create_2d_array(name_, w, h, d, mips, format, data);
      }
      else {
        return GPU_texture_create_3d(name_, w, h, d, mips, format, GPU_DATA_FLOAT, data);
      }
    }
  }
};

class TextureFromPool : public Texture, NonMovable {
 private:
  GPUTexture *tx_tmp_saved_ = nullptr;

 public:
  TextureFromPool(const char *name = "gpu::Texture") : Texture(name){};

  /* Always use `release()` after rendering and `sync()` in sync phase. */
  void acquire(int2 extent, eGPUTextureFormat format, void *owner_)
  {
    BLI_assert(this->tx_ == nullptr);
    if (this->tx_ != nullptr) {
      return;
    }
    if (tx_tmp_saved_ != nullptr) {
      if (GPU_texture_width(tx_tmp_saved_) != extent.x ||
          GPU_texture_height(tx_tmp_saved_) != extent.y ||
          GPU_texture_format(tx_tmp_saved_) != format) {
        this->tx_tmp_saved_ = nullptr;
      }
      else {
        this->tx_ = tx_tmp_saved_;
        return;
      }
    }
    DrawEngineType *owner = (DrawEngineType *)owner_;
    this->tx_ = DRW_texture_pool_query_2d(UNPACK2(extent), format, owner);
  }

  void release(void)
  {
    /* Allows multiple release. */
    if (this->tx_ != nullptr) {
      tx_tmp_saved_ = this->tx_;
      this->tx_ = nullptr;
    }
  }

  /**
   * Clears any reference. Workaround for pool texture not being able to release on demand.
   * Needs to be called at during the sync phase.
   */
  void sync(void)
  {
    tx_tmp_saved_ = nullptr;
  }

  /** Remove methods that are forbidden with this type of textures. */
  bool ensure_1d(int, int, eGPUTextureFormat, float *) = delete;
  bool ensure_1d_array(int, int, int, eGPUTextureFormat, float *) = delete;
  bool ensure_2d(int, int, int, eGPUTextureFormat, float *) = delete;
  bool ensure_2d_array(int, int, int, int, eGPUTextureFormat, float *) = delete;
  bool ensure_3d(int, int, int, int, eGPUTextureFormat, float *) = delete;
  bool ensure_cube(int, int, eGPUTextureFormat, float *) = delete;
  bool ensure_cube_array(int, int, int, eGPUTextureFormat, float *) = delete;
  void filter_mode(bool) = delete;
  void free() = delete;
  GPUTexture *mip_view(int) = delete;
  GPUTexture *layer_view(int) = delete;
  GPUTexture *stencil_view() = delete;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Framebuffer
 * \{ */

class Framebuffer : NonCopyable {
 private:
  GPUFrameBuffer *fb_ = nullptr;
  const char *name_;

 public:
  Framebuffer() : name_(""){};
  Framebuffer(const char *name) : name_(name){};

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
    GPU_framebuffer_ensure_config(
        &fb_, {depth, color1, color2, color3, color4, color5, color6, color7, color8});
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

  operator GPUFrameBuffer *() const
  {
    return fb_;
  }

  /**
   * Swap the content of the two framebuffer.
   */
  static void swap(Framebuffer &a, Framebuffer &b)
  {
    SWAP(GPUFrameBuffer *, a.fb_, b.fb_);
    SWAP(const char *, a.name_, b.name_);
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
  std::array<T, len> chain_;
  int64_t index_ = 0;

 public:
  void swap()
  {
    index_ = (index_ + 1) % len;
  }

  T &current()
  {
    return chain_[index_];
  }

  T &previous()
  {
    /* Avoid modulo operation with negative numbers. */
    return chain_[(index_ + len - 1) % len];
  }

  T &next()
  {
    return chain_[(index_ + 1) % len];
  }

  const T &current() const
  {
    return chain_[index_];
  }

  const T &previous() const
  {
    /* Avoid modulo operation with negative numbers. */
    return chain_[(index_ + len - 1) % len];
  }

  const T &next() const
  {
    return chain_[(index_ + 1) % len];
  }
};

/** \} */

}  // namespace blender::draw
