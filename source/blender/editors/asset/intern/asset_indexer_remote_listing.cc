/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <fstream>
#include <string>

#include <fmt/format.h>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"
#include "BLI_set.hh"

#include "BKE_asset.hh"
#include "BKE_idtype.hh"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"

namespace blender::ed::asset::index {

using namespace blender::io::serialize;

/* -------------------------------------------------------------------- */
/** \name General functions for reading.
 * \{ */

static std::unique_ptr<Value> read_contents(StringRefNull filepath)
{
  JsonFormatter formatter;
  std::ifstream is;
  is.open(filepath.c_str());
  BLI_SCOPED_DEFER([&]() { is.close(); });

  return formatter.deserialize(is);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remote asset listing page
 *
 * The listing is split into multiple .json pages, the following is for reading them.
 * \{ */

struct AssetLibraryListingPageV1 {
  static bool read_into_entries_vec(const StringRefNull root_dirpath,
                                    const StringRefNull page_rel_path,
                                    Vector<RemoteListingAssetEntry> &io_entries);
};

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
    listing_entry.idcode = BKE_idtype_idcode_from_name_case_insensitive(idtype_name->c_str());
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
  const DictionaryValue *metadata_dict = dictionary.lookup_dict("meta");
  listing_entry.datablock_info.asset_data = metadata_dict ?
                                                asset_metadata_from_dictionary(*metadata_dict) :
                                                BKE_asset_metadata_create();
  listing_entry.datablock_info.free_asset_data = true;

  return listing_entry;
}

static bool listing_entries_from_root(const DictionaryValue &value,
                                      Vector<RemoteListingAssetEntry> &io_entries)
{
  const ArrayValue *entries = value.lookup_array("assets");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return false;
  }

  for (const std::shared_ptr<Value> &element : entries->elements()) {
    const char *failure_reason = "";
    std::optional<RemoteListingAssetEntry> entry = listing_entry_from_asset_dictionary(
        *element->as_dictionary_value(), &failure_reason);
    if (!entry) {
      /* Don't add this entry on failure to read it. */
      printf("Error reading asset listing entry, skipping. Reason: %s\n", failure_reason);
      continue;
    }

    io_entries.append(std::move(*entry));
  }

  return true;
}

bool AssetLibraryListingPageV1::read_into_entries_vec(const StringRefNull root_dirpath,
                                                      const StringRefNull page_rel_path,
                                                      Vector<RemoteListingAssetEntry> &io_entries)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), root_dirpath.c_str(), page_rel_path.c_str());

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

  listing_entries_from_root(*root, io_entries);
  // CLOG_INFO(&LOG, 1, "Read %d entries from remote asset listing for [%s].", r_entries.size(),
  // filepath);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Meta files
 * \{ */

struct AssetLibraryMeta {
  Set<int> api_versions;

  static std::optional<AssetLibraryMeta> read(StringRefNull root_dirpath);
};

/**
 * \return the supported API versions read from the `_asset-library-meta.json` file.
 */
std::optional<AssetLibraryMeta> AssetLibraryMeta::read(StringRefNull root_dirpath)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), root_dirpath.c_str(), "_asset-library-meta.json");

  if (!BLI_exists(filepath)) {
    /** TODO report error message? */
    return {};
  }

  std::unique_ptr<Value> contents = read_contents(filepath);
  if (!contents) {
    /** TODO report error message? */
    return {};
  }

  const DictionaryValue *root = contents->as_dictionary_value();
  if (!root) {
    /** TODO report error message? */
    return {};
  }

  const ArrayValue *entries = root->lookup_array("api_versions");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return {};
  }

  AssetLibraryMeta library_meta;

  int i = 0;
  for (const std::shared_ptr<Value> &element : entries->elements()) {
    const IntValue *version = element->as_int_value();
    if (!version) {
      printf(
          "Error reading asset listing API version at index %i in %s - ignoring\n", i, filepath);
      i++;
      continue;
    }
    library_meta.api_versions.add(std::move(version->value()));
    i++;
  }

  return library_meta;
}

struct AssetLibraryListingV1 {
  /** File paths to the individual asset listing files containing the assets, relative to \a
   * root_dirpath. */
  Vector<std::string> page_rel_paths;

  static std::optional<AssetLibraryListingV1> read(StringRefNull root_dirpath, int api_version);
};

std::optional<AssetLibraryListingV1> AssetLibraryListingV1::read(StringRefNull root_dirpath,
                                                                 const int api_version)
{
  const std::string api_version_str = fmt::format("_v{}", api_version);

  char filepath[FILE_MAX];
  BLI_path_join(filepath,
                sizeof(filepath),
                root_dirpath.c_str(),
                api_version_str.c_str(),
                /* TODO should be called `asset-listing.json`. */
                "asset-index.json");

  if (!BLI_exists(filepath)) {
    /** TODO report error message? */
    return {};
  }

  std::unique_ptr<Value> contents = read_contents(filepath);
  if (!contents) {
    /** TODO report error message? */
    return {};
  }

  const DictionaryValue *root = contents->as_dictionary_value();
  if (!root) {
    /** TODO report error message? */
    return {};
  }

  const ArrayValue *entries = root->lookup_array("page_urls");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return {};
  }

  AssetLibraryListingV1 listing;

  int i = 0;
  for (const std::shared_ptr<Value> &element : entries->elements()) {
    const StringValue *page_path = element->as_string_value();
    if (!page_path) {
      printf("Error reading asset listing page path at index %i in %s - ignoring\n", i, filepath);
      i++;
      continue;
    }
    listing.page_rel_paths.append(std::move(page_path->value()));
    i++;
  }

  return listing;
}

/** \} */

static std::optional<int> choose_api_version(const AssetLibraryMeta &library_meta)
{
  /* API versions this version of Blender can handle, in descending order (most preferred to least
   * preferred order). */
  const Vector readable_versions = {
      1,
  };

  for (int version : readable_versions) {
    if (library_meta.api_versions.contains(version)) {
      return version;
    }
  }

  return {};
}

bool read_remote_listing(StringRefNull root_dirpath, Vector<RemoteListingAssetEntry> *r_entries)
{
  /* TODO: Error reporting for all false return branches. */

  const std::optional<AssetLibraryMeta> meta = AssetLibraryMeta::read(root_dirpath);
  if (!meta) {
    return false;
  }

  const std::optional<int> api_version = choose_api_version(*meta);
  if (!api_version) {
    return false;
  }

  const std::optional<AssetLibraryListingV1> listing = AssetLibraryListingV1::read(root_dirpath,
                                                                                   *api_version);
  if (!listing) {
    return false;
  }

  Vector<RemoteListingAssetEntry> read_entries{};
  for (const std::string &page_path : listing->page_rel_paths) {
    AssetLibraryListingPageV1::read_into_entries_vec(root_dirpath, page_path, read_entries);
  }

  *r_entries = std::move(read_entries);

  return true;
}

}  // namespace blender::ed::asset::index
