#include "BLI_map.hh"
#include "BLI_rand.h"
#include "BLI_set.hh"
#include "BLI_strict_flags.h"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"
#include "testing/testing.h"

using namespace BLI;

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

  BLI::Set<float> values;

  uint iterations = 0;
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

  BLI::Set<int> keys;

  uint iterations = 0;
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

  BLI::Set<int> keys;
  BLI::Set<float> values;

  uint iterations = 0;
  for (auto item : map.items()) {
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

static float return_42()
{
  return 42.0f;
}

TEST(map, LookupOrAdd_SeparateFunction)
{
  Map<int, float> map;
  EXPECT_EQ(map.lookup_or_add(0, return_42), 42.0f);
  EXPECT_EQ(map.lookup(0), 42);

  map.keys();
}

TEST(map, LookupOrAdd_Lambdas)
{
  Map<int, float> map;
  auto lambda1 = []() { return 11.0f; };
  EXPECT_EQ(map.lookup_or_add(0, lambda1), 11.0f);
  auto lambda2 = []() { return 20.0f; };
  EXPECT_EQ(map.lookup_or_add(1, lambda2), 20.0f);

  EXPECT_EQ(map.lookup_or_add(0, lambda2), 11.0f);
  EXPECT_EQ(map.lookup_or_add(1, lambda1), 20.0f);
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

TEST(map, MoveConstructorSmall)
{
  Map<int, float> map1;
  map1.add(1, 2.0f);
  map1.add(4, 1.0f);
  Map<int, float> map2(std::move(map1));
  EXPECT_EQ(map2.size(), 2);
  EXPECT_EQ(map2.lookup(1), 2.0f);
  EXPECT_EQ(map2.lookup(4), 1.0f);
  EXPECT_EQ(map1.size(), 0);
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
  EXPECT_EQ(map1.size(), 0);
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
  EXPECT_EQ(map1.size(), 0);
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
  auto value1 = std::unique_ptr<int>(new int());
  auto value2 = std::unique_ptr<int>(new int());
  auto value3 = std::unique_ptr<int>(new int());

  int *value1_ptr = value1.get();

  Map<int, std::unique_ptr<int>> map;
  map.add_new(1, std::move(value1));
  map.add(2, std::move(value2));
  map.add_overwrite(3, std::move(value3));
  map.lookup_or_add(4, []() { return std::unique_ptr<int>(new int()); });
  map.add_new(5, std::unique_ptr<int>(new int()));
  map.add(6, std::unique_ptr<int>(new int()));
  map.add_overwrite(7, std::unique_ptr<int>(new int()));

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

/**
 * Set this to 1 to activate the benchmark. It is disabled by default, because it prints a lot.
 */
#if 0
template<typename MapT>
BLI_NOINLINE void benchmark_random_ints(StringRef name, uint amount, uint factor)
{
  RNG *rng = BLI_rng_new(0);
  Vector<int> values;
  for (uint i = 0; i < amount; i++) {
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

TEST(map, Benchmark)
{
  for (uint i = 0; i < 3; i++) {
    benchmark_random_ints<BLI::Map<int, int>>("BLI::Map          ", 1000000, 1);
    benchmark_random_ints<BLI::StdUnorderedMapWrapper<int, int>>("std::unordered_map", 1000000, 1);
  }
  std::cout << "\n";
  for (uint i = 0; i < 3; i++) {
    uint32_t factor = (3 << 10);
    benchmark_random_ints<BLI::Map<int, int>>("BLI::Map          ", 1000000, factor);
    benchmark_random_ints<BLI::StdUnorderedMapWrapper<int, int>>(
        "std::unordered_map", 1000000, factor);
  }
}

/**
 * Timer 'BLI::Map           Add' took 61.7616 ms
 * Timer 'BLI::Map           Contains' took 18.4989 ms
 * Timer 'BLI::Map           Remove' took 20.5864 ms
 * Count: 1999755
 * Timer 'std::unordered_map Add' took 188.674 ms
 * Timer 'std::unordered_map Contains' took 44.3741 ms
 * Timer 'std::unordered_map Remove' took 169.52 ms
 * Count: 1999755
 * Timer 'BLI::Map           Add' took 37.9196 ms
 * Timer 'BLI::Map           Contains' took 16.7361 ms
 * Timer 'BLI::Map           Remove' took 20.9568 ms
 * Count: 1999755
 * Timer 'std::unordered_map Add' took 166.09 ms
 * Timer 'std::unordered_map Contains' took 40.6133 ms
 * Timer 'std::unordered_map Remove' took 142.85 ms
 * Count: 1999755
 * Timer 'BLI::Map           Add' took 37.3053 ms
 * Timer 'BLI::Map           Contains' took 16.6731 ms
 * Timer 'BLI::Map           Remove' took 18.8304 ms
 * Count: 1999755
 * Timer 'std::unordered_map Add' took 170.964 ms
 * Timer 'std::unordered_map Contains' took 38.1824 ms
 * Timer 'std::unordered_map Remove' took 140.263 ms
 * Count: 1999755
 *
 * Timer 'BLI::Map           Add' took 50.1131 ms
 * Timer 'BLI::Map           Contains' took 25.0491 ms
 * Timer 'BLI::Map           Remove' took 32.4225 ms
 * Count: 1889920
 * Timer 'std::unordered_map Add' took 150.129 ms
 * Timer 'std::unordered_map Contains' took 34.6999 ms
 * Timer 'std::unordered_map Remove' took 120.907 ms
 * Count: 1889920
 * Timer 'BLI::Map           Add' took 50.4438 ms
 * Timer 'BLI::Map           Contains' took 25.2677 ms
 * Timer 'BLI::Map           Remove' took 32.3047 ms
 * Count: 1889920
 * Timer 'std::unordered_map Add' took 144.015 ms
 * Timer 'std::unordered_map Contains' took 36.3387 ms
 * Timer 'std::unordered_map Remove' took 119.109 ms
 * Count: 1889920
 * Timer 'BLI::Map           Add' took 48.6995 ms
 * Timer 'BLI::Map           Contains' took 25.1846 ms
 * Timer 'BLI::Map           Remove' took 33.0283 ms
 * Count: 1889920
 * Timer 'std::unordered_map Add' took 143.494 ms
 * Timer 'std::unordered_map Contains' took 34.8905 ms
 * Timer 'std::unordered_map Remove' took 122.739 ms
 * Count: 1889920
 */

#endif /* Benchmark */
