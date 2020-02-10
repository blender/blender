#pragma once

namespace BLI {

class NonCopyable {
 public:
  /* Disable copy construction and assignment. */
  NonCopyable(const NonCopyable &other) = delete;
  NonCopyable &operator=(const NonCopyable &other) = delete;

  /* Explicitly enable default construction, move construction and move assignment. */
  NonCopyable() = default;
  NonCopyable(NonCopyable &&other) = default;
  NonCopyable &operator=(NonCopyable &&other) = default;
};

class NonMovable {
 public:
  /* Disable move construction and assignment. */
  NonMovable(NonMovable &&other) = delete;
  NonMovable &operator=(NonMovable &&other) = delete;

  /* Explicitly enable default construction, copy construction and copy assignment. */
  NonMovable() = default;
  NonMovable(const NonMovable &other) = default;
  NonMovable &operator=(const NonMovable &other) = default;
};

}  // namespace BLI
