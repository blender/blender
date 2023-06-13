/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_inplace_priority_queue.hh"
#include "BLI_rand.hh"

namespace blender::tests {

TEST(inplace_priority_queue, BuildSmall)
{
  Array<int> values = {1, 5, 2, 8, 5, 6, 5, 4, 3, 6, 7, 3};
  InplacePriorityQueue<int> priority_queue{values};

  EXPECT_EQ(priority_queue.peek(), 8);
  EXPECT_EQ(priority_queue.pop(), 8);
  EXPECT_EQ(priority_queue.peek(), 7);
  EXPECT_EQ(priority_queue.pop(), 7);
  EXPECT_EQ(priority_queue.pop(), 6);
  EXPECT_EQ(priority_queue.pop(), 6);
  EXPECT_EQ(priority_queue.pop(), 5);
}

TEST(inplace_priority_queue, DecreasePriority)
{
  Array<int> values = {5, 2, 7, 4};
  InplacePriorityQueue<int> priority_queue(values);

  EXPECT_EQ(priority_queue.peek(), 7);
  values[2] = 0;
  EXPECT_EQ(priority_queue.peek(), 0);
  priority_queue.priority_decreased(2);
  EXPECT_EQ(priority_queue.peek(), 5);
}

TEST(inplace_priority_queue, IncreasePriority)
{
  Array<int> values = {5, 2, 7, 4};
  InplacePriorityQueue<int> priority_queue(values);

  EXPECT_EQ(priority_queue.peek(), 7);
  values[1] = 10;
  EXPECT_EQ(priority_queue.peek(), 7);
  priority_queue.priority_increased(1);
  EXPECT_EQ(priority_queue.peek(), 10);
}

TEST(inplace_priority_queue, PopAll)
{
  RandomNumberGenerator rng;
  Vector<int> values;
  const int amount = 1000;
  for (int i = 0; i < amount; i++) {
    values.append(rng.get_int32() % amount);
  }

  InplacePriorityQueue<int> priority_queue(values);

  int last_value = amount;
  while (!priority_queue.is_empty()) {
    const int value = priority_queue.pop();
    EXPECT_LE(value, last_value);
    last_value = value;
  }
}

TEST(inplace_priority_queue, ManyPriorityChanges)
{
  RandomNumberGenerator rng;
  Vector<int> values;
  const int amount = 1000;
  for (int i = 0; i < amount; i++) {
    values.append(rng.get_int32() % amount);
  }

  InplacePriorityQueue<int> priority_queue(values);

  for (int i = 0; i < amount; i++) {
    const int index = rng.get_int32() % amount;
    const int new_priority = rng.get_int32() % amount;
    values[index] = new_priority;
    priority_queue.priority_changed(index);
  }

  int last_value = amount;
  while (!priority_queue.is_empty()) {
    const int value = priority_queue.pop();
    EXPECT_LE(value, last_value);
    last_value = value;
  }
}

TEST(inplace_priority_queue, IndicesAccess)
{
  Array<int> values = {4, 6, 2, 4, 8, 1, 10, 2, 5};
  InplacePriorityQueue<int> priority_queue(values);

  EXPECT_EQ(priority_queue.active_indices().size(), 9);
  EXPECT_EQ(priority_queue.inactive_indices().size(), 0);
  EXPECT_EQ(priority_queue.all_indices().size(), 9);
  EXPECT_EQ(priority_queue.pop(), 10);
  EXPECT_EQ(priority_queue.active_indices().size(), 8);
  EXPECT_EQ(priority_queue.inactive_indices().size(), 1);
  EXPECT_EQ(values[priority_queue.inactive_indices()[0]], 10);
  EXPECT_EQ(priority_queue.all_indices().size(), 9);
  EXPECT_EQ(priority_queue.pop(), 8);
  EXPECT_EQ(priority_queue.inactive_indices().size(), 2);
  EXPECT_EQ(values[priority_queue.inactive_indices()[0]], 8);
  EXPECT_EQ(values[priority_queue.inactive_indices()[1]], 10);
  EXPECT_EQ(priority_queue.all_indices().size(), 9);
}

}  // namespace blender::tests
