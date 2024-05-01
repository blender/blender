/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <memory>
#include <utility>

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_space_types.h"

#include "AS_asset_identifier.hh"
#include "AS_asset_library.hh"

#include "BKE_asset.hh"
#include "BKE_asset_edit.hh"
#include "BKE_blendfile.hh"
#include "BKE_blendfile_link_append.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"

#include "BLO_read_write.hh"
#include "BLO_readfile.hh"
#include "BLO_writefile.hh"

#include "DNA_asset_types.h"

#include "MEM_guardedalloc.h"

namespace blender::bke {

/**
 * Asset library blend file, with editable contents.
 */
struct AssetEditBlend {
  std::string filepath;
  Main *main;
  bool is_editable = false;

  AssetEditBlend(const std::string &filepath);
  ~AssetEditBlend();

  AssetEditBlend(const AssetEditBlend &) = delete;
  AssetEditBlend(AssetEditBlend &&other);
  AssetEditBlend &operator=(AssetEditBlend &&other);

  ID *ensure_id(ID_Type id_type, const char *asset_name);
  void reload(Main &global_main);
  void clear_users(Main &global_main);
};

AssetEditBlend::AssetEditBlend(const std::string &filepath)
    : filepath(std::move(filepath)), main(BKE_main_new())
{
  this->main->is_asset_weak_reference_main = true;
  BLI_assert(!BLI_path_is_rel(filepath.c_str()));

  /* Fairly simple check based on filepath only.
   * - Ends with `.asset.bend` extensions.
   * - Is located in user asset library.
   *
   * TODO?
   * - Check file contents.
   * - Check file is writable.
   */
  this->is_editable = StringRef(filepath).endswith(BLENDER_ASSET_FILE_SUFFIX) &&
                      BKE_preferences_asset_library_containing_path(&U, filepath.c_str());
}

AssetEditBlend::~AssetEditBlend()
{
  if (main) {
    BKE_main_free(main);
  }
}

AssetEditBlend::AssetEditBlend(AssetEditBlend &&other)
    : filepath(std::exchange(other.filepath, "")), main(std::exchange(other.main, nullptr))
{
}
AssetEditBlend &AssetEditBlend::operator=(AssetEditBlend &&other)
{
  if (this == &other) {
    return *this;
  }
  this->filepath = std::exchange(other.filepath, "");
  this->main = std::exchange(other.main, nullptr);
  return *this;
}

ID *AssetEditBlend::ensure_id(const ID_Type id_type, const char *asset_name)
{
  /* Check if we have the asset already. */
  ID *local_asset = BKE_libblock_find_name(this->main, id_type, asset_name);
  if (local_asset) {
    BLI_assert(ID_IS_ASSET(local_asset));
    return local_asset;
  }

  /* Load asset from asset library. */
  LibraryLink_Params lapp_params{};
  lapp_params.bmain = this->main;
  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
  BKE_blendfile_link_append_context_flag_set(lapp_context, BLO_LIBLINK_FORCE_INDIRECT, true);
  BKE_blendfile_link_append_context_flag_set(lapp_context, 0, true);

  BKE_blendfile_link_append_context_library_add(lapp_context, filepath.c_str(), nullptr);

  BlendfileLinkAppendContextItem *lapp_item = BKE_blendfile_link_append_context_item_add(
      lapp_context, asset_name, id_type, nullptr);
  BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, lapp_item, 0);

  BKE_blendfile_link(lapp_context, nullptr);
  BKE_blendfile_append(lapp_context, nullptr);

  local_asset = BKE_blendfile_link_append_context_item_newid_get(lapp_context, lapp_item);

  BKE_blendfile_link_append_context_free(lapp_context);

  BKE_main_id_tag_all(this->main, LIB_TAG_ASSET_MAIN, true);

  /* Verify that the name matches. It must for referencing the same asset again to work.  */
  BLI_assert(local_asset == nullptr || STREQ(local_asset->name + 2, asset_name));

  return local_asset;
}

static std::string asset_root_path_for_save(const bUserAssetLibrary &user_library,
                                            const ID_Type id_type)
{
  BLI_assert(user_library.dirpath[0] != '\0');

  char libpath[FILE_MAX];
  BLI_strncpy(libpath, user_library.dirpath, sizeof(libpath));
  BLI_path_slash_native(libpath);
  BLI_path_normalize(libpath);

  /* Capitalize folder name. Ideally this would already available in
   * the type info to work correctly with multiple words. */
  const IDTypeInfo *id_type_info = BKE_idtype_get_info_from_idcode(id_type);
  std::string name = id_type_info->name_plural;
  name[0] = BLI_toupper_ascii(name[0]);

  return std::string(libpath) + SEP + "Saved" + SEP + name;
}

static std::string asset_blendfile_path_for_save(const bUserAssetLibrary &user_library,
                                                 const StringRef base_name,
                                                 const ID_Type id_type,
                                                 ReportList &reports)
{
  std::string root_path = asset_root_path_for_save(user_library, id_type);
  BLI_assert(!root_path.empty());

  if (!BLI_dir_create_recursive(root_path.c_str())) {
    BKE_report(&reports, RPT_ERROR, "Failed to create asset library directory to save brush");
    return "";
  }

  char base_name_filesafe[FILE_MAXFILE];
  BLI_strncpy(base_name_filesafe,
              base_name.data(),
              std::min(sizeof(base_name_filesafe), size_t(base_name.size() + 1)));
  BLI_path_make_safe_filename(base_name_filesafe);

  const std::string filepath = root_path + SEP + base_name_filesafe + BLENDER_ASSET_FILE_SUFFIX;
  if (!BLI_is_file(filepath.c_str())) {
    return filepath;
  }

  for (int i = 1;; i++) {
    const std::string filepath = root_path + SEP + base_name_filesafe + "_" + std::to_string(i++) +
                                 BLENDER_ASSET_FILE_SUFFIX;
    if (!BLI_is_file((filepath.c_str()))) {
      return filepath;
    }
  }

  return "";
}

static bool asset_write_in_library(Main *bmain,
                                   const ID &id_const,
                                   const StringRef name,
                                   const StringRefNull filepath,
                                   std::string &final_full_file_path,
                                   ReportList &reports)
{
  /* TODO: Comment seems to be resolved by separate #Main storage?
   *  XXX
   * FIXME
   *
   * This code is _pure evil_. It does in-place manipulation on IDs in global Main database,
   * temporarilly remove them and add them back...
   *
   * Use it as-is for now (in a similar way as python API or copy-to-buffer works). Nut the whole
   * 'BKE_blendfile_write_partial' code needs to be completely refactored.
   *
   * Ideas:
   *   - Have `BKE_blendfile_write_partial_begin` return a new temp Main.
   *   - Replace `BKE_blendfile_write_partial_tag_ID` by API to add IDs to this temp Main.
   *     + This should _duplicate_ the ID, not remove the original one from the source Main!
   *   - Have API to automatically also duplicate dependencies into temp Main.
   *     + Have options to e.g. make all duplicated IDs 'local' (i.e. remove their library data).
   *   - `BKE_blendfile_write_partial` then simply write the given temp main.
   *   - `BKE_blendfile_write_partial_end` frees the temp Main.
   */

  ID &id = const_cast<ID &>(id_const);

  const short prev_flag = id.flag;
  const int prev_tag = id.tag;
  const int prev_us = id.us;
  const std::string prev_name = id.name + 2;
  /* TODO: Remove library overrides stuff now that they are not used for brush assets. */
  IDOverrideLibrary *prev_liboverride = id.override_library;
  const int write_flags = 0; /* Could use #G_FILE_COMPRESS ? */
  const eBLO_WritePathRemap remap_mode = BLO_WRITE_PATH_REMAP_RELATIVE;

  BKE_blendfile_write_partial_begin(bmain);

  id.flag |= LIB_FAKEUSER;
  id.tag &= ~LIB_TAG_RUNTIME;
  id.us = 1;
  BLI_strncpy(id.name + 2, name.data(), std::min(sizeof(id.name) - 2, size_t(name.size() + 1)));
  id.override_library = nullptr;

  BKE_blendfile_write_partial_tag_ID(&id, true);

  /* TODO: check overwriting existing file. */
  /* TODO: ensure filepath contains only valid characters for file system. */
  const bool sucess = BKE_blendfile_write_partial(
      bmain, filepath.c_str(), write_flags, remap_mode, &reports);

  if (sucess) {
    final_full_file_path = std::string(filepath) + SEP + "Brush" + SEP + name;
  }

  BKE_blendfile_write_partial_end(bmain);

  BKE_blendfile_write_partial_tag_ID(&id, false);
  id.flag = prev_flag;
  id.tag = prev_tag;
  id.us = prev_us;
  BLI_strncpy(id.name + 2, prev_name.c_str(), sizeof(id.name) - 2);
  id.override_library = prev_liboverride;

  return sucess;
}

void AssetEditBlend::reload(Main &global_main)
{
  Main *old_main = this->main;
  this->main = BKE_main_new();
  this->main->is_asset_weak_reference_main = true;

  /* Fill fresh main database with same datablock as before. */
  LibraryLink_Params lapp_params{};
  lapp_params.bmain = this->main;
  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
  BKE_blendfile_link_append_context_flag_set(lapp_context, BLO_LIBLINK_FORCE_INDIRECT, true);
  BKE_blendfile_link_append_context_flag_set(lapp_context, 0, true);

  BKE_blendfile_link_append_context_library_add(lapp_context, this->filepath.c_str(), nullptr);

  /* Requests all existing datablocks to be appended again. */
  ID *old_id;
  FOREACH_MAIN_ID_BEGIN (old_main, old_id) {
    ID_Type old_id_code = GS(old_id->name);
    if (BKE_idtype_idcode_is_linkable(old_id_code)) {
      BlendfileLinkAppendContextItem *lapp_item = BKE_blendfile_link_append_context_item_add(
          lapp_context, old_id->name + 2, old_id_code, nullptr);
      BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, lapp_item, 0);
    }
  }
  FOREACH_MAIN_ID_END;

  BKE_blendfile_link(lapp_context, nullptr);
  BKE_blendfile_append(lapp_context, nullptr);

  BKE_blendfile_link_append_context_free(lapp_context);

  BKE_main_id_tag_all(this->main, LIB_TAG_ASSET_MAIN, true);

  /* Remap old to new. */
  bke::id::IDRemapper mappings;
  FOREACH_MAIN_ID_BEGIN (old_main, old_id) {
    ID *new_id = BKE_libblock_find_name(this->main, GS(old_id->name), old_id->name + 2);
    mappings.add(old_id, new_id);
  }
  FOREACH_MAIN_ID_END;
  BKE_libblock_remap_multiple(&global_main, mappings, 0);

  /* Free old database. */
  BKE_main_free(old_main);
}

void AssetEditBlend::clear_users(Main &global_main)
{
  /* Remap old to null pointer. */
  bke::id::IDRemapper mappings;
  ID *old_id;
  FOREACH_MAIN_ID_BEGIN (this->main, old_id) {
    mappings.add(old_id, nullptr);
  }
  FOREACH_MAIN_ID_END;
  BKE_libblock_remap_multiple(&global_main, mappings, 0);
}

/**
 * Public API
 */

static Vector<AssetEditBlend> &asset_edit_blend_get_all()
{
  static Vector<AssetEditBlend> mains;
  return mains;
}

static AssetEditBlend *asset_edit_blend_from_id(const ID &id)
{
  BLI_assert(id.tag & LIB_TAG_ASSET_MAIN);

  for (AssetEditBlend &asset_blend : asset_edit_blend_get_all()) {
    /* TODO: Look into make this whole thing more efficient. */
    ListBase *lb = which_libbase(asset_blend.main, GS(id.name));
    LISTBASE_FOREACH (ID *, other_id, lb) {
      if (&id == other_id) {
        return &asset_blend;
      }
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

Main *asset_edit_main(const ID &id)
{
  const AssetEditBlend *asset_blend = asset_edit_blend_from_id(id);
  return (asset_blend) ? asset_blend->main : nullptr;
}

static AssetEditBlend &asset_edit_blend_file_ensure(const StringRef filepath)
{
  for (AssetEditBlend &asset_blend : asset_edit_blend_get_all()) {
    if (asset_blend.filepath == filepath) {
      return asset_blend;
    }
  }

  asset_edit_blend_get_all().append_as(filepath);
  return asset_edit_blend_get_all().last();
}

std::optional<std::string> asset_edit_id_save_as(Main &global_main,
                                                 const ID &id,
                                                 const StringRef name,
                                                 const bUserAssetLibrary &user_library,
                                                 ReportList &reports)
{
  const std::string filepath = asset_blendfile_path_for_save(
      user_library, name, GS(id.name), reports);

  /* Save to asset library. */
  Main *asset_main = BKE_main_from_id(&global_main, &id);

  std::string final_full_asset_filepath;
  const bool success = asset_write_in_library(
      asset_main, id, name, filepath, final_full_asset_filepath, reports);
  if (!success) {
    BKE_report(&reports, RPT_ERROR, "Failed to write to asset library");
    return std::nullopt;
  }

  BKE_reportf(&reports, RPT_INFO, "Saved \"%s\"", filepath.c_str());

  return final_full_asset_filepath;
}

bool asset_edit_id_save(Main & /*global_main*/, const ID &id, ReportList &reports)
{
  AssetEditBlend *asset_blend = asset_edit_blend_from_id(id);
  if (asset_blend == nullptr) {
    return false;
  }

  std::string final_full_asset_filepath;
  const bool success = asset_write_in_library(asset_blend->main,
                                              id,
                                              id.name + 2,
                                              asset_blend->filepath.c_str(),
                                              final_full_asset_filepath,
                                              reports);

  if (!success) {
    BKE_report(&reports, RPT_ERROR, "Failed to write to asset library");
    return false;
  }

  return true;
}

bool asset_edit_id_revert(Main &global_main, const ID &id, ReportList & /*reports*/)
{
  AssetEditBlend *asset_blend = asset_edit_blend_from_id(id);
  if (asset_blend == nullptr) {
    return false;
  }

  /* Reload entire main, including texture dependencies. This relies on there
   * being only a single brush asset per blend file. */
  asset_blend->reload(global_main);

  return true;
}

bool asset_edit_id_delete(Main &global_main, const ID &id, ReportList &reports)
{
  AssetEditBlend *asset_blend = asset_edit_blend_from_id(id);
  if (asset_blend == nullptr) {
    return false;
  }

  if (BLI_delete(asset_blend->filepath.c_str(), false, false) != 0) {
    BKE_report(&reports, RPT_ERROR, "Failed to delete asset library file");
    return false;
  }

  asset_blend->clear_users(global_main);

  int index = 0;
  for (AssetEditBlend &asset_blend_iter : asset_edit_blend_get_all()) {
    if (&asset_blend_iter == asset_blend) {
      asset_edit_blend_get_all().remove(index);
      break;
    }
    index++;
  }

  return true;
}

ID *asset_edit_id_from_weak_reference(Main &global_main,
                                      const ID_Type id_type,
                                      const AssetWeakReference &weak_ref)
{
  char asset_full_path_buffer[FILE_MAX_LIBEXTRA];
  char *asset_lib_path, *asset_group, *asset_name;

  AS_asset_full_path_explode_from_weak_ref(
      &weak_ref, asset_full_path_buffer, &asset_lib_path, &asset_group, &asset_name);
  if (asset_lib_path == nullptr && asset_group == nullptr && asset_name == nullptr) {
    return nullptr;
  }

  BLI_assert(asset_name != nullptr);

  /* Find asset in current blend file. */
  if (asset_lib_path == nullptr) {
    ID *local_asset = BKE_libblock_find_name(&global_main, id_type, asset_name);
    BLI_assert(local_asset == nullptr || ID_IS_ASSET(local_asset));
    return local_asset;
  }

  /* If weak reference resolves to a null library path, assume we are in local asset case. */
  AssetEditBlend &asset_blend = asset_edit_blend_file_ensure(asset_lib_path);
  return asset_blend.ensure_id(id_type, asset_name);
}

bool asset_edit_id_is_editable(const ID &id)
{
  if (!(id.tag & LIB_TAG_ASSET_MAIN)) {
    return false;
  }

  const AssetEditBlend *asset_blend = asset_edit_blend_from_id(id);
  return (asset_blend) ? asset_blend->is_editable : false;
}

void asset_edit_main_free_all()
{
  asset_edit_blend_get_all().clear_and_shrink();
}

}  // namespace blender::bke
