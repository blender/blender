/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_ressource_strings.h"
#include "BLI_string.h"

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
    link = (Link *)listbase->first;
    TESTFAIL(link->prev == nullptr);

    link = (Link *)listbase->last;
    TESTFAIL(link->next == nullptr);

    prev = nullptr;
    link = (Link *)listbase->first;
    do {
      TESTFAIL(link->prev == prev);
    } while ((void)(prev = link), (link = link->next));
    TESTFAIL(prev == listbase->last);

    prev = nullptr;
    link = (Link *)listbase->last;
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
  ListBase lb;
  void *link1 = MEM_callocN(sizeof(Link), "link1");
  void *link2 = MEM_callocN(sizeof(Link), "link2");

  /* Empty list */
  BLI_listbase_clear(&lb);
  EXPECT_EQ(BLI_findlink(&lb, -1), (void *)nullptr);
  EXPECT_EQ(BLI_findlink(&lb, 0), (void *)nullptr);
  EXPECT_EQ(BLI_findlink(&lb, 1), (void *)nullptr);
  EXPECT_EQ(BLI_rfindlink(&lb, -1), (void *)nullptr);
  EXPECT_EQ(BLI_rfindlink(&lb, 0), (void *)nullptr);
  EXPECT_EQ(BLI_rfindlink(&lb, 1), (void *)nullptr);
  EXPECT_EQ(BLI_findindex(&lb, link1), -1);

  /* One link */
  BLI_addtail(&lb, link1);
  EXPECT_EQ(BLI_findlink(&lb, 0), link1);
  EXPECT_EQ(BLI_rfindlink(&lb, 0), link1);
  EXPECT_EQ(BLI_findindex(&lb, link1), 0);

  /* Two links */
  BLI_addtail(&lb, link2);
  EXPECT_EQ(BLI_findlink(&lb, 1), link2);
  EXPECT_EQ(BLI_rfindlink(&lb, 0), link2);
  EXPECT_EQ(BLI_findindex(&lb, link2), 1);

  BLI_freelistN(&lb);
}

/* -------------------------------------------------------------------- */
/* Sort utilities & test */

static int testsort_array_str_cmp(const void *a, const void *b)
{
  int i = strcmp(*(const char **)a, *(const char **)b);
  return (i > 0) ? 1 : (i < 0) ? -1 : 0;
}

static int testsort_listbase_str_cmp(const void *a, const void *b)
{
  const LinkData *link_a = (LinkData *)a;
  const LinkData *link_b = (LinkData *)b;
  int i = strcmp((const char *)link_a->data, (const char *)link_b->data);
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
static bool testsort_listbase_array_str_cmp(ListBase *lb, char **arr, int arr_tot)
{
  LinkData *link_step;
  int i;

  link_step = (LinkData *)lb->first;
  for (i = 0; i < arr_tot; i++) {
    if (strcmp(arr[i], (char *)link_step->data) != 0) {
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
static bool testsort_listbase_sort_is_stable(ListBase *lb, bool forward)
{
  LinkData *link_step;

  link_step = (LinkData *)lb->first;
  while (link_step && link_step->next) {
    if (strcmp((const char *)link_step->data, (const char *)link_step->next->data) == 0) {
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
  int words_tot;
  char **words_arr; /* qsort for comparison */
  int i;
  char *w_step;
  ListBase words_lb;
  LinkData *words_linkdata_arr;

  /* delimit words */
  words_tot = 1 + char_switch(words, ' ', '\0');

  words_arr = (char **)MEM_mallocN(sizeof(*words_arr) * words_tot, __func__);

  words_linkdata_arr = (LinkData *)MEM_mallocN(sizeof(*words_linkdata_arr) * words_tot, __func__);

  /* create array */
  w_step = words;
  for (i = 0; i < words_tot; i++) {
    words_arr[i] = w_step;
    w_step += strlen(w_step) + 1;
  }

  /* sort empty list */
  {
    BLI_listbase_clear(&words_lb);
    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp);
    EXPECT_TRUE(listbase_is_valid(&words_lb));
  }

  /* sort single single */
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
  for (i = 0; i < words_tot; i++) {
    LinkData *link = &words_linkdata_arr[i];
    link->data = w_step;
    BLI_addtail(&words_lb, link);
    w_step += strlen(w_step) + 1;
  }
  EXPECT_TRUE(listbase_is_valid(&words_lb));

  /* sort (forward) */
  {
    qsort(words_arr, words_tot, sizeof(*words_arr), testsort_array_str_cmp);

    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp);
    EXPECT_TRUE(listbase_is_valid(&words_lb));
    EXPECT_TRUE(testsort_listbase_array_str_cmp(&words_lb, words_arr, words_tot));
    EXPECT_TRUE(testsort_listbase_sort_is_stable(&words_lb, true));
  }

  /* sort (reverse) */
  {
    qsort(words_arr, words_tot, sizeof(*words_arr), testsort_array_str_cmp_reverse);

    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp_reverse);
    EXPECT_TRUE(listbase_is_valid(&words_lb));
    EXPECT_TRUE(testsort_listbase_array_str_cmp(&words_lb, words_arr, words_tot));
    EXPECT_TRUE(testsort_listbase_sort_is_stable(&words_lb, true));
  }

  /* sort (forward but after reversing, test stability in alternate direction) */
  {
    BLI_array_reverse(words_arr, words_tot);
    BLI_listbase_reverse(&words_lb);

    EXPECT_TRUE(listbase_is_valid(&words_lb));
    EXPECT_TRUE(testsort_listbase_array_str_cmp(&words_lb, words_arr, words_tot));
    EXPECT_TRUE(testsort_listbase_sort_is_stable(&words_lb, false));

    /* and again */
    BLI_array_reverse(words_arr, words_tot);
    BLI_listbase_sort(&words_lb, testsort_listbase_str_cmp_reverse);
    EXPECT_TRUE(testsort_listbase_array_str_cmp(&words_lb, words_arr, words_tot));
    EXPECT_TRUE(testsort_listbase_sort_is_stable(&words_lb, false));
  }

  MEM_freeN(words);
  MEM_freeN(words_arr);
  MEM_freeN(words_linkdata_arr);
}
