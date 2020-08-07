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
 * This file implements different probing strategies. Those can be used by different hash table
 * implementations like blender::Set and blender::Map. A probing strategy produces a sequence of
 * values based on an initial hash value.
 *
 * A probing strategy has to implement the following methods:
 * - Constructor(uint64_t hash): Start a new probing sequence based on the given hash.
 * - get() const -> uint64_t: Get the current value in the sequence.
 * - next() -> void: Update the internal state, so that the next value can be accessed with get().
 * - linear_steps() -> int64_t: Returns number of linear probing steps that should be done.
 *
 * Using linear probing steps between larger jumps can result in better performance, due to
 * improved cache usage. It's a way of getting the benefits or linear probing without the
 * clustering issues. However, more linear steps can also make things slower when the initial hash
 * produces many collisions.
 *
 * Every probing strategy has to guarantee, that every possible uint64_t is returned eventually.
 * This is necessary for correctness. If this is not the case, empty slots might not be found.
 *
 * The SLOT_PROBING_BEGIN and SLOT_PROBING_END macros can be used to implement a loop that iterates
 * over a probing sequence.
 *
 * Probing strategies can be evaluated with many different criteria. Different use cases often
 * have different optimal strategies. Examples:
 * - If the hash function generates a well distributed initial hash value, the constructor should
 *   be as short as possible. This is because the hash value can be used as slot index almost
 *   immediately, without too many collisions. This is also a perfect use case for linear steps.
 * - If the hash function is bad, it can help if the probing strategy remixes the hash value,
 *   before the first slot is accessed.
 * - Different next() methods can remix the hash value in different ways. Depending on which bits
 *   of the hash value contain the most information, different rehashing strategies work best.
 * - When the hash table is very small, having a trivial hash function and then doing linear
 *   probing might work best.
 */

#include "BLI_sys_types.h"

namespace blender {

/**
 * The simplest probing strategy. It's bad in most cases, because it produces clusters in the hash
 * table, which result in many collisions. However, if the hash function is very good or the hash
 * table is small, this strategy might even work best.
 */
class LinearProbingStrategy {
 private:
  uint64_t hash_;

 public:
  LinearProbingStrategy(const uint64_t hash) : hash_(hash)
  {
  }

  void next()
  {
    hash_++;
  }

  uint64_t get() const
  {
    return hash_;
  }

  int64_t linear_steps() const
  {
    return UINT32_MAX;
  }
};

/**
 * A slightly adapted quadratic probing strategy. The distance to the original slot increases
 * quadratically. This method also leads to clustering. Another disadvantage is that not all bits
 * of the original hash are used.
 *
 * The distance i * i is not used, because it does not guarantee, that every slot is hit.
 * Instead (i * i + i) / 2 is used, which has this desired property.
 *
 * In the first few steps, this strategy can have good cache performance. It largely depends on how
 * many keys fit into a cache line in the hash table.
 */
class QuadraticProbingStrategy {
 private:
  uint64_t original_hash_;
  uint64_t current_hash_;
  uint64_t iteration_;

 public:
  QuadraticProbingStrategy(const uint64_t hash)
      : original_hash_(hash), current_hash_(hash), iteration_(1)
  {
  }

  void next()
  {
    current_hash_ = original_hash_ + ((iteration_ * iteration_ + iteration_) >> 1);
    iteration_++;
  }

  uint64_t get() const
  {
    return current_hash_;
  }

  int64_t linear_steps() const
  {
    return 1;
  }
};

/**
 * This is the probing strategy used by CPython (in 2020).
 *
 * It is very fast when the original hash value is good. If there are collisions, more bits of the
 * hash value are taken into account.
 *
 * LinearSteps: Can be set to something larger than 1 for improved cache performance in some cases.
 * PreShuffle: When true, the initial call to next() will be done to the constructor. This can help
 *   when the hash function has put little information into the lower bits.
 */
template<uint64_t LinearSteps = 1, bool PreShuffle = false> class PythonProbingStrategy {
 private:
  uint64_t hash_;
  uint64_t perturb_;

 public:
  PythonProbingStrategy(const uint64_t hash) : hash_(hash), perturb_(hash)
  {
    if (PreShuffle) {
      this->next();
    }
  }

  void next()
  {
    perturb_ >>= 5;
    hash_ = 5 * hash_ + 1 + perturb_;
  }

  uint64_t get() const
  {
    return hash_;
  }

  int64_t linear_steps() const
  {
    return LinearSteps;
  }
};

/**
 * Similar to the Python probing strategy. However, it does a bit more shuffling in the next()
 * method. This way more bits are taken into account earlier. After a couple of collisions (that
 * should happen rarely), it will fallback to a sequence that hits every slot.
 */
template<uint64_t LinearSteps = 2, bool PreShuffle = false> class ShuffleProbingStrategy {
 private:
  uint64_t hash_;
  uint64_t perturb_;

 public:
  ShuffleProbingStrategy(const uint64_t hash) : hash_(hash), perturb_(hash)
  {
    if (PreShuffle) {
      this->next();
    }
  }

  void next()
  {
    if (perturb_ != 0) {
      perturb_ >>= 10;
      hash_ = ((hash_ >> 16) ^ hash_) * 0x45d9f3b + perturb_;
    }
    else {
      hash_ = 5 * hash_ + 1;
    }
  }

  uint64_t get() const
  {
    return hash_;
  }

  int64_t linear_steps() const
  {
    return LinearSteps;
  }
};

/**
 * Having a specified default is convenient.
 */
using DefaultProbingStrategy = PythonProbingStrategy<>;

/* Turning off clang format here, because otherwise it will mess up the alignment between the
 * macros. */
// clang-format off

/**
 * Both macros together form a loop that iterates over slot indices in a hash table with a
 * power-of-two size.
 *
 * You must not `break` out of this loop. Only `return` is permitted. If you don't return
 * out of the loop, it will be an infinite loop. These loops should not be nested within the
 * same function.
 *
 * PROBING_STRATEGY: Class describing the probing strategy.
 * HASH: The initial hash as produced by a hash function.
 * MASK: A bit mask such that (hash & MASK) is a valid slot index.
 * R_SLOT_INDEX: Name of the variable that will contain the slot index.
 */
#define SLOT_PROBING_BEGIN(PROBING_STRATEGY, HASH, MASK, R_SLOT_INDEX) \
  PROBING_STRATEGY probing_strategy(HASH); \
  do { \
    int64_t linear_offset = 0; \
    uint64_t current_hash = probing_strategy.get(); \
    do { \
      int64_t R_SLOT_INDEX = static_cast<int64_t>((current_hash + static_cast<uint64_t>(linear_offset)) & MASK);

#define SLOT_PROBING_END() \
    } while (++linear_offset < probing_strategy.linear_steps()); \
    probing_strategy.next(); \
  } while (true)

// clang-format on

}  // namespace blender
