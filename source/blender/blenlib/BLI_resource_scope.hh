/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_linear_allocator.hh"
#include "BLI_linear_allocator_chunked_list.hh"
#include "BLI_utility_mixins.hh"

namespace blender {

/**
 * A `ResourceScope` takes ownership of arbitrary data/resources. Those resources will be
 * destructed and/or freed when the `ResourceScope` is destructed. Destruction happens in reverse
 * order. That allows resources do depend on other resources that have been added before.
 *
 * A `ResourceScope` can also be thought of as a dynamic/runtime version of normal scopes in C++
 * that are surrounded by braces.
 *
 * The main purpose of a `ResourceScope` is to allow functions to inject data into the scope of the
 * caller. Traditionally, that can only be done by returning a value that owns everything it needs.
 * This is fine until one has to deal with optional ownership. There are many ways to have a type
 * optionally own something else, all of which are fairly annoying. A `ResourceScope` can be used
 * to avoid having to deal with optional ownership. If some value would be owned, it can just be
 * added to the resource scope, otherwise not.
 *
 * When a function takes a `ResourceScope` as parameter, it usually means that its return value
 * will live at least as long as the passed in resources scope. However, it might also live longer.
 * That can happen when the function returns a reference to statically allocated data or
 * dynamically allocated data depending on some condition.
 */
class ResourceScope : NonMovable {
 private:
  struct ResourceData {
    void *data;
    void (*free)(void *data);
  };
  using ResourceDataList = linear_allocator::ChunkedList<ResourceData, 4>;

  /** This stores all resources. They are later freed in the reverse order of construction. */
  ResourceDataList resources_;
  /**
   * Used allocator. This is either provided by the caller when creating the #ResourceScope, or is
   * owned by the scope itself In the latter case, the ownership of the allocator is also tracked
   * by #resources_ above.
   */
  LinearAllocator<> &allocator_;

 public:
  /**
   * Construct a #ResourceScope with an initial buffer of the given size.
   */
  explicit ResourceScope(int64_t initial_size = 32);

  /**
   * Construct a #ResourceScope in the provided buffer. It may allocate additional memory as
   * necessary though.
   */
  template<size_t Size, size_t Alignment>
  explicit ResourceScope(AlignedBuffer<Size, Alignment> &buffer);

  /**
   * Construct a #ResourceScope in the provided buffer. It may allocate additional memory as
   * necessary though.
   */
  ResourceScope(void *buffer, int64_t size);

  /**
   * Construct a #ResourceScope that uses the given allocator instead of creating a new one.
   */
  explicit ResourceScope(LinearAllocator<> &allocator);

  ~ResourceScope();

  /**
   * Pass ownership of the resource to the #ResourceScope. It will be destructed and freed when
   * the #ResourceScope is destructed.
   */
  template<typename T> T *add(std::unique_ptr<T> resource);

  /**
   * Pass ownership of the resource to the #ResourceScope. The reference count will be decremented
   * when the #ResourceScope is destructed.
   */
  template<typename T> T *add(std::shared_ptr<T> resource);

  /**
   * Pass ownership of the resource to the #ResourceScope. It will be destructed when the
   * #ResourceScope is destructed.
   */
  template<typename T> T *add(destruct_ptr<T> resource);

  /**
   * Pass ownership of some resource to the #ResourceScope. The given free function will be
   * called when the #ResourceScope is destructed.
   */
  void add(void *userdata, void (*free)(void *));

  /**
   * Construct an object with the same value in the #ResourceScope and return a reference to the
   * new value.
   */
  template<typename T> T &add_value(T &&value);

  /**
   * The passed in function will be called when the scope is destructed.
   */
  template<typename Func> void add_destruct_call(Func func);

  /**
   * Utility method to construct an instance of type T that will be owned by the #ResourceScope.
   */
  template<typename T, typename... Args> T &construct(Args &&...args);

  /**
   * Allocate a buffer for the given type. The caller is responsible for initializing it before the
   * #ResourceScope is destructed. The value in the returned buffer is destructed automatically.
   */
  void *allocate_owned(const CPPType &type);

  /**
   * Returns a reference to a linear allocator that is used by the #ResourceScope.
   */
  LinearAllocator<> &allocator();

 private:
  static LinearAllocator<> &create_own_allocator(ResourceDataList &r_resources,
                                                 int64_t initial_size);
  static LinearAllocator<> &create_own_allocator_in_buffer(ResourceDataList &r_resources,
                                                           void *data,
                                                           int64_t size,
                                                           bool free_on_destruct);
};

/* This tests the expected size of #ResourceScope. It's currently easy to break by e.g. inheriting
 * from #NonCopyable. That happens because then there would be two #NonCopyable types at the same
 * address which is not allowed. The proper solution is probably to change how NonCopyable and
 * NonMovable are used overall. */
static_assert(sizeof(ResourceScope) == 16);

/* -------------------------------------------------------------------- */
/** \name #ResourceScope Inline Methods
 * \{ */

inline ResourceScope::ResourceScope(LinearAllocator<> &allocator) : allocator_(allocator) {}

template<size_t Size, size_t Alignment>
inline ResourceScope::ResourceScope(AlignedBuffer<Size, Alignment> &buffer)
    : ResourceScope(buffer.ptr(), Size)
{
}

template<typename T> inline T *ResourceScope::add(std::unique_ptr<T> resource)
{
  T *ptr = resource.release();
  if (ptr == nullptr) {
    return nullptr;
  }
  this->add(ptr, [](void *data) {
    T *typed_data = reinterpret_cast<T *>(data);
    delete typed_data;
  });
  return ptr;
}

template<typename T> inline T *ResourceScope::add(std::shared_ptr<T> resource)
{
  if (!resource) {
    return nullptr;
  }
  T *ptr = resource.get();
  this->add_destruct_call([resource = std::move(resource)]() mutable { resource.reset(); });
  return ptr;
}

template<typename T> inline T *ResourceScope::add(destruct_ptr<T> resource)
{
  T *ptr = resource.release();
  if (ptr == nullptr) {
    return nullptr;
  }
  /* There is no need to keep track of such types. */
  if constexpr (std::is_trivially_destructible_v<T>) {
    return ptr;
  }

  this->add(ptr, [](void *data) {
    T *typed_data = reinterpret_cast<T *>(data);
    typed_data->~T();
  });
  return ptr;
}

inline void ResourceScope::add(void *userdata, void (*free)(void *))
{
  ResourceData data;
  data.data = userdata;
  data.free = free;
  resources_.append(allocator_, data);
}

template<typename T> inline T &ResourceScope::add_value(T &&value)
{
  return this->construct<T>(std::forward<T>(value));
}

template<typename Func> inline void ResourceScope::add_destruct_call(Func func)
{
  void *buffer = allocator_.allocate(sizeof(Func), alignof(Func));
  new (buffer) Func(std::move(func));
  this->add(buffer, [](void *data) { (*static_cast<Func *>(data))(); });
}

template<typename T, typename... Args> inline T &ResourceScope::construct(Args &&...args)
{
  destruct_ptr<T> value_ptr = allocator_.construct<T>(std::forward<Args>(args)...);
  T &value_ref = *value_ptr;
  this->add(std::move(value_ptr));
  return value_ref;
}

inline void *ResourceScope::allocate_owned(const CPPType &type)
{
  void *buffer = allocator_.allocate(type);
  if (!type.is_trivially_destructible) {
    this->add_destruct_call([&type, buffer]() { type.destruct(buffer); });
  }
  return buffer;
}

inline LinearAllocator<> &ResourceScope::allocator()
{
  return allocator_;
}

/** \} */

}  // namespace blender
