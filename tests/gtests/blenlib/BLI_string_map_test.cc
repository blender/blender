#include "BLI_string_map.h"
#include "BLI_vector.h"
#include "testing/testing.h"

using namespace BLI;

TEST(string_map, DefaultConstructor)
{
  StringMap<int> map;
  EXPECT_EQ(map.size(), 0);
}

TEST(string_map, CopyConstructor)
{
  StringMap<Vector<int, 4>> map1;
  map1.add_new("A", {1, 2, 3});
  map1.add_new("B", {1, 2, 3, 4, 5, 6});

  StringMap<Vector<int>> map2(map1);

  EXPECT_EQ(map1.size(), 2);
  EXPECT_EQ(map2.size(), 2);
  EXPECT_EQ(map1.lookup("A")[1], 2);
  EXPECT_EQ(map2.lookup("A")[1], 2);
  EXPECT_EQ(map1.lookup("B")[5], 6);
  EXPECT_EQ(map2.lookup("B")[5], 6);
}

TEST(string_map, MoveConstructor)
{
  StringMap<Vector<int, 4>> map1;
  map1.add_new("A", {1, 2, 3});
  map1.add_new("B", {1, 2, 3, 4, 5, 6});

  StringMap<Vector<int>> map2(std::move(map1));

  EXPECT_EQ(map1.size(), 0);
  EXPECT_FALSE(map1.contains("A"));
  EXPECT_FALSE(map1.contains("B"));

  EXPECT_EQ(map2.size(), 2);
  EXPECT_EQ(map2.lookup("A")[1], 2);
  EXPECT_EQ(map2.lookup("B")[5], 6);
}

TEST(string_map, Add)
{
  StringMap<int> map;
  EXPECT_EQ(map.size(), 0);

  map.add("test", 1);
  EXPECT_EQ(map.lookup("test"), 1);

  map.add("test", 2);
  EXPECT_EQ(map.lookup("test"), 1);

  map.add("test2", 2);
  EXPECT_EQ(map.lookup("test2"), 2);
}

TEST(string_map, AddNew)
{
  StringMap<int> map;
  EXPECT_EQ(map.size(), 0);

  map.add_new("Why", 5);
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map.lookup("Why"), 5);

  map.add_new("Where", 6);
  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.lookup("Where"), 6);
}

TEST(string_map, AddNew_Many)
{
  StringMap<int> map;

  for (uint i = 0; i < 100; i++) {
    map.add_new(std::to_string(i), i);
  }
  EXPECT_EQ(map.size(), 100);
}

TEST(string_map, Contains)
{
  StringMap<int> map;
  map.add_new("A", 0);
  map.add_new("B", 0);
  EXPECT_TRUE(map.contains("A"));
  EXPECT_TRUE(map.contains("B"));
  EXPECT_FALSE(map.contains("C"));
}

TEST(string_map, Contains_Many)
{
  StringMap<int> map;
  for (uint i = 0; i < 50; i++) {
    map.add_new(std::to_string(i), i);
  }
  for (uint i = 100; i < 200; i++) {
    map.add_new(std::to_string(i), i);
  }
  EXPECT_EQ(map.size(), 150);
  for (uint i = 0; i < 200; i++) {
    if (i < 50 || i >= 100) {
      EXPECT_TRUE(map.contains(std::to_string(i)));
    }
    else {
      EXPECT_FALSE(map.contains(std::to_string(i)));
    }
  }
}

TEST(string_map, Lookup)
{
  StringMap<int> map;
  map.add_new("A", 5);
  map.add_new("B", 8);
  map.add_new("C", 10);
  EXPECT_EQ(map.lookup("A"), 5);
  EXPECT_EQ(map.lookup("B"), 8);
  EXPECT_EQ(map.lookup("C"), 10);
}

TEST(string_map, LookupPtr)
{
  StringMap<int> map;
  map.add_new("test1", 13);
  map.add_new("test2", 14);
  map.add_new("test3", 15);
  EXPECT_EQ(*map.lookup_ptr("test1"), 13);
  EXPECT_EQ(*map.lookup_ptr("test2"), 14);
  EXPECT_EQ(*map.lookup_ptr("test3"), 15);
  EXPECT_EQ(map.lookup_ptr("test4"), nullptr);
}

TEST(string_map, LookupDefault)
{
  StringMap<int> map;
  EXPECT_EQ(map.lookup_default("test", 42), 42);
  map.add_new("test", 5);
  EXPECT_EQ(map.lookup_default("test", 42), 5);
}

TEST(string_map, TryLookup)
{
  StringMap<int> map;
  map.add_new("test", 4);
  EXPECT_TRUE(map.try_lookup("test").has_value());
  EXPECT_FALSE(map.try_lookup("value").has_value());
  EXPECT_EQ(map.try_lookup("test").value(), 4);
}

TEST(string_map, FindKeyForValue)
{
  StringMap<int> map;
  map.add_new("A", 1);
  map.add_new("B", 2);
  map.add_new("C", 3);
  EXPECT_EQ(map.find_key_for_value(1), "A");
  EXPECT_EQ(map.find_key_for_value(2), "B");
  EXPECT_EQ(map.find_key_for_value(3), "C");
}

TEST(string_map, ForeachValue)
{
  StringMap<int> map;
  map.add_new("A", 4);
  map.add_new("B", 5);
  map.add_new("C", 1);

  Vector<int> values;
  map.foreach_value([&values](int &value) { values.append(value); });
  EXPECT_EQ(values.size(), 3);
  EXPECT_TRUE(values.contains(1));
  EXPECT_TRUE(values.contains(4));
  EXPECT_TRUE(values.contains(5));
}

TEST(string_map, ForeachKey)
{
  StringMap<int> map;
  map.add_new("A", 4);
  map.add_new("B", 5);
  map.add_new("C", 1);

  Vector<std::string> keys;
  map.foreach_key([&keys](StringRefNull key) { keys.append(key); });
  EXPECT_EQ(keys.size(), 3);
  EXPECT_TRUE(keys.contains("A"));
  EXPECT_TRUE(keys.contains("B"));
  EXPECT_TRUE(keys.contains("C"));
}

TEST(string_map, ForeachKeyValuePair)
{
  StringMap<int> map;
  map.add_new("A", 4);
  map.add_new("B", 5);
  map.add_new("C", 1);

  Vector<std::string> keys;
  Vector<int> values;

  map.foreach_item([&keys, &values](StringRefNull key, int value) {
    keys.append(key);
    values.append(value);
  });

  EXPECT_EQ(keys.size(), 3);
  EXPECT_EQ(values[keys.index("A")], 4);
  EXPECT_EQ(values[keys.index("B")], 5);
  EXPECT_EQ(values[keys.index("C")], 1);
}

TEST(string_map, WithVectors)
{
  StringMap<Vector<int>> map;
  map.add_new("A", {1, 2, 3});
  map.add_new("B", {1, 2, 3, 4, 5, 6, 7});
  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.lookup("A").size(), 3);
  EXPECT_EQ(map.lookup("B").size(), 7);
}

TEST(string_map, UniquePtrValues)
{
  StringMap<std::unique_ptr<int>> map;
  map.add_new("A", std::unique_ptr<int>(new int()));
  std::unique_ptr<int> &a = map.lookup("A");
  std::unique_ptr<int> *b = map.lookup_ptr("A");
  EXPECT_EQ(a.get(), b->get());
}
