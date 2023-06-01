/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <set>
#include <unordered_set>

#include "BLI_exception_safety_test_utils.hh"
#include "BLI_ghash.h"
#include "BLI_rand.h"
#include "BLI_set.hh"
#include "BLI_strict_flags.h"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"
#include "testing/testing.h"

namespace blender {
namespace tests {

TEST(set, DefaultConstructor)
{
  Set<int> set;
  EXPECT_EQ(set.size(), 0);
  EXPECT_TRUE(set.is_empty());
}

TEST(set, ContainsNotExistant)
{
  Set<int> set;
  EXPECT_FALSE(set.contains(3));
}

TEST(set, ContainsExistant)
{
  Set<int> set;
  EXPECT_FALSE(set.contains(5));
  EXPECT_TRUE(set.is_empty());
  set.add(5);
  EXPECT_TRUE(set.contains(5));
  EXPECT_FALSE(set.is_empty());
}

TEST(set, AddMany)
{
  Set<int> set;
  for (int i = 0; i < 100; i++) {
    set.add(i);
  }

  for (int i = 50; i < 100; i++) {
    EXPECT_TRUE(set.contains(i));
  }
  for (int i = 100; i < 150; i++) {
    EXPECT_FALSE(set.contains(i));
  }
}

TEST(set, InitializerListConstructor)
{
  Set<int> set = {4, 5, 6};
  EXPECT_EQ(set.size(), 3);
  EXPECT_TRUE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  EXPECT_TRUE(set.contains(6));
  EXPECT_FALSE(set.contains(2));
  EXPECT_FALSE(set.contains(3));
}

TEST(set, CopyConstructor)
{
  Set<int> set = {3};
  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));

  Set<int> set2(set);
  set2.add(4);
  EXPECT_TRUE(set2.contains(3));
  EXPECT_TRUE(set2.contains(4));

  EXPECT_FALSE(set.contains(4));
}

TEST(set, MoveConstructor)
{
  Set<int> set = {1, 2, 3};
  EXPECT_EQ(set.size(), 3);
  Set<int> set2(std::move(set));
  EXPECT_EQ(set.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(set2.size(), 3);
}

TEST(set, CopyAssignment)
{
  Set<int> set = {3};
  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));

  Set<int> set2;
  set2 = set;
  set2.add(4);
  EXPECT_TRUE(set2.contains(3));
  EXPECT_TRUE(set2.contains(4));

  EXPECT_FALSE(set.contains(4));
}

TEST(set, MoveAssignment)
{
  Set<int> set = {1, 2, 3};
  EXPECT_EQ(set.size(), 3);
  Set<int> set2;
  set2 = std::move(set);
  EXPECT_EQ(set.size(), 0); /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(set2.size(), 3);
}

TEST(set, RemoveContained)
{
  Set<int> set = {3, 4, 5};
  EXPECT_TRUE(set.contains(3));
  EXPECT_TRUE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove_contained(4);
  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove_contained(3);
  EXPECT_FALSE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove_contained(5);
  EXPECT_FALSE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_FALSE(set.contains(5));
}

TEST(set, RemoveContainedMany)
{
  Set<int> set;
  for (int i = 0; i < 1000; i++) {
    set.add(i);
  }
  for (int i = 100; i < 1000; i++) {
    set.remove_contained(i);
  }
  for (int i = 900; i < 1000; i++) {
    set.add(i);
  }

  for (int i = 0; i < 1000; i++) {
    if (i < 100 || i >= 900) {
      EXPECT_TRUE(set.contains(i));
    }
    else {
      EXPECT_FALSE(set.contains(i));
    }
  }
}

TEST(set, Intersects)
{
  Set<int> a = {3, 4, 5, 6};
  Set<int> b = {1, 2, 5};
  EXPECT_TRUE(Set<int>::Intersects(a, b));
  EXPECT_FALSE(Set<int>::Disjoint(a, b));
}

TEST(set, Disjoint)
{
  Set<int> a = {5, 6, 7, 8};
  Set<int> b = {2, 3, 4, 9};
  EXPECT_FALSE(Set<int>::Intersects(a, b));
  EXPECT_TRUE(Set<int>::Disjoint(a, b));
}

TEST(set, AddMultiple)
{
  Set<int> a;
  a.add_multiple({5, 7});
  EXPECT_TRUE(a.contains(5));
  EXPECT_TRUE(a.contains(7));
  EXPECT_FALSE(a.contains(4));
  a.add_multiple({2, 4, 7});
  EXPECT_TRUE(a.contains(4));
  EXPECT_TRUE(a.contains(2));
  EXPECT_EQ(a.size(), 4);
}

TEST(set, AddMultipleNew)
{
  Set<int> a;
  a.add_multiple_new({5, 6});
  EXPECT_TRUE(a.contains(5));
  EXPECT_TRUE(a.contains(6));
}

TEST(set, Iterator)
{
  Set<int> set = {1, 3, 2, 5, 4};
  blender::Vector<int> vec;
  for (int value : set) {
    vec.append(value);
  }
  EXPECT_EQ(vec.size(), 5);
  EXPECT_TRUE(vec.contains(1));
  EXPECT_TRUE(vec.contains(3));
  EXPECT_TRUE(vec.contains(2));
  EXPECT_TRUE(vec.contains(5));
  EXPECT_TRUE(vec.contains(4));
}

TEST(set, OftenAddRemoveContained)
{
  Set<int> set;
  for (int i = 0; i < 100; i++) {
    set.add(42);
    EXPECT_EQ(set.size(), 1);
    set.remove_contained(42);
    EXPECT_EQ(set.size(), 0);
  }
}

TEST(set, UniquePtrValues)
{
  Set<std::unique_ptr<int>> set;
  set.add_new(std::make_unique<int>());
  auto value1 = std::make_unique<int>();
  set.add_new(std::move(value1));
  set.add(std::make_unique<int>());

  EXPECT_EQ(set.size(), 3);
}

TEST(set, Clear)
{
  Set<int> set = {3, 4, 6, 7};
  EXPECT_EQ(set.size(), 4);
  set.clear();
  EXPECT_EQ(set.size(), 0);
}

TEST(set, StringSet)
{
  Set<std::string> set;
  set.add("hello");
  set.add("world");
  EXPECT_EQ(set.size(), 2);
  EXPECT_TRUE(set.contains("hello"));
  EXPECT_TRUE(set.contains("world"));
  EXPECT_FALSE(set.contains("world2"));
}

TEST(set, PointerSet)
{
  int a, b, c;
  Set<int *> set;
  set.add(&a);
  set.add(&b);
  EXPECT_EQ(set.size(), 2);
  EXPECT_TRUE(set.contains(&a));
  EXPECT_TRUE(set.contains(&b));
  EXPECT_FALSE(set.contains(&c));
}

TEST(set, Remove)
{
  Set<int> set = {1, 2, 3, 4, 5, 6};
  EXPECT_EQ(set.size(), 6);
  EXPECT_TRUE(set.remove(2));
  EXPECT_EQ(set.size(), 5);
  EXPECT_FALSE(set.contains(2));
  EXPECT_FALSE(set.remove(2));
  EXPECT_EQ(set.size(), 5);
  EXPECT_TRUE(set.remove(5));
  EXPECT_EQ(set.size(), 4);
}

struct Type1 {
  uint32_t value;
};

struct Type2 {
  uint32_t value;
};

static bool operator==(const Type1 &a, const Type1 &b)
{
  return a.value == b.value;
}
static bool operator==(const Type2 &a, const Type1 &b)
{
  return a.value == b.value;
}

}  // namespace tests

/* This has to be defined in ::blender namespace. */
template<> struct DefaultHash<tests::Type1> {
  uint32_t operator()(const tests::Type1 &value) const
  {
    return value.value;
  }

  uint32_t operator()(const tests::Type2 &value) const
  {
    return value.value;
  }
};

namespace tests {

TEST(set, ContainsAs)
{
  Set<Type1> set;
  set.add(Type1{5});
  EXPECT_TRUE(set.contains_as(Type1{5}));
  EXPECT_TRUE(set.contains_as(Type2{5}));
  EXPECT_FALSE(set.contains_as(Type1{6}));
  EXPECT_FALSE(set.contains_as(Type2{6}));
}

TEST(set, ContainsAsString)
{
  Set<std::string> set;
  set.add("test");
  EXPECT_TRUE(set.contains_as("test"));
  EXPECT_TRUE(set.contains_as(StringRef("test")));
  EXPECT_FALSE(set.contains_as("string"));
  EXPECT_FALSE(set.contains_as(StringRef("string")));
}

TEST(set, RemoveContainedAs)
{
  Set<Type1> set;
  set.add(Type1{5});
  EXPECT_TRUE(set.contains_as(Type2{5}));
  set.remove_contained_as(Type2{5});
  EXPECT_FALSE(set.contains_as(Type2{5}));
}

TEST(set, RemoveAs)
{
  Set<Type1> set;
  set.add(Type1{5});
  EXPECT_TRUE(set.contains_as(Type2{5}));
  set.remove_as(Type2{6});
  EXPECT_TRUE(set.contains_as(Type2{5}));
  set.remove_as(Type2{5});
  EXPECT_FALSE(set.contains_as(Type2{5}));
  set.remove_as(Type2{5});
  EXPECT_FALSE(set.contains_as(Type2{5}));
}

TEST(set, AddAs)
{
  Set<std::string> set;
  EXPECT_TRUE(set.add_as("test"));
  EXPECT_TRUE(set.add_as(StringRef("qwe")));
  EXPECT_FALSE(set.add_as(StringRef("test")));
  EXPECT_FALSE(set.add_as("qwe"));
}

template<uint N> struct EqualityIntModN {
  bool operator()(uint a, uint b) const
  {
    return (a % N) == (b % N);
  }
};

template<uint N> struct HashIntModN {
  uint64_t operator()(uint value) const
  {
    return value % N;
  }
};

TEST(set, CustomizeHashAndEquality)
{
  Set<uint, 0, DefaultProbingStrategy, HashIntModN<10>, EqualityIntModN<10>> set;
  set.add(4);
  EXPECT_TRUE(set.contains(4));
  EXPECT_TRUE(set.contains(14));
  EXPECT_TRUE(set.contains(104));
  EXPECT_FALSE(set.contains(5));
  set.add(55);
  EXPECT_TRUE(set.contains(5));
  EXPECT_TRUE(set.contains(14));
  set.remove(1004);
  EXPECT_FALSE(set.contains(14));
}

TEST(set, IntrusiveIntKey)
{
  Set<int,
      2,
      DefaultProbingStrategy,
      DefaultHash<int>,
      DefaultEquality<int>,
      IntegerSetSlot<int, 100, 200>>
      set;
  EXPECT_TRUE(set.add(4));
  EXPECT_TRUE(set.add(3));
  EXPECT_TRUE(set.add(11));
  EXPECT_TRUE(set.add(8));
  EXPECT_FALSE(set.add(3));
  EXPECT_FALSE(set.add(4));
  EXPECT_TRUE(set.remove(4));
  EXPECT_FALSE(set.remove(7));
  EXPECT_TRUE(set.add(4));
  EXPECT_TRUE(set.remove(4));
}

struct MyKeyType {
  uint32_t key;
  int32_t attached_data;

  uint64_t hash() const
  {
    return key;
  }

  friend bool operator==(const MyKeyType &a, const MyKeyType &b)
  {
    return a.key == b.key;
  }
};

TEST(set, LookupKey)
{
  Set<MyKeyType> set;
  set.add({1, 10});
  set.add({2, 20});
  EXPECT_EQ(set.lookup_key({1, 30}).attached_data, 10);
  EXPECT_EQ(set.lookup_key({2, 0}).attached_data, 20);
}

TEST(set, LookupKeyDefault)
{
  Set<MyKeyType> set;
  set.add({1, 10});
  set.add({2, 20});

  MyKeyType fallback{5, 50};
  EXPECT_EQ(set.lookup_key_default({1, 66}, fallback).attached_data, 10);
  EXPECT_EQ(set.lookup_key_default({4, 40}, fallback).attached_data, 50);
}

TEST(set, LookupKeyPtr)
{
  Set<MyKeyType> set;
  set.add({1, 10});
  set.add({2, 20});
  EXPECT_EQ(set.lookup_key_ptr({1, 50})->attached_data, 10);
  EXPECT_EQ(set.lookup_key_ptr({2, 50})->attached_data, 20);
  EXPECT_EQ(set.lookup_key_ptr({3, 50}), nullptr);
}

TEST(set, LookupKeyOrAdd)
{
  Set<MyKeyType> set;
  set.lookup_key_or_add({1, 10});
  set.lookup_key_or_add({2, 20});
  EXPECT_EQ(set.size(), 2);
  EXPECT_EQ(set.lookup_key_or_add({2, 40}).attached_data, 20);
  EXPECT_EQ(set.size(), 2);
  EXPECT_EQ(set.lookup_key_or_add({3, 40}).attached_data, 40);
  EXPECT_EQ(set.size(), 3);
  EXPECT_EQ(set.lookup_key_or_add({3, 60}).attached_data, 40);
  EXPECT_EQ(set.size(), 3);
}

TEST(set, StringViewKeys)
{
  Set<std::string_view> set;
  set.add("hello");
  set.add("world");
  EXPECT_FALSE(set.contains("worlds"));
  EXPECT_TRUE(set.contains("world"));
  EXPECT_TRUE(set.contains("hello"));
}

TEST(set, SpanConstructorExceptions)
{
  std::array<ExceptionThrower, 5> array = {1, 2, 3, 4, 5};
  array[3].throw_during_copy = true;
  Span<ExceptionThrower> span = array;

  EXPECT_ANY_THROW({ Set<ExceptionThrower> set(span); });
}

TEST(set, CopyConstructorExceptions)
{
  Set<ExceptionThrower> set = {1, 2, 3, 4, 5};
  set.lookup_key(3).throw_during_copy = true;
  EXPECT_ANY_THROW({ Set<ExceptionThrower> set_copy(set); });
}

TEST(set, MoveConstructorExceptions)
{
  using SetType = Set<ExceptionThrower, 4>;
  SetType set = {1, 2, 3};
  set.lookup_key(2).throw_during_move = true;
  EXPECT_ANY_THROW({ SetType set_moved(std::move(set)); });
  EXPECT_EQ(set.size(), 0); /* NOLINT: bugprone-use-after-move */
  set.add_multiple({3, 6, 7});
  EXPECT_EQ(set.size(), 3);
}

TEST(set, AddNewExceptions)
{
  Set<ExceptionThrower> set;
  ExceptionThrower value;
  value.throw_during_copy = true;
  EXPECT_ANY_THROW({ set.add_new(value); });
  EXPECT_EQ(set.size(), 0);
  EXPECT_ANY_THROW({ set.add_new(value); });
  EXPECT_EQ(set.size(), 0);
}

TEST(set, AddExceptions)
{
  Set<ExceptionThrower> set;
  ExceptionThrower value;
  value.throw_during_copy = true;
  EXPECT_ANY_THROW({ set.add(value); });
  EXPECT_EQ(set.size(), 0);
  EXPECT_ANY_THROW({ set.add(value); });
  EXPECT_EQ(set.size(), 0);
}

TEST(set, ForwardIterator)
{
  Set<int> set = {5, 2, 6, 4, 1};
  Set<int>::iterator iter1 = set.begin();
  int value1 = *iter1;
  Set<int>::iterator iter2 = iter1++;
  EXPECT_EQ(*iter2, value1);
  EXPECT_EQ(*(++iter2), *iter1);
  /* Interesting find: On GCC & MSVC this will succeed, as the 2nd argument is evaluated before the
   * 1st. On Apple Clang it's the other way around, and the test fails. */
  // EXPECT_EQ(*iter1, *(++iter1));
  Set<int>::iterator iter3 = ++iter1;
  /* Check that #iter1 itself changed. */
  EXPECT_EQ(*iter3, *iter1);
}

TEST(set, GenericAlgorithms)
{
  Set<int> set = {1, 20, 30, 40};
  EXPECT_FALSE(std::any_of(set.begin(), set.end(), [](int v) { return v == 5; }));
  EXPECT_TRUE(std::any_of(set.begin(), set.end(), [](int v) { return v == 30; }));
  EXPECT_EQ(std::count(set.begin(), set.end(), 20), 1);
}

TEST(set, RemoveDuringIteration)
{
  Set<int> set;
  set.add(6);
  set.add(5);
  set.add(2);
  set.add(3);

  EXPECT_EQ(set.size(), 4);

  using Iter = Set<int>::Iterator;
  Iter begin = set.begin();
  Iter end = set.end();
  for (Iter iter = begin; iter != end; ++iter) {
    int item = *iter;
    if (item == 2) {
      set.remove(iter);
    }
  }

  EXPECT_EQ(set.size(), 3);
  EXPECT_TRUE(set.contains(5));
  EXPECT_TRUE(set.contains(6));
  EXPECT_TRUE(set.contains(3));
}

TEST(set, RemoveIf)
{
  Set<int64_t> set;
  for (const int64_t i : IndexRange(100)) {
    set.add(i * i);
  }
  const int64_t removed = set.remove_if([](const int64_t key) { return key > 100; });
  EXPECT_EQ(set.size() + removed, 100);
  for (const int64_t i : IndexRange(100)) {
    EXPECT_EQ(set.contains(i * i), i <= 10);
  }
}

TEST(set, RemoveUniquePtrWithRaw)
{
  Set<std::unique_ptr<int>> set;
  std::unique_ptr<int> a = std::make_unique<int>(5);
  int *a_ptr = a.get();
  set.add(std::move(a));
  EXPECT_EQ(set.size(), 1);
  set.remove_as(a_ptr);
  EXPECT_TRUE(set.is_empty());
}

TEST(set, Equality)
{
  const Set<int> a = {1, 2, 3, 4, 5};
  const Set<int> b = {5, 2, 3, 1, 4};
  const Set<int> c = {1, 2, 3};
  const Set<int> d = {1, 2, 3, 4, 5, 6};
  const Set<int> e = {};
  const Set<int> f = {10, 11, 12, 13, 14};

  EXPECT_EQ(a, a);
  EXPECT_EQ(a, b);
  EXPECT_EQ(b, a);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);
  EXPECT_NE(a, e);
  EXPECT_NE(a, f);
  EXPECT_NE(c, a);
  EXPECT_NE(d, a);
  EXPECT_NE(e, a);
  EXPECT_NE(f, a);
}

/**
 * Set this to 1 to activate the benchmark. It is disabled by default, because it prints a lot.
 */
#if 0
template<typename SetT>
BLI_NOINLINE void benchmark_random_ints(StringRef name, int amount, int factor)
{
  RNG *rng = BLI_rng_new(0);
  Vector<int> values;
  for (int i = 0; i < amount; i++) {
    values.append(BLI_rng_get_int(rng) * factor);
  }
  BLI_rng_free(rng);

  SetT set;
  {
    SCOPED_TIMER(name + " Add");
    for (int value : values) {
      set.add(value);
    }
  }
  int count = 0;
  {
    SCOPED_TIMER(name + " Contains");
    for (int value : values) {
      count += set.contains(value);
    }
  }
  {
    SCOPED_TIMER(name + " Remove");
    for (int value : values) {
      count += set.remove(value);
    }
  }

  /* Print the value for simple error checking and to avoid some compiler optimizations. */
  std::cout << "Count: " << count << "\n";
}

TEST(set, Benchmark)
{
  for (int i = 0; i < 3; i++) {
    benchmark_random_ints<blender::Set<int>>("blender::Set      ", 100000, 1);
    benchmark_random_ints<blender::StdUnorderedSetWrapper<int>>("std::unordered_set", 100000, 1);
  }
  std::cout << "\n";
  for (int i = 0; i < 3; i++) {
    uint32_t factor = (3 << 10);
    benchmark_random_ints<blender::Set<int>>("blender::Set      ", 100000, factor);
    benchmark_random_ints<blender::StdUnorderedSetWrapper<int>>("std::unordered_set", 100000, factor);
  }
}

/**
 * Output of the rudimentary benchmark above on my hardware.
 *
 * Timer 'blender::Set       Add' took 5.5573 ms
 * Timer 'blender::Set       Contains' took 0.807384 ms
 * Timer 'blender::Set       Remove' took 0.953436 ms
 * Count: 199998
 * Timer 'std::unordered_set Add' took 12.551 ms
 * Timer 'std::unordered_set Contains' took 2.3323 ms
 * Timer 'std::unordered_set Remove' took 5.07082 ms
 * Count: 199998
 * Timer 'blender::Set       Add' took 2.62526 ms
 * Timer 'blender::Set       Contains' took 0.407499 ms
 * Timer 'blender::Set       Remove' took 0.472981 ms
 * Count: 199998
 * Timer 'std::unordered_set Add' took 6.26945 ms
 * Timer 'std::unordered_set Contains' took 1.17236 ms
 * Timer 'std::unordered_set Remove' took 3.77402 ms
 * Count: 199998
 * Timer 'blender::Set       Add' took 2.59152 ms
 * Timer 'blender::Set       Contains' took 0.415254 ms
 * Timer 'blender::Set       Remove' took 0.477559 ms
 * Count: 199998
 * Timer 'std::unordered_set Add' took 6.28129 ms
 * Timer 'std::unordered_set Contains' took 1.17562 ms
 * Timer 'std::unordered_set Remove' took 3.77811 ms
 * Count: 199998
 *
 * Timer 'blender::Set       Add' took 3.16514 ms
 * Timer 'blender::Set       Contains' took 0.732895 ms
 * Timer 'blender::Set       Remove' took 1.08171 ms
 * Count: 198790
 * Timer 'std::unordered_set Add' took 6.57377 ms
 * Timer 'std::unordered_set Contains' took 1.17008 ms
 * Timer 'std::unordered_set Remove' took 3.7946 ms
 * Count: 198790
 * Timer 'blender::Set       Add' took 3.11439 ms
 * Timer 'blender::Set       Contains' took 0.740159 ms
 * Timer 'blender::Set       Remove' took 1.06749 ms
 * Count: 198790
 * Timer 'std::unordered_set Add' took 6.35597 ms
 * Timer 'std::unordered_set Contains' took 1.17713 ms
 * Timer 'std::unordered_set Remove' took 3.77826 ms
 * Count: 198790
 * Timer 'blender::Set       Add' took 3.09876 ms
 * Timer 'blender::Set       Contains' took 0.742072 ms
 * Timer 'blender::Set       Remove' took 1.06622 ms
 * Count: 198790
 * Timer 'std::unordered_set Add' took 6.4469 ms
 * Timer 'std::unordered_set Contains' took 1.16515 ms
 * Timer 'std::unordered_set Remove' took 3.80639 ms
 * Count: 198790
 */

#endif /* Benchmark */

}  // namespace tests
}  // namespace blender
