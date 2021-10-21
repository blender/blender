#pragma once

#include "BLI_task.h"

typedef struct BLI_lfmempool {
  void *unused;
} BLI_lfmempool;

typedef struct BLI_lfmempool_iter {
  void *chunk;
  BLI_lfmempool *pool;
  int i;
  void **curchunk_threaded_shared;
} BLI_lfmempool_iter;

#ifdef __cplusplus
extern "C" {
#endif

BLI_lfmempool *BLI_lfmempool_create(int esize, int psize);
void BLI_lfmempool_destroy(BLI_lfmempool *pool);
void *BLI_lfmempool_alloc(BLI_lfmempool *pool);
void *BLI_lfmempool_calloc(BLI_lfmempool *pool);
void BLI_lfmempool_free(BLI_lfmempool *pool, void *mem);
void BLI_lfmempool_iternew(BLI_lfmempool *_pool, BLI_lfmempool_iter *iter);
void *BLI_lfmempool_iterstep(BLI_lfmempool_iter *iter);
// int BLI_lfmempool_len(BLI_lfmempool *pool);
void *BLI_lfmempool_iterstep_threaded(BLI_lfmempool *iter);
void *BLI_lfmempool_findelem(BLI_lfmempool *pool, int index);

typedef struct ParallelLFMempoolTaskData {
  BLI_lfmempool_iter ts_iter;
  TaskParallelTLS tls;
} ParallelLFMempoolTaskData;

ParallelLFMempoolTaskData *lfmempool_iter_threadsafe_create(BLI_lfmempool *pool,
                                                            const size_t num_iter);
void lfmempool_iter_threadsafe_destroy(ParallelLFMempoolTaskData *iter_arr);

#ifdef __cplusplus
}
#endif  //__cplusplus
