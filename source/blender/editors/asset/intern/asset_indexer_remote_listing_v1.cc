/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_asset.hh"
#include "BKE_idtype.hh"

#include "CLG_log.h"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"
#include "asset_indexer_remote_listing.hh"

static CLG_LogRef LOG = {"asset.remote_listing"};

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
    const DictionaryValue &dictionary,
    const char **r_failure_reason,
    const Map<std::string, RemoteListingFileEntry> &file_path_to_entry_map)
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

  /* 'file': required string. */
  if (const std::optional<StringRef> file_path = dictionary.lookup_str("file")) {
    listing_entry.online_info.download_dst_filepath = *file_path;
  }
  else {
    *r_failure_reason = "could not read asset location, 'file' field not set";
    return {};
  }

  /* Look up the file URL and hash from the <files> section of the JSON. */
  if (const RemoteListingFileEntry *file_entry = file_path_to_entry_map.lookup_ptr(
          listing_entry.online_info.download_dst_filepath))
  {
    listing_entry.online_info.asset_url = file_entry->download_url;
  }
  else {
    /* TODO: include the path that's not found. */
    *r_failure_reason = "asset references unknown file";
    return {};
  }

  /* 'thumbnail': URL and hash of the preview image. */
  listing_entry.online_info.preview_url = ed::asset::index::parse_url_with_hash_dict(
      dictionary.lookup_dict("thumbnail"));

  /* 'metadata': optional dictionary. If all the metadata fields are empty, this can be left out of
   * the listing. Default metadata will then be allocated, with all fields empty/0. */
  const DictionaryValue *metadata_dict = dictionary.lookup_dict("meta");
  listing_entry.datablock_info.asset_data = metadata_dict ?
                                                asset_metadata_from_dictionary(*metadata_dict) :
                                                BKE_asset_metadata_create();
  listing_entry.datablock_info.free_asset_data = true;

  return listing_entry;
}

static std::optional<RemoteListingFileEntry> listing_file_from_asset_dictionary(
    const DictionaryValue &dictionary)
{
  RemoteListingFileEntry file_entry{};

  /* Path is mandatory. */
  if (const std::optional<StringRefNull> path = dictionary.lookup_str("path")) {
    file_entry.local_path = *path;
  }
  else {
    printf(
        "Error reading asset listing file entry, skipping. Reason: found a file without 'path' "
        "field\n");
    return {};
  }

  /* Hash is mandatory. */
  if (const std::optional<StringRefNull> hash = dictionary.lookup_str("hash")) {
    file_entry.download_url.hash = *hash;
  }
  else {
    printf(
        "Error reading asset listing file entry, skipping. Reason: found a file (%s) without "
        "'hash' field\n",
        file_entry.local_path.c_str());
    return {};
  }

  /* URL is optional, and defaults to the local path. That's handled in Python
   * (see `download_asset()` in `asset_downloader.py`) so here we can just use
   * an empty string to indicate "no URL". */
  file_entry.download_url.url = dictionary.lookup_str("url").value_or("");

  return file_entry;
}

static ReadingResult listing_entries_from_root(const DictionaryValue &value,
                                               const RemoteListingEntryProcessFn process_fn)
{
  const ArrayValue *assets = value.lookup_array("assets");
  BLI_assert(assets != nullptr);
  if (assets == nullptr) {
    return ReadingResult::Failure;
  }

  /* Build a mapping from local file path to its file info. */
  const ArrayValue *files = value.lookup_array("files");
  BLI_assert(files != nullptr);
  if (assets == nullptr) {
    /* The 'files' section is mandatory in the OpenAPI schema. */
    printf("Error reading asset listing, page file has no files section.\n");
    return ReadingResult::Failure;
  }
  Map<std::string, RemoteListingFileEntry> path_to_file_info;
  for (const std::shared_ptr<Value> &file_element : files->elements()) {
    std::optional<RemoteListingFileEntry> file_entry = listing_file_from_asset_dictionary(
        *file_element->as_dictionary_value());
    if (!file_entry) {
      continue;
    }
    if (file_entry->local_path.empty()) {
      continue;
    }
    std::string local_path = file_entry->local_path; /* Make a copy before std::moving. */
    path_to_file_info.add_overwrite(local_path, std::move(*file_entry));
  }

  /* Convert the assets into RemoteListingAssetEntry objects. */
  for (const std::shared_ptr<Value> &asset_element : assets->elements()) {
    const char *failure_reason = "";
    std::optional<RemoteListingAssetEntry> entry = listing_entry_from_asset_dictionary(
        *asset_element->as_dictionary_value(), &failure_reason, path_to_file_info);
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

  const ArrayValue *entries = root->lookup_array("pages");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return {};
  }

  AssetLibraryListingV1 listing;

  int i = 0;
  for (const std::shared_ptr<Value> &element : entries->elements()) {
    const std::optional<asset_system::URLWithHash> page_info = parse_url_with_hash_dict(
        element->as_dictionary_value());
    if (!page_info) {
      printf("Error reading asset listing page path at index %i in %s - ignoring\n",
             i,
             listing_filepath.c_str());
      i++;
      continue;
    }
    listing.page_rel_paths.append(std::move(page_info->url));
    i++;
  }

  return listing;
}

/** \} */

ReadingResult read_remote_listing_v1(const StringRefNull listing_root_dirpath,
                                     const RemoteListingEntryProcessFn process_fn,
                                     const RemoteListingWaitForPagesFn wait_fn,
                                     const std::optional<Timestamp> ignore_before_timestamp)
{
  /* Version 1 asset indices are always stored in this path by RemoteAssetListingDownloader. */
  constexpr const char *asset_index_relpath = "_v1/asset-index.processed.json";

  char asset_index_abspath[FILE_MAX];
  BLI_path_join(asset_index_abspath,
                sizeof(asset_index_abspath),
                listing_root_dirpath.c_str(),
                asset_index_relpath);

  if (ignore_before_timestamp) {
    std::optional<bool> is_older = file_older_than_timestamp(asset_index_abspath,
                                                             *ignore_before_timestamp);
    if (!is_older) {
      CLOG_ERROR(&LOG, "Couldn't find index file %s\n", asset_index_abspath);
      return {};
    }
    /* TODO the .processed.json file doesn't get touched by the downloader to indicate it's up to
     * date. Should this be done, or should we just note compare the timestamps for meta-files? The
     * downloader notifies about them being in place already anyway. */
    // if (*is_older) {
    //   CLOG_ERROR(&LOG, "Index file too old %s\n", asset_index_abspath);
    //   return {};
    // }
  }

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
      if (wait_fn) {
        if (!BLI_exists(filepath)) {
          continue;
        }
        if (ignore_before_timestamp &&
            file_older_than_timestamp(filepath, *ignore_before_timestamp).value_or(true))
        {
          CLOG_DEBUG(&LOG, "Ignoring old listing file %s - waiting for a new version\n", filepath);
          continue;
        }
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
