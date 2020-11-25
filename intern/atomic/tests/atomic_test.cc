/* Apache License, Version 2.0 */

#include <limits>

#include "atomic_ops.h"
#include "testing/testing.h"

#ifdef __GNUC__
#  if (__GNUC__ * 100 + __GNUC_MINOR__) >= 406 /* gcc4.6+ only */
#    pragma GCC diagnostic error "-Wsign-compare"
#  endif
#  if (__GNUC__ * 100 + __GNUC_MINOR__) >= 408
#    pragma GCC diagnostic error "-Wsign-conversion"
#  endif
#endif

/* -------------------------------------------------------------------- */
/** \name 64 bit unsigned int atomics
 * \{ */

TEST(atomic, atomic_add_and_fetch_uint64)
{
  {
    uint64_t value = 1;
    EXPECT_EQ(atomic_add_and_fetch_uint64(&value, 2), 3);
    EXPECT_EQ(value, 3);
  }

  {
    uint64_t value = 0x1020304050607080;
    EXPECT_EQ(atomic_add_and_fetch_uint64(&value, 0x0807060504030201), 0x1827364554637281);
    EXPECT_EQ(value, 0x1827364554637281);
  }

  {
    uint64_t value = 0x9020304050607080;
    EXPECT_EQ(atomic_add_and_fetch_uint64(&value, 0x0807060504030201), 0x9827364554637281);
    EXPECT_EQ(value, 0x9827364554637281);
  }
}

TEST(atomic, atomic_sub_and_fetch_uint64)
{
  {
    uint64_t value = 3;
    EXPECT_EQ(atomic_sub_and_fetch_uint64(&value, 2), 1);
    EXPECT_EQ(value, 1);
  }

  {
    uint64_t value = 0x1827364554637281;
    EXPECT_EQ(atomic_sub_and_fetch_uint64(&value, 0x0807060504030201), 0x1020304050607080);
    EXPECT_EQ(value, 0x1020304050607080);
  }

  {
    uint64_t value = 0x9827364554637281;
    EXPECT_EQ(atomic_sub_and_fetch_uint64(&value, 0x0807060504030201), 0x9020304050607080);
    EXPECT_EQ(value, 0x9020304050607080);
  }

  {
    uint64_t value = 1;
    EXPECT_EQ(atomic_sub_and_fetch_uint64(&value, 2), 0xffffffffffffffff);
    EXPECT_EQ(value, 0xffffffffffffffff);
  }
}

TEST(atomic, atomic_fetch_and_add_uint64)
{
  {
    uint64_t value = 1;
    EXPECT_EQ(atomic_fetch_and_add_uint64(&value, 2), 1);
    EXPECT_EQ(value, 3);
  }

  {
    uint64_t value = 0x1020304050607080;
    EXPECT_EQ(atomic_fetch_and_add_uint64(&value, 0x0807060504030201), 0x1020304050607080);
    EXPECT_EQ(value, 0x1827364554637281);
  }

  {
    uint64_t value = 0x9020304050607080;
    EXPECT_EQ(atomic_fetch_and_add_uint64(&value, 0x0807060504030201), 0x9020304050607080);
    EXPECT_EQ(value, 0x9827364554637281);
  }
}

TEST(atomic, atomic_fetch_and_sub_uint64)
{
  {
    uint64_t value = 3;
    EXPECT_EQ(atomic_fetch_and_sub_uint64(&value, 2), 3);
    EXPECT_EQ(value, 1);
  }

  {
    uint64_t value = 0x1827364554637281;
    EXPECT_EQ(atomic_fetch_and_sub_uint64(&value, 0x0807060504030201), 0x1827364554637281);
    EXPECT_EQ(value, 0x1020304050607080);
  }

  {
    uint64_t value = 0x9827364554637281;
    EXPECT_EQ(atomic_fetch_and_sub_uint64(&value, 0x0807060504030201), 0x9827364554637281);
    EXPECT_EQ(value, 0x9020304050607080);
  }

  {
    uint64_t value = 1;
    EXPECT_EQ(atomic_fetch_and_sub_uint64(&value, 2), 1);
    EXPECT_EQ(value, 0xffffffffffffffff);
  }
}

TEST(atomic, atomic_cas_uint64)
{
  {
    uint64_t value = 1;
    EXPECT_EQ(atomic_cas_uint64(&value, 1, 2), 1);
    EXPECT_EQ(value, 2);
  }

  {
    uint64_t value = 1;
    EXPECT_EQ(atomic_cas_uint64(&value, 2, 3), 1);
    EXPECT_EQ(value, 1);
  }

  {
    uint64_t value = 0x1234567890abcdef;
    EXPECT_EQ(atomic_cas_uint64(&value, 0x1234567890abcdef, 0xfedcba0987654321),
              0x1234567890abcdef);
    EXPECT_EQ(value, 0xfedcba0987654321);
  }

  {
    uint64_t value = 0x1234567890abcdef;
    EXPECT_EQ(atomic_cas_uint64(&value, 0xdeadbeefefefefef, 0xfedcba0987654321),
              0x1234567890abcdef);
    EXPECT_EQ(value, 0x1234567890abcdef);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name 64 bit signed int atomics
 * \{ */

TEST(atomic, atomic_add_and_fetch_int64)
{
  {
    int64_t value = 1;
    EXPECT_EQ(atomic_add_and_fetch_int64(&value, 2), 3);
    EXPECT_EQ(value, 3);
  }

  {
    int64_t value = 0x1020304050607080;
    EXPECT_EQ(atomic_add_and_fetch_int64(&value, 0x0807060504030201), 0x1827364554637281);
    EXPECT_EQ(value, 0x1827364554637281);
  }

  {
    int64_t value = -0x1020304050607080;
    EXPECT_EQ(atomic_add_and_fetch_int64(&value, -0x0807060504030201), -0x1827364554637281);
    EXPECT_EQ(value, -0x1827364554637281);
  }
}

TEST(atomic, atomic_sub_and_fetch_int64)
{
  {
    int64_t value = 3;
    EXPECT_EQ(atomic_sub_and_fetch_int64(&value, 2), 1);
    EXPECT_EQ(value, 1);
  }

  {
    int64_t value = 0x1827364554637281;
    EXPECT_EQ(atomic_sub_and_fetch_int64(&value, 0x0807060504030201), 0x1020304050607080);
    EXPECT_EQ(value, 0x1020304050607080);
  }

  {
    int64_t value = -0x1827364554637281;
    EXPECT_EQ(atomic_sub_and_fetch_int64(&value, -0x0807060504030201), -0x1020304050607080);
    EXPECT_EQ(value, -0x1020304050607080);
  }

  {
    int64_t value = 1;
    EXPECT_EQ(atomic_sub_and_fetch_int64(&value, 2), -1);
    EXPECT_EQ(value, -1);
  }
}

TEST(atomic, atomic_fetch_and_add_int64)
{
  {
    int64_t value = 1;
    EXPECT_EQ(atomic_fetch_and_add_int64(&value, 2), 1);
    EXPECT_EQ(value, 3);
  }

  {
    int64_t value = 0x1020304050607080;
    EXPECT_EQ(atomic_fetch_and_add_int64(&value, 0x0807060504030201), 0x1020304050607080);
    EXPECT_EQ(value, 0x1827364554637281);
  }

  {
    int64_t value = -0x1020304050607080;
    EXPECT_EQ(atomic_fetch_and_add_int64(&value, -0x0807060504030201), -0x1020304050607080);
    EXPECT_EQ(value, -0x1827364554637281);
  }
}

TEST(atomic, atomic_fetch_and_sub_int64)
{
  {
    int64_t value = 3;
    EXPECT_EQ(atomic_fetch_and_sub_int64(&value, 2), 3);
    EXPECT_EQ(value, 1);
  }

  {
    int64_t value = 0x1827364554637281;
    EXPECT_EQ(atomic_fetch_and_sub_int64(&value, 0x0807060504030201), 0x1827364554637281);
    EXPECT_EQ(value, 0x1020304050607080);
  }

  {
    int64_t value = -0x1827364554637281;
    EXPECT_EQ(atomic_fetch_and_sub_int64(&value, -0x0807060504030201), -0x1827364554637281);
    EXPECT_EQ(value, -0x1020304050607080);
  }

  {
    int64_t value = 1;
    EXPECT_EQ(atomic_fetch_and_sub_int64(&value, 2), 1);
    EXPECT_EQ(value, -1);
  }
}

TEST(atomic, atomic_cas_int64)
{
  {
    int64_t value = 1;
    EXPECT_EQ(atomic_cas_int64(&value, 1, 2), 1);
    EXPECT_EQ(value, 2);
  }

  {
    int64_t value = 1;
    EXPECT_EQ(atomic_cas_int64(&value, 2, 3), 1);
    EXPECT_EQ(value, 1);
  }

  // 0xfedcba0987654321 is -0x012345f6789abcdf
  // 0xdeadbeefefefefef is -0x2152411010101011

  {
    int64_t value = 0x1234567890abcdef;
    EXPECT_EQ(atomic_cas_int64(&value, 0x1234567890abcdef, -0x012345f6789abcdf),
              0x1234567890abcdef);
    EXPECT_EQ(value, -0x012345f6789abcdf);
  }

  {
    int64_t value = 0x1234567890abcdef;
    EXPECT_EQ(atomic_cas_int64(&value, 0x2152411010101011, -0x012345f6789abcdf),
              0x1234567890abcdef);
    EXPECT_EQ(value, 0x1234567890abcdef);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name 32 bit unsigned int atomics
 * \{ */

TEST(atomic, atomic_add_and_fetch_uint32)
{
  {
    uint32_t value = 1;
    EXPECT_EQ(atomic_add_and_fetch_uint32(&value, 2), 3);
    EXPECT_EQ(value, 3);
  }

  {
    uint32_t value = 0x10203040;
    EXPECT_EQ(atomic_add_and_fetch_uint32(&value, 0x04030201), 0x14233241);
    EXPECT_EQ(value, 0x14233241);
  }

  {
    uint32_t value = 0x90203040;
    EXPECT_EQ(atomic_add_and_fetch_uint32(&value, 0x04030201), 0x94233241);
    EXPECT_EQ(value, 0x94233241);
  }
}

TEST(atomic, atomic_sub_and_fetch_uint32)
{
  {
    uint32_t value = 3;
    EXPECT_EQ(atomic_sub_and_fetch_uint32(&value, 2), 1);
    EXPECT_EQ(value, 1);
  }

  {
    uint32_t value = 0x14233241;
    EXPECT_EQ(atomic_sub_and_fetch_uint32(&value, 0x04030201), 0x10203040);
    EXPECT_EQ(value, 0x10203040);
  }

  {
    uint32_t value = 0x94233241;
    EXPECT_EQ(atomic_sub_and_fetch_uint32(&value, 0x04030201), 0x90203040);
    EXPECT_EQ(value, 0x90203040);
  }

  {
    uint32_t value = 1;
    EXPECT_EQ(atomic_sub_and_fetch_uint32(&value, 2), 0xffffffff);
    EXPECT_EQ(value, 0xffffffff);
  }
}

TEST(atomic, atomic_cas_uint32)
{
  {
    uint32_t value = 1;
    EXPECT_EQ(atomic_cas_uint32(&value, 1, 2), 1);
    EXPECT_EQ(value, 2);
  }

  {
    uint32_t value = 1;
    EXPECT_EQ(atomic_cas_uint32(&value, 2, 3), 1);
    EXPECT_EQ(value, 1);
  }

  {
    uint32_t value = 0x12345678;
    EXPECT_EQ(atomic_cas_uint32(&value, 0x12345678, 0x87654321), 0x12345678);
    EXPECT_EQ(value, 0x87654321);
  }

  {
    uint32_t value = 0x12345678;
    EXPECT_EQ(atomic_cas_uint32(&value, 0xdeadbeef, 0x87654321), 0x12345678);
    EXPECT_EQ(value, 0x12345678);
  }
}

TEST(atomic, atomic_fetch_and_add_uint32)
{
  {
    uint32_t value = 1;
    EXPECT_EQ(atomic_fetch_and_add_uint32(&value, 2), 1);
    EXPECT_EQ(value, 3);
  }

  {
    uint32_t value = 0x10203040;
    EXPECT_EQ(atomic_fetch_and_add_uint32(&value, 0x04030201), 0x10203040);
    EXPECT_EQ(value, 0x14233241);
  }

  {
    uint32_t value = 0x90203040;
    EXPECT_EQ(atomic_fetch_and_add_uint32(&value, 0x04030201), 0x90203040);
    EXPECT_EQ(value, 0x94233241);
  }
}

TEST(atomic, atomic_fetch_and_or_uint32)
{
  {
    uint32_t value = 12;
    EXPECT_EQ(atomic_fetch_and_or_uint32(&value, 5), 12);
    EXPECT_EQ(value, 13);
  }

  {
    uint32_t value = 0x12345678;
    EXPECT_EQ(atomic_fetch_and_or_uint32(&value, 0x87654321), 0x12345678);
    EXPECT_EQ(value, 0x97755779);
  }

  {
    uint32_t value = 0x92345678;
    EXPECT_EQ(atomic_fetch_and_or_uint32(&value, 0x87654321), 0x92345678);
    EXPECT_EQ(value, 0x97755779);
  }
}

TEST(atomic, atomic_fetch_and_and_uint32)
{
  {
    uint32_t value = 12;
    EXPECT_EQ(atomic_fetch_and_and_uint32(&value, 5), 12);
    EXPECT_EQ(value, 4);
  }

  {
    uint32_t value = 0x12345678;
    EXPECT_EQ(atomic_fetch_and_and_uint32(&value, 0x87654321), 0x12345678);
    EXPECT_EQ(value, 0x02244220);
  }

  {
    uint32_t value = 0x92345678;
    EXPECT_EQ(atomic_fetch_and_and_uint32(&value, 0x87654321), 0x92345678);
    EXPECT_EQ(value, 0x82244220);
  }
}

/** \} */

/** \name 32 bit signed int atomics
 * \{ */

TEST(atomic, atomic_add_and_fetch_int32)
{
  {
    int32_t value = 1;
    EXPECT_EQ(atomic_add_and_fetch_int32(&value, 2), 3);
    EXPECT_EQ(value, 3);
  }

  {
    int32_t value = 0x10203040;
    EXPECT_EQ(atomic_add_and_fetch_int32(&value, 0x04030201), 0x14233241);
    EXPECT_EQ(value, 0x14233241);
  }

  {
    int32_t value = -0x10203040;
    EXPECT_EQ(atomic_add_and_fetch_int32(&value, -0x04030201), -0x14233241);
    EXPECT_EQ(value, -0x14233241);
  }
}

TEST(atomic, atomic_sub_and_fetch_int32)
{
  {
    int32_t value = 3;
    EXPECT_EQ(atomic_sub_and_fetch_int32(&value, 2), 1);
    EXPECT_EQ(value, 1);
  }

  {
    int32_t value = 0x14233241;
    EXPECT_EQ(atomic_sub_and_fetch_int32(&value, 0x04030201), 0x10203040);
    EXPECT_EQ(value, 0x10203040);
  }

  {
    int32_t value = -0x14233241;
    EXPECT_EQ(atomic_sub_and_fetch_int32(&value, -0x04030201), -0x10203040);
    EXPECT_EQ(value, -0x10203040);
  }

  {
    int32_t value = 1;
    EXPECT_EQ(atomic_sub_and_fetch_int32(&value, 2), 0xffffffff);
    EXPECT_EQ(value, 0xffffffff);
  }
}

TEST(atomic, atomic_cas_int32)
{
  {
    int32_t value = 1;
    EXPECT_EQ(atomic_cas_int32(&value, 1, 2), 1);
    EXPECT_EQ(value, 2);
  }

  {
    int32_t value = 1;
    EXPECT_EQ(atomic_cas_int32(&value, 2, 3), 1);
    EXPECT_EQ(value, 1);
  }

  // 0x87654321 is -0x789abcdf
  // 0xdeadbeef is -0x21524111

  {
    int32_t value = 0x12345678;
    EXPECT_EQ(atomic_cas_int32(&value, 0x12345678, -0x789abcdf), 0x12345678);
    EXPECT_EQ(value, -0x789abcdf);
  }

  {
    int32_t value = 0x12345678;
    EXPECT_EQ(atomic_cas_int32(&value, -0x21524111, -0x789abcdf), 0x12345678);
    EXPECT_EQ(value, 0x12345678);
  }
}

TEST(atomic, atomic_fetch_and_add_int32)
{
  {
    int32_t value = 1;
    EXPECT_EQ(atomic_fetch_and_add_int32(&value, 2), 1);
    EXPECT_EQ(value, 3);
  }

  {
    int32_t value = 0x10203040;
    EXPECT_EQ(atomic_fetch_and_add_int32(&value, 0x04030201), 0x10203040);
    EXPECT_EQ(value, 0x14233241);
  }

  {
    int32_t value = -0x10203040;
    EXPECT_EQ(atomic_fetch_and_add_int32(&value, -0x04030201), -0x10203040);
    EXPECT_EQ(value, -0x14233241);
  }
}

TEST(atomic, atomic_fetch_and_or_int32)
{
  {
    int32_t value = 12;
    EXPECT_EQ(atomic_fetch_and_or_int32(&value, 5), 12);
    EXPECT_EQ(value, 13);
  }

  // 0x87654321 is -0x789abcdf

  {
    int32_t value = 0x12345678;
    EXPECT_EQ(atomic_fetch_and_or_int32(&value, -0x789abcdf), 0x12345678);
    EXPECT_EQ(value, 0x97755779);
  }
}

TEST(atomic, atomic_fetch_and_and_int32)
{
  {
    int32_t value = 12;
    EXPECT_EQ(atomic_fetch_and_and_int32(&value, 5), 12);
    EXPECT_EQ(value, 4);
  }

  {
    int32_t value = 0x12345678;
    EXPECT_EQ(atomic_fetch_and_and_int32(&value, -0x789abcdf), 0x12345678);
    EXPECT_EQ(value, 0x02244220);
  }
}

/** \} */

/** \name 8 bit unsigned int atomics
 * \{ */

TEST(atomic, atomic_fetch_and_or_uint8)
{
  {
    uint8_t value = 12;
    EXPECT_EQ(atomic_fetch_and_or_uint8(&value, 5), 12);
    EXPECT_EQ(value, 13);
  }
}

TEST(atomic, atomic_fetch_and_and_uint8)
{
  {
    uint8_t value = 12;
    EXPECT_EQ(atomic_fetch_and_and_uint8(&value, 5), 12);
    EXPECT_EQ(value, 4);
  }
}

/** \} */

/** \name 8 bit signed int atomics
 * \{ */

TEST(atomic, atomic_fetch_and_or_int8)
{
  {
    int8_t value = 12;
    EXPECT_EQ(atomic_fetch_and_or_int8(&value, 5), 12);
    EXPECT_EQ(value, 13);
  }
}

TEST(atomic, atomic_fetch_and_and_int8)
{
  {
    int8_t value = 12;
    EXPECT_EQ(atomic_fetch_and_and_int8(&value, 5), 12);
    EXPECT_EQ(value, 4);
  }
}

/** \} */

/** \name char aliases
 * \{ */

TEST(atomic, atomic_fetch_and_or_char)
{
  {
    char value = 12;
    EXPECT_EQ(atomic_fetch_and_or_char(&value, 5), 12);
    EXPECT_EQ(value, 13);
  }
}

TEST(atomic, atomic_fetch_and_and_char)
{
  {
    char value = 12;
    EXPECT_EQ(atomic_fetch_and_and_char(&value, 5), 12);
    EXPECT_EQ(value, 4);
  }
}

/** \} */

/** \name size_t aliases
 * \{ */

TEST(atomic, atomic_add_and_fetch_z)
{
  /* Make sure alias is implemented. */
  {
    size_t value = 1;
    EXPECT_EQ(atomic_add_and_fetch_z(&value, 2), 3);
    EXPECT_EQ(value, 3);
  }

  /* Make sure alias is using proper bitness. */
  {
    const size_t size_t_max = std::numeric_limits<size_t>::max();
    size_t value = size_t_max - 10;
    EXPECT_EQ(atomic_add_and_fetch_z(&value, 2), size_t_max - 8);
    EXPECT_EQ(value, size_t_max - 8);
  }
}

TEST(atomic, atomic_sub_and_fetch_z)
{
  /* Make sure alias is implemented. */
  {
    size_t value = 3;
    EXPECT_EQ(atomic_sub_and_fetch_z(&value, 2), 1);
    EXPECT_EQ(value, 1);
  }

  /* Make sure alias is using proper bitness. */
  {
    const size_t size_t_max = std::numeric_limits<size_t>::max();
    size_t value = size_t_max - 10;
    EXPECT_EQ(atomic_sub_and_fetch_z(&value, 2), size_t_max - 12);
    EXPECT_EQ(value, size_t_max - 12);
  }
}

TEST(atomic, atomic_fetch_and_add_z)
{
  /* Make sure alias is implemented. */
  {
    size_t value = 1;
    EXPECT_EQ(atomic_fetch_and_add_z(&value, 2), 1);
    EXPECT_EQ(value, 3);
  }

  /* Make sure alias is using proper bitness. */
  {
    const size_t size_t_max = std::numeric_limits<size_t>::max();
    size_t value = size_t_max - 10;
    EXPECT_EQ(atomic_fetch_and_add_z(&value, 2), size_t_max - 10);
    EXPECT_EQ(value, size_t_max - 8);
  }
}

TEST(atomic, atomic_fetch_and_sub_z)
{
  /* Make sure alias is implemented. */
  {
    size_t value = 3;
    EXPECT_EQ(atomic_fetch_and_sub_z(&value, 2), 3);
    EXPECT_EQ(value, 1);
  }

  /* Make sure alias is using proper bitness. */
  {
    const size_t size_t_max = std::numeric_limits<size_t>::max();
    size_t value = size_t_max - 10;
    EXPECT_EQ(atomic_fetch_and_sub_z(&value, 2), size_t_max - 10);
    EXPECT_EQ(value, size_t_max - 12);
  }
}

TEST(atomic, atomic_cas_z)
{
  /* Make sure alias is implemented. */
  {
    size_t value = 1;
    EXPECT_EQ(atomic_cas_z(&value, 1, 2), 1);
    EXPECT_EQ(value, 2);
  }

  /* Make sure alias is using proper bitness. */
  {
    const size_t size_t_max = std::numeric_limits<size_t>::max();
    size_t value = 1;
    EXPECT_EQ(atomic_cas_z(&value, 1, size_t_max), 1);
    EXPECT_EQ(value, size_t_max);
  }
}

TEST(atomic, atomic_fetch_and_update_max_z)
{
  const size_t size_t_max = std::numeric_limits<size_t>::max();

  size_t value = 12;

  EXPECT_EQ(atomic_fetch_and_update_max_z(&value, 8), 12);
  EXPECT_EQ(value, 12);

  EXPECT_EQ(atomic_fetch_and_update_max_z(&value, 24), 12);
  EXPECT_EQ(value, 24);

  EXPECT_EQ(atomic_fetch_and_update_max_z(&value, size_t_max), 24);
  EXPECT_EQ(value, size_t_max);
}

/** \} */

/** \name unsigned int aliases
 * \{ */

TEST(atomic, atomic_add_and_fetch_u)
{
  /* Make sure alias is implemented. */
  {
    unsigned int value = 1;
    EXPECT_EQ(atomic_add_and_fetch_u(&value, 2), 3);
    EXPECT_EQ(value, 3);
  }

  /* Make sure alias is using proper bitness. */
  {
    const unsigned int uint_max = std::numeric_limits<unsigned int>::max();
    unsigned int value = uint_max - 10;
    EXPECT_EQ(atomic_add_and_fetch_u(&value, 2), uint_max - 8);
    EXPECT_EQ(value, uint_max - 8);
  }
}

TEST(atomic, atomic_sub_and_fetch_u)
{
  /* Make sure alias is implemented. */
  {
    unsigned int value = 3;
    EXPECT_EQ(atomic_sub_and_fetch_u(&value, 2), 1);
    EXPECT_EQ(value, 1);
  }

  /* Make sure alias is using proper bitness. */
  {
    const unsigned int uint_max = std::numeric_limits<unsigned int>::max();
    unsigned int value = uint_max - 10;
    EXPECT_EQ(atomic_sub_and_fetch_u(&value, 2), uint_max - 12);
    EXPECT_EQ(value, uint_max - 12);
  }
}

TEST(atomic, atomic_fetch_and_add_u)
{
  /* Make sure alias is implemented. */
  {
    unsigned int value = 1;
    EXPECT_EQ(atomic_fetch_and_add_u(&value, 2), 1);
    EXPECT_EQ(value, 3);
  }

  /* Make sure alias is using proper bitness. */
  {
    const unsigned int uint_max = std::numeric_limits<unsigned int>::max();
    unsigned int value = uint_max - 10;
    EXPECT_EQ(atomic_fetch_and_add_u(&value, 2), uint_max - 10);
    EXPECT_EQ(value, uint_max - 8);
  }
}

TEST(atomic, atomic_fetch_and_sub_u)
{
  /* Make sure alias is implemented. */
  {
    unsigned int value = 3;
    EXPECT_EQ(atomic_fetch_and_sub_u(&value, 2), 3);
    EXPECT_EQ(value, 1);
  }

  /* Make sure alias is using proper bitness. */
  {
    const unsigned int uint_max = std::numeric_limits<unsigned int>::max();
    unsigned int value = uint_max - 10;
    EXPECT_EQ(atomic_fetch_and_sub_u(&value, 2), uint_max - 10);
    EXPECT_EQ(value, uint_max - 12);
  }
}

TEST(atomic, atomic_cas_u)
{
  /* Make sure alias is implemented. */
  {
    unsigned int value = 1;
    EXPECT_EQ(atomic_cas_u(&value, 1, 2), 1);
    EXPECT_EQ(value, 2);
  }

  /* Make sure alias is using proper bitness. */
  {
    const unsigned int uint_max = std::numeric_limits<unsigned int>::max();
    unsigned int value = 1;
    EXPECT_EQ(atomic_cas_u(&value, 1, uint_max), 1);
    EXPECT_EQ(value, uint_max);
  }
}

/** \} */

/** \name pointer aliases
 * \{ */

#define INT_AS_PTR(a) reinterpret_cast<void *>((a))

TEST(atomic, atomic_cas_ptr)
{
  {
    void *value = INT_AS_PTR(0x7f);
    EXPECT_EQ(atomic_cas_ptr(&value, INT_AS_PTR(0x7f), INT_AS_PTR(0xef)), INT_AS_PTR(0x7f));
    EXPECT_EQ(value, INT_AS_PTR(0xef));
  }
}

#undef INT_AS_PTR

/** \} */

/** \name floating point atomics
 * \{ */

TEST(atomic, atomic_cas_float)
{
  {
    float value = 1.234f;
    EXPECT_EQ(atomic_cas_float(&value, 1.234f, 2.71f), 1.234f);
    EXPECT_EQ(value, 2.71f);
  }
}

TEST(atomic, atomic_add_and_fetch_fl)
{
  {
    float value = 1.23f;
    EXPECT_NEAR(atomic_add_and_fetch_fl(&value, 2.71f), 3.94f, 1e-8f);
    EXPECT_NEAR(value, 3.94f, 1e-8f);
  }
}

/** \} */
