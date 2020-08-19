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
  bool throw_during_copy;
  bool throw_during_move;

  ExceptionThrower()
      : state_(is_alive_state),
        my_memory_(MEM_mallocN(1, AT)),
        throw_during_copy(false),
        throw_during_move(false)
  {
  }

  ExceptionThrower(const ExceptionThrower &other) : ExceptionThrower()
  {
    EXPECT_EQ(other.state_, is_alive_state);
    if (other.throw_during_copy) {
      throw std::runtime_error("throwing during copy, as requested");
    }
  }

  ExceptionThrower(ExceptionThrower &&other) : ExceptionThrower()
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
    return *this;
  }

  ExceptionThrower &operator=(ExceptionThrower &&other)
  {
    EXPECT_EQ(other.state_, is_alive_state);
    if (throw_during_move || other.throw_during_move) {
      throw std::runtime_error("throwing during move, as requested");
    }
    return *this;
  }

  ~ExceptionThrower()
  {
    EXPECT_EQ(state_, is_alive_state);
    state_ = is_destructed_state;
    MEM_freeN(my_memory_);
  }
};

}  // namespace blender::tests
