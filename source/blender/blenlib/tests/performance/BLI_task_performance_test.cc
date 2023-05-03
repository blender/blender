/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_ressource_strings.h"
#include "testing/testing.h"

#include "atomic_ops.h"

#define GHASH_INTERNAL_API

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_task.h"

#include "PIL_time.h"

#define NUM_RUN_AVERAGED 100

static uint gen_pseudo_random_number(uint num)
{
  /* NOTE: this is taken from BLI_ghashutil_uinthash(), don't want to depend on external code that
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

/* *** Parallel iterations over double-linked list items. *** */

static void task_listbase_light_iter_func(void * /*userdata*/,
                                          void *item,
                                          int index,
                                          const TaskParallelTLS *__restrict /*tls*/)

{
  LinkData *data = (LinkData *)item;

  data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
}

static void task_listbase_light_membarrier_iter_func(void *userdata,
                                                     void *item,
                                                     int index,
                                                     const TaskParallelTLS *__restrict /*tls*/)

{
  LinkData *data = (LinkData *)item;
  int *count = (int *)userdata;

  data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
  atomic_sub_and_fetch_uint32((uint32_t *)count, 1);
}

static void task_listbase_heavy_iter_func(void * /*userdata*/,
                                          void *item,
                                          int index,
                                          const TaskParallelTLS *__restrict /*tls*/)

{
  LinkData *data = (LinkData *)item;

  /* 'Random' number of iterations. */
  const uint num = gen_pseudo_random_number(uint(index));

  for (uint i = 0; i < num; i++) {
    data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + ((i % 2) ? -index : index));
  }
}

static void task_listbase_heavy_membarrier_iter_func(void *userdata,
                                                     void *item,
                                                     int index,
                                                     const TaskParallelTLS *__restrict /*tls*/)

{
  LinkData *data = (LinkData *)item;
  int *count = (int *)userdata;

  /* 'Random' number of iterations. */
  const uint num = gen_pseudo_random_number(uint(index));

  for (uint i = 0; i < num; i++) {
    data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + ((i % 2) ? -index : index));
  }
  atomic_sub_and_fetch_uint32((uint32_t *)count, 1);
}

static void task_listbase_test_do(ListBase *list,
                                  const int items_num,
                                  int *items_tmp_num,
                                  const char *id,
                                  TaskParallelIteratorFunc func,
                                  const bool use_threads,
                                  const bool check_items_tmp_num)
{
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = use_threads;

  double averaged_timing = 0.0;
  for (int i = 0; i < NUM_RUN_AVERAGED; i++) {
    const double init_time = PIL_check_seconds_timer();
    BLI_task_parallel_listbase(list, items_tmp_num, func, &settings);
    averaged_timing += PIL_check_seconds_timer() - init_time;

    /* Those checks should ensure us all items of the listbase were processed once, and only once -
     * as expected. */
    if (check_items_tmp_num) {
      EXPECT_EQ(*items_tmp_num, 0);
    }
    LinkData *item;
    int j;
    for (j = 0, item = (LinkData *)list->first; j < items_num && item != nullptr;
         j++, item = item->next)
    {
      EXPECT_EQ(POINTER_AS_INT(item->data), j);
      item->data = POINTER_FROM_INT(0);
    }
    EXPECT_EQ(items_num, j);

    *items_tmp_num = items_num;
  }

  printf("\t%s: done in %fs on average over %d runs\n",
         id,
         averaged_timing / NUM_RUN_AVERAGED,
         NUM_RUN_AVERAGED);
}

static void task_listbase_test(const char *id, const int count, const bool use_threads)
{
  printf("\n========== STARTING %s ==========\n", id);

  ListBase list = {nullptr, nullptr};
  LinkData *items_buffer = (LinkData *)MEM_calloc_arrayN(count, sizeof(*items_buffer), __func__);

  BLI_threadapi_init();

  int items_num = 0;
  for (int i = 0; i < count; i++) {
    BLI_addtail(&list, &items_buffer[i]);
    items_num++;
  }
  int items_tmp_num = items_num;

  task_listbase_test_do(&list,
                        items_num,
                        &items_tmp_num,
                        "Light iter",
                        task_listbase_light_iter_func,
                        use_threads,
                        false);

  task_listbase_test_do(&list,
                        items_num,
                        &items_tmp_num,
                        "Light iter with mem barrier",
                        task_listbase_light_membarrier_iter_func,
                        use_threads,
                        true);

  task_listbase_test_do(&list,
                        items_num,
                        &items_tmp_num,
                        "Heavy iter",
                        task_listbase_heavy_iter_func,
                        use_threads,
                        false);

  task_listbase_test_do(&list,
                        items_num,
                        &items_tmp_num,
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
