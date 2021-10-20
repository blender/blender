#pragma once

typedef struct BLI_lfmempool_iter {
  void *chunk;
  BLI_lfmempool *pool;
  int i;
} BLI_lfmempool_iter;

void BLI_lfmempool_destroy(BLI_lfmempool *pool);
void *BLI_lfmempool_alloc(BLI_lfmempool *pool);
void BLI_lfmempool_free(BLI_lfmempool *pool, void *mem);
void BLI_lfmempool_iternew(BLI_lfmempool *_pool, BLI_lfmempool_iter *iter);
void *BLI_lfmempool_iterstep(BLI_lfmempool_iter *iter);
