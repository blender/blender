/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_array_store.h"
#include "BLI_array_utils.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_ressource_strings.h"
#include "BLI_string.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

/* print memory savings */
// #define DEBUG_PRINT

/* -------------------------------------------------------------------- */
/* Helper functions */

#ifdef DEBUG_PRINT
static void print_mem_saved(const char *id, const BArrayStore *bs)
{
  const double size_real = BLI_array_store_calc_size_compacted_get(bs);
  const double size_expand = BLI_array_store_calc_size_expanded_get(bs);
  const double percent = size_expand ? ((size_real / size_expand) * 100.0) : -1.0;
  printf("%s: %.8f%%\n", id, percent);
}
#endif

/* -------------------------------------------------------------------- */
/* Test Chunks (building data from list of chunks) */

struct TestChunk {
  struct TestChunk *next, *prev;
  const void *data;
  size_t data_len;
};

static TestChunk *testchunk_list_add(ListBase *lb, const void *data, size_t data_len)
{
  TestChunk *tc = (TestChunk *)MEM_mallocN(sizeof(*tc), __func__);
  tc->data = data;
  tc->data_len = data_len;
  BLI_addtail(lb, tc);

  return tc;
}

#if 0
static TestChunk *testchunk_list_add_copydata(ListBase *lb, const void *data, size_t data_len)
{
  void *data_copy = MEM_mallocN(data_len, __func__);
  memcpy(data_copy, data, data_len);
  return testchunk_list_add(lb, data_copy, data_len);
}
#endif

static void testchunk_list_free(ListBase *lb)
{
  for (TestChunk *tc = (TestChunk *)lb->first, *tb_next; tc; tc = tb_next) {
    tb_next = tc->next;
    MEM_freeN((void *)tc->data);
    MEM_freeN(tc);
  }
  BLI_listbase_clear(lb);
}

#if 0
static char *testchunk_as_data(ListBase *lb, size_t *r_data_len)
{
  size_t data_len = 0;
  for (TestChunk *tc = (TestChunk *)lb->first; tc; tc = tc->next) {
    data_len += tc->data_len;
  }
  char *data = (char *)MEM_mallocN(data_len, __func__);
  size_t i = 0;
  for (TestChunk *tc = (TestChunk *)lb->first; tc; tc = tc->next) {
    memcpy(&data[i], tc->data, tc->data_len);
    data_len += tc->data_len;
    i += tc->data_len;
  }
  if (r_data_len) {
    *r_data_len = i;
  }
  return data;
}
#endif

static char *testchunk_as_data_array(TestChunk **tc_array, int tc_array_len, size_t *r_data_len)
{
  size_t data_len = 0;
  for (int tc_index = 0; tc_index < tc_array_len; tc_index++) {
    data_len += tc_array[tc_index]->data_len;
  }
  char *data = (char *)MEM_mallocN(data_len, __func__);
  size_t i = 0;
  for (int tc_index = 0; tc_index < tc_array_len; tc_index++) {
    TestChunk *tc = tc_array[tc_index];
    memcpy(&data[i], tc->data, tc->data_len);
    i += tc->data_len;
  }
  if (r_data_len) {
    *r_data_len = i;
  }
  return data;
}

/* -------------------------------------------------------------------- */
/* Test Buffer */

/* API to handle local allocation of data so we can compare it with the data in the array_store */
struct TestBuffer {
  struct TestBuffer *next, *prev;
  const void *data;
  size_t data_len;

  /* for reference */
  BArrayState *state;
};

static TestBuffer *testbuffer_list_add(ListBase *lb, const void *data, size_t data_len)
{
  TestBuffer *tb = (TestBuffer *)MEM_mallocN(sizeof(*tb), __func__);
  tb->data = data;
  tb->data_len = data_len;
  tb->state = nullptr;
  BLI_addtail(lb, tb);
  return tb;
}

static TestBuffer *testbuffer_list_add_copydata(ListBase *lb, const void *data, size_t data_len)
{
  void *data_copy = MEM_mallocN(data_len, __func__);
  memcpy(data_copy, data, data_len);
  return testbuffer_list_add(lb, data_copy, data_len);
}

static void testbuffer_list_state_from_data(ListBase *lb, const char *data, const size_t data_len)
{
  testbuffer_list_add_copydata(lb, (const void *)data, data_len);
}

/**
 * A version of testbuffer_list_state_from_data that expand data by stride,
 * handy so we can test data at different strides.
 */
static void testbuffer_list_state_from_data__stride_expand(ListBase *lb,
                                                           const char *data,
                                                           const size_t data_len,
                                                           const size_t stride)
{
  if (stride == 1) {
    testbuffer_list_state_from_data(lb, data, data_len);
  }
  else {
    const size_t data_stride_len = data_len * stride;
    char *data_stride = (char *)MEM_mallocN(data_stride_len, __func__);

    for (size_t i = 0, i_stride = 0; i < data_len; i += 1, i_stride += stride) {
      memset(&data_stride[i_stride], data[i], stride);
    }

    testbuffer_list_add(lb, (const void *)data_stride, data_stride_len);
  }
}

#define testbuffer_list_state_from_string_array(lb, data_array) \
  { \
    unsigned int i_ = 0; \
    const char *data; \
    while ((data = data_array[i_++])) { \
      testbuffer_list_state_from_data(lb, data, strlen(data)); \
    } \
  } \
  ((void)0)

//

#define TESTBUFFER_STRINGS_CREATE(lb, ...) \
  { \
    BLI_listbase_clear(lb); \
    const char *data_array[] = {__VA_ARGS__ NULL}; \
    testbuffer_list_state_from_string_array((lb), data_array); \
  } \
  ((void)0)

/* test in both directions */
#define TESTBUFFER_STRINGS_EX(bs, ...) \
  { \
    ListBase lb; \
    TESTBUFFER_STRINGS_CREATE(&lb, __VA_ARGS__); \
\
    testbuffer_run_tests(bs, &lb); \
\
    testbuffer_list_free(&lb); \
  } \
  ((void)0)

#define TESTBUFFER_STRINGS(stride, chunk_count, ...) \
  { \
    ListBase lb; \
    TESTBUFFER_STRINGS_CREATE(&lb, __VA_ARGS__); \
\
    testbuffer_run_tests_simple(&lb, stride, chunk_count); \
\
    testbuffer_list_free(&lb); \
  } \
  ((void)0)

static bool testbuffer_item_validate(TestBuffer *tb)
{
  size_t data_state_len;
  bool ok = true;
  void *data_state = BLI_array_store_state_data_get_alloc(tb->state, &data_state_len);
  if (tb->data_len != data_state_len) {
    ok = false;
  }
  else if (memcmp(data_state, tb->data, data_state_len) != 0) {
    ok = false;
  }
  MEM_freeN(data_state);
  return ok;
}

static bool testbuffer_list_validate(const ListBase *lb)
{
  for (TestBuffer *tb = (TestBuffer *)lb->first; tb; tb = tb->next) {
    if (!testbuffer_item_validate(tb)) {
      return false;
    }
  }

  return true;
}

static void testbuffer_list_data_randomize(ListBase *lb, unsigned int random_seed)
{
  for (TestBuffer *tb = (TestBuffer *)lb->first; tb; tb = tb->next) {
    BLI_array_randomize((void *)tb->data, 1, tb->data_len, random_seed++);
  }
}

static void testbuffer_list_store_populate(BArrayStore *bs, ListBase *lb)
{
  for (TestBuffer *tb = (TestBuffer *)lb->first, *tb_prev = nullptr; tb;
       tb_prev = tb, tb = tb->next) {
    tb->state = BLI_array_store_state_add(
        bs, tb->data, tb->data_len, (tb_prev ? tb_prev->state : nullptr));
  }
}

static void testbuffer_list_store_clear(BArrayStore *bs, ListBase *lb)
{
  for (TestBuffer *tb = (TestBuffer *)lb->first; tb; tb = tb->next) {
    BLI_array_store_state_remove(bs, tb->state);
    tb->state = nullptr;
  }
}

static void testbuffer_list_free(ListBase *lb)
{
  for (TestBuffer *tb = (TestBuffer *)lb->first, *tb_next; tb; tb = tb_next) {
    tb_next = tb->next;
    MEM_freeN((void *)tb->data);
    MEM_freeN(tb);
  }
  BLI_listbase_clear(lb);
}

static void testbuffer_run_tests_single(BArrayStore *bs, ListBase *lb)
{
  testbuffer_list_store_populate(bs, lb);
  EXPECT_TRUE(testbuffer_list_validate(lb));
  EXPECT_TRUE(BLI_array_store_is_valid(bs));
#ifdef DEBUG_PRINT
  print_mem_saved("data", bs);
#endif
}

/* avoid copy-paste code to run tests */
static void testbuffer_run_tests(BArrayStore *bs, ListBase *lb)
{
  /* forwards */
  testbuffer_run_tests_single(bs, lb);
  testbuffer_list_store_clear(bs, lb);

  BLI_listbase_reverse(lb);

  /* backwards */
  testbuffer_run_tests_single(bs, lb);
  testbuffer_list_store_clear(bs, lb);
}

static void testbuffer_run_tests_simple(ListBase *lb, const int stride, const int chunk_count)
{
  BArrayStore *bs = BLI_array_store_create(stride, chunk_count);
  testbuffer_run_tests(bs, lb);
  BLI_array_store_destroy(bs);
}

/* -------------------------------------------------------------------- */
/* Basic Tests */

TEST(array_store, Nop)
{
  BArrayStore *bs = BLI_array_store_create(1, 32);
  BLI_array_store_destroy(bs);
}

TEST(array_store, NopState)
{
  BArrayStore *bs = BLI_array_store_create(1, 32);
  const unsigned char data[] = "test";
  BArrayState *state = BLI_array_store_state_add(bs, data, sizeof(data) - 1, nullptr);
  EXPECT_EQ(BLI_array_store_state_size_get(state), sizeof(data) - 1);
  BLI_array_store_state_remove(bs, state);
  BLI_array_store_destroy(bs);
}

TEST(array_store, Single)
{
  BArrayStore *bs = BLI_array_store_create(1, 32);
  const char data_src[] = "test";
  const char *data_dst;
  BArrayState *state = BLI_array_store_state_add(bs, data_src, sizeof(data_src), nullptr);
  size_t data_dst_len;
  data_dst = (char *)BLI_array_store_state_data_get_alloc(state, &data_dst_len);
  EXPECT_STREQ(data_src, data_dst);
  EXPECT_EQ(data_dst_len, sizeof(data_src));
  BLI_array_store_destroy(bs);
  MEM_freeN((void *)data_dst);
}

TEST(array_store, DoubleNop)
{
  BArrayStore *bs = BLI_array_store_create(1, 32);
  const char data_src[] = "test";
  const char *data_dst;

  BArrayState *state_a = BLI_array_store_state_add(bs, data_src, sizeof(data_src), nullptr);
  BArrayState *state_b = BLI_array_store_state_add(bs, data_src, sizeof(data_src), state_a);

  EXPECT_EQ(BLI_array_store_calc_size_compacted_get(bs), sizeof(data_src));
  EXPECT_EQ(BLI_array_store_calc_size_expanded_get(bs), sizeof(data_src) * 2);

  size_t data_dst_len;

  data_dst = (char *)BLI_array_store_state_data_get_alloc(state_a, &data_dst_len);
  EXPECT_STREQ(data_src, data_dst);
  MEM_freeN((void *)data_dst);

  data_dst = (char *)BLI_array_store_state_data_get_alloc(state_b, &data_dst_len);
  EXPECT_STREQ(data_src, data_dst);
  MEM_freeN((void *)data_dst);

  EXPECT_EQ(data_dst_len, sizeof(data_src));
  BLI_array_store_destroy(bs);
}

TEST(array_store, DoubleDiff)
{
  BArrayStore *bs = BLI_array_store_create(1, 32);
  const char data_src_a[] = "test";
  const char data_src_b[] = "####";
  const char *data_dst;

  BArrayState *state_a = BLI_array_store_state_add(bs, data_src_a, sizeof(data_src_a), nullptr);
  BArrayState *state_b = BLI_array_store_state_add(bs, data_src_b, sizeof(data_src_b), state_a);
  size_t data_dst_len;

  EXPECT_EQ(BLI_array_store_calc_size_compacted_get(bs), sizeof(data_src_a) * 2);
  EXPECT_EQ(BLI_array_store_calc_size_expanded_get(bs), sizeof(data_src_a) * 2);

  data_dst = (char *)BLI_array_store_state_data_get_alloc(state_a, &data_dst_len);
  EXPECT_STREQ(data_src_a, data_dst);
  MEM_freeN((void *)data_dst);

  data_dst = (char *)BLI_array_store_state_data_get_alloc(state_b, &data_dst_len);
  EXPECT_STREQ(data_src_b, data_dst);
  MEM_freeN((void *)data_dst);

  BLI_array_store_destroy(bs);
}

TEST(array_store, TextMixed)
{
  TESTBUFFER_STRINGS(1, 4, "", );
  TESTBUFFER_STRINGS(1, 4, "test", );
  TESTBUFFER_STRINGS(1, 4, "", "test", );
  TESTBUFFER_STRINGS(1, 4, "test", "", );
  TESTBUFFER_STRINGS(1, 4, "test", "", "test", );
  TESTBUFFER_STRINGS(1, 4, "", "test", "", );
}

TEST(array_store, TextDupeIncreaseDecrease)
{
  ListBase lb;

#define D "#1#2#3#4"
  TESTBUFFER_STRINGS_CREATE(&lb, D, D D, D D D, D D D D, );

  BArrayStore *bs = BLI_array_store_create(1, 8);

  /* forward */
  testbuffer_list_store_populate(bs, &lb);
  EXPECT_TRUE(testbuffer_list_validate(&lb));
  EXPECT_TRUE(BLI_array_store_is_valid(bs));
  EXPECT_EQ(BLI_array_store_calc_size_compacted_get(bs), strlen(D));

  testbuffer_list_store_clear(bs, &lb);
  BLI_listbase_reverse(&lb);

  /* backwards */
  testbuffer_list_store_populate(bs, &lb);
  EXPECT_TRUE(testbuffer_list_validate(&lb));
  EXPECT_TRUE(BLI_array_store_is_valid(bs));
  /* larger since first block doesn't de-duplicate */
  EXPECT_EQ(BLI_array_store_calc_size_compacted_get(bs), strlen(D) * 4);

#undef D
  testbuffer_list_free(&lb);

  BLI_array_store_destroy(bs);
}

/* -------------------------------------------------------------------- */
/* Plain Text Tests */

/**
 * Test that uses text input with different params for the array-store
 * to ensure no corner cases fail.
 */
static void plain_text_helper(const char *words,
                              int words_len,
                              const char word_delim,
                              const int stride,
                              const int chunk_count,
                              const int random_seed)
{

  ListBase lb;
  BLI_listbase_clear(&lb);

  for (int i = 0, i_prev = 0; i < words_len; i++) {
    if (ELEM(words[i], word_delim, '\0')) {
      if (i != i_prev) {
        testbuffer_list_state_from_data__stride_expand(&lb, &words[i_prev], i - i_prev, stride);
      }
      i_prev = i;
    }
  }

  if (random_seed) {
    testbuffer_list_data_randomize(&lb, random_seed);
  }

  testbuffer_run_tests_simple(&lb, stride, chunk_count);

  testbuffer_list_free(&lb);
}

/* split by '.' (multiple words) */
#define WORDS words10k, sizeof(words10k)
TEST(array_store, TextSentences_Chunk1)
{
  plain_text_helper(WORDS, '.', 1, 1, 0);
}
TEST(array_store, TextSentences_Chunk2)
{
  plain_text_helper(WORDS, '.', 1, 2, 0);
}
TEST(array_store, TextSentences_Chunk8)
{
  plain_text_helper(WORDS, '.', 1, 8, 0);
}
TEST(array_store, TextSentences_Chunk32)
{
  plain_text_helper(WORDS, '.', 1, 32, 0);
}
TEST(array_store, TextSentences_Chunk128)
{
  plain_text_helper(WORDS, '.', 1, 128, 0);
}
TEST(array_store, TextSentences_Chunk1024)
{
  plain_text_helper(WORDS, '.', 1, 1024, 0);
}
/* odd numbers */
TEST(array_store, TextSentences_Chunk3)
{
  plain_text_helper(WORDS, '.', 1, 3, 0);
}
TEST(array_store, TextSentences_Chunk13)
{
  plain_text_helper(WORDS, '.', 1, 13, 0);
}
TEST(array_store, TextSentences_Chunk131)
{
  plain_text_helper(WORDS, '.', 1, 131, 0);
}

/* split by ' ', individual words */
TEST(array_store, TextWords_Chunk1)
{
  plain_text_helper(WORDS, ' ', 1, 1, 0);
}
TEST(array_store, TextWords_Chunk2)
{
  plain_text_helper(WORDS, ' ', 1, 2, 0);
}
TEST(array_store, TextWords_Chunk8)
{
  plain_text_helper(WORDS, ' ', 1, 8, 0);
}
TEST(array_store, TextWords_Chunk32)
{
  plain_text_helper(WORDS, ' ', 1, 32, 0);
}
TEST(array_store, TextWords_Chunk128)
{
  plain_text_helper(WORDS, ' ', 1, 128, 0);
}
TEST(array_store, TextWords_Chunk1024)
{
  plain_text_helper(WORDS, ' ', 1, 1024, 0);
}
/* odd numbers */
TEST(array_store, TextWords_Chunk3)
{
  plain_text_helper(WORDS, ' ', 1, 3, 0);
}
TEST(array_store, TextWords_Chunk13)
{
  plain_text_helper(WORDS, ' ', 1, 13, 0);
}
TEST(array_store, TextWords_Chunk131)
{
  plain_text_helper(WORDS, ' ', 1, 131, 0);
}

/* various tests with different strides & randomizing */
TEST(array_store, TextSentencesRandom_Stride3_Chunk3)
{
  plain_text_helper(WORDS, 'q', 3, 3, 7337);
}
TEST(array_store, TextSentencesRandom_Stride8_Chunk8)
{
  plain_text_helper(WORDS, 'n', 8, 8, 5667);
}
TEST(array_store, TextSentencesRandom_Stride32_Chunk1)
{
  plain_text_helper(WORDS, 'a', 1, 32, 1212);
}
TEST(array_store, TextSentencesRandom_Stride12_Chunk512)
{
  plain_text_helper(WORDS, 'g', 12, 512, 9999);
}
TEST(array_store, TextSentencesRandom_Stride128_Chunk6)
{
  plain_text_helper(WORDS, 'b', 20, 6, 1000);
}

#undef WORDS

/* -------------------------------------------------------------------- */
/* Random Data Tests */

static unsigned int rand_range_i(RNG *rng,
                                 unsigned int min_i,
                                 unsigned int max_i,
                                 unsigned int step)
{
  if (min_i == max_i) {
    return min_i;
  }
  BLI_assert(min_i <= max_i);
  BLI_assert(((min_i % step) == 0) && ((max_i % step) == 0));
  unsigned int range = (max_i - min_i);
  unsigned int value = BLI_rng_get_uint(rng) % range;
  value = (value / step) * step;
  return min_i + value;
}

static void testbuffer_list_state_random_data(ListBase *lb,
                                              const size_t stride,
                                              const size_t data_min_len,
                                              const size_t data_max_len,

                                              const unsigned int mutate,
                                              RNG *rng)
{
  size_t data_len = rand_range_i(rng, data_min_len, data_max_len + stride, stride);
  char *data = (char *)MEM_mallocN(data_len, __func__);

  if (lb->last == nullptr) {
    BLI_rng_get_char_n(rng, data, data_len);
  }
  else {
    TestBuffer *tb_last = (TestBuffer *)lb->last;
    if (tb_last->data_len >= data_len) {
      memcpy(data, tb_last->data, data_len);
    }
    else {
      memcpy(data, tb_last->data, tb_last->data_len);
      BLI_rng_get_char_n(rng, &data[tb_last->data_len], data_len - tb_last->data_len);
    }

    /* perform multiple small mutations to the array. */
    for (int i = 0; i < mutate; i++) {
      enum {
        MUTATE_NOP = 0,
        MUTATE_ADD,
        MUTATE_REMOVE,
        MUTATE_ROTATE,
        MUTATE_RANDOMIZE,
        MUTATE_TOTAL,
      };

      switch ((BLI_rng_get_uint(rng) % MUTATE_TOTAL)) {
        case MUTATE_NOP: {
          break;
        }
        case MUTATE_ADD: {
          const unsigned int offset = rand_range_i(rng, 0, data_len, stride);
          if (data_len < data_max_len) {
            data_len += stride;
            data = (char *)MEM_reallocN((void *)data, data_len);
            memmove(&data[offset + stride], &data[offset], data_len - (offset + stride));
            BLI_rng_get_char_n(rng, &data[offset], stride);
          }
          break;
        }
        case MUTATE_REMOVE: {
          const unsigned int offset = rand_range_i(rng, 0, data_len, stride);
          if (data_len > data_min_len) {
            memmove(&data[offset], &data[offset + stride], data_len - (offset + stride));
            data_len -= stride;
          }
          break;
        }
        case MUTATE_ROTATE: {
          int items = data_len / stride;
          if (items > 1) {
            _bli_array_wrap(data, items, stride, (BLI_rng_get_uint(rng) % 2) ? -1 : 1);
          }
          break;
        }
        case MUTATE_RANDOMIZE: {
          if (data_len > 0) {
            const unsigned int offset = rand_range_i(rng, 0, data_len - stride, stride);
            BLI_rng_get_char_n(rng, &data[offset], stride);
          }
          break;
        }
        default:
          BLI_assert(0);
      }
    }
  }

  testbuffer_list_add(lb, (const void *)data, data_len);
}

static void random_data_mutate_helper(const int items_size_min,
                                      const int items_size_max,
                                      const int items_total,
                                      const int stride,
                                      const int chunk_count,
                                      const int random_seed,
                                      const int mutate)
{

  ListBase lb;
  BLI_listbase_clear(&lb);

  const size_t data_min_len = items_size_min * stride;
  const size_t data_max_len = items_size_max * stride;

  {
    RNG *rng = BLI_rng_new(random_seed);
    for (int i = 0; i < items_total; i++) {
      testbuffer_list_state_random_data(&lb, stride, data_min_len, data_max_len, mutate, rng);
    }
    BLI_rng_free(rng);
  }

  testbuffer_run_tests_simple(&lb, stride, chunk_count);

  testbuffer_list_free(&lb);
}

TEST(array_store, TestData_Stride1_Chunk32_Mutate2)
{
  random_data_mutate_helper(0, 100, 400, 1, 32, 9779, 2);
}
TEST(array_store, TestData_Stride8_Chunk512_Mutate2)
{
  random_data_mutate_helper(0, 128, 400, 8, 512, 1001, 2);
}
TEST(array_store, TestData_Stride12_Chunk48_Mutate2)
{
  random_data_mutate_helper(200, 256, 400, 12, 48, 1331, 2);
}
TEST(array_store, TestData_Stride32_Chunk64_Mutate1)
{
  random_data_mutate_helper(0, 256, 200, 32, 64, 3112, 1);
}
TEST(array_store, TestData_Stride32_Chunk64_Mutate8)
{
  random_data_mutate_helper(0, 256, 200, 32, 64, 7117, 8);
}

/* -------------------------------------------------------------------- */
/* Randomized Chunks Test */

static void random_chunk_generate(ListBase *lb,
                                  const int chunks_per_buffer,
                                  const int stride,
                                  const int chunk_count,
                                  const int random_seed)
{
  RNG *rng = BLI_rng_new(random_seed);
  const size_t chunk_size_bytes = stride * chunk_count;
  for (int i = 0; i < chunks_per_buffer; i++) {
    char *data_chunk = (char *)MEM_mallocN(chunk_size_bytes, __func__);
    BLI_rng_get_char_n(rng, data_chunk, chunk_size_bytes);
    testchunk_list_add(lb, data_chunk, chunk_size_bytes);
  }
  BLI_rng_free(rng);
}

/**
 * Add random chunks, then re-order them to ensure chunk de-duplication is working.
 */
static void random_chunk_mutate_helper(const int chunks_per_buffer,
                                       const int items_total,
                                       const int stride,
                                       const int chunk_count,
                                       const int random_seed)
{
  /* generate random chunks */

  ListBase random_chunks;
  BLI_listbase_clear(&random_chunks);
  random_chunk_generate(&random_chunks, chunks_per_buffer, stride, chunk_count, random_seed);
  TestChunk **chunks_array = (TestChunk **)MEM_mallocN(chunks_per_buffer * sizeof(TestChunk *),
                                                       __func__);
  {
    TestChunk *tc = (TestChunk *)random_chunks.first;
    for (int i = 0; i < chunks_per_buffer; i++, tc = tc->next) {
      chunks_array[i] = tc;
    }
  }

  /* add and re-order each time */
  ListBase lb;
  BLI_listbase_clear(&lb);

  {
    RNG *rng = BLI_rng_new(random_seed);
    for (int i = 0; i < items_total; i++) {
      BLI_rng_shuffle_array(rng, chunks_array, sizeof(TestChunk *), chunks_per_buffer);
      size_t data_len;
      char *data = testchunk_as_data_array(chunks_array, chunks_per_buffer, &data_len);
      BLI_assert(data_len == chunks_per_buffer * chunk_count * stride);
      testbuffer_list_add(&lb, (const void *)data, data_len);
    }
    BLI_rng_free(rng);
  }

  testchunk_list_free(&random_chunks);
  MEM_freeN(chunks_array);

  BArrayStore *bs = BLI_array_store_create(stride, chunk_count);
  testbuffer_run_tests_single(bs, &lb);

  size_t expected_size = chunks_per_buffer * chunk_count * stride;
  EXPECT_EQ(BLI_array_store_calc_size_compacted_get(bs), expected_size);

  BLI_array_store_destroy(bs);

  testbuffer_list_free(&lb);
}

TEST(array_store, TestChunk_Rand8_Stride1_Chunk64)
{
  random_chunk_mutate_helper(8, 100, 1, 64, 9779);
}
TEST(array_store, TestChunk_Rand32_Stride1_Chunk64)
{
  random_chunk_mutate_helper(32, 100, 1, 64, 1331);
}
TEST(array_store, TestChunk_Rand64_Stride8_Chunk32)
{
  random_chunk_mutate_helper(64, 100, 8, 32, 2772);
}
TEST(array_store, TestChunk_Rand31_Stride11_Chunk21)
{
  random_chunk_mutate_helper(31, 100, 11, 21, 7117);
}

#if 0
/* -------------------------------------------------------------------- */

/* Test From Files (disabled, keep for local tests.) */

void *file_read_binary_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size)
{
  FILE *fp = fopen(filepath, "rb");
  void *mem = NULL;

  if (fp) {
    long int filelen_read;
    fseek(fp, 0L, SEEK_END);
    const long int filelen = ftell(fp);
    if (filelen == -1) {
      goto finally;
    }
    fseek(fp, 0L, SEEK_SET);

    mem = MEM_mallocN(filelen + pad_bytes, __func__);
    if (mem == NULL) {
      goto finally;
    }

    filelen_read = fread(mem, 1, filelen, fp);
    if ((filelen_read != filelen) || ferror(fp)) {
      MEM_freeN(mem);
      mem = NULL;
      goto finally;
    }

    *r_size = filelen_read;

  finally:
    fclose(fp);
  }

  return mem;
}

TEST(array_store, PlainTextFiles)
{
  ListBase lb;
  BLI_listbase_clear(&lb);
  BArrayStore *bs = BLI_array_store_create(1, 128);

  for (int i = 0; i < 629; i++) {
    char str[512];
    BLI_snprintf(str, sizeof(str), "/src/py_array_cow/test_data/xz_data/%04d.c.xz", i);
    // BLI_snprintf(str, sizeof(str), "/src/py_array_cow/test_data/c_code/%04d.c", i);
    // printf("%s\n", str);
    size_t data_len;
    void *data;
    data = file_read_binary_as_mem(str, 0, &data_len);

    testbuffer_list_add(&lb, (const void *)data, data_len);
  }

  /* forwards */
  testbuffer_list_store_populate(bs, &lb);
  EXPECT_TRUE(testbuffer_list_validate(&lb));
  EXPECT_TRUE(BLI_array_store_is_valid(bs));
#  ifdef DEBUG_PRINT
  print_mem_saved("source code forward", bs);
#  endif

  testbuffer_list_store_clear(bs, &lb);
  BLI_listbase_reverse(&lb);

  /* backwards */
  testbuffer_list_store_populate(bs, &lb);
  EXPECT_TRUE(testbuffer_list_validate(&lb));
  EXPECT_TRUE(BLI_array_store_is_valid(bs));
#  ifdef DEBUG_PRINT
  print_mem_saved("source code backwards", bs);
#  endif

  testbuffer_list_free(&lb);
  BLI_array_store_destroy(bs);
}
#endif
