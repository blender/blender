/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edfile
 */

#pragma once

#include "BLO_readfile.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * File indexing for the file/asset browser.
 *
 * This file contains an API to create indexing functionality when listing blend files in
 * the file browser.
 *
 * To implement a custom indexer a `FileIndexerType` struct should be made and passed to the
 * `filelist_setindexer` function.
 */

struct LinkNode;

/**
 * Result code of the `read_index` callback.
 */
typedef enum eFileIndexerResult {
  /**
   * File listing entries are loaded from the index. Reading entries from the blend file itself
   * should be skipped.
   */
  FILE_INDEXER_ENTRIES_LOADED,

  /**
   * Index isn't available or not up to date. Entries should be read from the blend file and
   * `update_index` must be called to update the index.
   */
  FILE_INDEXER_NEEDS_UPDATE,
} eFileIndexerResult;

/**
 * FileIndexerEntry contains all data that is required to create a file listing entry.
 */
typedef struct FileIndexerEntry {
  struct BLODataBlockInfo datablock_info;
  short idcode;
} FileIndexerEntry;

/**
 * Contains all entries of a blend file.
 */
typedef struct FileIndexerEntries {
  struct LinkNode /* FileIndexerEntry */ *entries;
} FileIndexerEntries;

typedef void *(*FileIndexerInitUserDataFunc)(const char *root_directory,
                                             size_t root_directory_maxlen);
typedef void (*FileIndexerFreeUserDataFunc)(void *);
typedef void (*FileIndexerFinishedFunc)(void *);
typedef eFileIndexerResult (*FileIndexerReadIndexFunc)(const char *file_name,
                                                       FileIndexerEntries *entries,
                                                       int *r_read_entries_len,
                                                       void *user_data);
typedef void (*FileIndexerUpdateIndexFunc)(const char *file_name,
                                           FileIndexerEntries *entries,
                                           void *user_data);

typedef struct FileIndexerType {
  /**
   * Is called at the beginning of the file listing process. An indexer can
   * setup needed data. The result of this function will be passed around as `user_data` parameter.
   *
   * This is an optional callback.
   */
  FileIndexerInitUserDataFunc init_user_data;

  /**
   * Is called at the end of the file listing process. An indexer can free the data that it created
   * during the file listing process.
   *
   * This is an optional callback */
  FileIndexerFreeUserDataFunc free_user_data;

  /**
   * Is called at the end of the file listing process (before the `free_user_data`) where indexes
   * can perform clean-ups.
   *
   * This is an optional callback. Called when listing files completed.
   */
  FileIndexerFinishedFunc filelist_finished;

  /**
   * Is called for each blend file being listed to read data from the index.
   *
   * Read entries should be added to given `entries` parameter (type: `FileIndexerEntries`).
   * `*r_read_entries_len` must be set to the number of read entries.
   * and the function must return `eFileIndexerResult::FILE_INDEXER_ENTRIES_LOADED`.
   * In this case the blend file will not be opened and the FileIndexerEntry added to `entries`
   * will be used as the content of the file.
   *
   * When the index isn't available or could not be used no entries must be added to the
   * entries field, `r_read_entries_len` must be set to `0` and the function must return
   * `eFileIndexerResult::FILE_INDEXER_NEEDS_UPDATE`. In this case the blend file will read from
   * the blend file and the `update_index` function will be called.
   */
  FileIndexerReadIndexFunc read_index;

  /**
   * Update an index of a blend file.
   *
   * Is called after reading entries from the file when the result of `read_index` was
   * `eFileIndexerResult::FILE_INDEXER_NEED_UPDATE`. The callback should update the index so the
   * next time that read_index is called it will read the entries from the index.
   */
  FileIndexerUpdateIndexFunc update_index;
} FileIndexerType;

/* file_indexer.cc */

/** Removes all entries inside the given `indexer_entries`. */
void ED_file_indexer_entries_clear(FileIndexerEntries *indexer_entries);

/**
 * Adds all entries from the given `datablock_infos` to the `indexer_entries`.
 * The datablock_infos must only contain data for a single IDType. The specific IDType must be
 * passed in the `idcode` parameter.
 *
 * \note This can "steal" data contained in \a datablock_infos, to avoid expensive copies, which is
 *       supported by the #BLODataBlockInfo type.
 */
void ED_file_indexer_entries_extend_from_datablock_infos(
    FileIndexerEntries *indexer_entries,
    struct LinkNode * /*BLODataBlockInfo*/ datablock_infos,
    int idcode);

#ifdef __cplusplus
}
#endif
