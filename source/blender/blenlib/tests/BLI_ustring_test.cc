/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <atomic>
#include <thread>

#include "BLI_array.hh"
#include "BLI_map.hh"
#include "BLI_task.hh"
#include "BLI_task_c.hh"
#include "BLI_ustring.hh"
#include "BLI_vector.hh"

namespace blender::tests {

TEST(ustring, MapUString)
{
  Map<UString, int> map;
  map.add("hello"_ustr, 1);
  map.add_as("world", 2);
  map.add_as("world", 3);
  EXPECT_EQ(map.lookup("hello"_ustr), 1);
  EXPECT_EQ(map.lookup_as("hello"), 1);
  EXPECT_EQ(map.lookup_as("world"), 2);
  EXPECT_EQ(map.lookup_as("world"_ustr), 2);
}

TEST(ustring, MapStringRef)
{
  Map<StringRef, int> map;
  map.add("hello", 1);
  map.add("world"_ustr.ref(), 2);
  map.add_as("world"_ustr.ref(), 3);
  EXPECT_EQ(map.lookup("hello"), 1);
  EXPECT_EQ(map.lookup("hello"_ustr.ref()), 1);
  EXPECT_EQ(map.lookup("world"), 2);
  EXPECT_EQ(map.lookup("world"_ustr.ref()), 2);
}

TEST(ustring, MapStdString)
{
  Map<std::string, int> map;
  map.add("hello", 1);
  map.add_as("world"_ustr.string(), 2);
  EXPECT_EQ(map.lookup("hello"), 1);
  EXPECT_EQ(map.lookup_as("hello"_ustr.ref()), 1);
  EXPECT_EQ(map.lookup("world"), 2);
  EXPECT_EQ(map.lookup_as("world"_ustr.ref()), 2);
}

TEST(ustring, Equality)
{
  /* This is mostly just checking if all these equality checks compile fine without ambiguities. */
  EXPECT_EQ("test"_ustr, "test"_ustr);
  EXPECT_EQ("test"_ustr, "test");
  EXPECT_EQ("test", "test"_ustr);
  EXPECT_EQ("test"_ustr, StringRef("test"));
  EXPECT_EQ(StringRef("test"), "test"_ustr);
  EXPECT_EQ("test"_ustr, std::string("test"));
  EXPECT_EQ(std::string("test"), "test"_ustr);
  EXPECT_EQ("test"_ustr, StringRefNull("test"));
  EXPECT_EQ(StringRefNull("test"), "test"_ustr);
  EXPECT_EQ("test"_ustr, std::string_view("test"));
  EXPECT_EQ(std::string_view("test"), "test"_ustr);
}

TEST(ustring, UniqueCorrectness)
{
  static const char *from_static_literal = "unique_correctness_98765";

  const std::string to_chars_result = std::to_string(98765);
  const std::string from_to_chars = std::string("unique_correctness_") + to_chars_result;

  const std::string from_concatenation = std::string("unique_correctness_9876") + "5";

  const UString a(from_static_literal);
  const UString b(from_to_chars);
  const UString c(from_concatenation);

  /* All three must have found (or created, then found) the exact same table entry. */
  EXPECT_EQ(a.c_str(), b.c_str());
  EXPECT_EQ(a.c_str(), c.c_str());
  EXPECT_EQ(a, b);
  EXPECT_EQ(a, c);
}

TEST(ustring, Empty)
{
  const UString default_constructed;
  const UString from_literal("");
  const UString from_std_string{std::string()};
  const UString from_ustr_literal = ""_ustr;

  EXPECT_TRUE(default_constructed.is_empty());
  EXPECT_TRUE(from_literal.is_empty());
  EXPECT_EQ(from_literal.size(), 0);
  EXPECT_STREQ(from_literal.c_str(), "");
  EXPECT_EQ(from_literal[0], '\0');
  EXPECT_EQ(from_literal.hash(), default_constructed.hash());

  /* All the ways of getting an empty string should end up pointing at the same table entry. */
  EXPECT_EQ(from_literal.c_str(), from_std_string.c_str());
  EXPECT_EQ(from_literal.c_str(), from_ustr_literal.c_str());
  EXPECT_EQ(from_literal, from_std_string);
  EXPECT_EQ(from_literal, from_ustr_literal);
  /* The default constructor is documented to avoid touching the shared table, so it uses its own
   * always-empty entry. Check whether that still compares equal to an interned empty string. */
  EXPECT_EQ(default_constructed, from_literal);
}

TEST(ustring, LongString)
{
  const std::string long_a(500, 'a');
  const std::string long_b = std::string(300, 'b') + std::string(300, 'c');

  const UString a(long_a);
  const UString b(long_b);

  EXPECT_EQ(a.size(), 500);
  EXPECT_EQ(a.c_str()[a.size()], '\0');
  EXPECT_EQ(a[0], 'a');
  EXPECT_EQ(a[499], 'a');
  EXPECT_EQ(a[500], '\0');

  EXPECT_EQ(b.size(), 600);
  EXPECT_EQ(b.c_str()[b.size()], '\0');
  EXPECT_EQ(b[0], 'b');
  EXPECT_EQ(b[299], 'b');
  EXPECT_EQ(b[300], 'c');
  EXPECT_EQ(b[599], 'c');
}

TEST(ustring, LiteralOperator)
{
  const UString from_literal_op = "literal_operator_test_string"_ustr;
  const UString from_ctor("literal_operator_test_string");
  const UString from_std_string(std::string("literal_operator_test_string"));

  EXPECT_EQ(from_literal_op.c_str(), from_ctor.c_str());
  EXPECT_EQ(from_literal_op.c_str(), from_std_string.c_str());
  EXPECT_EQ(from_literal_op, from_ctor);

  /* Using the literal operator on the exact same literal a second time must hit its own
   * per-instantiation cache and still agree with the table. */
  const UString from_literal_op_again = "literal_operator_test_string"_ustr;
  EXPECT_EQ(from_literal_op.c_str(), from_literal_op_again.c_str());
}

TEST(ustring, Hashing)
{
  Map<UString, int> map;
  map.add("hashing_test_alpha"_ustr, 1);
  map.add("hashing_test_beta"_ustr, 2);

  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.lookup("hashing_test_alpha"_ustr), 1);
  EXPECT_EQ(map.lookup("hashing_test_beta"_ustr), 2);
  EXPECT_EQ(map.lookup_as("hashing_test_alpha"), 1);
}

TEST(ustring, Concurrency)
{
  BLI_task_scheduler_init();

  constexpr int64_t total = 20000;
  Vector<UString> results(total);

  threading::parallel_for(IndexRange(total), 1, [&](const IndexRange sub_range) {
    for (const int64_t i : sub_range) {
      if (i % 2 == 0) {
        /* Independently allocated on each iteration/thread, but should all intern to the same
         * entry: this is the actual thing being tested. */
        const std::string matching = std::string("concurrency_test_shared_string");
        results[i] = UString(matching);
      }
      else {
        /* A string that is unique to this iteration, to make sure concurrent insertion of
         * different strings doesn't corrupt the table or cross-contaminate entries. */
        const std::string number_str = std::to_string(i);
        results[i] = UString(StringRef(number_str));
      }
    }
  });

  const char *shared_ptr = nullptr;
  for (const int i : IndexRange(total)) {
    if (i % 2 == 0) {
      if (shared_ptr == nullptr) {
        shared_ptr = results[i].c_str();
      }
      else {
        EXPECT_EQ(results[i].c_str(), shared_ptr);
      }
    }
    else {
      const std::string number_str = std::to_string(i);
      EXPECT_EQ(results[i], StringRef(number_str));
    }
  }
}

TEST(ustring, ConcurrentSameNewString)
{
  /* Every thread interns the same list of strings, none of which exist yet, so the threads race
   * to add the *same* new string. */
  constexpr int THREADS_NUM = 8;
  constexpr int STRINGS_NUM = 200;

  Vector<std::string> strings;
  for (const int i : IndexRange(STRINGS_NUM)) {
    strings.append("concurrent_same_new_string_" + std::to_string(i));
  }

  Array<Array<UString>> results(THREADS_NUM);
  for (const int thread_i : IndexRange(THREADS_NUM)) {
    results[thread_i].reinitialize(STRINGS_NUM);
  }

  std::atomic<int> ready_num = 0;
  Vector<std::thread> threads;
  for (const int thread_i : IndexRange(THREADS_NUM)) {
    threads.append(std::thread([&, thread_i]() {
      /* Start interning at the same time in all threads, to maximize contention. */
      ready_num.fetch_add(1);
      while (ready_num.load() < THREADS_NUM) {
      }
      for (const int i : IndexRange(STRINGS_NUM)) {
        results[thread_i][i] = UString(strings[i]);
      }
    }));
  }
  for (std::thread &thread : threads) {
    thread.join();
  }

  for (const int i : IndexRange(STRINGS_NUM)) {
    const UString &expected = results[0][i];
    EXPECT_EQ(expected, StringRef(strings[i]));
    /* The UString for each thread should reference the same data. */
    for (const int thread_i : IndexRange(THREADS_NUM)) {
      EXPECT_EQ(results[thread_i][i].c_str(), expected.c_str());
      EXPECT_EQ(results[thread_i][i], expected);
    }
  }
}

}  // namespace blender::tests
