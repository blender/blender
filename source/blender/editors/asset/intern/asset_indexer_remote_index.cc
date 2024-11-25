/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <fstream>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"
#include "BLI_string.h"

#include "BKE_asset.hh"
#include "BKE_idtype.hh"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"

namespace blender::ed::asset::index {

using namespace blender::io::serialize;

struct RemoteAssetIndex;

static int init_indexer_entries_from_value(FileIndexerEntries &indexer_entries,
                                           const DictionaryValue &value);

/**
 * The index.json file containing metadata about the library, and the assets themself.
 */
class RemoteLibraryIndexFile {
 public:
  std::string root_dirpath;

  std::unique_ptr<RemoteAssetIndex> read_contents(StringRefNull filepath) const;
};

struct RemoteAssetIndex {
  /**
   * `io::serialize::Value` representing the contents of an index file.
   *
   * Value is used over #DictionaryValue as the contents of the index could be corrupted and
   * doesn't represent an object. In case corrupted files are detected the `get_version` would
   * return `UNKNOWN_VERSION`.
   */
  std::unique_ptr<Value> contents;

  RemoteAssetIndex(std::unique_ptr<Value> &value) : contents(std::move(value)) {}

  /**
   * Extract the contents of this index into the given \p indexer_entries.
   *
   * \return The number of entries read from the given entries.
   */
  int extract_into(FileIndexerEntries &indexer_entries) const
  {
    const DictionaryValue *root = this->contents->as_dictionary_value();
    const int num_entries_read = init_indexer_entries_from_value(indexer_entries, *root);
    return num_entries_read;
  }
};

std::unique_ptr<RemoteAssetIndex> RemoteLibraryIndexFile::read_contents(
    StringRefNull filepath) const
{
  JsonFormatter formatter;
  std::ifstream is;
  is.open(filepath.c_str());
  BLI_SCOPED_DEFER([&]() { is.close(); });

  std::unique_ptr<Value> read_data = formatter.deserialize(is);
  if (!read_data) {
    return nullptr;
  }

  return std::make_unique<RemoteAssetIndex>(read_data);
}

static bool init_indexer_entry_from_value(FileIndexerEntry &indexer_entry,
                                          const DictionaryValue &entry,
                                          const char **r_failure_reason)
{
  FileIndexerEntry temp_entry;

  if (const std::optional<StringRef> name = entry.lookup_str("id")) {
    name->copy(temp_entry.datablock_info.name);
  }
  else {
    *r_failure_reason = "could not read asset name, 'id' field not set";
    return false;
  }

  if (const std::optional<StringRefNull> idtype_name = entry.lookup_str("type")) {
    temp_entry.idcode = BKE_idtype_idcode_from_name(idtype_name->c_str());
    if (!BKE_idtype_idcode_is_valid(temp_entry.idcode)) {
      *r_failure_reason = "could not read asset type, 'type' field is not a valid type";
      return false;
    }
  }
  else {
    *r_failure_reason = "could not read asset type, 'type' field not set";
    return false;
  }

  if (const DictionaryValue *metadata = entry.lookup_dict("metadata")) {
    temp_entry.datablock_info.asset_data = asset_metadata_from_dictionary(*metadata);
    temp_entry.datablock_info.free_asset_data = true;
  }

  indexer_entry = temp_entry;
  return true;
}

static int init_indexer_entries_from_value(FileIndexerEntries &indexer_entries,
                                           const DictionaryValue &value)
{
  const ArrayValue *entries = value.lookup_array("assets");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return 0;
  }

  int num_entries_read = 0;
  for (const std::shared_ptr<Value> &element : entries->elements()) {
    FileIndexerEntry *entry = static_cast<FileIndexerEntry *>(
        MEM_callocN(sizeof(FileIndexerEntry), __func__));

    const char *failure_reason = "";
    if (!init_indexer_entry_from_value(*entry, *element->as_dictionary_value(), &failure_reason)) {
      /* Don't add this entry on failure to read it. */
      printf("Error reading asset index entry, skipping. Reason: %s\n", failure_reason);
      continue;
    }

    BLI_linklist_prepend(&indexer_entries.entries, entry);
    num_entries_read += 1;
  }

  return num_entries_read;
}

static eFileIndexerResult read_index(const char * /*filename*/,
                                     FileIndexerEntries *entries,
                                     int *r_read_entries_len,
                                     void *user_data)
{
  RemoteLibraryIndexFile *index = static_cast<RemoteLibraryIndexFile *>(user_data);

  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), index->root_dirpath.c_str(), "index.json");

  if (!BLI_exists(filepath)) {
    /** TODO handle failure? */
    BLI_assert_unreachable();
    return FILE_INDEXER_ENTRIES_LOADED;
  }

  std::unique_ptr<RemoteAssetIndex> contents = index->read_contents(filepath);
  if (!contents) {
    /** TODO handle failure? */
    BLI_assert_unreachable();
    return FILE_INDEXER_ENTRIES_LOADED;
  }

  const int read_entries_len = contents->extract_into(*entries);
  // CLOG_INFO(&LOG, 1, "Read %d entries from asset index for [%s].", read_entries_len, filename);
  *r_read_entries_len = read_entries_len;

  return FILE_INDEXER_ENTRIES_LOADED;
}

static void *init_user_data(const char *root_directory, size_t /*root_directory_maxncpy*/)
{
  RemoteLibraryIndexFile *index_file = MEM_new<RemoteLibraryIndexFile>(__func__);
  index_file->root_dirpath = root_directory;
  return index_file;
}

static void free_user_data(void *user_data)
{
  MEM_delete(static_cast<RemoteLibraryIndexFile *>(user_data));
}

static void update_index(const char *filename, FileIndexerEntries *entries, void *user_data) {}

constexpr FileIndexerType asset_indexer()
{
  FileIndexerType indexer = {nullptr};
  indexer.read_index = read_index;
  indexer.update_index = update_index;
  indexer.init_user_data = init_user_data;
  indexer.free_user_data = free_user_data;
  return indexer;
}

const FileIndexerType file_indexer_asset_remote_index = asset_indexer();

}  // namespace blender::ed::asset::index
