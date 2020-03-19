/* Apache License, Version 2.0 */

#include "BLI_ressource_strings.h"
#include "testing/testing.h"

#include "atomic_ops.h"

#define GHASH_INTERNAL_API

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_task.h"

#include "PIL_time.h"
}

#define NUM_RUN_AVERAGED 100

static uint gen_pseudo_random_number(uint num)
{
  /* Note: this is taken from BLI_ghashutil_uinthash(), don't want to depend on external code that
   * might change here... */
  num += ~(num << 16);
  num ^= (num >> 5);
  num += (num << 3);
  num ^= (num >> 13);
  num += ~(num << 9);
  num ^= (num >> 17);

  /* Make final number in [65 - 16385] range. */
  return ((num & 255) << 6) + 1;
}

/* *** Parallel iterations over range of indices. *** */

static void task_parallel_range_func(void *UNUSED(userdata),
                                     int index,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  const uint limit = gen_pseudo_random_number((uint)index);
  for (uint i = (uint)index; i < limit;) {
    i += gen_pseudo_random_number(i);
  }
}

static void task_parallel_range_test_do(const char *id,
                                        const int num_items,
                                        const bool use_threads)
{
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = use_threads;

  double averaged_timing = 0.0;
  for (int i = 0; i < NUM_RUN_AVERAGED; i++) {
    const double init_time = PIL_check_seconds_timer();
    for (int j = 0; j < 10; j++) {
      BLI_task_parallel_range(i + j, i + j + num_items, NULL, task_parallel_range_func, &settings);
    }
    averaged_timing += PIL_check_seconds_timer() - init_time;
  }

  printf("\t%s: non-pooled done in %fs on average over %d runs\n",
         id,
         averaged_timing / NUM_RUN_AVERAGED,
         NUM_RUN_AVERAGED);

  averaged_timing = 0.0;
  for (int i = 0; i < NUM_RUN_AVERAGED; i++) {
    const double init_time = PIL_check_seconds_timer();
    TaskParallelRangePool *range_pool = BLI_task_parallel_range_pool_init(&settings);
    for (int j = 0; j < 10; j++) {
      BLI_task_parallel_range_pool_push(
          range_pool, i + j, i + j + num_items, NULL, task_parallel_range_func, &settings);
    }
    BLI_task_parallel_range_pool_work_and_wait(range_pool);
    BLI_task_parallel_range_pool_free(range_pool);
    averaged_timing += PIL_check_seconds_timer() - init_time;
  }

  printf("\t%s: pooled done in %fs on average over %d runs\n",
         id,
         averaged_timing / NUM_RUN_AVERAGED,
         NUM_RUN_AVERAGED);
}

TEST(task, RangeIter10KNoThread)
{
  task_parallel_range_test_do(
      "Range parallel iteration - Single thread - 10K items", 10000, false);
}

TEST(task, RangeIter10k)
{
  task_parallel_range_test_do("Range parallel iteration - Threaded - 10K items", 10000, true);
}

TEST(task, RangeIter100KNoThread)
{
  task_parallel_range_test_do(
      "Range parallel iteration - Single thread - 100K items", 100000, false);
}

TEST(task, RangeIter100k)
{
  task_parallel_range_test_do("Range parallel iteration - Threaded - 100K items", 100000, true);
}

TEST(task, RangeIter1000KNoThread)
{
  task_parallel_range_test_do(
      "Range parallel iteration - Single thread - 1000K items", 1000000, false);
}

TEST(task, RangeIter1000k)
{
  task_parallel_range_test_do("Range parallel iteration - Threaded - 1000K items", 1000000, true);
}

/* *** Parallel iterations over double-linked list items. *** */

static void task_listbase_light_iter_func(void *UNUSED(userdata),
                                          void *item,
                                          int index,
                                          const TaskParallelTLS *__restrict UNUSED(tls))

{
  LinkData *data = (LinkData *)item;

  data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
}

static void task_listbase_light_membarrier_iter_func(void *userdata,
                                                     void *item,
                                                     int index,
                                                     const TaskParallelTLS *__restrict UNUSED(tls))

{
  LinkData *data = (LinkData *)item;
  int *count = (int *)userdata;

  data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
  atomic_sub_and_fetch_uint32((uint32_t *)count, 1);
}

static void task_listbase_heavy_iter_func(void *UNUSED(userdata),
                                          void *item,
                                          int index,
                                          const TaskParallelTLS *__restrict UNUSED(tls))

{
  LinkData *data = (LinkData *)item;

  /* 'Random' number of iterations. */
  const uint num = gen_pseudo_random_number((uint)index);

  for (uint i = 0; i < num; i++) {
    data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + ((i % 2) ? -index : index));
  }
}

static void task_listbase_heavy_membarrier_iter_func(void *userdata,
                                                     void *item,
                                                     int index,
                                                     const TaskParallelTLS *__restrict UNUSED(tls))

{
  LinkData *data = (LinkData *)item;
  int *count = (int *)userdata;

  /* 'Random' number of iterations. */
  const uint num = gen_pseudo_random_number((uint)index);

  for (uint i = 0; i < num; i++) {
    data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + ((i % 2) ? -index : index));
  }
  atomic_sub_and_fetch_uint32((uint32_t *)count, 1);
}

static void task_listbase_test_do(ListBase *list,
                                  const int num_items,
                                  int *num_items_tmp,
                                  const char *id,
                                  TaskParallelIteratorFunc func,
                                  const bool use_threads,
                                  const bool check_num_items_tmp)
{
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = use_threads;

  double averaged_timing = 0.0;
  for (int i = 0; i < NUM_RUN_AVERAGED; i++) {
    const double init_time = PIL_check_seconds_timer();
    BLI_task_parallel_listbase(list, num_items_tmp, func, &settings);
    averaged_timing += PIL_check_seconds_timer() - init_time;

    /* Those checks should ensure us all items of the listbase were processed once, and only once -
     * as expected. */
    if (check_num_items_tmp) {
      EXPECT_EQ(*num_items_tmp, 0);
    }
    LinkData *item;
    int j;
    for (j = 0, item = (LinkData *)list->first; j < num_items && item != NULL;
         j++, item = item->next) {
      EXPECT_EQ(POINTER_AS_INT(item->data), j);
      item->data = POINTER_FROM_INT(0);
    }
    EXPECT_EQ(num_items, j);

    *num_items_tmp = num_items;
  }

  printf("\t%s: done in %fs on average over %d runs\n",
         id,
         averaged_timing / NUM_RUN_AVERAGED,
         NUM_RUN_AVERAGED);
}

static void task_listbase_test(const char *id, const int nbr, const bool use_threads)
{
  printf("\n========== STARTING %s ==========\n", id);

  ListBase list = {NULL, NULL};
  LinkData *items_buffer = (LinkData *)MEM_calloc_arrayN(nbr, sizeof(*items_buffer), __func__);

  BLI_threadapi_init();

  int num_items = 0;
  for (int i = 0; i < nbr; i++) {
    BLI_addtail(&list, &items_buffer[i]);
    num_items++;
  }
  int num_items_tmp = num_items;

  task_listbase_test_do(&list,
                        num_items,
                        &num_items_tmp,
                        "Light iter",
                        task_listbase_light_iter_func,
                        use_threads,
                        false);

  task_listbase_test_do(&list,
                        num_items,
                        &num_items_tmp,
                        "Light iter with mem barrier",
                        task_listbase_light_membarrier_iter_func,
                        use_threads,
                        true);

  task_listbase_test_do(&list,
                        num_items,
                        &num_items_tmp,
                        "Heavy iter",
                        task_listbase_heavy_iter_func,
                        use_threads,
                        false);

  task_listbase_test_do(&list,
                        num_items,
                        &num_items_tmp,
                        "Heavy iter with mem barrier",
                        task_listbase_heavy_membarrier_iter_func,
                        use_threads,
                        true);

  MEM_freeN(items_buffer);
  BLI_threadapi_exit();

  printf("========== ENDED %s ==========\n\n", id);
}

TEST(task, ListBaseIterNoThread10k)
{
  task_listbase_test("ListBase parallel iteration - Single thread - 10000 items", 10000, false);
}

TEST(task, ListBaseIter10k)
{
  task_listbase_test("ListBase parallel iteration - Threaded - 10000 items", 10000, true);
}

TEST(task, ListBaseIterNoThread100k)
{
  task_listbase_test("ListBase parallel iteration - Single thread - 100000 items", 100000, false);
}

TEST(task, ListBaseIter100k)
{
  task_listbase_test("ListBase parallel iteration - Threaded - 100000 items", 100000, true);
}
