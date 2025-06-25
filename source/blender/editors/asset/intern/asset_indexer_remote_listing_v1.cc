/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "BLI_fileops.h"
#include "BLI_function_ref.hh"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_asset.hh"
#include "BKE_idtype.hh"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"

namespace blender::ed::asset::index {

using namespace blender::io::serialize;

/* -------------------------------------------------------------------- */
/** \name Remote asset listing page
 * \{ */

struct AssetLibraryListingPageV1 {
  static ReadingResult read_asset_entries(const StringRefNull filepath,
                                          RemoteListingEntryProcessFn process_fn);
};

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

static ReadingResult listing_entries_from_root(const DictionaryValue &value,
                                               const RemoteListingEntryProcessFn process_fn)
{
  const ArrayValue *entries = value.lookup_array("assets");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return ReadingResult::Failure;
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

    if (!process_fn(*entry)) {
      return ReadingResult::Cancelled;
    }
  }

  return ReadingResult::Success;
}

ReadingResult AssetLibraryListingPageV1::read_asset_entries(
    const StringRefNull filepath, const RemoteListingEntryProcessFn process_fn)
{
  if (!BLI_exists(filepath.c_str())) {
    /** TODO report error message? */
    return ReadingResult::Failure;
  }

  const std::unique_ptr<Value> contents = read_contents(filepath);
  if (!contents) {
    /** TODO report error message? */
    return ReadingResult::Failure;
  }

  const DictionaryValue *root = contents->as_dictionary_value();
  if (!root) {
    /** TODO report error message? */
    return ReadingResult::Failure;
  }

  const ReadingResult result = listing_entries_from_root(*root, process_fn);
  if (result != ReadingResult::Success) {
    return result;
  }
  // CLOG_INFO(&LOG, 1, "Read %d entries from remote asset listing for [%s].", r_entries.size(),
  // filepath);
  return ReadingResult::Success;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remote asset listing
 *
 * Sort of an index file listing the individual page files and meta information about the asset
 * listing (such as the count of assets).
 *
 * \{ */

struct AssetLibraryListingV1 {
  /** File paths to the individual asset listing files containing the assets, relative to \a
   * root_dirpath. */
  Vector<std::string> page_rel_paths;

  static std::optional<AssetLibraryListingV1> read(const StringRefNull listing_filepath);
};

std::optional<AssetLibraryListingV1> AssetLibraryListingV1::read(
    const StringRefNull listing_filepath)
{
  if (!BLI_exists(listing_filepath.c_str())) {
    /** TODO report error message? */
    return {};
  }

  const std::unique_ptr<Value> contents = read_contents(listing_filepath);
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
      printf("Error reading asset listing page path at index %i in %s - ignoring\n",
             i,
             listing_filepath.c_str());
      i++;
      continue;
    }
    listing.page_rel_paths.append(std::move(page_path->value()));
    i++;
  }

  return listing;
}

/** \} */

ReadingResult read_remote_listing_v1(const StringRefNull listing_root_dirpath,
                                     const RemoteListingEntryProcessFn process_fn,
                                     const RemoteListingWaitForPagesFn wait_fn)
{
  /* Version 1 asset indices are always stored in this path by RemoteAssetListingDownloader. */
  constexpr const char *asset_index_relpath = "_v1/asset-index.processed.json";

  char asset_index_abspath[FILE_MAX];
  BLI_path_join(asset_index_abspath,
                sizeof(asset_index_abspath),
                listing_root_dirpath.c_str(),
                asset_index_relpath);

  const std::optional<AssetLibraryListingV1> listing = AssetLibraryListingV1::read(
      asset_index_abspath);
  if (!listing) {
    printf("Couldn't read V1 listing from %s\n", asset_index_abspath);
    return ReadingResult::Failure;
  }

  Set<StringRef> done_pages;
  char filepath[FILE_MAX];

  // TODO should we have some timeout here too? Like timeout after 30 seconds without a new page?

  while (true) {
    for (const std::string &page_path : listing->page_rel_paths) {
      if (done_pages.contains(page_path)) {
        continue;
      }

      BLI_path_join(filepath, sizeof(filepath), listing_root_dirpath.c_str(), page_path.c_str());
      if (wait_fn && !BLI_exists(filepath)) {
        continue;
      }

      const ReadingResult result = AssetLibraryListingPageV1::read_asset_entries(filepath,
                                                                                 process_fn);
      done_pages.add(page_path);

      if (result != ReadingResult::Success) {
        printf("Couldn't read V1 listing from %s%c%s\n",
               listing_root_dirpath.c_str(),
               SEP,
               page_path.c_str());
        return result;
      }
    }

    BLI_assert(done_pages.size() <= listing->page_rel_paths.size());
    if (done_pages.size() >= listing->page_rel_paths.size()) {
      break;
    }
    if (!wait_fn) {
      break;
    }
    if (!wait_fn()) {
      return ReadingResult::Cancelled;
    }
  }

  return ReadingResult::Success;
}

}  // namespace blender::ed::asset::index
