/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <memory>
#include <unordered_map>

#include "BLI_exception_safety_test_utils.hh"
#include "BLI_map.hh"
#include "BLI_rand.h"
#include "BLI_set.hh"
#include "BLI_strict_flags.h"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

#include "testing/testing.h"

namespace blender::tests {

TEST(map, DefaultConstructor)
{
  Map<int, float> map;
  EXPECT_EQ(map.size(), 0);
  EXPECT_TRUE(map.is_empty());
}

TEST(map, AddIncreasesSize)
{
  Map<int, float> map;
  EXPECT_EQ(map.size(), 0);
  EXPECT_TRUE(map.is_empty());
  map.add(2, 5.0f);
  EXPECT_EQ(map.size(), 1);
  EXPECT_FALSE(map.is_empty());
  map.add(6, 2.0f);
  EXPECT_EQ(map.size(), 2);
  EXPECT_FALSE(map.is_empty());
}

TEST(map, Contains)
{
  Map<int, float> map;
  EXPECT_FALSE(map.contains(4));
  map.add(5, 6.0f);
  EXPECT_FALSE(map.contains(4));
  map.add(4, 2.0f);
  EXPECT_TRUE(map.contains(4));
}

TEST(map, LookupExisting)
{
  Map<int, float> map;
  map.add(2, 6.0f);
  map.add(4, 1.0f);
  EXPECT_EQ(map.lookup(2), 6.0f);
  EXPECT_EQ(map.lookup(4), 1.0f);
}

TEST(map, LookupNotExisting)
{
  Map<int, float> map;
  map.add(2, 4.0f);
  map.add(1, 1.0f);
  EXPECT_EQ(map.lookup_ptr(0), nullptr);
  EXPECT_EQ(map.lookup_ptr(5), nullptr);
}

TEST(map, AddMany)
{
  Map<int, int> map;
  for (int i = 0; i < 100; i++) {
    map.add(i * 30, i);
    map.add(i * 31, i);
  }
}

TEST(map, PopItem)
{
  Map<int, float> map;
  map.add(2, 3.0f);
  map.add(1, 9.0f);
  EXPECT_TRUE(map.contains(2));
  EXPECT_TRUE(map.contains(1));

  EXPECT_EQ(map.pop(1), 9.0f);
  EXPECT_TRUE(map.contains(2));
  EXPECT_FALSE(map.contains(1));

  EXPECT_EQ(map.pop(2), 3.0f);
  EXPECT_FALSE(map.contains(2));
  EXPECT_FALSE(map.contains(1));
}

TEST(map, PopTry)
{
  Map<int, int> map;
  map.add(1, 5);
  map.add(2, 7);
  EXPECT_EQ(map.size(), 2);
  std::optional<int> value = map.pop_try(4);
  EXPECT_EQ(map.size(), 2);
  EXPECT_FALSE(value.has_value());
  value = map.pop_try(2);
  EXPECT_EQ(map.size(), 1);
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(*value, 7);
  EXPECT_EQ(*map.pop_try(1), 5);
  EXPECT_EQ(map.size(), 0);
}

TEST(map, PopDefault)
{
  Map<int, int> map;
  map.add(1, 4);
  map.add(2, 7);
  map.add(3, 8);
  EXPECT_EQ(map.size(), 3);
  EXPECT_EQ(map.pop_default(4, 10), 10);
  EXPECT_EQ(map.size(), 3);
  EXPECT_EQ(map.pop_default(1, 10), 4);
  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.pop_default(2, 20), 7);
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map.pop_default(2, 20), 20);
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map.pop_default(3, 0), 8);
  EXPECT_EQ(map.size(), 0);
}

TEST(map, PopItemMany)
{
  Map<int, int> map;
  for (int i = 0; i < 100; i++) {
    map.add_new(i, i);
  }
  for (int i = 25; i < 80; i++) {
    EXPECT_EQ(map.pop(i), i);
  }
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(map.contains(i), i < 25 || i >= 80);
  }
}

TEST(map, ValueIterator)
{
  Map<int, float> map;
  map.add(3, 5.0f);
  map.add(1, 2.0f);
  map.add(7, -2.0f);

  blender::Set<float> values;

  int iterations = 0;
  for (float value : map.values()) {
    values.add(value);
    iterations++;
  }

  EXPECT_EQ(iterations, 3);
  EXPECT_TRUE(values.contains(5.0f));
  EXPECT_TRUE(values.contains(-2.0f));
  EXPECT_TRUE(values.contains(2.0f));
}

TEST(map, KeyIterator)
{
  Map<int, float> map;
  map.add(6, 3.0f);
  map.add(2, 4.0f);
  map.add(1, 3.0f);

  blender::Set<int> keys;

  int iterations = 0;
  for (int key : map.keys()) {
    keys.add(key);
    iterations++;
  }

  EXPECT_EQ(iterations, 3);
  EXPECT_TRUE(keys.contains(1));
  EXPECT_TRUE(keys.contains(2));
  EXPECT_TRUE(keys.contains(6));
}

TEST(map, ItemIterator)
{
  Map<int, float> map;
  map.add(5, 3.0f);
  map.add(2, 9.0f);
  map.add(1, 0.0f);

  blender::Set<int> keys;
  blender::Set<float> values;

  int iterations = 0;
  const Map<int, float> &const_map = map;
  for (auto item : const_map.items()) {
    keys.add(item.key);
    values.add(item.value);
    iterations++;
  }

  EXPECT_EQ(iterations, 3);
  EXPECT_TRUE(keys.contains(5));
  EXPECT_TRUE(keys.contains(2));
  EXPECT_TRUE(keys.contains(1));
  EXPECT_TRUE(values.contains(3.0f));
  EXPECT_TRUE(values.contains(9.0f));
  EXPECT_TRUE(values.contains(0.0f));
}

TEST(map, MutableValueIterator)
{
  Map<int, int> map;
  map.add(3, 6);
  map.add(2, 1);

  for (int &value : map.values()) {
    value += 10;
  }

  EXPECT_EQ(map.lookup(3), 16);
  EXPECT_EQ(map.lookup(2), 11);
}

TEST(map, MutableItemIterator)
{
  Map<int, int> map;
  map.add(3, 6);
  map.add(2, 1);

  for (auto item : map.items()) {
    item.value += item.key;
  }

  EXPECT_EQ(map.lookup(3), 9.0f);
  EXPECT_EQ(map.lookup(2), 3.0f);
}

TEST(map, MutableItemToItemConversion)
{
  Map<int, int> map;
  map.add(3, 6);
  map.add(2, 1);

  Vector<int> keys, values;
  for (MapItem<int, int> item : map.items()) {
    keys.append(item.key);
    values.append(item.value);
  }

  EXPECT_EQ(keys.size(), 2);
  EXPECT_EQ(values.size(), 2);
  EXPECT_TRUE(keys.contains(3));
  EXPECT_TRUE(keys.contains(2));
  EXPECT_TRUE(values.contains(6));
  EXPECT_TRUE(values.contains(1));
}

static float return_42()
{
  return 42.0f;
}

TEST(map, LookupOrAddCB_SeparateFunction)
{
  Map<int, float> map;
  EXPECT_EQ(map.lookup_or_add_cb(0, return_42), 42.0f);
  EXPECT_EQ(map.lookup(0), 42);

  map.keys();
}

TEST(map, LookupOrAddCB_Lambdas)
{
  Map<int, float> map;
  auto lambda1 = []() { return 11.0f; };
  EXPECT_EQ(map.lookup_or_add_cb(0, lambda1), 11.0f);
  auto lambda2 = []() { return 20.0f; };
  EXPECT_EQ(map.lookup_or_add_cb(1, lambda2), 20.0f);

  EXPECT_EQ(map.lookup_or_add_cb(0, lambda2), 11.0f);
  EXPECT_EQ(map.lookup_or_add_cb(1, lambda1), 20.0f);
}

TEST(map, AddOrModify)
{
  Map<int, float> map;
  auto create_func = [](float *value) {
    *value = 10.0f;
    return true;
  };
  auto modify_func = [](float *value) {
    *value += 5;
    return false;
  };
  EXPECT_TRUE(map.add_or_modify(1, create_func, modify_func));
  EXPECT_EQ(map.lookup(1), 10.0f);
  EXPECT_FALSE(map.add_or_modify(1, create_func, modify_func));
  EXPECT_EQ(map.lookup(1), 15.0f);
}

TEST(map, AddOrModifyReference)
{
  Map<int, std::unique_ptr<int>> map;
  auto create_func = [](std::unique_ptr<int> *value) -> int & {
    new (value) std::unique_ptr<int>(new int{10});
    return **value;
  };
  auto modify_func = [](std::unique_ptr<int> *value) -> int & {
    **value += 5;
    return **value;
  };
  EXPECT_EQ(map.add_or_modify(1, create_func, modify_func), 10);
  int &a = map.add_or_modify(1, create_func, modify_func);
  EXPECT_EQ(a, 15);
  a = 100;
  EXPECT_EQ(*map.lookup(1), 100);
}

TEST(map, AddOverwrite)
{
  Map<int, float> map;
  EXPECT_FALSE(map.contains(3));
  EXPECT_TRUE(map.add_overwrite(3, 6.0f));
  EXPECT_EQ(map.lookup(3), 6.0f);
  EXPECT_FALSE(map.add_overwrite(3, 7.0f));
  EXPECT_EQ(map.lookup(3), 7.0f);
  EXPECT_FALSE(map.add(3, 8.0f));
  EXPECT_EQ(map.lookup(3), 7.0f);
}

TEST(map, LookupOrAddDefault)
{
  Map<int, float> map;
  map.lookup_or_add_default(3) = 6;
  EXPECT_EQ(map.lookup(3), 6);
  map.lookup_or_add_default(5) = 2;
  EXPECT_EQ(map.lookup(5), 2);
  map.lookup_or_add_default(3) += 4;
  EXPECT_EQ(map.lookup(3), 10);
}

TEST(map, LookupOrAdd)
{
  Map<int, int> map;
  EXPECT_EQ(map.lookup_or_add(6, 4), 4);
  EXPECT_EQ(map.lookup_or_add(6, 5), 4);
  map.lookup_or_add(6, 4) += 10;
  EXPECT_EQ(map.lookup(6), 14);
}

TEST(map, MoveConstructorSmall)
{
  Map<int, float> map1;
  map1.add(1, 2.0f);
  map1.add(4, 1.0f);
  Map<int, float> map2(std::move(map1));
  EXPECT_EQ(map2.size(), 2);
  EXPECT_EQ(map2.lookup(1), 2.0f);
  EXPECT_EQ(map2.lookup(4), 1.0f);
  EXPECT_EQ(map1.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(map1.lookup_ptr(4), nullptr);
}

TEST(map, MoveConstructorLarge)
{
  Map<int, int> map1;
  for (int i = 0; i < 100; i++) {
    map1.add_new(i, i);
  }
  Map<int, int> map2(std::move(map1));
  EXPECT_EQ(map2.size(), 100);
  EXPECT_EQ(map2.lookup(1), 1);
  EXPECT_EQ(map2.lookup(4), 4);
  EXPECT_EQ(map1.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(map1.lookup_ptr(4), nullptr);
}

TEST(map, MoveAssignment)
{
  Map<int, float> map1;
  map1.add(1, 2.0f);
  map1.add(4, 1.0f);
  Map<int, float> map2;
  map2 = std::move(map1);
  EXPECT_EQ(map2.size(), 2);
  EXPECT_EQ(map2.lookup(1), 2.0f);
  EXPECT_EQ(map2.lookup(4), 1.0f);
  EXPECT_EQ(map1.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(map1.lookup_ptr(4), nullptr);
}

TEST(map, CopyAssignment)
{
  Map<int, float> map1;
  map1.add(1, 2.0f);
  map1.add(4, 1.0f);
  Map<int, float> map2;
  map2 = map1;
  EXPECT_EQ(map2.size(), 2);
  EXPECT_EQ(map2.lookup(1), 2.0f);
  EXPECT_EQ(map2.lookup(4), 1.0f);
  EXPECT_EQ(map1.size(), 2);
  EXPECT_EQ(*map1.lookup_ptr(4), 1.0f);
}

TEST(map, Clear)
{
  Map<int, float> map;
  map.add(1, 1.0f);
  map.add(2, 5.0f);

  EXPECT_EQ(map.size(), 2);
  EXPECT_TRUE(map.contains(1));
  EXPECT_TRUE(map.contains(2));

  map.clear();

  EXPECT_EQ(map.size(), 0);
  EXPECT_FALSE(map.contains(1));
  EXPECT_FALSE(map.contains(2));
}

TEST(map, UniquePtrValue)
{
  auto value1 = std::make_unique<int>();
  auto value2 = std::make_unique<int>();
  auto value3 = std::make_unique<int>();

  int *value1_ptr = value1.get();

  Map<int, std::unique_ptr<int>> map;
  map.add_new(1, std::move(value1));
  map.add(2, std::move(value2));
  map.add_overwrite(3, std::move(value3));
  map.lookup_or_add_cb(4, []() { return std::make_unique<int>(); });
  map.add_new(5, std::make_unique<int>());
  map.add(6, std::make_unique<int>());
  map.add_overwrite(7, std::make_unique<int>());
  map.lookup_or_add(8, std::make_unique<int>());
  map.pop_default(9, std::make_unique<int>());

  EXPECT_EQ(map.lookup(1).get(), value1_ptr);
  EXPECT_EQ(map.lookup_ptr(100), nullptr);
}

TEST(map, Remove)
{
  Map<int, int> map;
  map.add(2, 4);
  EXPECT_EQ(map.size(), 1);
  EXPECT_FALSE(map.remove(3));
  EXPECT_EQ(map.size(), 1);
  EXPECT_TRUE(map.remove(2));
  EXPECT_EQ(map.size(), 0);
}

TEST(map, PointerKeys)
{
  char a, b, c, d;

  Map<char *, int> map;
  EXPECT_TRUE(map.add(&a, 5));
  EXPECT_FALSE(map.add(&a, 4));
  map.add_new(&b, 1);
  map.add_new(&c, 1);
  EXPECT_EQ(map.size(), 3);
  EXPECT_TRUE(map.remove(&b));
  EXPECT_TRUE(map.add(&b, 8));
  EXPECT_FALSE(map.remove(&d));
  EXPECT_TRUE(map.remove(&a));
  EXPECT_TRUE(map.remove(&b));
  EXPECT_TRUE(map.remove(&c));
  EXPECT_TRUE(map.is_empty());
}

TEST(map, ConstKeysAndValues)
{
  Map<const std::string, const std::string> map;
  map.reserve(10);
  map.add("45", "643");
  EXPECT_TRUE(map.contains("45"));
  EXPECT_FALSE(map.contains("54"));
}

TEST(map, ForeachItem)
{
  Map<int, int> map;
  map.add(3, 4);
  map.add(1, 8);

  Vector<int> keys;
  Vector<int> values;
  map.foreach_item([&](int key, int value) {
    keys.append(key);
    values.append(value);
  });

  EXPECT_EQ(keys.size(), 2);
  EXPECT_EQ(values.size(), 2);
  EXPECT_EQ(keys.first_index_of(3), values.first_index_of(4));
  EXPECT_EQ(keys.first_index_of(1), values.first_index_of(8));
}

TEST(map, CopyConstructorExceptions)
{
  using MapType = Map<ExceptionThrower, ExceptionThrower>;
  MapType map;
  map.add(2, 2);
  map.add(4, 4);
  map.lookup(2).throw_during_copy = true;
  EXPECT_ANY_THROW({ MapType map_copy(map); });
}

TEST(map, MoveConstructorExceptions)
{
  using MapType = Map<ExceptionThrower, ExceptionThrower>;
  MapType map;
  map.add(1, 1);
  map.add(2, 2);
  map.lookup(1).throw_during_move = true;
  EXPECT_ANY_THROW({ MapType map_moved(std::move(map)); });
  map.add(5, 5); /* NOLINT: bugprone-use-after-move */
}

TEST(map, AddNewExceptions)
{
  Map<ExceptionThrower, ExceptionThrower> map;
  ExceptionThrower key1 = 1;
  key1.throw_during_copy = true;
  ExceptionThrower value1;
  EXPECT_ANY_THROW({ map.add_new(key1, value1); });
  EXPECT_EQ(map.size(), 0);
  ExceptionThrower key2 = 2;
  ExceptionThrower value2;
  value2.throw_during_copy = true;
  EXPECT_ANY_THROW({ map.add_new(key2, value2); });
}

TEST(map, ReserveExceptions)
{
  Map<ExceptionThrower, ExceptionThrower> map;
  map.add(3, 3);
  map.add(5, 5);
  map.add(2, 2);
  map.lookup(2).throw_during_move = true;
  EXPECT_ANY_THROW({ map.reserve(100); });
  map.add(1, 1);
  map.add(5, 5);
}

TEST(map, PopExceptions)
{
  Map<ExceptionThrower, ExceptionThrower> map;
  map.add(3, 3);
  map.lookup(3).throw_during_move = true;
  EXPECT_ANY_THROW({ map.pop(3); }); /* NOLINT: bugprone-throw-keyword-missing */
  EXPECT_EQ(map.size(), 1);
  map.add(1, 1);
  EXPECT_EQ(map.size(), 2);
}

TEST(map, AddOrModifyExceptions)
{
  Map<ExceptionThrower, ExceptionThrower> map;
  auto create_fn = [](ExceptionThrower * /*v*/) { throw std::runtime_error(""); };
  auto modify_fn = [](ExceptionThrower * /*v*/) {};
  EXPECT_ANY_THROW({ map.add_or_modify(3, create_fn, modify_fn); });
}

namespace {
enum class TestEnum {
  A = 0,
  B = 1,
  C = 2,
  D = 1,
};
}

TEST(map, EnumKey)
{
  Map<TestEnum, int> map;
  map.add(TestEnum::A, 4);
  map.add(TestEnum::B, 6);
  EXPECT_EQ(map.lookup(TestEnum::A), 4);
  EXPECT_EQ(map.lookup(TestEnum::B), 6);
  EXPECT_EQ(map.lookup(TestEnum::D), 6);
  EXPECT_FALSE(map.contains(TestEnum::C));
  map.lookup(TestEnum::D) = 10;
  EXPECT_EQ(map.lookup(TestEnum::B), 10);
}

TEST(map, GenericAlgorithms)
{
  Map<int, int> map;
  map.add(5, 2);
  map.add(1, 4);
  map.add(2, 2);
  map.add(7, 1);
  map.add(8, 6);
  EXPECT_TRUE(std::any_of(map.keys().begin(), map.keys().end(), [](int v) { return v == 1; }));
  EXPECT_TRUE(std::any_of(map.values().begin(), map.values().end(), [](int v) { return v == 1; }));
  EXPECT_TRUE(std::any_of(
      map.items().begin(), map.items().end(), [](auto item) { return item.value == 1; }));
  EXPECT_EQ(std::count(map.values().begin(), map.values().end(), 2), 2);
  EXPECT_EQ(std::count(map.values().begin(), map.values().end(), 4), 1);
  EXPECT_EQ(std::count(map.keys().begin(), map.keys().end(), 7), 1);
}

TEST(map, AddAsVariadic)
{
  Map<int, StringRef> map;
  map.add_as(3, "hello", 2);
  map.add_as(2, "test", 1);
  EXPECT_EQ(map.lookup(3), "he");
  EXPECT_EQ(map.lookup(2), "t");
}

TEST(map, RemoveDuringIteration)
{
  Map<int, int> map;
  map.add(2, 1);
  map.add(5, 2);
  map.add(1, 2);
  map.add(6, 0);
  map.add(3, 3);

  EXPECT_EQ(map.size(), 5);

  using Iter = Map<int, int>::MutableItemIterator;
  Iter begin = map.items().begin();
  Iter end = map.items().end();
  for (Iter iter = begin; iter != end; ++iter) {
    MutableMapItem<int, int> item = *iter;
    if (item.value == 2) {
      map.remove(iter);
    }
  }

  EXPECT_EQ(map.size(), 3);
  EXPECT_EQ(map.lookup(2), 1);
  EXPECT_EQ(map.lookup(6), 0);
  EXPECT_EQ(map.lookup(3), 3);
}

TEST(map, RemoveIf)
{
  Map<int64_t, int64_t> map;
  for (const int64_t i : IndexRange(100)) {
    map.add(i * i, i);
  }
  const int64_t removed = map.remove_if([](auto item) { return item.key > 100; });
  EXPECT_EQ(map.size() + removed, 100);
  for (const int64_t i : IndexRange(100)) {
    if (i <= 10) {
      EXPECT_EQ(map.lookup(i * i), i);
    }
    else {
      EXPECT_FALSE(map.contains(i * i));
    }
  }
}

TEST(map, LookupKey)
{
  Map<std::string, int> map;
  map.add("a", 0);
  map.add("b", 1);
  map.add("c", 2);
  EXPECT_EQ(map.lookup_key("a"), "a");
  EXPECT_EQ(map.lookup_key_as("c"), "c");
  EXPECT_EQ(map.lookup_key_ptr_as("d"), nullptr);
  EXPECT_EQ(map.lookup_key_ptr_as("b")->size(), 1);
  EXPECT_EQ(map.lookup_key_ptr("a"), map.lookup_key_ptr_as("a"));
}

TEST(map, VectorKey)
{
  Map<Vector<int>, int> map;
  map.add({1, 2, 3}, 100);
  map.add({3, 2, 1}, 200);

  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.lookup({1, 2, 3}), 100);
  EXPECT_EQ(map.lookup({3, 2, 1}), 200);
  EXPECT_FALSE(map.contains({1, 2}));

  std::array<int, 3> array = {1, 2, 3};
  EXPECT_EQ(map.lookup_as(array), 100);

  map.remove_as(Vector<int>({1, 2, 3}).as_mutable_span());
  EXPECT_EQ(map.size(), 1);
}

/**
 * Set this to 1 to activate the benchmark. It is disabled by default, because it prints a lot.
 */
#if 0
template<typename MapT>
BLI_NOINLINE void benchmark_random_ints(StringRef name, int amount, int factor)
{
  RNG *rng = BLI_rng_new(0);
  Vector<int> values;
  for (int i = 0; i < amount; i++) {
    values.append(BLI_rng_get_int(rng) * factor);
  }
  BLI_rng_free(rng);

  MapT map;
  {
    SCOPED_TIMER(name + " Add");
    for (int value : values) {
      map.add(value, value);
    }
  }
  int count = 0;
  {
    SCOPED_TIMER(name + " Contains");
    for (int value : values) {
      count += map.contains(value);
    }
  }
  {
    SCOPED_TIMER(name + " Remove");
    for (int value : values) {
      count += map.remove(value);
    }
  }

  /* Print the value for simple error checking and to avoid some compiler optimizations. */
  std::cout << "Count: " << count << "\n";
}

/**
 * A wrapper for std::unordered_map with the API of blender::Map. This can be used for
 * benchmarking.
 */
template<typename Key, typename Value> class StdUnorderedMapWrapper {
 private:
  using MapType = std::unordered_map<Key, Value, blender::DefaultHash<Key>>;
  MapType map_;

 public:
  int64_t size() const
  {
    return int64_t(map_.size());
  }

  bool is_empty() const
  {
    return map_.empty();
  }

  void reserve(int64_t n)
  {
    map_.reserve(n);
  }

  template<typename ForwardKey, typename... ForwardValue>
  void add_new(ForwardKey &&key, ForwardValue &&...value)
  {
    map_.insert({std::forward<ForwardKey>(key), Value(std::forward<ForwardValue>(value)...)});
  }

  template<typename ForwardKey, typename... ForwardValue>
  bool add(ForwardKey &&key, ForwardValue &&...value)
  {
    return map_
        .insert({std::forward<ForwardKey>(key), Value(std::forward<ForwardValue>(value)...)})
        .second;
  }

  bool contains(const Key &key) const
  {
    return map_.find(key) != map_.end();
  }

  bool remove(const Key &key)
  {
    return bool(map_.erase(key));
  }

  Value &lookup(const Key &key)
  {
    return map_.find(key)->second;
  }

  const Value &lookup(const Key &key) const
  {
    return map_.find(key)->second;
  }

  void clear()
  {
    map_.clear();
  }

  void print_stats(StringRef /*name*/ = "") const {}
};

TEST(map, Benchmark)
{
  for (int i = 0; i < 3; i++) {
    benchmark_random_ints<blender::Map<int, int>>("blender::Map          ", 1000000, 1);
    benchmark_random_ints<blender::StdUnorderedMapWrapper<int, int>>(
        "std::unordered_map", 1000000, 1);
  }
  std::cout << "\n";
  for (int i = 0; i < 3; i++) {
    uint32_t factor = (3 << 10);
    benchmark_random_ints<blender::Map<int, int>>("blender::Map          ", 1000000, factor);
    benchmark_random_ints<blender::StdUnorderedMapWrapper<int, int>>(
        "std::unordered_map", 1000000, factor);
  }
}

/**
 * Timer 'blender::Map           Add' took 61.7616 ms
 * Timer 'blender::Map           Contains' took 18.4989 ms
 * Timer 'blender::Map           Remove' took 20.5864 ms
 * Count: 1999755
 * Timer 'std::unordered_map Add' took 188.674 ms
 * Timer 'std::unordered_map Contains' took 44.3741 ms
 * Timer 'std::unordered_map Remove' took 169.52 ms
 * Count: 1999755
 * Timer 'blender::Map           Add' took 37.9196 ms
 * Timer 'blender::Map           Contains' took 16.7361 ms
 * Timer 'blender::Map           Remove' took 20.9568 ms
 * Count: 1999755
 * Timer 'std::unordered_map Add' took 166.09 ms
 * Timer 'std::unordered_map Contains' took 40.6133 ms
 * Timer 'std::unordered_map Remove' took 142.85 ms
 * Count: 1999755
 * Timer 'blender::Map           Add' took 37.3053 ms
 * Timer 'blender::Map           Contains' took 16.6731 ms
 * Timer 'blender::Map           Remove' took 18.8304 ms
 * Count: 1999755
 * Timer 'std::unordered_map Add' took 170.964 ms
 * Timer 'std::unordered_map Contains' took 38.1824 ms
 * Timer 'std::unordered_map Remove' took 140.263 ms
 * Count: 1999755
 *
 * Timer 'blender::Map           Add' took 50.1131 ms
 * Timer 'blender::Map           Contains' took 25.0491 ms
 * Timer 'blender::Map           Remove' took 32.4225 ms
 * Count: 1889920
 * Timer 'std::unordered_map Add' took 150.129 ms
 * Timer 'std::unordered_map Contains' took 34.6999 ms
 * Timer 'std::unordered_map Remove' took 120.907 ms
 * Count: 1889920
 * Timer 'blender::Map           Add' took 50.4438 ms
 * Timer 'blender::Map           Contains' took 25.2677 ms
 * Timer 'blender::Map           Remove' took 32.3047 ms
 * Count: 1889920
 * Timer 'std::unordered_map Add' took 144.015 ms
 * Timer 'std::unordered_map Contains' took 36.3387 ms
 * Timer 'std::unordered_map Remove' took 119.109 ms
 * Count: 1889920
 * Timer 'blender::Map           Add' took 48.6995 ms
 * Timer 'blender::Map           Contains' took 25.1846 ms
 * Timer 'blender::Map           Remove' took 33.0283 ms
 * Count: 1889920
 * Timer 'std::unordered_map Add' took 143.494 ms
 * Timer 'std::unordered_map Contains' took 34.8905 ms
 * Timer 'std::unordered_map Remove' took 122.739 ms
 * Count: 1889920
 */

#endif /* Benchmark */

}  // namespace blender::tests
