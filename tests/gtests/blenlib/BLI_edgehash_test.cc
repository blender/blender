/* Apache License, Version 2.0 */

#include "testing/testing.h"
#include <algorithm>
#include <random>
#include <vector>

extern "C" {
#include "BLI_edgehash.h"
#include "BLI_utildefines.h"
}

#define VALUE_1 POINTER_FROM_INT(1)
#define VALUE_2 POINTER_FROM_INT(2)
#define VALUE_3 POINTER_FROM_INT(3)

TEST(edgehash, InsertIncreasesLength)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  ASSERT_EQ(BLI_edgehash_len(eh), 0);
  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, ReinsertNewIncreasesLength)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  ASSERT_EQ(BLI_edgehash_len(eh), 0);
  BLI_edgehash_reinsert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, ReinsertExistingDoesNotIncreaseLength)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  ASSERT_EQ(BLI_edgehash_len(eh), 0);
  BLI_edgehash_reinsert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);
  BLI_edgehash_reinsert(eh, 1, 2, VALUE_2);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);
  BLI_edgehash_reinsert(eh, 2, 1, VALUE_2);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, ReinsertCanChangeValue)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_lookup(eh, 1, 2), VALUE_1);
  BLI_edgehash_reinsert(eh, 2, 1, VALUE_2);
  ASSERT_EQ(BLI_edgehash_lookup(eh, 1, 2), VALUE_2);
  BLI_edgehash_reinsert(eh, 1, 2, VALUE_3);
  ASSERT_EQ(BLI_edgehash_lookup(eh, 2, 1), VALUE_3);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, LookupExisting)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_lookup(eh, 1, 2), VALUE_1);
  ASSERT_EQ(BLI_edgehash_lookup(eh, 2, 1), VALUE_1);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, LookupNonExisting)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  ASSERT_EQ(BLI_edgehash_lookup(eh, 1, 2), nullptr);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, LookupNonExistingWithDefault)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  ASSERT_EQ(BLI_edgehash_lookup_default(eh, 1, 2, VALUE_1), VALUE_1);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, LookupExistingWithDefault)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_lookup_default(eh, 1, 2, VALUE_2), VALUE_1);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, LookupPExisting)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  void *value = VALUE_1;
  BLI_edgehash_insert(eh, 1, 2, value);
  void **value_p = BLI_edgehash_lookup_p(eh, 1, 2);
  ASSERT_EQ(*value_p, VALUE_1);
  *value_p = VALUE_2;
  ASSERT_EQ(BLI_edgehash_lookup(eh, 1, 2), VALUE_2);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, LookupPNonExisting)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  ASSERT_EQ(BLI_edgehash_lookup_p(eh, 1, 2), nullptr);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, EnsurePNonExisting)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  void **value_p;
  bool existed = BLI_edgehash_ensure_p(eh, 1, 2, &value_p);
  ASSERT_FALSE(existed);
  *value_p = VALUE_1;
  ASSERT_EQ(BLI_edgehash_lookup(eh, 1, 2), VALUE_1);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, EnsurePExisting)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  void **value_p;
  bool existed = BLI_edgehash_ensure_p(eh, 1, 2, &value_p);
  ASSERT_TRUE(existed);
  ASSERT_EQ(*value_p, VALUE_1);
  *value_p = VALUE_2;
  ASSERT_EQ(BLI_edgehash_lookup(eh, 1, 2), VALUE_2);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, RemoveExistingDecreasesLength)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);
  bool has_been_removed = BLI_edgehash_remove(eh, 1, 2, nullptr);
  ASSERT_EQ(BLI_edgehash_len(eh), 0);
  ASSERT_TRUE(has_been_removed);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, RemoveNonExistingDoesNotDecreaseLength)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);
  bool has_been_removed = BLI_edgehash_remove(eh, 4, 5, nullptr);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);
  ASSERT_FALSE(has_been_removed);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, PopKeyTwice)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_popkey(eh, 1, 2), VALUE_1);
  ASSERT_EQ(BLI_edgehash_popkey(eh, 1, 2), nullptr);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, LookupInvertedIndices)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_EQ(BLI_edgehash_lookup(eh, 2, 1), VALUE_1);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, HasKeyExisting)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  ASSERT_TRUE(BLI_edgehash_haskey(eh, 1, 2));
  ASSERT_TRUE(BLI_edgehash_haskey(eh, 2, 1));

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, HasKeyNonExisting)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  ASSERT_FALSE(BLI_edgehash_haskey(eh, 1, 2));

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, ClearSetsLengthToZero)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  BLI_edgehash_insert(eh, 1, 2, VALUE_2);
  ASSERT_EQ(BLI_edgehash_len(eh), 2);
  BLI_edgehash_clear(eh, nullptr);
  ASSERT_EQ(BLI_edgehash_len(eh), 0);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, IteratorFindsAllValues)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  BLI_edgehash_insert(eh, 1, 3, VALUE_2);
  BLI_edgehash_insert(eh, 1, 4, VALUE_3);

  EdgeHashIterator *ehi = BLI_edgehashIterator_new(eh);
  auto a = BLI_edgehashIterator_getValue(ehi);
  BLI_edgehashIterator_step(ehi);
  auto b = BLI_edgehashIterator_getValue(ehi);
  BLI_edgehashIterator_step(ehi);
  auto c = BLI_edgehashIterator_getValue(ehi);
  BLI_edgehashIterator_step(ehi);

  ASSERT_NE(a, b);
  ASSERT_NE(b, c);
  ASSERT_NE(a, c);
  ASSERT_TRUE(ELEM(a, VALUE_1, VALUE_2, VALUE_3));
  ASSERT_TRUE(ELEM(b, VALUE_1, VALUE_2, VALUE_3));
  ASSERT_TRUE(ELEM(c, VALUE_1, VALUE_2, VALUE_3));

  BLI_edgehashIterator_free(ehi);
  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, IterateIsDone)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  BLI_edgehash_insert(eh, 1, 3, VALUE_2);
  BLI_edgehash_insert(eh, 1, 4, VALUE_3);

  EdgeHashIterator *ehi = BLI_edgehashIterator_new(eh);
  ASSERT_FALSE(BLI_edgehashIterator_isDone(ehi));
  BLI_edgehashIterator_step(ehi);
  ASSERT_FALSE(BLI_edgehashIterator_isDone(ehi));
  BLI_edgehashIterator_step(ehi);
  ASSERT_FALSE(BLI_edgehashIterator_isDone(ehi));
  BLI_edgehashIterator_step(ehi);
  ASSERT_TRUE(BLI_edgehashIterator_isDone(ehi));

  BLI_edgehashIterator_free(ehi);
  BLI_edgehash_free(eh, nullptr);
}

TEST(edgehash, DoubleRemove)
{
  EdgeHash *eh = BLI_edgehash_new(__func__);

  BLI_edgehash_insert(eh, 1, 2, VALUE_1);
  BLI_edgehash_insert(eh, 1, 3, VALUE_2);
  BLI_edgehash_insert(eh, 1, 4, VALUE_3);
  ASSERT_EQ(BLI_edgehash_len(eh), 3);

  BLI_edgehash_remove(eh, 1, 2, nullptr);
  BLI_edgehash_remove(eh, 1, 3, nullptr);
  ASSERT_EQ(BLI_edgehash_len(eh), 1);

  BLI_edgehash_free(eh, nullptr);
}

struct Edge {
  uint v1, v2;
};

TEST(edgehash, StressTest)
{
  std::srand(0);
  int amount = 10000;

  std::vector<Edge> edges;
  for (int i = 0; i < amount; i++) {
    edges.push_back({(uint)i, amount + (uint)std::rand() % 12345});
  }

  EdgeHash *eh = BLI_edgehash_new(__func__);

  /* first insert all the edges */
  for (int i = 0; i < edges.size(); i++) {
    BLI_edgehash_insert(eh, edges[i].v1, edges[i].v2, POINTER_FROM_INT(i));
  }

  std::vector<Edge> shuffled = edges;
  std::shuffle(shuffled.begin(), shuffled.end(), std::default_random_engine());

  /* then remove half of them */
  int remove_until = shuffled.size() / 2;
  for (int i = 0; i < remove_until; i++) {
    BLI_edgehash_remove(eh, shuffled[i].v2, shuffled[i].v1, nullptr);
  }

  ASSERT_EQ(BLI_edgehash_len(eh), edges.size() - remove_until);

  /* check if the right ones have been removed */
  for (int i = 0; i < shuffled.size(); i++) {
    bool haskey = BLI_edgehash_haskey(eh, shuffled[i].v1, shuffled[i].v2);
    if (i < remove_until)
      ASSERT_FALSE(haskey);
    else
      ASSERT_TRUE(haskey);
  }

  /* reinsert all edges */
  for (int i = 0; i < edges.size(); i++) {
    BLI_edgehash_reinsert(eh, edges[i].v1, edges[i].v2, POINTER_FROM_INT(i));
  }

  ASSERT_EQ(BLI_edgehash_len(eh), edges.size());

  /* pop all edges */
  for (int i = 0; i < edges.size(); i++) {
    int value = POINTER_AS_INT(BLI_edgehash_popkey(eh, edges[i].v1, edges[i].v2));
    ASSERT_EQ(i, value);
  }

  ASSERT_EQ(BLI_edgehash_len(eh), 0);

  BLI_edgehash_free(eh, nullptr);
}

TEST(edgeset, AddNonExistingIncreasesLength)
{
  EdgeSet *es = BLI_edgeset_new(__func__);

  ASSERT_EQ(BLI_edgeset_len(es), 0);
  BLI_edgeset_add(es, 1, 2);
  ASSERT_EQ(BLI_edgeset_len(es), 1);
  BLI_edgeset_add(es, 1, 3);
  ASSERT_EQ(BLI_edgeset_len(es), 2);
  BLI_edgeset_add(es, 1, 4);
  ASSERT_EQ(BLI_edgeset_len(es), 3);

  BLI_edgeset_free(es);
}

TEST(edgeset, AddExistingDoesNotIncreaseLength)
{
  EdgeSet *es = BLI_edgeset_new(__func__);

  ASSERT_EQ(BLI_edgeset_len(es), 0);
  BLI_edgeset_add(es, 1, 2);
  ASSERT_EQ(BLI_edgeset_len(es), 1);
  BLI_edgeset_add(es, 2, 1);
  ASSERT_EQ(BLI_edgeset_len(es), 1);
  BLI_edgeset_add(es, 1, 2);
  ASSERT_EQ(BLI_edgeset_len(es), 1);

  BLI_edgeset_free(es);
}

TEST(edgeset, HasKeyNonExisting)
{
  EdgeSet *es = BLI_edgeset_new(__func__);

  ASSERT_FALSE(BLI_edgeset_haskey(es, 1, 2));

  BLI_edgeset_free(es);
}

TEST(edgeset, HasKeyExisting)
{
  EdgeSet *es = BLI_edgeset_new(__func__);

  BLI_edgeset_insert(es, 1, 2);
  ASSERT_TRUE(BLI_edgeset_haskey(es, 1, 2));

  BLI_edgeset_free(es);
}
