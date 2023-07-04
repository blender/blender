/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_memiter.h"
#include "BLI_string_utils.h"

#include "BLI_ressource_strings.h"
#include "BLI_string.h"

TEST(memiter, Nop)
{
  BLI_memiter *mi = BLI_memiter_create(64);
  BLI_memiter_destroy(mi);
}

static void memiter_empty_test(int elems_num, const int chunk_size)
{
  BLI_memiter *mi = BLI_memiter_create(chunk_size);
  void *data;
  for (int index = 0; index < elems_num; index++) {
    data = BLI_memiter_alloc(mi, 0);
  }
  int index = 0, total_size = 0;
  BLI_memiter_handle it;
  BLI_memiter_iter_init(mi, &it);
  uint elem_size;
  while ((data = BLI_memiter_iter_step_size(&it, &elem_size))) {
    index += 1;
    total_size += elem_size;
  }
  EXPECT_EQ(0, total_size);
  EXPECT_EQ(elems_num, index);

  BLI_memiter_destroy(mi);
}

#define MEMITER_NUMBER_TEST_FN(fn, number_type) \
  static void fn(int elems_num, const int chunk_size) \
  { \
    BLI_memiter *mi = BLI_memiter_create(chunk_size); \
    number_type *data; \
    for (int index = 0; index < elems_num; index++) { \
      data = (number_type *)BLI_memiter_alloc(mi, sizeof(number_type)); \
      *data = index; \
    } \
    BLI_memiter_handle it; \
    BLI_memiter_iter_init(mi, &it); \
    uint elem_size; \
    int index = 0; \
    while ((data = (number_type *)BLI_memiter_iter_step_size(&it, &elem_size))) { \
      EXPECT_EQ(sizeof(number_type), elem_size); \
      EXPECT_EQ(index, *data); \
      index += 1; \
    } \
    BLI_memiter_destroy(mi); \
  }

/* generate number functions */
MEMITER_NUMBER_TEST_FN(memiter_char_test, char)
MEMITER_NUMBER_TEST_FN(memiter_short_test, short)
MEMITER_NUMBER_TEST_FN(memiter_int_test, int)
MEMITER_NUMBER_TEST_FN(memiter_long_test, int64_t)

static void memiter_string_test(const char *strings[], const int chunk_size)
{
  BLI_memiter *mi = BLI_memiter_create(chunk_size);
  char *data;
  int index = 0;
  int total_size_expect = 0;
  while (strings[index]) {
    const int size = strlen(strings[index]) + 1;
    BLI_memiter_alloc_from(mi, size, strings[index]);
    total_size_expect += size;
    index += 1;
  }
  const int strings_len = index;
  int total_size = 0;
  BLI_memiter_handle it;
  BLI_memiter_iter_init(mi, &it);
  uint elem_size;
  index = 0;
  while ((data = (char *)BLI_memiter_iter_step_size(&it, &elem_size))) {
    EXPECT_EQ(strlen(strings[index]) + 1, elem_size);
    EXPECT_STREQ(strings[index], data);
    total_size += elem_size;
    index += 1;
  }
  EXPECT_EQ(total_size_expect, total_size);
  EXPECT_EQ(strings_len, index);

  BLI_memiter_destroy(mi);
}

static void memiter_words10k_test(const char split_char, const int chunk_size)
{
  const int words_len = sizeof(words10k) - 1;
  char *words = BLI_strdupn(words10k, words_len);
  BLI_string_replace_char(words, split_char, '\0');

  BLI_memiter *mi = BLI_memiter_create(chunk_size);

  char *data;
  int index;
  char *c_end, *c;
  c_end = words + words_len;
  c = words;
  index = 0;
  while (c < c_end) {
    int elem_size = strlen(c) + 1;
    data = (char *)BLI_memiter_alloc(mi, elem_size);
    memcpy(data, c, elem_size);
    c += elem_size;
    index += 1;
  }
  const int len_expect = index;
  c = words;
  uint size;
  BLI_memiter_handle it;
  BLI_memiter_iter_init(mi, &it);
  index = 0;
  while ((data = (char *)BLI_memiter_iter_step_size(&it, &size))) {
    int size_expect = strlen(c) + 1;
    EXPECT_EQ(size_expect, size);
    EXPECT_STREQ(c, data);
    c += size;
    index += 1;
  }
  EXPECT_EQ(len_expect, index);
  BLI_memiter_destroy(mi);
  MEM_freeN(words);
}

#define TEST_EMPTY_AT_CHUNK_SIZE(chunk_size) \
  TEST(memiter, Empty0_##chunk_size) \
  { \
    memiter_empty_test(0, chunk_size); \
  } \
  TEST(memiter, Empty1_##chunk_size) \
  { \
    memiter_empty_test(1, chunk_size); \
  } \
  TEST(memiter, Empty2_##chunk_size) \
  { \
    memiter_empty_test(2, chunk_size); \
  } \
  TEST(memiter, Empty3_##chunk_size) \
  { \
    memiter_empty_test(3, chunk_size); \
  } \
  TEST(memiter, Empty13_##chunk_size) \
  { \
    memiter_empty_test(13, chunk_size); \
  } \
  TEST(memiter, Empty256_##chunk_size) \
  { \
    memiter_empty_test(256, chunk_size); \
  }

TEST_EMPTY_AT_CHUNK_SIZE(1)
TEST_EMPTY_AT_CHUNK_SIZE(2)
TEST_EMPTY_AT_CHUNK_SIZE(3)
TEST_EMPTY_AT_CHUNK_SIZE(13)
TEST_EMPTY_AT_CHUNK_SIZE(256)

#define TEST_NUMBER_AT_CHUNK_SIZE(chunk_size) \
  TEST(memiter, Char1_##chunk_size) \
  { \
    memiter_char_test(1, chunk_size); \
  } \
  TEST(memiter, Short1_##chunk_size) \
  { \
    memiter_short_test(1, chunk_size); \
  } \
  TEST(memiter, Int1_##chunk_size) \
  { \
    memiter_int_test(1, chunk_size); \
  } \
  TEST(memiter, Long1_##chunk_size) \
  { \
    memiter_long_test(1, chunk_size); \
  } \
\
  TEST(memiter, Char2_##chunk_size) \
  { \
    memiter_char_test(2, chunk_size); \
  } \
  TEST(memiter, Short2_##chunk_size) \
  { \
    memiter_short_test(2, chunk_size); \
  } \
  TEST(memiter, Int2_##chunk_size) \
  { \
    memiter_int_test(2, chunk_size); \
  } \
  TEST(memiter, Long2_##chunk_size) \
  { \
    memiter_long_test(2, chunk_size); \
  } \
\
  TEST(memiter, Char3_##chunk_size) \
  { \
    memiter_char_test(3, chunk_size); \
  } \
  TEST(memiter, Short3_##chunk_size) \
  { \
    memiter_short_test(3, chunk_size); \
  } \
  TEST(memiter, Int3_##chunk_size) \
  { \
    memiter_int_test(3, chunk_size); \
  } \
  TEST(memiter, Long3_##chunk_size) \
  { \
    memiter_long_test(3, chunk_size); \
  } \
\
  TEST(memiter, Char256_##chunk_size) \
  { \
    memiter_char_test(256, chunk_size); \
  } \
  TEST(memiter, Short256_##chunk_size) \
  { \
    memiter_short_test(256, chunk_size); \
  } \
  TEST(memiter, Int256_##chunk_size) \
  { \
    memiter_int_test(256, chunk_size); \
  } \
  TEST(memiter, Long256_##chunk_size) \
  { \
    memiter_long_test(256, chunk_size); \
  }

TEST_NUMBER_AT_CHUNK_SIZE(1)
TEST_NUMBER_AT_CHUNK_SIZE(2)
TEST_NUMBER_AT_CHUNK_SIZE(3)
TEST_NUMBER_AT_CHUNK_SIZE(13)
TEST_NUMBER_AT_CHUNK_SIZE(256)

#define STRINGS_TEST(chunk_size, ...) \
  { \
    const char *data[] = {__VA_ARGS__, nullptr}; \
    memiter_string_test(data, chunk_size); \
  }

#define TEST_STRINGS_AT_CHUNK_SIZE(chunk_size) \
  TEST(memiter, Strings_##chunk_size) \
  { \
    STRINGS_TEST(chunk_size, ""); \
    STRINGS_TEST(chunk_size, "test", "me"); \
    STRINGS_TEST(chunk_size, "more", "test", "data", "to", "follow"); \
  }

TEST_STRINGS_AT_CHUNK_SIZE(1)
TEST_STRINGS_AT_CHUNK_SIZE(2)
TEST_STRINGS_AT_CHUNK_SIZE(3)
TEST_STRINGS_AT_CHUNK_SIZE(13)
TEST_STRINGS_AT_CHUNK_SIZE(256)

#define TEST_WORDS10K_AT_CHUNK_SIZE(chunk_size) \
  TEST(memiter, Words10kSentence_##chunk_size) \
  { \
    memiter_words10k_test('.', chunk_size); \
  } \
  TEST(memiter, Words10kWords_##chunk_size) \
  { \
    memiter_words10k_test(' ', chunk_size); \
  }

TEST_WORDS10K_AT_CHUNK_SIZE(1)
TEST_WORDS10K_AT_CHUNK_SIZE(2)
TEST_WORDS10K_AT_CHUNK_SIZE(3)
TEST_WORDS10K_AT_CHUNK_SIZE(13)
TEST_WORDS10K_AT_CHUNK_SIZE(256)
