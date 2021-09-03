#include "MEM_guardedalloc.h"

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"

#include "BLI_smallhash.h"
#include "BLI_utildefines.h"

#include "BLI_ghash.h"

#define PTR_TO_IDX(ts) ((GHash *)ts->ptr_to_idx.buckets)

TableGSet *BLI_table_gset_new(const char *info)
{
  TableGSet *ts = MEM_callocN(sizeof(TableGSet), info);

  ts->ptr_to_idx.buckets = (void *)BLI_ghash_ptr_new(info);

  return ts;
}

TableGSet *BLI_table_gset_new_ex(const char *info, int size)
{
  TableGSet *ts = MEM_callocN(sizeof(TableGSet), info);

  ts->ptr_to_idx.buckets = (void *)BLI_ghash_ptr_new_ex(info, (uint)size);
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

  BLI_ghash_free(PTR_TO_IDX(ts), freefp, NULL);

  MEM_freeN(ts);
}

bool BLI_table_gset_add(TableGSet *ts, void *elem)
{
  if (BLI_table_gset_haskey(ts, elem)) {
    return true;
  }

  BLI_table_gset_insert(ts, elem);
  return false;
}

void BLI_table_gset_insert(TableGSet *ts, void *elem)
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

    BLI_ghash_clear(PTR_TO_IDX(ts), NULL, NULL);

    // compact
    int i = 0, j = 0;
    for (i = 0; i < ts->cur; i++) {
      void *elem2 = ts->elems[i];

      if (elem2) {
        BLI_ghash_insert(PTR_TO_IDX(ts), elem2, (void *)j);
        ts->elems[j++] = elem2;
      }
    }

    ts->size = (int)newsize;
    ts->cur = j;
  }

  BLI_ghash_insert(PTR_TO_IDX(ts), elem, (void *)ts->cur);
  ts->elems[ts->cur++] = elem;
  ts->length++;
}

void BLI_table_gset_remove(TableGSet *ts, void *elem, GHashKeyFreeFP freefp)
{
  if (!elem || !ts) {
    return;
  }

  int *idx = (int *)BLI_ghash_lookup_p(PTR_TO_IDX(ts), elem);
  if (!idx) {
    return;
  }

  int idx2 = *idx;

  BLI_ghash_remove(PTR_TO_IDX(ts), elem, freefp, NULL);

  if (!ts->elems || ts->elems[idx2] != elem) {
    return;
  }

  ts->length--;
  ts->elems[idx2] = NULL;
}

bool BLI_table_gset_haskey(TableGSet *ts, void *elem)
{
  return BLI_ghash_haskey(PTR_TO_IDX(ts), elem);
}

int BLI_table_gset_len(TableGSet *ts)
{
  return ts->length;
}
