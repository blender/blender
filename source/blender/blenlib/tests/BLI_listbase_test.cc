/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_ressource_strings.h"
#include "BLI_string.h"

namespace blender {

/* local validation function */
static bool listbase_is_valid(const ListBase *listbase)
{
#define TESTFAIL(test) \
  if (!(test)) { \
    goto fail; \
  } \
  ((void)0)

  if (listbase->first) {
    const Link *prev, *link;
    link = static_cast<Link *>(listbase->first);
    TESTFAIL(link->prev == nullptr);

    link = static_cast<Link *>(listbase->last);
    TESTFAIL(link->next == nullptr);

    prev = nullptr;
    link = static_cast<Link *>(listbase->first);
    do {
      TESTFAIL(link->prev == prev);
    } while ((void)(prev = link), (link = link->next));
    TESTFAIL(prev == listbase->last);

    prev = nullptr;
    link = static_cast<Link *>(listbase->last);
    do {
      TESTFAIL(link->next == prev);
    } while ((void)(prev = link), (link = link->prev));
    TESTFAIL(prev == listbase->first);
  }
  else {
    TESTFAIL(listbase->last == nullptr);
  }
#undef TESTFAIL

  return true;

fail:
  return false;
}

static int char_switch(char *string, char ch_src, char ch_dst)
{
  int tot = 0;
  while (*string != 0) {
    if (*string == ch_src) {
      *string = ch_dst;
      tot++;
    }
    string++;
  }
  return tot;
}

TEST(listbase, FindLinkOrIndex)
{
  ListBaseT<Link> lb;
  void *link1 = MEM_callocN<Link>("link1");
  void *link2 = MEM_callocN<Link>("link2");

  /* Empty list */
  BLI_listbase_clear(&lb);
  EXPECT_EQ(BLI_findlink(&lb, -1), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_findlink(&lb, 0), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_findlink(&lb, 1), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_rfindlink(&lb, -1), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_rfindlink(&lb, 0), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_rfindlink(&lb, 1), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_findindex(&lb, link1), -1);
  EXPECT_EQ(BLI_findlinkfrom(static_cast<Link *>(lb.first), -1), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_findlinkfrom(static_cast<Link *>(lb.first), 0), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_findlinkfrom(static_cast<Link *>(lb.first), 1), static_cast<void *>(nullptr));

  /* One link */
  BLI_addtail(&lb, link1);
  EXPECT_EQ(BLI_findlink(&lb, 0), link1);
  EXPECT_EQ(BLI_rfindlink(&lb, 0), link1);
  EXPECT_EQ(BLI_findindex(&lb, link1), 0);
  EXPECT_EQ(BLI_findlinkfrom(static_cast<Link *>(lb.first), 0), link1);

  /* Two links */
  BLI_addtail(&lb, link2);
  EXPECT_EQ(BLI_findlink(&lb, 1), link2);
  EXPECT_EQ(BLI_rfindlink(&lb, 0), link2);
  EXPECT_EQ(BLI_findindex(&lb, link2), 1);
  EXPECT_EQ(BLI_findlinkfrom(static_cast<Link *>(lb.first), 1), link2);

  /* After end of list */
  EXPECT_EQ(BLI_findlinkfrom(static_cast<Link *>(lb.first), 2), static_cast<void *>(nullptr));

  BLI_freelistN(&lb);
}

TEST(listbase, FindLinkFromStringOrPointer)
{
  struct TestLink {
    TestLink *next, *prev;
    char name[64];
    const void *ptr;
  };

  const char *const link1_name = "Link1";
  const char *const link2_name = "Link2";
  const void *const link1_ptr = nullptr;
  const void *const link2_ptr = link2_name;

  const size_t name_offset = offsetof(TestLink, name);
  const size_t ptr_offset = offsetof(TestLink, ptr);

  ListBaseT<TestLink> lb;
  TestLink *link1 = MEM_callocN<TestLink>("link1");
  STRNCPY(link1->name, link1_name);
  link1->ptr = link1_ptr;
  TestLink *link2 = MEM_callocN<TestLink>("link2");
  STRNCPY(link2->name, link2_name);
  link2->ptr = link2_ptr;

  /* Empty list */
  BLI_listbase_clear(&lb);
  EXPECT_EQ(BLI_findptr(&lb, link1_ptr, ptr_offset), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_findstring(&lb, link1_name, name_offset), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_rfindptr(&lb, link1_ptr, ptr_offset), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_rfindstring(&lb, link1_name, name_offset), static_cast<void *>(nullptr));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, link1_name, name_offset, 0),
            static_cast<void *>(nullptr));

  /* One link */
  BLI_addtail(&lb, link1);
  EXPECT_EQ(BLI_findptr(&lb, link1_ptr, ptr_offset), static_cast<void *>(link1));
  EXPECT_EQ(BLI_findstring(&lb, link1_name, name_offset), static_cast<void *>(link1));
  EXPECT_EQ(BLI_rfindptr(&lb, link1_ptr, ptr_offset), static_cast<void *>(link1));
  EXPECT_EQ(BLI_rfindstring(&lb, link1_name, name_offset), static_cast<void *>(link1));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, link1_name, name_offset, 0),
            static_cast<void *>(link1));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, "", name_offset, 0),
            static_cast<void *>(link1));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, nullptr, name_offset, 0),
            static_cast<void *>(link1));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, nullptr, name_offset, 1),
            static_cast<void *>(nullptr));

  /* Two links */
  BLI_addtail(&lb, link2);
  EXPECT_EQ(BLI_findptr(&lb, link1_ptr, ptr_offset), static_cast<void *>(link1));
  EXPECT_EQ(BLI_findstring(&lb, link1_name, name_offset), static_cast<void *>(link1));
  EXPECT_EQ(BLI_rfindptr(&lb, link1_ptr, ptr_offset), static_cast<void *>(link1));
  EXPECT_EQ(BLI_rfindstring(&lb, link1_name, name_offset), static_cast<void *>(link1));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, link1_name, name_offset, 0),
            static_cast<void *>(link1));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, link2_name, name_offset, 0),
            static_cast<void *>(link2));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, nullptr, name_offset, 0),
            static_cast<void *>(link1));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, nullptr, name_offset, 1),
            static_cast<void *>(link2));
  EXPECT_EQ(BLI_listbase_string_or_index_find(&lb, nullptr, name_offset, -1),
            static_cast<void *>(nullptr));

  BLI_freelistN(&lb);
}

TEST(listbase, FromLink)
{
  ListBaseT<Link> lb = {nullptr, nullptr};
  Link *link1 = MEM_callocN<Link>("link1");
  Link *link2 = MEM_callocN<Link>("link2");
  Link *link3 = MEM_callocN<Link>("link3");

  /* Null safety. */
  EXPECT_EQ(lb, BLI_listbase_from_link(nullptr));

  /* One link. */
  BLI_addtail(&lb, link1);
  EXPECT_EQ(lb, BLI_listbase_from_link(link1));

  /* Two links. */
  BLI_addtail(&lb, link2);
  EXPECT_EQ(lb, BLI_listbase_from_link(link2));

  /* Three links, search from middle. */
  BLI_addtail(&lb, link3);
  EXPECT_EQ(lb, BLI_listbase_from_link(link2));

  BLI_freelistN(&lb);
}

TEST(listbase, SplitAfter)
{
  ListBaseT<Link> lb;
  ListBaseT<Link> split_after_lb;
  void *link1 = MEM_callocN<Link>("link1");
  void *link2 = MEM_callocN<Link>("link2");

  /* Empty list */
  BLI_listbase_clear(&lb);
  BLI_listbase_clear(&split_after_lb);

  BLI_listbase_split_after(&lb, &split_after_lb, nullptr);
  EXPECT_EQ(BLI_listbase_is_empty(&split_after_lb), true);

  /* One link */
  BLI_listbase_clear(&lb);
  BLI_listbase_clear(&split_after_lb);
  BLI_addtail(&lb, link1);

  BLI_listbase_split_after(&lb, &split_after_lb, nullptr);
  EXPECT_EQ(BLI_listbase_is_empty(&lb), true);
  EXPECT_EQ(BLI_listbase_count(&split_after_lb), 1);
  EXPECT_EQ(BLI_findindex(&split_after_lb, link1), 0);
  EXPECT_EQ(split_after_lb.first, link1);
  EXPECT_EQ(split_after_lb.last, link1);

  BLI_listbase_clear(&lb);
  BLI_listbase_clear(&split_after_lb);
  BLI_addtail(&lb, link1);

  BLI_listbase_split_after(&lb, &split_after_lb, link1);
  EXPECT_EQ(BLI_listbase_count(&lb), 1);
  EXPECT_EQ(BLI_findindex(&lb, link1), 0);
  EXPECT_EQ(lb.first, link1);
  EXPECT_EQ(lb.last, link1);
  EXPECT_EQ(BLI_listbase_is_empty(&split_after_lb), true);

  /* Two links */
  BLI_listbase_clear(&lb);
  BLI_listbase_clear(&split_after_lb);
  BLI_addtail(&lb, link1);
  BLI_addtail(&lb, link2);

  BLI_listbase_split_after(&lb, &split_after_lb, nullptr);
  EXPECT_EQ(BLI_listbase_is_empty(&lb), true);
  EXPECT_EQ(BLI_listbase_count(&split_after_lb), 2);
  EXPECT_EQ(BLI_findindex(&split_after_lb, link1), 0);
  EXPECT_EQ(BLI_findindex(&split_after_lb, link2), 1);
  EXPECT_EQ(split_after_lb.first, link1);
  EXPECT_EQ(split_after_lb.last, link2);

  BLI_listbase_clear(&lb);
  BLI_listbase_clear(&split_after_lb);
  BLI_addtail(&lb, link1);
  BLI_addtail(&lb, link2);

  BLI_listbase_split_after(&lb, &split_after_lb, link1);
  EXPECT_EQ(BLI_listbase_count(&lb), 1);
  EXPECT_EQ(BLI_findindex(&lb, link1), 0);
  EXPECT_EQ(lb.first, link1);
  EXPECT_EQ(lb.last, link1);
  EXPECT_EQ(BLI_listbase_count(&split_after_lb), 1);
  EXPECT_EQ(BLI_findindex(&split_after_lb, link2), 0);
  EXPECT_EQ(split_after_lb.first, link2);
  EXPECT_EQ(split_after_lb.last, link2);

  BLI_freelistN(&lb);
  BLI_freelistN(&split_after_lb);
}

TEST(listbase, EnumerateIterator)
{
  struct TestLink {
    TestLink *next, *prev;
    int value;
  };

  ListBaseT<TestLink> lb;
  BLI_listbase_clear(&lb);

  TestLink *link1 = MEM_callocN<TestLink>("link1");
  link1->value = 10;
  BLI_addtail(&lb, link1);

  TestLink *link2 = MEM_callocN<TestLink>("link2");
  link2->value = 20;
  BLI_addtail(&lb, link2);

  int count = 0;
  for (auto [i, link] : lb.enumerate()) {
    EXPECT_EQ(i, count);
    if (i == 0) {
      EXPECT_EQ(&link, link1);
      EXPECT_EQ(link.value, 10);
    }
    else if (i == 1) {
      EXPECT_EQ(&link, link2);
      EXPECT_EQ(link.value, 20);
    }
    count++;
  }
  EXPECT_EQ(count, 2);

  BLI_freelistN(&lb);
}

TEST(listbase, ReversedIterator)
{
  struct TestLink {
    TestLink *next, *prev;
    int value;
  };

  ListBaseT<TestLink> lb;
  BLI_listbase_clear(&lb);

  TestLink *link1 = MEM_callocN<TestLink>("link1");
  link1->value = 10;
  BLI_addtail(&lb, link1);

  TestLink *link2 = MEM_callocN<TestLink>("link2");
  link2->value = 20;
  BLI_addtail(&lb, link2);

  int count = 0;
  for (TestLink &link : lb.items_reversed()) {
    if (count == 0) {
      EXPECT_EQ(&link, link2);
    }
    else if (count == 1) {
      EXPECT_EQ(&link, link1);
    }
    count++;
  }
  EXPECT_EQ(count, 2);

  BLI_freelistN(&lb);
}

TEST(listbase, MutableIterator)
{
  struct TestLink {
    TestLink *next, *prev;
    int value;
  };

  ListBaseT<TestLink> lb;
  BLI_listbase_clear(&lb);

  TestLink *link1 = MEM_callocN<TestLink>("link1");
  BLI_addtail(&lb, link1);

  TestLink *link2 = MEM_callocN<TestLink>("link2");
  BLI_addtail(&lb, link2);

  TestLink *link3 = MEM_callocN<TestLink>("link3");
  BLI_addtail(&lb, link3);

  int count = 0;
  for (TestLink &link : lb.items_mutable()) {
    count++;
    if (&link == link2) {
      BLI_freelinkN(&lb, &link);
    }
  }
  EXPECT_EQ(count, 3);
  EXPECT_EQ(BLI_listbase_count(&lb), 2);
  EXPECT_EQ(lb.first, link1);
  EXPECT_EQ(lb.last, link3);

  BLI_freelistN(&lb);
}

TEST(listbase, MutableReversedIterator)
{
  struct TestLink {
    TestLink *next, *prev;
    int value;
  };

  ListBaseT<TestLink> lb;
  BLI_listbase_clear(&lb);

  TestLink *link1 = MEM_callocN<TestLink>("link1");
  BLI_addtail(&lb, link1);

  TestLink *link2 = MEM_callocN<TestLink>("link2");
  BLI_addtail(&lb, link2);

  TestLink *link3 = MEM_callocN<TestLink>("link3");
  BLI_addtail(&lb, link3);

  int count = 0;
  for (TestLink &link : lb.items_reversed_mutable()) {
    count++;
    if (&link == link2) {
      BLI_freelinkN(&lb, &link);
    }
  }
  EXPECT_EQ(count, 3);
  EXPECT_EQ(BLI_listbase_count(&lb), 2);
  EXPECT_EQ(lb.first, link1);
  EXPECT_EQ(lb.last, link3);

  BLI_freelistN(&lb);
}

/* -------------------------------------------------------------------- */
/* Sort utilities & test */

static int testsort_array_str_cmp(const void *a, const void *b)
{
  int i = strcmp(*static_cast<const char **>(const_cast<void *>(a)),
                 *static_cast<const char **>(const_cast<void *>(b)));
  return (i > 0) ? 1 : (i < 0) ? -1 : 0;
}

static int testsort_listbase_str_cmp(const void *a, const void *b)
{
  const LinkData *link_a = static_cast<LinkData *>(const_cast<void *>(a));
  const LinkData *link_b = static_cast<LinkData *>(const_cast<void *>(b));
  int i = strcmp(static_cast<const char *>(link_a->data), static_cast<const char *>(link_b->data));
  return (i > 0) ? 1 : (i < 0) ? -1 : 0;
}

static int testsort_array_str_cmp_reverse(const void *a, const void *b)
{
  return -testsort_array_str_cmp(a, b);
}

static int testsort_listbase_str_cmp_reverse(const void *a, const void *b)
{
  return -testsort_listbase_str_cmp(a, b);
}

/* check array and listbase compare */
static bool testsort_listbase_array_str_cmp(ListBaseT<LinkData> *lb, char **arr, int arr_num)
{
  LinkData *link_step;
  int i;

  link_step = static_cast<LinkData *>(lb->first);
  for (i = 0; i < arr_num; i++) {
    if (!STREQ(arr[i], (char *)link_step->data)) {
      return false;
    }
    link_step = link_step->next;
  }
  if (link_step) {
    return false;
  }

  return true;
}

/* assumes nodes are allocated in-order */
static bool testsort_listbase_sort_is_stable(ListBaseT<LinkData> *lb, bool forward)
{
  LinkData *link_step;

  link_step = static_cast<LinkData *>(lb->first);
  while (link_step && link_step->next) {
    if (STREQ((const char *)link_step->data, (const char *)link_step->next->data)) {
      if ((link_step < link_step->next) != forward) {
        return false;
      }
    }
    link_step = link_step->next;
  }
  return true;
}

TEST(listbase, Sort)
{
  const int words_len = sizeof(words10k) - 1;
  char *words = BLI_strdupn(words10k, words_len);
  int words_num;
  char **words_arr; /* qsort for comparison */
  int i;
  char *w_step;
  ListBaseT<LinkData> words_lb;
  LinkData *words_linkdata_arr;

  /* delimit words */
  words_num = 1 + char_switch(words, ' ', '\0');

  words_arr = MEM_malloc_arrayN<char *>(size_t(words_num), __func__);

  words_linkdata_arr = MEM_malloc_arrayN<LinkData>(size_t(words_num), __func__);

  /* create array */
  w_step = words;
  for (i = 0; i < words_num; i++) {
    words_arr[i] = w_step;
    w_step += strlen(w_step) + 1;
  }

  /* sort empty list */
  {
    BLI_listbase_clear(&words_lb);
    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp);
    EXPECT_TRUE(listbase_is_valid(&words_lb));
  }

  /* Sort single list. */
  {
    LinkData link;
    link.data = words;
    BLI_addtail(&words_lb, &link);
    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp);
    EXPECT_TRUE(listbase_is_valid(&words_lb));
    BLI_listbase_clear(&words_lb);
  }

  /* create listbase */
  BLI_listbase_clear(&words_lb);
  w_step = words;
  for (i = 0; i < words_num; i++) {
    LinkData *link = &words_linkdata_arr[i];
    link->data = w_step;
    BLI_addtail(&words_lb, link);
    w_step += strlen(w_step) + 1;
  }
  EXPECT_TRUE(listbase_is_valid(&words_lb));

  /* sort (forward) */
  {
    qsort(words_arr, words_num, sizeof(*words_arr), testsort_array_str_cmp);

    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp);
    EXPECT_TRUE(listbase_is_valid(&words_lb));
    EXPECT_TRUE(testsort_listbase_array_str_cmp(&words_lb, words_arr, words_num));
    EXPECT_TRUE(testsort_listbase_sort_is_stable(&words_lb, true));
  }

  /* sort (reverse) */
  {
    qsort(words_arr, words_num, sizeof(*words_arr), testsort_array_str_cmp_reverse);

    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp_reverse);
    EXPECT_TRUE(listbase_is_valid(&words_lb));
    EXPECT_TRUE(testsort_listbase_array_str_cmp(&words_lb, words_arr, words_num));
    EXPECT_TRUE(testsort_listbase_sort_is_stable(&words_lb, true));
  }

  /* sort (forward but after reversing, test stability in alternate direction) */
  {
    BLI_array_reverse(words_arr, words_num);
    BLI_listbase_reverse(&words_lb);

    EXPECT_TRUE(listbase_is_valid(&words_lb));
    EXPECT_TRUE(testsort_listbase_array_str_cmp(&words_lb, words_arr, words_num));
    EXPECT_TRUE(testsort_listbase_sort_is_stable(&words_lb, false));

    /* and again */
    BLI_array_reverse(words_arr, words_num);
    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp_reverse);
    EXPECT_TRUE(testsort_listbase_array_str_cmp(&words_lb, words_arr, words_num));
    EXPECT_TRUE(testsort_listbase_sort_is_stable(&words_lb, false));
  }

  MEM_freeN(words);
  MEM_freeN(words_arr);
  MEM_freeN(words_linkdata_arr);
}

}  // namespace blender
