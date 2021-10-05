/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bli
 *
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

#include "BLI_linear_allocator.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

class ResourceScope : NonCopyable, NonMovable {
 private:
  struct ResourceData {
    void *data;
    void (*free)(void *data);
  };

  LinearAllocator<> allocator_;
  Vector<ResourceData> resources_;

 public:
  ResourceScope();
  ~ResourceScope();

  template<typename T> T *add(std::unique_ptr<T> resource);
  template<typename T> T *add(destruct_ptr<T> resource);
  void add(void *userdata, void (*free)(void *));

  template<typename T> T &add_value(T &&value);
  template<typename Func> void add_destruct_call(Func func);

  template<typename T, typename... Args> T &construct(Args &&...args);

  LinearAllocator<> &linear_allocator();
};

/* -------------------------------------------------------------------- */
/** \name #ResourceScope Inline Methods
 * \{ */

/**
 * Pass ownership of the resource to the ResourceScope. It will be destructed and freed when
 * the collector is destructed.
 */
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

/**
 * Pass ownership of the resource to the ResourceScope. It will be destructed when the
 * collector is destructed.
 */
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

/**
 * Pass ownership of some resource to the ResourceScope. The given free function will be
 * called when the collector is destructed.
 */
inline void ResourceScope::add(void *userdata, void (*free)(void *))
{
  ResourceData data;
  data.data = userdata;
  data.free = free;
  resources_.append(data);
}

/**
 * Construct an object with the same value in the ResourceScope and return a reference to the
 * new value.
 */
template<typename T> inline T &ResourceScope::add_value(T &&value)
{
  return this->construct<T>(std::forward<T>(value));
}

/**
 * The passed in function will be called when the scope is destructed.
 */
template<typename Func> inline void ResourceScope::add_destruct_call(Func func)
{
  void *buffer = allocator_.allocate(sizeof(Func), alignof(Func));
  new (buffer) Func(std::move(func));
  this->add(buffer, [](void *data) { (*(Func *)data)(); });
}

/**
 * Utility method to construct an instance of type T that will be owned by the ResourceScope.
 */
template<typename T, typename... Args> inline T &ResourceScope::construct(Args &&...args)
{
  destruct_ptr<T> value_ptr = allocator_.construct<T>(std::forward<Args>(args)...);
  T &value_ref = *value_ptr;
  this->add(std::move(value_ptr));
  return value_ref;
}

/**
 * Returns a reference to a linear allocator that is owned by the ResourcesCollector. Memory
 * allocated through this allocator will be freed when the collector is destructed.
 */
inline LinearAllocator<> &ResourceScope::linear_allocator()
{
  return allocator_;
}

/** \} */

}  // namespace blender
