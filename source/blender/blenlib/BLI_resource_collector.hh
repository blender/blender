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
 * A ResourceCollector holds an arbitrary set of resources, that will be destructed and/or freed
 * when the ResourceCollector is destructed. This is useful when some object has to take ownership
 * of other objects, but it does not know the type of those other objects.
 *
 * Resources owned by the ResourceCollector will be freed in reverse order. That allows resources
 * that are added later to depend on resources that have been added before.
 */

#include "BLI_linear_allocator.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

class ResourceCollector : NonCopyable, NonMovable {
 private:
  struct ResourceData {
    void *data;
    void (*free)(void *data);
    const char *debug_name;
  };

  LinearAllocator<> m_allocator;
  Vector<ResourceData> m_resources;

 public:
  ResourceCollector() = default;

  ~ResourceCollector()
  {
    /* Free in reversed order. */
    for (int64_t i = m_resources.size(); i--;) {
      ResourceData &data = m_resources[i];
      data.free(data.data);
    }
  }

  /**
   * Pass ownership of the resource to the ResourceCollector. It will be destructed and freed when
   * the collector is destructed.
   */
  template<typename T> void add(std::unique_ptr<T> resource, const char *name)
  {
    BLI_assert(resource.get() != nullptr);
    this->add(
        resource.release(),
        [](void *data) {
          T *typed_data = reinterpret_cast<T *>(data);
          delete typed_data;
        },
        name);
  }

  /**
   * Pass ownership of the resource to the ResourceCollector. It will be destructed when the
   * collector is destructed.
   */
  template<typename T> void add(destruct_ptr<T> resource, const char *name)
  {
    /* There is no need to keep track of such types. */
    if (std::is_trivially_destructible_v<T>) {
      resource.release();
      return;
    }

    BLI_assert(resource.get() != nullptr);
    this->add(
        resource.release(),
        [](void *data) {
          T *typed_data = reinterpret_cast<T *>(data);
          typed_data->~T();
        },
        name);
  }

  /**
   * Pass ownership of some resource to the ResourceCollector. The given free function will be
   * called when the collector is destructed.
   */
  void add(void *userdata, void (*free)(void *), const char *name)
  {
    ResourceData data;
    data.debug_name = name;
    data.data = userdata;
    data.free = free;
    m_resources.append(data);
  }

  /**
   * Construct an object with the same value in the ResourceCollector and return a reference to the
   * new value.
   */
  template<typename T> T &add_value(T &&value, const char *name)
  {
    return this->construct<T>(name, std::forward<T>(value));
  }

  /**
   * Returns a reference to a linear allocator that is owned by the ResourcesCollector. Memory
   * allocated through this allocator will be freed when the collector is destructed.
   */
  LinearAllocator<> &linear_allocator()
  {
    return m_allocator;
  }

  /**
   * Utility method to construct an instance of type T that will be owned by the ResourceCollector.
   */
  template<typename T, typename... Args> T &construct(const char *name, Args &&... args)
  {
    destruct_ptr<T> value_ptr = m_allocator.construct<T>(std::forward<Args>(args)...);
    T &value_ref = *value_ptr;
    this->add(std::move(value_ptr), name);
    return value_ref;
  }

  /**
   * Print the names of all the resources that are owned by this ResourceCollector. This can be
   * useful for debugging.
   */
  void print(StringRef name) const
  {
    if (m_resources.size() == 0) {
      std::cout << "\"" << name << "\" has no resources.\n";
      return;
    }
    else {
      std::cout << "Resources for \"" << name << "\":\n";
      for (const ResourceData &data : m_resources) {
        std::cout << "  " << data.data << ": " << data.debug_name << '\n';
      }
    }
  }
};

}  // namespace blender
