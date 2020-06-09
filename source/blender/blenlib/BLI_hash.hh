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

#ifndef __BLI_HASH_HH__
#define __BLI_HASH_HH__

/** \file
 * \ingroup bli
 *
 * A specialization of `BLI::DefaultHash<T>` provides a hash function for values of type T. This
 * hash function is used by default in hash table implementations in blenlib.
 *
 * The actual hash function is in the `operator()` method of DefaultHash<T>. The following code
 * computes the hash of some value using DefaultHash.
 *
 *   T value = ...;
 *   DefaultHash<T> hash_function;
 *   uint32_t hash = hash_function(value);
 *
 * Hash table implementations like BLI::Set support heterogeneous key lookups. That means that one
 * can do a lookup with a key of type A in a hash table that stores keys of type B. This is
 * commonly done when B is std::string, because the conversion from e.g. a StringRef to std::string
 * can be costly and is unnecessary. To make this work, values of type A and B that compare equal
 * have to have the same hash value. This is achieved by defining potentially multiple `operator()`
 * in a specialization of DefaultHash. All those methods have to compute the same hash for values
 * that compare equal.
 *
 * The computed hash is an unsigned 32 bit integer. Ideally, the hash function would generate
 * uniformly random hash values for a set of keys. However, in many cases trivial hash functions
 * are faster and produce a good enough distribution. In general it is better when more information
 * is in the lower bits of the hash. By choosing a good probing strategy, the effects of a bad hash
 * function are less noticable though. In this context a good probing strategy is one that takes
 * all bits of the hash into account eventually. One has to check on a case by case basis to see if
 * a better but more expensive or trivial hash function works better.
 *
 * There are three main ways to provide a hash table implementation with a custom hash function.
 *
 * - When you want to provide a default hash function for your own custom type: Add a `hash`
 *   member function to it. The function should return `uint32_t` and take no arguments. This
 *   method will be called by the default implementation of DefaultHash. It will automatically be
 *   used by hash table implementations.
 *
 * - When you want to provide a default hash function for a type that you cannot modify: Add a new
 *   specialization to the DefaultHash struct. This can be done by writing code like below in
 *   either global or BLI namespace.
 *
 *     template<> struct BLI::DefaultHash<TheType> {
 *       uint32_t operator()(const TheType &value) const {
 *         return ...;
 *       }
 *     };
 *
 * - When you want to provide a different hash function for a type that already has a default hash
 *   function: Implement a struct like the one below and pass it as template parameter to the hash
 *   table explicitely.
 *
 *     struct MyCustomHash {
 *       uint32_t operator()(const TheType &value) const {
 *         return ...;
 *       }
 *     };
 */

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "BLI_math_base.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

namespace BLI {

/**
 * If there is no other specialization of DefaultHash for a given type, try to call `hash()` on the
 * value. If there is no such method, this will result in a compiler error. Usually that means that
 * you have to implement a hash function using one of three strategies listed above.
 */
template<typename T> struct DefaultHash {
  uint32_t operator()(const T &value) const
  {
    return value.hash();
  }
};

#define TRIVIAL_DEFAULT_INT_HASH(TYPE) \
  template<> struct DefaultHash<TYPE> { \
    uint32_t operator()(TYPE value) const \
    { \
      return (uint32_t)value; \
    } \
  }

/**
 * We cannot make any assumptions about the distribution of keys, so use a trivial hash function by
 * default. The default probing strategy is designed to take all bits of the hash into account
 * to avoid worst case behavior when the lower bits are all zero. Special hash functions can be
 * implemented when more knowledge about a specific key distribution is available.
 */
TRIVIAL_DEFAULT_INT_HASH(int8_t);
TRIVIAL_DEFAULT_INT_HASH(uint8_t);
TRIVIAL_DEFAULT_INT_HASH(int16_t);
TRIVIAL_DEFAULT_INT_HASH(uint16_t);
TRIVIAL_DEFAULT_INT_HASH(int32_t);
TRIVIAL_DEFAULT_INT_HASH(uint32_t);

template<> struct DefaultHash<uint64_t> {
  uint32_t operator()(uint64_t value) const
  {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    return low ^ (high * 0x45d9f3b);
  }
};

template<> struct DefaultHash<int64_t> {
  uint32_t operator()(uint64_t value) const
  {
    return DefaultHash<uint64_t>{}((uint64_t)value);
  }
};

/**
 * One should try to avoid using floats as keys in hash tables, but sometimes it is convenient.
 */
template<> struct DefaultHash<float> {
  uint32_t operator()(float value) const
  {
    return *(uint32_t *)&value;
  }
};

inline uint32_t hash_string(StringRef str)
{
  uint32_t hash = 5381;
  for (char c : str) {
    hash = hash * 33 + c;
  }
  return hash;
}

template<> struct DefaultHash<std::string> {
  /**
   * Take a StringRef as parameter to support heterogeneous lookups in hash table implementations
   * when std::string is used as key.
   */
  uint32_t operator()(StringRef value) const
  {
    return hash_string(value);
  }
};

template<> struct DefaultHash<StringRef> {
  uint32_t operator()(StringRef value) const
  {
    return hash_string(value);
  }
};

template<> struct DefaultHash<StringRefNull> {
  uint32_t operator()(StringRef value) const
  {
    return hash_string(value);
  }
};

/**
 * While we cannot guarantee that the lower 4 bits of a pointer are zero, it is often the case.
 */
template<typename T> struct DefaultHash<T *> {
  uint32_t operator()(const T *value) const
  {
    uintptr_t ptr = (uintptr_t)value;
    uint32_t hash = (uint32_t)(ptr >> 4);
    return hash;
  }
};

template<typename T> struct DefaultHash<std::unique_ptr<T>> {
  uint32_t operator()(const std::unique_ptr<T> &value) const
  {
    return DefaultHash<T *>{}(value.get());
  }
};

template<typename T1, typename T2> struct DefaultHash<std::pair<T1, T2>> {
  uint32_t operator()(const std::pair<T1, T2> &value) const
  {
    uint32_t hash1 = DefaultHash<T1>{}(value.first);
    uint32_t hash2 = DefaultHash<T2>{}(value.second);
    return hash1 ^ (hash2 * 33);
  }
};

}  // namespace BLI

#endif /* __BLI_HASH_HH__ */
