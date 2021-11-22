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

#ifndef __UTIL_VECTOR_H__
#define __UTIL_VECTOR_H__

#include <cassert>
#include <cstring>
#include <vector>

#include "util/aligned_malloc.h"
#include "util/guarded_allocator.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* Own subclass-ed version of std::vector. Subclass is needed because:
 *
 * - Use own allocator which keeps track of used/peak memory.
 * - Have method to ensure capacity is re-set to 0.
 */
template<typename value_type, typename allocator_type = GuardedAllocator<value_type>>
class vector : public std::vector<value_type, allocator_type> {
 public:
  typedef std::vector<value_type, allocator_type> BaseClass;

  /* Inherit all constructors from base class. */
  using BaseClass::vector;

  /* Try as hard as possible to use zero memory. */
  void free_memory()
  {
    vector<value_type, allocator_type> empty;
    BaseClass::swap(empty);
  }

  /* Some external API might demand working with std::vector. */
  operator std::vector<value_type>()
  {
    return std::vector<value_type>(this->begin(), this->end());
  }
};

CCL_NAMESPACE_END

#endif /* __UTIL_VECTOR_H__ */
