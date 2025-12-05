/* SPDX-FileCopyrightText: 2023 Blender Authors All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"

#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_vector.hh"

#include "xxhash.h"

#include <mutex>

namespace blender::gpu {

/**
 * \brief Description of vertex inputs used by graphic pipelines.
 *
 * \note Building descriptions are done in #VKVertexAttributeObject.
 */
struct VKVertexInputDescription {
  Vector<VkVertexInputBindingDescription2EXT> bindings;
  Vector<VkVertexInputAttributeDescription2EXT> attributes;

  VKVertexInputDescription() = default;

  void clear();

  bool operator==(const VKVertexInputDescription &other) const
  {
    return attributes.size() == other.attributes.size() &&
           bindings.size() == other.bindings.size() &&
           memcmp(attributes.data(),
                  other.attributes.data(),
                  attributes.size() * sizeof(VkVertexInputAttributeDescription2EXT)) == 0 &&
           memcmp(bindings.data(),
                  other.bindings.data(),
                  bindings.size() * sizeof(VkVertexInputBindingDescription2EXT)) == 0;
  }

  uint64_t hash() const
  {
    uint64_t hash = XXH3_64bits(attributes.data(),
                                attributes.size() * sizeof(VkVertexInputAttributeDescription2EXT));
    hash = hash * 33 ^ XXH3_64bits(bindings.data(),
                                   bindings.size() * sizeof(VkVertexInputBindingDescription2EXT));
    return hash;
  }
};

/**
 * \brief Pool with all used vertex input descriptions.
 *
 * The pool is index based to ensure direct lookup.
 */
class VKVertexInputDescriptionPool {
 public:
  /** Key is indexed based. */
  using Key = int64_t;
  /** Invalid key will assert in debug modes when used. */
  static constexpr Key invalid_key = INT64_MIN;

 private:
  Mutex mutex_;

  Vector<std::unique_ptr<VKVertexInputDescription>> vertex_inputs_;
  Map<VKVertexInputDescription, Key> lookup_;

 public:
  /**
   * \brief Get the key of the given description. Will insert the description when it wasn't known.
   */
  Key get_or_insert(VKVertexInputDescription &description)
  {
    std::scoped_lock lock(mutex_);
    Key *result_ptr = lookup_.lookup_ptr(description);
    if (result_ptr != nullptr) {
      return *result_ptr;
    }

    Key result = vertex_inputs_.size();
    lookup_.add(description, result);
    vertex_inputs_.append(std::make_unique<VKVertexInputDescription>(description));
    return result;
  }

  /**
   * \brief Get the vertex input description of the given key.
   *
   * Will assert when the key isn't known or invalid.
   */
  const VKVertexInputDescription &get(Key key)
  {
    std::scoped_lock lock(mutex_);
    return *vertex_inputs_[key].get();
  }
};

}  // namespace blender::gpu
