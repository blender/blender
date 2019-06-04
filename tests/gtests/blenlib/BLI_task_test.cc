/* Apache License, Version 2.0 */

#include "testing/testing.h"
#include <string.h>

#include "atomic_ops.h"

extern "C" {
#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_task.h"

#include "MEM_guardedalloc.h"
};

#define NUM_ITEMS 10000

/* *** Parallel iterations over mempool items. *** */

static void task_mempool_iter_func(void *userdata, MempoolIterData *item)
{
  int *data = (int *)item;
  int *count = (int *)userdata;

  EXPECT_TRUE(data != NULL);

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

  /* 'Randomly' add and remove some items from mempool, to create a non-homogenous one. */
  int num_items = 0;
  for (i = 0; i < NUM_ITEMS; i++) {
    data[i] = (int *)BLI_mempool_alloc(mempool);
    *data[i] = i - 1;
    num_items++;
  }

  for (i = 0; i < NUM_ITEMS; i += 3) {
    BLI_mempool_free(mempool, data[i]);
    data[i] = NULL;
    num_items--;
  }

  for (i = 0; i < NUM_ITEMS; i += 7) {
    if (data[i] == NULL) {
      data[i] = (int *)BLI_mempool_alloc(mempool);
      *data[i] = i - 1;
      num_items++;
    }
  }

  for (i = 0; i < NUM_ITEMS - 5; i += 23) {
    for (int j = 0; j < 5; j++) {
      if (data[i + j] != NULL) {
        BLI_mempool_free(mempool, data[i + j]);
        data[i + j] = NULL;
        num_items--;
      }
    }
  }

  BLI_task_parallel_mempool(mempool, &num_items, task_mempool_iter_func, true);

  /* Those checks should ensure us all items of the mempool were processed once, and only once - as
   * expected. */
  EXPECT_EQ(num_items, 0);
  for (i = 0; i < NUM_ITEMS; i++) {
    if (data[i] != NULL) {
      EXPECT_EQ(*data[i], i);
    }
  }

  BLI_mempool_destroy(mempool);
  BLI_threadapi_exit();
}

/* *** Parallel iterations over double-linked list items. *** */

static void task_listbase_iter_func(void *userdata, Link *item, int index)
{
  LinkData *data = (LinkData *)item;
  int *count = (int *)userdata;

  data->data = POINTER_FROM_INT(POINTER_AS_INT(data->data) + index);
  atomic_sub_and_fetch_uint32((uint32_t *)count, 1);
}

TEST(task, ListBaseIter)
{
  ListBase list = {NULL, NULL};
  LinkData *items_buffer = (LinkData *)MEM_calloc_arrayN(
      NUM_ITEMS, sizeof(*items_buffer), __func__);
  BLI_threadapi_init();

  int i;

  int num_items = 0;
  for (i = 0; i < NUM_ITEMS; i++) {
    BLI_addtail(&list, &items_buffer[i]);
    num_items++;
  }

  BLI_task_parallel_listbase(&list, &num_items, task_listbase_iter_func, true);

  /* Those checks should ensure us all items of the listbase were processed once, and only once -
   * as expected. */
  EXPECT_EQ(num_items, 0);
  LinkData *item;
  for (i = 0, item = (LinkData *)list.first; i < NUM_ITEMS && item != NULL;
       i++, item = item->next) {
    EXPECT_EQ(POINTER_AS_INT(item->data), i);
  }
  EXPECT_EQ(NUM_ITEMS, i);

  MEM_freeN(items_buffer);
  BLI_threadapi_exit();
}
