/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __BLENDER_TESTING_H__
#define __BLENDER_TESTING_H__

#include <vector>

#include <gflags/gflags.h>  // IWYU pragma: export
#include <glog/logging.h>   // IWYU pragma: export
#include <gtest/gtest.h>    // IWYU pragma: export

/* Fwd.
 */
namespace blender {

template<typename T> class Span;

}  // namespace blender

namespace blender::tests {

/* These strings are passed on the CLI with the --test-asset-dir and --test-release-dir arguments.
 * The arguments are added automatically when invoking tests via `ctest`. */
const std::string &flags_test_asset_dir();   /* tests/files in the Blender repository. */
const std::string &flags_test_release_dir(); /* bin/{blender version} in the build directory. */

/* Returns true if the `BLENDER_TEST_IGNORE_BLOCKLIST` environment variable is set. */
bool should_ignore_blocklist();

}  // namespace blender::tests

#define EXPECT_V2_NEAR(a, b, eps) \
  { \
    EXPECT_NEAR(a[0], b[0], eps); \
    EXPECT_NEAR(a[1], b[1], eps); \
  } \
  (void)0

#define EXPECT_V3_NEAR(a, b, eps) \
  { \
    EXPECT_NEAR(a[0], b[0], eps); \
    EXPECT_NEAR(a[1], b[1], eps); \
    EXPECT_NEAR(a[2], b[2], eps); \
  } \
  (void)0

#define EXPECT_V4_NEAR(a, b, eps) \
  { \
    EXPECT_NEAR(a[0], b[0], eps); \
    EXPECT_NEAR(a[1], b[1], eps); \
    EXPECT_NEAR(a[2], b[2], eps); \
    EXPECT_NEAR(a[3], b[3], eps); \
  } \
  (void)0

#define EXPECT_M2_NEAR(a, b, eps) \
  do { \
    EXPECT_V2_NEAR(a[0], b[0], eps); \
    EXPECT_V2_NEAR(a[1], b[1], eps); \
  } while (false);

#define EXPECT_M3_NEAR(a, b, eps) \
  do { \
    EXPECT_V3_NEAR(a[0], b[0], eps); \
    EXPECT_V3_NEAR(a[1], b[1], eps); \
    EXPECT_V3_NEAR(a[2], b[2], eps); \
  } while (false);

#define EXPECT_M4_NEAR(a, b, eps) \
  do { \
    EXPECT_V4_NEAR(a[0], b[0], eps); \
    EXPECT_V4_NEAR(a[1], b[1], eps); \
    EXPECT_V4_NEAR(a[2], b[2], eps); \
    EXPECT_V4_NEAR(a[3], b[3], eps); \
  } while (false);

#define EXPECT_MATRIX_NEAR(a, b, tolerance) \
  do { \
    bool dims_match = (a.rows() == b.rows()) && (a.cols() == b.cols()); \
    EXPECT_EQ(a.rows(), b.rows()) << "Matrix rows don't match."; \
    EXPECT_EQ(a.cols(), b.cols()) << "Matrix cols don't match."; \
    if (dims_match) { \
      for (int r = 0; r < a.rows(); ++r) { \
        for (int c = 0; c < a.cols(); ++c) { \
          EXPECT_NEAR(a(r, c), b(r, c), tolerance) << "r=" << r << ", c=" << c << "."; \
        } \
      } \
    } \
  } while (false);

#define EXPECT_MATRIX_NEAR_ZERO(a, tolerance) \
  do { \
    for (int r = 0; r < a.rows(); ++r) { \
      for (int c = 0; c < a.cols(); ++c) { \
        EXPECT_NEAR(0.0, a(r, c), tolerance) << "r=" << r << ", c=" << c << "."; \
      } \
    } \
  } while (false);

#define EXPECT_MATRIX_EQ(a, b) \
  do { \
    bool dims_match = (a.rows() == b.rows()) && (a.cols() == b.cols()); \
    EXPECT_EQ(a.rows(), b.rows()) << "Matrix rows don't match."; \
    EXPECT_EQ(a.cols(), b.cols()) << "Matrix cols don't match."; \
    if (dims_match) { \
      for (int r = 0; r < a.rows(); ++r) { \
        for (int c = 0; c < a.cols(); ++c) { \
          EXPECT_EQ(a(r, c), b(r, c)) << "r=" << r << ", c=" << c << "."; \
        } \
      } \
    } \
  } while (false);

// Check that sin(angle(a, b)) < tolerance.
#define EXPECT_MATRIX_PROP(a, b, tolerance) \
  do { \
    bool dims_match = (a.rows() == b.rows()) && (a.cols() == b.cols()); \
    EXPECT_EQ(a.rows(), b.rows()) << "Matrix rows don't match."; \
    EXPECT_EQ(a.cols(), b.cols()) << "Matrix cols don't match."; \
    if (dims_match) { \
      double c = CosinusBetweenMatrices(a, b); \
      if (c * c < 1) { \
        double s = sqrt(1 - c * c); \
        EXPECT_NEAR(0, s, tolerance); \
      } \
    } \
  } while (false);

#ifdef LIBMV_NUMERIC_NUMERIC_H
template<class TMat> double CosinusBetweenMatrices(const TMat &a, const TMat &b)
{
  return (a.array() * b.array()).sum() / libmv::FrobeniusNorm(a) / libmv::FrobeniusNorm(b);
}
#endif

template<typename T>
inline void EXPECT_EQ_VECTOR(const std::vector<T> &expected, const std::vector<T> &actual)
{
  EXPECT_EQ(expected.size(), actual.size());
  if (expected.size() == actual.size()) {
    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i], actual[i]) << "Element mismatch at index " << i;
    }
  }
}

template<typename T>
inline void EXPECT_EQ_SPAN(const blender::Span<T> expected, const blender::Span<T> actual)
{
  EXPECT_EQ(expected.size(), actual.size());
  if (expected.size() == actual.size()) {
    for (const int64_t i : expected.index_range()) {
      EXPECT_EQ(expected[i], actual[i]) << "Element mismatch at index " << i;
    }
  }
}

template<typename T, typename U>
inline void EXPECT_NEAR_SPAN(const blender::Span<T> expected,
                             const blender::Span<T> actual,
                             const U tolerance)
{
  EXPECT_EQ(expected.size(), actual.size());
  if (expected.size() == actual.size()) {
    for (const int64_t i : expected.index_range()) {
      EXPECT_NEAR(expected[i], actual[i], tolerance) << "Element mismatch at index " << i;
    }
  }
}

template<typename T>
inline void EXPECT_EQ_ARRAY(const T *expected, const T *actual, const size_t N)
{
  for (size_t i = 0; i < N; ++i) {
    EXPECT_EQ(expected[i], actual[i]) << "Element mismatch at index " << i;
  }
}

template<typename T>
inline void EXPECT_EQ_ARRAY_ND(const T *expected, const T *actual, const size_t N, const size_t D)
{
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < D; ++j) {
      EXPECT_EQ(expected[i][j], actual[i][j])
          << "Element mismatch at index " << i << ", component index " << j;
    }
  }
}

template<typename T, typename U>
inline void EXPECT_NEAR_ARRAY_ND(
    const T *expected, const T *actual, const size_t N, const size_t D, const U tolerance)
{
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < D; ++j) {
      EXPECT_NEAR(expected[i][j], actual[i][j], tolerance)
          << "Element mismatch at index " << i << ", component index " << j;
    }
  }
}

#ifdef _WIN32
#  define ABORT_PREDICATE ::testing::ExitedWithCode(3)
#else
#  define ABORT_PREDICATE ::testing::KilledBySignal(SIGABRT)
#endif

/* Test macro for when BLI_assert() is expected to fail.
 * Note that the EXPECT_BLI_ASSERT macro is a no-op, unless used in a debug build with
 * WITH_ASSERT_ABORT=ON. */
#if defined(WITH_ASSERT_ABORT) && !defined(NDEBUG)
/* EXPECT_EXIT() is used as that's the only exit-expecting function in GTest that allows us to
 * check for SIGABRT. */
#  define EXPECT_BLI_ASSERT(function_call, expect_message) \
    EXPECT_EXIT(function_call, ABORT_PREDICATE, expect_message)
#else
#  define EXPECT_BLI_ASSERT(function_call, expect_message) function_call
#endif

#endif  // __BLENDER_TESTING_H__
