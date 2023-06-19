/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edfile
 *
 * This file implements the default file browser indexer and has some helper function to work with
 * `FileIndexerEntries`.
 */
#include "file_indexer.h"

#include "MEM_guardedalloc.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

namespace blender::ed::file::indexer {

static eFileIndexerResult read_index(const char * /*file_name*/,
                                     FileIndexerEntries * /*entries*/,
                                     int * /*r_read_entries_len*/,
                                     void * /*user_data*/)
{
  return FILE_INDEXER_NEEDS_UPDATE;
}

static void update_index(const char * /*file_name*/,
                         FileIndexerEntries * /*entries*/,
                         void * /*user_data*/)
{
}

constexpr FileIndexerType default_indexer()
{
  FileIndexerType indexer = {nullptr};
  indexer.read_index = read_index;
  indexer.update_index = update_index;
  return indexer;
}

static FileIndexerEntry *file_indexer_entry_create_from_datablock_info(
    BLODataBlockInfo *datablock_info, const int idcode)
{
  FileIndexerEntry *entry = static_cast<FileIndexerEntry *>(
      MEM_mallocN(sizeof(FileIndexerEntry), __func__));
  entry->idcode = idcode;
  /* Shallow copy data-block info and mark original as having its asset data ownership stolen. */
  entry->datablock_info = *datablock_info;
  datablock_info->free_asset_data = false;
  return entry;
}

}  // namespace blender::ed::file::indexer

extern "C" {

void ED_file_indexer_entries_extend_from_datablock_infos(
    FileIndexerEntries *indexer_entries,
    LinkNode * /*BLODataBlockInfo*/ datablock_infos,
    const int idcode)
{
  for (LinkNode *ln = datablock_infos; ln; ln = ln->next) {
    BLODataBlockInfo *datablock_info = static_cast<BLODataBlockInfo *>(ln->link);
    FileIndexerEntry *file_indexer_entry =
        blender::ed::file::indexer::file_indexer_entry_create_from_datablock_info(datablock_info,
                                                                                  idcode);
    BLI_linklist_prepend(&indexer_entries->entries, file_indexer_entry);
  }
}

static void ED_file_indexer_entry_free(void *indexer_entry_ptr)
{
  FileIndexerEntry *indexer_entry = static_cast<FileIndexerEntry *>(indexer_entry_ptr);
  BLO_datablock_info_free(&indexer_entry->datablock_info);
  MEM_freeN(indexer_entry);
}

void ED_file_indexer_entries_clear(FileIndexerEntries *indexer_entries)
{
  BLI_linklist_free(indexer_entries->entries, ED_file_indexer_entry_free);
  indexer_entries->entries = nullptr;
}

const FileIndexerType file_indexer_noop = blender::ed::file::indexer::default_indexer();
}
