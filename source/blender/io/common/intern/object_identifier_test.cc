/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */
#include "IO_abstract_hierarchy_iterator.h"

#include "testing/testing.h"

#include "BLI_utildefines.h"

#include <climits>

namespace blender::io {

namespace {

/* Return object pointer for use in tests. This makes it possible to reliably test for
 * order/equality functions while using hard-coded values for simplicity. */
Object *fake_pointer(int value)
{
  return static_cast<Object *>(POINTER_FROM_INT(value));
}

/* PersistentID subclass for use in tests, making it easier to construct test values. */
class TestPersistentID : public PersistentID {
 public:
  TestPersistentID(int value0,
                   int value1,
                   int value2,
                   int value3,
                   int value4,
                   int value5,
                   int value6,
                   int value7)
  {
    persistent_id_[0] = value0;
    persistent_id_[1] = value1;
    persistent_id_[2] = value2;
    persistent_id_[3] = value3;
    persistent_id_[4] = value4;
    persistent_id_[5] = value5;
    persistent_id_[6] = value6;
    persistent_id_[7] = value7;
  }
  TestPersistentID(int value0, int value1, int value2)
      : TestPersistentID(value0, value1, value2, INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX)
  {
  }
  TestPersistentID(int value0, int value1) : TestPersistentID(value0, value1, INT_MAX) {}
  explicit TestPersistentID(int value0) : TestPersistentID(value0, INT_MAX) {}
};

/* ObjectIdentifier subclass for use in tests, making it easier to construct test values. */
class TestObjectIdentifier : public ObjectIdentifier {
 public:
  TestObjectIdentifier(Object *object, Object *duplicated_by, const PersistentID &persistent_id)
      : ObjectIdentifier(object, duplicated_by, persistent_id)
  {
  }
};

}  // namespace

class ObjectIdentifierOrderTest : public testing::Test {
};

TEST_F(ObjectIdentifierOrderTest, graph_root)
{
  ObjectIdentifier id_root_1 = ObjectIdentifier::for_graph_root();
  ObjectIdentifier id_root_2 = ObjectIdentifier::for_graph_root();
  EXPECT_TRUE(id_root_1 == id_root_2);
  EXPECT_FALSE(id_root_1 < id_root_2);
  EXPECT_FALSE(id_root_2 < id_root_1);

  ObjectIdentifier id_a = ObjectIdentifier::for_real_object(fake_pointer(1));
  EXPECT_FALSE(id_root_1 == id_a);
  EXPECT_TRUE(id_root_1 < id_a);
  EXPECT_FALSE(id_a < id_root_1);

  ObjectIdentifier id_accidental_root = ObjectIdentifier::for_real_object(nullptr);
  EXPECT_TRUE(id_root_1 == id_accidental_root);
  EXPECT_FALSE(id_root_1 < id_accidental_root);
  EXPECT_FALSE(id_accidental_root < id_root_1);
}

TEST_F(ObjectIdentifierOrderTest, real_objects)
{
  ObjectIdentifier id_a = ObjectIdentifier::for_real_object(fake_pointer(1));
  ObjectIdentifier id_b = ObjectIdentifier::for_real_object(fake_pointer(2));
  EXPECT_FALSE(id_a == id_b);
  EXPECT_TRUE(id_a < id_b);
}

TEST_F(ObjectIdentifierOrderTest, duplicated_objects)
{
  ObjectIdentifier id_real_a = ObjectIdentifier::for_real_object(fake_pointer(1));
  TestObjectIdentifier id_dupli_a(fake_pointer(1), fake_pointer(2), TestPersistentID(0));
  TestObjectIdentifier id_dupli_b(fake_pointer(1), fake_pointer(3), TestPersistentID(0));
  TestObjectIdentifier id_different_dupli_b(fake_pointer(1), fake_pointer(3), TestPersistentID(1));

  EXPECT_FALSE(id_real_a == id_dupli_a);
  EXPECT_FALSE(id_dupli_a == id_dupli_b);
  EXPECT_TRUE(id_real_a < id_dupli_a);
  EXPECT_TRUE(id_real_a < id_dupli_b);
  EXPECT_TRUE(id_dupli_a < id_dupli_b);
  EXPECT_TRUE(id_dupli_a < id_different_dupli_b);

  EXPECT_FALSE(id_dupli_b == id_different_dupli_b);
  EXPECT_FALSE(id_dupli_a == id_different_dupli_b);
  EXPECT_TRUE(id_dupli_b < id_different_dupli_b);
  EXPECT_FALSE(id_different_dupli_b < id_dupli_b);
}

TEST_F(ObjectIdentifierOrderTest, behavior_as_map_keys)
{
  ObjectIdentifier id_root = ObjectIdentifier::for_graph_root();
  ObjectIdentifier id_another_root = ObjectIdentifier::for_graph_root();
  ObjectIdentifier id_real_a = ObjectIdentifier::for_real_object(fake_pointer(1));
  TestObjectIdentifier id_dupli_a(fake_pointer(1), fake_pointer(2), TestPersistentID(0));
  TestObjectIdentifier id_dupli_b(fake_pointer(1), fake_pointer(3), TestPersistentID(0));
  AbstractHierarchyIterator::ExportGraph graph;

  /* This inserts the keys with default values. */
  graph[id_root];
  graph[id_real_a];
  graph[id_dupli_a];
  graph[id_dupli_b];
  graph[id_another_root];

  EXPECT_EQ(4, graph.size());

  graph.erase(id_another_root);
  EXPECT_EQ(3, graph.size());

  TestObjectIdentifier id_another_dupli_b(fake_pointer(1), fake_pointer(3), TestPersistentID(0));
  graph.erase(id_another_dupli_b);
  EXPECT_EQ(2, graph.size());
}

TEST_F(ObjectIdentifierOrderTest, map_copy_and_update)
{
  ObjectIdentifier id_root = ObjectIdentifier::for_graph_root();
  ObjectIdentifier id_real_a = ObjectIdentifier::for_real_object(fake_pointer(1));
  TestObjectIdentifier id_dupli_a(fake_pointer(1), fake_pointer(2), TestPersistentID(0));
  TestObjectIdentifier id_dupli_b(fake_pointer(1), fake_pointer(3), TestPersistentID(0));
  TestObjectIdentifier id_dupli_c(fake_pointer(1), fake_pointer(3), TestPersistentID(1));
  AbstractHierarchyIterator::ExportGraph graph;

  /* This inserts the keys with default values. */
  graph[id_root];
  graph[id_real_a];
  graph[id_dupli_a];
  graph[id_dupli_b];
  graph[id_dupli_c];
  EXPECT_EQ(5, graph.size());

  AbstractHierarchyIterator::ExportGraph graph_copy = graph;
  EXPECT_EQ(5, graph_copy.size());

  /* Updating a value in a copy should not update the original. */
  HierarchyContext ctx1;
  HierarchyContext ctx2;
  ctx1.object = fake_pointer(1);
  ctx2.object = fake_pointer(2);

  graph_copy[id_root].insert(&ctx1);
  EXPECT_EQ(0, graph[id_root].size());

  /* Deleting a key in the copy should not update the original. */
  graph_copy.erase(id_dupli_c);
  EXPECT_EQ(4, graph_copy.size());
  EXPECT_EQ(5, graph.size());
}

class PersistentIDTest : public testing::Test {
};

TEST_F(PersistentIDTest, is_from_same_instancer)
{
  PersistentID child_id_a = TestPersistentID(42, 327);
  PersistentID child_id_b = TestPersistentID(17, 327);
  PersistentID child_id_c = TestPersistentID(17);

  EXPECT_TRUE(child_id_a.is_from_same_instancer_as(child_id_b));
  EXPECT_FALSE(child_id_a.is_from_same_instancer_as(child_id_c));
}

TEST_F(PersistentIDTest, instancer_id)
{
  PersistentID child_id = TestPersistentID(42, 327);

  PersistentID expect_instancer_id = TestPersistentID(327);
  EXPECT_EQ(expect_instancer_id, child_id.instancer_pid());

  PersistentID empty_id;
  EXPECT_EQ(empty_id, child_id.instancer_pid().instancer_pid());

  EXPECT_LT(child_id, expect_instancer_id);
  EXPECT_LT(expect_instancer_id, empty_id);
}

TEST_F(PersistentIDTest, as_object_name_suffix)
{
  EXPECT_EQ("", PersistentID().as_object_name_suffix());
  EXPECT_EQ("47", TestPersistentID(47).as_object_name_suffix());
  EXPECT_EQ("327-47", TestPersistentID(47, 327).as_object_name_suffix());
  EXPECT_EQ("42-327-47", TestPersistentID(47, 327, 42).as_object_name_suffix());

  EXPECT_EQ("7-6-5-4-3-2-1-0", TestPersistentID(0, 1, 2, 3, 4, 5, 6, 7).as_object_name_suffix());

  EXPECT_EQ("0-0-0", TestPersistentID(0, 0, 0).as_object_name_suffix());
  EXPECT_EQ("0-0", TestPersistentID(0, 0).as_object_name_suffix());
  EXPECT_EQ("-3--2--1", TestPersistentID(-1, -2, -3).as_object_name_suffix());
}

}  // namespace blender::io
