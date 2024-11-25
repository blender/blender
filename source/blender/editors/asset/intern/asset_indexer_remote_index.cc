/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <fstream>
#include <string>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"
#include "BLI_string.h"

#include "BKE_idtype.hh"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"

namespace blender::ed::asset::index {

using namespace blender::io::serialize;

static std::optional<FileIndexerEntry> indexer_entry_from_asset_dictionary(
    const DictionaryValue &dictionary, const char **r_failure_reason)
{
  FileIndexerEntry temp_entry;

  if (const std::optional<StringRef> name = dictionary.lookup_str("id")) {
    name->copy(temp_entry.datablock_info.name);
  }
  else {
    *r_failure_reason = "could not read asset name, 'id' field not set";
    return {};
  }

  if (const std::optional<StringRefNull> idtype_name = dictionary.lookup_str("type")) {
    temp_entry.idcode = BKE_idtype_idcode_from_name(idtype_name->c_str());
    if (!BKE_idtype_idcode_is_valid(temp_entry.idcode)) {
      *r_failure_reason = "could not read asset type, 'type' field is not a valid type";
      return {};
    }
  }
  else {
    *r_failure_reason = "could not read asset type, 'type' field not set";
    return {};
  }

  if (const DictionaryValue *metadata = dictionary.lookup_dict("metadata")) {
    temp_entry.datablock_info.asset_data = asset_metadata_from_dictionary(*metadata);
    temp_entry.datablock_info.free_asset_data = true;
  }

  return temp_entry;
}

static Vector<FileIndexerEntry> indexer_entries_from_root(const DictionaryValue &value)
{
  const ArrayValue *entries = value.lookup_array("assets");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return {};
  }

  Vector<FileIndexerEntry> read_entries;

  for (const std::shared_ptr<Value> &element : entries->elements()) {
    const char *failure_reason = "";
    std::optional<FileIndexerEntry> entry = indexer_entry_from_asset_dictionary(
        *element->as_dictionary_value(), &failure_reason);
    if (!entry) {
      /* Don't add this entry on failure to read it. */
      printf("Error reading asset index entry, skipping. Reason: %s\n", failure_reason);
      continue;
    }

    read_entries.append(*entry);
  }

  return read_entries;
}

static std::unique_ptr<Value> read_contents(StringRefNull filepath)
{
  JsonFormatter formatter;
  std::ifstream is;
  is.open(filepath.c_str());
  BLI_SCOPED_DEFER([&]() { is.close(); });

  return formatter.deserialize(is);
}

bool read_remote_index(StringRefNull root_dirpath, Vector<FileIndexerEntry> *r_entries)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), root_dirpath.c_str(), "index.json");

  if (!BLI_exists(filepath)) {
    /** TODO report error message? */
    return false;
  }

  std::unique_ptr<Value> contents = read_contents(filepath);
  if (!contents) {
    /** TODO report error message? */
    return false;
  }

  const DictionaryValue *root = contents->as_dictionary_value();
  if (!root) {
    /** TODO report error message? */
    return false;
  }

  *r_entries = indexer_entries_from_root(*root);
  // CLOG_INFO(&LOG, 1, "Read %d entries from asset index for [%s].", r_entries.size(), filepath);
  return true;
}

}  // namespace blender::ed::asset::index
