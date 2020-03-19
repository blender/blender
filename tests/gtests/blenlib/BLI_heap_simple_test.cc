/* Apache License, Version 2.0 */

#include "testing/testing.h"
#include <string.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_compiler_attrs.h"
#include "BLI_heap_simple.h"
#include "BLI_rand.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
};

#define SIZE 1024

static void range_fl(float *array_tar, const int size)
{
  float *array_pt = array_tar + (size - 1);
  int i = size;
  while (i--) {
    *(array_pt--) = (float)i;
  }
}

TEST(heap, SimpleEmpty)
{
  HeapSimple *heap;

  heap = BLI_heapsimple_new();
  EXPECT_TRUE(BLI_heapsimple_is_empty(heap));
  EXPECT_EQ(BLI_heapsimple_len(heap), 0);
  BLI_heapsimple_free(heap, NULL);
}

TEST(heap, SimpleOne)
{
  HeapSimple *heap;
  const char *in = "test";

  heap = BLI_heapsimple_new();

  BLI_heapsimple_insert(heap, 0.0f, (void *)in);
  EXPECT_FALSE(BLI_heapsimple_is_empty(heap));
  EXPECT_EQ(BLI_heapsimple_len(heap), 1);
  EXPECT_EQ(in, BLI_heapsimple_pop_min(heap));
  EXPECT_TRUE(BLI_heapsimple_is_empty(heap));
  EXPECT_EQ(BLI_heapsimple_len(heap), 0);
  BLI_heapsimple_free(heap, NULL);
}

TEST(heap, SimpleRange)
{
  const int items_total = SIZE;
  HeapSimple *heap = BLI_heapsimple_new();
  for (int in = 0; in < items_total; in++) {
    BLI_heapsimple_insert(heap, (float)in, POINTER_FROM_INT(in));
  }
  for (int out_test = 0; out_test < items_total; out_test++) {
    EXPECT_EQ(out_test, POINTER_AS_INT(BLI_heapsimple_pop_min(heap)));
  }
  EXPECT_TRUE(BLI_heapsimple_is_empty(heap));
  BLI_heapsimple_free(heap, NULL);
}

TEST(heap, SimpleRangeReverse)
{
  const int items_total = SIZE;
  HeapSimple *heap = BLI_heapsimple_new();
  for (int in = 0; in < items_total; in++) {
    BLI_heapsimple_insert(heap, (float)-in, POINTER_FROM_INT(-in));
  }
  for (int out_test = items_total - 1; out_test >= 0; out_test--) {
    EXPECT_EQ(-out_test, POINTER_AS_INT(BLI_heapsimple_pop_min(heap)));
  }
  EXPECT_TRUE(BLI_heapsimple_is_empty(heap));
  BLI_heapsimple_free(heap, NULL);
}

TEST(heap, SimpleDuplicates)
{
  const int items_total = SIZE;
  HeapSimple *heap = BLI_heapsimple_new();
  for (int in = 0; in < items_total; in++) {
    BLI_heapsimple_insert(heap, 1.0f, 0);
  }
  for (int out_test = 0; out_test < items_total; out_test++) {
    EXPECT_EQ(0, POINTER_AS_INT(BLI_heapsimple_pop_min(heap)));
  }
  EXPECT_TRUE(BLI_heapsimple_is_empty(heap));
  BLI_heapsimple_free(heap, NULL);
}

static void random_heapsimple_helper(const int items_total, const int random_seed)
{
  HeapSimple *heap = BLI_heapsimple_new();
  float *values = (float *)MEM_mallocN(sizeof(float) * items_total, __func__);
  range_fl(values, items_total);
  BLI_array_randomize(values, sizeof(float), items_total, random_seed);
  for (int i = 0; i < items_total; i++) {
    BLI_heapsimple_insert(heap, values[i], POINTER_FROM_INT((int)values[i]));
  }
  for (int out_test = 0; out_test < items_total; out_test++) {
    EXPECT_EQ(out_test, POINTER_AS_INT(BLI_heapsimple_pop_min(heap)));
  }
  EXPECT_TRUE(BLI_heapsimple_is_empty(heap));
  BLI_heapsimple_free(heap, NULL);
  MEM_freeN(values);
}

TEST(heap, SimpleRand1)
{
  random_heapsimple_helper(1, 1234);
}
TEST(heap, SimpleRand2)
{
  random_heapsimple_helper(2, 1234);
}
TEST(heap, SimpleRand100)
{
  random_heapsimple_helper(100, 4321);
}
