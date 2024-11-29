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

#include "BKE_asset.hh"
#include "BKE_idtype.hh"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"

namespace blender::ed::asset::index {

using namespace blender::io::serialize;

RemoteIndexAssetEntry::RemoteIndexAssetEntry(RemoteIndexAssetEntry &&other)
{
  this->datablock_info = other.datablock_info;
  other.datablock_info = {};
  this->idcode = other.idcode;

  this->archive_url = std::move(other.archive_url);
  this->thumbnail_url = std::move(other.thumbnail_url);
}

RemoteIndexAssetEntry &RemoteIndexAssetEntry::operator=(RemoteIndexAssetEntry &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) RemoteIndexAssetEntry(std::move(other));
  return *this;
}

RemoteIndexAssetEntry::~RemoteIndexAssetEntry()
{
  BLO_datablock_info_free(&datablock_info);
}

static std::optional<RemoteIndexAssetEntry> indexer_entry_from_asset_dictionary(
    const DictionaryValue &dictionary, const char **r_failure_reason)
{
  RemoteIndexAssetEntry indexer_entry{};

  /* 'id': name of the asset. Required string. */
  if (const std::optional<StringRef> name = dictionary.lookup_str("name")) {
    name->copy(indexer_entry.datablock_info.name);
  }
  else {
    *r_failure_reason = "could not read asset name, 'name' field not set";
    return {};
  }

  /* 'type': data-block type, must match the #IDTypeInfo.name of the given type. required string.
   */
  if (const std::optional<StringRefNull> idtype_name = dictionary.lookup_str("id_type")) {
    indexer_entry.idcode = BKE_idtype_idcode_from_name(idtype_name->c_str());
    if (!BKE_idtype_idcode_is_valid(indexer_entry.idcode)) {
      *r_failure_reason = "could not read asset type, 'id_type' field is not a valid type";
      return {};
    }
  }
  else {
    *r_failure_reason = "could not read asset type, 'type' field not set";
    return {};
  }

  /* 'archive_url': required string. */
  if (const std::optional<StringRef> archive_url = dictionary.lookup_str("archive_url")) {
    indexer_entry.archive_url = *archive_url;
  }
  else {
    *r_failure_reason = "could not read asset location, 'archive_url' field not set";
    return {};
  }

  /* 'thumbnail': optional string. */
  indexer_entry.thumbnail_url = dictionary.lookup_str("thumbnail_url").value_or("");

  /* 'metadata': optional dictionary. If all the metadata fields are empty, this can be left out of
   * the index. Default metadata will then be allocated, with all fields empty/0. */
  const DictionaryValue *metadata_dict = dictionary.lookup_dict("metadata");
  indexer_entry.datablock_info.asset_data = metadata_dict ?
                                                asset_metadata_from_dictionary(*metadata_dict) :
                                                BKE_asset_metadata_create();
  indexer_entry.datablock_info.free_asset_data = true;

  return indexer_entry;
}

static Vector<RemoteIndexAssetEntry> indexer_entries_from_root(const DictionaryValue &value)
{
  const ArrayValue *entries = value.lookup_array("assets");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return {};
  }

  Vector<RemoteIndexAssetEntry> read_entries;

  for (const std::shared_ptr<Value> &element : entries->elements()) {
    const char *failure_reason = "";
    std::optional<RemoteIndexAssetEntry> entry = indexer_entry_from_asset_dictionary(
        *element->as_dictionary_value(), &failure_reason);
    if (!entry) {
      /* Don't add this entry on failure to read it. */
      printf("Error reading asset index entry, skipping. Reason: %s\n", failure_reason);
      continue;
    }

    read_entries.append(std::move(*entry));
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

bool read_remote_index(StringRefNull root_dirpath, Vector<RemoteIndexAssetEntry> *r_entries)
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
