/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_hash.hh"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"
#include "testing/testing.h"

namespace blender::tests {

class ExceptionThrower {
 private:
  /* Use some random values that are unlikely to exist at the memory location already. */
  static constexpr uint32_t is_alive_state = 0x21254634;
  static constexpr uint32_t is_destructed_state = 0xFA4BC327;

  uint32_t state_;

  /* Make use of leak detector to check if this value has been destructed. */
  void *my_memory_;

 public:
  mutable bool throw_during_copy;
  mutable bool throw_during_move;
  /* Used for hashing and comparing. */
  int value;

  ExceptionThrower(int value = 0)
      : state_(is_alive_state),
        my_memory_(MEM_mallocN(1, AT)),
        throw_during_copy(false),
        throw_during_move(false),
        value(value)
  {
  }

  ExceptionThrower(const ExceptionThrower &other) : ExceptionThrower(other.value)
  {
    EXPECT_EQ(other.state_, is_alive_state);
    if (other.throw_during_copy) {
      throw std::runtime_error("throwing during copy, as requested");
    }
  }

  ExceptionThrower(ExceptionThrower &&other) : ExceptionThrower(other.value)
  {
    EXPECT_EQ(other.state_, is_alive_state);
    if (other.throw_during_move) {
      throw std::runtime_error("throwing during move, as requested");
    }
  }

  ExceptionThrower &operator=(const ExceptionThrower &other)
  {
    EXPECT_EQ(other.state_, is_alive_state);
    if (throw_during_copy || other.throw_during_copy) {
      throw std::runtime_error("throwing during copy, as requested");
    }
    value = other.value;
    return *this;
  }

  ExceptionThrower &operator=(ExceptionThrower &&other)
  {
    EXPECT_EQ(other.state_, is_alive_state);
    if (throw_during_move || other.throw_during_move) {
      throw std::runtime_error("throwing during move, as requested");
    }
    value = other.value;
    return *this;
  }

  ~ExceptionThrower()
  {
    const char *message = "";
    if (state_ != is_alive_state) {
      if (state_ == is_destructed_state) {
        message = "Trying to destruct an already destructed instance.";
      }
      else {
        message = "Trying to destruct an uninitialized instance.";
      }
    }
    EXPECT_EQ(state_, is_alive_state) << message;
    state_ = is_destructed_state;
    MEM_freeN(my_memory_);
  }

  uint64_t hash() const
  {
    return uint64_t(value);
  }

  friend bool operator==(const ExceptionThrower &a, const ExceptionThrower &b)
  {
    return a.value == b.value;
  }

  friend bool operator!=(const ExceptionThrower &a, const ExceptionThrower &b)
  {
    return !(a == b);
  }
};

}  // namespace blender::tests
