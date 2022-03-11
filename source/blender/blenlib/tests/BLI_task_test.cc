/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"
#include <atomic>
#include <cstring>

#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_task.h"
#include "BLI_task.hh"

#define NUM_ITEMS 10000

/* *** Parallel iterations over range of integer values. *** */

static void task_range_iter_func(void *userdata, int index, const TaskParallelTLS *__restrict tls)
{
  int *data = (int *)userdata;
  data[index] = index;
  *((int *)tls->userdata_chunk) += index;
  //  printf("%d, %d, %d\n", index, data[index], *((int *)tls->userdata_chunk));
}

static void task_range_iter_reduce_func(const void *__restrict UNUSED(userdata),
                                        void *__restrict join_v,
                                        void *__restrict userdata_chunk)
{
  int *join = (int *)join_v;
  int *chunk = (int *)userdata_chunk;
  *join += *chunk;
  //  printf("%d, %d\n", data[NUM_ITEMS], *((int *)userdata_chunk));
}

TEST(task, RangeIter)
{
  int data[NUM_ITEMS] = {0};
  int sum = 0;

  BLI_threadapi_init();

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 1;

  settings.userdata_chunk = &sum;
  settings.userdata_chunk_size = sizeof(sum);
  settings.func_reduce = task_range_iter_reduce_func;

  BLI_task_parallel_range(0, NUM_ITEMS, data, task_range_iter_func, &settings);

  /* Those checks should ensure us all items of the listbase were processed once, and only once
   * as expected. */

  int expected_sum = 0;
  for (int i = 0; i < NUM_ITEMS; i++) {
    EXPECT_EQ(data[i], i);
    expected_sum += i;
  }
  EXPECT_EQ(sum, expected_sum);

  BLI_threadapi_exit();
}

/* *** Parallel iterations over mempool items. *** */

static void task_mempool_iter_func(void *userdata,
                                   MempoolIterData *item,
                                   const TaskParallelTLS *__restrict UNUSED(tls))
{
  int *data = (int *)item;
  int *count = (int *)userdata;

  EXPECT_TRUE(data != nullptr);

  *data += 1;
  atomic_sub_and_fetch_uint32((uint32_t *)count, 1);
}

TEST(task, MempoolIter)
{
  int *data[NUM_ITEMS];
  BLI_threadapi_init();
  BLI_mempool *mempool = BLI_mempool_create(
      sizeof(*data[0]), NUM_ITEMS, 32, BLI_MEMPOOL_ALLOW_ITER);

  int i;

  /* 'Randomly' add and remove some items from mempool, to create a non-homogeneous one. */
  int num_items = 0;
  for (i = 0; i < NUM_ITEMS; i++) {
    data[i] = (int *)BLI_mempool_alloc(mempool);
    *data[i] = i - 1;
    num_items++;
  }

  for (i = 0; i < NUM_ITEMS; i += 3) {
    BLI_mempool_free(mempool, data[i]);
    data[i] = nullptr;
    num_items--;
  }

  for (i = 0; i < NUM_ITEMS; i += 7) {
    if (data[i] == nullptr) {
      data[i] = (int *)BLI_mempool_alloc(mempool);
      *data[i] = i - 1;
      num_items++;
    }
  }

  for (i = 0; i < NUM_ITEMS - 5; i += 23) {
    for (int j = 0; j < 5; j++) {
      if (data[i + j] != nullptr) {
        BLI_mempool_free(mempool, data[i + j]);
        data[i + j] = nullptr;
        num_items--;
      }
    }
  }

  TaskParallelSettings settings;
  BLI_parallel_mempool_settings_defaults(&settings);

  BLI_task_parallel_mempool(mempool, &num_items, task_mempool_iter_func, &settings);

  /* Those checks should ensure us all items of the mempool were processed once, and only once - as
   * expected. */
  EXPECT_EQ(num_items, 0);
  for (i = 0; i < NUM_ITEMS; i++) {
    if (data[i] != nullptr) {
      EXPECT_EQ(*data[i], i);
    }
  }

  BLI_mempool_destroy(mempool);
  BLI_threadapi_exit();
}

/* *** Parallel iterations over mempool items with TLS. *** */

using TaskMemPool_Chunk = struct TaskMemPool_Chunk {
  ListBase *accumulate_items;
};

static void task_mempool_iter_tls_func(void *UNUSED(userdata),
                                       MempoolIterData *item,
                                       const TaskParallelTLS *__restrict tls)
{
  TaskMemPool_Chunk *task_data = (TaskMemPool_Chunk *)tls->userdata_chunk;
  int *data = (int *)item;

  EXPECT_TRUE(data != nullptr);
  if (task_data->accumulate_items == nullptr) {
    task_data->accumulate_items = MEM_cnew<ListBase>(__func__);
  }

  /* Flip to prove this has been touched. */
  *data = -*data;

  BLI_addtail(task_data->accumulate_items, BLI_genericNodeN(data));
}

static void task_mempool_iter_tls_reduce(const void *__restrict UNUSED(userdata),
                                         void *__restrict chunk_join,
                                         void *__restrict chunk)
{
  TaskMemPool_Chunk *join_chunk = (TaskMemPool_Chunk *)chunk_join;
  TaskMemPool_Chunk *data_chunk = (TaskMemPool_Chunk *)chunk;

  if (data_chunk->accumulate_items != nullptr) {
    if (join_chunk->accumulate_items == nullptr) {
      join_chunk->accumulate_items = MEM_cnew<ListBase>(__func__);
    }
    BLI_movelisttolist(join_chunk->accumulate_items, data_chunk->accumulate_items);
  }
}

static void task_mempool_iter_tls_free(const void *UNUSED(userdata),
                                       void *__restrict userdata_chunk)
{
  TaskMemPool_Chunk *task_data = (TaskMemPool_Chunk *)userdata_chunk;
  MEM_freeN(task_data->accumulate_items);
}

TEST(task, MempoolIterTLS)
{
  int *data[NUM_ITEMS];
  BLI_threadapi_init();
  BLI_mempool *mempool = BLI_mempool_create(
      sizeof(*data[0]), NUM_ITEMS, 32, BLI_MEMPOOL_ALLOW_ITER);

  int i;

  /* Add numbers negative `1..NUM_ITEMS` inclusive. */
  int num_items = 0;
  for (i = 0; i < NUM_ITEMS; i++) {
    data[i] = (int *)BLI_mempool_alloc(mempool);
    *data[i] = -(i + 1);
    num_items++;
  }

  TaskParallelSettings settings;
  BLI_parallel_mempool_settings_defaults(&settings);

  TaskMemPool_Chunk tls_data;
  tls_data.accumulate_items = nullptr;

  settings.userdata_chunk = &tls_data;
  settings.userdata_chunk_size = sizeof(tls_data);

  settings.func_free = task_mempool_iter_tls_free;
  settings.func_reduce = task_mempool_iter_tls_reduce;

  BLI_task_parallel_mempool(mempool, nullptr, task_mempool_iter_tls_func, &settings);

  EXPECT_EQ(BLI_listbase_count(tls_data.accumulate_items), NUM_ITEMS);

  /* Check that all elements are added into the list once. */
  int num_accum = 0;
  for (LinkData *link = (LinkData *)tls_data.accumulate_items->first; link; link = link->next) {
    int *data = (int *)link->data;
    num_accum += *data;
  }
  EXPECT_EQ(num_accum, (NUM_ITEMS * (NUM_ITEMS + 1)) / 2);

  BLI_freelistN(tls_data.accumulate_items);
  MEM_freeN(tls_data.accumulate_items);

  BLI_mempool_destroy(mempool);
  BLI_threadapi_exit();
}

/* *** Parallel iterations over double-linked list items. *** */

static void task_listbase_iter_func(void *userdata,
                                    void *item,
                                    int index,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  LinkData *data = (LinkData *)item;
  int *count = (int *)userdata;

  data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
  atomic_sub_and_fetch_uint32((uint32_t *)count, 1);
}

TEST(task, ListBaseIter)
{
  ListBase list = {nullptr, nullptr};
  LinkData *items_buffer = (LinkData *)MEM_calloc_arrayN(
      NUM_ITEMS, sizeof(*items_buffer), __func__);
  BLI_threadapi_init();

  int i;

  int num_items = 0;
  for (i = 0; i < NUM_ITEMS; i++) {
    BLI_addtail(&list, &items_buffer[i]);
    num_items++;
  }

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  BLI_task_parallel_listbase(&list, &num_items, task_listbase_iter_func, &settings);

  /* Those checks should ensure us all items of the listbase were processed once, and only once -
   * as expected. */
  EXPECT_EQ(num_items, 0);
  LinkData *item;
  for (i = 0, item = (LinkData *)list.first; i < NUM_ITEMS && item != nullptr;
       i++, item = item->next) {
    EXPECT_EQ(POINTER_AS_INT(item->data), i);
  }
  EXPECT_EQ(NUM_ITEMS, i);

  MEM_freeN(items_buffer);
  BLI_threadapi_exit();
}

TEST(task, ParallelInvoke)
{
  std::atomic<int> counter = 0;
  blender::threading::parallel_invoke([&]() { counter++; },
                                      [&]() { counter++; },
                                      [&]() { counter++; },
                                      [&]() { counter++; },
                                      [&]() { counter++; },
                                      [&]() { counter++; });
  EXPECT_EQ(counter, 6);
}
