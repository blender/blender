/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <fmt/format.h>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_asset.hh"
#include "BKE_idtype.hh"

#include "BLT_translation.hh"

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
  static ReadingResult<> read_asset_entries(const StringRefNull filepath,
                                            RemoteListingEntryProcessFn process_fn);
};

static ReadingResult<RemoteListingAssetEntry> listing_entry_from_asset_dictionary(
    const DictionaryValue &dictionary,
    const Map<std::string, RemoteListingFileEntry> &file_path_to_entry_map)
{
  RemoteListingAssetEntry listing_entry{};

  /* 'id': name of the asset. Required string. */
  const std::optional<StringRef> asset_name_opt = dictionary.lookup_str("name");
  if (!asset_name_opt) {
    return ReadingResult<RemoteListingAssetEntry>::Failure(
        N_("could not read asset name, 'name' field not set"));
  }
  const StringRef asset_name = *asset_name_opt;
  asset_name.copy_utf8_truncated(listing_entry.datablock_info.name);

  /* 'type': data-block type, must match the #IDTypeInfo.name of the given type. required string.
   */
  if (const std::optional<StringRefNull> idtype_name = dictionary.lookup_str("id_type")) {
    listing_entry.idcode = BKE_idtype_idcode_from_name_case_insensitive(idtype_name->c_str());
    if (!BKE_idtype_idcode_is_valid(listing_entry.idcode)) {
      return ReadingResult<RemoteListingAssetEntry>::Failure(fmt::format(
          N_("could not read type of asset '{:s}': 'id_type' field is not a valid type"),
          asset_name));
    }
  }
  else {
    return ReadingResult<RemoteListingAssetEntry>::Failure(
        fmt::format(N_("could not read type of asset '{:s}', 'type' field not set"), asset_name));
  }

  /* 'files': required list of strings. */
  if (const ArrayValue *file_paths = dictionary.lookup_array("files")) {
    if (file_paths->elements().is_empty()) {
      return ReadingResult<RemoteListingAssetEntry>::Failure(
          fmt::format(N_("asset '{:s}' has no files"), asset_name));
    }
    for (const std::shared_ptr<Value> &file_path_element : file_paths->elements()) {
      asset_system::OnlineAssetFile file = {};

      const io::serialize::StringValue *file_path_string = file_path_element->as_string_value();
      if (!file_path_string) {
        return ReadingResult<RemoteListingAssetEntry>::Failure(fmt::format(
            N_("asset '{:s}' has a non-string entry in its 'files' list"), asset_name));
      }
      file.path = file_path_string->value();
      if (file.path.empty()) {
        /* TODO: use CLOG to have _some_ logging of this dubious empty file
         * entry. But keep going, maybe there's another, non-empty entry. */
        continue;
      }

      /* Look up the file URL and hash from the <files> section of the JSON. */
      if (const RemoteListingFileEntry *file_entry = file_path_to_entry_map.lookup_ptr(file.path))
      {
        file.url = file_entry->download_url;
      }
      else {
        return ReadingResult<RemoteListingAssetEntry>::Failure(
            fmt::format(N_("asset '{:s}' references unknown file '{:s}'"), asset_name, file.path));
      }

      listing_entry.online_info.files.append(file);
    }
  }
  else {
    return ReadingResult<RemoteListingAssetEntry>::Failure(
        fmt::format(N_("asset '{:s}' has no 'files' field"), asset_name));
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

  return ReadingResult<RemoteListingAssetEntry>::Success(std::move(listing_entry));
}

static ReadingResult<RemoteListingFileEntry> listing_file_from_asset_dictionary(
    const DictionaryValue &dictionary)
{
  RemoteListingFileEntry file_entry{};

  /* Path is mandatory. */
  if (const std::optional<StringRefNull> path = dictionary.lookup_str("path")) {
    file_entry.local_path = *path;
  }
  else {
    return ReadingResult<RemoteListingFileEntry>::Failure(
        N_("Error reading asset listing file entry, skipping. Reason: found a file without 'path' "
           "field"));
  }

  /* Hash is mandatory. */
  if (const std::optional<StringRefNull> hash = dictionary.lookup_str("hash")) {
    file_entry.download_url.hash = *hash;
  }
  else {
    return ReadingResult<RemoteListingFileEntry>::Failure(fmt::format(
        N_("Error reading asset listing file entry, skipping. Reason: found a file ({:s}) without "
           "'hash' field"),
        file_entry.local_path.c_str()));
  }

  /* URL is optional, and defaults to the local path. That's handled in Python
   * (see `download_asset()` in `asset_downloader.py`) so here we can just use
   * an empty string to indicate "no URL". */
  file_entry.download_url.url = dictionary.lookup_str("url").value_or("");

  return ReadingResult<RemoteListingFileEntry>::Success(std::move(file_entry));
}

static ReadingResult<> listing_entries_from_root(const DictionaryValue &value,
                                                 const RemoteListingEntryProcessFn process_fn)
{
  const ArrayValue *assets = value.lookup_array("assets");
  BLI_assert(assets != nullptr);
  if (assets == nullptr) {
    return ReadingResult<>::Failure(N_("no assets listed"));
  }

  /* Build a mapping from local file path to its file info. */
  const ArrayValue *files = value.lookup_array("files");
  BLI_assert(files != nullptr);
  if (assets == nullptr) {
    /* The 'files' section is mandatory in the OpenAPI schema. */
    return ReadingResult<>::Failure(
        N_("error reading asset listing, page file has no files section"));
  }

  Vector<std::string> warnings;
  Map<std::string, RemoteListingFileEntry> path_to_file_info;
  for (const std::shared_ptr<Value> &file_element : files->elements()) {
    ReadingResult<RemoteListingFileEntry> file_result = listing_file_from_asset_dictionary(
        *file_element->as_dictionary_value());
    if (file_result.is_failure()) {
      warnings.append(std::move(file_result.failure_reason));
      continue;
    }
    if (file_result.is_cancelled()) {
      return ReadingResult<>::Cancelled();
    }
    BLI_assert(file_result.is_success());
    RemoteListingFileEntry &file_entry = *file_result;
    if (file_entry.local_path.empty()) {
      continue;
    }
    std::string local_path = file_entry.local_path; /* Make a copy before std::moving. */
    path_to_file_info.add_overwrite(local_path, std::move(file_entry));
  }

  /* Convert the assets into RemoteListingAssetEntry objects. */
  for (const std::shared_ptr<Value> &asset_element : assets->elements()) {
    ReadingResult<RemoteListingAssetEntry> result = listing_entry_from_asset_dictionary(
        *asset_element->as_dictionary_value(), path_to_file_info);
    if (result.is_failure()) {
      if (!result.failure_reason.empty()) {
        warnings.append(std::move(result.failure_reason));
      }
      continue;
    }

    RemoteListingAssetEntry &entry = *result.success_value;
    if (!process_fn(entry)) {
      return ReadingResult<>::Cancelled();
    }
  }

  ReadingResult<> overall_result = ReadingResult<>::Success();
  overall_result.warnings.extend(std::move(warnings));
  return overall_result;
}

ReadingResult<> AssetLibraryListingPageV1::read_asset_entries(
    const StringRefNull filepath, const RemoteListingEntryProcessFn process_fn)
{
  if (!BLI_exists(filepath.c_str())) {
    return ReadingResult<>::Failure(fmt::format(N_("file does not exist: {:s}"), filepath));
  }

  const std::unique_ptr<Value> contents = read_contents(filepath);
  if (!contents) {
    return ReadingResult<>::Failure(fmt::format(N_("file is empty: {:s}"), filepath));
  }

  const DictionaryValue *root = contents->as_dictionary_value();
  if (!root) {
    return ReadingResult<>::Failure(
        fmt::format(N_("file is not a JSON dictionary: {:s}"), filepath));
  }

  return listing_entries_from_root(*root, process_fn);
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

ReadingResult<> read_remote_listing_v1(const StringRefNull listing_root_dirpath,
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
      return ReadingResult<>::Failure(
          fmt::format(N_("Couldn't find index file {:s}"), asset_index_abspath));
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
    return ReadingResult<>::Failure(
        fmt::format(N_("Couldn't read V1 listing from {:s}"), asset_index_abspath));
  }

  Set<StringRef> done_pages;
  char filepath[FILE_MAX];

  // TODO should we have some timeout here too? Like timeout after 30 seconds without a new page?

  Vector<std::string> warnings;
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

      ReadingResult page_result = AssetLibraryListingPageV1::read_asset_entries(filepath,
                                                                                process_fn);
      done_pages.add(page_path);
      if (page_result.is_cancelled()) {
        return page_result;
      }
      if (page_result.is_failure()) {
        printf("Couldn't read V1 listing from %s%c%s: %s\n",
               listing_root_dirpath.c_str(),
               SEP,
               page_path.c_str(),
               page_result.failure_reason.c_str());
        return page_result;
      }
      BLI_assert(page_result.is_success());

      /* Gather per-page warnings into the overall result. */
      if (page_result.has_warnings()) {
        warnings.extend(std::move(page_result.warnings));
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
      return ReadingResult<>::Cancelled();
    }
  }

  /* Return a success, with all the warnings. */
  ReadingResult<> result = ReadingResult<>::Success();
  result.warnings.extend(std::move(warnings));
  return result;
}

}  // namespace blender::ed::asset::index
