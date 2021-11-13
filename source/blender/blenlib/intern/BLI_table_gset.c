#include "MEM_guardedalloc.h"

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#
#include "BLI_smallhash.h"
#include "BLI_utildefines.h"

#include "BLI_ghash.h"

//#define PTR_TO_IDX(ts) ((GHash *)ts->ptr_to_idx.buckets)
#define PTR_TO_IDX(ts) &(ts)->ptr_to_idx

TableGSet *BLI_table_gset_new(const char *info)
{
  TableGSet *ts = MEM_callocN(sizeof(TableGSet), info);

  // ts->ptr_to_idx.buckets = (void *)BLI_ghash_ptr_new(info);
  BLI_smallhash_init(&ts->ptr_to_idx);

  return ts;
}

TableGSet *BLI_table_gset_new_ex(const char *info, int size)
{
  TableGSet *ts = MEM_callocN(sizeof(TableGSet), info);

  // ts->ptr_to_idx.buckets = (void *)BLI_ghash_ptr_new_ex(info, (uint)size);
  BLI_smallhash_init_ex(&ts->ptr_to_idx, size);

  if (size) {
    ts->elems = MEM_callocN(sizeof(void *) * (uint)size, info);
    ts->size = size;
    ts->length = 0;
    ts->cur = 0;
  }

  return ts;
}

void BLI_table_gset_free(TableGSet *ts, GHashKeyFreeFP freefp)
{
  if (!PTR_TO_IDX(ts)) {
    return;
  }

  if (ts->elems) {
    MEM_freeN(ts->elems);
  }

  // BLI_ghash_free(PTR_TO_IDX(ts), freefp, NULL);
  BLI_smallhash_release(&ts->ptr_to_idx);

  MEM_freeN(ts);
}

static void table_gset_resize(TableGSet *ts)
{
  if (ts->cur >= ts->size) {
    uint newsize = (uint)(ts->cur + 1);
    newsize = (newsize << 1U) - (newsize >> 1U);
    newsize = MAX2(newsize, 8U);

    if (!ts->elems) {
      ts->elems = (void *)MEM_mallocN(sizeof(void *) * newsize, "ts->elems");
    }
    else {
      ts->elems = (void *)MEM_reallocN(ts->elems, newsize * sizeof(void *));
    }

    // BLI_smallhash_clear(PTR_TO_IDX(ts), 0ULL);

    // compact
    int i = 0, j = 0;
    for (i = 0; i < ts->cur; i++) {
      void *elem2 = ts->elems[i];

      if (elem2) {
        void **val;
        BLI_smallhash_ensure_p(PTR_TO_IDX(ts), (uintptr_t)elem2, &val);

        // BLI_smallhash_insert(PTR_TO_IDX(ts), elem2, (void *)j);
        *val = POINTER_FROM_INT(j);

        ts->elems[j++] = elem2;
      }
    }

    ts->size = (int)newsize;
    ts->cur = j;
  }
}

bool BLI_table_gset_add(TableGSet *ts, void *elem)
{
  void **val;

  table_gset_resize(ts);

  bool ret = BLI_smallhash_ensure_p(PTR_TO_IDX(ts), (uintptr_t)elem, &val);

  if (!ret) {
    *val = ts->cur;

    ts->elems[ts->cur++] = elem;
    ts->length++;
  }

  return ret;
}

void BLI_table_gset_insert(TableGSet *ts, void *elem)
{
  table_gset_resize(ts);

  BLI_smallhash_insert(PTR_TO_IDX(ts), elem, (void *)ts->cur);

  ts->elems[ts->cur++] = elem;
  ts->length++;
}

void BLI_table_gset_remove(TableGSet *ts, void *elem, GHashKeyFreeFP freefp)
{
  if (!elem || !ts) {
    return;
  }

  int *idx = (int *)BLI_smallhash_lookup_p(PTR_TO_IDX(ts), elem);
  if (!idx) {
    return;
  }

  int idx2 = *idx;

  BLI_smallhash_remove(PTR_TO_IDX(ts), elem);

  if (!ts->elems || ts->elems[idx2] != elem) {
    return;
  }

  ts->length--;
  ts->elems[idx2] = NULL;
}

bool BLI_table_gset_haskey(TableGSet *ts, void *elem)
{
  return BLI_smallhash_haskey(PTR_TO_IDX(ts), elem);
}

int BLI_table_gset_len(TableGSet *ts)
{
  return ts->length;
}
