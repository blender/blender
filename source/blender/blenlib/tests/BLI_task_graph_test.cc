/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_task.h"

struct TaskData {
  int value;
  int store;
};

static void TaskData_increase_value(void *taskdata)
{
  TaskData *data = (TaskData *)taskdata;
  data->value += 1;
}
static void TaskData_decrease_value(void *taskdata)
{
  TaskData *data = (TaskData *)taskdata;
  data->value -= 1;
}
static void TaskData_multiply_by_two_value(void *taskdata)
{
  TaskData *data = (TaskData *)taskdata;
  data->value *= 2;
}

static void TaskData_multiply_by_two_store(void *taskdata)
{
  TaskData *data = (TaskData *)taskdata;
  data->store *= 2;
}

static void TaskData_store_value(void *taskdata)
{
  TaskData *data = (TaskData *)taskdata;
  data->store = data->value;
}

static void TaskData_square_value(void *taskdata)
{
  TaskData *data = (TaskData *)taskdata;
  data->value *= data->value;
}

/* Sequential Test for using `BLI_task_graph` */
TEST(task, GraphSequential)
{
  TaskData data = {0};
  TaskGraph *graph = BLI_task_graph_create();

  /* 0 => 1 */
  TaskNode *node_a = BLI_task_graph_node_create(graph, TaskData_increase_value, &data, nullptr);
  /* 1 => 2 */
  TaskNode *node_b = BLI_task_graph_node_create(
      graph, TaskData_multiply_by_two_value, &data, nullptr);
  /* 2 => 1 */
  TaskNode *node_c = BLI_task_graph_node_create(graph, TaskData_decrease_value, &data, nullptr);
  /* 2 => 1 */
  TaskNode *node_d = BLI_task_graph_node_create(graph, TaskData_square_value, &data, nullptr);
  /* 1 => 1 */
  TaskNode *node_e = BLI_task_graph_node_create(graph, TaskData_increase_value, &data, nullptr);
  /* 1 => 2 */
  const int expected_value = 2;

  BLI_task_graph_edge_create(node_a, node_b);
  BLI_task_graph_edge_create(node_b, node_c);
  BLI_task_graph_edge_create(node_c, node_d);
  BLI_task_graph_edge_create(node_d, node_e);

  EXPECT_TRUE(BLI_task_graph_node_push_work(node_a));
  BLI_task_graph_work_and_wait(graph);

  EXPECT_EQ(expected_value, data.value);
  BLI_task_graph_free(graph);
}

TEST(task, GraphStartAtAnyNode)
{
  TaskData data = {4};
  TaskGraph *graph = BLI_task_graph_create();

  TaskNode *node_a = BLI_task_graph_node_create(graph, TaskData_increase_value, &data, nullptr);
  TaskNode *node_b = BLI_task_graph_node_create(
      graph, TaskData_multiply_by_two_value, &data, nullptr);
  TaskNode *node_c = BLI_task_graph_node_create(graph, TaskData_decrease_value, &data, nullptr);
  TaskNode *node_d = BLI_task_graph_node_create(graph, TaskData_square_value, &data, nullptr);
  TaskNode *node_e = BLI_task_graph_node_create(graph, TaskData_increase_value, &data, nullptr);

  // ((4 - 1) * (4 - 1)) + 1
  const int expected_value = 10;

  BLI_task_graph_edge_create(node_a, node_b);
  BLI_task_graph_edge_create(node_b, node_c);
  BLI_task_graph_edge_create(node_c, node_d);
  BLI_task_graph_edge_create(node_d, node_e);

  EXPECT_TRUE(BLI_task_graph_node_push_work(node_c));
  BLI_task_graph_work_and_wait(graph);

  EXPECT_EQ(expected_value, data.value);
  BLI_task_graph_free(graph);
}

TEST(task, GraphSplit)
{
  TaskData data = {1};

  TaskGraph *graph = BLI_task_graph_create();
  TaskNode *node_a = BLI_task_graph_node_create(graph, TaskData_increase_value, &data, nullptr);
  TaskNode *node_b = BLI_task_graph_node_create(graph, TaskData_store_value, &data, nullptr);
  TaskNode *node_c = BLI_task_graph_node_create(graph, TaskData_increase_value, &data, nullptr);
  TaskNode *node_d = BLI_task_graph_node_create(
      graph, TaskData_multiply_by_two_store, &data, nullptr);
  BLI_task_graph_edge_create(node_a, node_b);
  BLI_task_graph_edge_create(node_b, node_c);
  BLI_task_graph_edge_create(node_b, node_d);
  EXPECT_TRUE(BLI_task_graph_node_push_work(node_a));
  BLI_task_graph_work_and_wait(graph);

  EXPECT_EQ(3, data.value);
  EXPECT_EQ(4, data.store);
  BLI_task_graph_free(graph);
}

TEST(task, GraphForest)
{
  TaskData data1 = {1};
  TaskData data2 = {3};

  TaskGraph *graph = BLI_task_graph_create();

  {
    TaskNode *tree1_node_a = BLI_task_graph_node_create(
        graph, TaskData_increase_value, &data1, nullptr);
    TaskNode *tree1_node_b = BLI_task_graph_node_create(
        graph, TaskData_store_value, &data1, nullptr);
    TaskNode *tree1_node_c = BLI_task_graph_node_create(
        graph, TaskData_increase_value, &data1, nullptr);
    TaskNode *tree1_node_d = BLI_task_graph_node_create(
        graph, TaskData_multiply_by_two_store, &data1, nullptr);
    BLI_task_graph_edge_create(tree1_node_a, tree1_node_b);
    BLI_task_graph_edge_create(tree1_node_b, tree1_node_c);
    BLI_task_graph_edge_create(tree1_node_b, tree1_node_d);
    EXPECT_TRUE(BLI_task_graph_node_push_work(tree1_node_a));
  }

  {
    TaskNode *tree2_node_a = BLI_task_graph_node_create(
        graph, TaskData_increase_value, &data2, nullptr);
    TaskNode *tree2_node_b = BLI_task_graph_node_create(
        graph, TaskData_store_value, &data2, nullptr);
    TaskNode *tree2_node_c = BLI_task_graph_node_create(
        graph, TaskData_increase_value, &data2, nullptr);
    TaskNode *tree2_node_d = BLI_task_graph_node_create(
        graph, TaskData_multiply_by_two_store, &data2, nullptr);
    BLI_task_graph_edge_create(tree2_node_a, tree2_node_b);
    BLI_task_graph_edge_create(tree2_node_b, tree2_node_c);
    BLI_task_graph_edge_create(tree2_node_b, tree2_node_d);
    EXPECT_TRUE(BLI_task_graph_node_push_work(tree2_node_a));
  }

  BLI_task_graph_work_and_wait(graph);

  EXPECT_EQ(3, data1.value);
  EXPECT_EQ(4, data1.store);
  EXPECT_EQ(5, data2.value);
  EXPECT_EQ(8, data2.store);
  BLI_task_graph_free(graph);
}

TEST(task, GraphTaskData)
{
  TaskData data = {0};
  TaskGraph *graph = BLI_task_graph_create();
  TaskNode *node_a = BLI_task_graph_node_create(
      graph, TaskData_store_value, &data, TaskData_increase_value);
  TaskNode *node_b = BLI_task_graph_node_create(graph, TaskData_store_value, &data, nullptr);
  BLI_task_graph_edge_create(node_a, node_b);
  EXPECT_TRUE(BLI_task_graph_node_push_work(node_a));
  BLI_task_graph_work_and_wait(graph);
  EXPECT_EQ(0, data.value);
  EXPECT_EQ(0, data.store);
  BLI_task_graph_free(graph);
  /* data should be freed once */
  EXPECT_EQ(1, data.value);
  EXPECT_EQ(0, data.store);
}
