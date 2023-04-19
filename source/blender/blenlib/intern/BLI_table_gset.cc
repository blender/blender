#include "MEM_guardedalloc.h"

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#
#include "BLI_smallhash.h"
#include "BLI_utildefines.h"

#include "BLI_ghash.h"

#ifdef USE_TGSET_SMALLHASH
#  include "BLI_smallhash.h"
#  define PTR_TO_IDX(ts) static_cast<SmallHash *>((ts)->ptr_to_idx)
#else
#  include "BLI_map.hh"
#  define PTR_TO_IDX(ts) static_cast<blender::Map<void *, int> *>((ts)->ptr_to_idx)
#endif

TableGSet *BLI_table_gset_new_ex(const char *info, int size)
{
  TableGSet *ts = MEM_new<TableGSet>(info);

#ifdef USE_TGSET_SMALLHASH
  ts->ptr_to_idx = static_cast<void *>(MEM_cnew<SmallHash>("table gset smallhash"));
  BLI_smallhash_init_ex(PTR_TO_IDX(->ptr_to_idx), size);
#else
  ts->ptr_to_idx = static_cast<void *>(MEM_new<blender::Map<void *, int>>("ts->ptr_to_idx"));
#endif

  if (size) {
    ts->elems = static_cast<void **>(MEM_callocN(sizeof(void *) * (uint)size, info));
    ts->size = size;
    ts->length = 0;
    ts->cur = 0;
  }

  return ts;
}

TableGSet *BLI_table_gset_new(const char *info)
{
  return BLI_table_gset_new_ex(info, 0);
}

void BLI_table_gset_free(TableGSet *ts, GHashKeyFreeFP freefp)
{
  MEM_SAFE_FREE(ts->elems);

#ifdef USE_TGSET_SMALLHASH
  BLI_smallhash_release(PTR_TO_IDX(ts->ptr_to_idx));
  MEM_freeN(ts->ptr_to_idx);
#else
  MEM_delete<blender::Map<void *, int>>(PTR_TO_IDX(ts));
#endif

  MEM_delete<TableGSet>(ts);
}

static void table_gset_resize(TableGSet *ts)
{
  if (ts->cur >= ts->size) {
    uint newsize = (uint)(ts->cur + 1);
    newsize = (newsize << 1U) - (newsize >> 1U);
    newsize = MAX2(newsize, 8U);

    if (!ts->elems) {
      ts->elems = static_cast<void **>(MEM_mallocN(sizeof(void *) * newsize, "ts->elems"));
    }
    else {
      ts->elems = static_cast<void **>(MEM_reallocN(ts->elems, newsize * sizeof(void *)));
    }

#ifdef USE_TGSET_SMALLHASH
    BLI_smallhash_clear(PTR_TO_IDX(ts));
#else
    PTR_TO_IDX(ts)->clear();
#endif

    /* Compact. */
    int compact_i = 0;
    for (int i = 0; i < ts->cur; i++) {
      void *elem2 = ts->elems[i];

      if (elem2) {
#ifdef USE_TGSET_SMALLHASH
        void **val;
        BLI_smallhash_ensure_p(PTR_TO_IDX(ts), (uintptr_t)elem2, &val);
        *val = POINTER_FROM_INT(compact_i);
#else
        PTR_TO_IDX(ts)->add_overwrite(elem2, compact_i);
#endif

        ts->elems[compact_i++] = elem2;
      }
    }

    ts->cur = compact_i;
    ts->size = (int)newsize;
  }
}

bool BLI_table_gset_add(TableGSet *ts, void *elem)
{
  table_gset_resize(ts);

#ifdef USE_TGSET_SMALLHASH
  void **val;

  bool ret = BLI_smallhash_ensure_p(PTR_TO_IDX(ts), (uintptr_t)elem, &val);

  if (!ret) {
    *val = POINTER_FROM_INT(ts->cur);

    ts->elems[ts->cur++] = elem;
    ts->length++;
  }

  return ret;
#else
  auto createfn = [&](int *value) {
    *value = ts->cur;
    ts->elems[ts->cur++] = elem;
    ts->length++;
    return true;
  };
  auto modifyfn = [&](int *value) { return false; };

  return PTR_TO_IDX(ts)->add_or_modify(elem, createfn, modifyfn);
#endif
}

void BLI_table_gset_insert(TableGSet *ts, void *elem)
{
  table_gset_resize(ts);

#ifdef USE_TGSET_SMALLHASH
  BLI_smallhash_insert(PTR_TO_IDX(ts), (uintptr_t)elem, (void *)ts->cur);
#else
  PTR_TO_IDX(ts)->add(elem, ts->cur);
#endif

  ts->elems[ts->cur++] = elem;
  ts->length++;
}

void BLI_table_gset_remove(TableGSet *ts, void *elem, GHashKeyFreeFP freefp)
{
  BLI_assert(ts && elem);

#ifdef USE_TGSET_SMALLHASH
  int *idx = (int *)BLI_smallhash_lookup_p(PTR_TO_IDX(ts), (uintptr_t)elem);
  if (!idx) {
    return;
  }

  BLI_smallhash_remove(PTR_TO_IDX(ts), (uintptr_t)elem);
#else
  int *idx = PTR_TO_IDX(ts)->lookup_ptr(elem);
  if (!idx) {
    return;
  }

  PTR_TO_IDX(ts)->remove(elem);
#endif

  int idx2 = *idx;

  if (!ts->elems || ts->elems[idx2] != elem) {
    return;
  }

  ts->length--;
  ts->elems[idx2] = nullptr;
}

bool BLI_table_gset_haskey(TableGSet *ts, void *elem)
{
#ifdef USE_TGSET_SMALLHASH
  return BLI_smallhash_haskey(PTR_TO_IDX(ts), (uintptr_t)elem);
#else
  return PTR_TO_IDX(ts)->contains(elem);
#endif
}

int BLI_table_gset_len(TableGSet *ts)
{
  return ts->length;
}
