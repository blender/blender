/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_linklist_lockfree.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

TEST(LockfreeLinkList, Init)
{
  LockfreeLinkList list;
  BLI_linklist_lockfree_init(&list);
  EXPECT_EQ(list.head, &list.dummy_node);
  EXPECT_EQ(list.tail, &list.dummy_node);
  BLI_linklist_lockfree_free(&list, nullptr);
}

TEST(LockfreeLinkList, InsertSingle)
{
  LockfreeLinkList list;
  LockfreeLinkNode node;
  BLI_linklist_lockfree_init(&list);
  BLI_linklist_lockfree_insert(&list, &node);
  EXPECT_EQ(list.head, &list.dummy_node);
  EXPECT_EQ(list.head->next, &node);
  EXPECT_EQ(list.tail, &node);
  BLI_linklist_lockfree_free(&list, nullptr);
}

TEST(LockfreeLinkList, InsertMultiple)
{
  static const int nodes_num = 128;
  LockfreeLinkList list;
  LockfreeLinkNode nodes[nodes_num];
  BLI_linklist_lockfree_init(&list);
  /* Insert all the nodes. */
  for (LockfreeLinkNode &node : nodes) {
    BLI_linklist_lockfree_insert(&list, &node);
  }
  /* Check head and tail. */
  EXPECT_EQ(list.head, &list.dummy_node);
  EXPECT_EQ(list.tail, &nodes[nodes_num - 1]);
  /* Check rest of the nodes. */
  int node_index = 0;
  for (LockfreeLinkNode *node = BLI_linklist_lockfree_begin(&list); node != nullptr;
       node = node->next, ++node_index)
  {
    EXPECT_EQ(node, &nodes[node_index]);
    if (node_index != nodes_num - 1) {
      EXPECT_EQ(node->next, &nodes[node_index + 1]);
    }
  }
  /* Free list. */
  BLI_linklist_lockfree_free(&list, nullptr);
}

namespace {

struct IndexedNode {
  IndexedNode *next;
  int index;
};

void concurrent_insert(TaskPool *__restrict pool, void *taskdata)
{
  LockfreeLinkList *list = (LockfreeLinkList *)BLI_task_pool_user_data(pool);
  CHECK_NOTNULL(list);
  IndexedNode *node = (IndexedNode *)MEM_mallocN(sizeof(IndexedNode), "test node");
  node->index = POINTER_AS_INT(taskdata);
  BLI_linklist_lockfree_insert(list, (LockfreeLinkNode *)node);
}

}  // namespace

TEST(LockfreeLinkList, InsertMultipleConcurrent)
{
  static const int nodes_num = 655360;
  /* Initialize list. */
  LockfreeLinkList list;
  BLI_linklist_lockfree_init(&list);
  /* Initialize task scheduler and pool. */
  TaskPool *pool = BLI_task_pool_create_suspended(&list, TASK_PRIORITY_HIGH);
  /* Push tasks to the pool. */
  for (int i = 0; i < nodes_num; ++i) {
    BLI_task_pool_push(pool, concurrent_insert, POINTER_FROM_INT(i), false, nullptr);
  }
  /* Run all the tasks. */
  BLI_task_pool_work_and_wait(pool);
  /* Verify we've got all the data properly inserted. */
  EXPECT_EQ(list.head, &list.dummy_node);
  bool *visited_nodes = (bool *)MEM_callocN(sizeof(bool) * nodes_num, "visited nodes");
  /* First, we make sure that none of the nodes are added twice. */
  for (LockfreeLinkNode *node_v = BLI_linklist_lockfree_begin(&list); node_v != nullptr;
       node_v = node_v->next)
  {
    IndexedNode *node = (IndexedNode *)node_v;
    EXPECT_GE(node->index, 0);
    EXPECT_LT(node->index, nodes_num);
    EXPECT_FALSE(visited_nodes[node->index]);
    visited_nodes[node->index] = true;
  }
  /* Then we make sure node was added. */
  for (int node_index = 0; node_index < nodes_num; ++node_index) {
    EXPECT_TRUE(visited_nodes[node_index]);
  }
  MEM_freeN(visited_nodes);
  /* Cleanup data. */
  BLI_linklist_lockfree_free(&list, MEM_freeN);
  BLI_task_pool_free(pool);
}
