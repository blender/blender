/* Apache License, Version 2.0 */

#include "testing/testing.h"
#include "BLI_ressource_strings.h"

#include "atomic_ops.h"

#define GHASH_INTERNAL_API

extern "C" {
#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_task.h"

#include "PIL_time.h"

#include "MEM_guardedalloc.h"
}

/* *** Parallel iterations over double-linked list items. *** */

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

static void task_listbase_light_iter_func(void *UNUSED(userdata), Link *item, int index)
{
  LinkData *data = (LinkData *)item;

  data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
}

static void task_listbase_light_membarrier_iter_func(void *userdata, Link *item, int index)
{
  LinkData *data = (LinkData *)item;
  int *count = (int *)userdata;

  data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
  atomic_sub_and_fetch_uint32((uint32_t *)count, 1);
}

static void task_listbase_heavy_iter_func(void *UNUSED(userdata), Link *item, int index)
{
  LinkData *data = (LinkData *)item;

  /* 'Random' number of iterations. */
  const uint num = gen_pseudo_random_number((uint)index);

  for (uint i = 0; i < num; i++) {
    data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + ((i % 2) ? -index : index));
  }
}

static void task_listbase_heavy_membarrier_iter_func(void *userdata, Link *item, int index)
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
                                  TaskParallelListbaseFunc func,
                                  const bool use_threads,
                                  const bool check_num_items_tmp)
{
  double averaged_timing = 0.0;
  for (int i = 0; i < NUM_RUN_AVERAGED; i++) {
    const double init_time = PIL_check_seconds_timer();
    BLI_task_parallel_listbase(list, num_items_tmp, func, use_threads);
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
