#include "BLI_utildefines.h"
#include "testing/testing.h"

namespace blender::tests {

struct ExceptionThrower {
  static constexpr uint32_t is_alive_state = 0x21254634;
  static constexpr uint32_t is_destructed_state = 0xFA4BC327;
  uint32_t state;
  bool throw_during_copy;
  bool throw_during_move;

  ExceptionThrower() : state(is_alive_state), throw_during_copy(false), throw_during_move(false)
  {
  }

  ExceptionThrower(const ExceptionThrower &other)
      : state(is_alive_state), throw_during_copy(false), throw_during_move(false)
  {
    EXPECT_EQ(other.state, is_alive_state);
    if (other.throw_during_copy) {
      throw std::runtime_error("throwing during copy, as requested");
    }
  }

  ExceptionThrower(ExceptionThrower &&other)
      : state(is_alive_state), throw_during_copy(false), throw_during_move(false)
  {
    EXPECT_EQ(other.state, is_alive_state);
    if (other.throw_during_move) {
      throw std::runtime_error("throwing during move, as requested");
    }
  }

  ExceptionThrower &operator=(const ExceptionThrower &other)
  {
    EXPECT_EQ(other.state, is_alive_state);
    if (throw_during_copy || other.throw_during_copy) {
      throw std::runtime_error("throwing during copy, as requested");
    }
    return *this;
  }

  ExceptionThrower &operator=(ExceptionThrower &&other)
  {
    EXPECT_EQ(other.state, is_alive_state);
    if (throw_during_move || other.throw_during_move) {
      throw std::runtime_error("throwing during move, as requested");
    }
    return *this;
  }

  ~ExceptionThrower()
  {
    EXPECT_EQ(state, is_alive_state);
    state = is_destructed_state;
  }
};

}  // namespace blender::tests
