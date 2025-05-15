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

#include "BKE_asset.hh"
#include "BKE_idtype.hh"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"

namespace blender::ed::asset::index {

using namespace blender::io::serialize;

/* -------------------------------------------------------------------- */
/** \name Remote asset listing
 * \{ */

RemoteListingAssetEntry::RemoteListingAssetEntry(RemoteListingAssetEntry &&other)
{
  this->datablock_info = other.datablock_info;
  other.datablock_info = {};
  this->idcode = other.idcode;

  this->archive_url = std::move(other.archive_url);
  this->thumbnail_url = std::move(other.thumbnail_url);
}

RemoteListingAssetEntry &RemoteListingAssetEntry::operator=(RemoteListingAssetEntry &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) RemoteListingAssetEntry(std::move(other));
  return *this;
}

RemoteListingAssetEntry::~RemoteListingAssetEntry()
{
  BLO_datablock_info_free(&datablock_info);
}

static std::optional<RemoteListingAssetEntry> listing_entry_from_asset_dictionary(
    const DictionaryValue &dictionary, const char **r_failure_reason)
{
  RemoteListingAssetEntry listing_entry{};

  /* 'id': name of the asset. Required string. */
  if (const std::optional<StringRef> name = dictionary.lookup_str("name")) {
    name->copy_utf8_truncated(listing_entry.datablock_info.name);
  }
  else {
    *r_failure_reason = "could not read asset name, 'name' field not set";
    return {};
  }

  /* 'type': data-block type, must match the #IDTypeInfo.name of the given type. required string.
   */
  if (const std::optional<StringRefNull> idtype_name = dictionary.lookup_str("id_type")) {
    listing_entry.idcode = BKE_idtype_idcode_from_name(idtype_name->c_str());
    if (!BKE_idtype_idcode_is_valid(listing_entry.idcode)) {
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
    listing_entry.archive_url = *archive_url;
  }
  else {
    *r_failure_reason = "could not read asset location, 'archive_url' field not set";
    return {};
  }

  /* 'thumbnail': optional string. */
  listing_entry.thumbnail_url = dictionary.lookup_str("thumbnail_url").value_or("");

  /* 'metadata': optional dictionary. If all the metadata fields are empty, this can be left out of
   * the listing. Default metadata will then be allocated, with all fields empty/0. */
  const DictionaryValue *metadata_dict = dictionary.lookup_dict("metadata");
  listing_entry.datablock_info.asset_data = metadata_dict ?
                                                asset_metadata_from_dictionary(*metadata_dict) :
                                                BKE_asset_metadata_create();
  listing_entry.datablock_info.free_asset_data = true;

  return listing_entry;
}

static Vector<RemoteListingAssetEntry> listing_entries_from_root(const DictionaryValue &value)
{
  const ArrayValue *entries = value.lookup_array("assets");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return {};
  }

  Vector<RemoteListingAssetEntry> read_entries;

  for (const std::shared_ptr<Value> &element : entries->elements()) {
    const char *failure_reason = "";
    std::optional<RemoteListingAssetEntry> entry = listing_entry_from_asset_dictionary(
        *element->as_dictionary_value(), &failure_reason);
    if (!entry) {
      /* Don't add this entry on failure to read it. */
      printf("Error reading asset listing entry, skipping. Reason: %s\n", failure_reason);
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

bool read_remote_listing(StringRefNull root_dirpath, Vector<RemoteListingAssetEntry> *r_entries)
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

  *r_entries = listing_entries_from_root(*root);
  // CLOG_INFO(&LOG, 1, "Read %d entries from remote asset listing for [%s].", r_entries.size(),
  // filepath);
  return true;
}

/** \} */

}  // namespace blender::ed::asset::index
